#include "app_controller.h"
#include "model_downloader.h"
#include <QStandardPaths>

#define SENTENCE_BREAK_PERIOD 250
#define IMAGE_DISPLAY_PERIOD 3000

void AppState::loadState()
{
	pdfPath = settings.value("app/pdfPath").toString();
	musicPath = settings.value("app/musicPath").toString();
	page = settings.value("app/page", 0).toUInt();
	sentenceIdx = settings.value("app/sentenceIdx", 0).toUInt();
	playbackIdx = settings.value("app/playbackIdx", 0).toUInt();
	modelId = settings.value("tts/modelId", 0).toInt();
	speakerId = settings.value(QString("tts/%1/speakerId").arg(modelId), 0).toInt();
	ttsSpeed = settings.value("tts/speed", 1.0f).toFloat();
	musicVolume = (float)settings.value("app/musicVolume", 0.3).toDouble();
	musicEnabled = settings.value("app/musicEnabled", true).toBool();
}

void AppState::saveState()
{
	settings.setValue("app/pdfPath", pdfPath);
	settings.setValue("app/musicPath", musicPath);
	settings.setValue("app/page", page);
	settings.setValue("app/sentenceIdx", sentenceIdx);
	settings.setValue("app/playbackIdx", playbackIdx);
	settings.setValue("tts/modelId", modelId);
	settings.setValue(QString("tts/%1/speakerId").arg(modelId), speakerId);
	settings.setValue("tts/speed", ttsSpeed);
	settings.setValue("app/musicVolume", musicVolume);
	settings.setValue("app/musicEnabled", musicEnabled);
	settings.sync();
}

AppController::AppController(QObject *parent)
	: QObject(parent)
	, m_modelsDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/models")
{
	m_pdfParser = std::make_unique<PDFParser>();
	m_ttsManager = std::make_unique<TTSManager>();
	m_audioManager = std::make_unique<AudioManager>();
	m_threadManager = std::make_unique<ThreadManager>();
	m_modelDownloader = std::make_unique<ModelDownloader>(m_modelsDir.absolutePath());
	m_foregroundService = std::make_unique<ForegroundPlaybackService>();

	m_sentenceTimer.setInterval(SENTENCE_BREAK_PERIOD);
	m_sentenceTimer.setSingleShot(true);
	connect(&m_sentenceTimer, &QTimer::timeout, this, &AppController::drivePlayback);
	m_imageTimer.setInterval(IMAGE_DISPLAY_PERIOD);
	m_imageTimer.setSingleShot(true);
	connect(&m_imageTimer, &QTimer::timeout, this, &AppController::drivePlayback);

	connect(m_ttsManager.get(), &TTSManager::ttsInitializationComplete, this,
			&AppController::onTtsInitializationComplete);
	connect(m_ttsManager.get(), &TTSManager::ttsInitializationFailed, this,
			&AppController::onTtsInitializationFailed);
	connect(m_pdfParser.get(), &PDFParser::pdfLoaded, this, &AppController::onPdfLoaded);
	connect(m_pdfParser.get(), &PDFParser::pdfLoadFailed, this, &AppController::onPdfLoadFailed);
	connect(m_pdfParser.get(), &PDFParser::pageExtracted, this, &AppController::onPageExtracted);
	connect(m_pdfParser.get(), &PDFParser::pageExtractionFailed, this,
			&AppController::onPageExtractionFailed);
	connect(m_ttsManager.get(), &TTSManager::synthesisComplete, this,
			&AppController::onSynthesisComplete);
	connect(m_ttsManager.get(), &TTSManager::synthesisFailed, this,
			&AppController::onSynthesisFailed);
	connect(m_audioManager.get(), &AudioManager::speechFinished, this,
			&AppController::onSpeechFinished);
	connect(m_audioManager.get(), &AudioManager::musicFinished, this,
			&AppController::onMusicFinished);
	connect(m_modelDownloader.get(), &ModelDownloader::statusMessage, this,
			&AppController::statusMessage);
	connect(m_modelDownloader.get(), &ModelDownloader::errorOccurred, this,
			&AppController::errorOccurred);
	connect(m_modelDownloader.get(), &ModelDownloader::downloadProgress, this,
			[this](int modelId, float percentage) {
				emit modelDownloadProgress(modelId, percentage);
				if (modelId == m_appState.modelId)
					emit downloadProgress(percentage);
			});
	connect(m_modelDownloader.get(), &ModelDownloader::downloadFinished, this,
			[this](int modelId, bool success) {
				emit modelDownloadFinished(modelId, success);

				if (modelId == m_pendingModelId) {
					m_pendingModelId = -1;
					if (success)
						applyTtsModel(modelId);
				}

				if (success && modelId == m_appState.modelId) {
					m_needsDownload = false;
					emit needsDownloadChanged();
					QTimer::singleShot(0, this, &AppController::initialize);
				}
			});
	connect(this, &AppController::playbackStateChanged, this, [this]() {
		if (m_playbackState == PlaybackState::PLAYING)
			m_foregroundService->start("Reading", QString());
		else
			m_foregroundService->stop();
	});

	connect(this, &AppController::infoMessage, this, [this](const QString &sentence) {
		if (m_playbackState == PlaybackState::PLAYING)
			m_foregroundService->updateNotification("Reading", sentence);
	});

	checkDownloads();

	if (!m_needsDownload)
		QTimer::singleShot(0, this, &AppController::initialize);
}

AppController::~AppController()
{
	resetState();
}

void AppController::checkDownloads()
{
	m_needsDownload = !isModelDownloaded(m_appState.modelId);
}

bool AppController::isModelDownloaded(int modelId) const
{
	return m_modelDownloader->isModelDownloaded(modelId);
}

void AppController::download()
{
	downloadModel(m_appState.modelId);
}

void AppController::downloadModel(int modelId)
{
	m_modelDownloader->downloadModel(modelId);
}

void AppController::initializeTts()
{
	m_initializingTts = true;
	emit initializingTtsChanged();

	const auto &models = ModelDownloader::availableModels();
	m_threadManager->submitTask(ThreadType::TTSManager, [this, models]() {
		m_ttsManager->initialize(m_modelsDir.filePath(models[m_appState.modelId]));
	});
}

void AppController::initialize()
{
	m_appState.loadState();

	m_audioManager->setMusicVolume(m_appState.musicVolume);
	if (!m_appState.musicEnabled)
		m_audioManager->toggleMusic();

	emit ttsVoicesChanged();
	emit ttsModelChanged();
	emit ttsSpeedChanged();
	emit ttsSpeakerChanged();
	emit musicVolumeChanged();
	emit isMusicEnabledChanged();

	initializeTts();

	if (!m_appState.pdfPath.isEmpty())
		openPDF(m_appState.pdfPath);

	if (!m_appState.musicPath.isEmpty())
		openMusic(m_appState.musicPath);
}

void AppController::onTtsInitializationComplete()
{
	m_initializingTts = false;
	emit initializingTtsChanged();
	emit statusMessage(false, QString("%1 TTS model initialized")
								  .arg(ModelDownloader::availableModels()[m_appState.modelId]));

	if (m_pdfLoaded) {
		if (m_playbackState != PlaybackState::PAUSED) {
			m_playbackState = PlaybackState::PLAYING;
			emit playbackStateChanged();
		}

		parsePage(0);
		driveSynthesis();
	}
}

void AppController::onTtsInitializationFailed(const QString &error)
{
	m_initializingTts = false;
	emit initializingTtsChanged();
	emit errorOccurred(true, error + " Select another Model");
}

QStringList AppController::ttsModels() const
{
	return ModelDownloader::availableModels();
}

QStringList AppController::ttsVoices() const
{
	const QString currentModel = ModelDownloader::availableModels()[m_appState.modelId];
	return ModelDownloader::modelVoices()[currentModel];
}

void AppController::setTtsModel(int modelId)
{
	if (m_appState.modelId == modelId)
		return;

	if (!isModelDownloaded(modelId)) {
		m_pendingModelId = modelId;
		emit statusMessage(
			false, QString("Downloading %1...").arg(ModelDownloader::availableModels()[modelId]));
		downloadModel(modelId);
		return;
	}

	applyTtsModel(modelId);
}

void AppController::applyTtsModel(int modelId)
{
	m_appState.set(m_appState.modelId, "tts/modelId", modelId);
	m_appState.get(m_appState.speakerId, QString("tts/%1/speakerId").arg(modelId), 0);
	emit ttsModelChanged();
	emit ttsVoicesChanged();
	emit ttsSpeakerChanged();

	if (m_pdfLoaded) {
		m_audioManager->stopSpeech();
		emit infoClose();
		m_playbacks.clear();
		m_lookAheadCount = 0;
		m_historyCount = 0;
		m_synthesisFinished = false;
		m_synthesisPos = { m_appState.page, m_appState.sentenceIdx };
		cancelSynthesis();
	}

	initializeTts();
}

void AppController::setTtsSpeed(float speed)
{
	if (qFuzzyCompare(m_appState.ttsSpeed, speed))
		return;

	m_appState.set(m_appState.ttsSpeed, "tts/speed", speed);
	emit ttsSpeedChanged();

	if (!m_pdfLoaded)
		return;

	m_audioManager->stopSpeech();
	emit infoClose();
	m_playbacks.clear();
	m_lookAheadCount = 0;
	m_historyCount = 0;
	m_synthesisFinished = false;
	m_synthesisPos = { m_appState.page, m_appState.sentenceIdx };

	cancelSynthesis();

	m_isBusy = true;
	emit isBusyChanged();

	driveSynthesis();
	drivePlayback();
}

void AppController::setTtsSpeaker(int speakerId)
{
	if (m_appState.speakerId == speakerId)
		return;

	m_appState.set(m_appState.speakerId, QString("tts/%1/speakerId").arg(m_appState.modelId),
				   speakerId);
	emit ttsSpeakerChanged();

	if (!m_pdfLoaded)
		return;

	m_audioManager->stopSpeech();
	emit infoClose();
	m_playbacks.clear();
	m_lookAheadCount = 0;
	m_historyCount = 0;
	m_synthesisFinished = false;
	m_synthesisPos = { m_appState.page, m_appState.sentenceIdx };

	cancelSynthesis();

	m_isBusy = true;
	emit isBusyChanged();

	driveSynthesis();
	drivePlayback();
}

void AppController::cancelSynthesis()
{
	m_synthesisGenId++;
	m_synthesizing.clear();
	m_synthesisTaskInFlight = false;
}

void AppController::setMusicVolume(float volume)
{
	if (qFuzzyCompare(m_appState.musicVolume, volume))
		return;

	m_audioManager->setMusicVolume(volume);
	m_appState.set(m_appState.musicVolume, "app/musicVolume", volume);
	emit musicVolumeChanged();
}

void AppController::resetState()
{
	m_audioManager->stopSpeech();
	emit infoClose();
	m_sentenceTimer.stop();
	m_imageTimer.stop();
	m_pageDataMap.clear();
	m_playbacks.clear();
	m_coverImage.reset();
	m_currentImage.reset();
	m_lookAheadCount = 0;
	m_historyCount = 0;
	m_synthesisFinished = false;
	m_seekDirection = SeekDirection::NONE;

	m_parseGenId++;
	m_synthesisGenId++;
	m_synthesizing.clear();
	m_synthesisTaskInFlight = false;

	m_imageId = 0;
	emit imageIdChanged();
}

void AppController::openPDF(const QString &uri)
{
	resetState();

	if (m_pdfLoaded) {
		m_appState.set(m_appState.page, "app/page", 0);
		m_appState.set(m_appState.sentenceIdx, "app/sentenceIdx", 0);
		m_appState.set(m_appState.playbackIdx, "app/playbackIdx", 0);
	}
	m_appState.set(m_appState.pdfPath, "app/pdfPath", uri);

	m_isBusy = true;
	emit isBusyChanged();

	m_pdfLoaded = false;
	m_threadManager->submitTask(ThreadType::PDFParser, [this, uri] { m_pdfParser->loadPdf(uri); });
}

bool AppController::isPositionValid(Position &pos)
{
	return pos.pageNo < m_totalPages && m_sentenceCounts[pos.pageNo] > pos.sentenceIdx;
}

bool AppController::prevPosition(Position &pos)
{
	Position p = pos;
	if (p.sentenceIdx > 0) {
		p.sentenceIdx--;
		pos = p;
		return true;
	}

	do {
		if (p.pageNo == 0)
			return false;

		p.pageNo--;
	} while (m_pageDataMap[p.pageNo].state == LoadState::FAILED || m_sentenceCounts[p.pageNo] == 0);
	p.sentenceIdx = m_sentenceCounts[p.pageNo] - 1;
	pos = p;

	return true;
}

bool AppController::nextPosition(Position &pos)
{
	Position p = pos;
	if (m_sentenceCounts[p.pageNo] != 0 && p.sentenceIdx < m_sentenceCounts[p.pageNo] - 1) {
		p.sentenceIdx++;
		pos = p;
		return true;
	}

	do {
		if (p.pageNo == m_totalPages - 1)
			return false;

		p.pageNo++;
	} while (m_pageDataMap[p.pageNo].state == LoadState::FAILED || m_sentenceCounts[p.pageNo] == 0);
	p.sentenceIdx = 0;
	pos = p;

	return true;
}

void AppController::onPdfLoaded(int totalPages, const QVector<uint16_t> &sentenceCounts)
{
	m_totalPages = totalPages;
	emit totalPagesChanged();
	m_sentenceCounts = sentenceCounts;

	m_parsePage = m_appState.page;
	m_synthesisPos = { m_appState.page, m_appState.sentenceIdx };
	if (!isPositionValid(m_synthesisPos))
		nextPosition(m_synthesisPos);

	emit statusMessage(false, QString("PDF Loaded : %2").arg(m_totalPages));

	if (m_playbackState != PlaybackState::PAUSED) {
		m_playbackState = PlaybackState::PLAYING;
		emit playbackStateChanged();
	}
	m_pdfLoaded = true;

	if (m_ttsManager->isInitialized())
		parsePage(0);
}

void AppController::onPdfLoadFailed(const QString &error)
{
	m_appState.set(m_appState.pdfPath, "app/pdfPath", "");
	m_pdfLoaded = false;
	m_totalPages = 0;
	emit totalPagesChanged();

	if (m_playbackState != PlaybackState::PAUSED) {
		m_playbackState = PlaybackState::PLAYING;
		emit playbackStateChanged();
	}

	emit errorOccurred(true, error);
}

void AppController::parsePage(uint16_t page)
{
	uint8_t genId = m_parseGenId.load(std::memory_order_relaxed);
	m_threadManager->submitTask(ThreadType::PDFParser, [this, page, genId]() {
		if (genId != m_parseGenId.load(std::memory_order_relaxed))
			return;

		m_pdfParser->extractPageContents(page, genId);
	});
	m_pageDataMap[page].state = LoadState::LOADING;
}

void AppController::parseTillPage(uint16_t page)
{
	for (int pg = page; pg >= m_parsePage; pg--) {
		if (m_pageDataMap[pg].state != LoadState::UNLOADED)
			continue;

		parsePage(pg);
	}

	m_parsePage = page + 1;
}

void AppController::onPageExtracted(int pageNumber, const QStringList &sentences,
									const QList<QImage> &images,
									const QList<PlaybackSegment> &segments, uint8_t genId)
{
	if (genId != m_parseGenId.load(std::memory_order_relaxed)) {
		m_isBusy = false;
		emit isBusyChanged();

		driveSynthesis();
		drivePlayback();
		return;
	}

	m_pageDataMap[pageNumber] = { .state = LoadState::LOADED,
								  .sentences = sentences,
								  .images = images,
								  .playbackSegments = segments };

	if (pageNumber == 0 && images.size() > 0) {
		m_coverImage = m_pageDataMap[pageNumber].images[0];
		m_currentImage = m_coverImage;
		emit imageIdChanged();
	}

	if (m_seekDirection != SeekDirection::NONE && pageNumber == m_appState.page) {
		m_seekDirection = SeekDirection::NONE;
		setPlaybackIdx();
	}

	m_isBusy = false;
	emit isBusyChanged();

	driveSynthesis();
	drivePlayback();
}

void AppController::onPageExtractionFailed(int pageNumber, const QString &error, uint8_t genId)
{
	if (genId != m_parseGenId.load(std::memory_order_relaxed)) {
		m_isBusy = false;
		emit isBusyChanged();

		driveSynthesis();
		drivePlayback();
		return;
	}

	m_pageDataMap[pageNumber] = { .state = LoadState::FAILED, .errorMessage = error };

	m_isBusy = false;
	emit isBusyChanged();

	if (m_seekDirection != SeekDirection::NONE && pageNumber == m_appState.page) {
		seekSentence(m_seekDirection);
		m_seekDirection = SeekDirection::NONE;
		return;
	}

	driveSynthesis();
	drivePlayback();
}

void AppController::driveSynthesis()
{
	if (!m_pdfLoaded || m_synthesisTaskInFlight || m_lookAheadCount >= m_maxLookAheadCount ||
		m_synthesisFinished) {
		return;
	}

	auto &pageData = m_pageDataMap[m_synthesisPos.pageNo];

	switch (pageData.state) {
	case LoadState::UNLOADED:
		parseTillPage(m_synthesisPos.pageNo);
		return;

	case LoadState::FAILED:
		if (!nextPosition(m_synthesisPos)) {
			m_synthesisFinished = true;
			return;
		}
		driveSynthesis();
		return;

	case LoadState::LOADING:
		return;

	default:
		break;
	}

	uint16_t page = m_synthesisPos.pageNo;
	uint16_t sentenceIdx = m_synthesisPos.sentenceIdx;
	QString sentence = m_pageDataMap[page].sentences[sentenceIdx];
	uint8_t genId = m_synthesisGenId.load(std::memory_order_relaxed);
	m_threadManager->submitTask(ThreadType::TTSManager, [this, sentence, page, sentenceIdx,
														 genId]() {
		if (genId != m_synthesisGenId.load(std::memory_order_relaxed))
			return;

		m_ttsManager->synthesizeText(sentence, page, sentenceIdx, m_appState.speakerId,
									 m_appState.ttsSpeed, genId);
	});
	m_synthesizing.enqueue(m_synthesisPos);
	m_synthesisTaskInFlight = true;

	if (!nextPosition(m_synthesisPos))
		m_synthesisFinished = true;
}

void AppController::onSynthesisComplete(uint16_t pageNumber, uint16_t sentenceIdx,
										const QByteArray &audioData, int sampleRate, uint8_t genId)
{
	if (genId != m_synthesisGenId.load(std::memory_order_relaxed)) {
		m_isBusy = false;
		emit isBusyChanged();

		driveSynthesis();
		drivePlayback();
		return;
	}

	m_synthesizing.dequeue();
	m_synthesisTaskInFlight = false;
	m_playbacks.enqueue(PlaybackData{ .state = LoadState::LOADED,
									  .pos = { pageNumber, sentenceIdx },
									  .audio = audioData,
									  .sampleRate = sampleRate });
	m_lookAheadCount++;

	m_isBusy = false;
	emit isBusyChanged();

	driveSynthesis();
	drivePlayback();
}

void AppController::onSynthesisFailed(uint16_t pageNumber, uint16_t sentenceIdx,
									  const QString &error, uint8_t genId)
{
	if (genId != m_synthesisGenId.load(std::memory_order_relaxed)) {
		m_isBusy = false;
		emit isBusyChanged();

		driveSynthesis();
		drivePlayback();
		return;
	}

	m_synthesizing.dequeue();
	m_synthesisTaskInFlight = false;
	m_playbacks.enqueue(PlaybackData{
		.state = LoadState::FAILED, .pos = { pageNumber, sentenceIdx }, .errorMessage = error });
	m_lookAheadCount++;

	m_isBusy = false;
	emit isBusyChanged();

	driveSynthesis();
	drivePlayback();
}

void AppController::onSpeechFinished(uint16_t pageNumber, uint16_t sentenceIdx)
{
	(void)pageNumber;
	(void)sentenceIdx;
	if (m_playbackState == PlaybackState::PLAYING) {
		m_sentenceTimer.start();
		emit infoClose();
	}
	driveSynthesis();
}

void AppController::updateCurrentPage(uint16_t page)
{
	m_imageTimer.stop();
	m_sentenceTimer.stop();
	if (page < m_totalPages) {
		m_appState.set(m_appState.page, "app/page", page);
		emit currentPageChanged();
	} else {
		m_appState.set(m_appState.page, "app/page", m_totalPages - 1);
		emit currentPageChanged();
		m_playbackState = PlaybackState::RESTART;
		emit playbackStateChanged();
	}
}

void AppController::evictHistory()
{
	while (m_historyCount > m_maxHistoryCount) {
		PlaybackData front = m_playbacks.dequeue();
		front.audio.clear();
		front.audio.squeeze();
		int page = front.pos.pageNo;
		if (front.pos.sentenceIdx == m_sentenceCounts[page] - 1 && page < m_appState.page) {
			while (page > 0 && m_pageDataMap[page].state != LoadState::UNLOADED)
				m_pageDataMap.remove(page--);
		}
		m_historyCount--;
	}
}

void AppController::drivePlayback()
{
	if (!m_pdfLoaded || m_playbackState != PlaybackState::PLAYING ||
		m_audioManager->isSpeechPlaying() || m_sentenceTimer.isActive() || m_imageTimer.isActive())
		return;

	auto &pageData = m_pageDataMap[m_appState.page];
	switch (pageData.state) {
	case LoadState::UNLOADED:
		m_isBusy = true;
		emit isBusyChanged();
		parseTillPage(m_appState.page);
		return;

	case LoadState::FAILED:
		emit statusMessage(false, QString("Failed to parse page: %1").arg(m_appState.page));
		updateCurrentPage(m_appState.page + 1);
		m_appState.set(m_appState.sentenceIdx, "app/sentenceIdx", 0);
		m_appState.set(m_appState.playbackIdx, "app/playbackIdx", 0);
		drivePlayback();
		return;

	case LoadState::LOADING:
		m_isBusy = true;
		emit isBusyChanged();
		return;

	default:
		break;
	}

	if (m_appState.playbackIdx >= pageData.playbackSegments.size()) {
		updateCurrentPage(m_appState.page + 1);
		m_appState.set(m_appState.sentenceIdx, "app/sentenceIdx", 0);
		m_appState.set(m_appState.playbackIdx, "app/playbackIdx", 0);
		drivePlayback();
		return;
	}

	auto &playbackSegment = pageData.playbackSegments[m_appState.playbackIdx];

	if (playbackSegment.hasSentences()) {
		if (m_lookAheadCount == 0) {
			m_isBusy = true;
			emit isBusyChanged();
			return;
		}

		PlaybackData &playback = m_playbacks[m_historyCount];
		if (playback.state == LoadState::FAILED) {
			m_historyCount++;
			m_lookAheadCount--;
			evictHistory();
			drivePlayback();
			return;
		} else if (playback.state == LoadState::LOADED) {
			if (m_playbackState != PlaybackState::PLAYING) {
				m_playbackState = PlaybackState::PLAYING;
				emit playbackStateChanged();
			}
			m_audioManager->playSpeech(playback.audio, playback.sampleRate, playback.pos.pageNo,
									   playback.pos.sentenceIdx);
			m_appState.set(m_appState.sentenceIdx, "app/sentenceIdx", playback.pos.sentenceIdx);
			emit infoMessage(pageData.sentences[playback.pos.sentenceIdx]);
			m_historyCount++;
			m_lookAheadCount--;
			evictHistory();
		}

		if (m_appState.sentenceIdx == playbackSegment.lastSentenceIdx) {
			m_appState.playbackIdx++;
			m_appState.set(m_appState.playbackIdx, "app/playbackIdx", m_appState.playbackIdx);
		}

		if (!(m_imageId == 0 || m_appState.sentenceIdx == playbackSegment.firstSentenceIdx))
			return;
	}

	if (playbackSegment.hasImage()) {
		m_currentImage = pageData.images[playbackSegment.imageIdx];
		m_imageId++;
		if (!playbackSegment.hasSentences()) {
			m_imageTimer.start();
			m_appState.playbackIdx++;
			m_appState.set(m_appState.playbackIdx, "app/playbackIdx", m_appState.playbackIdx);
		}
	} else if (m_coverImage) {
		m_currentImage = m_coverImage.value();
		m_imageId++;
	} else {
		m_currentImage.reset();
	}

	emit imageIdChanged();
}

void AppController::pause()
{
	if (m_playbackState == PlaybackState::PLAYING) {
		m_playbackState = PlaybackState::PAUSED;
		emit playbackStateChanged();
		m_audioManager->pause();
	}
}

void AppController::play()
{
	if (m_playbackState == PlaybackState::PAUSED) {
		m_playbackState = PlaybackState::PLAYING;
		emit playbackStateChanged();
		m_audioManager->resume();

		if (!m_audioManager->isSpeechPlaying())
			drivePlayback();
	}
}

void AppController::restart()
{
	if (!m_pdfLoaded)
		return;

	m_playbackState = PlaybackState::PLAYING;
	emit playbackStateChanged();
	resetState();
	m_parsePage = 0;
	updateCurrentPage(0);
	m_appState.set(m_appState.sentenceIdx, "app/sentenceIdx", 0);
	m_appState.set(m_appState.playbackIdx, "app/playbackIdx", 0);
	changeSynthesisPos({ 0, 0 });
}

void AppController::changeSynthesisPos(Position pos)
{
	m_playbacks.clear();
	m_lookAheadCount = 0;
	m_historyCount = 0;
	m_pageDataMap.clear();
	m_synthesisFinished = false;
	m_synthesisPos = pos;
	if (!isPositionValid(m_synthesisPos))
		nextPosition(m_synthesisPos);

	m_parseGenId++;
	cancelSynthesis();

	driveSynthesis();
}

void AppController::setPlaybackIdx()
{
	auto &segments = m_pageDataMap[m_appState.page].playbackSegments;
	m_appState.playbackIdx = segments.size();
	for (int i = 0; i < segments.size(); i++) {
		if (m_appState.sentenceIdx >= segments[i].firstSentenceIdx &&
			m_appState.sentenceIdx <= segments[i].lastSentenceIdx) {
			m_appState.set(m_appState.playbackIdx, "app/playbackIdx", i);
			break;
		}
	}
}

void AppController::seekSentence(SeekDirection dir)
{
	if (!m_pdfLoaded)
		return;

	m_audioManager->stopSpeech();
	emit infoClose();
	m_sentenceTimer.stop();
	m_imageTimer.stop();

	Position currentPos = { m_appState.page, m_appState.sentenceIdx };
	bool ok = (dir == SeekDirection::NEXT) ? nextPosition(currentPos) : prevPosition(currentPos);
	if (!ok && dir == SeekDirection::NEXT) {
		updateCurrentPage(m_totalPages);
		m_appState.set(m_appState.sentenceIdx, "app/sentenceIdx",
					   m_sentenceCounts[m_totalPages - 1] - 1);
		m_appState.set(m_appState.playbackIdx, "app/playbackIdx", 1000);
		return;
	}

	m_appState.set(m_appState.page, "app/page", currentPos.pageNo);
	m_appState.set(m_appState.sentenceIdx, "app/sentenceIdx", currentPos.sentenceIdx);

	if (m_pageDataMap[currentPos.pageNo].state != LoadState::LOADED) {
		m_seekDirection = dir;
		m_isBusy = true;
		emit isBusyChanged();
		m_parsePage = currentPos.pageNo;
		updateCurrentPage(currentPos.pageNo);
		m_appState.set(m_appState.sentenceIdx, "app/sentenceIdx", m_synthesisPos.sentenceIdx);
		changeSynthesisPos(currentPos);
		return;
	}

	setPlaybackIdx();

	auto it =
		std::find_if(m_playbacks.cbegin(), m_playbacks.cend(),
					 [&currentPos](const PlaybackData &item) { return item.pos == currentPos; });
	if (it != m_playbacks.cend()) {
		m_historyCount = std::distance(m_playbacks.cbegin(), it);
		m_lookAheadCount = m_playbacks.size() - m_historyCount;
	} else if (m_synthesisTaskInFlight && !m_synthesizing.isEmpty() &&
			   m_synthesizing.front() == currentPos) {
		m_playbacks.clear();
		m_historyCount = 0;
		m_lookAheadCount = 0;
	} else {
		m_parsePage = currentPos.pageNo;
		updateCurrentPage(currentPos.pageNo);
		m_appState.set(m_appState.sentenceIdx, "app/sentenceIdx", m_synthesisPos.sentenceIdx);
		changeSynthesisPos(currentPos);
	}

	drivePlayback();
}

void AppController::goToPage(uint16_t page)
{
	if (!m_pdfLoaded || page >= m_totalPages)
		return;

	m_audioManager->stopSpeech();
	emit infoClose();

	m_parsePage = page;
	updateCurrentPage(page);
	m_appState.set(m_appState.sentenceIdx, "app/sentenceIdx", 0);
	m_appState.set(m_appState.playbackIdx, "app/playbackIdx", 0);

	changeSynthesisPos({ page, 0 });
	drivePlayback();
}

void AppController::openMusic(const QString &uri)
{
	m_appState.set(m_appState.musicPath, "app/musicPath", uri);
	m_audioManager->playMusic(uri);
}

void AppController::onMusicFinished()
{
	m_audioManager->playMusic(m_appState.musicPath);
}

void AppController::toggleMusic()
{
	m_audioManager->toggleMusic();
	m_appState.set(m_appState.musicEnabled, "app/musicEnabled", m_audioManager->isMusicEnabled());
	emit isMusicEnabledChanged();
}
