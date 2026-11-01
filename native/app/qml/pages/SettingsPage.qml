import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import ".."
import "../controls"

Item {
    id: root
    objectName: "settingsPage"
    property var controller: null
    signal backRequested()
    signal teamManagerRequested()
    readonly property string closeBehavior: controller && controller.closeBehavior ? controller.closeBehavior : "ask"
    readonly property bool closeBehaviorUnsaved: controller ? controller.closeBehaviorUnsaved === true : false
    readonly property string closeBehaviorError: controller && controller.closeBehaviorError ? controller.closeBehaviorError : ""
    property var eventMapping: ({ version: 1, event: "", members: [] })
    property string mappingError: ""
    readonly property bool wideLayout: settingsScrollArea.width >= 900

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

    Rectangle { anchors.fill: parent; color: Theme.canvas }

    Flickable {
        id: settingsScrollArea
        objectName: "settingsScrollArea"
        anchors.fill: parent
        anchors.margins: Theme.spaceLarge
        contentWidth: width
        contentHeight: settingsViewport.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Item {
            id: settingsViewport
            width: Math.min(settingsScrollArea.width, 1180)
            height: settingsContent.implicitHeight
            x: Math.max(0, (settingsScrollArea.width - width) / 2)

            ColumnLayout {
                id: settingsContent
                width: settingsViewport.width
                spacing: Theme.spaceLarge

                RowLayout {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 44
                    spacing: Theme.spaceMedium
                    ConsoleToolButton {
                        id: settingsBackButton
                        objectName: "settingsBackButton"
                        Layout.preferredWidth: Theme.controlHeight
                        Layout.preferredHeight: Theme.controlHeight
                        Accessible.name: "返回监控工作区"
                        ToolTip.text: Accessible.name
                        onClicked: root.backRequested()
                        contentItem: Text { anchors.centerIn: parent; text: "‹"; color: Theme.text; font.pixelSize: 28 }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceXXS
                        Text {
                            text: "设置"
                            color: Theme.text
                            font.bold: true
                            font.pixelSize: Theme.fontPageTitle
                            Layout.fillWidth: true
                        }
                        Text { text: "外观、窗口与工作区偏好"; color: Theme.mutedText; font.pixelSize: Theme.fontControl; Layout.fillWidth: true }
                    }
                }

                GridLayout {
                    id: settingsGroups
                    Layout.fillWidth: true
                    columns: root.wideLayout ? 2 : 1
                    columnSpacing: Theme.spaceLarge
                    rowSpacing: Theme.spaceLarge

                    ColumnLayout {
                        id: generalSettingsColumn
                        Layout.column: 0
                        Layout.row: 0
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: root.wideLayout ? 360 : settingsViewport.width
                        Layout.minimumWidth: 0
                        spacing: Theme.spaceLarge

                        Rectangle {
                            objectName: "appearanceSection"
                            Layout.fillWidth: true
                            implicitHeight: appearanceContent.implicitHeight + 32
                            color: Theme.managementSurface
                            border.color: Theme.border
                            radius: Theme.radiusSmall
                            ColumnLayout {
                                id: appearanceContent
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: Theme.spaceLarge
                                spacing: Theme.spaceMedium
                                Text {
                                    text: qsTr("外观")
                                    color: Theme.text
                                    font.pixelSize: Theme.fontHeading
                                    font.bold: true
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: qsTr("选择日间或夜间界面，视频区域始终保持深色。")
                                    color: Theme.mutedText
                                    font.pixelSize: Theme.fontControl
                                    wrapMode: Text.WordWrap
                                    Layout.fillWidth: true
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spaceSmall
                                    ConsoleButton {
                                        objectName: "nightThemeButton"
                                        Layout.fillWidth: true
                                        text: qsTr("夜间")
                                        checked: !Theme.isDay
                                        enabled: !!root.controller
                                        onClicked: if (root.controller) root.controller.setThemeMode("night")
                                    }
                                    ConsoleButton {
                                        objectName: "dayThemeButton"
                                        Layout.fillWidth: true
                                        text: qsTr("白天")
                                        checked: Theme.isDay
                                        enabled: !!root.controller
                                        onClicked: if (root.controller) root.controller.setThemeMode("day")
                                    }
                                }
                            }
                        }

                        Rectangle {
                            id: closeBehaviorSection
                            objectName: "closeBehaviorSection"
                            Layout.fillWidth: true
                            implicitHeight: closeBehaviorContent.implicitHeight + 32
                            radius: Theme.radiusMedium
                            color: Theme.managementSurface
                            border.color: Theme.border
                            ColumnLayout {
                                id: closeBehaviorContent
                                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: Theme.spaceLarge
                                spacing: Theme.spaceSmall
                                Text { text: "关闭按钮行为"; color: Theme.text; font.bold: true; font.pixelSize: Theme.fontHeading; Layout.fillWidth: true }
                                Text { text: "选择关闭窗口时的默认动作。每次询问不会保存默认选择。"; color: Theme.mutedText; font.pixelSize: Theme.fontCaption; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                                ButtonGroup { id: closeBehaviorGroup }
                                Repeater {
                                    id: closeBehaviorOptions
                                    objectName: "closeBehaviorOptions"
                                    model: [
                                        { value: "ask", title: "每次询问", detail: "关闭时显示操作选择" },
                                        { value: "background", title: "进入后台", detail: "隐藏窗口并保留直播播放" },
                                        { value: "quit", title: "完全退出", detail: "停止播放并退出应用" }
                                    ]
                                    delegate: ConsoleRadioButton {
                                        id: option
                                        required property var modelData
                                        objectName: "closeBehaviorOption_" + modelData.value
                                        Layout.fillWidth: true; Layout.preferredHeight: 48
                                        text: modelData.title; checked: root.closeBehavior === modelData.value
                                        ButtonGroup.group: closeBehaviorGroup
                                        onClicked: root.chooseCloseBehavior(modelData.value)
                                        leftPadding: 0
                                        contentItem: Column {
                                            anchors.left: parent.left; anchors.leftMargin: 28; anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
                                            spacing: Theme.spaceXXS
                                            Text { text: option.text; color: Theme.text; font.pixelSize: Theme.fontControl }
                                            Text { text: option.modelData.detail; color: Theme.mutedText; font.pixelSize: Theme.fontCaption }
                                        }
                                    }
                                }
                                RowLayout {
                                    visible: root.closeBehaviorUnsaved
                                    Layout.fillWidth: true
                                    spacing: Theme.spaceSmall
                                    Text {
                                        objectName: "closeBehaviorSaveStatus"
                                        text: root.closeBehaviorError.length > 0
                                              ? root.closeBehaviorError
                                              : "当前会话已生效，但尚未保存"
                                        color: Theme.warning
                                        font.pixelSize: Theme.fontCaption
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }
                                    ConsoleButton {
                                        objectName: "retryCloseBehaviorSaveButton"
                                        text: "重试保存"
                                        onClicked: if (root.controller) root.controller.retryCloseBehaviorSave()
                                    }
                                }
                            }
                        }

                        Rectangle {
                            id: teamManagementSection
                            objectName: "teamManagementSection"
                            Layout.fillWidth: true
                            implicitHeight: teamManagementContent.implicitHeight + 32
                            radius: Theme.radiusMedium; color: Theme.managementSurface; border.color: Theme.border
                            ColumnLayout {
                                id: teamManagementContent
                                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: Theme.spaceLarge
                                spacing: Theme.spaceSmall
                                Text { text: "队伍管理"; color: Theme.text; font.bold: true; font.pixelSize: Theme.fontHeading; Layout.fillWidth: true }
                                Text { text: "队伍是本导航页的主要分组，成员调整不影响现有直播间与旧分组。"; color: Theme.mutedText; font.pixelSize: Theme.fontCaption; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                                ConsoleButton { objectName: "manageTeamsButton"; text: "管理队伍"; enabled: root.controller !== null; Layout.alignment: Qt.AlignLeft; onClicked: root.teamManagerRequested() }
                            }
                        }

                        Rectangle {
                            id: updateSection
                            objectName: "updateSection"
                            Layout.fillWidth: true
                            implicitHeight: updateContent.implicitHeight + 32
                            radius: Theme.radiusMedium; color: Theme.managementSurface; border.color: Theme.border
                            ColumnLayout {
                                id: updateContent
                                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: Theme.spaceLarge
                                spacing: Theme.spaceSmall
                                Text { text: "更新"; color: Theme.text; font.bold: true; font.pixelSize: Theme.fontHeading; Layout.fillWidth: true }
                                Text { text: "检查应用是否有新版本。"; color: Theme.mutedText; font.pixelSize: Theme.fontCaption; Layout.fillWidth: true }
                                RowLayout {
                                    Layout.fillWidth: true; spacing: Theme.spaceSmall
                                    ConsoleButton { objectName: "checkUpdateButton"; text: "检查更新"; enabled: !!root.controller && root.controller.updateState !== "checking"; Layout.preferredWidth: Theme.buttonMinWidth; onClicked: if (root.controller) root.controller.checkForUpdates() }
                                    ConsoleButton { objectName: "openReleaseButton"; visible: !!root.controller && root.controller.updateState === "updateAvailable" && root.controller.updateReleaseUrl.toString().length > 0; text: "打开发布页"; Layout.preferredWidth: Theme.buttonMinWidth; onClicked: root.controller.openLatestRelease() }
                                }
                                Text { visible: text.length > 0; text: root.controller && root.controller.updateMessage ? root.controller.updateMessage : ""; color: Theme.mutedText; font.pixelSize: Theme.fontCaption; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            }
                        }
                    }

                    Rectangle {
                        id: eventMappingSection
                        objectName: "eventMappingSection"
                        Layout.column: root.wideLayout ? 1 : 0
                        Layout.row: root.wideLayout ? 0 : 1
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        Layout.preferredWidth: root.wideLayout ? settingsViewport.width - 392 : settingsViewport.width
                        Layout.minimumWidth: 0
                        implicitHeight: eventMappingContent.implicitHeight + 32
                        radius: Theme.radiusMedium; color: Theme.managementSurface; border.color: Theme.border
                        ColumnLayout {
                            id: eventMappingContent
                            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: Theme.spaceLarge
                            spacing: Theme.spaceSmall
                            Text { text: "活动角色"; color: Theme.text; font.bold: true; font.pixelSize: Theme.fontHeading; Layout.fillWidth: true }
                            Text { text: "配置活动名称、角色身份和队伍归属。角色设置只影响本地活动映射。"; color: Theme.mutedText; font.pixelSize: Theme.fontCaption; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                            ConsoleTextField { objectName: "eventMappingName"; Layout.fillWidth: true; text: root.eventMapping.event; placeholderText: "活动名称"; maximumLength: 80; onTextEdited: root.eventMapping.event = text }
                            RowLayout {
                                Layout.fillWidth: true; spacing: Theme.spaceSmall
                                Text { text: "主播"; color: Theme.mutedText; font.pixelSize: Theme.fontCaption; Layout.fillWidth: true }
                                Text { text: "身份"; color: Theme.mutedText; font.pixelSize: Theme.fontCaption; Layout.preferredWidth: 132 }
                                Text { text: "队伍"; color: Theme.mutedText; font.pixelSize: Theme.fontCaption; Layout.preferredWidth: 132 }
                            }
                            ColumnLayout {
                                id: eventMappingRowsColumn
                                objectName: "eventMappingRowsColumn"
                                Layout.fillWidth: true
                                spacing: Theme.spaceXS
                                Repeater {
                                    id: eventMappingRows
                                    objectName: "eventMappingRows"
                                    model: root.eventMapping.members
                                    delegate: RowLayout {
                                        required property var modelData
                                        required property int index
                                        readonly property int memberIndex: index
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 36
                                        spacing: Theme.spaceSmall
                                        Text { text: modelData.name; color: Theme.text; font.pixelSize: Theme.fontControl; elide: Text.ElideRight; verticalAlignment: Text.AlignVCenter; Layout.fillWidth: true; Layout.minimumWidth: 100 }
                                        ConsoleComboBox {
                                            objectName: "eventMappingRole_" + memberIndex
                                            Layout.preferredWidth: 132; Layout.minimumWidth: 112; Layout.maximumWidth: 160; Layout.preferredHeight: Theme.controlHeight
                                            model: ["团长", "队长", "队员"]
                                            currentIndex: ["leader", "captain", "member"].indexOf(modelData.role)
                                            onActivated: function(selectedIndex) {
                                                root.eventMapping.members[memberIndex].role = ["leader", "captain", "member"][selectedIndex]
                                            }
                                        }
                                        ConsoleComboBox {
                                            objectName: "eventMappingTeam_" + memberIndex
                                            Layout.preferredWidth: 132; Layout.minimumWidth: 112; Layout.maximumWidth: 160; Layout.preferredHeight: Theme.controlHeight
                                            model: ["无回退队伍", "红队", "黑队", "紫队", "蓝队"]
                                            currentIndex: modelData.team === null || modelData.team === undefined ? 0 : modelData.team + 1
                                            onActivated: function(selectedIndex) {
                                                root.eventMapping.members[memberIndex].team = selectedIndex === 0 ? null : selectedIndex - 1
                                            }
                                        }
                                    }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true; spacing: Theme.spaceSmall
                                ConsoleComboBox { id: mappingMemberPicker; objectName: "eventMappingMemberPicker"; Layout.fillWidth: true; model: root.controller ? root.controller.guildRoster : []; textRole: "anchorName" }
                                ConsoleButton {
                                    text: "添加主播"; enabled: mappingMemberPicker.currentIndex >= 0; Layout.preferredWidth: Theme.buttonMinWidth
                                    onClicked: {
                                        const member = mappingMemberPicker.model[mappingMemberPicker.currentIndex]
                                        if (!member.roomId) { root.mappingError = "该主播尚未确认房间号"; return }
                                        const entries = root.eventMapping.members.slice()
                                        for (let i = 0; i < entries.length; ++i) if (entries[i].roomId === member.roomId) return
                                        entries.push({ roomId: member.roomId, name: member.anchorName, role: "member", team: null })
                                        root.eventMapping = { version: 1, event: root.eventMapping.event, members: entries }
                                    }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true; spacing: Theme.spaceSmall
                                ConsoleButton { objectName: "saveEventMappingButton"; text: "保存角色配置"; highlighted: true; enabled: !!root.controller && !!root.controller.saveEventMapping; Layout.preferredWidth: Theme.buttonMinWidth + 20; onClicked: root.mappingError = root.controller.saveEventMapping(JSON.stringify(root.eventMapping)) }
                                Item { Layout.fillWidth: true }
                            }
                            Text { visible: text.length > 0; text: root.mappingError; color: Theme.danger; font.pixelSize: Theme.fontCaption; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        }
                    }
                }
            }
        }
    }
}
