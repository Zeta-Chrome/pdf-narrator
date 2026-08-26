pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

ComboBox {
    id: control

    padding: 10
    font.pixelSize: 14
    implicitWidth: contentItem.implicitWidth + padding * 2

    background: Rectangle {
        radius: 8
        color: control.pressed ? "#333333" : control.hovered ? "#2A2A2A" : "#1F1F1F"
        border.color: "#3A3A3A"
    }

    delegate: ItemDelegate {
        id: delegateItem

        required property var model
        required property var modelData
        required property int index

        width: control.width
        contentItem: Text {
            text: control.textRole ? delegateItem.model[control.textRole] : (delegateItem.modelData !== undefined ?  delegateItem.modelData : delegateItem.model)
            color: "#FF8F8F8F"
            font: control.font
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
            horizontalAlignment: Text.AlignHCenter
        }
        highlighted: control.highlightedIndex === delegateItem.index
    }

    contentItem: Text {
        text: control.currentText
        color: "white"
        font: control.font
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        leftPadding: 10
    }

    popup: Popup {
        y: control.height - 1
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight, 200)
        padding: 1

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex

            ScrollIndicator.vertical: ScrollIndicator {}
        }

        background: Rectangle {
            color: "#FF040404"
            border.color: "#FF1F1F1F"
            radius: 2
        }
    }
}
