import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#3F8F8F8F"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 5
        spacing: 5

        IconButton {
            id: openPDF
            iconSource: "qrc:/PDFNarrator/assets/images/prev_page.svg"
        }

        IconButton {
            id: openMusic
            iconSource: "qrc:/PDFNarrator/assets/images/prev_line.svg"
        }

        IconButton {
            id: toggleMusic
            iconSource: "qrc:/PDFNarrator/assets/images/prev_line.svg"
            iconRotation: 180
        }

        IconButton {
            id: settings
            iconSource: "qrc:/PDFNarrator/assets/images/prev_page.svg"
            iconRotation: 180
        }
    }
}
