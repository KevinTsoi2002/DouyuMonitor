import QtQuick
import QtQuick.Controls

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
    modal: true
    title: "管理分组"
    width: 440
    height: 500
    anchors.centerIn: parent

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

    contentItem: Column {
        spacing: 10

        Row {
            width: parent.width
            spacing: 6

            TextField {
                id: groupNameInput
                width: parent.width - createGroupButton.width - renameGroupButton.width - 12
                placeholderText: "分组名称"
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
            width: parent.width
            height: 190
            clip: true
            model: root.workspaceModel ? root.workspaceModel.groups : []
            spacing: 4

            delegate: Rectangle {
                required property var modelData
                width: groupList.width
                height: 42
                color: root.selectedGroupId === modelData.id ? "#2a211c" : "#171d25"
                border.color: root.selectedGroupId === modelData.id ? "#79462f" : "#343b45"
                radius: 4

                Row {
                    anchors.fill: parent
                    anchors.margins: 7
                    spacing: 6
                    Text { width: parent.width - activateButton.width - deleteButton.width - 12; anchors.verticalCenter: parent.verticalCenter; text: modelData.name; color: "#f4f6f8"; elide: Text.ElideRight }
                    Button {
                        id: activateButton
                        text: modelData.active ? "当前" : "启用"
                        enabled: root.controller !== null && !modelData.active
                        onClicked: root.controller.setActiveGroup(modelData.id)
                    }
                    Button {
                        id: deleteButton
                        text: "删除"
                        enabled: root.controller !== null
                        onClicked: {
                            root.controller.deleteGroup(modelData.id)
                            if (root.selectedGroupId === modelData.id) root.selectedGroupId = ""
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.LeftButton
                    onClicked: {
                        root.selectedGroupId = modelData.id
                        groupNameInput.text = modelData.name
                    }
                }
            }
        }

        Text {
            width: parent.width
            color: "#f4f6f8"
            font.bold: true
            font.pixelSize: 11
            text: root.selectedGroupId.length > 0 ? "分组成员" : "请选择分组"
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
                width: parent.width - assignButton.width - 6
                placeholderText: "输入房间号后分配到所选分组"
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
    }

    footer: DialogButtonBox {
        Button {
            text: "关闭"
            onClicked: root.close()
        }
    }
}
