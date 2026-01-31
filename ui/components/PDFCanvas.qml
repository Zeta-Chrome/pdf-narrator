import QtQuick
import QtQuick.Controls
import PDFNarrator

Item {
    id: root

    Image {
        id: pageImage
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        smooth: true
        cache: false

        source: "qrc:PDFNarrator/assets/images/icon.svg"
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: true
        width: parent.width * Style.busyIndicatorFactor
        height: width
    }
}
