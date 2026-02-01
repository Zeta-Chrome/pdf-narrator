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
        running: !appController.isPdfLoaded 
        visible: running
        width: parent.width * Style.busyIndicatorFactor
        height: width
    }

    IconButton {
        id: playPause
        anchors.centerIn: parent
        iconSource: appController.isPlaying ? 
                    "qrc:/PDFNarrator/assets/images/pause.svg" : 
                    "qrc:/PDFNarrator/assets/images/play.svg" 
        enabled: appController.isPdfLoaded
        visible: root.controlsVisible && enabled 
        width: parent.width * Style.playPauseFactor
        height: width
        buttonRadius: 1.0
    }
}
