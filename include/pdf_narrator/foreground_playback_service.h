#pragma once
#include <QObject>
#include <QString>

class ForegroundPlaybackService : public QObject {
	Q_OBJECT
public:
	explicit ForegroundPlaybackService(QObject *parent = nullptr);

	void start(const QString &title, const QString &subtitle);
	void updateNotification(const QString &title, const QString &subtitle);
	void stop();
};
