import QtQuick
import QtQuick.Controls

Item {
    id: root

    property var workspaceModel: null
    property color borderColor: "#343b45"
    property color accentColor: "#ff7a18"
    property color textColor: "#f4f6f8"
    property color mutedTextColor: "#9ba5b1"
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
        radius: 6
        color: root.level === "error" ? "#2a1719" : root.level === "success" ? "#17271f" : "#171d25"
        border.color: root.level === "error" ? "#7e403d" : root.level === "success" ? "#3f7b58" : root.borderColor

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
