#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>

extern "C" {
    #include "sherpa-onnx/c-api/c-api.h"
}

class TTSManager : public QObject {
    Q_OBJECT

public:
    explicit TTSManager(QObject *parent = nullptr);
    ~TTSManager();

    bool initialize(const QString &modelPath);
    void shutdown();
    
    void synthesizeText(const QString &text, int sentenceId);
    
    bool isInitialized() const { return m_tts != nullptr; }
    float getSampleRate() const;

signals:
    void synthesisComplete(int sentenceId, const QByteArray &audioData, float sampleRate);
    void synthesisFailed(const QString &error);
    void initializationComplete(bool success);

private:
    const SherpaOnnxOfflineTts *m_tts;
    bool m_isInitialized;
    QString m_modelPath;
};
