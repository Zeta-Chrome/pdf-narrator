#include "app_controller.h"
#include <QStandardPaths>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProcess>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#endif

#define SENTENCE_BREAK_PERIOD 250
#define IMAGE_DISPLAY_PERIOD 3000

static const QMap<QString, QString> g_modelSources = {
	{ "kokoro-en-v0_19",
	  "https://huggingface.com/ZetaChrome/pdf-narrator-models/resolve/main/kokoro-en-v0_19.tar.gz" },
	{ "kitten-micro-en-v0_8",
	  "https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/kitten-micro-en-v0_8.tar.bz2" },
	{ "kitten-nano-en-v0_2",
	  "https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/kitten-nano-en-v0_2-fp16.tar.bz2" }
};

static const QMap<QString, QStringList> g_modelVoices = {
	{ "kokoro-en-v0_19",
	  { "af", "af_bella", "af_nicole", "af_sarah", "af_sky", "am_adam", "am_michael", "bf_emma",
		"bf_isabella", "bm_george", "bm_lewis" } },
	{ "kitten-micro-en-v0_8",
	  { "Jasper", "Bella", "Bruno", "Luna", "Hugo", "Rosie", "Leo", "Kiki" } },
};

#if defined(Q_OS_ANDROID)
static const QStringList g_models = { "kokoro-en-v0_19", "kitten-micro-en-v0_8" };
#else
static const QStringList g_models = { "kokoro-en-v0_19", "kitten-micro-en-v0_8" };
#endif

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
	for (auto &modelName : g_models) {
		QDir modelPath = m_modelsDir.filePath(modelName);

		if (!modelPath.exists()) {
			m_needsDownload = true;
			return;
		}
	}
}

void AppController::download()
{
	QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	QDir modelsDir(baseDir + "/models");

	if (!modelsDir.exists()) {
		modelsDir.mkpath(".");
	}

	m_downloadQueue.clear();
	for (const QString &modelName : g_models) {
		if (!QDir(modelsDir.filePath(modelName)).exists()) {
			m_downloadQueue.append(modelName);
		}
	}

	downloadModel(new QNetworkAccessManager(this));
}

static QString tarExtractFlagFor(const QString &archiveName)
{
	static const QVector<QPair<QString, QString>> extToFlag = {
		{ ".tar.gz", "-xzf" }, { ".tgz", "-xzf" }, { ".tar.bz2", "-xjf" }, { ".tbz2", "-xjf" },
		{ ".tar.xz", "-xJf" }, { ".txz", "-xJf" }, { ".tar", "-xf" },
	};

	for (const auto &pair : extToFlag) {
		if (archiveName.endsWith(pair.first, Qt::CaseInsensitive))
			return pair.second;
	}
	return {};
}

void AppController::downloadModel(QNetworkAccessManager *networkManager)
{
	if (m_downloadQueue.isEmpty()) {
		delete networkManager;
		m_needsDownload = false;
		emit needsDownloadChanged();
		qDebug() << "All models ready. Initializing...";
		QTimer::singleShot(0, this, &AppController::initialize);
		return;
	}

	QString modelName = m_downloadQueue.first();

	emit statusMessage(false, QString("Downloading %1...").arg(modelName));

	QNetworkRequest request(g_modelSources[modelName]);
	QNetworkReply *reply = networkManager->get(request);

	connect(reply, &QNetworkReply::downloadProgress, this,
			[this, modelName](qint64 bytesReceived, qint64 bytesTotal) {
				if (bytesTotal > 0) {
					float percentage = ((float)bytesReceived / (float)bytesTotal) * 100.0f;
					emit downloadProgress(percentage);
				}
			});

	connect(reply, &QNetworkReply::finished, this, [this, reply, modelName, networkManager]() {
		reply->deleteLater();
		if (reply->error() != QNetworkReply::NoError) {
			qWarning() << "Download failed:" << reply->errorString();
			emit errorOccurred(false, QString("%1 download failed").arg(modelName));
			return;
		}

		QString modelPath = m_modelsDir.filePath(modelName);
		QDir().mkpath(modelPath);

		// Derive the real extension from the source URL, not a hardcoded guess
		QString sourceFileName = QUrl(g_modelSources[modelName]).fileName();
		QString extractFlag = tarExtractFlagFor(sourceFileName);
		if (extractFlag.isEmpty()) {
			qWarning() << "Unrecognized archive format for" << modelName << ":" << sourceFileName;
			emit errorOccurred(false,
							   QString("%1 has an unsupported archive format").arg(modelName));
			QDir(modelPath).removeRecursively();
			return;
		}
		QString ext = sourceFileName.mid(sourceFileName.indexOf('.'));
		QString archivePath = modelPath + ext;

		QFile file(archivePath);
		if (!file.open(QIODevice::WriteOnly)) {
			qWarning() << "Could not write archive for" << modelName;
			emit errorOccurred(false, QString("%1 could not be saved").arg(modelName));
			QDir(modelPath).removeRecursively();
			return;
		}
		file.write(reply->readAll());
		file.close();

		// Extract archive
		QProcess tar;
		tar.start("tar", QStringList() << extractFlag << archivePath << "-C" << modelPath
									   << "--strip-components=1");
		tar.waitForFinished(-1);
		QFile::remove(archivePath);

		if (tar.exitCode() != 0) {
			qWarning() << "Extraction failed: " << tar.exitCode();
			emit errorOccurred(false, QString("%1 extraction failed").arg(modelName));
			QDir(modelPath).removeRecursively();
			return;
		}

		qDebug() << "Extracted " << modelName << "to path: " << modelPath;
		m_downloadQueue.removeFirst();
		downloadModel(networkManager);
	});
}

void AppController::initializeTts()
{
	m_initializingTts = true;
	emit initializingTtsChanged();

	m_threadManager->submitTask(ThreadType::TTSManager, [this]() {
		m_ttsManager->initialize(m_modelsDir.filePath(g_models[m_appState.modelId]));
	});
}

void AppController::initialize()
{
	// Load initial user preferences
	m_appState.loadState();
	emit ttsVoicesChanged();
	emit ttsModelChanged();
	emit ttsSpeedChanged();
	emit ttsSpeakerChanged();
	if (!m_appState.musicEnabled)
		toggleMusic();

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
	emit statusMessage(false,
					   QString("%1 TTS model initialized").arg(g_models[m_appState.modelId]));

	if (m_pdfLoaded) {
		if (m_playbackState != PlaybackState::PAUSED) {
			m_playbackState = PlaybackState::PLAYING;
			emit playbackStateChanged();
		}

		parsePage(0);
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
	return g_models;
}

QStringList AppController::ttsVoices() const
{
	return g_modelVoices[g_models[m_appState.modelId]];
}

void AppController::setTtsModel(int modelId)
{
	if (m_appState.modelId == modelId)
		return;

	m_appState.set(m_appState.modelId, "tts/modelId", modelId);
	emit ttsModelChanged();
	emit ttsVoicesChanged();
	m_appState.loadState(); // Load the new speakerId
	emit ttsSpeakerChanged();

	if (m_pdfLoaded) {
		m_audioManager->stopSpeech();
		m_playbacks.clear();
		m_lookAheadCount = 0;
		m_historyCount = 0;
		m_synthesisFinished = false;
		m_synthesisPos = { m_appState.page, m_appState.sentenceIdx };
		if (m_threadManager->queuedTaskCount(ThreadType::TTSManager) > 0) {
			m_synthesisGenId++;
			m_synthesizing.clear();
		}
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
	m_playbacks.clear();
	m_lookAheadCount = 0;
	m_historyCount = 0;
	m_synthesisFinished = false;
	m_synthesisPos = { m_appState.page, m_appState.sentenceIdx };
	if (m_threadManager->queuedTaskCount(ThreadType::TTSManager) > 0) {
		m_synthesisGenId++;
		m_synthesizing.clear();
		m_playbackState = PlaybackState::BUSY;
		emit playbackStateChanged();
	} else {
		driveSynthesis();
	}
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
	m_playbacks.clear();
	m_lookAheadCount = 0;
	m_historyCount = 0;
	m_synthesisFinished = false;
	m_synthesisPos = { m_appState.page, m_appState.sentenceIdx };
	if (m_threadManager->queuedTaskCount(ThreadType::TTSManager) > 0) {
		m_synthesisGenId++;
		m_synthesizing.clear();
		m_playbackState = PlaybackState::BUSY;
		emit playbackStateChanged();
	} else {
		driveSynthesis();
	}
}

void AppController::resetState()
{
	m_audioManager->stopSpeech();
	m_sentenceTimer.stop();
	m_imageTimer.stop();
	m_pageDataMap.clear();
	m_playbacks.clear();
	m_coverImage.reset();
	m_currentImage.reset();
	m_lookAheadCount = 0;
	m_historyCount = 0;
	m_imageId = 0;
	m_synthesisFinished = false;
	m_seekDirection = SeekDirection::NONE;
	m_parseGenId++;
	m_synthesisGenId++;
	m_synthesizing.clear();
	m_playbackState = PlaybackState::BUSY;
	emit playbackStateChanged();
	emit imageIdChanged();
	m_playbackState = PlaybackState::PLAYING;
	emit playbackStateChanged();
}

void AppController::openPDF(const QString &uri)
{
	resetState();

	// Initialize page, sentenceIdx and playbackIdx only if a pdf was loaded before
	if (m_pdfLoaded) {
		m_appState.set(m_appState.page, "app/page", 0);
		m_appState.set(m_appState.sentenceIdx, "app/sentenceIdx", 0);
		m_appState.set(m_appState.playbackIdx, "app/playbackIdx", 0);
	}
	m_appState.set(m_appState.pdfPath, "app/pdfPath", uri);
	m_playbackState = PlaybackState::BUSY;
	emit playbackStateChanged();

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

	QString filename = m_appState.pdfPath.section("/", -1, -1);
	emit statusMessage(false, QString("%1 Loaded : %2").arg(filename).arg(m_totalPages));

	m_playbackState = PlaybackState::PLAYING;
	emit playbackStateChanged();
	m_pdfLoaded = true;

	if (m_ttsManager->isInitialized())
		parsePage(0); // Parse Page for cover image
}

void AppController::onPdfLoadFailed(const QString &error)
{
	m_appState.set(m_appState.pdfPath, "app/pdfPath", "");
	m_pdfLoaded = false;
	m_totalPages = 0;
	emit totalPagesChanged();
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
		if (m_playbackState == PlaybackState::BUSY)
			m_playbackState = PlaybackState::PLAYING;

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

	if (m_playbackState == PlaybackState::BUSY) {
		m_playbackState = PlaybackState::PLAYING;
		emit playbackStateChanged();
	}

	driveSynthesis();
	drivePlayback();
}

void AppController::onPageExtractionFailed(int pageNumber, const QString &error, uint8_t genId)
{
	if (genId != m_parseGenId.load(std::memory_order_relaxed)) {
		if (m_playbackState == PlaybackState::BUSY)
			m_playbackState = PlaybackState::PLAYING;

		driveSynthesis();
		drivePlayback();
		return;
	}

	m_pageDataMap[pageNumber] = { .state = LoadState::FAILED, .errorMessage = error };

	if (m_playbackState == PlaybackState::BUSY) {
		m_playbackState = PlaybackState::PLAYING;
		emit playbackStateChanged();
	}

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
	if (!m_pdfLoaded || m_threadManager->queuedTaskCount(ThreadType::TTSManager) > 0 ||
		m_lookAheadCount >= m_maxLookAheadCount || m_synthesisFinished) {
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

	if (!nextPosition(m_synthesisPos))
		m_synthesisFinished = true;
}

void AppController::onSynthesisComplete(uint16_t pageNumber, uint16_t sentenceIdx,
										const QByteArray &audioData, int sampleRate, uint8_t genId)
{
	if (genId != m_synthesisGenId.load(std::memory_order_relaxed)) {
		if (m_playbackState == PlaybackState::BUSY)
			m_playbackState = PlaybackState::PLAYING;

		driveSynthesis();
		drivePlayback();
		return;
	}

	m_synthesizing.dequeue();
	m_playbacks.enqueue(PlaybackData{ .state = LoadState::LOADED,
									  .pos = { pageNumber, sentenceIdx },
									  .audio = audioData,
									  .sampleRate = sampleRate });
	m_lookAheadCount++;

	if (m_playbackState == PlaybackState::BUSY) {
		m_playbackState = PlaybackState::PLAYING;
		emit playbackStateChanged();
	}

	driveSynthesis();
	drivePlayback();
}

void AppController::onSynthesisFailed(uint16_t pageNumber, uint16_t sentenceIdx,
									  const QString &error, uint8_t genId)
{
	if (genId != m_synthesisGenId.load(std::memory_order_relaxed)) {
		if (m_playbackState == PlaybackState::BUSY)
			m_playbackState = PlaybackState::PLAYING;

		driveSynthesis();
		drivePlayback();
		return;
	}

	m_synthesizing.dequeue();
	m_playbacks.enqueue(PlaybackData{
		.state = LoadState::FAILED, .pos = { pageNumber, sentenceIdx }, .errorMessage = error });
	m_lookAheadCount++;

	if (m_playbackState == PlaybackState::BUSY) {
		m_playbackState = PlaybackState::PLAYING;
		emit playbackStateChanged();
	}

	driveSynthesis();
	drivePlayback();
}

void AppController::onSpeechFinished(uint16_t pageNumber, uint16_t sentenceIdx)
{
	(void)pageNumber;
	(void)sentenceIdx;
	m_sentenceTimer.start();
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
		if (front.pos.sentenceIdx == m_sentenceCounts[page] - 1) {
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
		m_playbackState = PlaybackState::BUSY;
		emit playbackStateChanged();
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
		m_playbackState = PlaybackState::BUSY;
		emit playbackStateChanged();
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
			m_playbackState = PlaybackState::BUSY;
			emit playbackStateChanged();
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
			m_audioManager->playSpeech(playback.audio, playback.sampleRate, playback.pos.pageNo,
									   playback.pos.sentenceIdx);
			m_appState.set(m_appState.sentenceIdx, "app/sentenceIdx", playback.pos.sentenceIdx);
			m_historyCount++;
			m_lookAheadCount--;
			evictHistory();
		}

		if (m_appState.sentenceIdx == playbackSegment.lastSentenceIdx) {
			m_appState.playbackIdx++;
			m_appState.set(m_appState.playbackIdx, "app/playbackIdx", m_appState.playbackIdx);
			return;
		} else if (m_appState.sentenceIdx > playbackSegment.firstSentenceIdx &&
				   m_appState.sentenceIdx < playbackSegment.lastSentenceIdx && m_imageId > 0) {
			return;
		}
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
	// PDF is always loaded her
	if (!m_pdfLoaded)
		return;

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
	m_synthesisGenId++;
	m_synthesizing.clear();
	m_playbackState = PlaybackState::BUSY;
	emit playbackStateChanged();

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
	m_sentenceTimer.stop();
	m_imageTimer.stop();

	Position currentPos = { m_appState.page, m_appState.sentenceIdx };
	bool ok = (dir == SeekDirection::NEXT) ? nextPosition(currentPos) : prevPosition(currentPos);
	if (!ok && dir == SeekDirection::NEXT) {
		updateCurrentPage(m_totalPages);
		m_appState.set(m_appState.sentenceIdx, "app/sentenceIdx",
					   m_sentenceCounts[m_totalPages - 1] - 1);
		m_appState.set(m_appState.playbackIdx, "app/playbackIdx",
					   1000); // Large number to indicate End
		return;
	}

	m_appState.set(m_appState.page, "app/page", currentPos.pageNo);
	m_appState.set(m_appState.sentenceIdx, "app/sentenceIdx", currentPos.sentenceIdx);

	if (m_pageDataMap[currentPos.pageNo].state != LoadState::LOADED) {
		m_seekDirection = dir;
		m_playbackState = PlaybackState::BUSY;
		emit playbackStateChanged();
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
	} else if (m_threadManager->queuedTaskCount(ThreadType::TTSManager) > 0 &&
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
	emit isMusicEnabledChanged();
	m_appState.set(m_appState.musicEnabled, "app/musicEnabled", m_audioManager->isMusicEnabled());
}
