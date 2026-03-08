#include "app_controller.h"
#include "pdf_parser.h"
#include "thread_manager.h"
#include "tts_manager.h"
#include <QCoreApplication>
#include <QString>
#include <cstdint>
#include <iostream>
#include <qtmetamacros.h>

AppController::AppController(QObject *parent)
    : QObject(parent), m_pdfPath(""), m_musicPath(""), m_isBusy(false), m_isPlaying(true),
      m_totalPages(0), m_currentPage(0), m_ttsSpeed(1.0), m_synthPos{0, 0}, m_playbackPos{0, 0},
      m_maxLookAheadCount(10), m_maxHistoryCount(5), m_maxQueuedCount(5), m_imageIdx(0), m_imageId(0),
      m_hasImage(false), m_allImagesDisplayed(false)
{
    m_pdfParser = std::make_unique<PDFParser>();
    m_ttsManager = std::make_unique<TTSManager>();
    m_audioManager = std::make_unique<AudioManager>();
    m_threadManager = std::make_unique<ThreadManager>();

    m_imageTimer.setInterval(2000);
    m_imageTimer.setSingleShot(false);
    connect(&m_imageTimer, &QTimer::timeout, this, &AppController::updateImage);
    connect(m_pdfParser.get(), &PDFParser::pdfLoaded, this, &AppController::onPdfLoaded);
    connect(m_pdfParser.get(), &PDFParser::pdfLoadFailed, this, &AppController::onPdfLoadFailed);
    connect(m_pdfParser.get(), &PDFParser::pageExtracted, this, &AppController::onPageExtracted);
    connect(m_pdfParser.get(), &PDFParser::pageExtractionFailed, this,
            &AppController::onPageExtractionFailed);
    connect(m_ttsManager.get(), &TTSManager::synthesisComplete, this,
            &AppController::onSythesisComplete);
    connect(m_ttsManager.get(), &TTSManager::synthesisFailed, this, &AppController::onSythesisFailed);
    connect(m_audioManager.get(), &AudioManager::speechFinished, this, &AppController::onSpeechFinished);
    connect(m_audioManager.get(), &AudioManager::speechFailed, this, &AppController::onSpeechFailed);

    if (!m_ttsManager->initialize("assets/model/kokoro-en-v0_19"))
    {
        std::cout << "Failed to initialize the kokoro model\n";
    }
}

void AppController::openPDF(const QString &filePath)
{
    m_threadManager->submitTask(ThreadType::PDFParser, [this, filePath] { m_pdfParser->closePDF(); });

    m_threadManager->submitTask(ThreadType::PDFParser,
                                [this, filePath] { m_pdfParser->loadPDF(filePath); });
    m_pdfPath = filePath;
    m_isBusy = true;
    emit isBusyChanged();
}

void AppController::openMusic(const QString &musicPath)
{
    m_musicPath = musicPath;
    m_audioManager->stopMusic();
    m_audioManager->playMusic(musicPath);
}

void AppController::toggleMusic()
{
    m_audioManager->toggleMusic();
    emit isMusicEnabledChanged();
}

void AppController::pause()
{
    if (m_isPlaying)
    {
        m_isPlaying = false;
        m_audioManager->pause();
        emit isPlayingChanged();
    }
}

void AppController::play()
{
    if (!m_isPlaying)
    {
        m_isPlaying = true;
        m_audioManager->resume();
        emit isPlayingChanged();

        if (!m_audioManager->isSpeechPlaying())
        {
            playTargetPlayback();
        }
    }
}

void AppController::evictHistory()
{
    if (m_plHistory.length() > m_maxHistoryCount)
    {
        Position pos = m_plHistory.dequeue();
        PageData &page = m_pageDataMap[pos.pageNo];
        QByteArray &audio = page.playbacks[pos.sentenceIdx].audio;
        audio.clear();
        audio.squeeze();
        if (pos.sentenceIdx == page.sentences.length() - 1)  // Delete the page
        {
            m_pageDataMap.remove(pos.pageNo);
        }
    }
}

void AppController::navigateTo(Position target)
{
    m_audioManager->stopSpeech();

    // Case 1: target is ahead in lookahead
    if (m_plLookAhead.contains(target))
    {
        while (!m_plLookAhead.empty() && m_plLookAhead.front() != target)
        {
            m_plHistory.enqueue(m_playbackPos);
            evictHistory();
            m_playbackPos = m_plLookAhead.dequeue();
        }
        playTargetPlayback();
        synthesizeTargetSentence();
        return;
    }

    // Case 2: target is behind in history
    if (m_plHistory.contains(target))
    {
        while (!m_plHistory.empty() && m_plHistory.back() != target)
        {
            m_plLookAhead.prepend(m_playbackPos);
            m_playbackPos = m_plHistory.takeLast();
        }
        playTargetPlayback();
        synthesizeTargetSentence();
        return;
    }

    // Case 3: target is outside both windows — full reset or clear until target
    m_plHistory.clear();
    m_plLookAhead.clear();
    if (!m_synthQueue.contains(target))
    {
        m_playbackPos = target;
        m_synthPos = target;
    }

    if (m_currentPage != m_playbackPos.pageNo)
    {
        m_currentPage = m_playbackPos.pageNo;
        emit currentPageChanged();
        m_imageIdx = 0;
        restartImageTimer();
    }
    cancelOutstandingTasks(target);
    synthesizeTargetSentence();
}

void AppController::cancelOutstandingTasks(Position target)
{
    QMutexLocker lock(&m_cancelMutex);
    int targetIdx = m_synthQueue.indexOf(target);

    if (targetIdx == -1)
    {
        // Target not in queue — cancel everything
        for (const Position &pos : m_synthQueue)
        {
            m_cancelledTasks.push_back(pos);
            m_pageDataMap[pos.pageNo].playbacks[pos.sentenceIdx].state = LoadState::Unloaded;
        }
        m_synthQueue.clear();
        return;
    }

    // Cancel only what's before target
    for (int i = 0; i < targetIdx; i++)
    {
        m_cancelledTasks.push_back(m_synthQueue[i]);
        m_pageDataMap[m_synthQueue[i].pageNo]
            .playbacks[m_synthQueue[i].sentenceIdx]
            .state = LoadState::Unloaded;
    }

    for (int i = 1; i < m_synthQueue.length(); i++)
    {
        nextPosition(target);
        if (m_synthQueue[i] != target)
        {
            for (int j = i; j < m_synthQueue.length(); j++)
            {
                m_cancelledTasks.push_back(m_synthQueue[j]);
                m_pageDataMap[m_synthQueue[j].pageNo]
                    .playbacks[m_synthQueue[j].sentenceIdx]
                    .state = LoadState::Unloaded;
            }
            break;
        }
    }
}

void AppController::updateImage()
{
    if (m_pageDataMap[m_currentPage].state != LoadState::Loaded)
    {
        return;
    }

    if (m_imageIdx >= m_pageDataMap[m_currentPage].images.length())
    {
        m_imageIdx = 0;
        m_allImagesDisplayed = true;
        if (m_coverImage)
        {
            m_currentImage = m_coverImage.value();
            m_hasImage = true;
        }
        else
        {
            m_hasImage = false;
        }
        emit imageIdChanged();
        m_imageTimer.stop();  // stop any further updates until we go to the next page
        return;
    }
    m_currentImage = m_pageDataMap[m_currentPage].images[m_imageIdx++];
    m_allImagesDisplayed = false;
    m_hasImage = true;
    m_imageId++;
    emit imageIdChanged();
}

void AppController::restartImageTimer()
{
    updateImage();
    m_imageTimer.start();
}

void AppController::prevLine()
{
    if (m_audioManager->isSpeechPlaying())
    {
        // m_playbackPos points to the next speech to play hence go back twice
        prevPosition(m_playbackPos);
        prevPosition(m_playbackPos);
    }
    else
    {
        prevPosition(m_playbackPos);
    }

    while (m_pageDataMap[m_playbackPos.pageNo].state == LoadState::Loaded &&
           m_pageDataMap[m_playbackPos.pageNo].sentences.length() == 0)
    {
        prevPosition(m_playbackPos);
        if (m_playbackPos.pageNo == 0)
        {
            break;
        }
    }

    navigateTo(m_playbackPos);
}

void AppController::nextLine()
{
    // m_playbackPos points to the next speech hence dont go next
    if (!m_audioManager->isSpeechPlaying())
    {
        nextPosition(m_playbackPos);
    }

    navigateTo(m_playbackPos);
}

void AppController::prevPage()
{
    if (m_playbackPos.pageNo != 0)
    {
        m_playbackPos.pageNo--;
    }
    m_playbackPos.sentenceIdx = 0;

    while (m_pageDataMap[m_playbackPos.pageNo].state == LoadState::Loaded &&
           m_pageDataMap[m_playbackPos.pageNo].sentences.length() == 0)
    {
        prevPosition(m_playbackPos);
        if (m_playbackPos.pageNo == 0)
        {
            break;
        }
    }

    navigateTo(m_playbackPos);
}

void AppController::nextPage()
{
    if (m_playbackPos.pageNo != m_totalPages - 1)
    {
        m_playbackPos.pageNo++;
    }
    m_playbackPos.sentenceIdx = 0;

    navigateTo(m_playbackPos);
}

void AppController::goToPage(uint16_t page)
{
    m_playbackPos = {page, 0};
    navigateTo(m_playbackPos);
}

void AppController::nextPosition(Position &pos)
{
    if (pos.sentenceIdx == m_pageDataMap[pos.pageNo].sentences.length())
    {
        if (m_playbackPos.pageNo != m_totalPages - 1)
        {
            m_playbackPos.pageNo++;
        }

        if (m_pageDataMap[pos.pageNo].state == LoadState::Failed ||
            (m_pageDataMap[pos.pageNo].sentences.length() == 0 &&
             m_pageDataMap[pos.pageNo].images.length() == 0))
        {
            nextPosition(pos);
        }
        else
        {
            pos.sentenceIdx = 0;
        }
        return;
    }

    pos.sentenceIdx++;
}

void AppController::prevPosition(Position &pos)
{
    if (pos.sentenceIdx == 0)
    {
        if (m_playbackPos.pageNo != 0)
        {
            m_playbackPos.pageNo--;
        }

        if (m_pageDataMap[pos.pageNo].state == LoadState::Failed ||
            (m_pageDataMap[pos.pageNo].sentences.length() == 0 &&
             m_pageDataMap[pos.pageNo].images.length() == 0))
        {
            prevPosition(pos);
        }
        else if (m_pageDataMap[pos.pageNo].state != LoadState::Loaded)
        {
            pos.sentenceIdx = UINT16_MAX;  // sentinal value if page not loaded (unloaded or loading)
        }
        else
        {
            pos.sentenceIdx = m_pageDataMap[pos.pageNo].sentences.length() - 1;
        }
        return;
    }

    pos.sentenceIdx--;
}

void AppController::parsePage(int pageNumber)
{
    m_threadManager->submitTask(ThreadType::PDFParser,
                                [this, pageNumber]() { m_pdfParser->extractPageContents(pageNumber); });
    m_pageDataMap[m_synthPos.pageNo].state = LoadState::Loading;
}

void AppController::synthesizeTargetSentence()
{
    if (m_plLookAhead.length() > m_maxLookAheadCount ||
        m_threadManager->queuedTaskCount(ThreadType::TTSManager) > m_maxQueuedCount)
    {
        return;
    }

    if (m_pageDataMap[m_synthPos.pageNo].state == LoadState::Unloaded)
    {
        parsePage(m_synthPos.pageNo);
    }
    else if (m_pageDataMap[m_synthPos.pageNo].state == LoadState::Loaded &&
             m_synthPos.sentenceIdx == UINT16_MAX)  // is sentinal value
    {
        m_synthPos.sentenceIdx = m_pageDataMap[m_synthPos.pageNo].sentences.length() - 1;
    }
    else if (m_pageDataMap[m_synthPos.pageNo].state == LoadState::Failed ||
             m_pageDataMap[m_synthPos.pageNo].sentences.length() == 0)
    {
        nextPosition(m_synthPos);
        if (m_pageDataMap[m_synthPos.pageNo].state == LoadState::Unloaded)
        {
            parsePage(m_synthPos.pageNo);
        }
    }

    if (m_pageDataMap[m_synthPos.pageNo].state != LoadState::Loaded)
    {
        return;
    }

    uint16_t parsePage = m_synthPos.pageNo;
    uint16_t sentenceIdx = m_synthPos.sentenceIdx;
    m_threadManager->submitTask(ThreadType::TTSManager,
                                [this, parsePage, sentenceIdx]()
                                {
                                    {
                                        QMutexLocker lock(&m_cancelMutex);
                                        if (m_cancelledTasks.contains({parsePage, sentenceIdx}))
                                        {
                                            m_cancelledTasks.removeOne({parsePage, sentenceIdx});
                                            return;
                                        }
                                    }
                                    m_ttsManager->synthesizeText(
                                        m_pageDataMap[parsePage].sentences[sentenceIdx], parsePage,
                                        sentenceIdx, m_ttsSpeed);
                                });
    m_synthQueue.enqueue(m_synthPos);
    nextPosition(m_synthPos);
}

void AppController::playTargetPlayback()
{
    if (!(m_isPlaying && m_plLookAhead.contains(m_playbackPos)) &&
        ((m_currentPage == m_playbackPos.pageNo) || m_allImagesDisplayed))
    {
        return;
    }

    Position pos = m_playbackPos;
    LoadState currState = m_pageDataMap[pos.pageNo].playbacks[pos.sentenceIdx].state;
    if (currState == LoadState::Loaded)
    {
        if (pos.sentenceIdx == UINT16_MAX)
        {
            pos.sentenceIdx = m_pageDataMap[pos.pageNo].sentences.length() - 1;
        }
        m_audioManager->playSpeech(m_pageDataMap[pos.pageNo].playbacks[pos.sentenceIdx].audio,
                                   m_pageDataMap[pos.pageNo].playbacks[pos.sentenceIdx].sampleRate);

        nextPosition(m_playbackPos);
        if (m_currentPage != pos.pageNo)
        {
            m_imageIdx = 0;
            m_currentPage = pos.pageNo;
            emit currentPageChanged();
            restartImageTimer();
        }
        evictHistory();
        m_plHistory.enqueue(pos);
    }
}

void AppController::resetOnPdfLoad()
{
    m_isBusy = false;
    emit isBusyChanged();
    m_currentPage = 0;
    emit currentPageChanged();

    // Containers
    m_pageDataMap.clear();
    m_synthQueue.clear();
    m_plLookAhead.clear();
    m_plHistory.clear();
    m_cancelledTasks.clear();

    // Positions
    m_synthPos = {0, 0};
    m_playbackPos = {0, 0};

    // Images
    m_imageIdx = 0;
    m_hasImage = false;
    m_allImagesDisplayed = false;
    m_currentImage = QImage();
    m_coverImage.reset();
    m_imageTimer.stop();
}

void AppController::onPdfLoaded(int totalPages)
{
    emit statusMessage(QString("PDF Loaded : %1").arg(totalPages));
    m_totalPages = totalPages;
    emit totalPagesChanged();
    resetOnPdfLoad();
    synthesizeTargetSentence();
}

void AppController::onPdfLoadFailed(const QString &error)
{
    m_pdfPath = "";
    m_totalPages = 0;
    emit totalPagesChanged();
    resetOnPdfLoad();
    emit errorOccurred(error);
}

void AppController::onPageExtracted(uint16_t pageNumber, QVector<QString> sentences,
                                    QVector<QImage> images)
{
    m_pageDataMap[pageNumber] = {.state = LoadState::Loaded, .sentences = sentences, .images = images};
    m_pageDataMap[pageNumber].playbacks.resize(sentences.length());
    synthesizeTargetSentence();

    if (pageNumber == 0 && m_pageDataMap[pageNumber].images.length() > 0)
    {
        m_coverImage = m_pageDataMap[pageNumber].images[0];
    }

    if (m_currentPage == pageNumber)
    {
        restartImageTimer();
    }
}

void AppController::onPageExtractionFailed(uint16_t pageNumber, const QString &error)
{
    m_pageDataMap[pageNumber].state = LoadState::Failed;
    synthesizeTargetSentence();
}

void AppController::onSythesisComplete(uint16_t pageNumber, uint16_t sentenceIdx,
                                       const QByteArray &audioData, int sampleRate)
{
    emit statusMessage(QString("Syntesis complete : %1, %2, %3")
                           .arg(pageNumber)
                           .arg(sentenceIdx)
                           .arg(m_pageDataMap[pageNumber].sentences[sentenceIdx]));
    {
        QMutexLocker lock(&m_cancelMutex);
        if (m_cancelledTasks.contains({pageNumber, sentenceIdx}))
        {
            m_cancelledTasks.removeOne({pageNumber, sentenceIdx});
            return;
        }
    }

    m_pageDataMap[pageNumber].playbacks[sentenceIdx].setValues(LoadState::Loaded, audioData, sampleRate);
    m_plLookAhead.enqueue(Position{pageNumber, sentenceIdx});
    m_synthQueue.dequeue();
    if (!m_audioManager->isSpeechPlaying())
    {
        playTargetPlayback();
    }
    synthesizeTargetSentence();
}

void AppController::onSythesisFailed(uint16_t pageNumber, uint16_t sentenceIdx, const QString &error)
{
    m_pageDataMap[pageNumber].playbacks[sentenceIdx].setValues(LoadState::Failed);
    m_plLookAhead.enqueue(Position{pageNumber, sentenceIdx});
    m_synthQueue.dequeue();
    synthesizeTargetSentence();
}

void AppController::onSpeechFinished()
{
    playTargetPlayback();
    synthesizeTargetSentence();
}

void AppController::onSpeechFailed()
{
    emit errorOccurred("Failed to play the sentence. \n Moving to the next sentence");
    synthesizeTargetSentence();
}

void AppController::onMusicFinished()
{
    if (m_isPlaying)
    {
        m_audioManager->playMusic(m_musicPath);
    }
}
