#include "tts_manager.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <iostream>

TTSManager::TTSManager(QObject *parent) : QObject(parent), m_tts(nullptr), m_isInitialized(false) {}

TTSManager::~TTSManager()
{
    shutdown();
}

bool TTSManager::initialize(const QString &modelPath)
{
    shutdown();

    m_modelPath = modelPath;

    QDir modelDir(modelPath);
    if (!modelDir.exists())
    {
        QString error = QString("Model directory does not exist: %1").arg(modelPath);
        qWarning() << error;
        return false;
    }

    QString modelFile = modelDir.filePath("model.onnx");
    QString tokensFile = modelDir.filePath("tokens.txt");
    QString voicesFile = modelDir.filePath("voices.bin");
    QString dataDir = modelDir.filePath("espeak-ng-data");

    if (!QFile::exists(modelFile))
    {
        QString error = QString("Model file not found: %1").arg(modelFile);
        qWarning() << error;
        return false;
    }

    if (!QDir(dataDir).exists())
    {
        QString error = QString("espeak-ng-data directory not found: %1").arg(dataDir);
        qWarning() << error;
        return false;
    }
    
    QByteArray modelFileBa = modelFile.toUtf8();
    QByteArray dataDirBa = dataDir.toUtf8();
    QByteArray tokensFileBa = tokensFile.toUtf8();
    QByteArray voicesFileBa = voicesFile.toUtf8();

    // Initialize config for Kokoro model
    SherpaOnnxOfflineTtsConfig config{0};
    config.model.debug = 0;
    config.model.num_threads = 2;
    config.model.provider = "cpu";
    config.model.kokoro.model = modelFileBa.constData();
    config.model.kokoro.data_dir = dataDirBa.constData();
    config.model.kokoro.tokens = tokensFileBa.constData();
    config.model.kokoro.voices = voicesFileBa.constData();

    try
    {
        m_tts = SherpaOnnxCreateOfflineTts(&config);

        if (!m_tts)
        {
            QString error = "TTS initialization failed: Could not create TTS instance";
            qWarning() << error;
            return false;
        }

        int sampleRate = SherpaOnnxOfflineTtsSampleRate(m_tts);
        if (sampleRate == 0)
        {
            SherpaOnnxDestroyOfflineTts(m_tts);
            m_tts = nullptr;
            QString error = "TTS initialization failed: Invalid sample rate";
            qWarning() << error;
            return false;
        }

        m_isInitialized = true;
        qInfo() << "TTS initialized successfully. Sample rate:" << sampleRate;
        return true;
    }
    catch (const std::exception &e)
    {
        if (m_tts)
        {
            SherpaOnnxDestroyOfflineTts(m_tts);
            m_tts = nullptr;
        }
        QString error = QString("TTS initialization failed: %1").arg(e.what());
        qWarning() << error;
        return false;
    }
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

void TTSManager::synthesizeText(const QString &text, int pageNumber, int sentenceId, int speed)
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
            SherpaOnnxOfflineTtsGenerate(m_tts, textUtf8.constData(), 0, speed);

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

        int sampleRate = static_cast<float>(audio->sample_rate);

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
