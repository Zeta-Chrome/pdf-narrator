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
                text: "Speech Speed: " + speechRateSlider.value.toFixed(1) + "x"
                color: "white"
                font.pixelSize: 14
            }

            Slider {
                id: speechRateSlider
                width: parent.width
                from: 0.5
                to: 2.0
                value: appController.ttsSpeed
                stepSize: 0.1

                onValueChanged: {
                    appController.ttsSpeed = value;
                }
            }
        }

        Column {
            width: parent.width
            spacing: 10

            Text {
                text: "Music Volume: " + (musicVolumeSlider.value * 100).toFixed(0) + "%"
                color: "white"
                font.pixelSize: 14
            }

            Slider {
                id: musicVolumeSlider
                width: parent.width
                from: 0
                to: 1.0
                value: 0.3
                stepSize: 0.05

                onValueChanged: {
                    appController.musicVolume = value;
                }
            }
        }

        Column {
            width: parent.width
            spacing: 10

            Text {
                text: "Speakers"
                color: "white"
                font.pixelSize: 14
            }

            CustomComboBox {
                id: ttsVoiceList
                width: parent.width
                model: appController.ttsVoices
                currentIndex: appController.ttsSpeaker

                onActivated: function(index) {
                    appController.ttsSpeaker = index;
                }
            }
        }

        Column {
            width: parent.width
            spacing: 10

            Text {
                text: "TTS Models"
                color: "white"
                font.pixelSize: 14
            }

            CustomComboBox {
                id: ttsModelList
                width: parent.width
                model: appController.ttsModels
                currentIndex: appController.ttsModel

                onActivated: function(index) {
                    appController.ttsModel = index;
                }
            }
        }
    }
}
