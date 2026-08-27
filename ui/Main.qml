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

    // Safe fallbacks to prevent undefined lookup crashes during startup
    readonly property bool isEditing: (typeof topControlBar !== "undefined" && topControlBar !== null) ? topControlBar.isEditingPage : false
    readonly property bool isReady: !appController.needsDownload && !appController.initializingTts

    property bool manualControlsVisible: true
    property bool controlsVisible: isReady && (manualControlsVisible || isEditing)
    property int hideControlsDelay: 3000

    Timer {
        id: hideTimer
        interval: root.hideControlsDelay
        running: false
        repeat: false
        onTriggered: {
            if (!root.isEditing) {
                root.manualControlsVisible = false;
            }
        }
    }

    function showControls() {
        manualControlsVisible = true;
        if (!root.isEditing) {
            hideTimer.restart();
        } else {
            hideTimer.stop();
        }
    }

    onIsEditingChanged: {
        if (isEditing) {
            hideTimer.stop();
        } else {
            hideTimer.restart();
        }
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
                    appController.seekSentence(AppController.PREV);
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
                    appController.goToPage(appController.currentPage - 1);
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

        onPositionChanged: root.showControls()
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
        
        function onInfoMessage(message) {
            infoText.text = message;
            infoRect.visible = true;
            console.log(infoText.text)
        }

        function onInfoClose() {
            infoRect.visible = false;
        }
        
        function onStatusMessage(persistent, message) {
            statusText.text = "Status: " + message;
            statusRect.visible = true;
            errorRect.visible = false;
            if (!persistent) {
                statusTimer.restart();
            }
            console.log(statusText.text)
        }
        
        function onErrorOccurred(persistent, error) {
            errorText.text = "Error: " + error;
            errorRect.visible = true;
            statusRect.visible = false;
            if (!persistent) {
                errorTimer.restart();
            }
            console.log(errorText.text)
        }

        function onInitializingTtsChanged() {
            if (appController.initializingTts) {
                root.manualControlsVisible = false;
            } else {
                root.showControls();
            }
        }
    }

    // Top control bar
    TopControlBar {    
        id: topControlBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height * Style.topBarHFactor
        visible: root.controlsVisible
        windowHeight: root.height
        z: 2
    }

    // Bottom control bar
    BottomControlBar {
        id: bottomControlBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height * Style.bottomBarHFactor
        visible: root.controlsVisible
        z: 2
    }

    // Info Message Display
    Rectangle {
        id: infoRect
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: parent.height * 0.1
        width: Math.min(infoText.implicitWidth + 40, root.width * 0.9)
        height: infoText.height + 20
        color: "#AA000000"
        radius: 10
        visible: false
        z: 10

        Text {
            id: infoText
            anchors.centerIn: parent
            width: parent.width - 40
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            color: "#EFFFFF"
            font.pixelSize: 18
            font.bold: true
        }
    }

    // Status Message Display
    Rectangle {
        id: statusRect
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: parent.height * 0.2
        width: Math.min(statusText.implicitWidth + 40, root.width * 0.9)
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

    // Error Message Display
    Rectangle {
        id: errorRect
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: parent.height * 0.2
        width: Math.min(errorText.implicitWidth + 40, root.width * 0.9)
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

    Component.onCompleted: {
        showControls();
    }
}
