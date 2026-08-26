#include "tts_manager.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QElapsedTimer>
#include <QThread>

TTSManager::TTSManager(QObject *parent)
	: QObject(parent)
	, m_tts(nullptr)
{
}

TTSManager::~TTSManager()
{
	shutdown();
}

void TTSManager::initialize(const QString &modelPath)
{
	shutdown();
	m_modelPath = modelPath;

	QDir modelDir(modelPath);
	if (!modelDir.exists()) {
		emit ttsInitializationFailed(QString("Model directory does not exist: %1").arg(modelPath));
		return;
	}

	const QStringList candidates = { "model.int8.onnx", "model.fp16.onnx", "model.onnx" };

	auto it = std::find_if(candidates.begin(), candidates.end(), [&modelDir](const QString &name) {
		return QFile::exists(modelDir.filePath(name));
	});

	QString modelFile = (it != candidates.end()) ? modelDir.filePath(*it) : QString();

	if (modelFile.isEmpty())
		qWarning() << "No suitable model file found in" << modelDir.path();

	QString tokensFile = modelDir.filePath("tokens.txt");
	QString voicesFile = modelDir.filePath("voices.bin");
	QString dataDir = modelDir.filePath("espeak-ng-data");
	QString lexicon = modelDir.filePath("lexicon-us-en.txt");

	if (!QFile::exists(modelFile)) {
		emit ttsInitializationFailed(QString("Model file not found: %1").arg(modelFile));
		return;
	}
	if (!QDir(dataDir).exists()) {
		emit ttsInitializationFailed(QString("espeak-ng-data not found: %1").arg(dataDir));
		return;
	}

	// Pick the model family by folder name. Kitten and Kokoro share the same
	const bool isKitten = modelDir.dirName().contains("kitten", Qt::CaseInsensitive);

	QByteArray modelBa = modelFile.toUtf8();
	QByteArray tokensBa = tokensFile.toUtf8();
	QByteArray voicesBa = voicesFile.toUtf8();
	QByteArray dataBa = dataDir.toUtf8();
	QByteArray lexiconBa = QFile::exists(lexicon) ? lexicon.toUtf8() : QByteArray("");

	SherpaOnnxOfflineTtsConfig config{};
	config.model.debug = 0;
	config.model.num_threads = QThread::idealThreadCount() - 1;

#if defined(Q_OS_ANDROID)
	config.model.provider = "nnapi";
#elif defined(Q_OS_WINDOWS)
	config.model.provider = "directml";
#else
	config.model.provider = "cpu";
#endif

	if (isKitten) {
		config.model.kitten.model = modelBa.constData();
		config.model.kitten.tokens = tokensBa.constData();
		config.model.kitten.voices = voicesBa.constData();
		config.model.kitten.data_dir = dataBa.constData();
	} else {
		config.model.kokoro.model = modelBa.constData();
		config.model.kokoro.tokens = tokensBa.constData();
		config.model.kokoro.voices = voicesBa.constData();
		config.model.kokoro.data_dir = dataBa.constData();
		config.model.kokoro.lexicon = lexiconBa.constData();
	}

	m_tts = SherpaOnnxCreateOfflineTts(&config);
	if (!m_tts) {
		emit ttsInitializationFailed("Could not create TTS instance");
		return;
	}

	int sampleRate = SherpaOnnxOfflineTtsSampleRate(m_tts);
	if (sampleRate == 0) {
		SherpaOnnxDestroyOfflineTts(m_tts);
		m_tts = nullptr;
		emit ttsInitializationFailed("Invalid sample rate returned by TTS");
		return;
	}

	m_isInitialized = true;
	qInfo() << "TTS initialized (" << (isKitten ? "kitten" : "kokoro")
			<< "). Sample rate:" << sampleRate;
	emit ttsInitializationComplete();
}

void TTSManager::shutdown()
{
	if (m_tts) {
		SherpaOnnxDestroyOfflineTts(m_tts);
		m_tts = nullptr;
	}
	m_isInitialized = false;
}

void TTSManager::synthesizeText(const QString &text, int pageNumber, int sentenceId, int speakerId,
								float speed, uint8_t genId)
{
	if (!m_isInitialized || !m_tts) {
		emit synthesisFailed(pageNumber, sentenceId, "TTS not initialized", genId);
		return;
	}

	if (text.trimmed().isEmpty()) {
		emit synthesisFailed(pageNumber, sentenceId, "Empty text provided", genId);
		return;
	}

	try {
		QByteArray textUtf8 = text.toUtf8();

		SherpaOnnxGenerationConfig genConfig{};
		genConfig.sid = speakerId;
		genConfig.speed = speed;

		QElapsedTimer t;
		t.start();

		const SherpaOnnxGeneratedAudio *audio = SherpaOnnxOfflineTtsGenerateWithConfig(
			m_tts, textUtf8.constData(), &genConfig, nullptr, nullptr);

		float gen_dur = (float)t.elapsed() / 1000;
		float audio_dur = (float)audio->n / (float)audio->sample_rate;
		qDebug() << "Audio: " << audio_dur << "\tGen" << gen_dur << "\tRTF:" << gen_dur / audio_dur;
		qDebug() << "Text: " << text << "\n";

		if (!audio || audio->n == 0) {
			emit synthesisFailed(pageNumber, sentenceId, "TTS generated no audio", genId);
			if (audio) {
				SherpaOnnxDestroyOfflineTtsGeneratedAudio(audio);
			}
			return;
		}

		// Copy audio samples to QByteArray
		QByteArray audioData;
		audioData.resize(audio->n * sizeof(float));
		memcpy(audioData.data(), audio->samples, audioData.size());

		int sampleRate = audio->sample_rate;

		// Clean up
		SherpaOnnxDestroyOfflineTtsGeneratedAudio(audio);

		emit synthesisComplete(pageNumber, sentenceId, audioData, sampleRate, genId);
	} catch (const std::exception &e) {
		QString error = QString("TTS synthesis failed: %1").arg(e.what());
		qWarning() << error;
		emit synthesisFailed(pageNumber, sentenceId, error, genId);
	}
}
