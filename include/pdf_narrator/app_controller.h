#pragma once

#include "audio_manager.h"
#include "foreground_playback_service.h"
#include "pdf_parser.h"
#include "thread_manager.h"
#include "tts_manager.h"
#include <cstdint>
#include <QObject>
#include <QSettings>
#include <QQueue>
#include <QTimer>
#include <QDir>
#include <qqmlregistration.h>
#include <QNetworkAccessManager>
#include <qtmetamacros.h>
#include "model_downloader.h"

enum class LoadState : uint8_t { UNLOADED, LOADING, LOADED, FAILED };

struct Position {
	uint16_t pageNo{ 0 };
	uint16_t sentenceIdx{ 0 };

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

struct AppState {
	QString pdfPath{};
	QString musicPath{};
	uint16_t page{ 0 };
	uint16_t sentenceIdx{ 0 };
	uint16_t playbackIdx{ 0 };
	int modelId{ 0 };
	int speakerId{ 0 };
	float ttsSpeed{ 1.0 };
	float musicVolume{ 0.3f };
	bool musicEnabled{ true };
	QSettings settings;

	void loadState();
	void saveState();

	template <typename T, typename DefaultType>
	void get(T &member, const QString &key, DefaultType &&defaultValue)
	{
		member = settings.value(key, QVariant::fromValue(defaultValue)).template value<T>();
	}

	template <typename T, typename U> void set(T &member, const QString &key, U &&value)
	{
		if (member == value)
			return;

		member = std::forward<U>(value);
		settings.setValue(key, QVariant::fromValue(member));
		settings.sync();
	}
};

struct PageData {
	LoadState state{ LoadState::UNLOADED };
	QString errorMessage{};
	QVector<QString> sentences{};
	QVector<QImage> images{};
	QList<PlaybackSegment> playbackSegments{};
};

struct PlaybackData {
	LoadState state{ LoadState::UNLOADED };
	Position pos{};
	QString errorMessage{};
	QByteArray audio{};
	int sampleRate{};
};

class AppController : public QObject {
	Q_OBJECT
	QML_ELEMENT
	QML_UNCREATABLE("AppController is provided from C++")

	Q_PROPERTY(bool needsDownload READ needsDownload NOTIFY needsDownloadChanged)
	Q_PROPERTY(bool initializingTts READ initializingTts NOTIFY initializingTtsChanged)
	Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)
	Q_PROPERTY(PlaybackState playbackState READ playbackState NOTIFY playbackStateChanged)
	Q_PROPERTY(int totalPages READ totalPages NOTIFY totalPagesChanged)
	Q_PROPERTY(int currentPage READ currentPage WRITE goToPage NOTIFY currentPageChanged)
	Q_PROPERTY(QStringList ttsModels READ ttsModels CONSTANT)
	Q_PROPERTY(QStringList ttsVoices READ ttsVoices NOTIFY ttsVoicesChanged)
	Q_PROPERTY(int ttsModel READ ttsModel WRITE setTtsModel NOTIFY ttsModelChanged)
	Q_PROPERTY(float ttsSpeed READ ttsSpeed WRITE setTtsSpeed NOTIFY ttsSpeedChanged)
	Q_PROPERTY(int ttsSpeaker READ ttsSpeaker WRITE setTtsSpeaker NOTIFY ttsSpeakerChanged)
	Q_PROPERTY(bool isMusicEnabled READ isMusicEnabled NOTIFY isMusicEnabledChanged)
	Q_PROPERTY(float musicVolume READ musicVolume WRITE setMusicVolume NOTIFY musicVolumeChanged)
	Q_PROPERTY(QString imageId READ imageId NOTIFY imageIdChanged)

public:
	explicit AppController(QObject *parent = nullptr);
	~AppController();

	// Property getters
	enum class PlaybackState : uint8_t { PLAYING, PAUSED, RESTART };
	Q_ENUM(PlaybackState)

	enum class SeekDirection : uint8_t { NONE, NEXT, PREV };
	Q_ENUM(SeekDirection)

	bool needsDownload() const
	{
		return m_needsDownload;
	}

	bool initializingTts() const
	{
		return m_initializingTts;
	}

	bool isBusy() const
	{
		return m_isBusy;
	}

	PlaybackState playbackState() const
	{
		return m_playbackState;
	}

	int totalPages() const
	{
		return m_totalPages;
	}

	int currentPage() const
	{
		return m_appState.page;
	}

	int ttsModel() const
	{
		return m_appState.modelId;
	}

	float ttsSpeed() const
	{
		return m_appState.ttsSpeed;
	}

	int ttsSpeaker() const
	{
		return m_appState.speakerId;
	}

	QString imageId() const
	{
		return m_currentImage.has_value() ? QString("%1").arg(m_imageId) : "";
	}

	QImage currentImage()
	{
		return m_currentImage.value_or(QImage());
	}

	float musicVolume() const
	{
		return m_audioManager->musicVolume();
	}

	bool isMusicEnabled() const
	{
		return m_audioManager->isMusicEnabled();
	}

	QStringList ttsModels() const;
	QStringList ttsVoices() const;

	// Property setters
	void setTtsModel(int modelId);
	void setTtsSpeed(float speed);
	void setTtsSpeaker(int speakerId);
	void setMusicVolume(float volume);

	// QML Invokables
	Q_INVOKABLE void download();
	Q_INVOKABLE void downloadModel(int modelId);
	Q_INVOKABLE bool isModelDownloaded(int modelId) const;
	Q_INVOKABLE void openPDF(const QString &uri);
	Q_INVOKABLE void openMusic(const QString &uri);
	Q_INVOKABLE void toggleMusic();
	Q_INVOKABLE void pause();
	Q_INVOKABLE void play();
	Q_INVOKABLE void restart();
	Q_INVOKABLE void seekSentence(SeekDirection dir);
	Q_INVOKABLE void goToPage(uint16_t page);

signals:
	void infoMessage(const QString &message);
	void infoClose();
	void statusMessage(bool persistent, const QString &message);
	void errorOccurred(bool persistent, const QString &error);
	void downloadProgress(float percentage);
	void modelDownloadProgress(int modelId, float percentage);
	void modelDownloadFinished(int modelId, bool success);
	void needsDownloadChanged();
	void initializingTtsChanged();
	void isBusyChanged();
	void playbackStateChanged();
	void totalPagesChanged();
	void currentPageChanged();
	void ttsVoicesChanged();
	void ttsModelChanged();
	void ttsSpeedChanged();
	void ttsSpeakerChanged();
	void musicVolumeChanged();
	void isMusicEnabledChanged();
	void imageIdChanged();

private slots:
	void onTtsInitializationComplete();
	void onTtsInitializationFailed(const QString &error);
	void onPdfLoaded(int pageCount, const QVector<uint16_t> &sentenceCounts);
	void onPdfLoadFailed(const QString &error);
	void onPageExtracted(int pageNumber, const QStringList &sentences, const QList<QImage> &images,
						 const QList<PlaybackSegment> &segments, uint8_t genId);
	void onPageExtractionFailed(int pageNumber, const QString &error, uint8_t genId);
	void onSynthesisComplete(uint16_t page, uint16_t sentenceIdx, const QByteArray &audioData,
							 int sampleRate, uint8_t genId);
	void onSynthesisFailed(uint16_t page, uint16_t sentenceIdx, const QString &error,
						   uint8_t genId);
	void onSpeechFinished(uint16_t page, uint16_t sentenceIdx);
	void onMusicFinished();

private:
	void checkDownloads();
	void processDownloadQueue(QNetworkAccessManager *networkManager);
	void initializeTts();
	void initialize();
	void applyTtsModel(int modelId);
	// Cancels whatever synthesis request is currently outstanding (if any)
	// and resets bookkeeping so a fresh one can be submitted. Safe to call
	// even when nothing is actually in flight.
	void cancelSynthesis();
	bool prevPosition(Position &pos);
	bool nextPosition(Position &pos);
	void resetState();
	void parsePage(uint16_t page);
	void parseTillPage(uint16_t page);
	bool isPositionValid(Position &pos);
	void driveSynthesis();
	void updateCurrentPage(uint16_t page);
	void evictHistory();
	void drivePlayback();
	void changeSynthesisPos(Position pos);
	void setPlaybackIdx();

private:
	std::unique_ptr<PDFParser> m_pdfParser;
	std::unique_ptr<TTSManager> m_ttsManager;
	std::unique_ptr<AudioManager> m_audioManager;
	std::unique_ptr<ThreadManager> m_threadManager;
	std::unique_ptr<ModelDownloader> m_modelDownloader;
	std::unique_ptr<ForegroundPlaybackService> m_foregroundService;

	AppState m_appState;
	QDir m_modelsDir;
	QMap<uint16_t, PageData> m_pageDataMap;
	QVector<uint16_t> m_sentenceCounts;

	QQueue<Position> m_synthesizing;
	QQueue<PlaybackData> m_playbacks;

	QTimer m_imageTimer;
	QTimer m_sentenceTimer;
	std::optional<QImage> m_coverImage;
	std::optional<QImage> m_currentImage;

	std::atomic<uint8_t> m_parseGenId{ 0 };
	std::atomic<uint8_t> m_synthesisGenId{ 0 };
	uint16_t m_totalPages{ 0 };
	uint16_t m_parsePage{ 0 };
	uint32_t m_imageId{ 0 };
	Position m_synthesisPos;
	SeekDirection m_seekDirection{ SeekDirection::NONE };
	PlaybackState m_playbackState{ PlaybackState::PLAYING };

	int m_pendingModelId{ -1 };

	uint8_t m_lookAheadCount{ 0 };
	uint8_t m_maxLookAheadCount{ 5 };
	uint8_t m_historyCount{ 0 };
	uint8_t m_maxHistoryCount{ 5 };
	bool m_initializingTts{ false };
	bool m_needsDownload{ false };
	bool m_isBusy{ false };
	bool m_pdfLoaded{ false };
	bool m_synthesisFinished{ false };
	bool m_synthesisTaskInFlight{ false };
};
