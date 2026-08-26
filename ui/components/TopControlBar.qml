import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import PDFNarrator

Rectangle {
    id: root
    color: "#af1f1f1f"
    property real windowHeight: 0

    RowLayout {
        anchors.fill: parent
        anchors.margins: 5
        spacing: 5

        IconButton {
            id: openPDF
            iconSource: "qrc:/qt/qml/PDFNarrator/assets/images/open_pdf.svg"
            onClicked: pdfDialog.open()
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        IconButton {
            id: openMusic
            iconSource: "qrc:/qt/qml/PDFNarrator/assets/images/open_music.svg"
            onClicked: musicDialog.open()
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        Item {
            Layout.preferredWidth: 120
            Layout.fillHeight: true

            Row {
                anchors.centerIn: parent
                spacing: 5

                TextField {
                    id: pageField
                    text: appController.totalPages === 0 ? "0" : (appController.currentPage + 1).toString()
                    leftPadding: 0
                    rightPadding: 0
                    topPadding: 0
                    bottomPadding: 0
                    font.pixelSize: 16
                    verticalAlignment: TextInput.AlignVCenter
                    horizontalAlignment: TextInput.AlignHCenter
                    color: "white"
                    background: Rectangle {
                        color: "transparent"
                    }
                    validator: IntValidator {
                        bottom: 1
                        top: Math.max(1, appController.totalPages)
                    }
                    onAccepted: {
                        var pageNum = parseInt(text) - 1
                        appController.goToPage(pageNum)
                        focus = false
                    }
                    Connections {
                        target: appController
                        function onCurrentPageChanged() {
                            if (!pageField.activeFocus) {
                                pageField.text = (appController.currentPage + 1).toString()
                            }
                        }
                    }
                }

                Text {
                    text: "/"
                    color: "white"
                    font.pixelSize: 16
                    verticalAlignment: Text.AlignVCenter
                }

                Text {
                    text: appController.totalPages
                    color: "white"
                    font.pixelSize: 16
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        IconButton {
            id: toggleMusic
            iconSource: appController.isMusicEnabled ? 
                        "qrc:/qt/qml/PDFNarrator/assets/images/music_on.svg" :
                        "qrc:/qt/qml/PDFNarrator/assets/images/music_off.svg"
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
            onClicked: appController.toggleMusic()
        }

        IconButton {
            id: settings
            iconSource: "qrc:/qt/qml/PDFNarrator/assets/images/settings.svg"
            onClicked: settingsPopup.open()
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        FileDialog {
            id: pdfDialog
            title: "Open PDF"
            nameFilters: ["PDF files (*.pdf)"]
            onAccepted: {
                var path = selectedFile.toString();
                appController.openPDF(path)
            }
        }

        FileDialog {
            id: musicDialog
            title: "Open Music"
            nameFilters: ["Audio files (*.mp3 *.wav *.ogg *.flac)"]
            onAccepted: {
                var path = selectedFile.toString();
                appController.openMusic(path)
            }
        }
    }

    SettingsPopup {
        id: settingsPopup
        x: parent.width - width - 10
        y: parent.height + 5
        width: parent.width * Style.settingsWFactor
        height: Math.min(implicitHeight + 25, root.windowHeight)  
    }
}
