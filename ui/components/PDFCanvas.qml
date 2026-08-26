import QtQuick
import QtQuick.Controls
import PDFNarrator

Item {
    id: root
    property bool controlsVisible: true
    property int minImageDisplayMs: 2000
    property bool imageTimerActive: false

        Item {
        id: pageImage
        anchors.fill: parent

        property string source: appController.imageId != "" ?
                "image://pdfimages/" + appController.imageId :
                "qrc:/qt/qml/PDFNarrator/assets/images/background.svg"

        property int fadeDuration: 500

        Image {
            id: imgA
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            smooth: true
            cache: false
            opacity: 1
            Behavior on opacity { NumberAnimation { duration: pageImage.fadeDuration; easing.type: Easing.InOutQuad } }
        }
        Image {
            id: imgB
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            smooth: true
            cache: false
            opacity: 0
            Behavior on opacity { NumberAnimation { duration: pageImage.fadeDuration; easing.type: Easing.InOutQuad } }
        }

        property bool aIsFront: true

        onSourceChanged: {
            var front = aIsFront ? imgA : imgB
            var back  = aIsFront ? imgB : imgA

            back.source = source          
            back.opacity = 1               
            front.opacity = 0

            aIsFront = !aIsFront
        }

        Component.onCompleted: {
            imgA.source = source
            imgA.opacity = 1
        }
    }

    IconButton {
        id: download
        anchors.centerIn: parent
        iconSource: "qrc:/qt/qml/PDFNarrator/assets/images/download.svg"
        visible: appController.needsDownload
        enabled: visible
        width: parent.width * Style.downloadFactor
        height: width
        buttonRadius: 0.3

        property real progressPercentage: 0.0

        Connections {
            target: appController
            function onDownloadProgress(percentage) {
                download.progressPercentage = percentage
            }
        }

        Connections {
            target: appController
            function onNeedsDownloadChanged() {
                if (appController.needsDownload) {
                    download.progressPercentage = 0.0
                }
            }
        }

        Rectangle {
            id: progressContainer
            anchors.fill: parent
            radius: parent.width * parent.buttonRadius
            color: "transparent"
            layer.enabled: true
            clip: true
            z: -1 

            Rectangle {
                id: fillBar
                width: parent.width
                height: parent.height * (download.progressPercentage / 100.0)
                anchors.bottom: parent.bottom
                color: Qt.alpha(Style.accentColor || "#007ACC", 0.5)

                Behavior on height {
                    NumberAnimation { duration: 150 }
                }
            }
        }

        onClicked: {
            appController.download()
            download.enabled = false
        }
    }

    BusyIndicator {
        anchors.centerIn: parent
        // Only run and show if NO downloads are required
        running: !appController.needsDownload && (appController.playbackState === AppController.BUSY ||
                                                 appController.initializingTts)
        visible: running
        width: parent.width * Style.indicatorFactor
        height: width
    }

    IconButton {
        id: playPause
        anchors.centerIn: parent
        width: parent.width * Style.indicatorFactor
        height: width
        buttonRadius: 1.0
        
        // Hide if downloads are needed, or if busy, or if controls are toggled off
        visible: !appController.needsDownload 
                 && root.controlsVisible 
                 && appController.playbackState !== AppController.BUSY

        iconSource: {
            switch (appController.playbackState) {
            case AppController.RESTART:
                return "qrc:/qt/qml/PDFNarrator/assets/images/restart.svg"
            case AppController.PLAYING:
                return "qrc:/qt/qml/PDFNarrator/assets/images/pause.svg"
            case AppController.PAUSED:
                return "qrc:/qt/qml/PDFNarrator/assets/images/play.svg"
            default:
                return ""
            }
        }

        onClicked: {
            switch (appController.playbackState) {
            case AppController.RESTART:
                appController.restart()
                break
            case AppController.PLAYING:
                appController.pause()
                break
            case AppController.PAUSED:
                appController.play()
                break
            }
        }
    }
}
