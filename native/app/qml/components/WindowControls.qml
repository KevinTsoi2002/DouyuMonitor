import QtQuick
import QtQuick.Controls
import ".."
import "../controls"

Row {
    id: root

    property var controller: null
    readonly property bool maximized: root.Window.visibility === Window.Maximized
    signal closeRequested()
    height: Theme.headerControlHeight

    ConsoleToolButton {
        objectName: "minimizeButton"
        width: Theme.headerControlHeight
        height: Theme.headerControlHeight
        padding: 4
        property string accessibilityLabel: "最小化窗口"
        Accessible.name: accessibilityLabel
        ToolTip.visible: false
        ToolTip.text: Accessible.name
        onClicked: if (root.controller) root.controller.minimizeWindow()
        contentItem: ThemeIcon { objectName: "minimizeIcon"; anchors.centerIn: parent; width: 16; height: 16; source: Qt.resolvedUrl("../assets/icons/window-minimize.svg"); opacity: parent.hovered ? 1 : 0.82 }
    }
    ConsoleToolButton {
        objectName: "maximizeButton"
        width: Theme.headerControlHeight
        height: Theme.headerControlHeight
        padding: 4
        property string accessibilityLabel: root.maximized ? qsTr("还原窗口") : qsTr("最大化窗口")
        Accessible.name: accessibilityLabel
        ToolTip.visible: false
        ToolTip.text: Accessible.name
        onClicked: if (root.controller) root.controller.toggleMaximizedWindow()
        contentItem: ThemeIcon {
            objectName: "maximizeIcon"
            anchors.centerIn: parent
            width: 15
            height: 15
            sourceSize: Qt.size(15, 15)
            source: root.maximized
                    ? Qt.resolvedUrl("../assets/icons/window-restore.svg")
                    : Qt.resolvedUrl("../assets/icons/window-maximize.svg")
            opacity: parent.hovered ? 1 : 0.82
        }
    }
    ConsoleToolButton {
        objectName: "closeButton"
        width: Theme.headerControlHeight
        height: Theme.headerControlHeight
        padding: 4
        property string accessibilityLabel: "关闭窗口"
        destructive: true
        Accessible.name: accessibilityLabel
        ToolTip.visible: false
        ToolTip.text: Accessible.name
        onClicked: {
            if (!root.controller) return
            if (root.controller.closeBehavior === "ask") root.closeRequested()
            else root.controller.closeWindow()
        }
        contentItem: ThemeIcon {
            objectName: "windowCloseIcon"
            anchors.centerIn: parent
            width: 16
            height: 16
            source: Qt.resolvedUrl("../assets/icons/window-close.svg")
            opacity: parent.hovered ? 1 : 0.82
        }
    }
}
