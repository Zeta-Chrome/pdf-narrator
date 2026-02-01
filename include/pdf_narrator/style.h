#pragma once

#include <QObject>
#include <QtQml/qqmlregistration.h>

class Style : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
    Q_PROPERTY(bool isMobile READ isMobile CONSTANT)
    Q_PROPERTY(double topBarHFactor READ topBarHFactor CONSTANT)
    Q_PROPERTY(double bottomBarHFactor READ bottomBarHFactor CONSTANT)
    Q_PROPERTY(double settingsHFactor READ settingsHFactor CONSTANT)
    Q_PROPERTY(double settingsWFactor READ settingsWFactor CONSTANT)
    Q_PROPERTY(double busyIndicatorFactor READ busyIndicatorFactor CONSTANT)
    Q_PROPERTY(double playPauseFactor READ playPauseFactor CONSTANT)
    Q_PROPERTY(double buttonRadiusFactor READ buttonRadiusFactor CONSTANT)
    Q_PROPERTY(int totalPagesTopPadding READ totalPagesTopPadding CONSTANT)

public:
    Style(QObject *parent = nullptr);

    bool isMobile() const;
    double topBarHFactor() const;
    double bottomBarHFactor() const;
    double settingsHFactor() const;
    double settingsWFactor() const;
    double busyIndicatorFactor() const;
    double playPauseFactor() const;
    double buttonRadiusFactor() const;
    int totalPagesTopPadding() const;
};
