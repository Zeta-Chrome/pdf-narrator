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
            id: prevPage 
            iconSource: "qrc:/PDFNarrator/assets/images/prev_page.svg"
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
            onClicked: appController.prevPage()
        }

        IconButton {
            id: prevLine 
            iconSource: "qrc:/PDFNarrator/assets/images/prev_line.svg"
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
            onClicked: appController.prevLine()
        }

        IconButton {
            id: nextLine 
            iconSource: "qrc:/PDFNarrator/assets/images/prev_line.svg"
            iconRotation: 180
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
            onClicked: appController.nextLine()
        }

        IconButton {
            id: nextPage 
            iconSource: "qrc:/PDFNarrator/assets/images/prev_page.svg"
            iconRotation: 180
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
            onClicked: appController.nextPage()
        }
    }
}
