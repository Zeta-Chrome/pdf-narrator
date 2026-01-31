#include "style.h"
#include <QGuiApplication>

Style::Style(QObject *parent) : QObject(parent) {}

bool Style::isMobile() const
{
#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS)
    return true;
#else
    return false;
#endif
}

double Style::topBarHFactor() const
{
    return isMobile() ? 0.06 : 0.06;
}

double Style::bottomBarHFactor() const
{
    return isMobile() ? 0.06 : 0.06;
}

double Style::settingsHFactor() const
{
    return isMobile() ? 0.4 : 0.3;
}

double Style::settingsWFactor() const
{
    return isMobile() ? 0.75 : 0.5;
}

double Style::busyIndicatorFactor() const
{
    return isMobile() ? 0.12 : 0.08;
}

double Style::buttonRadiusFactor() const
{
    return isMobile() ? 0.1 : 0.5;
}

int Style::totalPagesTopPadding() const
{
    return isMobile() ? 8 : 0;
}
