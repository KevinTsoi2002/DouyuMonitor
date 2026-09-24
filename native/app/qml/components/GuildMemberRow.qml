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
    readonly property string memberId: member ? String(member.id || "") : ""
    readonly property string anchorName: member ? String(member.anchorName || memberId) : ""
    readonly property string roomId: member ? String(member.roomId || "") : ""
    readonly property bool resolved: roomId.length > 0
    readonly property bool confirming: roomEdit.visible

    height: confirming ? 76 : 38

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
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 38
        spacing: 6

        Column {
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(80, parent.width - statusLabel.width - addButton.width - 18)
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
                height: 15
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

        Text {
            id: statusLabel
            objectName: "guildMemberStatus"
            anchors.verticalCenter: parent.verticalCenter
            width: 44
            color: root.active ? Theme.online : Theme.mutedText
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            font.pixelSize: 9
            text: root.active ? "已添加" : ""
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
        anchors.topMargin: 40
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