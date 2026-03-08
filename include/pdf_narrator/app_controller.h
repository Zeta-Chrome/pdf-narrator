#pragma once

#include <QObject>
#include <QString>
#include <QImage>
#include <QQueue>
#include <QTimer>
#include <optional>
#include "pdf_parser.h"
#include "tts_manager.h"
#include "audio_manager.h"
#include "image_provider.h"
#include "thread_manager.h"

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
    inline size_t qHash(const Position &pos, size_t seed = 0)
    {
        return qHashMulti(seed, pos.pageNo, pos.sentenceIdx);
    }
};

class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool isPlaying READ isPlaying NOTIFY isPlayingChanged)
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)
    Q_PROPERTY(int totalPages READ totalPages NOTIFY totalPagesChanged)
    Q_PROPERTY(int currentPage READ currentPage WRITE goToPage NOTIFY currentPageChanged)
    Q_PROPERTY(bool isMusicEnabled READ isMusicEnabled NOTIFY isMusicEnabledChanged)
    Q_PROPERTY(float ttsSpeed READ ttsSpeed WRITE setTtsSpeed NOTIFY ttsSpeedChanged)
    Q_PROPERTY(float musicVolume READ musicVolume WRITE setMusicVolume NOTIFY musicVolumeChanged)
    Q_PROPERTY(QString imageId READ imageId NOTIFY imageIdChanged)

public:
    explicit AppController(QObject *parent = nullptr);
    ~AppController() = default;

    // Property getters
    bool isPlaying() const
    {
        return m_isPlaying;
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
        return m_currentPage;
    }
    float ttsSpeed() const
    {
        return m_ttsSpeed;
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
        return m_hasImage ? QString("%1").arg(m_imageId) : "";
    }

    // Property setters
    void setTtsSpeed(float speed)
    {
        m_ttsSpeed = speed;
    }
    void setMusicVolume(float volume)
    {
        m_audioManager->setMusicVolume(volume);
    }

    // Q_INVOKABLE methods for QML
    Q_INVOKABLE void openPDF(const QString &filePath);
    Q_INVOKABLE void openMusic(const QString &musicPath);
    Q_INVOKABLE void toggleMusic();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void play();
    Q_INVOKABLE void prevLine();
    Q_INVOKABLE void nextLine();
    Q_INVOKABLE void prevPage();
    Q_INVOKABLE void nextPage();
    void goToPage(uint16_t page);

    // Image provider access (called from QQuickImageProvider)
    QImage getCurrentImage() 
    {
        return m_currentImage;
    }

signals:
    void errorOccurred(const QString &error);
    void statusMessage(const QString &message);
    void isBusyChanged();
    void isPlayingChanged();
    void totalPagesChanged();
    void currentPageChanged();
    void ttsSpeedChanged();
    void musicVolumeChanged();
    void isMusicEnabledChanged();
    void imageIdChanged();

private slots:
    void onPdfLoaded(int totalPages);
    void onPdfLoadFailed(const QString &error);
    void onPageExtracted(uint16_t pageNumber, QVector<QString> sentences, QVector<QImage> images);
    void onPageExtractionFailed(uint16_t pageNumber, const QString &error);
    void onSythesisComplete(uint16_t pageNumber, uint16_t sentenceIdx, const QByteArray &audioData,
                            int sampleRate);
    void onSythesisFailed(uint16_t pageNumber, uint16_t sentenceId, const QString &error);
    void onSpeechFinished();
    void onSpeechFailed();
    void onMusicFinished();

private:
    void nextPosition(Position &pos);
    void prevPosition(Position &pos);
    void parsePage(int pageNumber);
    void synthesizeTargetSentence();
    void playTargetPlayback();
    void navigateTo(Position target);
    void evictHistory();
    void cancelOutstandingTasks(Position position);
    void updateImage();
    void restartImageTimer();
    void resetOnPdfLoad();

    std::unique_ptr<PDFParser> m_pdfParser;
    std::unique_ptr<TTSManager> m_ttsManager;
    std::unique_ptr<AudioManager> m_audioManager;
    std::unique_ptr<ThreadManager> m_threadManager;

    QString m_pdfPath;
    QString m_musicPath;
    bool m_isBusy;
    bool m_isPlaying;
    uint16_t m_totalPages;
    uint16_t m_currentPage;
    int m_ttsSpeed;

    QMap<uint16_t, PageData> m_pageDataMap;
    QQueue<Position> m_synthQueue;    // sentences under synthesis
    QQueue<Position> m_plLookAhead;   // sentences already synthesized
    QQueue<Position> m_plHistory;     // playback history
    QVector<Position> m_cancelledTasks;  // cancelledTasks
    QMutex m_cancelMutex;
    Position m_synthPos;     // Position of next sentence to sythesize
    Position m_playbackPos;  // Position of next sentence to play
    uint8_t m_maxLookAheadCount;
    uint8_t m_maxHistoryCount;
    uint8_t m_maxQueuedCount; 

    uint8_t m_imageIdx; // Index of the next Image to be displayed
    uint32_t m_imageId; // Image identification number
    bool m_hasImage;
    QTimer m_imageTimer; 
    QImage m_currentImage;
    std::optional<QImage> m_coverImage;
    bool m_allImagesDisplayed;
};
