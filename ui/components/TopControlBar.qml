import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import PDFNarrator

Rectangle {
    id: root
    color: "#3F8F8F8F"
    property real windowHeight: 0

    RowLayout {
        anchors.fill: parent
        anchors.margins: 5
        spacing: 5

        IconButton {
            id: openPDF
            iconSource: "qrc:/PDFNarrator/assets/images/open_pdf.svg"
            onClicked: pdfDialog.open()
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        IconButton {
            id: openMusic
            iconSource: "qrc:/PDFNarrator/assets/images/open_music.svg"
            onClicked: musicDialog.open()
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        Row {
            spacing: 10
            Layout.preferredWidth: 120

            TextField {
                id: pageField
                text: appController.totalPages == 0 ? 0 : appController.currentPage + 1
                implicitWidth: 60
                horizontalAlignment: Text.AlignHCenter
                color: "white"

                background: Rectangle {
                    color: "#20FFFFFF"
                    radius: 3
                }

                validator: IntValidator {
                    bottom: 1
                    top: appController.totalPages
                }

                onAccepted: {
                    var pageNum = parseInt(text) - 1
                    if (pageNum >= 0 && pageNum < appController.totalPages) {
                        appController.setCurrentPage(pageNum)
                    } else {
                        text = appController.currentPage + 1
                    }
                }
                
                Connections {
                    target: appController
                    function onCurrentPageChanged() {
                        if (!pageField.activeFocus) {
                            pageField.text = appController.currentPage + 1
                        }
                    }
                }
            }

            Text {
                text: "/ " + String(appController.totalPages)
                color: "white"
                font.pixelSize: 16
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        IconButton {
            id: toggleMusic
            iconSource: appController.isMusicEnabled ? 
                        "qrc:/PDFNarrator/assets/images/music_on.svg" :
                        "qrc:/PDFNarrator/assets/images/music_off.svg"
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
            onClicked: appController.toggleMusic()
        }

        IconButton {
            id: settings
            iconSource: "qrc:/PDFNarrator/assets/images/settings.svg"
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
                if (path.startsWith("file://")) {
                    path = path.substring(7);
                }
                appController.openPDF(path)
            }
        }

        FileDialog {
            id: musicDialog
            title: "Open Music"
            nameFilters: ["Audio files (*.mp3 *.wav *.ogg *.flac)"]
            onAccepted: {
                var path = selectedFile.toString();
                if (path.startsWith("file://")) {
                    path = path.substring(7);
                }
                appController.openMusic(path)
            }
        }
    }

    SettingsPopup {
        id: settingsPopup
        x: parent.width - width - 10
        y: parent.height + 5
        width: parent.width * Style.settingsWFactor
        height: root.windowHeight * Style.settingsHFactor
    }
}
