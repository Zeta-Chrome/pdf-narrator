import QtQuick
import QtQuick.Controls
import QtQuick.Window
import PDFNarrator
import "components"

ApplicationWindow {
    id: root
    visibility: Window.Maximized
    title: "PDF Narrator"
    color: "#000000"

    property bool isReady: !appController.needsDownload && !appController.initializingTts
    property bool controlsVisible: isReady
    property int hideControlsDelay: 3000

    Timer {
        id: hideTimer
        interval: root.hideControlsDelay
        running: false
        repeat: false
        onTriggered: {
            root.controlsVisible = false;
        }
    }

    function showControls() {
        controlsVisible = isReady;
        hideTimer.restart();
    }

    Item {
        id: keyboardHandler
        focus: true
        
        Keys.onPressed: (event) => {      
            switch(event.key) {
                case Qt.Key_Space:
                    if (appController.isPlaying) {
                        appController.pause();
                    } else {
                        appController.play();
                    }
                    event.accepted = true;
                    break;
                    
                case Qt.Key_Left:
                    appController.appController.seekSentence(AppController.PREV);
                    event.accepted = true;
                    break;
                    
                case Qt.Key_Right:
                    appController.seekSentence(AppController.NEXT);
                    event.accepted = true;
                    break;
                    
                case Qt.Key_Up:
                    appController.goToPage(appController.currentPage + 1);
                    event.accepted = true;
                    break;
                    
                case Qt.Key_Down:
                    appController.goToPage(appController.currentPage - 1)();
                    event.accepted = true;
                    break;
                    
                case Qt.Key_M:
                    appController.toggleMusic();
                    event.accepted = true;
                    break;
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        propagateComposedEvents: true

        onPositionChanged: {
            root.showControls();
        }

        onClicked: {
            root.showControls();
            keyboardHandler.forceActiveFocus();
        }
    }

    PDFCanvas {
        id: pdfCanvas
        anchors.fill: parent
        controlsVisible: root.controlsVisible
    }

    Connections {
        target: appController
        
        function onStatusMessage(persistent, message) {
            console.log("Status:", message);
            statusText.text = message;
            statusRect.visible = true;
            errorRect.visible = false;
            if (!persistent)
            {
                statusTimer.restart();
            }
        }
        
        function onErrorOccurred(persistent, error) {
            console.error("Error:", error);
            errorText.text = "Error: " + error;
            errorRect.visible = true;
            statusRect.visible = false;
            if (!persistent)
            {
                errorTimer.restart();
            }
        }

        function onInitializingTtsChanged() {
            if (appController.initializingTts) {
                root.controlsVisible = false;
            } else {
                root.showControls();
            }
        }
    }

    // Status message display
    Rectangle {
        id: statusRect
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: parent.height * 0.3
        width: Math.min(statusText.implicitWidth + 40, root.width * 0.8)
        height: statusText.height + 20
        color: "#66000000"
        radius: 10
        visible: false
        z: 10

        Text {
            id: statusText
            anchors.centerIn: parent
            width: parent.width - 40
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            color: "#00FF00"
            font.pixelSize: 18
            font.bold: true
        }

        Timer {
            id: statusTimer
            interval: 3000
            onTriggered: statusRect.visible = false
        }
    }

    // Error message display
    Rectangle {
        id: errorRect
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: parent.height * 0.3

        width: Math.min(errorText.implicitWidth + 40, root.width * 0.8)
        height: errorText.height + 20
        color: "#66000000" 
        radius: 10
        visible: false
        z: 10

        Text {
            id: errorText
            anchors.centerIn: parent
            width: parent.width - 40
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            color: "#FF4444" 
            font.pixelSize: 18
            font.bold: true
        }

        Timer {
            id: errorTimer
            interval: 5000
            onTriggered: errorRect.visible = false
        }
    }

    TopControlBar {    
        id: topControlBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height * Style.topBarHFactor
        visible: root.controlsVisible
        windowHeight: root.height
        z:2
    }

    BottomControlBar {
        id: bottomControlBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height * Style.bottomBarHFactor
        visible: root.controlsVisible
        z:2
    }
}
