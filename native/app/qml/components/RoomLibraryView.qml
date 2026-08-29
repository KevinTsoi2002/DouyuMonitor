import QtQuick
import QtQuick.Controls

Item {
    id: root

    property var controller: null
    property var libraryRooms: []
    property bool favoritesOnly: false
    property color accentColor: "#ff7a18"
    property color surfaceColor: "#1f242c"
    property color borderColor: "#343b45"
    property color textColor: "#f4f6f8"
    property color mutedTextColor: "#9ba5b1"

    function openedLabel(timestamp) {
        if (!timestamp || timestamp <= 0) return "未打开"
        return Qt.formatDateTime(new Date(timestamp), "MM-dd hh:mm")
    }

    ListView {
        id: historyList
        anchors.fill: parent
        clip: true
        model: root.libraryRooms
        spacing: 3
        leftMargin: 8
        rightMargin: 8
        topMargin: 8
        bottomMargin: 8
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        delegate: Item {
            required property var modelData

            readonly property var entry: modelData
            width: historyList.width - historyList.leftMargin - historyList.rightMargin
            height: root.favoritesOnly && !entry.favorite ? 0 : 60
            visible: !root.favoritesOnly || entry.favorite

            Rectangle {
                anchors.fill: parent
                radius: 5
                color: historyMouse.containsMouse ? "#252c34" : "transparent"
                border.color: entry.active ? "#75462f" : "transparent"
            }

            Rectangle {
                id: avatar
                x: 6
                anchors.verticalCenter: parent.verticalCenter
                width: 32
                height: 32
                radius: 16
                color: entry.active ? "#3a2820" : "#28313a"

                Image {
                    id: roomAvatarImage
                    objectName: "roomHistoryAvatarImage"
                    anchors.fill: parent
                    anchors.margins: 1
                    source: entry.avatarUrl
                    visible: entry.avatarUrl && entry.avatarUrl.toString().length > 0
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    smooth: true
                    clip: true
                }

                Text {
                    anchors.centerIn: parent
                    visible: !roomAvatarImage.visible
                    color: root.textColor
                    font.bold: true
                    font.pixelSize: 12
                    text: entry.anchorName.length > 0 ? entry.anchorName.slice(0, 1) : entry.roomId.slice(0, 1)
                }
            }

            Column {
                x: avatar.x + avatar.width + 7
                y: 9
                width: parent.width - x - 52
                spacing: 3

                Text {
                    width: parent.width
                    color: root.textColor
                    elide: Text.ElideRight
                    font.bold: true
                    font.pixelSize: 11
                    text: entry.anchorName.length > 0 ? entry.anchorName : entry.roomId
                }
                Text {
                    width: parent.width
                    color: root.mutedTextColor
                    elide: Text.ElideRight
                    font.pixelSize: 9
                    text: entry.title.length > 0 ? entry.title : entry.roomId
                }
                Text {
                    color: entry.active ? "#65d391" : root.mutedTextColor
                    font.pixelSize: 9
                    text: entry.active ? "当前播放" : root.openedLabel(entry.lastOpenedAtMs)
                }
            }

            ToolButton {
                anchors.right: parent.right
                anchors.rightMargin: 5
                anchors.verticalCenter: parent.verticalCenter
                width: 27
                height: 27
                enabled: !entry.active && root.controller !== null
                Accessible.name: "打开房间"
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: root.controller.addRoom(entry.roomId)
                contentItem: Image { anchors.centerIn: parent; width: 16; height: 16; source: Qt.resolvedUrl("../assets/icons/plus.svg"); opacity: parent.enabled ? 1 : 0.35 }
                background: Rectangle {
                    radius: 3
                    color: parent.hovered && parent.enabled ? "#2a211c" : "transparent"
                }
            }

            MouseArea {
                id: historyMouse
                anchors.fill: parent
                hoverEnabled: true
                acceptedButtons: Qt.NoButton
            }
        }

        Text {
            anchors.centerIn: parent
            visible: historyList.count === 0
            color: root.mutedTextColor
            font.pixelSize: 11
            text: root.favoritesOnly ? "暂无收藏" : "暂无历史记录"
        }
    }
}
