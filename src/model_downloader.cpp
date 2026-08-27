#include "model_downloader.h"
#include <QStandardPaths>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QUrl>
#include <QFile>
#include <QDebug>
#include <QtConcurrent/QtConcurrentRun>
#include <QFutureWatcher>

static const QMap<QString, QString> g_modelSources = {
	{ "kokoro-en-v0_19",
	  "https://huggingface.com/ZetaChrome/pdf-narrator-models/resolve/main/kokoro-en-v0_19.tar.gz" },
	{ "kitten-micro-en-v0_8",
	  "https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/kitten-micro-en-v0_8.tar.bz2" },
	{ "vits-piper-libritts-high",
	  "https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/vits-piper-en_US-libritts-high.tar.bz2" },
	{ "vits-piper-amy-medium",
	  "https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/vits-piper-en_US-amy-medium.tar.bz2" },
};

static const QMap<QString, QStringList> g_modelVoices = {
	{ "kokoro-en-v0_19",
	  { "af", "af_bella", "af_nicole", "af_sarah", "af_sky", "am_adam", "am_michael", "bf_emma",
		"bf_isabella", "bm_george", "bm_lewis" } },
	{ "kitten-micro-en-v0_8",
	  { "Jasper", "Bella", "Bruno", "Luna", "Hugo", "Rosie", "Leo", "Kiki" } },
	{ "vits-piper-libritts-high",
	  { "Speaker 0", "Speaker 1", "Speaker 2", "Speaker 3", "Speaker 4", "Speaker 5", "Speaker 6",
		"Speaker 7" } },
	{ "vits-piper-amy-medium", { "Amy (Default)" } },
};

#if defined(Q_OS_ANDROID)
static const QStringList g_models = { "kokoro-en-v0_19", "kitten-micro-en-v0_8",
									  "vits-piper-libritts-high", "vits-piper-amy-medium" };
#else
static const QStringList g_models = { "kokoro-en-v0_19", "kitten-micro-en-v0_8",
									  "vits-piper-libritts-high", "vits-piper-amy-medium" };
#endif

static QString tarExtractFlagFor(const QString &archiveName)
{
	static const QVector<QPair<QString, QString>> extToFlag = {
		{ ".tar.gz", "-xzf" }, { ".tgz", "-xzf" }, { ".tar.bz2", "-xjf" }, { ".tbz2", "-xjf" },
		{ ".tar.xz", "-xJf" }, { ".txz", "-xJf" }, { ".tar", "-xf" },
	};

	for (const auto &pair : extToFlag) {
		if (archiveName.endsWith(pair.first, Qt::CaseInsensitive))
			return pair.second;
	}
	return {};
}

static ExtractResult writeAndExtractArchive(const QByteArray &data, const QString &modelName,
											const QString &modelPath, const QString &sourceUrl)
{
	QDir().mkpath(modelPath);

	QString sourceFileName = QUrl(sourceUrl).fileName();
	QString extractFlag = tarExtractFlagFor(sourceFileName);
	if (extractFlag.isEmpty()) {
		qWarning() << "Unrecognized archive format for" << modelName << ":" << sourceFileName;
		QDir(modelPath).removeRecursively();
		return { false, QString("%1 has an unsupported archive format").arg(modelName) };
	}

	QString ext = sourceFileName.mid(sourceFileName.indexOf('.'));
	QString archivePath = modelPath + ext;

	QFile file(archivePath);
	if (!file.open(QIODevice::WriteOnly)) {
		qWarning() << "Could not write archive for" << modelName;
		QDir(modelPath).removeRecursively();
		return { false, QString("%1 could not be saved").arg(modelName) };
	}
	file.write(data);
	file.close();

	QProcess tar;
	tar.start("tar", QStringList() << extractFlag << archivePath << "-C" << modelPath
								   << "--strip-components=1");
	tar.waitForFinished(-1);
	QFile::remove(archivePath);

	if (tar.exitCode() != 0) {
		qWarning() << "Extraction failed: " << tar.exitCode();
		QDir(modelPath).removeRecursively();
		return { false, QString("%1 extraction failed").arg(modelName) };
	}

	qDebug() << "Extracted" << modelName << "to path:" << modelPath;
	return { true, {} };
}

ModelDownloader::ModelDownloader(const QString &modelsDirPath, QObject *parent)
	: QObject(parent)
	, m_modelsDir(modelsDirPath)
{
}

const QStringList &ModelDownloader::availableModels()
{
	return g_models;
}

const QMap<QString, QStringList> &ModelDownloader::modelVoices()
{
	return g_modelVoices;
}

bool ModelDownloader::isModelDownloaded(int modelId) const
{
	return QDir(m_modelsDir.filePath(g_models[modelId])).exists();
}

void ModelDownloader::downloadModel(int modelId)
{
	if (modelId < 0 || modelId >= g_models.size())
		return;

	if (isModelDownloaded(modelId)) {
		emit downloadFinished(modelId, true);
		return;
	}

	const QString &modelName = g_models[modelId];
	if (m_downloadQueue.contains(modelName))
		return; // Already in queue

	m_downloadQueue.append(modelName);

	// If not currently processing a download, start
	if (!m_isProcessing)
		processNextInQueue();
}

void ModelDownloader::processNextInQueue()
{
	if (m_downloadQueue.isEmpty()) {
		m_isProcessing = false;
		return;
	}

	m_isProcessing = true;
	QString modelName = m_downloadQueue.first();
	int modelId = (int)g_models.indexOf(modelName);

	emit statusMessage(false, QString("Downloading %1...").arg(modelName));

	QNetworkRequest request(g_modelSources[modelName]);
	QNetworkReply *reply = m_networkManager.get(request);

	connect(reply, &QNetworkReply::downloadProgress, this,
			[this, modelId](qint64 bytesReceived, qint64 bytesTotal) {
				if (bytesTotal > 0) {
					float percentage = ((float)bytesReceived / (float)bytesTotal) * 100.0f;
					emit downloadProgress(modelId, percentage);
				}
			});

	connect(reply, &QNetworkReply::finished, this, [this, reply, modelName, modelId]() {
		reply->deleteLater();

		if (reply->error() != QNetworkReply::NoError) {
			qWarning() << "Download failed:" << reply->errorString();
			emit errorOccurred(false, QString("%1 download failed").arg(modelName));
			m_downloadQueue.removeFirst();
			emit downloadFinished(modelId, false);
			processNextInQueue();
			return;
		}

		QByteArray data = reply->readAll();
		QString modelPath = m_modelsDir.filePath(modelName);
		QString sourceUrl = g_modelSources[modelName];

		emit statusMessage(false, QString("Extracting %1...").arg(modelName));

		auto *watcher = new QFutureWatcher<ExtractResult>(this);
		connect(watcher, &QFutureWatcher<ExtractResult>::finished, this,
				[this, watcher, modelId]() {
					ExtractResult result = watcher->result();
					watcher->deleteLater();

					if (!result.success)
						emit errorOccurred(false, result.errorMessage);

					m_downloadQueue.removeFirst();
					emit downloadFinished(modelId, result.success);

					// Process the next queued model
					processNextInQueue();
				});

		watcher->setFuture(
			QtConcurrent::run(writeAndExtractArchive, data, modelName, modelPath, sourceUrl));
	});
}
