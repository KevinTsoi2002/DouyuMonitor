import QtQuick
import QtQuick.Controls

Rectangle {
    id: root

    property var controller: null
    property var roomModel: null
    property var libraryRooms: []
    property var workspaceModel: null
    property color accentColor: "#ff7a18"
    property color surfaceColor: "#1f242c"
    property color borderColor: "#343b45"
    property color textColor: "#f4f6f8"
    property color mutedTextColor: "#9ba5b1"
    property string viewMode: "current"
    signal addRoomRequested()

    color: surfaceColor
    border.color: borderColor
    border.width: 1
    clip: true

    function submitRoom() {
        const normalized = roomInput.text.trim()
        if (normalized.length > 0 && controller) {
            controller.addRoom(normalized)
            roomInput.clear()
        }
    }

    function requestRoomRemoval(roomId) {
        const controller = root.controller
        const requestedRoomId = String(roomId || "")
        if (!controller || requestedRoomId.length === 0) return
        controller.requestRemoveRoom(requestedRoomId)
    }

    Column {
        anchors.fill: parent
        spacing: 0

        Row {
            x: 12
            width: parent.width - 22
            height: 52
            spacing: 6

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - addButton.width - 16
                color: root.textColor
                elide: Text.ElideRight
                font.bold: true
                font.pixelSize: 15
                text: "房间列表"
            }
            ToolButton {
                id: addButton
                objectName: "quickAddButton"
                anchors.verticalCenter: parent.verticalCenter
                width: 28
                height: 28
                Accessible.name: "添加房间"
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: root.addRoomRequested()
                contentItem: Image { anchors.centerIn: parent; width: 17; height: 17; source: Qt.resolvedUrl("../assets/icons/plus.svg"); opacity: parent.hovered ? 1 : 0.8 }
                background: Rectangle { radius: 4; border.color: root.borderColor; color: parent.hovered ? "#252c34" : "#171d25" }
            }
        }

        Row {
            x: 10
            width: parent.width - 20
            height: 38
            spacing: 5

            TextField {
                id: roomInput
                width: parent.width - addRoomButton.width - 5
                height: 29
                placeholderText: "输入房间号"
                color: root.textColor
                font.pixelSize: 11
                selectByMouse: true
                onAccepted: root.submitRoom()
                background: Rectangle { radius: 4; color: "#10151b"; border.color: roomInput.activeFocus ? root.accentColor : root.borderColor }
            }
            ToolButton {
                id: addRoomButton
                width: 29
                height: 29
                enabled: roomInput.text.trim().length > 0 && root.controller !== null
                Accessible.name: "确认添加房间"
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: root.submitRoom()
                contentItem: Image { anchors.centerIn: parent; width: 16; height: 16; source: Qt.resolvedUrl("../assets/icons/plus.svg"); opacity: parent.enabled ? 1 : 0.35 }
                background: Rectangle { radius: 4; color: parent.hovered && parent.enabled ? "#2a211c" : "#171d25"; border.color: root.borderColor }
            }
        }

        Row {
            x: 10
            width: parent.width - 20
            height: 38
            spacing: 4

            ToolButton {
                height: 27
                width: 52
                checkable: true
                checked: root.viewMode === "current"
                Accessible.name: "当前"
                onClicked: root.viewMode = "current"
                contentItem: Text { text: parent.text; color: parent.checked ? "#ff9b5a" : root.mutedTextColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 10 }
                background: Rectangle { radius: 4; color: parent.checked ? "#241b17" : "transparent"; border.color: parent.checked ? "#6e432d" : "transparent" }
                text: "当前"
            }

            ToolButton {
                objectName: "historyTab"
                height: 27
                width: 52
                checkable: true
                checked: root.viewMode === "history"
                Accessible.name: "历史"
                onClicked: root.viewMode = "history"
                contentItem: Text { text: parent.text; color: parent.checked ? "#ff9b5a" : root.mutedTextColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 10 }
                background: Rectangle { radius: 4; color: parent.checked ? "#241b17" : "transparent"; border.color: parent.checked ? "#6e432d" : "transparent" }
                text: "历史"
            }

            ToolButton {
                height: 27
                width: 52
                checkable: true
                checked: root.viewMode === "favorites"
                Accessible.name: "收藏"
                onClicked: root.viewMode = "favorites"
                contentItem: Text { text: parent.text; color: parent.checked ? "#ff9b5a" : root.mutedTextColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 10 }
                background: Rectangle { radius: 4; color: parent.checked ? "#241b17" : "transparent"; border.color: parent.checked ? "#6e432d" : "transparent" }
                text: "收藏"
            }
        }

        Rectangle { width: parent.width; height: 1; color: root.borderColor }

        Item {
            width: parent.width
            height: parent.height - 200

            ListView {
                id: roomList
                objectName: "roomList"
                anchors.fill: parent
                visible: root.viewMode === "current"
                clip: true
                model: root.roomModel
                spacing: 3
                leftMargin: 8
                rightMargin: 8
                topMargin: 8
                bottomMargin: 8
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                delegate: Item {
                    id: roomRow
                    required property int index
                    required property string roomId
                    required property string anchorName
                    required property string title
                    required property string category
                    required property string viewerLabel
                    required property url avatarUrl
                    required property string liveState
                    required property bool primary
                    required property bool favorite
                    required property bool audioFocused
                    width: roomList.width - roomList.leftMargin - roomList.rightMargin
                    height: 60

                    Rectangle {
                        anchors.fill: parent
                        radius: 5
                        color: roomMouse.containsMouse ? "#252c34" : "transparent"
                        border.color: primary ? "#75462f" : "transparent"
                    }

                    Rectangle {
                    id: avatar
                    x: 6
                    anchors.verticalCenter: parent.verticalCenter
                    width: 32
                    height: 32
                    radius: 16
                    color: primary ? "#3a2820" : "#28313a"
                    Image {
                        id: roomAvatarImage
                        objectName: "roomSidebarAvatarImage"
                        anchors.fill: parent
                        anchors.margins: 1
                        source: roomRow.avatarUrl
                        visible: roomRow.avatarUrl.toString().length > 0 && status !== Image.Error
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        smooth: true
                        clip: true
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: !roomAvatarImage.visible
                        text: roomRow.anchorName.length > 0 ? roomRow.anchorName.slice(0, 1) : roomRow.roomId.slice(0, 1)
                        color: root.textColor
                        font.bold: true
                        font.pixelSize: 12
                    }
                }

                    Column {
                    anchors.left: avatar.right
                    anchors.leftMargin: 7
                    anchors.right: roomActionBar.left
                    anchors.rightMargin: 6
                    y: 9
                    spacing: 3
                    Text { width: parent.width; color: root.textColor; text: roomRow.anchorName.trim().length > 0 ? roomRow.anchorName : roomRow.roomId; elide: Text.ElideRight; font.bold: true; font.pixelSize: 11 }
                    Text { width: parent.width; color: root.mutedTextColor; text: roomRow.title.trim().length > 0 ? roomRow.title : "斗鱼直播间"; elide: Text.ElideRight; font.pixelSize: 9 }
                    Text { width: parent.width; color: roomRow.liveState === "online" ? "#65d391" : roomRow.liveState === "unknown" ? "#c1cad4" : root.mutedTextColor; text: roomRow.liveState === "online" ? "直播中" : roomRow.liveState === "unknown" ? "检查中" : "未开播"; font.pixelSize: 9 }
                }

                    Row {
                    id: roomActionBar
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1
                    ToolButton {
                        width: 23; height: 23
                        Accessible.name: favorite ? "取消收藏" : "收藏"
                        ToolTip.visible: hovered; ToolTip.text: Accessible.name
                        onClicked: if (root.controller) root.controller.setFavorite(roomRow.roomId, !roomRow.favorite)
                        contentItem: Image {
                            objectName: "roomFavoriteIcon"
                            anchors.centerIn: parent
                            width: 15
                            height: 15
                            source: Qt.resolvedUrl("../assets/icons/star.svg")
                            opacity: roomRow.favorite ? 1 : 0.55
                        }
                        background: Rectangle { radius: 3; color: parent.hovered ? "#2a211c" : "transparent" }
                    }
                    ToolButton {
                        objectName: "moveRoomUpButton"
                        width: 23; height: 23
                        enabled: index > 0
                        Accessible.name: "上移"
                        ToolTip.visible: hovered; ToolTip.text: Accessible.name
                        onClicked: if (root.controller) root.controller.moveRoom(roomRow.roomId, -1)
                        contentItem: Image { anchors.centerIn: parent; width: 15; height: 15; source: Qt.resolvedUrl("../assets/icons/chevron-up.svg") }
                        background: Rectangle { radius: 3; color: parent.hovered ? "#252c34" : "transparent" }
                    }
                    ToolButton {
                        objectName: "moveRoomDownButton"
                        width: 23; height: 23
                        enabled: index < roomList.count - 1
                        Accessible.name: "下移"
                        ToolTip.visible: hovered; ToolTip.text: Accessible.name
                        onClicked: if (root.controller) root.controller.moveRoom(roomRow.roomId, 1)
                        contentItem: Image { anchors.centerIn: parent; width: 15; height: 15; source: Qt.resolvedUrl("../assets/icons/chevron-down.svg") }
                        background: Rectangle { radius: 3; color: parent.hovered ? "#252c34" : "transparent" }
                    }
                    ToolButton {
                        width: 23; height: 23
                        Accessible.name: audioFocused ? "关闭声音焦点" : "播放声音"
                        ToolTip.visible: hovered; ToolTip.text: Accessible.name
                        onClicked: if (root.controller) root.controller.setAudioRoom(roomRow.audioFocused ? "" : roomRow.roomId)
                        contentItem: Image { anchors.centerIn: parent; width: 15; height: 15; source: Qt.resolvedUrl("../assets/icons/volume-2.svg"); opacity: audioFocused ? 1 : 0.62 }
                        background: Rectangle { radius: 3; color: parent.hovered ? "#252c34" : "transparent" }
                    }
                    ToolButton {
                        width: 23; height: 23
                        Accessible.name: "移除房间"
                        ToolTip.visible: hovered; ToolTip.text: Accessible.name
                        onClicked: root.requestRoomRemoval(roomRow.roomId)
                        contentItem: Image { anchors.centerIn: parent; width: 15; height: 15; source: Qt.resolvedUrl("../assets/icons/x.svg"); opacity: parent.hovered ? 1 : 0.62 }
                        background: Rectangle { radius: 3; color: parent.hovered ? "#4b1c1c" : "transparent" }
                    }
                }

                    MouseArea { id: roomMouse; anchors.fill: parent; hoverEnabled: true; acceptedButtons: Qt.NoButton }

                }
            }

            RoomLibraryView {
                id: roomLibraryView
                objectName: "roomLibraryView"
                anchors.fill: parent
                visible: root.viewMode === "history" || root.viewMode === "favorites"
                controller: root.controller
                libraryRooms: root.libraryRooms
                favoritesOnly: root.viewMode === "favorites"
                accentColor: root.accentColor
                surfaceColor: root.surfaceColor
                borderColor: root.borderColor
                textColor: root.textColor
                mutedTextColor: root.mutedTextColor
            }
        }
    }
}
