#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>

extern "C"
{
#include "sherpa-onnx/c-api/c-api.h"
}

class TTSManager : public QObject
{
    Q_OBJECT

public:
    explicit TTSManager(QObject *parent = nullptr);
    ~TTSManager();

    void initialize(const QString &modelPath);
    void shutdown();
    void synthesizeText(const QString &text, int pageNumber, int sentenceId, int speakerId, float speed);
    QStringList getVoices() const 
    {
        return m_voices;
    }

signals:
    void ttsInitializationComplete();
    void ttsInitializationFailed(QString error);
    void synthesisComplete(int pageNumber, int sentenceIdx, const QByteArray& audioData, int sampleRate);
    void synthesisFailed(int pageNumber, int sentenceIdx, const QString &error);

private:
    const SherpaOnnxOfflineTts *m_tts;
    bool m_isInitialized = false;
    QString m_modelPath = "";
    QStringList m_voices;
};
