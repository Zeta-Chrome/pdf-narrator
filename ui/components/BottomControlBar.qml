import QtQuick
import QtQuick.Layouts
import PDFNarrator

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
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        IconButton {
            id: openMusic
            iconSource: "qrc:/PDFNarrator/assets/images/prev_line.svg"
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        IconButton {
            id: toggleMusic
            iconSource: "qrc:/PDFNarrator/assets/images/prev_line.svg"
            iconRotation: 180
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
        }

        IconButton {
            id: settings
            iconSource: "qrc:/PDFNarrator/assets/images/prev_page.svg"
            iconRotation: 180
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
        }
    }
}
