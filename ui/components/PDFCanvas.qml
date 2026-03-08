import QtQuick
import QtQuick.Controls
import PDFNarrator

Item {
    id: root
    property bool controlsVisible: true
    property int minImageDisplayMs: 2000
    property bool imageTimerActive: false

    Image {
        id: pageImage
        anchors.fill: parent
        fillMode: Image.PreserveAspectFit
        smooth: true
        cache: false

        source: appController.imageId != "" ? 
                "image://pdfimages/" + appController.imageId : 
                "qrc:PDFNarrator/assets/images/icon.svg"
    }

    BusyIndicator {
        anchors.centerIn: parent
        running: appController.isBusy
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
        visible: root.controlsVisible && enabled
        enabled: !appController.isBusy
        width: parent.width * Style.playPauseFactor
        height: width
        buttonRadius: 1.0
        onClicked: {
            appController.isPlaying ? appController.pause() : appController.play()
        } 
    }
}
