import QtQuick
import QtQuick.Controls
import ".."

Item {
    id: root

    property var member: null
    property string roomStatus: ""
    property bool active: false
    property bool canAdd: false
    signal addRequested(string memberId)
    signal roomIdSubmitted(string memberId, string roomId)
    signal hoverEntered(string memberId)
    signal hoverExited(string memberId)
    readonly property string memberId: member ? String(member.id || "") : ""
    readonly property string anchorName: member ? String(member.anchorName || memberId) : ""
    readonly property string roomId: member ? String(member.roomId || "") : ""
    readonly property bool resolved: roomId.length > 0
    readonly property bool confirming: roomEdit.visible

    height: confirming ? 84 : 46

    HoverHandler {
        id: memberHover
        onHoveredChanged: {
            if (hovered) root.hoverEntered(root.memberId)
            else root.hoverExited(root.memberId)
        }
    }

    function beginConfirmation()
    {
        if (root.resolved) return
        roomEdit.text = ""
        roomEdit.visible = true
        roomEdit.forceActiveFocus()
    }

    function submitConfirmation()
    {
        const value = roomEdit.text.trim()
        if (value.length === 0) return
        root.roomIdSubmitted(root.memberId, value)
        roomEdit.visible = false
    }

    Row {
        id: memberLine
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 46
        spacing: 6

        Rectangle {
            id: avatar
            objectName: "guildMemberAvatar"
            anchors.verticalCenter: parent.verticalCenter
            width: 30
            height: 30
            radius: 15
            color: Theme.well

            Image {
                id: avatarImage
                objectName: "guildMemberAvatarImage"
                anchors.fill: parent
                anchors.margins: 1
                source: root.member ? String(root.member.avatarUrl || "") : ""
                visible: source.toString().length > 0 && status !== Image.Error
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                sourceSize: Qt.size(64, 64)
                smooth: true
                clip: true
            }

            Text {
                objectName: "guildMemberAvatarFallback"
                anchors.centerIn: parent
                visible: !avatarImage.visible
                color: Theme.text
                font.bold: true
                font.pixelSize: 11
                text: root.anchorName.length > 0 ? root.anchorName.slice(0, 1)
                                                 : root.memberId.slice(0, 1)
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(80, parent.width - avatar.width - liveState.width - statusLabel.width
                           - addButton.width - 30)
            spacing: 1

            Text {
                objectName: "guildMemberName"
                width: parent.width
                color: Theme.text
                elide: Text.ElideRight
                font.pixelSize: 11
                text: root.anchorName
            }

            ToolButton {
                objectName: "guildMemberRoomLabel"
                width: parent.width
                height: 16
                enabled: !root.resolved
                text: root.resolved ? root.roomId : "待确认"
                padding: 0
                onClicked: root.beginConfirmation()
                contentItem: Text {
                    text: parent.text
                    color: root.resolved ? Theme.mutedText : Theme.warning
                    elide: Text.ElideRight
                    font.pixelSize: 9
                    verticalAlignment: Text.AlignVCenter
                }
                background: Item {}
            }
        }

        Row {
            id: liveState
            objectName: "guildMemberLiveState"
            anchors.verticalCenter: parent.verticalCenter
            width: 46
            spacing: 4

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 5
                height: 5
                radius: 3
                color: root.member && root.member.liveState === "online"
                       ? Theme.online
                       : (root.member && root.member.liveState === "offline"
                          ? Theme.mutedText : Theme.warning)
            }

            Text {
                objectName: "guildMemberLiveStateLabel"
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 9
                color: root.member && root.member.liveState === "online"
                       ? Theme.online
                       : (root.member && root.member.liveState === "offline"
                          ? Theme.mutedText : Theme.warning)
                elide: Text.ElideRight
                font.pixelSize: 9
                text: root.member && root.member.liveState === "online"
                      ? "直播中"
                      : (root.member && root.member.liveState === "offline"
                         ? "未开播" : "检查中")
            }
        }

        Text {
            id: statusLabel
            objectName: "guildMemberStatus"
            anchors.verticalCenter: parent.verticalCenter
            width: root.active ? 36 : 0
            visible: root.active
            color: Theme.online
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            font.pixelSize: 9
            text: "已添加"
        }

        ToolButton {
            id: addButton
            objectName: "guildQuickAddButton"
            anchors.verticalCenter: parent.verticalCenter
            width: 26
            height: 26
            enabled: root.canAdd
            Accessible.name: root.active ? "已加入房间列表" : (root.resolved ? "加入房间列表" : "请先确认房间号")
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name
            onClicked: root.addRequested(root.memberId)
            contentItem: Image {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: Qt.resolvedUrl("../assets/icons/plus.svg")
                opacity: parent.enabled ? 1 : 0.28
            }
            background: Rectangle {
                radius: 4
                color: parent.hovered && parent.enabled ? Theme.controlSurface : "transparent"
                border.color: parent.enabled ? Theme.border : "transparent"
            }
        }
    }
    TextField {
        id: roomEdit
        objectName: "guildMemberRoomInput"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 48
        height: 0
        visible: false
        placeholderText: "输入房间号"
        color: Theme.text
        placeholderTextColor: Theme.mutedText
        selectByMouse: true
        font.pixelSize: 10
        onAccepted: root.submitConfirmation()
        background: Rectangle {
            radius: 4
            color: Theme.well
            border.color: roomEdit.activeFocus ? Theme.accent : Theme.border
        }
        states: State {
            name: "visible"
            when: roomEdit.visible
            PropertyChanges { target: roomEdit; height: 30 }
        }
    }
}
