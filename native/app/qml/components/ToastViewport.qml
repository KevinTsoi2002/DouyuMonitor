import QtQuick
import QtQuick.Controls.Basic
import ".."
import "../controls"

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
    readonly property bool shouldDisplay: message.length > 0
                                       && (level === "error" || level === "warning" || level === "action")

    width: 380
    height: toast.visible ? Math.max(48, messageText.implicitHeight + 24) : 0
    z: 200

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
        objectName: "toastSurface"
        anchors.fill: parent
        visible: root.shouldDisplay
        radius: Theme.radiusMedium
        color: Theme.managementSurface
        border.color: root.level === "error" ? Theme.danger
                                               : root.level === "warning" ? Theme.warning : root.borderColor

        Text {
            id: messageText
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.right: dismiss.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            color: root.textColor
            wrapMode: Text.Wrap
            font.pixelSize: Theme.fontControl
            text: root.message
        }

        ConsoleToolButton {
            id: dismiss
            objectName: "toastDismissButton"
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            width: 27
            height: 27
            Accessible.name: "关闭通知"
            ToolTip.visible: false
            ToolTip.text: Accessible.name
            onClicked: if (root.workspaceModel) root.workspaceModel.setLastMessage("")
            contentItem: ThemeIcon { anchors.centerIn: parent; width: 15; height: 15; source: Qt.resolvedUrl("../assets/icons/x.svg") }
        }
    }
}
