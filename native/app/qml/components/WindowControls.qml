import QtQuick
import QtQuick.Controls
import ".."

Row {
    id: root

    property var controller: null
    signal closeRequested()
    height: Theme.controlHeight

    ToolButton {
        objectName: "minimizeButton"
        width: Theme.controlHeight
        height: Theme.controlHeight
        property string accessibilityLabel: "最小化窗口"
        Accessible.name: accessibilityLabel
        ToolTip.visible: hovered
        ToolTip.text: Accessible.name
        onClicked: if (root.controller) root.controller.minimizeWindow()
        contentItem: Image { objectName: "minimizeIcon"; anchors.centerIn: parent; width: 16; height: 16; source: Qt.resolvedUrl("../assets/icons/window-minimize.svg"); opacity: parent.hovered ? 1 : 0.82 }
        background: Rectangle {
            radius: Theme.radiusSmall
            color: parent.down ? Theme.well : (parent.hovered ? Theme.controlSurface : "transparent")
        }
    }
    ToolButton {
        objectName: "maximizeButton"
        width: Theme.controlHeight
        height: Theme.controlHeight
        property string accessibilityLabel: "最大化窗口"
        Accessible.name: accessibilityLabel
        ToolTip.visible: hovered
        ToolTip.text: Accessible.name
        onClicked: if (root.controller) root.controller.toggleMaximizedWindow()
        contentItem: Image { objectName: "maximizeIcon"; anchors.centerIn: parent; width: 15; height: 15; source: Qt.resolvedUrl("../assets/icons/window-maximize.svg"); opacity: parent.hovered ? 1 : 0.82 }
        background: Rectangle {
            radius: Theme.radiusSmall
            color: parent.down ? Theme.well : (parent.hovered ? Theme.controlSurface : "transparent")
        }
    }
    ToolButton {
        objectName: "closeButton"
        width: Theme.controlHeight
        height: Theme.controlHeight
        property string accessibilityLabel: "关闭窗口"
        Accessible.name: accessibilityLabel
        ToolTip.visible: hovered
        ToolTip.text: Accessible.name
        onClicked: {
            if (!root.controller) return
            if (root.controller.closeBehavior === "ask") root.closeRequested()
            else root.controller.closeWindow()
        }
        contentItem: Image {
            objectName: "windowCloseIcon"
            anchors.centerIn: parent
            width: 16
            height: 16
            source: Qt.resolvedUrl("../assets/icons/window-close.svg")
            opacity: parent.hovered ? 1 : 0.82
        }
        background: Rectangle {
            radius: Theme.radiusSmall
            color: parent.down
                   ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.42)
                   : (parent.hovered
                      ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.28)
                      : "transparent")
        }
    }
}
