#pragma once

#include "cancellation_token.h"
#include <QObject>
#include <QString>
#include <QByteArray>
#include <memory>

extern "C" {
#include "sherpa-onnx/c-api/c-api.h"
}

class TTSManager : public QObject {
	Q_OBJECT

public:
	explicit TTSManager(std::shared_ptr<GenerationID> genId, QObject *parent = nullptr);
	~TTSManager();

	bool isInitialized() const
	{
		return m_isInitialized;
	}
	void initialize(const QString &modelPath);
	void shutdown();
	void synthesizeText(const QString &text, int pageNumber, int sentenceId, int speakerId,
						float speed, uint32_t genId);
signals:
	void ttsInitializationComplete();
	void ttsInitializationFailed(const QString &error);
	void synthesisComplete(int pageNumber, int sentenceId, const QByteArray &audioData,
						   int sampleRate, uint32_t genId);
	void synthesisFailed(int pageNumber, int sentenceId, const QString &error, uint32_t genId);
	void synthesisCancelled(int pageNumber, int sentenceId);

private:
	static int32_t progressCallback(const float *samples, int32_t num_samples, float progress,
									void *arg);

	const SherpaOnnxOfflineTts *m_tts;
	bool m_isInitialized = false;
	QString m_modelPath = "";
	std::shared_ptr<GenerationID> m_genId;
};
