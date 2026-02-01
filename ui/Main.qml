import QtQuick
import QtQuick.Controls
import QtQuick.Window
import PDFNarrator
import "components"

ApplicationWindow {
    id: root
    visible: true
    title: "PDF Narrator"
    color: "#000000"

    property bool controlsVisible: true
    property int hideControlsDelay: 3000

    Component.onCompleted: {
        root.showMaximized();
    }

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

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        propagateComposedEvents: true

        onPositionChanged: {
            root.showControls();
        }

        onClicked: {
            root.showControls();
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
            console.log(message); 
        }
        function onErrorOccurred(error) {
            console.log("Error: " + error) 
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
    }

    BottomControlBar {
        id: bottomControlBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height * Style.bottomBarHFactor
        visible: root.controlsVisible
    }
}
