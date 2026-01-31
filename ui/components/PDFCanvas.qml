import QtQuick
import QtQuick.Controls
import PDFNarrator

Item {
    id: root
    property bool controlsVisible: true

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
        running: false
        visible: running
        width: parent.width * Style.busyIndicatorFactor
        height: width
    }

    IconButton {
        id: playPause
        anchors.centerIn: parent
        iconSource: "qrc:/PDFNarrator/assets/images/pause.svg"
        visible: root.controlsVisible 
        width: parent.width * Style.busyIndicatorFactor
        height: width
        buttonRadius: 1.0
    }
}
