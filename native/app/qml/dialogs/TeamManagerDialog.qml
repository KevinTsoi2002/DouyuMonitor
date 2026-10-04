import QtQuick
import QtQuick.Controls.Basic
import ".."

Dialog {
    id: root

    objectName: "teamManagerDialog"
    property var controller: null
    property var workspaceModel: null
    property string selectedTeamId: ""
    property string statusMessage: ""
    readonly property var teams: workspaceModel ? workspaceModel.teams : []
    readonly property var roster: controller ? controller.guildRoster : []
    readonly property bool validName: teamNameInput.text.trim().length > 0
                                     && teamNameInput.text.trim().length <= 30
    readonly property var selectedTeam: root.findTeam(root.selectedTeamId)
    readonly property var importData: controller && controller.maoziTeamImport !== undefined
                                      ? controller.maoziTeamImport : ({state: "idle"})
    readonly property bool importing: importData.state !== "idle"

    modal: true
    title: "队伍管理"
    width: 620
    height: 680
    anchors.centerIn: parent
    padding: 16
    palette.window: Theme.controlSurface
    palette.base: Theme.well
    palette.button: Theme.controlSurface
    palette.buttonText: Theme.text
    palette.text: Theme.text
    palette.windowText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.text
    onClosed: {
        if (root.controller && root.importing)
            root.controller.cancelMaoziTeamImport()
    }

    background: Rectangle {
        color: Theme.controlSurface
        border.color: Theme.border
        radius: Theme.radiusLarge
    }

    function findTeam(teamId)
    {
        for (let index = 0; index < teams.length; ++index) {
            if (teams[index].id === teamId) return teams[index]
        }
        return null
    }

    function selectTeam(team)
    {
        root.selectedTeamId = team.id
        teamNameInput.text = team.name
        root.statusMessage = ""
    }

    function currentTeamName(memberId)
    {
        for (let teamIndex = 0; teamIndex < teams.length; ++teamIndex) {
            const team = teams[teamIndex]
            if (team.memberIds && team.memberIds.indexOf(memberId) >= 0) return team.name
        }
        return ""
    }

    function memberBelongsToTeam(memberId)
    {
        for (let teamIndex = 0; teamIndex < teams.length; ++teamIndex) {
            const team = teams[teamIndex]
            if (team.memberIds && team.memberIds.indexOf(memberId) >= 0) return true
        }
        return false
    }

    function showStatus(message)
    {
        root.statusMessage = message || ""
        if (root.statusMessage.length > 0) statusTimer.restart()
    }

    function createTeam()
    {
        if (!root.controller) return
        const previousCount = root.teams.length
        const result = root.controller.createTeam(teamNameInput.text.trim())
        if (root.teams.length > previousCount && result.length > 0) {
            root.selectedTeamId = result
            teamNameInput.text = root.findTeam(result) ? root.findTeam(result).name : ""
            root.showStatus("")
            return
        }
        root.showStatus(result.length > 0 ? result : "无法创建队伍")
    }

    function renameTeam()
    {
        if (!root.controller || root.selectedTeamId.length === 0) return
        root.showStatus(root.controller.renameTeam(root.selectedTeamId,
                                                  teamNameInput.text.trim()))
    }

    function moveTeam(delta)
    {
        if (!root.controller || root.selectedTeamId.length === 0) return
        root.showStatus(root.controller.moveTeam(root.selectedTeamId, delta))
    }

    function deleteTeam()
    {
        if (!root.controller || root.selectedTeamId.length === 0) return
        const message = root.controller.deleteTeam(root.selectedTeamId)
        if (message.length > 0) {
            root.showStatus(message)
            return
        }
        root.selectedTeamId = ""
        teamNameInput.clear()
        root.showStatus("")
    }

    function assignMember(memberId)
    {
        if (!root.controller) return
        if (root.selectedTeamId.length === 0) {
            root.showStatus("请先选择队伍")
            return
        }
        root.showStatus(root.controller.assignGuildMemberToTeam(memberId,
                                                               root.selectedTeamId))
    }

    function removeMember(memberId)
    {
        if (!root.controller) return
        const team = root.findTeam(root.selectedTeamId)
        if (!team) {
            root.showStatus("请先选择队伍")
            return
        }
        root.showStatus(root.controller.removeGuildMemberFromTeam(team.id, memberId))
    }

    Timer {
        id: statusTimer
        interval: 4000
        onTriggered: root.statusMessage = ""
    }

    contentItem: Item {
      Column {
        anchors.fill: parent
        visible: !root.importing
        spacing: 10

        Row {
            width: parent.width
            spacing: 8
            Text {
                width: parent.width - importMaoziTeamsButton.width - 8
                anchors.verticalCenter: parent.verticalCenter
                text: "队伍 " + root.teams.length + "/20"
                color: Theme.mutedText
                font.pixelSize: 11
            }
            Button {
                id: importMaoziTeamsButton
                objectName: "importMaoziTeamsButton"
                text: qsTr("从野榜导入")
                palette.button: Theme.well
                palette.buttonText: Theme.text
                enabled: root.controller !== null && !root.importing
                onClicked: root.controller.previewMaoziTeamImport()
            }
        }

        Row {
            width: parent.width
            spacing: 6

            TextField {
                id: teamNameInput
                objectName: "teamNameInput"
                width: parent.width - createTeamButton.width - renameTeamButton.width - 12
                placeholderText: "队伍名称"
                color: Theme.text
                placeholderTextColor: Theme.mutedText
                selectByMouse: true
                maximumLength: 30
            }

            Button {
                id: createTeamButton
                objectName: "createTeamButton"
                text: "新建"
                enabled: root.validName && root.controller !== null
                onClicked: root.createTeam()
            }

            Button {
                id: renameTeamButton
                objectName: "renameTeamButton"
                text: "重命名"
                enabled: root.validName && root.selectedTeamId.length > 0
                         && root.controller !== null
                onClicked: root.renameTeam()
            }
        }

        Row {
            width: parent.width
            spacing: 8

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "队伍 " + root.teams.length + "/20"
                color: Theme.mutedText
                font.pixelSize: 11
            }

            Item {
                width: Math.max(0, parent.width - parent.children[0].width
                               - 3 * Theme.controlHeight - 4 * parent.spacing)
                height: 1
            }

            ToolButton {
                objectName: "moveTeamUpButton"
                width: Theme.controlHeight
                height: Theme.controlHeight
                enabled: root.selectedTeamId.length > 0 && root.controller !== null
                Accessible.name: "上移队伍"
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: root.moveTeam(-1)
                contentItem: Image {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    source: Qt.resolvedUrl("../assets/icons/chevron-up.svg")
                    opacity: parent.enabled ? 1 : 0.35
                }
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: parent.hovered ? Theme.well : "transparent"
                }
            }

            ToolButton {
                objectName: "moveTeamDownButton"
                width: Theme.controlHeight
                height: Theme.controlHeight
                enabled: root.selectedTeamId.length > 0 && root.controller !== null
                Accessible.name: "下移队伍"
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: root.moveTeam(1)
                contentItem: Image {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    source: Qt.resolvedUrl("../assets/icons/chevron-down.svg")
                    opacity: parent.enabled ? 1 : 0.35
                }
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: parent.hovered ? Theme.well : "transparent"
                }
            }

            ToolButton {
                objectName: "deleteTeamButton"
                width: Theme.controlHeight
                height: Theme.controlHeight
                enabled: root.selectedTeamId.length > 0 && root.controller !== null
                Accessible.name: "删除队伍"
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: root.deleteTeam()
                contentItem: Image {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    source: Qt.resolvedUrl("../assets/icons/x.svg")
                    opacity: parent.enabled ? 1 : 0.35
                }
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: parent.down ? Theme.danger : (parent.hovered ? Theme.well : "transparent")
                }
            }
        }

        ListView {
            id: teamList
            objectName: "teamList"
            width: parent.width
            height: 142
            clip: true
            spacing: 4
            model: root.teams
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: teamRow
                required property var modelData
                width: teamList.width
                height: 40
                radius: Theme.radiusSmall
                color: root.selectedTeamId === modelData.id ? Theme.well : Theme.canvas
                border.color: root.selectedTeamId === modelData.id
                              ? Theme.accent : Theme.border
                border.width: 1

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: deleteTeamRowButton.left
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.name + "  ·  " + modelData.memberCount + " 人"
                    color: Theme.text
                    elide: Text.ElideRight
                }

                ToolButton {
                    id: deleteTeamRowButton
                    objectName: "deleteTeamRowButton"
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    width: 30
                    height: 30
                    Accessible.name: "删除队伍"
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.selectedTeamId = modelData.id
                        root.deleteTeam()
                    }
                    contentItem: Image {
                        anchors.centerIn: parent
                        width: 12
                        height: 12
                        source: Qt.resolvedUrl("../assets/icons/x.svg")
                        opacity: parent.hovered ? 1 : 0.65
                    }
                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: parent.down ? Theme.danger : (parent.hovered ? Theme.well : "transparent")
                    }
                }

                MouseArea {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.right: deleteTeamRowButton.left
                    onClicked: root.selectTeam(modelData)
                }
            }
        }

        Text {
            width: parent.width
            text: "成员分配"
            color: Theme.text
            font.bold: true
            font.pixelSize: 13
        }

        ListView {
            id: teamMemberAssignmentList
            objectName: "teamMemberAssignmentList"
            width: parent.width
            height: 258
            clip: true
            spacing: 3
            model: root.roster
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                required property var modelData
                width: teamMemberAssignmentList.width
                height: 40
                radius: Theme.radiusSmall
                color: Theme.canvas
                border.color: Theme.border

                Row {
                    anchors.fill: parent
                    anchors.margins: 5
                    spacing: 6

                    Text {
                        width: Math.max(80, parent.width - 315)
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.anchorName
                        color: Theme.text
                        elide: Text.ElideRight
                    }

                    Text {
                        width: 58
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.roomId.length > 0 ? modelData.roomId : "待确认"
                        color: Theme.mutedText
                        elide: Text.ElideRight
                        font.pixelSize: 10
                    }

                    Text {
                        width: 72
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.memberBelongsToTeam(modelData.id)
                              ? root.currentTeamName(modelData.id) : "未分配"
                        color: root.memberBelongsToTeam(modelData.id)
                               ? Theme.text : Theme.mutedText
                        elide: Text.ElideRight
                        font.pixelSize: 10
                    }

                    Button {
                        objectName: "assignGuildMemberButton"
                        width: 88
                        height: 28
                        text: "加入"
                        enabled: root.selectedTeamId.length > 0
                                 && root.controller !== null
                        onClicked: root.assignMember(modelData.id)
                    }

                    Button {
                        objectName: "removeGuildMemberButton"
                        width: 58
                        height: 28
                        text: "移出"
                        enabled: root.memberBelongsToTeam(modelData.id)
                                 && root.controller !== null
                        onClicked: root.removeMember(modelData.id)
                    }
                }
            }
        }

        Text {
            objectName: "teamManagerStatus"
            width: parent.width
            height: 18
            text: root.statusMessage
            color: Theme.warning
            elide: Text.ElideRight
            font.pixelSize: 10
        }
      }

      Column {
        anchors.fill: parent
        visible: root.importing
        spacing: 12
        Text {
            width: parent.width
            text: root.importData.state === "loading" ? qsTr("正在刷新野榜…")
                  : root.importData.state === "error" ? (root.importData.error || "")
                  : qsTr("新增 %1 队 · 归属变更 %2 人 · 无变化 %3 人")
                    .arg(root.importData.createdTeams || 0)
                    .arg(root.importData.changedMembers || 0)
                    .arg(root.importData.unchangedMembers || 0)
            color: root.importData.state === "error" ? Theme.warning : Theme.text
            wrapMode: Text.Wrap
            font.pixelSize: 13
        }
        ListView {
            id: importPreview
            objectName: "maoziTeamImportPreviewList"
            width: parent.width
            height: Math.max(100, parent.height - importIssues.height - 65)
            clip: true
            spacing: 12
            model: root.importData.teams || []
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: Column {
                required property var modelData
                width: importPreview.width
                spacing: 6
                Text {
                    width: parent.width
                    text: qsTr("%1 · 来源 %2 人 · 匹配 %3 人 · 本地保留 %4 人")
                        .arg(modelData.name).arg(modelData.sourceCount)
                        .arg(modelData.matchedCount).arg(modelData.retainedCount)
                    color: Theme.text
                    font.bold: true
                    wrapMode: Text.Wrap
                }
                Text {
                    width: parent.width
                    text: (modelData.members || []).join("、")
                    color: Theme.mutedText
                    wrapMode: Text.Wrap
                    font.pixelSize: 12
                }
                Text {
                    width: parent.width
                    visible: modelData.retainedCount > 0
                    text: qsTr("保留：%1").arg((modelData.retainedMembers || []).join("、"))
                    color: Theme.mutedText
                    wrapMode: Text.Wrap
                    font.pixelSize: 11
                }
            }
        }
        ScrollView {
            id: importIssues
            width: parent.width
            height: Math.min(100, issuesText.implicitHeight)
            clip: true
            Text {
                id: issuesText
                width: importIssues.availableWidth
                text: ((root.importData.unmatchedNames || []).length > 0
                    ? qsTr("未匹配：%1").arg(root.importData.unmatchedNames.join("、")) : "")
                    + ((root.importData.conflictNames || []).length > 0
                    ? qsTr("\n冲突（保留原归属）：%1").arg(root.importData.conflictNames.join("、")) : "")
                    + ((root.importData.unassignedNames || []).length > 0
                    ? qsTr("\n未分队（保留原归属）：%1").arg(root.importData.unassignedNames.join("、")) : "")
                color: Theme.warning
                wrapMode: Text.Wrap
                font.pixelSize: 11
            }
        }
      }
    }

    footer: Rectangle {
        implicitHeight: 56
        color: Theme.controlSurface
        border.color: Theme.border

        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            visible: root.importing
            Button {
                objectName: "confirmMaoziTeamImportButton"
                text: qsTr("确认导入")
                width: 100
                height: 34
                contentItem: Text {
                    text: parent.text
                    color: parent.enabled ? Theme.text : Theme.mutedText
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.enabled ? Theme.accent : Theme.well
                    border.color: Theme.border
                    radius: Theme.radiusSmall
                }
                enabled: root.importData.canConfirm === true
                onClicked: {
                    const message = root.controller.confirmMaoziTeamImport()
                    if (!message) root.showStatus(qsTr("队伍已导入"))
                }
            }
            Button {
                objectName: "cancelMaoziTeamImportButton"
                text: qsTr("取消")
                width: 76
                height: 34
                contentItem: Text {
                    text: parent.text
                    color: Theme.text
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    color: parent.hovered ? Theme.well : Theme.controlSurface
                    border.color: Theme.border
                    radius: Theme.radiusSmall
                }
                onClicked: root.controller.cancelMaoziTeamImport()
            }
        }
        Button {
            anchors.right: parent.right
            anchors.rightMargin: 0
            anchors.verticalCenter: parent.verticalCenter
            width: 92
            height: 34
            text: "关闭"
            contentItem: Text {
                text: parent.text
                color: Theme.text
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle {
                color: parent.hovered ? Theme.well : Theme.controlSurface
                border.color: Theme.border
                radius: Theme.radiusSmall
            }
            onClicked: root.close()
        }
    }
}
