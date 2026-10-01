import QtQuick
import QtQuick.Controls
import ".."

Item {
    id: root

    objectName: "maoziRankPage"
    property var controller: null
    signal backRequested()

    readonly property var rankClient: controller ? controller.maoziRank : null
    readonly property var entries: rankClient ? rankClient.entries : []
    readonly property var placementEntries: rankClient ? rankClient.placementEntries : []
    readonly property var placementColumns: rankClient ? rankClient.placementColumns : ({})
    readonly property var playValueEntries: rankClient ? rankClient.playValueEntries : []
    readonly property var visibleEntries: root.buildVisibleEntries()
    readonly property var visiblePlacementEntries: root.buildVisiblePlacementEntries()
    readonly property var visiblePlayValueEntries: root.buildVisiblePlayValueEntries()
    property string query: ""
    property string liveFilter: "all"
    property string activeTab: "ranking"
    readonly property var placementColumnList: root.buildPlacementColumnList()
    readonly property int rankColumnWidth: 44
    readonly property int hostColumnWidth: 150
    readonly property int hostAvatarWidth: 34
    readonly property int hostAvatarGap: 8
    readonly property int hostTextWidth: hostColumnWidth - hostAvatarWidth - hostAvatarGap
    readonly property int roomColumnWidth: 100
    readonly property int teamColumnWidth: 84
    readonly property int gradeColumnWidth: 64
    readonly property int scoreColumnWidth: 84
    readonly property int votersColumnWidth: 72
    readonly property int statusColumnWidth: 76
    readonly property int rowHorizontalPadding: 12
    readonly property int rowColumnSpacing: 8
    readonly property real tableWidth: rankColumnWidth + hostColumnWidth + roomColumnWidth
                                         + teamColumnWidth + gradeColumnWidth + scoreColumnWidth
                                         + votersColumnWidth + statusColumnWidth
                                         + rowColumnSpacing * 7

    function buildVisibleEntries()
    {
        const result = []
        const normalizedQuery = root.query.trim().toLowerCase()
        for (let index = 0; index < root.entries.length; ++index) {
            const entry = root.entries[index]
            if (root.liveFilter === "live" && !entry.live) continue
            if (root.liveFilter === "offline" && entry.live) continue
            if (normalizedQuery.length > 0) {
                const name = String(entry.name || "").toLowerCase()
                const roomId = String(entry.roomId || "").toLowerCase()
                const note = String(entry.note || "").toLowerCase()
                if (name.indexOf(normalizedQuery) < 0
                        && roomId.indexOf(normalizedQuery) < 0
                        && note.indexOf(normalizedQuery) < 0) {
                    continue
                }
            }
            result.push(entry)
        }
        return result
    }

    function buildPlacementColumnList()
    {
        const result = []
        const columns = root.placementColumns || ({})
        const keys = Object.keys(columns)
        keys.sort()
        for (let index = 0; index < keys.length; ++index) {
            const column = columns[keys[index]]
            if (column) result.push(column)
        }
        return result
    }

    function buildVisiblePlacementEntries()
    {
        const result = []
        const normalizedQuery = root.query.trim().toLowerCase()
        for (let index = 0; index < root.placementEntries.length; ++index) {
            const entry = root.placementEntries[index]
            if (normalizedQuery.length > 0) {
                const name = String(entry.name || "").toLowerCase()
                const roomId = String(entry.roomId || "").toLowerCase()
                if (name.indexOf(normalizedQuery) < 0 && roomId.indexOf(normalizedQuery) < 0) {
                    continue
                }
            }
            result.push(entry)
        }
        return result
    }

    function buildVisiblePlayValueEntries()
    {
        const result = []
        const normalizedQuery = root.query.trim().toLowerCase()
        for (let index = 0; index < root.playValueEntries.length; ++index) {
            const entry = root.playValueEntries[index]
            if (normalizedQuery.length > 0) {
                const name = String(entry.name || "").toLowerCase()
                const roomId = String(entry.roomId || "").toLowerCase()
                if (name.indexOf(normalizedQuery) < 0 && roomId.indexOf(normalizedQuery) < 0) {
                    continue
                }
            }
            result.push(entry)
        }
        return result
    }

    function roleLabel(role)
    {
        if (role === "leader") return "团长"
        if (role === "captain") return "队长"
        if (role === "member") return "队员"
        return "其他"
    }

    function placementScoreText(entry, key)
    {
        if (!entry || !entry.placementSessions) return "—"
        const value = entry.placementSessions[key]
        return value === undefined || value === null ? "—" : Number(value).toFixed(1)
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.canvas
    }

    Column {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 12

        Item {
            id: toolbar
            objectName: "maoziToolbar"
            width: parent.width
            height: 44

            ToolButton {
                objectName: "maoziBackButton"
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
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
                    font.pixelSize: 26
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: parent.hovered ? Theme.controlSurface : "transparent"
                }
            }

            Item {
                id: titleColumn
                objectName: "maoziTitleColumn"
                anchors.left: parent.left
                anchors.leftMargin: 52
                anchors.verticalCenter: parent.verticalCenter
                width: Math.max(0, searchField.x - x - 12)
                height: 42

                Text {
                    objectName: "maoziTitle"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    height: 26
                    verticalAlignment: Text.AlignVCenter
                    text: "郎团S1野榜"
                    color: Theme.text
                    font.bold: true
                    font.pixelSize: 20
                    elide: Text.ElideRight
                }
                Text {
                    objectName: "maoziSubtitle"
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 16
                    text: root.rankClient ? root.rankClient.statusText : "暂无野榜数据"
                    color: Theme.mutedText
                    font.pixelSize: 11
                    elide: Text.ElideRight
                }
            }

            TextField {
                id: searchField
                objectName: "maoziSearchField"
                anchors.right: liveFilterBox.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 180
                height: Theme.controlHeight
                placeholderText: "搜索主播 / 房间号"
                color: Theme.text
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: Theme.well
                    border.color: searchField.activeFocus ? Theme.accent : Theme.border
                }
                onTextChanged: root.query = text
            }

            ComboBox {
                id: liveFilterBox
                objectName: "maoziLiveFilter"
                anchors.right: openSiteButton.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 90
                height: Theme.controlHeight
                model: [
                    { value: "all", label: "全部" },
                    { value: "live", label: "直播中" },
                    { value: "offline", label: "未开播" }
                ]
                textRole: "label"
                valueRole: "value"
                onActivated: root.liveFilter = currentValue
            }

            Button {
                id: openSiteButton
                objectName: "maoziOpenSiteButton"
                anchors.right: refreshButton.left
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                text: "打开野榜"
                height: Theme.controlHeight
                enabled: root.controller !== null
                onClicked: {
                    if (!root.controller) return
                    if (!root.controller.openExternalUrl("https://dy656750-39nb2xg.maozi.io/"))
                        console.warn("failed to open Windows rank site")
                }
            }

            Button {
                id: refreshButton
                objectName: "maoziRefreshButton"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: "刷新"
                height: Theme.controlHeight
                enabled: root.rankClient && !root.rankClient.loading
                onClicked: if (root.rankClient) root.rankClient.refresh()
            }
        }

        Row {
            id: tabBar
            objectName: "maoziTabBar"
            width: parent.width
            height: Theme.controlHeight
            spacing: 8

            Button {
                objectName: "maoziRankingTab"
                text: "综合排行"
                checkable: true
                checked: root.activeTab === "ranking"
                height: Theme.controlHeight
                onClicked: root.activeTab = "ranking"
            }
            Button {
                objectName: "maoziPlacementTab"
                text: "定级赛"
                checkable: true
                checked: root.activeTab === "placement"
                height: Theme.controlHeight
                onClicked: root.activeTab = "placement"
            }
            Button {
                objectName: "maoziPlayValueTab"
                text: "游乐值榜"
                checkable: true
                checked: root.activeTab === "playValue"
                height: Theme.controlHeight
                onClicked: root.activeTab = "playValue"
            }
        }

        Rectangle {
            id: headerBar
            objectName: "maoziRankingHeaderBar"
            visible: root.activeTab === "ranking"
            width: parent.width
            height: 32
            radius: Theme.radiusSmall
            color: Theme.controlSurface
            border.color: Theme.border

            Row {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text { objectName: "maoziHeaderRank"; width: root.rankColumnWidth; text: "排名"; color: Theme.mutedText; font.pixelSize: 11; height: headerBar.height; verticalAlignment: Text.AlignVCenter }
                Text { objectName: "maoziHeaderHost"; width: root.hostColumnWidth; text: "主播"; color: Theme.mutedText; font.pixelSize: 11; height: headerBar.height; verticalAlignment: Text.AlignVCenter }
                Text { objectName: "maoziHeaderRoom"; width: root.roomColumnWidth; text: "房间号"; color: Theme.mutedText; font.pixelSize: 11; height: headerBar.height; verticalAlignment: Text.AlignVCenter }
                Text { objectName: "maoziHeaderTeam"; width: root.teamColumnWidth; text: "队伍"; color: Theme.mutedText; font.pixelSize: 11; height: headerBar.height; verticalAlignment: Text.AlignVCenter }
                Text { objectName: "maoziHeaderGrade"; width: root.gradeColumnWidth; text: "评级"; color: Theme.mutedText; font.pixelSize: 11; height: headerBar.height; verticalAlignment: Text.AlignVCenter }
                Text { objectName: "maoziHeaderScore"; width: root.scoreColumnWidth; text: "综合评分"; color: Theme.mutedText; font.pixelSize: 11; height: headerBar.height; verticalAlignment: Text.AlignVCenter }
                Text { objectName: "maoziHeaderVoters"; width: root.votersColumnWidth; text: "人数"; color: Theme.mutedText; font.pixelSize: 11; height: headerBar.height; verticalAlignment: Text.AlignVCenter }
                Text { objectName: "maoziHeaderStatus"; width: root.statusColumnWidth; text: "状态"; color: Theme.mutedText; font.pixelSize: 11; height: headerBar.height; verticalAlignment: Text.AlignVCenter }
            }
        }

        Item {
            objectName: "maoziListWidthProbe"
            width: 1
            height: 1
            property real tableWidth: root.tableWidth
            property real contentWidth: rankList.width
        }

        ListView {
            id: rankList
            objectName: "maoziRankList"
            visible: root.activeTab === "ranking"
            width: parent.width
            height: parent.height - 156
            clip: true
            model: root.visibleEntries
            spacing: 3
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: rankRow
                required property var modelData
                required property int index
                readonly property var entry: modelData
                width: rankList.width
                height: 54
                radius: Theme.radiusSmall
                color: index % 2 === 0 ? Theme.controlSurface : Theme.well
                border.color: entry.live ? Theme.online : "transparent"
                border.width: entry.live ? 1 : 0

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 8

                    Text {
                        objectName: "maoziCellRank"
                        width: root.rankColumnWidth
                        anchors.verticalCenter: parent.verticalCenter
                        text: entry.rank
                        color: entry.rank <= 3 ? Theme.accent : Theme.text
                        font.bold: true
                        font.pixelSize: 14
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

                            Image {
                                id: avatarImage
                                objectName: "maoziAvatarImage"
                                anchors.fill: parent
                                anchors.margins: 1
                                source: entry.posterUrl
                                sourceSize.width: root.hostAvatarWidth
                                sourceSize.height: root.hostAvatarWidth
                                asynchronous: true
                                fillMode: Image.PreserveAspectCrop
                                visible: status === Image.Ready
                                clip: true
                                onStatusChanged: {
                                    if (status === Image.Error) {
                                        console.warn("maozi avatar failed:", source, errorString)
                                    }
                                }
                            }
                            Text {
                                anchors.centerIn: parent
                                visible: !entry.posterUrl || entry.posterUrl.length === 0
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
                                width: parent.width
                                text: entry.name
                                color: Theme.text
                                font.bold: true
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                            Text {
                                width: parent.width
                                text: entry.note
                                color: Theme.mutedText
                                font.pixelSize: 10
                                elide: Text.ElideRight
                            }
                        }
                    }
                    Text { objectName: "maoziCellRoom"; width: root.roomColumnWidth; anchors.verticalCenter: parent.verticalCenter; text: entry.roomId; color: Theme.mutedText; font.pixelSize: 11 }
                    Text { objectName: "maoziCellTeam"; width: root.teamColumnWidth; anchors.verticalCenter: parent.verticalCenter; text: entry.teamName; color: Theme.mutedText; font.pixelSize: 11 }
                    Rectangle {
                        objectName: "maoziCellGrade"
                        width: root.gradeColumnWidth
                        height: 22
                        radius: Theme.radiusSmall
                        anchors.verticalCenter: parent.verticalCenter
                        color: entry.gradeColor
                        Text {
                            anchors.centerIn: parent
                            text: entry.grade
                            color: "#11151b"
                            font.bold: true
                            font.pixelSize: 11
                        }
                    }
                    Text {
                        objectName: "maoziCellScore"
                        width: root.scoreColumnWidth
                        anchors.verticalCenter: parent.verticalCenter
                        text: entry.score >= 0 ? entry.score.toFixed(1) : "—"
                        color: Theme.text
                        font.bold: true
                        font.pixelSize: 12
                    }
                    Text { objectName: "maoziCellVoters"; width: root.votersColumnWidth; anchors.verticalCenter: parent.verticalCenter; text: entry.voters; color: Theme.mutedText; font.pixelSize: 11 }
                    Text {
                        objectName: "maoziCellStatus"
                        width: root.statusColumnWidth
                        anchors.verticalCenter: parent.verticalCenter
                        text: entry.live ? "直播中" : "未开播"
                        color: entry.live ? Theme.online : Theme.mutedText
                        font.pixelSize: 11
                    }
                }
            }
        }

        Rectangle {
            objectName: "maoziPlacementHeaderBar"
            visible: root.activeTab === "placement"
            width: parent.width
            height: 32
            radius: Theme.radiusSmall
            color: Theme.controlSurface
            border.color: Theme.border

            Row {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text { text: "排名"; color: Theme.mutedText; font.pixelSize: 11; width: 44; height: parent.height; verticalAlignment: Text.AlignVCenter }
                Text { text: "主播"; color: Theme.mutedText; font.pixelSize: 11; width: 140; height: parent.height; verticalAlignment: Text.AlignVCenter }
                Repeater {
                    model: root.placementColumnList
                    Text {
                        width: 68
                        height: parent.height
                        text: modelData.label
                        color: Theme.mutedText
                        font.pixelSize: 11
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }
                Text { text: "总评"; color: Theme.mutedText; font.pixelSize: 11; width: 72; height: parent.height; verticalAlignment: Text.AlignVCenter }
            }
        }

        ListView {
            id: placementList
            objectName: "maoziPlacementList"
            visible: root.activeTab === "placement"
            width: parent.width
            height: parent.height - 156
            clip: true
            model: root.visiblePlacementEntries
            spacing: 3
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                required property var modelData
                required property int index
                readonly property var entry: modelData
                width: placementList.width
                height: 46
                radius: Theme.radiusSmall
                color: index % 2 === 0 ? Theme.controlSurface : Theme.well

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 8

                    Text {
                        objectName: "maoziPlacementCellRank"
                        width: 44
                        anchors.verticalCenter: parent.verticalCenter
                        text: entry.placementRank
                        color: entry.placementRank <= 3 ? Theme.accent : Theme.text
                        font.bold: true
                        font.pixelSize: 13
                    }
                    Item {
                        objectName: "maoziPlacementCellHost"
                        width: 140
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
                                Image {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    source: entry.posterUrl
                                    sourceSize.width: 30
                                    sourceSize.height: 30
                                    fillMode: Image.PreserveAspectCrop
                                    visible: status === Image.Ready
                                    clip: true
                                }
                            }
                            Text {
                                width: parent.width - 38
                                anchors.verticalCenter: parent.verticalCenter
                                text: entry.name
                                color: Theme.text
                                font.bold: true
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                        }
                    }
                    Repeater {
                        model: root.placementColumnList
                        Text {
                            width: 68
                            anchors.verticalCenter: parent.verticalCenter
                            text: root.placementScoreText(entry, modelData.key)
                            color: Theme.text
                            font.pixelSize: 11
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                    Text {
                        objectName: "maoziPlacementCellAverage"
                        width: 72
                        anchors.verticalCenter: parent.verticalCenter
                        text: Number(entry.placementAverage).toFixed(1)
                        color: Theme.accent
                        font.bold: true
                        font.pixelSize: 12
                    }
                }
            }
        }

        Rectangle {
            objectName: "maoziPlayValueHeaderBar"
            visible: root.activeTab === "playValue"
            width: parent.width
            height: 32
            radius: Theme.radiusSmall
            color: Theme.controlSurface
            border.color: Theme.border

            Row {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text { text: "排名"; color: Theme.mutedText; font.pixelSize: 11; width: 44; height: parent.height; verticalAlignment: Text.AlignVCenter }
                Text { text: "主播"; color: Theme.mutedText; font.pixelSize: 11; width: 150; height: parent.height; verticalAlignment: Text.AlignVCenter }
                Text { text: "房间号"; color: Theme.mutedText; font.pixelSize: 11; width: 100; height: parent.height; verticalAlignment: Text.AlignVCenter }
                Text { text: "队伍"; color: Theme.mutedText; font.pixelSize: 11; width: 84; height: parent.height; verticalAlignment: Text.AlignVCenter }
                Text { text: "角色"; color: Theme.mutedText; font.pixelSize: 11; width: 64; height: parent.height; verticalAlignment: Text.AlignVCenter }
                Text { text: "游乐值"; color: Theme.mutedText; font.pixelSize: 11; width: 100; height: parent.height; verticalAlignment: Text.AlignVCenter }
                Text { text: "状态"; color: Theme.mutedText; font.pixelSize: 11; width: 76; height: parent.height; verticalAlignment: Text.AlignVCenter }
            }
        }

        ListView {
            id: playValueList
            objectName: "maoziPlayValueList"
            visible: root.activeTab === "playValue"
            width: parent.width
            height: parent.height - 156
            clip: true
            model: root.visiblePlayValueEntries
            spacing: 3
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                required property var modelData
                required property int index
                readonly property var entry: modelData
                width: playValueList.width
                height: 50
                radius: Theme.radiusSmall
                color: index % 2 === 0 ? Theme.controlSurface : Theme.well

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 8

                    Text { width: 44; anchors.verticalCenter: parent.verticalCenter; text: entry.rank; color: entry.rank <= 3 ? Theme.accent : Theme.text; font.bold: true; font.pixelSize: 13 }
                    Item {
                        width: 150
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
                                Image {
                                    anchors.fill: parent
                                    anchors.margins: 1
                                    source: entry.posterUrl
                                    sourceSize.width: 30
                                    sourceSize.height: 30
                                    fillMode: Image.PreserveAspectCrop
                                    visible: status === Image.Ready
                                    clip: true
                                }
                            }
                            Text {
                                width: parent.width - 38
                                anchors.verticalCenter: parent.verticalCenter
                                text: entry.name
                                color: Theme.text
                                font.bold: true
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                        }
                    }
                    Text { width: 100; anchors.verticalCenter: parent.verticalCenter; text: entry.roomId || "—"; color: Theme.mutedText; font.pixelSize: 11 }
                    Text { width: 84; anchors.verticalCenter: parent.verticalCenter; text: entry.teamName || "未分队"; color: Theme.mutedText; font.pixelSize: 11; elide: Text.ElideRight }
                    Text { width: 64; anchors.verticalCenter: parent.verticalCenter; text: root.roleLabel(entry.role); color: Theme.mutedText; font.pixelSize: 11 }
                    Text { width: 100; anchors.verticalCenter: parent.verticalCenter; text: Number(entry.points).toFixed(1); color: Theme.accent; font.bold: true; font.pixelSize: 12 }
                    Text { width: 76; anchors.verticalCenter: parent.verticalCenter; text: entry.live ? "直播中" : "未开播"; color: entry.live ? Theme.online : Theme.mutedText; font.pixelSize: 11 }
                }
            }
        }
    }
}
