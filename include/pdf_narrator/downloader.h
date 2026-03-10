#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>

class Downloader : public QObject
{
    Q_OBJECT

public:
    explicit Downloader(QObject *parent = nullptr);
    void download(const QString &url, const QString &destPath);

signals:
    void progressChanged(qint64 bytesReceived, qint64 bytesTotal);
    void finished(const QString &destPath);
    void failed(const QString &error);

private:
    QNetworkAccessManager m_nam;
};
