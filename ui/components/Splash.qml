import QtQuick
import PDFNarrator

Rectangle {
    anchors.fill: parent
    color: "#000000"
    visible: !appController.isInitialized
    z: 999
    
    MouseArea {
        anchors.fill: parent
        enabled: parent.visible
    }

    Image {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: -30
        width: parent.width * 0.18
        height: width
        source: "qrc:PDFNarrator/assets/images/icon.svg"
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: parent.height * 0.12
        text: "PDF NARRATOR"
        color: "#ffffff"
        font.pixelSize: 18
        font.letterSpacing: 6
        font.weight: Font.Light
    }
}
