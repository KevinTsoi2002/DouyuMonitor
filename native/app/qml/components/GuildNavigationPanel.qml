pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import ".."
import "../controls"

Rectangle {
    id: root

    property var controller: null
    property var workspaceModel: null
    signal addRoomRequested()
    property string statusFilter: "all"
    property string genderFilter: "all"
    property string selectedMemberId: ""
    readonly property var selectedMember: root.findMember(root.selectedMemberId)
    readonly property bool wideDetails: root.width >= 1100

    readonly property var roster: controller && controller.guildRoster ? controller.guildRoster : []
    readonly property string query: searchInput.text.trim().toLowerCase()
    readonly property var directoryMembers: root.buildDirectoryMembers()
    readonly property var visibleMembers: root.buildVisibleMembers()
    readonly property int visibleMemberCount: root.visibleMembers.length
    property bool syncActive: visible

    color: Theme.canvas
    clip: true

    function memberMatches(member)
    {
        if (member.directoryListed === false) return false
        if (root.genderFilter !== "all" && member.gender !== root.genderFilter) return false
        if (root.statusFilter === "online" && member.liveState !== "online") return false
        if (root.statusFilter === "active" && !member.active) return false
        if (root.query.length === 0) return true
        const anchorName = String(member.anchorName || "").toLowerCase()
        const roomId = String(member.roomId || "").toLowerCase()
        return anchorName.indexOf(root.query) >= 0 || roomId.indexOf(root.query) >= 0
    }

    function buildDirectoryMembers()
    {
        const members = []
        for (let index = 0; index < root.roster.length; ++index) {
            const member = root.roster[index]
            if (member.directoryListed === false) continue
            const order = member.directoryOrder !== undefined && member.directoryOrder !== null
                          ? Number(member.directoryOrder) : index
            members.push({ member: member, order: Number.isFinite(order) && order >= 0 ? order : index,
                           sourceIndex: index })
        }
        members.sort(function(left, right) {
            return left.order - right.order || left.sourceIndex - right.sourceIndex
        })
        return members.map(function(entry) { return entry.member })
    }

    function buildVisibleMembers()
    {
        return root.directoryMembers.filter(function(member) { return root.memberMatches(member) })
    }

    function findMember(memberId) {
        for (const member of root.roster) if (member.id === memberId) return member
        return null
    }

    function syncRank()
    {
        if (!root.syncActive || !root.controller) return
        if (!root.controller.maoziRank) return
        if (root.controller.maoziRank.checkForChanges)
            root.controller.maoziRank.checkForChanges()
    }

    function syncLiveStatus()
    {
        if (root.syncActive && root.controller && root.controller.refreshGuildLiveStatus)
            root.controller.refreshGuildLiveStatus()
    }

    function rebuildNavigationRows()
    {
        const nextRows = root.directoryMembers.map(function(member) {
            return {
                memberId: String(member.id || ""),
                anchorName: String(member.anchorName || ""),
                roomId: String(member.roomId || ""),
                roomStatus: String(member.status || ""),
                avatarUrl: String(member.avatarUrl || ""),
                fallbackAvatarUrl: String(member.fallbackAvatarUrl || ""),
                portraitIdentityVerified: member.portraitIdentityVerified === true,
                liveState: String(member.liveState || "unknown"),
                memberActive: !!member.active,
                memberVisible: root.memberMatches(member)
            }
        })
        // Keep the directory model stable; filtering must not recreate row controls and avatars.
        for (let index = 0; index < nextRows.length; ++index) {
            const next = nextRows[index]
            let current = index < navigationRows.count ? navigationRows.get(index) : null
            if (!current || current.memberId !== next.memberId) {
                let previousIndex = -1
                for (let later = index + 1; later < navigationRows.count; ++later) {
                    if (navigationRows.get(later).memberId === next.memberId) {
                        previousIndex = later
                        break
                    }
                }
                if (previousIndex >= 0) navigationRows.move(previousIndex, index, 1)
                else navigationRows.insert(index, next)
                current = navigationRows.get(index)
            }
            for (const key in next) {
                if (current[key] !== next[key]) navigationRows.setProperty(index, key, next[key])
            }
        }
        if (navigationRows.count > nextRows.length)
            navigationRows.remove(nextRows.length, navigationRows.count - nextRows.length)
    }

    function updateNavigationVisibility()
    {
        const visibleIds = {}
        for (const member of root.visibleMembers) visibleIds[String(member.id || "")] = true
        for (let index = 0; index < navigationRows.count; ++index) {
            const current = navigationRows.get(index)
            const memberVisible = visibleIds[current.memberId] === true
            if (current.memberVisible !== memberVisible)
                navigationRows.setProperty(index, "memberVisible", memberVisible)
        }
    }

    function memberForRow(memberId, anchorName, roomId, status, avatarUrl, fallbackAvatarUrl,
                          portraitIdentityVerified, liveState, active)
    {
        return {
            id: memberId,
            anchorName: anchorName,
            roomId: roomId,
            status: status,
            avatarUrl: avatarUrl,
            fallbackAvatarUrl: fallbackAvatarUrl,
            portraitIdentityVerified: portraitIdentityVerified,
            liveState: liveState,
            active: active
        }
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
        if (!root.controller) return false
        const message = root.controller.setGuildMemberRoomId(memberId, roomId)
        if (message && message.length > 0 && root.workspaceModel) {
            root.workspaceModel.setLastMessage(message, "error", 2400)
        }
        return !message || message.length === 0
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
        onTriggered: {
            root.syncRank()
            root.syncLiveStatus()
        }
    }

    Component.onCompleted: root.rebuildNavigationRows()
    onDirectoryMembersChanged: root.rebuildNavigationRows()
    onVisibleMembersChanged: {
        root.updateNavigationVisibility()
        if (root.selectedMemberId.length > 0 && !root.visibleMembers.some(function(member) {
            return member.id === root.selectedMemberId
        })) root.selectedMemberId = ""
    }
    onSyncActiveChanged: if (syncActive) root.syncRank()
    onSelectedMemberIdChanged: {
        if (selectedMemberId.length > 0 && !root.wideDetails) detailsClose.forceActiveFocus()
    }

    Column {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        Column {
            id: titleHeader
            width: parent.width
            spacing: 12
            Row {
                width: parent.width
                spacing: 16
                Text {
                    objectName: "guildNavigationTitle"
                    width: parent.width - (headerActions.visible ? headerActions.width + 16 : 0)
                    color: Theme.text
                    font.bold: true
                    font.pixelSize: Theme.fontPageTitle
                    elide: Text.ElideRight
                    text: qsTr("CSTG 主播导航")
                }
                Row {
                    id: headerActions
                    visible: titleHeader.width >= 650
                    spacing: 8
                    ConsoleButton {
                        objectName: "guildRefreshStatusButton"
                        text: qsTr("刷新直播状态")
                        enabled: !!root.controller
                        onClicked: root.syncLiveStatus()
                    }
                    ConsoleButton {
                        objectName: "guildAddRoomButton"
                        text: qsTr("添加房间")
                        highlighted: true
                        onClicked: root.addRoomRequested()
                    }
                }
            }
            Text {
                width: parent.width
                text: qsTr("公会主播 · 按官网目录顺序 · %1 位").arg(root.visibleMemberCount)
                color: Theme.mutedText
                font.pixelSize: Theme.fontControl
                wrapMode: Text.Wrap
            }
            Row {
                visible: !headerActions.visible
                spacing: 8
                ConsoleButton {
                    text: qsTr("刷新直播状态")
                    enabled: !!root.controller
                    onClicked: root.syncLiveStatus()
                }
                ConsoleButton { text: qsTr("添加房间"); highlighted: true; onClicked: root.addRoomRequested() }
            }
        }

        Flow {
            id: filters
            width: parent.width
            spacing: 8
            Row {
                objectName: "guildGenderFilters"
                spacing: 4
                Repeater {
                    model: [{ value: "all", label: qsTr("全部") },
                            { value: "m", label: qsTr("男主播") },
                            { value: "f", label: qsTr("女主播") }]
                    delegate: ConsoleToolButton {
                        id: genderButton
                        required property var modelData
                        objectName: "guildGenderFilter_" + genderButton.modelData.value
                        text: genderButton.modelData.label
                        checkable: true
                        checked: root.genderFilter === genderButton.modelData.value
                        onClicked: root.genderFilter = genderButton.modelData.value
                    }
                }
            }
            ConsoleTextField {
                id: searchInput
                objectName: "guildNavigationSearch"
                width: Math.min(340, Math.max(160, filters.width - 410))
                height: Theme.controlHeight
                placeholderText: qsTr("搜索主播或房间号")
                selectByMouse: true
                font.pixelSize: Theme.fontControl
            }
            ConsoleComboBox {
                objectName: "guildStatusFilter"
                width: 140
                model: [qsTr("全部状态"), qsTr("直播中"), qsTr("已添加")]
                currentIndex: ["all", "online", "active"].indexOf(root.statusFilter)
                onActivated: function(index) { root.statusFilter = ["all", "online", "active"][index] }
            }
        }

        Flickable {
            id: teamScroll
            objectName: "guildNavigationScroll"
            visible: !root.selectedMember || root.wideDetails
            width: parent.width - (root.selectedMember && root.wideDetails ? 340 : 0)
            height: Math.max(0, parent.height - titleHeader.height - filters.height - parent.spacing * 2)
            clip: true
            contentWidth: width
            contentHeight: teamColumn.implicitHeight
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: teamColumn
                width: teamScroll.width
                spacing: 2

                Repeater {
                    objectName: "guildVisibleRows"
                    model: navigationRows

                    delegate: Item {
                        id: rowItem
                        required property var model
                        width: teamColumn.width
                        height: visible ? memberRow.height : 0
                        visible: rowItem.model.memberVisible
                        enabled: visible

                        Rectangle {
                            objectName: "guildMemberRowBackground"
                            anchors.fill: parent
                            color: rowItem.model.memberId === root.selectedMemberId ? Theme.selectedSurface
                                   : memberRow.hovered ? Theme.controlHover : Theme.managementSurface
                            radius: Theme.radiusSmall
                            border.width: rowItem.model.memberId === root.selectedMemberId ? 2 : 0
                            border.color: Theme.accent
                        }

                        GuildMemberRow {
                            id: memberRow
                            objectName: "guildMemberRow"
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            anchors.top: parent.top
                            height: visible ? (confirming ? 104 : 64) : 0
                            member: root.memberForRow(rowItem.model.memberId, rowItem.model.anchorName,
                                                      rowItem.model.roomId, rowItem.model.roomStatus,
                                                      rowItem.model.avatarUrl, rowItem.model.fallbackAvatarUrl,
                                                      rowItem.model.portraitIdentityVerified,
                                                      rowItem.model.liveState,
                                                      rowItem.model.memberActive)
                            roomStatus: rowItem.model.roomStatus
                            active: rowItem.model.memberActive
                            selected: rowItem.model.memberId === root.selectedMemberId
                            onSelectedRequested: function(memberId) { root.selectedMemberId = memberId }
                            confirmRoomId: function(memberId, roomId) { return root.submitRoomId(memberId, roomId) }
                            canAdd: root.canAddMember(root.memberForRow(
                                        rowItem.model.memberId, rowItem.model.anchorName, rowItem.model.roomId,
                                        rowItem.model.roomStatus, rowItem.model.avatarUrl,
                                        rowItem.model.fallbackAvatarUrl,
                                        rowItem.model.portraitIdentityVerified,
                                        rowItem.model.liveState, rowItem.model.memberActive))
                            onAddRequested: function(memberId) {
                                root.quickAdd(memberId)
                            }
                        }
                    }
                }
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: root.visibleMemberCount === 0
        text: root.query.length > 0 || root.genderFilter !== "all" || root.statusFilter !== "all"
              ? qsTr("没有匹配的主播，请调整筛选或搜索") : qsTr("暂无主播资料")
        color: Theme.mutedText
        font.pixelSize: Theme.fontBody
    }

    Rectangle {
        id: memberDetails
        objectName: "guildMemberDetails"
        anchors.right: parent.right
        anchors.rightMargin: 24
        anchors.top: parent.top
        anchors.topMargin: contentColumn.y + titleHeader.height + filters.height + contentColumn.spacing * 2
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        width: root.wideDetails ? 320 : root.width - 48
        visible: !!root.selectedMember
        color: Theme.managementSurface
        border.color: Theme.border
        radius: Theme.radiusSmall
        z: 2
        Keys.onEscapePressed: function(event) {
            root.selectedMemberId = ""
            searchInput.forceActiveFocus()
            event.accepted = true
        }

        Flickable {
            anchors.fill: parent
            anchors.margins: 16
            contentWidth: width
            contentHeight: detailContent.implicitHeight
            clip: true
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            Column {
                id: detailContent
                width: parent.width
                spacing: 12
                Row {
                    width: parent.width
                    Text { width: parent.width - 40; text: qsTr("主播资料"); color: Theme.text; font.bold: true; font.pixelSize: Theme.fontHeading }
                    ConsoleToolButton {
                        id: detailsClose
                        objectName: "guildMemberDetailsClose"
                        width: 32; height: 32
                        text: "×"
                        Accessible.name: qsTr("关闭主播资料")
                        onClicked: { root.selectedMemberId = ""; searchInput.forceActiveFocus() }
                    }
                }
                Text {
                    objectName: "guildSelectedMemberName"
                    width: parent.width
                    text: root.selectedMember ? root.selectedMember.anchorName : ""
                    color: Theme.text; font.bold: true; font.pixelSize: Theme.fontHeading; wrapMode: Text.Wrap
                }
                Text {
                    width: parent.width
                    text: root.selectedMember ? ({ m: qsTr("男主播"), f: qsTr("女主播") }[root.selectedMember.gender] || qsTr("公会主播")) : ""
                    color: Theme.mutedText; font.pixelSize: Theme.fontControl; wrapMode: Text.Wrap
                }
                Text {
                    width: parent.width
                    text: root.selectedMember ? (root.selectedMember.roomId ? qsTr("房间 %1").arg(root.selectedMember.roomId) : qsTr("房间待确认")) : ""
                    color: Theme.text; font.pixelSize: Theme.fontControl; wrapMode: Text.Wrap
                }
                Text {
                    width: parent.width
                    text: !root.selectedMember ? "" : ({ online: qsTr("直播中"), offline: qsTr("未开播"), unknown: qsTr("直播状态未知") }[root.selectedMember.liveState] || qsTr("直播状态未知"))
                        + (root.selectedMember.active ? qsTr(" · 已加入") : "")
                    color: Theme.mutedText; font.pixelSize: Theme.fontControl; wrapMode: Text.Wrap
                }
                Text {
                    objectName: "guildSelectedRankSummary"
                    width: parent.width
                    text: root.selectedMember && root.selectedMember.rankMatched
                          ? qsTr("线上野榜 · 综合分 %1").arg(root.selectedMember.score >= 0 ? Number(root.selectedMember.score).toFixed(1) : "—")
                          : qsTr("暂无野榜数据")
                    color: Theme.text; font.pixelSize: Theme.fontControl; wrapMode: Text.Wrap
                }
                Text {
                    objectName: "guildSelectedPlacementSummary"
                    width: parent.width
                    visible: !!root.selectedMember && !!root.selectedMember.rankMatched
                    readonly property var metric: root.selectedMember ? root.selectedMember.placementAverage : null
                    text: qsTr("定级赛总评 %1").arg(metric !== null && metric !== undefined && Number(metric) >= 0 ? Number(metric).toFixed(2) : "—")
                    color: Theme.text; font.pixelSize: Theme.fontControl; wrapMode: Text.Wrap
                }
                Text {
                    objectName: "guildSelectedPlayValueSummary"
                    width: parent.width
                    visible: !!root.selectedMember && !!root.selectedMember.rankMatched
                    readonly property var metric: root.selectedMember ? root.selectedMember.playValue : null
                    text: qsTr("游乐值 %1").arg(metric !== null && metric !== undefined ? Number(metric).toFixed(1) : "—")
                    color: Theme.text; font.pixelSize: Theme.fontControl; wrapMode: Text.Wrap
                }
                Repeater {
                    model: root.selectedMember && root.selectedMember.rankMatched ? (root.selectedMember.radarDimensions || []) : []
                    delegate: Text {
                        required property var modelData
                        width: detailContent.width
                        text: String(modelData.label || modelData.name || "") + "  "
                              + (modelData.average !== undefined && modelData.average !== null && Number(modelData.average) >= 0 ? Number(modelData.average).toFixed(1) : "—")
                        color: Theme.mutedText; font.pixelSize: Theme.fontControl; wrapMode: Text.Wrap
                    }
                }
                ConsoleButton {
                    objectName: "guildSelectedMemberAdd"
                    width: parent.width
                    text: root.selectedMember && root.selectedMember.active ? qsTr("已加入工作区") : qsTr("加入工作区")
                    enabled: root.canAddMember(root.selectedMember)
                    highlighted: true
                    onClicked: root.quickAdd(root.selectedMemberId)
                }
                Text {
                    width: parent.width
                    visible: !!root.selectedMember && !root.canAddMember(root.selectedMember) && !root.selectedMember.active
                    text: root.selectedMember && !root.selectedMember.roomId ? qsTr("请先在名单中确认房间号") : qsTr("当前布局的房间容量已满")
                    color: Theme.warning; font.pixelSize: Theme.fontCaption; wrapMode: Text.Wrap
                }
            }
        }
    }
}
