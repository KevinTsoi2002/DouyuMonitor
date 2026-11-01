import QtQuick
import QtQuick.Controls
import ".."
import "../controls"

Rectangle {
    id: root

    property var controller: null
    property var roomModel: null
    property var libraryRooms: []
    property var workspaceModel: null
    property color accentColor: Theme.accent
    property color surfaceColor: Theme.managementSurface
    property color borderColor: Theme.border
    property color textColor: Theme.text
    property color mutedTextColor: Theme.mutedText
    property string viewMode: "current"
    signal addRoomRequested()
    signal settingsRequested()
    signal closeRequested()

    color: surfaceColor
    border.color: borderColor
    border.width: 1
    clip: true

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
                width: parent.width - addButton.width - closeButton.width - 12
                color: root.textColor
                elide: Text.ElideRight
                font.bold: true
                font.pixelSize: 15
                text: "房间列表"
            }
            ConsoleButton {
                id: addButton
                objectName: "quickAddButton"
                anchors.verticalCenter: parent.verticalCenter
                width: 104
                height: Theme.controlHeight
                text: qsTr("添加房间")
                Accessible.name: text
                icon.source: "../assets/icons/plus.svg"
                onClicked: root.addRoomRequested()
            }
            ConsoleToolButton {
                id: closeButton
                objectName: "closeRoomToolsButton"
                anchors.verticalCenter: parent.verticalCenter
                width: 32
                height: 32
                icon.source: "../assets/icons/x.svg"
                Accessible.name: qsTr("关闭房间工具")
                onClicked: root.closeRequested()
            }
        }

        Row {
            x: 10
            width: parent.width - 20
            height: 38
            spacing: 4

            ConsoleToolButton {
                height: Theme.controlHeight
                width: 52
                checkable: true
                checked: root.viewMode === "current"
                Accessible.name: "当前"
                onClicked: root.viewMode = "current"
                contentItem: Text { text: parent.text; color: parent.checked ? root.accentColor : root.mutedTextColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: Theme.fontControl }
                text: "当前"
            }

            ConsoleToolButton {
                objectName: "historyTab"
                height: Theme.controlHeight
                width: 52
                checkable: true
                checked: root.viewMode === "history"
                Accessible.name: "历史"
                onClicked: root.viewMode = "history"
                contentItem: Text { text: parent.text; color: parent.checked ? root.accentColor : root.mutedTextColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: Theme.fontControl }
                text: "历史"
            }

            ConsoleToolButton {
                objectName: "favoritesTab"
                height: Theme.controlHeight
                width: 52
                checkable: true
                checked: root.viewMode === "favorites"
                Accessible.name: "收藏"
                onClicked: root.viewMode = "favorites"
                contentItem: Text { text: parent.text; color: parent.checked ? root.accentColor : root.mutedTextColor; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: Theme.fontControl }
                text: "收藏"
            }
        }

        Rectangle { width: parent.width; height: 1; color: root.borderColor }

        Item {
            width: parent.width
            height: Math.max(0, parent.height - 134)

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
                    required property bool muted
                    width: roomList.width - roomList.leftMargin - roomList.rightMargin
                    height: Theme.roomRowHeight

                    HoverHandler { id: rowHover }

                    Rectangle {
                        id: roomSidebarSelectionSurface
                        objectName: "roomSidebarSelectionSurface"
                        anchors.fill: parent
                        radius: Theme.radiusSmall
                        color: primary
                               ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.08)
                               : (rowHover.hovered ? Theme.controlSurface : "transparent")
                        border.color: primary
                                      ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.28)
                                      : "transparent"
                        border.width: primary ? 1 : 0
                        Behavior on color { ColorAnimation { duration: Theme.motionNormal } }
                        Behavior on border.color { ColorAnimation { duration: Theme.motionNormal } }
                        Rectangle {
                            objectName: "roomSidebarSelectionIndicator"
                            width: 3
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            radius: 2
                            color: root.accentColor
                            opacity: primary ? 1 : 0
                            Behavior on opacity { NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic } }
                        }
                    }

                    Rectangle {
                        id: avatar
                        x: 6
                        anchors.verticalCenter: parent.verticalCenter
                        width: 32
                        height: 32
                        radius: 16
                        color: primary ? Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.18) : Theme.well
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
                    sourceSize: Qt.size(64, 64)
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
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2

                        Text {
                            objectName: "sidebarRoomAnchorName"
                            width: parent.width
                            color: root.textColor
                            text: roomRow.anchorName.trim().length > 0 ? roomRow.anchorName : roomRow.roomId
                            elide: Text.ElideRight
                            font.bold: true
                            font.pixelSize: Theme.fontControl
                        }
                        Text {
                            objectName: "sidebarRoomTitle"
                            width: parent.width
                            color: root.mutedTextColor
                            text: roomRow.title.trim().length > 0 ? roomRow.title : "斗鱼直播间"
                            elide: Text.ElideRight
                            font.pixelSize: Theme.fontCaption
                        }
                        Row {
                            spacing: 4

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 5
                                height: 5
                                radius: 3
                                color: roomRow.liveState === "online"
                                       ? Theme.online
                                       : (roomRow.liveState === "unknown" ? Theme.warning : Theme.mutedText)
                            }
                            Text {
                                color: roomRow.liveState === "online"
                                       ? Theme.online
                                       : (roomRow.liveState === "unknown" ? Theme.warning : root.mutedTextColor)
                                text: roomRow.liveState === "online"
                                      ? "直播中"
                                      : (roomRow.liveState === "unknown" ? "检查中" : "未开播")
                                font.pixelSize: Theme.fontCaption
                            }
                        }
                    }

                    Row {
                        id: roomActionBar
                        objectName: "sidebarRoomActionBar"
                        anchors.right: parent.right
                        anchors.rightMargin: 4
                        anchors.verticalCenter: parent.verticalCenter
                        width: 139
                        height: 27
                        spacing: 1
                        ConsoleToolButton {
                        id: roomFavoriteButton
                        objectName: "roomFavoriteButton"
                        width: 27; height: 27
                        padding: (27 - Theme.iconSize) / 2
                        Accessible.name: favorite ? "取消收藏" : "收藏"
                        ToolTip.visible: false; ToolTip.text: Accessible.name
                        onClicked: if (root.controller) root.controller.setFavorite(roomRow.roomId, !roomRow.favorite)
                        contentItem: ThemeIcon {
                            objectName: "roomFavoriteIcon"
                            anchors.centerIn: parent
                            width: Theme.iconSize
                            height: Theme.iconSize
                            source: Qt.resolvedUrl("../assets/icons/star.svg")
                            opacity: roomRow.favorite ? 1 : 0.55
                        }
                    }
                    ConsoleToolButton {
                        objectName: "moveRoomUpButton"
                        width: 27; height: 27
                        padding: (27 - Theme.iconSize) / 2
                        enabled: index > 0
                        Accessible.name: "上移"
                        ToolTip.visible: false; ToolTip.text: Accessible.name
                        onClicked: if (root.controller) root.controller.moveRoom(roomRow.roomId, -1)
                        contentItem: ThemeIcon { anchors.centerIn: parent; width: Theme.iconSize; height: Theme.iconSize; source: Qt.resolvedUrl("../assets/icons/chevron-up.svg") }
                    }
                    ConsoleToolButton {
                        objectName: "moveRoomDownButton"
                        width: 27; height: 27
                        padding: (27 - Theme.iconSize) / 2
                        enabled: index < roomList.count - 1
                        Accessible.name: "下移"
                        ToolTip.visible: false; ToolTip.text: Accessible.name
                        onClicked: if (root.controller) root.controller.moveRoom(roomRow.roomId, 1)
                        contentItem: ThemeIcon { anchors.centerIn: parent; width: Theme.iconSize; height: Theme.iconSize; source: Qt.resolvedUrl("../assets/icons/chevron-down.svg") }
                    }
                    ConsoleToolButton {
                        objectName: "roomSidebarAudioButton"
                        width: 27; height: 27
                        padding: (27 - Theme.iconSize) / 2
                        Accessible.name: root.workspaceModel && root.workspaceModel.audioMode === "multi"
                                         ? (audioFocused ? "静音直播间" : "取消直播间静音")
                                         : (audioFocused ? "关闭声音焦点" : "播放声音")
                        ToolTip.visible: false; ToolTip.text: Accessible.name
                        onClicked: {
                            if (!root.controller) return
                            if (root.workspaceModel && root.workspaceModel.audioMode === "multi") {
                                root.controller.setRoomMuted(roomRow.roomId, !roomRow.muted)
                            } else {
                                const error = root.controller.setAudioRoom(roomRow.audioFocused ? "" : roomRow.roomId)
                                if (error && root.workspaceModel) {
                                    root.workspaceModel.setLastMessage(error, "error", 3200)
                                }
                            }
                        }
                        contentItem: ThemeIcon { anchors.centerIn: parent; width: Theme.iconSize; height: Theme.iconSize; source: Qt.resolvedUrl("../assets/icons/volume-2.svg"); opacity: audioFocused ? 1 : 0.62 }
                    }
                    ConsoleToolButton {
                        objectName: "roomRemoveButton"
                        width: 27; height: 27
                        padding: (27 - Theme.iconSize) / 2
                        Accessible.name: "移除房间"
                        ToolTip.visible: false; ToolTip.text: Accessible.name
                        onClicked: root.requestRoomRemoval(roomRow.roomId)
                        contentItem: ThemeIcon { anchors.centerIn: parent; width: Theme.iconSize; height: Theme.iconSize; source: Qt.resolvedUrl("../assets/icons/x.svg"); opacity: parent.hovered ? 1 : 0.62 }
                        destructive: true
                        }
                    }

                    MouseArea {
                        id: roomMouse
                        objectName: "roomSidebarHoverArea"
                        anchors.left: parent.left
                        anchors.right: roomActionBar.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                    }

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

        Rectangle {
            width: parent.width
            height: 1
            color: root.borderColor
        }

        ConsoleToolButton {
            id: settingsButton
            objectName: "settingsButton"
            width: parent.width - 20
            height: 42
            anchors.horizontalCenter: parent.horizontalCenter
            Accessible.name: "设置"
            ToolTip.visible: false
            ToolTip.text: Accessible.name
            onClicked: root.settingsRequested()
            contentItem: Row {
                spacing: 8
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                ThemeIcon {
                    width: 16
                    height: 16
                    anchors.verticalCenter: parent.verticalCenter
                    source: Qt.resolvedUrl("../assets/icons/settings.svg")
                    tint: root.textColor
                    sourceSize: Qt.size(Theme.iconSize, Theme.iconSize)
                    opacity: settingsButton.hovered ? 1 : 0.8
                }
                Text {
                    text: "设置"
                    color: root.textColor
                    font.pixelSize: Theme.fontControl
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
            background: Rectangle {
                radius: Theme.radiusSmall
                color: settingsButton.hovered ? Theme.controlSurface : "transparent"
            }
        }
    }
}
