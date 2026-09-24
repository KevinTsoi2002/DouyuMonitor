import QtQuick
import QtQuick.Controls
import ".."

Rectangle {
    id: root

    property var controller: null
    property var workspaceModel: null

    readonly property var roster: controller ? controller.guildRoster : []
    readonly property var teams: workspaceModel
                                 ? workspaceModel.teams
                                 : (controller && controller.teams ? controller.teams : [])
    readonly property string query: searchInput.text.trim().toLowerCase()
    readonly property var teamSections: root.buildTeamSections()
    readonly property int visibleTeamCount: root.teamSections.length
    readonly property int visibleMemberCount: root.countVisibleMembers()

    color: Theme.managementSurface
    border.color: Theme.border
    border.width: 1
    clip: true

    function memberBelongsToTeam(memberId, team)
    {
        return team.memberIds && team.memberIds.indexOf(memberId) >= 0
    }

    function memberMatches(member)
    {
        if (root.query.length === 0) return true
        const anchorName = String(member.anchorName || "").toLowerCase()
        const roomId = String(member.roomId || "").toLowerCase()
        return anchorName.indexOf(root.query) >= 0 || roomId.indexOf(root.query) >= 0
    }

    function buildTeamSections()
    {
        const sections = []
        const assigned = ({})
        for (let teamIndex = 0; teamIndex < root.teams.length; ++teamIndex) {
            const team = root.teams[teamIndex]
            const memberIds = team.memberIds || []
            const members = []
            for (let rosterIndex = 0; rosterIndex < root.roster.length; ++rosterIndex) {
                const member = root.roster[rosterIndex]
                if (memberIds.indexOf(member.id) < 0) continue
                assigned[member.id] = true
                if (root.memberMatches(member)) members.push(member)
            }
            sections.push({
                teamId: String(team.id || ""),
                title: String(team.name || "未命名队伍"),
                isUnassigned: false,
                members: members
            })
        }

        const unassigned = []
        for (let rosterIndex = 0; rosterIndex < root.roster.length; ++rosterIndex) {
            const member = root.roster[rosterIndex]
            if (assigned[member.id] || !root.memberMatches(member)) continue
            unassigned.push(member)
        }
        sections.push({
            teamId: "",
            title: "未分队",
            isUnassigned: true,
            members: unassigned
        })
        return sections
    }


    function rebuildNavigationRows()
    {
        navigationRows.clear()
        const sections = root.teamSections
        for (let sectionIndex = 0; sectionIndex < sections.length; ++sectionIndex) {
            const section = sections[sectionIndex]
            navigationRows.append({
                rowType: "header",
                title: section.title,
                memberId: "",
                anchorName: "",
                roomId: "",
                roomStatus: "",
                memberActive: false
            })
            for (let memberIndex = 0; memberIndex < section.members.length; ++memberIndex) {
                const member = section.members[memberIndex]
                navigationRows.append({
                    rowType: "member",
                    title: "",
                    memberId: String(member.id || ""),
                    anchorName: String(member.anchorName || ""),
                    roomId: String(member.roomId || ""),
                    roomStatus: String(member.status || ""),
                    memberActive: !!member.active
                })
            }
        }
    }

    function memberForRow(memberId, anchorName, roomId, status, active)
    {
        return {
            id: memberId,
            anchorName: anchorName,
            roomId: roomId,
            status: status,
            active: active
        }
    }

    function countVisibleMembers()
    {
        let count = 0
        for (let index = 0; index < root.teamSections.length; ++index) {
            count += root.teamSections[index].members.length
        }
        return count
    }

    function memberActive(memberId)
    {
        for (let index = 0; index < root.roster.length; ++index) {
            if (root.roster[index].id === memberId) return !!root.roster[index].active
        }
        return false
    }

    function canonicalMember(member)
    {
        for (let index = 0; index < root.roster.length; ++index) {
            if (root.roster[index].id === member.id) return root.roster[index]
        }
        return member
    }

    function canAddMember(member)
    {
        if (!member || String(member.roomId || "").length === 0 || member.active) return false
        const rooms = root.controller ? root.controller.rooms : null
        if (!rooms) return true
        const roomCount = Number(rooms.roomCount || 0)
        const layoutMode = root.workspaceModel ? root.workspaceModel.layoutMode : "auto"
        return (layoutMode === "primary-two" || roomCount < 9) && roomCount < 10
    }

    function quickAdd(memberId)
    {
        if (!root.controller) return
        const message = root.controller.addGuildMemberRoom(memberId)
        if (message && message.length > 0 && root.workspaceModel) {
            root.workspaceModel.setLastMessage(message, "error", 2400)
        }
    }

    function submitRoomId(memberId, roomId)
    {
        if (!root.controller) return
        const message = root.controller.setGuildMemberRoomId(memberId, roomId)
        if (message && message.length > 0 && root.workspaceModel) {
            root.workspaceModel.setLastMessage(message, "error", 2400)
        }
    }

    ListModel {
        id: navigationRows
    }

    Component.onCompleted: root.rebuildNavigationRows()
    onTeamSectionsChanged: root.rebuildNavigationRows()

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Text {
            color: Theme.text
            font.bold: true
            font.pixelSize: 15
            text: "仓鼠特工"
        }

        Item {
            objectName: "guildTeamSections"
            width: 0
            height: 0
            property int count: root.teamSections.length
        }

        TextField {
            id: searchInput
            objectName: "guildNavigationSearch"
            width: parent.width
            height: 30
            placeholderText: "搜索主播或房间号"
            color: Theme.text
            placeholderTextColor: Theme.mutedText
            selectByMouse: true
            font.pixelSize: 11
            background: Rectangle {
                radius: 4
                color: Theme.well
                border.color: searchInput.activeFocus ? Theme.accent : Theme.border
            }
        }

        Flickable {
            id: teamScroll
            width: parent.width
            height: Math.max(0, parent.height - searchInput.height - 50)
            clip: true
            contentWidth: width
            contentHeight: teamColumn.implicitHeight
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: teamColumn
                width: teamScroll.width
                spacing: 8

                Repeater {
                    objectName: "guildVisibleRows"
                    model: navigationRows

                    delegate: Item {
                        width: teamColumn.width
                        height: rowType === "header" ? 20 : 46

                        Text {
                            objectName: "guildTeamTitle"
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: visible ? implicitHeight : 0
                            visible: rowType === "header"
                            color: Theme.mutedText
                            font.bold: true
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            text: title
                        }

                        GuildMemberRow {
                            objectName: "guildMemberRow"
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: visible ? (confirming ? 76 : 38) : 0
                            visible: rowType === "member"
                            member: root.memberForRow(memberId, anchorName, roomId,
                                                      roomStatus, memberActive)
                            roomStatus: roomStatus
                            active: memberActive
                            canAdd: rowType === "member"
                                    && root.canAddMember(root.memberForRow(
                                        memberId, anchorName, roomId,
                                        roomStatus, memberActive))
                            onAddRequested: function(memberId) {
                                root.quickAdd(memberId)
                            }
                            onRoomIdSubmitted: function(memberId, roomId) {
                                root.submitRoomId(memberId, roomId)
                            }
                        }
                    }
                }
            }
        }
    }
}