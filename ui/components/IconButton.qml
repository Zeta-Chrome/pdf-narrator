import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Button {
    id: root 

    property color iconColor: "#FF8F8F8F"
    property alias iconSource: icon.source
    property alias iconRotation: effect.rotation
    property real buttonRadius: 0

    hoverEnabled: Qt.platform.os !== "android"
    background: Rectangle {
        color: root.pressed ? "#3FFFFFFF" : (root.hovered ? "#1FFFFFFF" : "#00000000")
        radius: parent.width * root.buttonRadius 
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
