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

    bool initialize(const QString &modelPath);
    void shutdown();

    void synthesizeText(const QString &text, int pageNumber, int sentenceId, int speed);
signals:
    void synthesisComplete(int pageNumber, int sentenceIdx, const QByteArray& audioData, int sampleRate);
    void synthesisFailed(int pageNumber, int sentenceIdx, const QString &error);

private:
    const SherpaOnnxOfflineTts *m_tts;
    bool m_isInitialized;
    QString m_modelPath;
};
