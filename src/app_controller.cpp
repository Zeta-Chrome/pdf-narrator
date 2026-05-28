#include "app_controller.h"
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <iostream>

AppController::AppController(QObject *parent)
{
    m_pdfParser = std::make_unique<PDFParser>();
    m_ttsManager = std::make_unique<TTSManager>();
    m_audioManager = std::make_unique<AudioManager>();
    m_threadManager = std::make_unique<ThreadManager>();

    m_imageTimer.setInterval(2000);
    m_imageTimer.setSingleShot(false);
    connect(&m_imageTimer, &QTimer::timeout, this, &AppController::updateImage);
    connect(m_ttsManager.get(), &TTSManager::ttsInitializationComplete, this,
            &AppController::onTtsInitializationComplete);
    connect(m_ttsManager.get(), &TTSManager::ttsInitializationFailed, this,
            &AppController::onTtsInitializationFailed);
    connect(m_pdfParser.get(), &PDFParser::pdfLoaded, this, &AppController::onPdfLoaded);
    connect(m_pdfParser.get(), &PDFParser::pdfLoadFailed, this, &AppController::onPdfLoadFailed);
    connect(m_pdfParser.get(), &PDFParser::pdfStructureExtracted, this,
            &AppController::onPdfStructureExtracted);
    connect(m_pdfParser.get(), &PDFParser::pageExtracted, this, &AppController::onPageExtracted);
    connect(m_pdfParser.get(), &PDFParser::pageExtractionFailed, this,
            &AppController::onPageExtractionFailed);
    connect(m_ttsManager.get(), &TTSManager::synthesisComplete, this,
            &AppController::onSythesisComplete);
    connect(m_ttsManager.get(), &TTSManager::synthesisFailed, this, &AppController::onSythesisFailed);
    connect(m_audioManager.get(), &AudioManager::speechFinished, this, &AppController::onSpeechFinished);

    QTimer::singleShot(0, this, &AppController::initialize);
}

AppController::~AppController()
{
    cancelOutstandingTasks({}, 0);
}

void AppController::loadState()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile file(path + "/PDFNarrator.json");

    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray data = file.readAll();

    QJsonDocument doc = QJsonDocument::fromJson(data);

    if (!doc.isObject())
    {
        return;
    }

    m_appState.fromJson(doc.object());

    m_audioManager->setMusicVolume(doc.object().value("MusicVolume").toDouble());
    if (!doc.object().value("IsMusicEnabled").toBool())
    {
        m_audioManager->toggleMusic();
    }
}

void AppController::saveState()
{
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);

    QFile file(path + "/PDFNarrator.json");

    if (!file.open(QIODevice::WriteOnly))
        return;

    QJsonObject obj = m_appState.toJson();
    obj["MusicVolume"] = m_audioManager->musicVolume();
    obj["IsMusicEnabled"] = m_audioManager->isMusicEnabled();

    QJsonDocument doc(obj);
    file.write(doc.toJson());
}

void AppController::initialize()
{
    loadState();
    m_threadManager->submitTask(ThreadType::TTSManager,
                                [this]() { m_ttsManager->initialize("assets/model/kokoro-int8-multi-lang-v1_1"); });
}

void AppController::onTtsInitializationComplete()
{
    m_isInitialized = true;
    emit isInitializedChanged();

    if (!m_appState.pdfPath.isEmpty())
    {
        openPDF(m_appState.pdfPath);
    }

    if (!m_appState.musicPath.isEmpty())
    {
        openMusic(m_appState.musicPath);
    }
}

void AppController::onTtsInitializationFailed(QString error)
{
    emit errorOccurred(error);
}

void AppController::setTtsSpeed(float speed)
{
    if (m_isPdfLoaded)
    {
        // start again from the beginning of the sentence
        if (m_audioManager->isSpeechPlaying())
        {
            // m_playbackPos points to the next speech to play hence go back twice
            prevPosition(m_playbackPos);
        }
        navigateTo(m_playbackPos);
    }

    cancelOutstandingTasks({}, true);
    m_plLookAhead.clear();
    m_plHistory.clear();
    m_pageDataMap.clear();
    m_appState.ttsSpeed = speed;
    saveState();
}

void AppController::setTtsSpeaker(int speakerId)
{
    if (m_isPdfLoaded)
    {
        // start again from the beginning of the sentence
        if (m_audioManager->isSpeechPlaying())
        {
            // m_playbackPos points to the next speech to play hence go back twice
            prevPosition(m_playbackPos);
        }
        navigateTo(m_playbackPos);
    }

    cancelOutstandingTasks({}, true);
    m_plLookAhead.clear();
    m_plHistory.clear();
    m_pageDataMap.clear();
    m_appState.speakerId = speakerId;
    saveState();
}

void AppController::openPDF(const QString &filePath)
{
    m_audioManager->stopSpeech();
    m_threadManager->submitTask(ThreadType::PDFParser, [this] { m_pdfParser->closePdf(); });

    m_threadManager->submitTask(ThreadType::PDFParser,
                                [this, filePath] { m_pdfParser->loadPdf(filePath); });
    m_appState.pdfPath = filePath;
    saveState();
    m_isBusy = true;
    emit isBusyChanged();
}

void AppController::openMusic(const QString &musicPath)
{
    m_appState.musicPath = musicPath;
    saveState();
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

        if (!m_audioManager->isSpeechPlaying() && m_isPdfLoaded)
        {
            playTargetPlayback();
        }
    }
}

void AppController::restart()
{
    m_audioManager->stopSpeech();
    resetState();
    driveSynthesis();
}

void AppController::prevLine()
{
    if (!m_isPdfLoaded)
    {
        return;
    }

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

    navigateTo(m_playbackPos);
}

void AppController::nextLine()
{
    if (!m_isPdfLoaded)
    {
        return;
    }

    // m_playbackPos points to the next speech hence dont go next
    if (!m_audioManager->isSpeechPlaying())
    {
        nextPosition(m_playbackPos);
    }

    navigateTo(m_playbackPos);
}

void AppController::prevPage()
{
    if (!m_isPdfLoaded)
    {
        return;
    }

    if (m_playbackPos.pageNo != 0)
    {
        m_playbackPos.pageNo--;
    }
    m_playbackPos.sentenceIdx = 0;

    navigateTo(m_playbackPos);
}

void AppController::nextPage()
{
    if (!m_isPdfLoaded)
    {
        return;
    }

    m_playbackPos.pageNo++;
    m_playbackPos.sentenceIdx = 0;

    navigateTo(m_playbackPos);
}

void AppController::goToPage(uint16_t page)
{
    if (!m_isPdfLoaded)
    {
        return;
    }

    m_playbackPos = {page, 0};
    navigateTo(m_playbackPos);
}

void AppController::navigateTo(Position target)
{
    cancelOutstandingTasks(target);

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
        driveSynthesis();
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
        driveSynthesis();
        return;
    }

    // Case 3: target is outside both windows — full reset or clear until target
    m_plLookAhead.clear();
    m_plHistory.clear();
    m_pageDataMap.clear();
    m_playbackPos = target;
    if (!m_synthQueue.contains(target))
    {
        m_synthPos = target;
    }

    if (m_appState.currentPage != m_playbackPos.pageNo)
    {
        m_appState.imageIdx = 0;
        saveState();
        updateCurrentPage(m_playbackPos.pageNo, true);
        restartImageTimer();
    }
    driveSynthesis();
}

void AppController::cancelOutstandingTasks(Position target, bool all)
{
    QMutexLocker lock(&m_cancelMutex);

    int targetIdx = -1;
    if (!all)
    {
        targetIdx = m_synthQueue.indexOf(target);
    }

    if (targetIdx == -1)
    {
        // Target not in queue — cancel everything
        for (const Position &pos : m_synthQueue)
        {
            m_cancelList.push_back(pos);
            m_pageDataMap[pos.pageNo].playbacks[pos.sentenceIdx].state = LoadState::Unloaded;
        }
        m_synthQueue.clear();
        return;
    }

    // Cancel only what's before target
    for (int i = 0; i < targetIdx; i++)
    {
        m_cancelList.push_back(m_synthQueue[i]);
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
                m_cancelList.push_back(m_synthQueue[j]);
                m_pageDataMap[m_synthQueue[j].pageNo]
                .playbacks[m_synthQueue[j].sentenceIdx]
                .state = LoadState::Unloaded;
            }
            break;
        }
    }
}

void AppController::resetState()
{
    m_pageDataMap.clear();
    m_plLookAhead.clear();
    m_plHistory.clear();
    m_synthQueue.clear();
    m_cancelList.clear();
    m_imageTimer.stop();
    m_coverImage.reset();
    m_currentImage.reset();

    if (m_appState.isLoaded)
    {
        updateCurrentPage(m_appState.currentPage, true);
        m_synthPos = {m_appState.currentPage, m_appState.sentenceIdx};
        if (m_pdfStructure[m_appState.currentPage].sentenceCount == 0)
        {
            nextPosition(m_synthPos);
        }
        m_playbackPos = m_synthPos;
        m_appState.imageIdx = m_appState.imageIdx;
        parsePage(0);  // parse the first Page to save the coverImage if available
    }
    else
    {
        updateCurrentPage(0, true);
        m_synthPos = {0, 0};
        // get the first valid sentence position
        if (m_pdfStructure[0].sentenceCount == 0)
        {
            nextPosition(m_synthPos);
        }
        m_playbackPos = m_synthPos;
        m_appState.imageIdx = 0;
        saveState();
    }
}

void AppController::parsePage(uint16_t pageNumber)
{
    // Load all the pages from currentPage to givenPageNumber
    for (int pg = m_appState.currentPage; pg <= pageNumber; pg++)
    {
        if (m_pageDataMap[pg].state == LoadState::Loaded)
            continue;

        m_threadManager->submitTask(ThreadType::PDFParser,
                                    [this, pg]() { m_pdfParser->extractPageContents(pg); });
        m_pageDataMap[pg].state = LoadState::Loading;
    }
}

void AppController::prevPosition(Position &pos)
{
    if (pos.sentenceIdx == 0 || m_pdfStructure[pos.pageNo].sentenceCount == 0)
    {
        Position originalPos = pos;
        do
        {
            if (pos.pageNo == 0)
            {
                break;
            }
            pos.pageNo--;
        } while (m_pdfStructure[pos.pageNo].sentenceCount == 0);

        if (m_pdfStructure[pos.pageNo].sentenceCount == 0)
        {
            pos = originalPos;
        }
        else
        {
            pos.sentenceIdx = m_pdfStructure[pos.pageNo].sentenceCount - 1;
        }
        return;
    }
    pos.sentenceIdx--;
}

void AppController::nextPosition(Position &pos)
{
    if (pos.sentenceIdx == m_pdfStructure[pos.pageNo].sentenceCount - 1 ||
        m_pdfStructure[pos.pageNo].sentenceCount == 0)
    {
        Position originalPos = pos;
        do
        {
            if (pos.pageNo == m_totalPages - 1)
            {
                break;
            }
            pos.pageNo++;
        } while (m_pdfStructure[pos.pageNo].sentenceCount == 0);

        if (m_pdfStructure[pos.pageNo].sentenceCount == 0)
        {
            pos = originalPos;
        }
        else
        {
            pos.sentenceIdx = 0;
        }
        return;
    }
    pos.sentenceIdx++;
}

bool AppController::isEndPosition(Position &pos)
{
    return pos.pageNo == m_totalPages - 1 &&
           (m_pageDataMap[pos.pageNo].state == LoadState::Failed ||
            m_synthPos.sentenceIdx >= m_pdfStructure[pos.pageNo].sentenceCount);
}

void AppController::driveSynthesis()
{
    if (m_plLookAhead.length() > m_maxLookAhead ||
        m_threadManager->queuedTaskCount(ThreadType::TTSManager) > m_maxSynth ||
        isEndPosition(m_synthPos))
    {
        return;
    }

    if (m_pageDataMap[m_synthPos.pageNo].state == LoadState::Unloaded)
    {
        parsePage(m_synthPos.pageNo);
    }
    else if (m_pageDataMap[m_synthPos.pageNo].state == LoadState::Failed &&
             m_synthPos.pageNo < m_totalPages - 1)
    {
        // If page parsing failed, go to nextPage
        m_synthPos.pageNo++;
    }

    if (m_pageDataMap[m_synthPos.pageNo].state != LoadState::Loaded)
    {
        return;
    }

    uint16_t parsePage = m_synthPos.pageNo;
    uint16_t sentenceIdx = m_synthPos.sentenceIdx;
    QString sentence = m_pageDataMap[parsePage].sentences[sentenceIdx];
    m_threadManager->submitTask(ThreadType::TTSManager,
                                [this, sentence, parsePage, sentenceIdx]()
                                {
                                    {
                                        QMutexLocker lock(&m_cancelMutex);
                                        if (m_cancelList.contains({parsePage, sentenceIdx}))
                                        {
                                            m_cancelList.removeOne({parsePage, sentenceIdx});
                                            return;
                                        }
                                    }
                                    m_ttsManager->synthesizeText(sentence, parsePage, sentenceIdx,
                                                                 m_appState.speakerId,
                                                                 m_appState.ttsSpeed);
                                });
    m_synthQueue.enqueue(m_synthPos);
    nextPosition(m_synthPos);
}

void AppController::evictHistory()
{
    if (m_plHistory.length() > m_maxHistory)
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

void AppController::updateCurrentPage(int page, bool force)
{
    if (force || ((m_allPlaybacksPlayed || m_pdfStructure[m_appState.currentPage].sentenceCount == 0) &&
                  (m_allImagesDisplayed || m_pdfStructure[m_appState.currentPage].imageCount == 0)))
    {
        // Skip empty pages
        while (m_pdfStructure[page].sentenceCount == 0 && m_pdfStructure[page].imageCount == 0)
        {
            page++;
            if (page == m_totalPages)
            {
                break;
            }
        }

        if (page < m_totalPages)
        {
            m_allImagesDisplayed = false;
            m_allPlaybacksPlayed = false;

            m_appState.currentPage = page;
            saveState();
            emit currentPageChanged();
            restartImageTimer();
        }
        else
        {
            m_isRestart = true;
            emit isRestartChanged();
        }
    }
}

void AppController::playTargetPlayback()
{
    if (!m_isPlaying || m_appState.currentPage != m_playbackPos.pageNo)
    {
        return;
    }

    if (m_plLookAhead.length() == 0 && !isEndPosition(m_playbackPos))
    {
        m_isBusy = true;
        emit isBusyChanged();
        return;
    }

    Position pos = m_playbackPos;
    LoadState currState = m_pageDataMap[pos.pageNo].playbacks[pos.sentenceIdx].state;
    if (currState == LoadState::Loaded)
    {
        m_audioManager->playSpeech(m_pageDataMap[pos.pageNo].playbacks[pos.sentenceIdx].audio,
                                   m_pageDataMap[pos.pageNo].playbacks[pos.sentenceIdx].sampleRate,
                                   pos.pageNo, pos.sentenceIdx);
        m_appState.sentenceIdx = pos.sentenceIdx;
        saveState();
        if (m_isBusy)
        {
            m_isBusy = false;
            emit isBusyChanged();
        }
        if (m_plLookAhead.length() > 0)
        {
            m_plHistory.enqueue(m_playbackPos);
            m_plLookAhead.dequeue();
            evictHistory();
        }
    }

    if (currState != LoadState::Unloaded)
    {
        nextPosition(m_playbackPos);
    }
}

void AppController::restartImageTimer()
{
    updateImage();
    m_imageTimer.start();
}

void AppController::updateImage()
{
    if (!m_isPlaying || m_isBusy || m_pageDataMap[m_appState.currentPage].state != LoadState::Loaded)
    {
        return;
    }

    if (m_appState.imageIdx >= m_pdfStructure[m_appState.currentPage].imageCount)
    {
        m_appState.imageIdx = 0;
        saveState();
        m_allImagesDisplayed = true;
        if (m_coverImage)
        {
            m_currentImage = m_coverImage.value();
        }
        else
        {
            m_currentImage.reset();
        }
        emit imageIdChanged();
        m_imageTimer.stop();  // stop any further updates until we go to the next page
        updateCurrentPage(m_appState.currentPage + 1);
        return;
    }
    m_currentImage = m_pageDataMap[m_appState.currentPage].images[m_appState.imageIdx++];
    m_imageId++;
    emit imageIdChanged();
}

void AppController::onPdfLoaded(int totalPages)
{
    m_totalPages = totalPages;
    emit totalPagesChanged();

    m_threadManager->submitTask(ThreadType::PDFParser, [this]() { m_pdfParser->extractPdfStructure(); });
}

void AppController::onPdfLoadFailed(const QString &error)
{
    m_appState.pdfPath = "";
    m_isPdfLoaded = false;
    saveState();
    m_totalPages = 0;
    emit totalPagesChanged();
    m_isBusy = false;
    emit isBusyChanged();
    m_appState.isLoaded = false;
    resetState();
    m_pdfStructure.clear();
    emit errorOccurred(error);
}

void AppController::onPdfStructureExtracted(QVector<PageIndex> structure)
{
    m_pdfStructure = structure;
    m_isPdfLoaded = true;
    m_isBusy = false;
    emit isBusyChanged();
    emit statusMessage(QString("PDF Loaded : %1").arg(m_totalPages));
    resetState();
    driveSynthesis();
}

void AppController::onPageExtracted(uint16_t pageNumber, QVector<QString> sentences,
                                    QVector<QImage> images)
{
    m_pageDataMap[pageNumber] = {.state = LoadState::Loaded, .sentences = sentences, .images = images};
    m_pageDataMap[pageNumber].playbacks.resize(sentences.length());

    std::cout << "PPageNo: " << pageNumber << std::endl;
    std::cout << "PPlaybacks: " << m_pageDataMap[pageNumber].playbacks.size() << std::endl;

    if (pageNumber == 0 && m_pdfStructure[pageNumber].imageCount > 0)
    {
        m_coverImage = m_pageDataMap[pageNumber].images[0];
    }

    if (pageNumber == m_appState.currentPage)
    {
        restartImageTimer();
    }

    driveSynthesis();
}

void AppController::onPageExtractionFailed(uint16_t pageNumber, const QString &error)
{
    m_pageDataMap[pageNumber].state = LoadState::Failed;
    driveSynthesis();
}

void AppController::onSythesisComplete(uint16_t pageNumber, uint16_t sentenceIdx,
                                       const QByteArray &audioData, int sampleRate)
{
    {
        QMutexLocker lock(&m_cancelMutex);
        if (m_cancelList.contains({pageNumber, sentenceIdx}))
        {
            m_cancelList.removeOne({pageNumber, sentenceIdx});
            return;
        }
    }

    emit statusMessage(QString("onSythesisComplete: %1 %2 %3")
                       .arg(m_pageDataMap[pageNumber].sentences[sentenceIdx])
                       .arg(pageNumber)
                       .arg(sentenceIdx)); 

    m_pageDataMap[pageNumber].playbacks[sentenceIdx].setValues(LoadState::Loaded, audioData, sampleRate);
    m_plLookAhead.enqueue(Position{pageNumber, sentenceIdx});
    m_synthQueue.dequeue();
    if (!m_audioManager->isSpeechPlaying())
    {
        playTargetPlayback();
    }
    driveSynthesis();
}

void AppController::onSythesisFailed(uint16_t pageNumber, uint16_t sentenceIdx, const QString &error)
{
    {
        QMutexLocker lock(&m_cancelMutex);
        if (m_cancelList.contains({pageNumber, sentenceIdx}))
        {
            m_cancelList.removeOne({pageNumber, sentenceIdx});
            return;
        }
    }

    m_pageDataMap[pageNumber].playbacks[sentenceIdx].setValues(LoadState::Failed);
    m_plLookAhead.enqueue(Position{pageNumber, sentenceIdx});
    m_synthQueue.dequeue();
    driveSynthesis();
}

void AppController::onSpeechFinished(uint16_t pageNumber, uint16_t sentenceIdx)
{
    if (sentenceIdx == m_pdfStructure[pageNumber].sentenceCount - 1)
    {
        m_allPlaybacksPlayed = true;
        updateCurrentPage(m_appState.currentPage + 1);
    }
    playTargetPlayback();
    driveSynthesis();
}

void AppController::onMusicFinished()
{
    if (m_isPlaying)
    {
        m_audioManager->playMusic(m_appState.musicPath);
    }
}

