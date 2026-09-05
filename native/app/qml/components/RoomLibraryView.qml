import QtQuick
import QtQuick.Controls
import ".."

Item {
    id: root

    property var controller: null
    property var libraryRooms: []
    property bool favoritesOnly: false
    property var visibleRooms: []
    property color accentColor: Theme.accent
    property color surfaceColor: Theme.managementSurface
    property color borderColor: Theme.border
    property color textColor: Theme.text
    property color mutedTextColor: Theme.mutedText

    function openedLabel(timestamp) {
        if (!timestamp || timestamp <= 0) return "未打开"
        return Qt.formatDateTime(new Date(timestamp), "MM-dd hh:mm")
    }

    function rebuildVisibleRooms() {
        const source = root.libraryRooms || []
        const result = []
        for (let index = 0; index < source.length; ++index) {
            const entry = source[index]
            if (root.favoritesOnly ? entry.favorite : entry.lastOpenedAtMs > 0) result.push(entry)
        }
        root.visibleRooms = result
    }

    onLibraryRoomsChanged: rebuildVisibleRooms()
    onFavoritesOnlyChanged: rebuildVisibleRooms()
    Component.onCompleted: rebuildVisibleRooms()

    ListView {
        id: historyList
        anchors.fill: parent
        clip: true
        model: root.visibleRooms
        spacing: 3
        leftMargin: 8
        rightMargin: 8
        topMargin: 8
        bottomMargin: 8
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        delegate: Item {
            id: roomRow
            required property var modelData

            readonly property var entry: modelData
            width: historyList.width - historyList.leftMargin - historyList.rightMargin
            height: Theme.roomRowHeight

            Rectangle {
                anchors.fill: parent
                radius: Theme.radiusSmall
                color: entry.active
                       ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.08)
                       : (historyMouse.containsMouse ? Theme.controlSurface : "transparent")
                border.color: entry.active
                              ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.6)
                              : "transparent"
                border.width: entry.active ? 1 : 0
            }

            Rectangle {
                id: avatar
                x: 6
                anchors.verticalCenter: parent.verticalCenter
                width: 32
                height: 32
                radius: 16
                color: entry.active ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.18) : Theme.well

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
                anchors.left: avatar.right
                anchors.leftMargin: 7
                anchors.right: roomActionBar.left
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Text {
                    objectName: "libraryRoomTitle"
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
                Row {
                    spacing: 4

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 5
                        height: 5
                        radius: 3
                        color: entry.active ? Theme.online : Theme.mutedText
                    }
                    Text {
                        color: entry.active ? Theme.online : root.mutedTextColor
                        font.pixelSize: 9
                        text: entry.active ? "当前播放" : root.openedLabel(entry.lastOpenedAtMs)
                    }
                }
            }

            Item {
                id: roomActionBar
                objectName: "libraryRoomActionBar"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 5
                width: root.favoritesOnly ? 27 : 58
                height: 27

                ToolButton {
                    objectName: "openHistoryRoomButton"
                    anchors.right: parent.right
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
                        radius: Theme.radiusSmall
                        color: parent.hovered && parent.enabled ? "#2a211c" : "transparent"
                    }
                }

                ToolButton {
                    objectName: "removeHistoryButton"
                    anchors.right: parent.right
                    anchors.rightMargin: 31
                    anchors.verticalCenter: parent.verticalCenter
                    width: 27
                    height: 27
                    visible: !root.favoritesOnly
                    enabled: !entry.active && entry.lastOpenedAtMs > 0 && root.controller !== null
                    Accessible.name: "删除历史记录"
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: root.controller.removeHistoryRoom(entry.roomId)
                    contentItem: Image { anchors.centerIn: parent; width: 16; height: 16; source: Qt.resolvedUrl("../assets/icons/x.svg"); opacity: parent.enabled ? 1 : 0.62 }
                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: parent.hovered && parent.enabled ? "#4b1c1c" : "transparent"
                    }
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
