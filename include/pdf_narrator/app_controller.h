#pragma once

#include "audio_manager.h"
#include "pdf_parser.h"
#include "thread_manager.h"
#include "tts_manager.h"
#include <QObject>
#include <QMap>
#include <QQueue>
#include <QTimer>
#include <QJsonObject>
#include <iostream>
#include <optional>
#include <qtmetamacros.h>

enum class LoadState
{
    Unloaded,
    Loading,
    Loaded,
    Failed
};

struct PlaybackData
{
    LoadState state = LoadState::Unloaded;
    QByteArray audio;
    float sampleRate;

    void setValues(LoadState st)
    {
        state = st;
    }
    void setValues(LoadState st, QByteArray au, float sr)
    {
        state = st;
        audio = au;
        sampleRate = sr;
    }
};

struct PageData
{
    LoadState state = LoadState::Unloaded;
    QVector<QString> sentences;
    QVector<QImage> images;
    QVector<PlaybackData> playbacks;
};

struct Position
{
    uint16_t pageNo;
    uint16_t sentenceIdx;

    bool operator==(const Position &other) const
    {
        return other.pageNo == pageNo && other.sentenceIdx == sentenceIdx;
    }
    bool operator!=(const Position &other) const
    {
        return !(*this == other);
    }
    bool operator<(const Position &other) const
    {
        return (pageNo < other.pageNo) ||
               ((pageNo == other.pageNo) && (sentenceIdx < other.sentenceIdx));
    }
    bool operator>(const Position &other) const
    {
        return (*this != other) && !(*this < other);
    }
    bool operator<=(const Position &other) const
    {
        return !(*this > other);
    }
    bool operator>=(const Position &other) const
    {
        return !(*this < other);
    }
};

struct AppState
{
    bool isLoaded = false;
    QString pdfPath = "";
    QString musicPath = "";
    uint16_t currentPage = 0;
    uint16_t sentenceIdx = 0;
    uint8_t imageIdx = 0;
    int speakerId = 0;
    float ttsSpeed = 1.0;

    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["PDFPath"] = pdfPath;
        obj["MusicPath"] = musicPath;
        obj["CurrentPage"] = currentPage;
        obj["SentenceIdx"] = sentenceIdx;
        obj["ImageIdx"] = imageIdx;
        obj["SpeakerId"] = speakerId;
        obj["TTSSpeed"] = ttsSpeed;

        return obj;
    }

    void fromJson(const QJsonObject &obj)
    {
        pdfPath = obj.value("PDFPath").toString();
        if (!pdfPath.isEmpty())
        {
            isLoaded = true;
        }

        musicPath = obj.value("MusicPath").toString();
        currentPage = obj.value("CurrentPage").toInt();
        sentenceIdx = obj.value("SentenceIdx").toInt();
        imageIdx = obj.value("ImageIdx").toInt();
        speakerId = obj.value("SpeakerId").toInt();
        ttsSpeed = obj.value("TTSSpeed").toDouble();
    }
};

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isInitialized READ isInitialized NOTIFY isInitializedChanged)
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(bool isRestart READ isRestart NOTIFY isRestartChanged)
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)
    Q_PROPERTY(int totalPages READ totalPages NOTIFY totalPagesChanged)
    Q_PROPERTY(int currentPage READ currentPage WRITE goToPage NOTIFY currentPageChanged)
    Q_PROPERTY(bool isMusicEnabled READ isMusicEnabled NOTIFY isMusicEnabledChanged)
    Q_PROPERTY(float ttsSpeed READ ttsSpeed WRITE setTtsSpeed NOTIFY ttsSpeedChanged)
    Q_PROPERTY(float ttsSpeaker READ ttsSpeaker WRITE setTtsSpeaker NOTIFY ttsSpeakerChanged)
    Q_PROPERTY(QStringList ttsVoices READ ttsVoices NOTIFY ttsVoicesChanged)
    Q_PROPERTY(float musicVolume READ musicVolume WRITE setMusicVolume NOTIFY musicVolumeChanged)
    Q_PROPERTY(QString imageId READ imageId NOTIFY imageIdChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController();

    // Property getters
    bool isInitialized() const
    {
        return m_isInitialized;
    }

    bool isPlaying() const
    {
        return m_isPlaying;
    }

    bool isRestart() const
    {
        return m_isRestart;
    }

    bool isBusy() const
    {
        return m_isBusy;
    }

    int totalPages() const
    {
        return m_totalPages;
    }

    int currentPage() const
    {
        return m_appState.currentPage;
    }

    float ttsSpeed() const
    {
        return m_appState.ttsSpeed;
    }

    QStringList ttsVoices() const
    {
        return m_ttsManager->getVoices();
    }

    int ttsSpeaker() const
    {
        return m_appState.speakerId;
    }

    float musicVolume() const
    {
        return m_audioManager->musicVolume();
    }

    bool isMusicEnabled() const
    {
        return m_audioManager->isMusicEnabled();
    }

    QString imageId() const
    {
        return m_currentImage.has_value() ? QString("%1").arg(m_imageId) : "";
    }

    QImage currentImage()
    {
        return m_currentImage.value_or(QImage());
    }

    // Property setters
    void setTtsSpeed(float speed);
    void setTtsSpeaker(int speakerId);
    void setMusicVolume(float volume)
    {
        m_audioManager->setMusicVolume(volume);
    }

    void initialize();
    Q_INVOKABLE void openPDF(const QString &filePath);
    Q_INVOKABLE void openMusic(const QString &musicPath);
    Q_INVOKABLE void toggleMusic();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void play();
    Q_INVOKABLE void restart();
    Q_INVOKABLE void prevLine();
    Q_INVOKABLE void nextLine();
    Q_INVOKABLE void prevPage();
    Q_INVOKABLE void nextPage();
    void goToPage(uint16_t page);

signals:
    void errorOccurred(const QString &error);
    void statusMessage(const QString &message);
    void isInitializedChanged();
    void isPlayingChanged();
    void isBusyChanged();
    void isRestartChanged();
    void totalPagesChanged();
    void currentPageChanged();
    void ttsSpeedChanged();
    void ttsVoicesChanged();
    void ttsSpeakerChanged();
    void musicVolumeChanged();
    void isMusicEnabledChanged();
    void imageIdChanged();

private slots:
    void onTtsInitializationComplete();
    void onTtsInitializationFailed(QString error);
    void onPdfLoaded(int totalPages);
    void onPdfLoadFailed(const QString &error);
    void onPdfStructureExtracted(QVector<PageIndex> structure);
    void onPageExtracted(uint16_t pageNumber, QVector<QString> sentences, QVector<QImage> images);
    void onPageExtractionFailed(uint16_t pageNumber, const QString &error);
    void onSythesisComplete(uint16_t pageNumber, uint16_t sentenceIdx, const QByteArray &audioData,
                            int sampleRate);
    void onSythesisFailed(uint16_t pageNumber, uint16_t sentenceIdx, const QString &error);
    void onSpeechFinished(uint16_t pageNumber, uint16_t sentenceIdx);
    void onMusicFinished();

private:
    void loadState();
    void saveState();
    void navigateTo(Position target);
    void cancelOutstandingTasks(Position target, bool all = false);
    void resetState();
    void parsePage(uint16_t pageNumber);
    void prevPosition(Position &pos);
    void nextPosition(Position &pos);
    bool isEndPosition(Position &pos);
    void driveSynthesis();
    void evictHistory();
    void updateCurrentPage(int page, bool force = false);
    void playTargetPlayback();
    void restartImageTimer();
    void updateImage();

    std::unique_ptr<PDFParser> m_pdfParser;
    std::unique_ptr<TTSManager> m_ttsManager;
    std::unique_ptr<AudioManager> m_audioManager;
    std::unique_ptr<ThreadManager> m_threadManager;

    bool m_isBusy = false;
    bool m_isPlaying = true;
    bool m_isRestart = false;
    bool m_isPdfLoaded = false;
    bool m_isInitialized = false;

    AppState m_appState;
    uint16_t m_totalPages = 0;
    QMap<uint16_t, PageData> m_pageDataMap;
    QVector<PageIndex> m_pdfStructure;
    Position m_synthPos = {0, 0};
    Position m_playbackPos = {0, 0};
    QQueue<Position> m_plLookAhead;  // preloaded playbacks
    QQueue<Position> m_plHistory;    // previous playbacks
    QQueue<Position> m_synthQueue;   // sentences under synthesis
    uint8_t m_maxLookAhead = 10;
    uint8_t m_maxHistory = 5;
    uint8_t m_maxSynth = 5;
    QMutex m_cancelMutex;
    QVector<Position> m_cancelList;
    bool m_allPlaybacksPlayed = true;

    QTimer m_imageTimer;
    uint32_t m_imageId = 0;
    bool m_allImagesDisplayed = false;
    std::optional<QImage> m_coverImage;
    std::optional<QImage> m_currentImage;
};
