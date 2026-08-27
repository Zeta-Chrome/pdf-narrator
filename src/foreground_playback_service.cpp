#include "foreground_playback_service.h"

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#include <QtCore/qnativeinterface.h>

namespace
{
constexpr auto kServiceClass = "org/qtproject/example/pdfnarrator/TtsForegroundService";
}

ForegroundPlaybackService::ForegroundPlaybackService(QObject *parent)
	: QObject(parent)
{
}

void ForegroundPlaybackService::start(const QString &title, const QString &subtitle)
{
	QJniObject jTitle = QJniObject::fromString(title);
	QJniObject jSubtitle = QJniObject::fromString(subtitle);
	QJniObject::callStaticMethod<void>(
		kServiceClass, "start", "(Landroid/content/Context;Ljava/lang/String;Ljava/lang/String;)V",
		QNativeInterface::QAndroidApplication::context(), jTitle.object<jstring>(),
		jSubtitle.object<jstring>());
}

void ForegroundPlaybackService::updateNotification(const QString &title, const QString &subtitle)
{
	start(title, subtitle); // re-invoking start() just refreshes the existing notification
}

void ForegroundPlaybackService::stop()
{
	QJniObject::callStaticMethod<void>(kServiceClass, "stop", "(Landroid/content/Context;)V",
									   QNativeInterface::QAndroidApplication::context());
}

#else // desktop/dev builds: no-op stub

ForegroundPlaybackService::ForegroundPlaybackService(QObject *parent)
	: QObject(parent)
{
}
void ForegroundPlaybackService::start(const QString &, const QString &)
{
}
void ForegroundPlaybackService::updateNotification(const QString &, const QString &)
{
}
void ForegroundPlaybackService::stop()
{
}

#endif
