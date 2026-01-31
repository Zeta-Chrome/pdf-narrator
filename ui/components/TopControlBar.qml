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
        }

        IconButton {
            id: openMusic
            iconSource: "qrc:/PDFNarrator/assets/images/open_music.svg"
            onClicked: musicDialog.open()
        }

        Row {
            spacing: 10

            TextField {
                id: pageField
                text: "2"
                implicitWidth: parent.width * 0.5
                horizontalAlignment: Text.AlignHCenter
                color: "white"

                background: Rectangle {
                    color: "#20FFFFFF"
                    radius: 3
                }

                validator: IntValidator {
                    bottom: 1
                    top: 100
                }

                onAccepted: {
                    console.log(text);
                }
            }

            Text {
                text: "/ " + "100"
                color: "white"
                font.pixelSize: 16
                topPadding: Style.totalPagesTopPadding 
            }
        }

        IconButton {
            id: toggleMusic
            iconSource: "qrc:/PDFNarrator/assets/images/music_on.svg"
        }

        IconButton {
            id: settings
            iconSource: "qrc:/PDFNarrator/assets/images/settings.svg"
            onClicked: settingsPopup.open()
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
