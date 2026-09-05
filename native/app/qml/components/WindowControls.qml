import QtQuick
import QtQuick.Controls
import ".."

Row {
    id: root

    property var controller: null

    ToolButton {
        objectName: "minimizeButton"
        width: 36
        height: 44
        property string accessibilityLabel: "minimize"
        Accessible.name: accessibilityLabel
        ToolTip.visible: hovered
        ToolTip.text: Accessible.name
        onClicked: if (root.controller) root.controller.minimizeWindow()
        contentItem: Image { objectName: "minimizeIcon"; anchors.centerIn: parent; width: 16; height: 16; source: Qt.resolvedUrl("../assets/icons/window-minimize.svg"); opacity: parent.hovered ? 1 : 0.82 }
        background: Rectangle { color: parent.hovered ? Theme.controlSurface : "transparent" }
    }
    ToolButton {
        objectName: "maximizeButton"
        width: 36
        height: 44
        property string accessibilityLabel: "maximize-or-restore"
        Accessible.name: accessibilityLabel
        ToolTip.visible: hovered
        ToolTip.text: Accessible.name
        onClicked: if (root.controller) root.controller.toggleMaximizedWindow()
        contentItem: Image { objectName: "maximizeIcon"; anchors.centerIn: parent; width: 15; height: 15; source: Qt.resolvedUrl("../assets/icons/window-maximize.svg"); opacity: parent.hovered ? 1 : 0.82 }
        background: Rectangle { color: parent.hovered ? Theme.controlSurface : "transparent" }
    }
    ToolButton {
        objectName: "closeButton"
        width: 36
        height: 44
        property string accessibilityLabel: "close"
        Accessible.name: accessibilityLabel
        ToolTip.visible: hovered
        ToolTip.text: Accessible.name
        onClicked: if (root.controller) root.controller.closeWindow()
        contentItem: Image {
            objectName: "windowCloseIcon"
            anchors.centerIn: parent
            width: 16
            height: 16
            source: Qt.resolvedUrl("../assets/icons/window-close.svg")
            opacity: parent.hovered ? 1 : 0.82
        }
        background: Rectangle { color: parent.hovered ? Theme.danger : "transparent" }
    }
}
