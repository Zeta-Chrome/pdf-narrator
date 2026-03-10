#include "tts_manager.h"
#include <QDebug>
#include <QDir>
#include <QFile>

TTSManager::TTSManager(QObject *parent) : QObject(parent), m_tts(nullptr) {}

TTSManager::~TTSManager()
{
    shutdown();
}

void TTSManager::initialize(const QString &modelPath)
{
    shutdown();
    m_modelPath = modelPath;

    QDir modelDir(modelPath);
    if (!modelDir.exists())
    {
        emit ttsInitializationFailed(QString("Model directory does not exist: %1").arg(modelPath));
        return;
    }

    QString modelFile = modelDir.filePath("model.int8.onnx");
    if (!QFile::exists(modelFile))
        modelFile = modelDir.filePath("model.onnx");

    QString tokensFile = modelDir.filePath("tokens.txt");
    QString voicesFile = modelDir.filePath("voices.bin");
    QString dataDir    = modelDir.filePath("espeak-ng-data");
    QString lexicon    = modelDir.filePath("lexicon-us-en.txt");

    if (!QFile::exists(modelFile))
    {
        emit ttsInitializationFailed(QString("Model file not found: %1").arg(modelFile));
        return;
    }
    if (!QDir(dataDir).exists())
    {
        emit ttsInitializationFailed(QString("espeak-ng-data not found: %1").arg(dataDir));
        return;
    }

    QByteArray modelBa   = modelFile.toUtf8();
    QByteArray tokensBa  = tokensFile.toUtf8();
    QByteArray voicesBa  = voicesFile.toUtf8();
    QByteArray dataBa    = dataDir.toUtf8();
    QByteArray lexiconBa = QFile::exists(lexicon) ? lexicon.toUtf8() : QByteArray("");

    SherpaOnnxOfflineTtsConfig config{};
    config.model.debug           = 0;
    config.model.num_threads     = 2;
    config.model.provider        = "cpu";
    config.model.kokoro.model    = modelBa.constData();
    config.model.kokoro.tokens   = tokensBa.constData();
    config.model.kokoro.voices   = voicesBa.constData();
    config.model.kokoro.data_dir = dataBa.constData();
    config.model.kokoro.lexicon  = lexiconBa.constData();

    m_tts = SherpaOnnxCreateOfflineTts(&config);
    if (!m_tts)
    {
        emit ttsInitializationFailed("Could not create TTS instance");
        return;
    }

    int sampleRate = SherpaOnnxOfflineTtsSampleRate(m_tts);
    if (sampleRate == 0)
    {
        SherpaOnnxDestroyOfflineTts(m_tts);
        m_tts = nullptr;
        emit ttsInitializationFailed("Invalid sample rate returned by TTS");
        return;
    }

    m_isInitialized = true;
    qInfo() << "TTS initialized. Sample rate:" << sampleRate;
    emit ttsInitializationComplete();
}

void TTSManager::shutdown()
{
    if (m_tts)
    {
        SherpaOnnxDestroyOfflineTts(m_tts);
        m_tts = nullptr;
    }
    m_isInitialized = false;
}

void TTSManager::synthesizeText(const QString &text, int pageNumber, int sentenceId, int speakerId, float speed)
{
    if (!m_isInitialized || !m_tts)
    {
        emit synthesisFailed(pageNumber, sentenceId, "TTS not initialized");
        return;
    }

    if (text.trimmed().isEmpty())
    {
        emit synthesisFailed(pageNumber, sentenceId, "Empty text provided");
        return;
    }

    try
    {
        QByteArray textUtf8 = text.toUtf8();

        // Generate audio using C API
        // Speed: 1.0 = normal, <1.0 = slower, >1.0 = faster
        // sid: speaker ID (0 for single-speaker models like Kokoro)
        const SherpaOnnxGeneratedAudio *audio =
            SherpaOnnxOfflineTtsGenerate(m_tts, textUtf8.constData(), speakerId, speed);

        if (!audio || audio->n == 0)
        {
            emit synthesisFailed(pageNumber, sentenceId, "TTS generated no audio");
            if (audio)
            {
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

        emit synthesisComplete(pageNumber, sentenceId, audioData, sampleRate);
    }
    catch (const std::exception &e)
    {
        QString error = QString("TTS synthesis failed: %1").arg(e.what());
        qWarning() << error;
        emit synthesisFailed(pageNumber, sentenceId, error);
    }
}
