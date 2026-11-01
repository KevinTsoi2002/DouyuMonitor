import QtQuick
import QtQuick.Controls.Basic
import ".."
import "../controls"

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
    width: Math.min(680, parent ? parent.width - 32 : 680)
    height: Math.min(680, parent ? parent.height - 32 : 680)
    anchors.centerIn: parent
    padding: 16
    palette {
        window: Theme.managementSurface
        base: Theme.well
        alternateBase: Theme.controlSurface
        light: Theme.controlHover
        midlight: Theme.controlSurface
        mid: Theme.border
        dark: Theme.mutedText
        shadow: Theme.canvas
        toolTipBase: Theme.controlSurface
        toolTipText: Theme.text
        link: Theme.accent
        linkVisited: Theme.mutedText
        active {
            windowText: Theme.text
            text: Theme.text
            button: Theme.controlSurface
            buttonText: Theme.text
            brightText: Theme.text
            highlight: Theme.accent
            highlightedText: Theme.accentText
            placeholderText: Theme.mutedText
        }
        inactive {
            windowText: Theme.text
            text: Theme.text
            button: Theme.controlSurface
            buttonText: Theme.text
            brightText: Theme.text
            highlight: Theme.accent
            highlightedText: Theme.accentText
            placeholderText: Theme.mutedText
        }
        disabled {
            windowText: Theme.disabledText
            text: Theme.disabledText
            button: Theme.controlDisabled
            buttonText: Theme.disabledText
            brightText: Theme.disabledText
            highlightedText: Theme.disabledText
            highlight: Theme.borderStrong
            placeholderText: Theme.disabledText
        }
    }
    onOpened: root.importing
              ? cancelMaoziTeamImportButton.forceActiveFocus()
              : teamNameInput.forceActiveFocus()
    onClosed: {
        if (root.controller && root.importing)
            root.controller.cancelMaoziTeamImport()
    }

    header: Item {
        implicitHeight: 56
        Text {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.text
            font.bold: true
            font.pixelSize: Theme.fontHeading
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
    }

    background: Rectangle {
        color: Theme.managementSurface
        border.color: Theme.border
        radius: Theme.radiusSmall
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
      Flickable {
        id: teamManagerGeneralScroll
        objectName: "teamManagerGeneralScroll"
        anchors.fill: parent
        visible: !root.importing
        clip: true
        contentWidth: width
        contentHeight: teamManagerGeneralContent.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
          id: teamManagerGeneralContent
          width: teamManagerGeneralScroll.width
          spacing: Theme.spaceSmall

        Row {
            width: parent.width
            spacing: Theme.spaceSmall
            Text {
                width: parent.width - importMaoziTeamsButton.width - 8
                anchors.verticalCenter: parent.verticalCenter
                text: "本地队伍 · " + root.teams.length + " / 20"
                color: Theme.mutedText
                font.pixelSize: Theme.fontCaption
            }
            ConsoleButton {
                id: importMaoziTeamsButton
                objectName: "importMaoziTeamsButton"
                text: qsTr("从野榜导入")
                enabled: root.controller !== null && !root.importing
                onClicked: root.controller.previewMaoziTeamImport()
            }
        }

        Row {
            width: parent.width
            spacing: Theme.spaceXS

            ConsoleTextField {
                id: teamNameInput
                objectName: "teamNameInput"
                width: parent.width - createTeamButton.width - renameTeamButton.width - 12
                placeholderText: "队伍名称"
                selectByMouse: true
                maximumLength: 30
            }

            ConsoleButton {
                id: createTeamButton
                objectName: "createTeamButton"
                text: "新建"
                enabled: root.validName && root.controller !== null
                onClicked: root.createTeam()
            }

            ConsoleButton {
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
            spacing: Theme.spaceSmall

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "选择队伍后管理成员"
                color: Theme.mutedText
                font.pixelSize: Theme.fontCaption
            }

            Item {
                width: Math.max(0, parent.width - parent.children[0].width
                               - 3 * Theme.controlHeight - 4 * parent.spacing)
                height: 1
            }

            ConsoleToolButton {
                objectName: "moveTeamUpButton"
                width: Theme.controlHeight
                height: Theme.controlHeight
                enabled: root.selectedTeamId.length > 0 && root.controller !== null
                Accessible.name: "上移队伍"
                ToolTip.text: Accessible.name
                onClicked: root.moveTeam(-1)
                icon.source: Qt.resolvedUrl("../assets/icons/chevron-up.svg")
            }

            ConsoleToolButton {
                objectName: "moveTeamDownButton"
                width: Theme.controlHeight
                height: Theme.controlHeight
                enabled: root.selectedTeamId.length > 0 && root.controller !== null
                Accessible.name: "下移队伍"
                ToolTip.text: Accessible.name
                onClicked: root.moveTeam(1)
                icon.source: Qt.resolvedUrl("../assets/icons/chevron-down.svg")
            }

            ConsoleToolButton {
                objectName: "deleteTeamButton"
                width: Theme.controlHeight
                height: Theme.controlHeight
                enabled: root.selectedTeamId.length > 0 && root.controller !== null
                Accessible.name: "删除队伍"
                ToolTip.text: Accessible.name
                onClicked: root.deleteTeam()
                icon.source: Qt.resolvedUrl("../assets/icons/x.svg")
                destructive: true
            }
        }

        ListView {
            id: teamList
            objectName: "teamList"
            width: parent.width
            height: 142
            clip: true
            spacing: 0
            model: root.teams
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: teamRow
                required property var modelData
                width: teamList.width
                height: 48
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

                ConsoleToolButton {
                    id: deleteTeamRowButton
                    objectName: "deleteTeamRowButton"
                    anchors.right: parent.right
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    width: 30
                    height: 30
                    Accessible.name: "删除队伍"
                    ToolTip.text: Accessible.name
                    onClicked: {
                        root.selectedTeamId = modelData.id
                        root.deleteTeam()
                    }
                    icon.source: Qt.resolvedUrl("../assets/icons/x.svg")
                    destructive: true
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
            font.pixelSize: Theme.fontBody
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
                height: 48
                radius: Theme.radiusSmall
                color: Theme.canvas
                border.color: Theme.border

                Row {
                    anchors.fill: parent
                    anchors.margins: 5
                    spacing: Theme.spaceXS

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
                        font.pixelSize: Theme.fontCaption
                    }

                    Text {
                        width: 72
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.memberBelongsToTeam(modelData.id)
                              ? root.currentTeamName(modelData.id) : "未分配"
                        color: root.memberBelongsToTeam(modelData.id)
                               ? Theme.text : Theme.mutedText
                        elide: Text.ElideRight
                        font.pixelSize: Theme.fontCaption
                    }

                    ConsoleButton {
                        objectName: "assignGuildMemberButton"
                        width: 88
                        height: 28
                        text: "加入"
                        enabled: root.selectedTeamId.length > 0
                                 && root.controller !== null
                        onClicked: root.assignMember(modelData.id)
                    }

                    ConsoleButton {
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
            font.pixelSize: Theme.fontCaption
        }
        }
      }

      Flickable {
        id: teamManagerImportScroll
        anchors.fill: parent
        visible: root.importing
        clip: true
        contentWidth: width
        contentHeight: teamManagerImportContent.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
          id: teamManagerImportContent
          width: teamManagerImportScroll.width
          spacing: Theme.spaceMedium
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
            font.pixelSize: Theme.fontBody
        }
        ListView {
            id: importPreview
            objectName: "maoziTeamImportPreviewList"
            width: parent.width
            height: Math.max(100, teamManagerImportScroll.height - importIssues.height - Theme.spaceLarge)
            clip: true
            spacing: Theme.spaceMedium
            model: root.importData.teams || []
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            delegate: Column {
                required property var modelData
                width: importPreview.width
                spacing: Theme.spaceXS
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
                    font.pixelSize: Theme.fontControl
                }
                Text {
                    width: parent.width
                    visible: modelData.retainedCount > 0
                    text: qsTr("保留：%1").arg((modelData.retainedMembers || []).join("、"))
                    color: Theme.mutedText
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontCaption
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
                font.pixelSize: Theme.fontCaption
            }
        }
        }
    }
    }

    footer: Rectangle {
        implicitHeight: 56
        color: Theme.managementSurface
        border.color: Theme.border

        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.spaceSmall
            visible: root.importing
            ConsoleButton {
                objectName: "confirmMaoziTeamImportButton"
                text: qsTr("确认导入")
                width: 100
                height: 34
                highlighted: true
                enabled: root.importData.canConfirm === true
                onClicked: {
                    const message = root.controller.confirmMaoziTeamImport()
                    if (!message) root.showStatus(qsTr("队伍已导入"))
                }
            }
            ConsoleButton {
                id: cancelMaoziTeamImportButton
                objectName: "cancelMaoziTeamImportButton"
                text: qsTr("取消")
                width: 76
                height: 34
                onClicked: root.controller.cancelMaoziTeamImport()
            }
        }
        ConsoleButton {
            anchors.right: parent.right
            anchors.rightMargin: 0
            anchors.verticalCenter: parent.verticalCenter
            width: 92
            height: 34
            text: "关闭"
            onClicked: root.close()
        }
    }
}
