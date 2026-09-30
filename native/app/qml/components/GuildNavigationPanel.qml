import QtQuick
import QtQuick.Controls
import ".."

Rectangle {
    id: root

    property var controller: null
    property var workspaceModel: null
    signal memberHovered(string memberId, real x, real y)
    signal memberHoverExited()

    readonly property var roster: controller ? controller.guildRoster : []
    readonly property var teams: workspaceModel
                                 ? workspaceModel.teams
                                 : (controller && controller.teams ? controller.teams : [])
    readonly property string query: searchInput.text.trim().toLowerCase()
    readonly property var teamSections: root.buildTeamSections()
    readonly property int visibleTeamCount: root.teamSections.length
    readonly property int visibleMemberCount: root.countVisibleMembers()
    property bool syncActive: visible

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

    function roleRank(role)
    {
        if (role === "leader") return 0
        if (role === "captain") return 1
        if (role === "member") return 2
        return 3
    }

    function compareMembers(left, right)
    {
        const leftKey = String(left.pinyinKey || "#")
        const rightKey = String(right.pinyinKey || "#")
        const keyOrder = leftKey.localeCompare(rightKey)
        if (keyOrder !== 0) return keyOrder
        return String(left.anchorName || "").localeCompare(String(right.anchorName || ""))
    }

    function syncRank()
    {
        if (!root.syncActive || !root.controller || !root.controller.maoziRank) return
        if (root.controller.maoziRank.checkForChanges)
            root.controller.maoziRank.checkForChanges()
    }

    function buildTeamSections()
    {
        const assigned = ({})
        const sourceSections = []
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
            sourceSections.push({
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
        const unassignedSection = {
            teamId: "",
            title: "未分队",
            isUnassigned: true,
            members: unassigned
        }

        const leadIn = []
        const teamSections = []
        const leaderMembers = []
        for (let index = 0; index < sourceSections.length; ++index) {
            const section = sourceSections[index]
            const captain = []
            const member = []
            const other = []
            for (let memberIndex = 0; memberIndex < section.members.length; ++memberIndex) {
                const entry = section.members[memberIndex]
                if (entry.role === "leader") leaderMembers.push(entry)
                else if (entry.role === "captain") captain.push(entry)
                else if (entry.role === "member") member.push(entry)
                else other.push(entry)
            }
            const groups = []
            if (captain.length > 0) groups.push({ title: "队长", members: captain.sort(root.compareMembers) })
            if (member.length > 0) groups.push({ title: "队员", members: member.sort(root.compareMembers) })
            if (other.length > 0) groups.push({ title: "其他", members: other.sort(root.compareMembers) })
            teamSections.push({ teamId: section.teamId, title: section.title,
                                isUnassigned: false, groups: groups })
        }
        leaderMembers.sort(root.compareMembers)
        if (leaderMembers.length > 0)
            leadIn.push({ teamId: "", title: "团长", isUnassigned: false,
                          isLeaderSection: true, groups: [{ title: "", members: leaderMembers }] })

        const unassignedCaptain = []
        const unassignedMember = []
        const unassignedOther = []
        for (let index = 0; index < unassignedSection.members.length; ++index) {
            const entry = unassignedSection.members[index]
            if (entry.role === "captain") unassignedCaptain.push(entry)
            else if (entry.role === "member") unassignedMember.push(entry)
            else unassignedOther.push(entry)
        }
        const unassignedGroups = []
        if (unassignedCaptain.length > 0)
            unassignedGroups.push({ title: "队长", members: unassignedCaptain.sort(root.compareMembers) })
        if (unassignedMember.length > 0)
            unassignedGroups.push({ title: "队员", members: unassignedMember.sort(root.compareMembers) })
        if (unassignedOther.length > 0)
            unassignedGroups.push({ title: "其他", members: unassignedOther.sort(root.compareMembers) })
        teamSections.push({ teamId: "", title: "未分队", isUnassigned: true,
                            groups: unassignedGroups })
        return leadIn.concat(teamSections)
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
                avatarUrl: "",
                liveState: "unknown",
                memberActive: false
            })
            for (let groupIndex = 0; groupIndex < section.groups.length; ++groupIndex) {
                const group = section.groups[groupIndex]
                if (group.title.length > 0) {
                    navigationRows.append({
                        rowType: "subheader",
                        title: group.title,
                        memberId: "",
                        anchorName: "",
                        roomId: "",
                        roomStatus: "",
                        avatarUrl: "",
                        liveState: "unknown",
                        memberActive: false
                    })
                }
                for (let memberIndex = 0; memberIndex < group.members.length; ++memberIndex) {
                    const member = group.members[memberIndex]
                    navigationRows.append({
                        rowType: "member",
                        title: "",
                        memberId: String(member.id || ""),
                        anchorName: String(member.anchorName || ""),
                        roomId: String(member.roomId || ""),
                        roomStatus: String(member.status || ""),
                        avatarUrl: String(member.avatarUrl || ""),
                        liveState: String(member.liveState || "unknown"),
                        memberActive: !!member.active
                    })
                }
            }
        }
    }

    function memberForRow(memberId, anchorName, roomId, status, avatarUrl, liveState, active)
    {
        return {
            id: memberId,
            anchorName: anchorName,
            roomId: roomId,
            status: status,
            avatarUrl: avatarUrl,
            liveState: liveState,
            active: active
        }
    }

    function countVisibleMembers()
    {
        let count = 0
        for (let index = 0; index < root.teamSections.length; ++index) {
            const section = root.teamSections[index]
            for (let groupIndex = 0; groupIndex < section.groups.length; ++groupIndex) {
                count += section.groups[groupIndex].members.length
            }
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
        const maxRooms = Number(root.workspaceModel && root.workspaceModel.maxRooms
                                ? root.workspaceModel.maxRooms : 16)
        return roomCount < maxRooms
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

    Timer {
        id: rankSyncTimer
        objectName: "rankSyncTimer"
        interval: 60000
        repeat: true
        running: root.syncActive
        onTriggered: root.syncRank()
    }

    Component.onCompleted: root.rebuildNavigationRows()
    onTeamSectionsChanged: root.rebuildNavigationRows()
    onSyncActiveChanged: if (syncActive) root.syncRank()

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Text {
            objectName: "guildNavigationTitle"
            color: Theme.text
            font.bold: true
            font.pixelSize: 15
            text: "CSTG狼团S1"
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
                        id: rowItem
                        width: teamColumn.width
                        height: model.rowType === "header" ? 20
                              : (model.rowType === "subheader" ? 18 : 46)

                        Text {
                            objectName: "guildTeamTitle"
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: visible ? implicitHeight : 0
                            visible: model.rowType === "header"
                            color: Theme.mutedText
                            font.bold: true
                            font.pixelSize: 11
                            elide: Text.ElideRight
                            text: model.title
                        }

                        Text {
                            objectName: "guildRoleTitle"
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: visible ? implicitHeight : 0
                            visible: model.rowType === "subheader"
                            color: Theme.mutedText
                            font.pixelSize: 10
                            elide: Text.ElideRight
                            text: model.title
                        }

                        GuildMemberRow {
                            objectName: "guildMemberRow"
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            height: visible ? (confirming ? 84 : 46) : 0
                            visible: model.rowType === "member"
                            member: root.memberForRow(model.memberId, model.anchorName,
                                                      model.roomId, model.roomStatus,
                                                      model.avatarUrl, model.liveState,
                                                      model.memberActive)
                            roomStatus: model.roomStatus
                            active: model.memberActive
                            canAdd: model.rowType === "member"
                                    && root.canAddMember(root.memberForRow(
                                        model.memberId, model.anchorName, model.roomId,
                                        model.roomStatus, model.avatarUrl,
                                        model.liveState, model.memberActive))
                            onAddRequested: function(memberId) {
                                root.quickAdd(memberId)
                            }
                            onRoomIdSubmitted: function(memberId, roomId) {
                                root.submitRoomId(memberId, roomId)
                            }
                            onHoverEntered: function(memberId) {
                                const point = rowItem.mapToItem(root, rowItem.width - 6,
                                                                Math.max(0, rowItem.height / 2))
                                root.memberHovered(memberId, point.x, point.y)
                            }
                            onHoverExited: root.memberHoverExited()
                        }
                    }
                }
            }
        }
    }
}
