#include "image_provider.h"
#include "app_controller.h"
#include <QDebug>

ImageProvider::ImageProvider(AppController *controller)
    : QQuickImageProvider(QQuickImageProvider::Image),
      m_controller(controller)
{
}

QImage ImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    if (!m_controller)
    {
        qWarning() << "ImageProvider: No controller set";
        return QImage();
    }

    // Get current image from controller
    QImage image = m_controller->currentImage();
    
    if (image.isNull())
    {
        qWarning() << "ImageProvider: No image available for id:" << id;
        return QImage();
    }

    // Set the original size
    if (size)
    {
        *size = image.size();
    }

    // Scale if requested
    if (requestedSize.isValid() && !requestedSize.isEmpty())
    {
        return image.scaled(requestedSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    return image;
}
