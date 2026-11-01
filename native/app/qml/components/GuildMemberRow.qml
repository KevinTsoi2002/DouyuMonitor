import QtQuick
import QtQuick.Controls.Basic
import ".."
import "../controls"

Item {
    id: root

    property var member: null
    property string roomStatus: ""
    property bool active: false
    property bool canAdd: false
    property bool selected: false
    property var confirmRoomId: null
    signal selectedRequested(string memberId)
    signal addRequested(string memberId)
    signal roomIdSubmitted(string memberId, string roomId)
    readonly property string memberId: member ? String(member.id || "") : ""
    readonly property string anchorName: member ? String(member.anchorName || memberId) : ""
    readonly property string roomId: member ? String(member.roomId || "") : ""
    readonly property var avatarCandidates: {
        const primary = member ? String(member.avatarUrl || "") : ""
        const fallback = member && member.portraitIdentityVerified === true
                          ? String(member.fallbackAvatarUrl || "") : ""
        return [primary, fallback].filter(function(source, index, sources) {
            return source.length > 0 && sources.indexOf(source) === index
        })
    }
    readonly property string avatarCandidateKey: JSON.stringify(root.avatarCandidates)
    property int avatarCandidateIndex: 0
    readonly property bool resolved: roomId.length > 0
    readonly property bool confirming: roomEdit.visible
    readonly property bool hovered: memberHover.hovered
    onAvatarCandidateKeyChanged: avatarCandidateIndex = 0
    onResolvedChanged: if (resolved) roomEdit.visible = false

    height: confirming ? 104 : 64

    HoverHandler {
        id: memberHover

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
        if (root.resolved) return
        const value = roomEdit.text.trim()
        if (value.length === 0) return
        if (root.confirmRoomId && !root.confirmRoomId(root.memberId, value)) return
        root.roomIdSubmitted(root.memberId, value)
        roomEdit.visible = false
    }

    Row {
        id: memberLine
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 64
        spacing: 12

        Rectangle {
            id: avatar
            objectName: "guildMemberAvatar"
            anchors.verticalCenter: parent.verticalCenter
            width: 40
            height: 40
            radius: Theme.radiusSmall
            color: Theme.well

            Image {
                id: avatarImage
                objectName: "guildMemberAvatarImage"
                anchors.fill: parent
                anchors.margins: 1
                source: root.avatarCandidates[root.avatarCandidateIndex] || ""
                visible: source.toString().length > 0 && status !== Image.Error
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                sourceSize: Qt.size(64, 64)
                smooth: true
                clip: true
                onStatusChanged: {
                    if (status === Image.Error && root.avatarCandidateIndex + 1 < root.avatarCandidates.length)
                        root.avatarCandidateIndex += 1
                }
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
                           - addButton.width - 48)
            spacing: 4

            ToolButton {
                id: memberNameButton
                objectName: "guildMemberName"
                width: parent.width
                height: 24
                padding: 0
                flat: true
                hoverEnabled: true
                Accessible.name: qsTr("查看 %1 的资料").arg(root.anchorName)
                onClicked: root.selectedRequested(root.memberId)
                text: root.anchorName
                contentItem: Text { text: memberNameButton.text; color: Theme.text; elide: Text.ElideRight; font.pixelSize: Theme.fontBody; font.bold: true; verticalAlignment: Text.AlignVCenter }
                background: Rectangle {
                    objectName: "guildMemberNameBackground"
                    color: "transparent"
                    radius: Theme.radiusSmall
                    border.width: memberNameButton.visualFocus ? Theme.focusBorderWidth : 0
                    border.color: Theme.accent
                }
            }

            ConsoleToolButton {
                id: roomIdentityButton
                objectName: "guildMemberRoomLabel"
                width: parent.width
                height: 20
                enabled: !root.resolved
                text: root.resolved ? root.roomId : "待确认"
                padding: 0
                flat: true
                onClicked: root.beginConfirmation()
                contentItem: Text {
                    text: roomIdentityButton.text
                    color: root.resolved ? Theme.mutedText : Theme.warning
                    elide: Text.ElideRight
                    font.pixelSize: Theme.fontControl
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        Row {
            id: liveState
            objectName: "guildMemberLiveState"
            anchors.verticalCenter: parent.verticalCenter
            width: 70
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
                font.pixelSize: Theme.fontControl
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
            width: root.active ? 56 : 0
            visible: root.active
            color: Theme.online
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideRight
            font.pixelSize: Theme.fontControl
            text: "已添加"
        }

        ConsoleToolButton {
            id: addButton
            objectName: "guildQuickAddButton"
            anchors.verticalCenter: parent.verticalCenter
            width: 40
            height: 32
            enabled: root.canAdd
            flat: true
            Accessible.name: root.active ? "已加入房间列表" : (root.resolved ? "加入房间列表" : "请先确认房间号")
            ToolTip.visible: false
            ToolTip.text: Accessible.name
            onClicked: root.addRequested(root.memberId)
            contentItem: ThemeIcon {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: Qt.resolvedUrl("../assets/icons/plus.svg")
                opacity: parent.enabled ? 1 : 0.28
            }
        }
    }
    ConsoleTextField {
        id: roomEdit
        objectName: "guildMemberRoomInput"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 66
        height: 0
        visible: false
        placeholderText: "输入房间号"
        selectByMouse: true
        maximumLength: 20
        validator: RegularExpressionValidator { regularExpression: /[0-9]{1,20}/ }
        font.pixelSize: Theme.fontControl
        onAccepted: root.submitConfirmation()
        states: State {
            name: "visible"
            when: roomEdit.visible
            PropertyChanges { roomEdit.height: 30 }
        }
    }
}
