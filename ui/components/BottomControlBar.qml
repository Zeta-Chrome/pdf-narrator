import QtQuick
import QtQuick.Layouts
import PDFNarrator

Rectangle {
    id: root
    color: "#af1f1f1f"

    RowLayout {
        anchors.fill: parent
        anchors.margins: 5
        spacing: 5

        IconButton {
            id: prevPage 
            iconSource: "qrc:/qt/qml/PDFNarrator/assets/images/prev_page.svg"
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
            onClicked: appController.goToPage(appController.currentPage - 1)
        }

        IconButton {
            id: prevLine 
            iconSource: "qrc:/qt/qml/PDFNarrator/assets/images/prev_line.svg"
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
            onClicked: appController.seekSentence(AppController.PREV)
        }

        IconButton {
            id: nextLine 
            iconSource: "qrc:/qt/qml/PDFNarrator/assets/images/prev_line.svg"
            iconRotation: 180
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
            onClicked: appController.seekSentence(AppController.NEXT)
        }

        IconButton {
            id: nextPage 
            iconSource: "qrc:/qt/qml/PDFNarrator/assets/images/prev_page.svg"
            iconRotation: 180
            buttonRadius: Style.buttonRadiusFactor
            Layout.fillWidth: true
            Layout.fillHeight: true
            onClicked: appController.goToPage(appController.currentPage + 1)
        }
    }
}
