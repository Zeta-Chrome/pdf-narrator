#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QDir>
#include <QNetworkAccessManager>

struct ExtractResult {
	bool success{ false };
	QString errorMessage{};
};

class ModelDownloader : public QObject {
	Q_OBJECT

public:
	explicit ModelDownloader(const QString &modelsDirPath, QObject *parent = nullptr);
	~ModelDownloader() override = default;

	bool isModelDownloaded(int modelId) const;
	bool isDownloading() const
	{
		return !m_downloadQueue.isEmpty();
	}
	void downloadModel(int modelId);

	static const QStringList &availableModels();
	static const QMap<QString, QStringList> &modelVoices();

signals:
	void downloadProgress(int modelId, float percentage);
	void downloadFinished(int modelId, bool success);
	void statusMessage(bool persistent, const QString &message);
	void errorOccurred(bool persistent, const QString &error);

private:
	void processNextInQueue();

	QDir m_modelsDir;
	QStringList m_downloadQueue;
	QNetworkAccessManager m_networkManager;
	bool m_isProcessing{ false };
};
