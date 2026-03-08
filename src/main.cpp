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

    app.setOrganizationName("PDFNarrator");
    app.setOrganizationDomain("pdfnarrator.app");
    app.setApplicationName("PDF Narrator");

    QQmlApplicationEngine engine;

    // Create controller
    AppController *controller = new AppController(&app);
    engine.rootContext()->setContextProperty("appController", controller);

    // Register image provider for PDF images
    engine.addImageProvider("pdfimages", new ImageProvider(controller));

    const QUrl url(QStringLiteral("qrc:/PDFNarrator/ui/Main.qml"));

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject *obj, const QUrl &objUrl)
        {
            if (!obj && url == objUrl)
                QCoreApplication::exit(-1);
        },
        Qt::QueuedConnection);

    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec(); 
}
