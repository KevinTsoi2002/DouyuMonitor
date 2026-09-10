import QtQuick
import QtQuick.Controls
import ".."

Dialog {
    id: root

    objectName: "groupManagerDialog"
    property var controller: null
    property var workspaceModel: null
    property var roomModel: null
    property var libraryRooms: []
    property string selectedGroupId: ""
    readonly property bool validName: groupNameInput.text.trim().length > 0
                                     && groupNameInput.text.trim().length <= 30
    readonly property var activeGroup: root.selectedGroup()
    readonly property int activeMemberCount: activeGroup ? activeGroup.roomIds.length : 0
    modal: true
    title: "管理分组"
    width: 440
    height: 500
    anchors.centerIn: parent
    palette.window: Theme.controlSurface
    palette.base: Theme.well
    palette.button: Theme.controlSurface
    palette.buttonText: Theme.text
    palette.text: Theme.text
    palette.windowText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.text

    background: Rectangle {
        color: Theme.controlSurface
        border.color: Theme.border
        radius: Theme.radiusLarge
    }

    function selectedGroup() {
        const groups = workspaceModel ? workspaceModel.groups : []
        for (let index = 0; index < groups.length; ++index) {
            if (groups[index].id === selectedGroupId) return groups[index]
        }
        return null
    }

    function roomLabel(roomId) {
        for (let index = 0; index < root.libraryRooms.length; ++index) {
            const room = root.libraryRooms[index]
            if (room.roomId === roomId) {
                return room.anchorName && room.anchorName.length > 0 ? room.anchorName : roomId
            }
        }
        return roomId
    }

    function roomInSelectedGroup(roomId) {
        return root.activeGroup !== null && root.activeGroup.roomIds.indexOf(roomId) >= 0
    }

    contentItem: Column {
        spacing: 10

        Row {
            width: parent.width
            spacing: 6

            TextField {
                id: groupNameInput
                objectName: "groupNameInput"
                width: parent.width - createGroupButton.width - renameGroupButton.width - 12
                placeholderText: "分组名称"
                color: Theme.text
                placeholderTextColor: Theme.mutedText
                selectByMouse: true
            }
            Button {
                id: createGroupButton
                text: "新建"
                enabled: root.validName && root.controller !== null
                onClicked: {
                    root.controller.createGroup(groupNameInput.text.trim())
                    groupNameInput.clear()
                }
            }
            Button {
                id: renameGroupButton
                text: "重命名"
                enabled: root.validName && root.selectedGroupId.length > 0 && root.controller !== null
                onClicked: {
                    root.controller.renameGroup(root.selectedGroupId, groupNameInput.text.trim())
                    groupNameInput.clear()
                }
            }
        }

        ListView {
            id: groupList
            objectName: "groupManagerList"
            width: parent.width
            height: 190
            clip: true
            model: root.workspaceModel ? root.workspaceModel.groups : []
            spacing: 4

            delegate: Rectangle {
                objectName: "groupManagerRow"
                required property var modelData
                width: groupList.width
                height: 42
                color: root.selectedGroupId === modelData.id ? "#2a211c" : "#171d25"
                border.color: root.selectedGroupId === modelData.id ? "#79462f" : "#343b45"
                radius: 4

                Row {
                    z: 2
                    anchors.fill: parent
                    anchors.margins: 7
                    spacing: 6
                    Text { width: parent.width - activateButton.width - deleteButton.width - 12; anchors.verticalCenter: parent.verticalCenter; text: modelData.name; color: "#f4f6f8"; elide: Text.ElideRight }
                    Button {
                        id: activateButton
                        objectName: "activateButton"
                        text: modelData.active ? "当前" : "启用"
                        enabled: root.controller !== null && !modelData.active
                        onClicked: root.controller.setActiveGroup(modelData.id)
                    }
                    Button {
                        id: deleteButton
                        objectName: "deleteButton"
                        text: "删除"
                        enabled: root.controller !== null
                        onClicked: {
                            root.controller.deleteGroup(modelData.id)
                            if (root.selectedGroupId === modelData.id) root.selectedGroupId = ""
                        }
                    }
                }

                MouseArea {
                    objectName: "groupManagerRowMouseArea"
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    x: 7
                    width: parent.width - activateButton.width - deleteButton.width - 26
                    acceptedButtons: Qt.LeftButton
                    onClicked: {
                        root.selectedGroupId = modelData.id
                        groupNameInput.text = modelData.name
                    }
                }
            }
        }

        Text {
            id: groupCapacityLabel
            objectName: "groupCapacityLabel"
            width: parent.width
            color: "#f4f6f8"
            font.bold: true
            font.pixelSize: 11
            text: root.selectedGroupId.length > 0
                  ? "分组成员 " + root.activeMemberCount + "/9"
                  : "请选择分组"
        }

        ListView {
            id: memberList
            objectName: "groupMemberList"
            width: parent.width
            height: 110
            clip: true
            spacing: 3
            model: root.selectedGroup() ? root.selectedGroup().roomIds : []
            visible: root.selectedGroup() !== null

            delegate: Rectangle {
                id: memberRow
                objectName: "groupMemberRow"
                required property string modelData
                required property int index
                property string memberRoomId: modelData
                width: memberList.width
                height: 34
                color: "#171d25"
                border.color: "#343b45"
                radius: 4

                Row {
                    anchors.fill: parent
                    anchors.margins: 5
                    spacing: 4

                    Text {
                        width: parent.width - 84
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#f4f6f8"
                        elide: Text.ElideRight
                        font.pixelSize: 10
                        text: root.roomLabel(memberRow.memberRoomId)
                    }

                    ToolButton {
                        id: moveUpButton
                        objectName: "moveGroupMemberUpButton"
                        width: 22
                        height: 22
                        enabled: index > 0 && root.controller !== null
                        Accessible.name: "上移成员"
                        ToolTip.visible: hovered
                        ToolTip.text: Accessible.name
                        onClicked: root.controller.moveRoomInGroup(root.selectedGroupId,
                                                                    memberRow.memberRoomId,
                                                                    -1)
                        contentItem: Image {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            source: Qt.resolvedUrl("../assets/icons/chevron-up.svg")
                            opacity: parent.enabled ? 1 : 0.35
                        }
                        background: Rectangle { radius: 3; color: parent.hovered ? "#252c34" : "transparent" }
                    }

                    ToolButton {
                        id: moveDownButton
                        objectName: "moveGroupMemberDownButton"
                        width: 22
                        height: 22
                        enabled: index < memberList.count - 1 && root.controller !== null
                        Accessible.name: "下移成员"
                        ToolTip.visible: hovered
                        ToolTip.text: Accessible.name
                        onClicked: root.controller.moveRoomInGroup(root.selectedGroupId,
                                                                    memberRow.memberRoomId,
                                                                    1)
                        contentItem: Image {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            source: Qt.resolvedUrl("../assets/icons/chevron-down.svg")
                            opacity: parent.enabled ? 1 : 0.35
                        }
                        background: Rectangle { radius: 3; color: parent.hovered ? "#252c34" : "transparent" }
                    }

                    ToolButton {
                        id: removeButton
                        objectName: "removeGroupMemberButton"
                        width: 22
                        height: 22
                        enabled: root.controller !== null
                        Accessible.name: "移除成员"
                        ToolTip.visible: hovered
                        ToolTip.text: Accessible.name
                        onClicked: root.controller.removeRoomFromGroup(root.selectedGroupId,
                                                                        memberRow.memberRoomId)
                        contentItem: Image {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            source: Qt.resolvedUrl("../assets/icons/x.svg")
                            opacity: parent.enabled ? 1 : 0.7
                        }
                        background: Rectangle { radius: 3; color: parent.hovered ? "#4b1c1c" : "transparent" }
                    }
                }
            }
        }

        Row {
            width: parent.width
            spacing: 6
            TextField {
                id: roomIdInput
                objectName: "groupRoomIdInput"
                width: parent.width - assignButton.width - 6
                placeholderText: "输入房间号后分配到所选分组"
                color: Theme.text
                placeholderTextColor: Theme.mutedText
                inputMethodHints: Qt.ImhDigitsOnly
                selectByMouse: true
            }
            Button {
                id: assignButton
                text: "分配"
                enabled: /^[0-9]{1,20}$/.test(roomIdInput.text.trim())
                         && root.selectedGroupId.length > 0 && root.controller !== null
                onClicked: {
                    root.controller.assignRoomToGroup(roomIdInput.text.trim(), root.selectedGroupId)
                    roomIdInput.clear()
                }
            }
        }

        Text {
            objectName: "groupManagerStatus"
            width: parent.width
            color: "#9ba5b1"
            font.pixelSize: 10
            text: root.selectedGroupId.length > 0
                  ? "可从收藏与历史记录中选择房间；同一房间可加入多个分组"
                  : "先选择一个分组"
            wrapMode: Text.WordWrap
        }

        ListView {
            id: groupLibraryList
            objectName: "groupLibraryList"
            width: parent.width
            height: 110
            clip: true
            spacing: 3
            model: root.libraryRooms
            visible: root.selectedGroupId.length > 0

            delegate: Rectangle {
                required property var modelData
                width: groupLibraryList.width
                height: 32
                color: "#171d25"
                border.color: "#343b45"
                radius: 4

                Row {
                    anchors.fill: parent
                    anchors.margins: 4
                    spacing: 5
                    Text {
                        width: parent.width - addLibraryRoomButton.width - 8
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#d6dde5"
                        elide: Text.ElideRight
                        font.pixelSize: 10
                        text: (modelData.anchorName && modelData.anchorName.length > 0)
                              ? modelData.anchorName : modelData.roomId
                    }
                    ToolButton {
                        id: addLibraryRoomButton
                        width: 24
                        height: 24
                        enabled: root.controller !== null
                                 && !root.roomInSelectedGroup(modelData.roomId)
                                 && root.activeMemberCount < 9
                        Accessible.name: "添加到分组"
                        ToolTip.visible: hovered
                        ToolTip.text: enabled ? Accessible.name : "已在分组中或分组已满"
                        onClicked: {
                            const message = root.controller.assignRoomToGroup(modelData.roomId,
                                                                                root.selectedGroupId)
                            if (message && root.workspaceModel) {
                                root.workspaceModel.setLastMessage(message, "error", 2400)
                            }
                        }
                        contentItem: Image {
                            anchors.centerIn: parent
                            width: 14
                            height: 14
                            source: Qt.resolvedUrl("../assets/icons/plus.svg")
                            opacity: parent.enabled ? 1 : 0.35
                        }
                        background: Rectangle { radius: 3; color: parent.hovered ? "#2a211c" : "transparent" }
                    }
                }
            }
        }
    }

    footer: Rectangle {
        objectName: "groupManagerDialogFooter"
        implicitHeight: 56
        color: Theme.controlSurface
        border.color: Theme.border
        Button {
            anchors.right: parent.right
            anchors.rightMargin: 0
            anchors.verticalCenter: parent.verticalCenter
            width: 92
            height: 34
            text: "关闭"
            onClicked: root.close()
            contentItem: Text { text: parent.text; color: Theme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 12 }
            background: Rectangle { radius: Theme.radiusSmall; color: parent.down ? Theme.borderStrong : (parent.hovered ? Theme.border : Theme.well); border.color: Theme.borderStrong }
        }
    }
}
