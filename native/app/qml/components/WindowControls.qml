import QtQuick
import QtQuick.Controls

Row {
    id: root

    property var controller: null

    ToolButton {
        width: 36
        height: 44
        Accessible.name: "最小化"
        ToolTip.visible: hovered
        ToolTip.text: Accessible.name
        onClicked: if (root.controller) root.controller.minimizeWindow()
        contentItem: Image { anchors.centerIn: parent; width: 16; height: 16; source: Qt.resolvedUrl("../assets/icons/minus.svg"); opacity: parent.hovered ? 1 : 0.82 }
        background: Rectangle { color: parent.hovered ? "#252c34" : "transparent" }
    }
    ToolButton {
        width: 36
        height: 44
        Accessible.name: "最大化或还原"
        ToolTip.visible: hovered
        ToolTip.text: Accessible.name
        onClicked: if (root.controller) root.controller.toggleMaximizedWindow()
        contentItem: Image { anchors.centerIn: parent; width: 15; height: 15; source: Qt.resolvedUrl("../assets/icons/square.svg"); opacity: parent.hovered ? 1 : 0.82 }
        background: Rectangle { color: parent.hovered ? "#252c34" : "transparent" }
    }
    ToolButton {
        width: 36
        height: 44
        Accessible.name: "关闭"
        ToolTip.visible: hovered
        ToolTip.text: Accessible.name
        onClicked: if (root.controller) root.controller.closeWindow()
        contentItem: Image {
            objectName: "windowCloseIcon"
            anchors.centerIn: parent
            width: 16
            height: 16
            source: Qt.resolvedUrl("../assets/icons/x.svg")
            opacity: parent.hovered ? 1 : 0.82
        }
        background: Rectangle { color: parent.hovered ? "#c42b1c" : "transparent" }
    }
}
