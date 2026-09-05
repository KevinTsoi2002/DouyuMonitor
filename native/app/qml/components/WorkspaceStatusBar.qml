import QtQuick
import ".."

Rectangle {
    id: root

    objectName: "workspaceStatusBar"
    property int onlineCount: 0
    property int offlineCount: 0
    property int danmakuCount: 0
    property string audioLabel: "无"
    property bool silentChecking: true

    height: 28
    radius: Theme.radiusSmall
    color: Theme.controlSurface
    border.color: Theme.border
    clip: true

    Row {
        anchors.fill: parent
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        spacing: 14

        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.online
            font.pixelSize: 10
            text: root.onlineCount + " 直播中"
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.mutedText
            font.pixelSize: 10
            text: root.offlineCount + " 未开播"
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.mutedText
            font.pixelSize: 10
            text: "弹幕 " + root.danmakuCount + " 已连接"
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.mutedText
            font.pixelSize: 10
            text: "声音焦点：" + root.audioLabel
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.mutedText
            font.pixelSize: 10
            text: root.silentChecking ? "静默检查" : "检查中"
        }
    }
}
