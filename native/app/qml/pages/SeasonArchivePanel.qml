import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import ".."
import "../controls"
import "ArchiveDetailModel.js" as Archive

Item {
    id: root
    property var matches: []
    property string overview: ""
    property string query: ""
    property bool football: false
    property bool dateFilterEnabled: false
    property var leaderboards: ({})
    property string leaderboardView: "goals"
    property var views: ({})
    property int pageIndex: 0
    property int selectedIndex: -1
    property string summaryView: "matches"
    readonly property var dates: Array.from(new Set(root.matches.map(function (match) {
        return String(match.date || "");
    }).filter(function (date) {
        return date.length > 0;
    })))
    readonly property var visibleMatches: root.matches.filter(function (match) {
        const date = dateFilter.currentIndex > 0 ? root.dates[dateFilter.currentIndex - 1] : "";
        return (!root.dateFilterEnabled || !date || match.date === date) && (root.query.trim().length === 0 || String(match.title || match.summary || "").toLowerCase().indexOf(root.query.trim().toLowerCase()) >= 0);
    })
    onQueryChanged: {
        pageIndex = 0;
        selectedIndex = -1;
    }
    readonly property var currentMatch: selectedIndex >= 0 && selectedIndex < visibleMatches.length ? visibleMatches[selectedIndex] : null
    readonly property string displayedSummary: root.summaryView === "overview" && !root.football ? root.overview : String(root.views[root.summaryView === "overview" ? "stand" : root.summaryView] || "未记录")
    readonly property var visibleLeaders: root.leaderboards[root.leaderboardView] || []

    function detailText(match) {
        if (!match)
            return "";
        if (!root.football)
            return String(match.detail || "未记录");
        const detail = match.detail || ({});
        const sections = detail.sections || [];
        return [detail.date || "", detail.title || "", detail.note || ""].concat(sections.map(function (section) {
            return section.text || "";
        })).filter(function (text) {
            return text.length > 0;
        }).join("\n\n");
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            ConsoleToolButton {
                objectName: "seasonArchiveBack"
                visible: root.selectedIndex >= 0 || root.summaryView !== "matches"
                Layout.preferredWidth: Theme.controlHeight
                text: "‹"
                Accessible.name: "返回赛程"
                onClicked: {
                    root.selectedIndex = -1;
                    root.summaryView = "matches";
                }
            }
            Text {
                Layout.fillWidth: true
                text: root.selectedIndex >= 0 && root.currentMatch ? Archive.matchTitle(root.currentMatch, root.football) : root.visibleMatches.length + " 场记录"
                color: Theme.text
                font.pixelSize: Theme.fontHeading
                font.bold: true
                wrapMode: Text.Wrap
            }
            ConsoleComboBox {
                id: dateFilter
                objectName: "seasonArchiveDateFilter"
                visible: root.dateFilterEnabled && root.selectedIndex < 0
                Layout.preferredWidth: 180
                model: ["全部日期"].concat(root.dates)
                Accessible.name: "赛程日期"
                onCurrentIndexChanged: {
                    root.pageIndex = 0;
                    root.selectedIndex = -1;
                }
            }
        }

        Row {
            visible: root.selectedIndex < 0
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.controlHeight
            spacing: 4
            ConsoleToolButton {
                text: "赛程"
                checkable: true
                checked: root.summaryView === "matches"
                onClicked: root.summaryView = "matches"
            }
            ConsoleToolButton {
                text: root.football ? "积分" : "总览"
                checkable: true
                checked: root.summaryView === "overview"
                onClicked: root.summaryView = "overview"
            }
            ConsoleToolButton {
                visible: root.football
                text: "赛树"
                checkable: true
                checked: root.summaryView === "tree"
                onClicked: root.summaryView = "tree"
            }
            ConsoleToolButton {
                visible: root.football
                objectName: "seasonArchiveLeadersTab"
                text: "球员榜"
                checkable: true
                checked: root.summaryView === "leaders"
                onClicked: root.summaryView = "leaders"
            }
        }

        Row {
            visible: root.football && root.summaryView === "leaders" && root.selectedIndex < 0
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.controlHeight
            spacing: 4
            ConsoleToolButton {
                text: "射手榜"
                checkable: true
                checked: root.leaderboardView === "goals"
                onClicked: root.leaderboardView = "goals"
            }
            ConsoleToolButton {
                objectName: "seasonArchiveAssistsTab"
                text: "助攻榜"
                checkable: true
                checked: root.leaderboardView === "assists"
                onClicked: root.leaderboardView = "assists"
            }
            ConsoleToolButton {
                text: "MVP 榜"
                checkable: true
                checked: root.leaderboardView === "mvp"
                onClicked: root.leaderboardView = "mvp"
            }
        }

        ListView {
            id: leaderList
            objectName: "seasonArchiveLeaderList"
            visible: root.football && root.summaryView === "leaders" && root.selectedIndex < 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.visibleLeaders
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
            header: Rectangle {
                width: leaderList.width
                height: 36
                color: Theme.managementSurface
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 12
                    Text {
                        Layout.preferredWidth: 48
                        text: "名次"
                        color: Theme.mutedText
                        font.pixelSize: Theme.fontCaption
                    }
                    Text {
                        Layout.fillWidth: true
                        text: "球员"
                        color: Theme.mutedText
                        font.pixelSize: Theme.fontCaption
                    }
                    Text {
                        Layout.preferredWidth: 120
                        text: "队伍"
                        color: Theme.mutedText
                        font.pixelSize: Theme.fontCaption
                    }
                    Text {
                        Layout.preferredWidth: 80
                        text: "记录"
                        color: Theme.mutedText
                        font.pixelSize: Theme.fontCaption
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }
            delegate: Rectangle {
                required property var modelData
                required property int index
                width: leaderList.width
                height: 48
                color: index % 2 === 0 ? Theme.managementSurface : Theme.canvas
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width
                    height: 1
                    color: Theme.border
                }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12
                    spacing: 12
                    Text {
                        Layout.preferredWidth: 48
                        text: modelData.rank
                        color: Theme.accent
                        font.bold: true
                        font.pixelSize: Theme.fontBody
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData.name
                        color: Theme.text
                        elide: Text.ElideRight
                        font.pixelSize: Theme.fontBody
                    }
                    Text {
                        Layout.preferredWidth: 120
                        text: modelData.team
                        color: Theme.mutedText
                        elide: Text.ElideRight
                        font.pixelSize: Theme.fontControl
                    }
                    Text {
                        Layout.preferredWidth: 80
                        text: modelData.value
                        color: Theme.text
                        font.bold: true
                        font.pixelSize: Theme.fontBody
                        horizontalAlignment: Text.AlignRight
                    }
                }
            }
            Text {
                anchors.centerIn: parent
                visible: leaderList.count === 0
                text: "暂无该项球员记录"
                color: Theme.mutedText
                font.pixelSize: Theme.fontBody
            }
        }

        Item {
            objectName: "seasonArchiveDetail"
            readonly property string text: root.selectedIndex >= 0 ? root.detailText(root.currentMatch) : root.displayedSummary
            visible: root.selectedIndex >= 0
            Layout.fillWidth: true
            Layout.fillHeight: true
            Loader {
                anchors.fill: parent
                active: root.selectedIndex >= 0
                sourceComponent: root.football ? footballDetails : seasonDetails
            }
            Component {
                id: seasonDetails
                SeasonMatchDetail { matchRecord: root.currentMatch }
            }
            Component {
                id: footballDetails
                FootballMatchDetail { matchRecord: root.currentMatch }
            }
        }

        ScrollView {
            contentWidth: availableWidth
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            visible: root.selectedIndex < 0 && root.summaryView !== "matches" && root.summaryView !== "leaders"
            TextArea {
                objectName: "seasonArchiveSummary"
                width: parent.width
                padding: 16
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.Wrap
                text: root.displayedSummary
                color: Theme.text
                font.pixelSize: Theme.fontBody
                background: Rectangle {
                    color: Theme.managementSurface
                    radius: Theme.radiusSmall
                    border.color: Theme.border
                }
            }
        }

        ListView {
            id: matchList
            objectName: root.football ? "maoziFootballList" : (root.objectName === "regularSeasonPanel" ? "maoziRegularSeasonList" : "maoziFinalsList")
            visible: root.selectedIndex < 0 && root.summaryView === "matches"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 0
            model: root.visibleMatches.slice(root.pageIndex * 10, root.pageIndex * 10 + 10)
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
            delegate: ConsoleButton {
                id: matchRow
                required property var modelData
                required property int index
                width: matchList.width
                height: Math.max(64, matchTitle.implicitHeight + 32)
                text: Archive.matchTitle(modelData, root.football)
                onClicked: root.selectedIndex = root.pageIndex * 10 + index
                Keys.onReturnPressed: function(event) {
                    root.selectedIndex = root.pageIndex * 10 + index
                    event.accepted = true
                }
                Keys.onEnterPressed: function(event) {
                    root.selectedIndex = root.pageIndex * 10 + index
                    event.accepted = true
                }
                Keys.onSpacePressed: function(event) {
                    root.selectedIndex = root.pageIndex * 10 + index
                    event.accepted = true
                }
                background: Rectangle {
                    color: matchRow.hovered ? Theme.controlHover : (matchRow.index % 2 === 0 ? Theme.managementSurface : Theme.canvas)
                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: Theme.border
                    }
                    border.width: matchRow.activeFocus ? 2 : 0
                    border.color: Theme.accent
                }
                contentItem: RowLayout {
                    spacing: 16
                    Text {
                        Layout.preferredWidth: 96
                        text: String(matchRow.modelData.date || (root.football && matchRow.modelData.detail ? matchRow.modelData.detail.date : "") || "日期未记录")
                        color: Theme.mutedText
                        font.pixelSize: Theme.fontControl
                        wrapMode: Text.Wrap
                    }
                    Text {
                        id: matchTitle
                        Layout.fillWidth: true
                        text: Archive.matchTitle(matchRow.modelData, root.football)
                        color: Theme.text
                        font.pixelSize: Theme.fontBody
                        wrapMode: Text.Wrap
                    }
                    Text {
                        text: "查看详情  ›"
                        color: Theme.accent
                        font.pixelSize: Theme.fontControl
                    }
                }
            }
            Text {
                anchors.centerIn: parent
                visible: matchList.count === 0
                text: root.query.trim().length > 0 ? "没有匹配的赛程，试试其他关键词" : "暂无赛程记录"
                color: Theme.mutedText
                font.pixelSize: Theme.fontBody
            }
        }

        RowLayout {
            visible: root.selectedIndex < 0 && root.summaryView === "matches"
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.controlHeight
            spacing: 8
            Text {
                Layout.fillWidth: true
                text: "每页 10 场 · 共 " + root.visibleMatches.length + " 场"
                color: Theme.mutedText
                font.pixelSize: Theme.fontCaption
            }
            ConsoleToolButton {
                objectName: "seasonArchivePreviousPage"
                text: "‹"
                Accessible.name: "上一页"
                Layout.preferredWidth: Theme.controlHeight
                enabled: root.pageIndex > 0
                onClicked: --root.pageIndex
            }
            Text {
                color: Theme.mutedText
                font.pixelSize: Theme.fontControl
                text: (root.pageIndex + 1) + " / " + Math.max(1, Math.ceil(root.visibleMatches.length / 10))
            }
            ConsoleToolButton {
                objectName: "seasonArchiveNextPage"
                text: "›"
                Accessible.name: "下一页"
                Layout.preferredWidth: Theme.controlHeight
                enabled: root.pageIndex + 1 < Math.ceil(root.visibleMatches.length / 10)
                onClicked: ++root.pageIndex
            }
        }
    }
}
