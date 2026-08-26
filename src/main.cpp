#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>

#include "app_controller.h"
#include "image_provider.h"

int main(int argc, char *argv[])
{
#ifdef Q_OS_LINUX
	// Force FFmpeg to use PulseAudio backend on Linux
	qputenv("QT_AUDIO_BACKEND", "pulseaudio");
#endif

	QGuiApplication app(argc, argv);

	app.setOrganizationName("cosmic");
	app.setOrganizationDomain("cosmic.local");
	app.setApplicationName("PDFNarrator");

	QQmlApplicationEngine engine;

	// Create controller
	AppController *controller = new AppController(&app);
	engine.rootContext()->setContextProperty("appController", controller);

	// Register image provider for PDF images
	engine.addImageProvider("pdfimages", new ImageProvider(controller));

	QObject::connect(
		&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
		[]() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

	engine.loadFromModule("PDFNarrator", "Main");

	return app.exec();
}
