import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects
import PDFNarrator

Button {
    id: root
    Layout.fillWidth: true
    Layout.fillHeight: true

    property color iconColor: "#FF8F8F8F"
    property alias iconSource: icon.source
    property alias iconRotation: effect.rotation

    hoverEnabled: Qt.platform.os !== "android"
    background: Rectangle {
        color: root.pressed ? "#3FFFFFFF" : (root.hovered ? "#1FFFFFFF" : "#00000000")
        radius: parent.width * Style.buttonRadiusFactor
    }

    Image {
        id: icon
        anchors.centerIn: parent
        source: ""
        width: parent.width * 0.8 
        height: parent.height * 0.8
        fillMode: Image.PreserveAspectFit
        visible: false
    }

    MultiEffect {
        id: effect
        source: icon
        anchors.fill: icon 
        colorization: 1
        colorizationColor: root.iconColor
    }
}
