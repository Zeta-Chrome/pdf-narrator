pragma Singleton
import QtQuick

QtObject {
    readonly property bool isMobile: Qt.platform.os === "android" || Qt.platform.os === "ios"

    readonly property double topBarHFactor: isMobile ? 0.06 : 0.06
    readonly property double bottomBarHFactor: isMobile ? 0.06 : 0.06
    readonly property double settingsWFactor: isMobile ? 0.75 : 0.5
    readonly property double downloadFactor: isMobile ? 0.25 : 0.1
    readonly property double indicatorFactor: isMobile ? 0.12 : 0.08
    readonly property double buttonRadiusFactor: isMobile ? 0.1 : 0.5
    readonly property int totalPagesTopPadding: isMobile ? 8 : 0
}
