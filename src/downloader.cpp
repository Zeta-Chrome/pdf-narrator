#include "downloader.h"
#include <QDir>

Downloader::Downloader(QObject *parent) : QObject(parent) {}

void Downloader::download(const QString &url, const QString &destPath)
{
    // Already exists — skip download
    if (QFile::exists(destPath))
    {
        emit finished(destPath);
        return;
    }

    // Ensure directory exists
    QDir().mkpath(QFileInfo(destPath).absolutePath());

    QNetworkRequest request(url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_nam.get(request);
    QFile *file = new QFile(destPath + ".tmp", this);

    if (!file->open(QIODevice::WriteOnly))
    {
        reply->abort();
        emit failed("Cannot open file for writing: " + destPath);
        return;
    }

    connect(reply, &QNetworkReply::downloadProgress, this, &Downloader::progressChanged);

    connect(reply, &QNetworkReply::readyRead, this, [reply, file]() { file->write(reply->readAll()); });

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, file, destPath]()
            {
                file->close();
                reply->deleteLater();
                file->deleteLater();

                if (reply->error() != QNetworkReply::NoError)
                {
                    QFile::remove(destPath + ".tmp");
                    emit failed(reply->errorString());
                    return;
                }

                // Rename tmp to final only on success
                QFile::remove(destPath);
                QFile::rename(destPath + ".tmp", destPath);
                emit finished(destPath);
            });
}
