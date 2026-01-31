pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

Popup {
    id: root
    modal: true
    focus: true

    background: Rectangle {
        color: "#DD000000"
        radius: 10
        border.color: "#40FFFFFF"
        border.width: 1
    }

    contentItem: Column {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 20

        Text {
            text: "Settings"
            color: "white"
            font.pixelSize: 20
            font.bold: true
        }

        Column {
            width: parent.width
            spacing: 10

            Text {
                text: "Speech Rate"
                color: "white"
                font.pixelSize: 14
            }

            Slider {
                id: speechRateSlider
                width: parent.width
                from: 0.5
                to: 2
                value: 1.0
            }
        }

        Column {
            width: parent.width
            spacing: 10

            Text {
                text: "Music Volume"
                color: "white"
                font.pixelSize: 14
            }

            Slider {
                id: musicVolumeSlider
                width: parent.width
                from: 0
                to: 1
                value: 0.3
            }
        }

        ComboBox {
            id: ttsVoiceList
            width: parent.width
            model: ["Low", "Medium", "High"]

            padding: 10
            implicitWidth: contentItem.implicitWidth + padding * 2
            font.pixelSize: 14

            background: Rectangle {
                radius: 8
                color: ttsVoiceList.pressed ? "#333333" : ttsVoiceList.hovered ? "#2A2A2A" : "#1F1F1F"
                border.color: "#3A3A3A"
            }

             delegate: ItemDelegate {
                id: delegate

                required property var model
                required property int index

                width: ttsVoiceList.width
                contentItem: Text {
                    text: delegate.model[ttsVoiceList.textRole]
                    color: "#FF8F8F8F"
                    font: ttsVoiceList.font
                    elide: Text.ElideRight
                    verticalAlignment: Text.AlignVCenter
                    horizontalAlignment: Text.AlignHCenter
                }
                highlighted: ttsVoiceList.highlightedIndex === index
            }

            contentItem: Text {
                text: ttsVoiceList.currentText
                color: "white"
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                leftPadding: 10
            } 

            popup: Popup {
                y: ttsVoiceList.height - 1
                width: ttsVoiceList.width
                height: Math.min(contentItem.implicitHeight, root.height - topMargin - bottomMargin)
                padding: 1

                contentItem: ListView {
                    clip: true
                    implicitHeight: contentHeight
                    model: ttsVoiceList.popup.visible ? ttsVoiceList.delegateModel : null
                    currentIndex: ttsVoiceList.highlightedIndex

                    ScrollIndicator.vertical: ScrollIndicator {}
                }

                background: Rectangle {
                    color: "#FF040404"
                    border.color: "#FF1F1F1F"
                    radius: 2
                }
            }
            onCurrentTextChanged: {
                console.log("Selected:", currentText);
            }
        }
    }
}
