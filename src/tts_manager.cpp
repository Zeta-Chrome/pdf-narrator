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

	QString dirName = modelDir.dirName().toLower();

	// Identify Model Family
	bool isKokoro = dirName.contains("kokoro");
	bool isKitten = dirName.contains("kitten");
	bool isMatcha = dirName.contains("matcha");
	bool isMelo = dirName.contains("melo");
	bool isVits = dirName.contains("vits") || isMelo;

	// Locate .onnx files
	QStringList onnxFiles = modelDir.entryList({ "*.onnx" }, QDir::Files);
	if (onnxFiles.isEmpty()) {
		emit ttsInitializationFailed(QString("No .onnx file found in: %1").arg(modelPath));
		return;
	}

	QString modelFile;
	QString vocoderFile;

	if (isMatcha) {
		// Optional: Find separate vocoder if present
		auto vocoderIt = std::find_if(onnxFiles.begin(), onnxFiles.end(), [](const QString &f) {
			QString lower = f.toLower();
			return lower.contains("vocoder") || lower.contains("hifi");
		});

		if (vocoderIt != onnxFiles.end()) {
			vocoderFile = modelDir.filePath(*vocoderIt);
			onnxFiles.erase(vocoderIt);
		}

		if (onnxFiles.isEmpty()) {
			emit ttsInitializationFailed("Matcha acoustic model .onnx not found");
			return;
		}
	}

	// Prefer .onnx (e.g. "model.onnx" or "name.onnx" without .int8/.fp16)
	auto pureOnnxIt = std::find_if(onnxFiles.begin(), onnxFiles.end(), [](const QString &f) {
		QString lower = f.toLower();
		return !lower.contains(".int8.") && !lower.contains(".fp16.") && !lower.contains(".fp32.");
	});

	if (pureOnnxIt != onnxFiles.end()) {
		modelFile = modelDir.filePath(*pureOnnxIt);
	} else {
		// Fallback to the first available onnx file if only quantized/fp16 variants exist
		modelFile = modelDir.filePath(onnxFiles[0]);
	}

	// Locate Shared Assets
	QString tokensFile = modelDir.filePath("tokens.txt");
	QString voicesFile = modelDir.filePath("voices.bin");
	QString dataDir = modelDir.filePath("espeak-ng-data");
	QString dictDir = modelDir.filePath("dict");

	// Search for lexicons across various naming conventions
	QString lexiconFile;
	const QStringList lexiconCandidates = { "lexicon-us-en.txt", "lexicon.txt",
											"dict/lexicon.txt" };
	for (const QString &lex : lexiconCandidates) {
		if (QFile::exists(modelDir.filePath(lex))) {
			lexiconFile = modelDir.filePath(lex);
			break;
		}
	}
	if (lexiconFile.isEmpty()) {
		QStringList lexFiles = modelDir.entryList({ "*lexicon*.txt" }, QDir::Files);
		if (!lexFiles.isEmpty())
			lexiconFile = modelDir.filePath(lexFiles.first());
	}

	// espeak-ng-data is only mandatory for models using espeak (Kokoro, Kitten, Piper)
	bool requiresEspeak = isKokoro || isKitten || (isVits && !isMelo);
	if (requiresEspeak && !QDir(dataDir).exists()) {
		emit ttsInitializationFailed(QString("espeak-ng-data folder not found: %1").arg(dataDir));
		return;
	}

	QByteArray modelBa = modelFile.toUtf8();
	QByteArray tokensBa = tokensFile.toUtf8();
	QByteArray voicesBa = voicesFile.toUtf8();
	QByteArray dataBa = QDir(dataDir).exists() ? dataDir.toUtf8() : QByteArray();
	QByteArray dictBa = QDir(dictDir).exists() ? dictDir.toUtf8() : QByteArray();
	QByteArray lexiconBa = lexiconFile.toUtf8();
	QByteArray vocoderBa = vocoderFile.toUtf8();

	// Configure Sherpa-ONNX
	SherpaOnnxOfflineTtsConfig config{};
	config.model.debug = 0;
	config.model.num_threads = std::max(1, QThread::idealThreadCount() - 1);
	config.model.provider = "cpu";

	QString engineType;

	if (isKokoro) {
		engineType = "Kokoro";
		config.model.kokoro.model = modelBa.constData();
		config.model.kokoro.tokens = tokensBa.constData();
		config.model.kokoro.voices = voicesBa.constData();
		config.model.kokoro.data_dir = dataBa.constData();
		if (!lexiconBa.isEmpty())
			config.model.kokoro.lexicon = lexiconBa.constData();
	} else if (isKitten) {
		engineType = "Kitten";
		config.model.kitten.model = modelBa.constData();
		config.model.kitten.tokens = tokensBa.constData();
		config.model.kitten.voices = voicesBa.constData();
		config.model.kitten.data_dir = dataBa.constData();
	} else if (isMatcha) {
		engineType = "Matcha-TTS";
		config.model.matcha.acoustic_model = modelBa.constData();
		if (!vocoderBa.isEmpty())
			config.model.matcha.vocoder = vocoderBa.constData();
		config.model.matcha.tokens = tokensBa.constData();
		if (!dataBa.isEmpty())
			config.model.matcha.data_dir = dataBa.constData();
		if (!lexiconBa.isEmpty())
			config.model.matcha.lexicon = lexiconBa.constData();
	} else if (isVits) {
		engineType = isMelo ? "MeloTTS (VITS)" : "Piper (VITS)";
		config.model.vits.model = modelBa.constData();
		config.model.vits.tokens = tokensBa.constData();
		if (!dataBa.isEmpty())
			config.model.vits.data_dir = dataBa.constData();
		if (!dictBa.isEmpty())
			config.model.vits.dict_dir = dictBa.constData();
		if (!lexiconBa.isEmpty())
			config.model.vits.lexicon = lexiconBa.constData();
	} else {
		emit ttsInitializationFailed(QString("Unknown model type for: %1").arg(dirName));
		return;
	}

	m_tts = SherpaOnnxCreateOfflineTts(&config);
	if (!m_tts) {
		emit ttsInitializationFailed("Could not create TTS instance with provided configuration");
		return;
	}

	int sampleRate = SherpaOnnxOfflineTtsSampleRate(m_tts);
	if (sampleRate <= 0) {
		SherpaOnnxDestroyOfflineTts(m_tts);
		m_tts = nullptr;
		emit ttsInitializationFailed("Invalid sample rate returned by TTS runtime");
		return;
	}

	m_isInitialized = true;
	qInfo() << "TTS initialized (" << engineType << "). Model:" << modelFile
			<< "Sample rate:" << sampleRate;
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
		qInfo() << "Audio: " << audio_dur << "\tGen" << gen_dur << "\tRTF:" << gen_dur / audio_dur;
		qInfo() << "Text: " << text << "\n";

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
