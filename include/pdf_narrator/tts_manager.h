#pragma once

#include <QObject>
#include <QString>
#include <QByteArray>
#include <mupdf/fitz/separation.h>

extern "C" {
#include "sherpa-onnx/c-api/c-api.h"
}

class TTSManager : public QObject {
	Q_OBJECT

public:
	explicit TTSManager(QObject *parent = nullptr);
	~TTSManager();

	bool isInitialized() const
	{
		return m_isInitialized;
	}
	void initialize(const QString &modelPath);
	void shutdown();
	void synthesizeText(const QString &text, int pageNumber, int sentenceId, int speakerId,
						float speed, uint8_t genId);
signals:
	void ttsInitializationComplete();
	void ttsInitializationFailed(const QString &error);
	void synthesisComplete(int pageNumber, int sentenceId, const QByteArray &audioData,
						   int sampleRate, uint8_t genId);
	void synthesisFailed(int pageNumber, int sentenceId, const QString &error, uint8_t genId);

private:
	const SherpaOnnxOfflineTts *m_tts;
	bool m_isInitialized = false;
	QString m_modelPath = "";
};
