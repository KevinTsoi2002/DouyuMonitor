import QtQuick
import QtQuick.Controls.Basic
import ".."

Item {
    id: root

    objectName: "settingsPage"
    property var controller: null
    signal backRequested()
    signal teamManagerRequested()

    readonly property string closeBehavior: controller ? controller.closeBehavior : "ask"
    property var eventMapping: ({ version: 1, event: "", members: [] })
    property string mappingError: ""
    function loadEventMapping() {
        if (!root.controller || !root.controller.eventMappingJson) return
        root.eventMapping = JSON.parse(root.controller.eventMappingJson)
    }
    onControllerChanged: root.loadEventMapping()
    Component.onCompleted: root.loadEventMapping()
    onVisibleChanged: if (visible) root.loadEventMapping()
    function chooseCloseBehavior(behavior) {
        if (!controller) return
        if (behavior === "ask") controller.clearCloseBehavior()
        else controller.setCloseBehavior(behavior, true)
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.canvas
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 24
        contentWidth: width
        contentHeight: settingsContent.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: settingsContent
            width: Math.min(parent.width, 780)
            spacing: 18

            Row {
                width: parent.width
                height: 36
                spacing: 10

                ToolButton {
                    id: settingsBackButton
                    objectName: "settingsBackButton"
                    width: Theme.controlHeight
                    height: Theme.controlHeight
                    Accessible.name: "返回监控工作区"
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: root.backRequested()
                    contentItem: Text {
                        anchors.centerIn: parent
                        text: "‹"
                        color: Theme.text
                        font.pixelSize: 28
                    }
                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: settingsBackButton.hovered ? Theme.controlSurface : "transparent"
                    }
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        text: "设置"
                        color: Theme.text
                        font.bold: true
                        font.pixelSize: 20
                    }
                    Text {
                        text: "窗口与应用行为"
                        color: Theme.mutedText
                        font.pixelSize: 12
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: closeBehaviorSection.height + 32
                radius: Theme.radiusMedium
                color: Theme.controlSurface
                border.color: Theme.border

                Column {
                    id: closeBehaviorSection
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 12

                    Text {
                        text: "关闭按钮行为"
                        color: Theme.text
                        font.bold: true
                        font.pixelSize: 14
                    }
                    Text {
                        text: "选择关闭窗口时的默认动作。每次询问不会保存默认选择。"
                        color: Theme.mutedText
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }

                    ButtonGroup { id: closeBehaviorGroup }

                    Repeater {
                        model: [
                            { value: "ask", title: "每次询问", detail: "关闭时显示操作选择" },
                            { value: "background", title: "进入后台", detail: "隐藏窗口并保留直播播放" },
                            { value: "quit", title: "完全退出", detail: "停止播放并退出应用" }
                        ]

                        delegate: RadioButton {
                            id: option
                            required property var modelData
                            objectName: "closeBehaviorOption_" + modelData.value
                            width: closeBehaviorSection.width
                            height: 48
                            text: modelData.title
                            checked: root.closeBehavior === modelData.value
                            ButtonGroup.group: closeBehaviorGroup
                            onClicked: root.chooseCloseBehavior(modelData.value)

                            indicator: Rectangle {
                                x: 0
                                anchors.verticalCenter: parent.verticalCenter
                                width: 18
                                height: 18
                                radius: 9
                                color: Theme.well
                                border.color: option.checked ? Theme.accent : Theme.borderStrong
                                border.width: option.checked ? 5 : 1
                            }
                            contentItem: Column {
                                anchors.left: parent.left
                                anchors.leftMargin: 28
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2
                                Text {
                                    text: option.text
                                    color: Theme.text
                                    font.pixelSize: 12
                                }
                                Text {
                                    text: option.modelData.detail
                                    color: Theme.mutedText
                                    font.pixelSize: 10
                                }
                            }
                            background: Rectangle {
                                radius: Theme.radiusSmall
                                color: option.hovered ? Theme.well : "transparent"
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: teamManagementSection.height + 32
                radius: Theme.radiusMedium
                color: Theme.controlSurface
                border.color: Theme.border

                Column {
                    id: teamManagementSection
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 10

                    Text {
                        text: "队伍管理"
                        color: Theme.text
                        font.bold: true
                        font.pixelSize: 14
                    }
                    Text {
                        text: "队伍是本导航页的主要分组，成员调整不影响现有直播间与旧分组。"
                        color: Theme.mutedText
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }
                    Button {
                        objectName: "manageTeamsButton"
                        text: "管理队伍"
                        enabled: root.controller !== null
                        onClicked: root.teamManagerRequested()
                    }
                }
            }

            Column {
                width: parent.width
                spacing: 8
                Text {
                    text: "活动角色（本地配置）"
                    color: Theme.text
                    font.bold: true
                    font.pixelSize: 14
                }
                TextField {
                    objectName: "eventMappingName"
                    width: parent.width
                    color: Theme.text
                    palette.base: Theme.well
                    palette.text: Theme.text
                    palette.highlight: Theme.accent
                    text: root.eventMapping.event
                    placeholderText: "活动名称"
                    maximumLength: 80
                    onTextEdited: root.eventMapping.event = text
                }
                Repeater {
                    objectName: "eventMappingRows"
                    model: root.eventMapping.members
                    delegate: Row {
                        required property var modelData
                        required property int index
                        width: settingsContent.width
                        height: 36
                        spacing: 8
                        Text {
                            width: Math.max(100, parent.width - 256)
                            height: parent.height
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideRight
                            text: modelData.name
                            color: Theme.text
                            font.pixelSize: 12
                        }
                        ComboBox {
                            objectName: "eventMappingRole_" + parent.index
                            palette.button: Theme.controlSurface
                            palette.buttonText: Theme.text
                            palette.base: Theme.well
                            palette.window: Theme.well
                            palette.light: Theme.controlSurface
                            palette.midlight: Theme.borderStrong
                            palette.mid: Theme.border
                            palette.dark: Theme.mutedText
                            palette.text: Theme.text
                            palette.highlight: Theme.accent
                            palette.highlightedText: Theme.text
                            width: 112
                            height: 32
                            model: ["团长", "队长", "队员"]
                            currentIndex: ["leader", "captain", "member"].indexOf(modelData.role)
                            onActivated: root.eventMapping.members[parent.index].role = ["leader", "captain", "member"][currentIndex]
                        }
                        ComboBox {
                            objectName: "eventMappingTeam_" + parent.index
                            palette.button: Theme.controlSurface
                            palette.buttonText: Theme.text
                            palette.base: Theme.well
                            palette.window: Theme.well
                            palette.light: Theme.controlSurface
                            palette.midlight: Theme.borderStrong
                            palette.mid: Theme.border
                            palette.dark: Theme.mutedText
                            palette.text: Theme.text
                            palette.highlight: Theme.accent
                            palette.highlightedText: Theme.text
                            width: 112
                            height: 32
                            model: ["无回退队伍", "红队", "黑队", "紫队", "蓝队"]
                            currentIndex: modelData.team === null || modelData.team === undefined ? 0 : modelData.team + 1
                            onActivated: root.eventMapping.members[parent.index].team = currentIndex === 0 ? null : currentIndex - 1
                        }
                    }
                }
                ComboBox {
                    id: mappingMemberPicker
                    objectName: "eventMappingMemberPicker"
                    palette.button: Theme.controlSurface
                    palette.buttonText: Theme.text
                    palette.base: Theme.well
                    palette.window: Theme.well
                    palette.light: Theme.controlSurface
                    palette.midlight: Theme.borderStrong
                    palette.mid: Theme.border
                    palette.dark: Theme.mutedText
                    palette.text: Theme.text
                    palette.highlight: Theme.accent
                    palette.highlightedText: Theme.text
                    width: parent.width
                    model: root.controller ? root.controller.guildRoster : []
                    textRole: "anchorName"
                }
                Row {
                    spacing: 8
                    Button {
                        text: "添加主播"
                        palette.button: Theme.controlSurface
                        palette.buttonText: Theme.text
                        enabled: mappingMemberPicker.currentIndex >= 0
                        onClicked: {
                            const member = mappingMemberPicker.model[mappingMemberPicker.currentIndex]
                            if (!member.roomId) { root.mappingError = "该主播尚未确认房间号"; return }
                            const entries = root.eventMapping.members.slice()
                            for (let i = 0; i < entries.length; ++i) {
                                if (entries[i].roomId === member.roomId) return
                            }
                            entries.push({ roomId: member.roomId, name: member.anchorName, role: "member", team: null })
                            root.eventMapping = { version: 1, event: root.eventMapping.event, members: entries }
                        }
                    }
                    Button {
                        objectName: "saveEventMappingButton"
                        text: "保存角色配置"
                        palette.button: Theme.controlSurface
                        palette.buttonText: Theme.text
                        enabled: !!root.controller && !!root.controller.saveEventMapping
                        onClicked: root.mappingError = root.controller.saveEventMapping(JSON.stringify(root.eventMapping))
                    }
                }
                Text {
                    width: parent.width
                    visible: text.length > 0
                    text: root.mappingError
                    color: Theme.text
                    wrapMode: Text.WordWrap
                }
            }

            Rectangle {
                width: parent.width
                height: updateSection.height + 32
                radius: Theme.radiusMedium
                color: Theme.controlSurface
                border.color: Theme.border

                Column {
                    id: updateSection
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 10

                    Text {
                        text: "更新"
                        color: Theme.text
                        font.bold: true
                        font.pixelSize: 14
                    }
                    Text {
                        text: "检查应用是否有新版本。"
                        color: Theme.mutedText
                        font.pixelSize: 11
                    }
                    Column {
                        width: parent.width
                        spacing: 8

                        Row {
                            spacing: 10
                            Button {
                                objectName: "checkUpdateButton"
                                text: "检查更新"
                                enabled: !!root.controller && root.controller.updateState !== "checking"
                                onClicked: if (root.controller) root.controller.checkForUpdates()
                            }
                            Button {
                                objectName: "openReleaseButton"
                                visible: !!root.controller
                                         && root.controller.updateState === "updateAvailable"
                                         && root.controller.updateReleaseUrl.toString().length > 0
                                text: "打开发布页"
                                onClicked: root.controller.openLatestRelease()
                            }
                        }

                        Text {
                            visible: !!root.controller && root.controller.updateMessage.length > 0
                            width: parent.width
                            text: root.controller ? root.controller.updateMessage : ""
                            color: Theme.mutedText
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }
}
