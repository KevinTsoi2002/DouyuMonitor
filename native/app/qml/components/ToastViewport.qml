import QtQuick
import QtQuick.Controls
import ".."

Item {
    id: root

    property var workspaceModel: null
    property color borderColor: Theme.border
    property color accentColor: Theme.accent
    property color textColor: Theme.text
    property color mutedTextColor: Theme.mutedText
    readonly property string message: workspaceModel ? workspaceModel.lastMessage : ""
    readonly property string level: workspaceModel ? workspaceModel.lastMessageLevel : "info"
    readonly property int autoDismissMs: workspaceModel ? workspaceModel.lastMessageTimeoutMs : 0

    width: 380
    height: toast.visible ? 48 : 0
    z: 20

    Timer {
        id: autoDismiss
        interval: root.autoDismissMs
        repeat: false
        running: root.message.length > 0 && root.autoDismissMs > 0
        onTriggered: if (root.workspaceModel && root.workspaceModel.lastMessage === root.message) root.workspaceModel.setLastMessage("")
    }

    onMessageChanged: {
        if (root.message.length > 0 && root.autoDismissMs > 0) autoDismiss.restart()
        else autoDismiss.stop()
    }

    Rectangle {
        id: toast
        anchors.fill: parent
        visible: root.message.length > 0
        radius: Theme.radiusMedium
        color: root.level === "error" ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.16) : root.level === "success" ? Qt.rgba(Theme.online.r, Theme.online.g, Theme.online.b, 0.14) : Theme.controlSurface
        border.color: root.level === "error" ? Theme.danger : root.level === "success" ? Theme.online : root.borderColor

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.right: dismiss.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            color: root.textColor
            elide: Text.ElideRight
            font.pixelSize: 11
            text: root.message
        }

        ToolButton {
            id: dismiss
            objectName: "toastDismissButton"
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: 27
            height: 27
            Accessible.name: "关闭通知"
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name
            onClicked: if (root.workspaceModel) root.workspaceModel.setLastMessage("")
            contentItem: Image { anchors.centerIn: parent; width: 15; height: 15; source: Qt.resolvedUrl("../assets/icons/x.svg") }
            background: Rectangle { radius: 4; color: parent.hovered ? "#252c34" : "transparent" }
        }
    }
}
