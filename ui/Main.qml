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

    property bool controlsVisible: true
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
        controlsVisible = true;
        hideTimer.restart();
    }

    Item {
        id: keyboardHandler
        focus: true
        
        Keys.onPressed: (event) => {
            if (!appController.isPdfLoaded) {
                return;
            }
            
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
                    appController.prevLine();
                    event.accepted = true;
                    break;
                    
                case Qt.Key_Right:
                    appController.nextLine();
                    event.accepted = true;
                    break;
                    
                case Qt.Key_Up:
                    appController.prevPage();
                    event.accepted = true;
                    break;
                    
                case Qt.Key_Down:
                    appController.nextPage();
                    event.accepted = true;
                    break;
                    
                case Qt.Key_M:
                    appController.toggleMusic();
                    event.accepted = true;
                    break;
                    
                case Qt.Key_Escape:
                    if (appController.isPlaying) {
                        appController.pause();
                    }
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
        
        function onStatusMessage(message) {
            console.log("Status:", message);
            statusText.text = message;
            statusText.visible = true;
            statusTimer.restart();
        }
        
        function onErrorOccurred(error) {
            console.error("Error:", error);
            errorText.text = "Error: " + error;
            errorText.visible = true;
            errorTimer.restart();
        }

        function onIsInitializedChanged() {
            if (appController.isInitialized) {
                root.showControls();
            }
        }
    }

    // Status message display
    Rectangle {
        id: statusRect
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: parent.height * 0.15
        width: statusText.width + 40
        height: statusText.height + 20
        color: "#CC000000"
        radius: 10
        visible: statusText.visible
        z:1
        
        Text {
            id: statusText
            anchors.centerIn: parent
            color: "#00FF00"
            font.pixelSize: 16
            visible: false
        }
        
        Timer {
            id: statusTimer
            interval: 3000
            onTriggered: statusText.visible = false
        }
    }

    // Error message display
    Rectangle {
        id: errorRect
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: parent.height * 0.15
        width: errorText.width + 40
        height: errorText.height + 20
        color: "#CC000000"
        radius: 10
        visible: errorText.visible
        z:1
        
        Text {
            id: errorText
            anchors.centerIn: parent
            color: "#FF4444"
            font.pixelSize: 16
            visible: false
        }
        
        Timer {
            id: errorTimer
            interval: 5000
            onTriggered: errorText.visible = false
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

    Splash {}
}
