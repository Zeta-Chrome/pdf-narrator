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
                onMoved: {
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
                value: appController.musicVolume
                stepSize: 0.01
                onMoved: {
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

            Repeater {
                id: modelRepeater
                model: appController.ttsModels

                delegate: Rectangle {
                    id: modelRow
                    required property string modelData
                    required property int index

                    property bool downloaded: appController.isModelDownloaded(index)
                    property real progress: 0
                    property bool downloading: false

                    width: parent ? parent.width : 0
                    height: 44
                    radius: 6
                    color: appController.ttsModel === index ? "#33FFFFFF" : "transparent"

                    Connections {
                        target: appController
                        function onModelDownloadProgress(id, pct) {
                            if (id === modelRow.index) {
                                modelRow.downloading = true;
                                modelRow.progress = pct;
                            }
                        }
                        function onModelDownloadFinished(id, success) {
                            if (id === modelRow.index) {
                                modelRow.downloading = false;
                                modelRow.downloaded = success;
                            }
                        }
                    }

                    Row {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10

                        RadioButton {
                            anchors.verticalCenter: parent.verticalCenter
                            checked: appController.ttsModel === modelRow.index
                            enabled: !modelRow.downloading
                            onClicked: {
                                if (!modelRow.downloaded) {
                                    modelRow.downloading = true;
                                }
                                appController.ttsModel = modelRow.index;
                            }
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelRow.modelData
                            color: "white"
                            font.pixelSize: 14
                            width: parent.width - 140
                            elide: Text.ElideRight
                        }

                        // Not downloaded and idle: show a tappable download prompt
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: !modelRow.downloaded && !modelRow.downloading
                            text: "↓ Download"
                            color: "#66CCFF"
                            font.pixelSize: 11

                            MouseArea {
                                anchors.fill: parent
                                enabled: !modelRow.downloading
                                onClicked: {
                                    modelRow.downloading = true;
                                    appController.downloadModel(modelRow.index);
                                }
                            }
                        }

                        // Currently downloading: show progress
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: modelRow.downloading
                            text: Math.round(modelRow.progress) + "%"
                            color: "#AAAAAA"
                            font.pixelSize: 11
                        }

                        // Already downloaded
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: modelRow.downloaded && !modelRow.downloading
                            text: "✓ Ready"
                            color: "#66FF99"
                            font.pixelSize: 13
                        }
                    }
                }
            }
        }
    }
}
