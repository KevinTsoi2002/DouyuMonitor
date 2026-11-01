import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import ".."
import "../controls"
import "../components"

Item {
    id: root

    objectName: "maoziRankPage"
    property var controller: null
    signal backRequested

    readonly property var rankClient: controller ? controller.maoziRank : null
    readonly property var archive: controller && controller.s1Archive ? controller.s1Archive : ({})
    readonly property var onlineEntries: rankClient && rankClient.entries ? rankClient.entries : []
    readonly property var guildRoster: controller && controller.guildRoster ? controller.guildRoster : []
    readonly property var portraitSources: root.buildPortraitSources(root.onlineEntries, root.guildRoster)
    readonly property var entries: archive.ranking && archive.ranking.participants ? archive.ranking.participants : (rankClient ? rankClient.entries : [])
    readonly property var placementEntries: archive.placement && archive.placement.participants ? archive.placement.participants : (rankClient ? rankClient.placementEntries : [])
    readonly property var placementColumns: archive.placement && archive.placement.sessions ? root.archivePlacementColumns() : (rankClient ? rankClient.placementColumns : ({}))
    readonly property var visibleEntries: root.buildVisibleEntries()
    readonly property var visiblePlacementEntries: root.buildVisiblePlacementEntries()
    property string query: ""
    onQueryChanged: {
        selectedRankIndex = -1;
        selectedPlacementIndex = -1;
    }
    property string liveFilter: "all"
    property string activeTab: "ranking"
    property int selectedRankIndex: -1
    property int selectedPlacementIndex: -1
    property int selectedMatch: -1
    property int seasonPage: 0
    property string seasonDate: "all"
    readonly property var regularMatches: archive.regularSeason ? archive.regularSeason.matches || [] : []
    readonly property var finalMatches: archive.finals ? archive.finals.matches || [] : []
    readonly property var footballMatches: archive.football ? archive.football.matches || [] : []
    readonly property var activeMatches: activeTab === "regularSeason" ? regularMatches : (activeTab === "finals" ? finalMatches : footballMatches)
    readonly property var filteredMatches: root.filterMatches()
    readonly property var pageMatches: filteredMatches.slice(seasonPage * 10, seasonPage * 10 + 10)
    readonly property var placementColumnList: root.buildPlacementColumnList()
    readonly property int rankColumnWidth: 44
    readonly property int hostColumnWidth: Math.max(184, root.width - 708)
    readonly property int hostAvatarWidth: 34
    readonly property int hostAvatarGap: 8
    readonly property int hostTextWidth: hostColumnWidth - hostAvatarWidth - hostAvatarGap
    readonly property int roomColumnWidth: 100
    readonly property int teamColumnWidth: 104
    readonly property int gradeColumnWidth: 64
    readonly property int scoreColumnWidth: 84
    readonly property int votersColumnWidth: 72
    readonly property int statusColumnWidth: 76
    readonly property int rowHorizontalPadding: 12
    readonly property int rowColumnSpacing: 8
    readonly property int tableHeaderHeight: 36
    readonly property int rankingRowHeight: 56
    readonly property int placementRowHeight: 48
    readonly property int placementRankColumnWidth: 44
    readonly property int placementHostColumnWidth: Math.max(172, root.width - 716)
    readonly property int placementMetricWidth: 68
    readonly property int placementAverageWidth: 72
    readonly property real tableWidth: rankColumnWidth + hostColumnWidth + roomColumnWidth + teamColumnWidth + gradeColumnWidth + scoreColumnWidth + votersColumnWidth + statusColumnWidth + rowColumnSpacing * 7

    readonly property bool archivedRanking: !!(archive.ranking && archive.ranking.participants)
    readonly property bool archivedPlacement: !!(archive.placement && archive.placement.participants)
    readonly property bool showingOnlineData: (activeTab === "ranking" && !archivedRanking) || (activeTab === "placement" && !archivedPlacement)
    readonly property bool onlineError: showingOnlineData && !!rankClient && rankClient.errorMessage !== undefined && String(rankClient.errorMessage).length > 0
    readonly property string sourceStatus: onlineError ? "线上榜单暂时无法更新 · 可点击刷新重试" : showingOnlineData ? (rankClient ? String(rankClient.statusText || "线上榜单") : "暂无线上榜单数据") : "S1 赛季档案 · " + String(archive.capturedAt || "采集日期未记录")
    readonly property int placementTableWidth: placementRankColumnWidth + placementHostColumnWidth + teamColumnWidth + placementColumnList.length * placementMetricWidth + placementAverageWidth + rowColumnSpacing * (placementColumnList.length + 3) + rowHorizontalPadding * 2

    function rowSurface(index, hovered) {
        if (hovered)
            return Theme.controlHover;
        return index % 2 === 0 ? Theme.managementSurface : Theme.canvas;
    }

    function buildPortraitSources(online, roster) {
        const sources = { byId: {}, byRoom: {} };
        function append(map, key, portrait) {
            if (!key.length || !portrait.length) return;
            if (!map[key]) map[key] = [];
            if (map[key].indexOf(portrait) < 0) map[key].push(portrait);
        }
        for (const entry of online) {
            const portrait = String(entry.posterUrl || "").trim();
            append(sources.byId, String(entry.id || ""), portrait);
            append(sources.byRoom, String(entry.roomId || ""), portrait);
        }
        for (const member of roster) {
            const id = String(member.rankHostId || "");
            const roomId = String(member.roomId || "");
            for (const candidate of [member.avatarUrl, member.fallbackAvatarUrl]) {
                const portrait = String(candidate || "").trim();
                if (member.portraitIdentityVerified !== false) append(sources.byId, id, portrait);
                append(sources.byRoom, roomId, portrait);
                for (const historicalRoom of member.portraitIdentityVerified !== false ? (member.portraitRoomIds || []) : [])
                    append(sources.byRoom, String(historicalRoom), portrait);
            }
        }
        return sources;
    }

    function portraitsFor(entry) {
        if (!entry) return [];
        const candidates = [String(entry.posterUrl || "")]
            .concat(root.portraitSources.byId[String(entry.id || "")] || [])
            .concat(root.portraitSources.byRoom[String(entry.roomId || "")] || []);
        return candidates.filter((value, index) => value.length > 0 && candidates.indexOf(value) === index);
    }

    function portraitFor(entry) {
        return root.portraitsFor(entry)[0] || "";
    }

    component PortraitImage: Image {
        id: portrait
        property var candidates: []
        readonly property string candidateKey: JSON.stringify(candidates)
        property int candidateIndex: 0
        source: candidates[candidateIndex] || ""
        onCandidateKeyChanged: candidateIndex = 0
        onStatusChanged: {
            if (status === Image.Error && candidateIndex + 1 < candidates.length)
                candidateIndex += 1;
        }
    }

    function archiveRankFor(entry) {
        const participants = root.archive.ranking ? root.archive.ranking.participants || [] : [];
        for (const candidate of participants) {
            if (entry.id && String(candidate.id) === String(entry.id)) return candidate;
        }
        return entry;
    }

    function filterMatches() {
        if (root.activeTab !== "regularSeason" && root.activeTab !== "finals")
            return root.activeMatches;
        const query = root.query.trim().toLowerCase();
        return root.activeMatches.filter(function (match) {
            if (root.seasonDate !== "all" && String(match.date || "") !== root.seasonDate)
                return false;
            return query.length === 0 || String(match.title || "").toLowerCase().indexOf(query) >= 0;
        });
    }

    function buildVisibleEntries() {
        const result = [];
        const normalizedQuery = root.query.trim().toLowerCase();
        for (let index = 0; index < root.entries.length; ++index) {
            const entry = root.entries[index];
            if (root.liveFilter === "live" && !entry.live)
                continue;
            if (root.liveFilter === "offline" && entry.live)
                continue;
            if (normalizedQuery.length > 0) {
                const name = String(entry.name || "").toLowerCase();
                const roomId = String(entry.roomId || "").toLowerCase();
                const note = String(entry.note || "").toLowerCase();
                if (name.indexOf(normalizedQuery) < 0 && roomId.indexOf(normalizedQuery) < 0 && note.indexOf(normalizedQuery) < 0) {
                    continue;
                }
            }
            if (root.archive.ranking && root.archive.ranking.participants) {
                result.push({
                    id: entry.id,
                    rank: Number(entry.rank),
                    name: entry.name,
                    roomId: entry.roomId,
                    note: "",
                    teamName: entry.team || "未分队",
                    grade: entry.grade || "—",
                    gradeColor: Theme.borderStrong,
                    score: entry.score,
                    voters: "—",
                    live: false,
                    posterUrl: root.portraitFor(entry),
                    posterCandidates: root.portraitsFor(entry)
                });
            } else {
                result.push(Object.assign({}, entry, { posterUrl: root.portraitFor(entry), posterCandidates: root.portraitsFor(entry) }));
            }
        }
        return result;
    }

    function buildPlacementColumnList() {
        const result = [];
        const columns = root.placementColumns || ({});
        const keys = Object.keys(columns);
        keys.sort();
        for (let index = 0; index < keys.length; ++index) {
            const column = columns[keys[index]];
            if (column)
                result.push(column);
        }
        return result;
    }

    function archivePlacementColumns() {
        const columns = ({});
        const labels = ["28午", "28晚", "29午", "29晚", "30午", "30晚"];
        const keys = ["2026-09-28:1", "2026-09-28:2", "2026-09-29:1", "2026-09-29:2", "2026-09-30:1", "2026-09-30:2"];
        for (let index = 0; index < keys.length; ++index)
            columns[keys[index]] = {
                key: keys[index],
                label: labels[index]
            };
        return columns;
    }

    function buildVisiblePlacementEntries() {
        const result = [];
        const normalizedQuery = root.query.trim().toLowerCase();
        for (let index = 0; index < root.placementEntries.length; ++index) {
            const entry = root.placementEntries[index];
            const identity = root.archivedPlacement ? root.archiveRankFor(entry) : entry;
            if (normalizedQuery.length > 0) {
                const name = String(entry.name || "").toLowerCase();
                const roomId = String(identity.roomId || "").toLowerCase();
                if (name.indexOf(normalizedQuery) < 0 && roomId.indexOf(normalizedQuery) < 0) {
                    continue;
                }
            }
            if (root.archive.placement && root.archive.placement.participants) {
                const scores = ({});
                for (let session = 0; session < (entry.stats || []).length; ++session) {
                    const stat = entry.stats[session];
                    scores[["2026-09-28:1", "2026-09-28:2", "2026-09-29:1", "2026-09-29:2", "2026-09-30:1", "2026-09-30:2"][session]] = stat.value;
                }
                result.push({
                    id: entry.id,
                    placementRank: index + 1,
                    name: entry.name,
                    roomId: identity.roomId || "",
                    posterUrl: root.portraitFor(identity),
                    posterCandidates: root.portraitsFor(identity),
                    teamName: entry.team || "—",
                    placementSessions: scores,
                    placementAverage: entry.overall
                });
            } else {
                result.push(Object.assign({}, entry, { posterUrl: root.portraitFor(entry), posterCandidates: root.portraitsFor(entry) }));
            }
        }
        return result;
    }

    function placementScoreText(entry, key) {
        if (!entry || !entry.placementSessions)
            return "—";
        return scoreText(entry.placementSessions[key]);
    }

    function scoreText(value) {
        if ((typeof value !== "number" && typeof value !== "string") || String(value).trim().length === 0)
            return "—";
        const numericValue = Number(value);
        return isFinite(numericValue) && numericValue >= 0 ? numericValue.toFixed(1) : "—";
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.canvas
    }

    ColumnLayout {
        id: pageLayout
        anchors.fill: parent
        anchors.margins: root.width < 1000 ? 16 : 24
        spacing: 12

        RowLayout {
            id: toolbar
            objectName: "maoziToolbar"
            Layout.fillWidth: true
            Layout.preferredHeight: 56
            spacing: 12
            ConsoleToolButton {
                objectName: "maoziBackButton"
                Layout.preferredWidth: Theme.controlHeight
                Layout.preferredHeight: Theme.controlHeight
                text: "‹"
                Accessible.name: "返回监控工作区"
                onClicked: root.backRequested()
            }
            ColumnLayout {
                objectName: "maoziTitleColumn"
                Layout.fillWidth: true
                spacing: 4
                Text {
                    objectName: "maoziTitle"
                    Layout.fillWidth: true
                    text: "郎团 S1 野榜"
                    color: Theme.text
                    font.pixelSize: Theme.fontPageTitle
                    font.bold: true
                    elide: Text.ElideRight
                }
                Text {
                    objectName: "maoziSubtitle"
                    Layout.fillWidth: true
                    text: "赛季成绩、赛程与参与者档案"
                    color: Theme.mutedText
                    font.pixelSize: Theme.fontControl
                    elide: Text.ElideRight
                }
            }
            ConsoleButton {
                id: openSiteButton
                objectName: "maoziOpenSiteButton"
                text: "打开野榜"
                enabled: root.controller !== null
                onClicked: {
                    if (root.controller)
                        root.controller.openExternalUrl("https://cstg-zy.com/#/squad/s1");
                }
            }
            ConsoleButton {
                id: refreshButton
                objectName: "maoziRefreshButton"
                text: root.rankClient && root.rankClient.loading ? "刷新中" : "刷新"
                highlighted: true
                enabled: !root.archivedRanking && !!root.rankClient && !root.rankClient.loading
                onClicked: if (root.rankClient)
                    root.rankClient.refresh()
            }
        }

        RowLayout {
            objectName: "maoziSearchToolbar"
            Layout.fillWidth: true
            spacing: 8
            ConsoleTextField {
                id: searchField
                objectName: "maoziSearchField"
                Layout.preferredWidth: 360
                Layout.maximumWidth: 480
                Layout.fillWidth: true
                placeholderText: root.activeTab === "ranking" || root.activeTab === "placement" ? "搜索主播 / 房间号" : "搜索赛程"
                Accessible.name: placeholderText
                onTextChanged: root.query = text
            }
            ConsoleComboBox {
                id: liveFilterBox
                objectName: "maoziLiveFilter"
                visible: !root.archivedRanking
                Layout.preferredWidth: 120
                model: [
                    {
                        value: "all",
                        label: "全部"
                    },
                    {
                        value: "live",
                        label: "直播中"
                    },
                    {
                        value: "offline",
                        label: "未开播"
                    }
                ]
                textRole: "label"
                valueRole: "value"
                onActivated: root.liveFilter = currentValue
            }
            Item {
                Layout.fillWidth: true
            }
            Text {
                text: root.showingOnlineData ? "线上数据" : "历史档案"
                color: Theme.mutedText
                font.pixelSize: Theme.fontCaption
            }
        }
        Row {
            id: tabBar
            objectName: "maoziTabBar"
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.controlHeight
            spacing: 4

            ConsoleToolButton {
                objectName: "maoziRankingTab"
                text: "排行榜"
                checkable: true
                checked: root.activeTab === "ranking"
                width: 104
                height: Theme.controlHeight
                onClicked: root.activeTab = "ranking"
            }
            ConsoleToolButton {
                objectName: "maoziRegularSeasonTab"
                text: "常规赛"
                checkable: true
                checked: root.activeTab === "regularSeason"
                width: 88
                height: Theme.controlHeight
                onClicked: {
                    root.activeTab = "regularSeason";
                    root.selectedMatch = -1;
                    root.seasonPage = 0;
                }
            }
            ConsoleToolButton {
                objectName: "maoziFinalsTab"
                text: "决赛"
                checkable: true
                checked: root.activeTab === "finals"
                width: 80
                height: Theme.controlHeight
                onClicked: {
                    root.activeTab = "finals";
                    root.selectedMatch = -1;
                    root.seasonPage = 0;
                }
            }
            ConsoleToolButton {
                objectName: "maoziFootballTab"
                text: "足球赛"
                checkable: true
                checked: root.activeTab === "football"
                width: 88
                height: Theme.controlHeight
                onClicked: {
                    root.activeTab = "football";
                    root.selectedMatch = -1;
                    root.seasonPage = 0;
                }
            }
            ConsoleToolButton {
                objectName: "maoziPlacementTab"
                text: "定级赛"
                checkable: true
                checked: root.activeTab === "placement"
                width: 88
                height: Theme.controlHeight
                onClicked: root.activeTab = "placement"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 32
            color: Theme.managementSurface
            radius: Theme.radiusSmall
            Text {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                verticalAlignment: Text.AlignVCenter
                text: root.sourceStatus
                color: root.onlineError ? Theme.warning : Theme.mutedText
                font.pixelSize: Theme.fontCaption
                elide: Text.ElideRight
            }
        }

        Item {
            id: pageBody
            Layout.fillWidth: true
            Layout.fillHeight: true

            Flickable {
                id: rankTableViewport
                objectName: "maoziRankTableViewport"
                anchors.fill: parent
                visible: root.activeTab === "ranking" && root.selectedRankIndex < 0
                contentWidth: Math.max(width, root.tableWidth + root.rowHorizontalPadding * 2)
                contentHeight: height
                flickableDirection: Flickable.HorizontalFlick
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                ScrollBar.horizontal: ScrollBar {
                    id: rankHorizontalBar
                    policy: ScrollBar.AsNeeded
                }
                Column {
                    width: rankTableViewport.contentWidth
                    height: rankTableViewport.height - (rankHorizontalBar.visible ? 12 : 0)
                    spacing: 0
                    Rectangle {
                        id: headerBar
                        objectName: "maoziRankingHeaderBar"
                        visible: root.activeTab === "ranking" && root.selectedRankIndex < 0
                        width: parent.width
                        height: root.tableHeaderHeight
                        radius: 0
                        color: Theme.managementSurface
                        border.color: Theme.border

                        Row {
                            anchors.fill: parent
                            anchors.leftMargin: root.rowHorizontalPadding
                            anchors.rightMargin: root.rowHorizontalPadding
                            spacing: root.rowColumnSpacing

                            Text {
                                objectName: "maoziHeaderRank"
                                width: root.rankColumnWidth
                                text: "排名"
                                color: Theme.mutedText
                                font.pixelSize: Theme.fontCaption
                                font.bold: true
                                height: headerBar.height
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text {
                                objectName: "maoziHeaderHost"
                                width: root.hostColumnWidth
                                text: "主播"
                                color: Theme.mutedText
                                font.pixelSize: Theme.fontCaption
                                font.bold: true
                                height: headerBar.height
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text {
                                objectName: "maoziHeaderRoom"
                                width: root.roomColumnWidth
                                text: "房间号"
                                color: Theme.mutedText
                                font.pixelSize: Theme.fontCaption
                                font.bold: true
                                height: headerBar.height
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text {
                                objectName: "maoziHeaderTeam"
                                width: root.teamColumnWidth
                                text: "队伍"
                                color: Theme.mutedText
                                font.pixelSize: Theme.fontCaption
                                font.bold: true
                                height: headerBar.height
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text {
                                objectName: "maoziHeaderGrade"
                                width: root.gradeColumnWidth
                                text: "评级"
                                color: Theme.mutedText
                                font.pixelSize: Theme.fontCaption
                                font.bold: true
                                height: headerBar.height
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text {
                                objectName: "maoziHeaderScore"
                                width: root.scoreColumnWidth
                                text: "综合评分"
                                color: Theme.mutedText
                                font.pixelSize: Theme.fontCaption
                                font.bold: true
                                height: headerBar.height
                                horizontalAlignment: Text.AlignRight
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text {
                                objectName: "maoziHeaderVoters"
                                width: root.votersColumnWidth
                                text: "人数"
                                color: Theme.mutedText
                                font.pixelSize: Theme.fontCaption
                                font.bold: true
                                height: headerBar.height
                                horizontalAlignment: Text.AlignRight
                                verticalAlignment: Text.AlignVCenter
                            }
                            Text {
                                objectName: "maoziHeaderStatus"
                                width: root.statusColumnWidth
                                text: "状态"
                                color: Theme.mutedText
                                font.pixelSize: Theme.fontCaption
                                font.bold: true
                                height: headerBar.height
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }

                    ListView {
                        id: rankList
                        objectName: "maoziRankList"
                        visible: root.activeTab === "ranking" && root.selectedRankIndex < 0
                        width: parent.width
                        height: Math.max(0, parent.height - root.tableHeaderHeight)
                        clip: true
                        model: root.visibleEntries
                        spacing: 0
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                        }

                        delegate: Rectangle {
                            id: rankRow
                            required property var modelData
                            required property int index
                            readonly property var entry: modelData
                            readonly property bool rowHovered: rowHover.hovered
                            readonly property bool canOpenDetail: !!(root.archive.ranking && root.archive.ranking.participants)
                            width: rankList.width
                            height: root.rankingRowHeight
                            radius: 0
                            color: root.rowSurface(index, rowHovered)
                            border.color: activeFocus ? Theme.accent : "transparent"
                            border.width: activeFocus ? 1 : 0
                            activeFocusOnTab: canOpenDetail
                            Accessible.role: Accessible.ListItem
                            Accessible.name: qsTr("排名 %1，%2").arg(entry.rank).arg(String(entry.name || qsTr("未知主播")))

                            function openDetail() {
                                if (canOpenDetail) root.selectedRankIndex = index
                            }

                            Keys.onPressed: function(event) {
                                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                                    openDetail()
                                    event.accepted = true
                                }
                            }

                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 1
                                color: Theme.border
                            }

                            HoverHandler {
                                id: rowHover
                            }
                            TapHandler {
                                enabled: rankRow.canOpenDetail
                                onTapped: rankRow.openDetail()
                            }

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: root.rowHorizontalPadding
                                anchors.rightMargin: root.rowHorizontalPadding
                                spacing: root.rowColumnSpacing

                                Text {
                                    objectName: "maoziCellRank"
                                    width: root.rankColumnWidth
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: entry.rank
                                    color: entry.rank <= 3 ? Theme.accent : Theme.text
                                    font.bold: true
                                    font.pixelSize: Theme.fontBody
                                    horizontalAlignment: Text.AlignHCenter
                                }

                                Item {
                                    objectName: "maoziCellHost"
                                    width: root.hostColumnWidth
                                    height: 34
                                    anchors.verticalCenter: parent.verticalCenter

                                    Rectangle {
                                        width: root.hostAvatarWidth
                                        height: root.hostAvatarWidth
                                        radius: width / 2
                                        anchors.left: parent.left
                                        anchors.verticalCenter: parent.verticalCenter
                                        color: Theme.well

                                        PortraitImage {
                                            id: avatarImage
                                            objectName: "maoziAvatarImage"
                                            anchors.fill: parent
                                            anchors.margins: 1
                                            candidates: entry.posterCandidates || []
                                            sourceSize.width: root.hostAvatarWidth
                                            sourceSize.height: root.hostAvatarWidth
                                            asynchronous: true
                                            fillMode: Image.PreserveAspectCrop
                                            visible: status === Image.Ready
                                            clip: true
                                        }
                                        Text {
                                            anchors.centerIn: parent
                                            visible: avatarImage.status !== Image.Ready
                                            text: String(entry.name || "?").slice(0, 1)
                                            color: Theme.text
                                            font.bold: true
                                        }
                                    }

                                    Column {
                                        anchors.left: parent.left
                                        anchors.leftMargin: root.hostAvatarWidth + root.hostAvatarGap
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 1
                                        Text {
                                            id: hostName
                                            width: parent.width
                                            text: entry.name
                                            color: Theme.text
                                            font.bold: true
                                            font.pixelSize: Theme.fontBody
                                            elide: Text.ElideRight

                                        }
                                        Text {
                                            width: parent.width
                                            text: entry.note
                                            visible: text.length > 0
                                            color: Theme.mutedText
                                            font.pixelSize: Theme.fontCaption
                                            elide: Text.ElideRight
                                        }
                                    }
                                }
                                Text {
                                    objectName: "maoziCellRoom"
                                    width: root.roomColumnWidth
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: entry.roomId
                                    color: Theme.mutedText
                                    font.pixelSize: Theme.fontCaption
                                    elide: Text.ElideRight
                                }
                                Text {
                                    objectName: "maoziCellTeam"
                                    width: root.teamColumnWidth
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: entry.teamName
                                    color: Theme.mutedText
                                    font.pixelSize: Theme.fontCaption
                                    elide: Text.ElideRight
                                }
                                Rectangle {
                                    objectName: "maoziCellGrade"
                                    width: root.gradeColumnWidth
                                    height: 22
                                    radius: 0
                                    anchors.verticalCenter: parent.verticalCenter
                                    color: Theme.selectedSurface
                                    Text {
                                        anchors.centerIn: parent
                                        text: entry.grade
                                        color: Theme.accent
                                        font.bold: true
                                        font.pixelSize: Theme.fontCaption
                                    }
                                }
                                Text {
                                    objectName: "maoziCellScore"
                                    width: root.scoreColumnWidth
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: root.scoreText(entry.score)
                                    color: Theme.text
                                    font.bold: true
                                    font.pixelSize: Theme.fontBody
                                    horizontalAlignment: Text.AlignRight
                                }
                                Text {
                                    objectName: "maoziCellVoters"
                                    width: root.votersColumnWidth
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: entry.voters
                                    color: Theme.mutedText
                                    font.pixelSize: Theme.fontCaption
                                    horizontalAlignment: Text.AlignRight
                                }
                                Item {
                                    objectName: "maoziCellStatus"
                                    width: root.statusColumnWidth
                                    height: 20
                                    anchors.verticalCenter: parent.verticalCenter
                                    Row {
                                        anchors.fill: parent
                                        spacing: Theme.spaceXS
                                        Rectangle {
                                            objectName: "maoziLiveStatusDot"
                                            width: 6
                                            height: 6
                                            radius: 3
                                            anchors.verticalCenter: parent.verticalCenter
                                            color: root.archive.ranking && root.archive.ranking.participants ? Theme.accent : (entry.live ? Theme.online : Theme.borderStrong)
                                        }
                                        Text {
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: root.archive.ranking && root.archive.ranking.participants ? "赛季成绩" : (entry.live ? "直播中" : "未开播")
                                            color: root.archive.ranking && root.archive.ranking.participants ? Theme.mutedText : (entry.live ? Theme.online : Theme.mutedText)
                                            font.pixelSize: Theme.fontCaption
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                Text {
                    anchors.centerIn: parent
                    visible: rankList.count === 0
                    text: root.showingOnlineData && root.rankClient && root.rankClient.loading ? "正在加载榜单…" : root.onlineError ? "榜单加载失败，请点击刷新重试" : (root.query.length > 0 ? "没有匹配的主播，试试其他关键词" : "暂无排行榜记录")
                    color: Theme.mutedText
                    font.pixelSize: Theme.fontBody
                }
            }
            Item {
                objectName: "maoziListWidthProbe"
                width: 1
                height: 1
                property real tableWidth: root.tableWidth
                property real contentWidth: rankList.width
            }

            Column {
                visible: root.activeTab === "ranking" && root.selectedRankIndex >= 0
                anchors.fill: parent
                spacing: Theme.gap
                ConsoleToolButton {
                    objectName: "maoziRankDetailBack"
                    text: "‹ 返回排行榜"
                    onClicked: root.selectedRankIndex = -1
                }
                ScrollView {
                    id: rankingDetailScroll
                    objectName: "maoziRankDetailScroll"
                    contentWidth: availableWidth
                    width: parent.width
                    height: Math.max(0, parent.height - Theme.controlHeight - Theme.gap)
                    clip: true
                    Column {
                        id: rankingDetailContent
                        objectName: "maoziRankParticipantDetail"
                        width: rankingDetailScroll.availableWidth
                        spacing: 16
                        readonly property var selected: root.visibleEntries[root.selectedRankIndex]
                        readonly property var record: selected ? (root.archive.ranking ? root.entries.find(function (candidate) {
                                return candidate.id === selected.id;
                            }) : selected) || ({}) : ({})
                        readonly property string text: {
                            const visible = root.visibleEntries[root.selectedRankIndex];
                            const entry = visible && root.archive.ranking ? root.entries.find(function (candidate) {
                                return candidate.id === visible.id;
                            }) : visible;
                            if (!entry)
                                return "";
                            return [entry.name, entry.team || "", entry.grade || "", "综合评分 " + entry.score].concat((entry.dimensions || []).map(function (dimension) {
                                return dimension.name + " " + dimension.value;
                            })).join("\n");
                        }
                        Rectangle {
                            width: parent.width
                            height: 118
                            radius: Theme.radiusSmall
                            color: Theme.managementSurface
                            border.color: Theme.border
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 16
                                spacing: 16
                                Rectangle {
                                    Layout.preferredWidth: 68
                                    Layout.preferredHeight: 68
                                    radius: Theme.radiusSmall
                                    color: Theme.well
                                    PortraitImage {
                                        id: detailAvatar
                                        objectName: "maoziRankDetailAvatar"
                                        anchors.fill: parent
                                        anchors.margins: 1
                                        candidates: rankingDetailContent.selected ? rankingDetailContent.selected.posterCandidates || [] : []
                                        sourceSize: Qt.size(136, 136)
                                        asynchronous: true
                                        fillMode: Image.PreserveAspectCrop
                                        visible: status === Image.Ready
                                    }
                                    Text {
                                        anchors.centerIn: parent
                                        visible: detailAvatar.status !== Image.Ready
                                        text: String(rankingDetailContent.record.name || "?").slice(0, 1)
                                        color: Theme.text
                                        font.pixelSize: 26
                                        font.bold: true
                                    }
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    spacing: 8
                                    Text {
                                        Layout.fillWidth: true
                                        text: rankingDetailContent.record.name || "详情未记录"
                                        color: Theme.text
                                        font.pixelSize: Theme.fontPageTitle
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: [rankingDetailContent.record.team || rankingDetailContent.record.teamName || qsTr("未分队"), rankingDetailContent.record.grade || rankingDetailContent.record.tier || qsTr("评级未记录")].join(" · ")
                                        color: Theme.mutedText
                                        font.pixelSize: Theme.fontControl
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: rankingDetailContent.record.roomId ? qsTr("房间 %1").arg(rankingDetailContent.record.roomId) : qsTr("房间未记录")
                                        color: Theme.mutedText
                                        font.pixelSize: Theme.fontCaption
                                    }
                                }
                                Column {
                                    spacing: 4
                                    Text {
                                        text: "综合评分"
                                        color: Theme.mutedText
                                        font.pixelSize: Theme.fontCaption
                                    }
                                    Text {
                                        text: rankingDetailContent.record.score === undefined || rankingDetailContent.record.score === null ? "—" : String(rankingDetailContent.record.score)
                                        color: Theme.accent
                                        font.pixelSize: 28
                                        font.bold: true
                                    }
                                }
                            }
                        }
                        Text {
                            text: root.archivedRanking ? qsTr("S1 赛季能力记录") : qsTr("能力评分")
                            color: Theme.text
                            font.pixelSize: Theme.fontHeading
                            font.bold: true
                        }
                        GridLayout {
                            id: rankMetricsLayout
                            width: parent.width
                            columns: width >= 700 ? 2 : 1
                            columnSpacing: 16
                            rowSpacing: 16
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 280
                                Layout.preferredWidth: 380
                                Layout.preferredHeight: 352
                                color: Theme.managementSurface
                                radius: Theme.radiusSmall
                                border.color: Theme.border
                                Column {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 4
                                    Text {
                                        text: qsTr("九维能力 · 满分 20")
                                        color: Theme.mutedText
                                        font.pixelSize: Theme.fontControl
                                    }
                                    RadarChart {
                                        id: rankRadar
                                        objectName: "maoziRankRadar"
                                        width: parent.width
                                        height: 300
                                        dimensions: rankingDetailContent.record.dimensions || []
                                    }
                                }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 250
                                Layout.preferredWidth: 380
                                Layout.preferredHeight: Math.max(352, preciseMetrics.implicitHeight + 24)
                                color: Theme.managementSurface
                                radius: Theme.radiusSmall
                                border.color: Theme.border
                                Column {
                                    id: preciseMetrics
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 12
                                    spacing: 0
                                    Text {
                                        width: parent.width
                                        height: 28
                                        text: qsTr("维度明细")
                                        color: Theme.mutedText
                                        font.pixelSize: Theme.fontControl
                                    }
                                    Repeater {
                                        model: rankRadar.normalizedDimensions
                                        delegate: Item {
                                            required property var modelData
                                            width: preciseMetrics.width
                                            height: 32
                                            RowLayout {
                                                anchors.fill: parent
                                                spacing: 12
                                                Text {
                                                    Layout.fillWidth: true
                                                    text: modelData.name
                                                    color: Theme.text
                                                    font.pixelSize: Theme.fontControl
                                                    elide: Text.ElideRight
                                                }
                                                Text {
                                                    objectName: "maoziRankDimensionValue"
                                                    text: rankRadar.valueText(modelData.value)
                                                    color: modelData.value === null ? Theme.mutedText : Theme.accent
                                                    font.pixelSize: Theme.fontBody
                                                    font.bold: true
                                                }
                                            }
                                            Rectangle {
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.bottom: parent.bottom
                                                height: 1
                                                color: Theme.border
                                            }
                                        }
                                    }
                                    Text {
                                        width: parent.width
                                        visible: rankRadar.normalizedDimensions.length === 0
                                        text: qsTr("暂无维度数据")
                                        color: Theme.mutedText
                                        font.pixelSize: Theme.fontControl
                                    }
                                }
                            }
                        }
                        Text {
                            width: parent.width
                            text: root.archivedRanking ? qsTr("历史档案成绩 · 缺失数据以 — 显示，雷达图不补零")
                                                       : qsTr("线上评分 · 缺失数据以 — 显示，雷达图不补零")
                            color: Theme.mutedText
                            font.pixelSize: Theme.fontCaption
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }

            Item {
                anchors.fill: parent
                visible: root.activeTab === "placement"
                Row {
                    id: placementActions
                    width: parent.width
                    visible: root.activeTab === "placement" && root.selectedPlacementIndex < 0
                    height: Theme.controlHeight
                    spacing: Theme.gap
                    ConsoleToolButton {
                        text: "总览"
                        checkable: true
                        checked: !placementSummary.visible
                        onClicked: placementSummary.visible = false
                    }
                    ConsoleToolButton {
                        text: "得分统计"
                        checkable: true
                        checked: placementSummary.visible && placementSummary.mode === "scores"
                        onClicked: {
                            placementSummary.mode = "scores";
                            placementSummary.visible = true;
                        }
                    }
                    ConsoleToolButton {
                        text: "赛程统计"
                        checkable: true
                        checked: placementSummary.visible && placementSummary.mode === "schedule"
                        onClicked: {
                            placementSummary.mode = "schedule";
                            placementSummary.visible = true;
                        }
                    }
                }

                Flickable {
                    id: placementViewport
                    objectName: "maoziPlacementTableViewport"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: placementActions.bottom
                    anchors.topMargin: 8
                    anchors.bottom: parent.bottom
                    visible: !placementSummary.visible && root.selectedPlacementIndex < 0
                    contentWidth: Math.max(width, root.placementTableWidth)
                    contentHeight: height
                    flickableDirection: Flickable.HorizontalFlick
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true
                    ScrollBar.horizontal: ScrollBar {
                        id: placementHorizontalBar
                        policy: ScrollBar.AsNeeded
                    }
                    Column {
                        width: placementViewport.contentWidth
                        height: placementViewport.height - (placementHorizontalBar.visible ? 12 : 0)
                        spacing: 0
                        Rectangle {
                            objectName: "maoziPlacementHeaderBar"
                            visible: root.activeTab === "placement" && !placementSummary.visible && root.selectedPlacementIndex < 0
                            width: parent.width
                            height: root.tableHeaderHeight
                            radius: 0
                            color: Theme.well
                            border.color: Theme.border

                            Row {
                                anchors.fill: parent
                                anchors.leftMargin: root.rowHorizontalPadding
                                anchors.rightMargin: root.rowHorizontalPadding
                                spacing: root.rowColumnSpacing

                                Text {
                                    objectName: "maoziPlacementHeaderRank"
                                    text: root.archivedPlacement ? "序号" : "排名"
                                    color: Theme.mutedText
                                    font.pixelSize: Theme.fontCaption
                                    font.bold: true
                                    width: root.placementRankColumnWidth
                                    height: parent.height
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Text {
                                    objectName: "maoziPlacementHeaderHost"
                                    text: "主播"
                                    color: Theme.mutedText
                                    font.pixelSize: Theme.fontCaption
                                    font.bold: true
                                    width: root.placementHostColumnWidth
                                    height: parent.height
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Text {
                                    width: root.teamColumnWidth
                                    height: parent.height
                                    text: "队伍"
                                    color: Theme.mutedText
                                    font.pixelSize: Theme.fontCaption
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Repeater {
                                    model: root.placementColumnList
                                    Text {
                                        objectName: "maoziPlacementHeaderMetric"
                                        width: root.placementMetricWidth
                                        height: parent.height
                                        text: modelData.label
                                        color: Theme.mutedText
                                        font.pixelSize: Theme.fontCaption
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        elide: Text.ElideRight
                                    }
                                }
                                Text {
                                    objectName: "maoziPlacementHeaderAverage"
                                    text: "总评"
                                    color: Theme.mutedText
                                    font.pixelSize: Theme.fontCaption
                                    font.bold: true
                                    width: root.placementAverageWidth
                                    height: parent.height
                                    horizontalAlignment: Text.AlignRight
                                    verticalAlignment: Text.AlignVCenter
                                }
                            }
                        }

                        ListView {
                            id: placementList
                            objectName: "maoziPlacementList"
                            visible: root.activeTab === "placement" && !placementSummary.visible && root.selectedPlacementIndex < 0
                            width: parent.width
                            height: Math.max(0, parent.height - root.tableHeaderHeight)
                            clip: true
                            model: root.visiblePlacementEntries
                            spacing: 0
                            ScrollBar.vertical: ScrollBar {
                                policy: ScrollBar.AsNeeded
                            }

                            delegate: Rectangle {
                                id: placementRow
                                required property var modelData
                                required property int index
                                readonly property var entry: modelData
                                readonly property bool rowHovered: placementHover.hovered
                                width: placementList.width
                                height: root.placementRowHeight
                                radius: 0
                                color: root.rowSurface(index, rowHovered)
                                border.color: activeFocus ? Theme.accent : "transparent"
                                border.width: activeFocus ? 1 : 0
                                activeFocusOnTab: true
                                Accessible.role: Accessible.ListItem
                                Accessible.name: qsTr("第 %1 名，%2").arg(entry.placementRank).arg(String(entry.name || qsTr("未知主播")))

                                function openDetail() {
                                    root.selectedPlacementIndex = index
                                }

                                Keys.onPressed: function(event) {
                                    if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter || event.key === Qt.Key_Space) {
                                        openDetail()
                                        event.accepted = true
                                    }
                                }

                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: 1
                                    color: Theme.border
                                }

                                HoverHandler {
                                    id: placementHover
                                }
                                TapHandler {
                                    onTapped: placementRow.openDetail()
                                }

                                Row {
                                    anchors.fill: parent
                                    anchors.leftMargin: root.rowHorizontalPadding
                                    anchors.rightMargin: root.rowHorizontalPadding
                                    spacing: root.rowColumnSpacing

                                    Text {
                                        objectName: "maoziPlacementCellRank"
                                        width: root.placementRankColumnWidth
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: entry.placementRank
                                        color: entry.placementRank <= 3 ? Theme.accent : Theme.text
                                        font.bold: true
                                        font.pixelSize: Theme.fontControl
                                        horizontalAlignment: Text.AlignHCenter
                                    }
                                    Item {
                                        objectName: "maoziPlacementCellHost"
                                        width: root.placementHostColumnWidth
                                        height: 34
                                        anchors.verticalCenter: parent.verticalCenter
                                        Row {
                                            anchors.fill: parent
                                            spacing: 8
                                            Rectangle {
                                                width: 30
                                                height: 30
                                                radius: 15
                                                anchors.verticalCenter: parent.verticalCenter
                                                color: Theme.well
                                                PortraitImage {
                                                    id: placementAvatar
                                                    objectName: "maoziPlacementAvatarImage"
                                                    anchors.fill: parent
                                                    anchors.margins: 1
                                                    candidates: entry.posterCandidates || []
                                                    asynchronous: true
                                                    sourceSize.width: 30
                                                    sourceSize.height: 30
                                                    fillMode: Image.PreserveAspectCrop
                                                    visible: status === Image.Ready
                                                    clip: true
                                                }
                                                Text {
                                                    anchors.centerIn: parent
                                                    visible: placementAvatar.status !== Image.Ready
                                                    text: String(entry.name || "?").slice(0, 1)
                                                    color: Theme.text
                                                    font.bold: true
                                                }
                                            }
                                            Text {
                                                id: placementHostName
                                                width: parent.width - 38
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: entry.name
                                                color: Theme.text
                                                font.bold: true
                                                font.pixelSize: Theme.fontControl
                                                elide: Text.ElideRight

                                            }
                                        }
                                    }
                                    Text {
                                        width: root.teamColumnWidth
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: entry.teamName || entry.team || "—"
                                        color: Theme.mutedText
                                        font.pixelSize: Theme.fontControl
                                        elide: Text.ElideRight
                                    }
                                    Repeater {
                                        model: root.placementColumnList
                                        Text {
                                            width: root.placementMetricWidth
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: root.placementScoreText(entry, modelData.key)
                                            color: Theme.text
                                            font.pixelSize: Theme.fontCaption
                                            horizontalAlignment: Text.AlignHCenter
                                        }
                                    }
                                    Text {
                                        objectName: "maoziPlacementCellAverage"
                                        width: root.placementAverageWidth
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: root.scoreText(entry.placementAverage)
                                        color: Theme.accent
                                        font.bold: true
                                        font.pixelSize: Theme.fontControl
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }
                        }
                    }
                    Text {
                        anchors.centerIn: parent
                        visible: placementList.count === 0
                        text: root.query.length > 0 ? "没有匹配的定级赛参与者" : "暂无定级赛记录"
                        color: Theme.mutedText
                        font.pixelSize: Theme.fontBody
                    }
                }
                ScrollView {
                    id: placementSummary
                    contentWidth: availableWidth
                    objectName: "maoziPlacementSummary"
                    property string mode: "scores"
                    visible: false
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: placementActions.bottom
                    anchors.topMargin: Theme.gap
                    anchors.bottom: parent.bottom
                    clip: true
                    TextArea {
                        width: parent.width
                        padding: Theme.spaceLarge
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.Wrap
                        text: root.archive.placement && root.archive.placement.scoreSummary ? (placementSummary.mode === "scores" ? root.archive.placement.scoreSummary : root.archive.placement.sessions.map(function (session) {
                                return session.day + " " + session.slot + "\n" + session.text;
                            }).join("\n\n")) : "未记录"
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        background: Rectangle {
                            color: Theme.controlSurface
                        }
                    }
                }

                Column {
                    visible: root.activeTab === "placement" && root.selectedPlacementIndex >= 0
                    anchors.fill: parent
                    spacing: Theme.gap
                    ConsoleToolButton {
                        objectName: "maoziPlacementDetailBack"
                        text: "‹ 返回总览"
                        onClicked: root.selectedPlacementIndex = -1
                    }
                    ScrollView {
                        contentWidth: availableWidth
                        width: parent.width
                        height: Math.max(0, parent.height - Theme.controlHeight - Theme.gap)
                        clip: true
                        Column {
                            id: placementDetailContent
                            objectName: "maoziPlacementParticipantDetail"
                            width: parent.width
                            spacing: 16
                            readonly property var selected: root.visiblePlacementEntries[root.selectedPlacementIndex]
                            readonly property var record: selected ? (root.archive.placement ? root.placementEntries.find(function (candidate) {
                                    return candidate.id === selected.id;
                                }) : selected) || ({}) : ({})
                            readonly property string text: {
                                const visible = root.visiblePlacementEntries[root.selectedPlacementIndex];
                                const entry = visible && root.archive.placement ? root.placementEntries.find(function (candidate) {
                                    return candidate.id === visible.id;
                                }) : visible;
                                if (!entry)
                                    return "";
                                return [entry.name, entry.team || "", entry.tier || "", "总评 " + entry.overall].concat((entry.stats || []).map(function (stat) {
                                    return stat.session + " · " + stat.label + " " + stat.value;
                                })).join("\n");
                            }
                            Rectangle {
                                width: parent.width
                                height: 104
                                radius: Theme.radiusSmall
                                color: Theme.managementSurface
                                border.color: Theme.border
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 16
                                    spacing: 24
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 8
                                        Text {
                                            Layout.fillWidth: true
                                            text: placementDetailContent.record.name || "详情未记录"
                                            color: Theme.text
                                            font.pixelSize: Theme.fontPageTitle
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }
                                        Text {
                                            Layout.fillWidth: true
                                            text: [placementDetailContent.record.team || "未分队", placementDetailContent.record.grade || placementDetailContent.record.tier || "评级未记录"].join(" · ")
                                            color: Theme.mutedText
                                            font.pixelSize: Theme.fontControl
                                            elide: Text.ElideRight
                                        }
                                    }
                                    Column {
                                        spacing: 4
                                        Text {
                                            text: "总评"
                                            color: Theme.mutedText
                                            font.pixelSize: Theme.fontCaption
                                        }
                                        Text {
                                            text: placementDetailContent.record.overall === undefined || placementDetailContent.record.overall === null ? "—" : String(placementDetailContent.record.overall)
                                            color: Theme.accent
                                            font.pixelSize: 28
                                            font.bold: true
                                        }
                                    }
                                }
                            }
                            Text {
                                text: "S1 赛季记录"
                                color: Theme.text
                                font.pixelSize: Theme.fontHeading
                                font.bold: true
                            }
                            GridLayout {
                                width: parent.width
                                columns: width >= 780 ? 3 : 2
                                columnSpacing: 8
                                rowSpacing: 8
                                Repeater {
                                    model: placementDetailContent.record.stats || []
                                    Rectangle {
                                        required property var modelData
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: 64
                                        radius: Theme.radiusSmall
                                        color: Theme.managementSurface
                                        border.color: Theme.border
                                        Column {
                                            anchors.fill: parent
                                            anchors.margins: 12
                                            spacing: 4
                                            Text {
                                                width: parent.width
                                                text: modelData.name || [modelData.session || "", modelData.label || ""].join(" · ")
                                                color: Theme.mutedText
                                                font.pixelSize: Theme.fontControl
                                                elide: Text.ElideRight
                                            }
                                            Text {
                                                text: modelData.value === undefined || modelData.value === null ? "—" : String(modelData.value)
                                                color: Theme.text
                                                font.pixelSize: Theme.fontHeading
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }
                            Text {
                                width: parent.width
                                text: "历史档案成绩 · 缺失数据以 — 显示"
                                color: Theme.mutedText
                                font.pixelSize: Theme.fontCaption
                                wrapMode: Text.Wrap
                            }
                        }
                    }
                }
            }
            SeasonArchivePanel {
                objectName: "regularSeasonPanel"
                visible: root.activeTab === "regularSeason"
                anchors.fill: parent
                matches: root.regularMatches
                overview: root.archive.regularSeason ? root.archive.regularSeason.overview || "" : ""
                query: root.query
                dateFilterEnabled: true
            }

            SeasonArchivePanel {
                objectName: "finalsPanel"
                visible: root.activeTab === "finals"
                anchors.fill: parent
                matches: root.finalMatches
                overview: root.archive.finals ? root.archive.finals.overview || "" : ""
                query: root.query
            }

            SeasonArchivePanel {
                objectName: "footballPanel"
                visible: root.activeTab === "football"
                anchors.fill: parent
                matches: root.footballMatches
                overview: root.archive.football ? root.archive.football.overview || "" : ""
                views: root.archive.football ? root.archive.football.views || ({}) : ({})
                leaderboards: root.archive.football ? root.archive.football.leaderboards || ({}) : ({})
                football: true
                query: root.query
            }
        }
    }
}
