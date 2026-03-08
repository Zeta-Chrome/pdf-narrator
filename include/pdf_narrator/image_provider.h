#pragma once

#include <QQuickImageProvider>
#include <QImage>

class AppController;

/**
 * Image provider for displaying PDF page images in QML
 * 
 * Usage in QML:
 *   Image {
 *       source: "image://pdfimages/" + appController.currentImageId
 *   }
 */
class ImageProvider : public QQuickImageProvider
{
public:
    explicit ImageProvider(AppController *controller);
    
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;

private:
    AppController *m_controller;
};
