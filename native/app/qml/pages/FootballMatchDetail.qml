pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import ".."
import "../controls"
import "ArchiveDetailModel.js" as Archive

ScrollView {
    id: root
    property var matchRecord: null
    readonly property var record: Archive.football(matchRecord)
    property bool showSource: false
    contentWidth: availableWidth
    clip: true
    onMatchRecordChanged: {
        showSource = false
    }

    Column {
        width: root.availableWidth
        spacing: 16

        Rectangle {
            objectName: "footballScoreboard"
            width: parent.width
            height: scoreContent.implicitHeight + 32
            color: Theme.managementSurface
            radius: Theme.radiusSmall
            border.color: Theme.border
            Column {
                id: scoreContent
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 12
                Text { width: parent.width; text: [root.record.code, root.record.kickoff, root.record.status].filter(Boolean).join(" · "); color: Theme.mutedText; font.pixelSize: Theme.fontControl; horizontalAlignment: Text.AlignHCenter }
                RowLayout {
                    width: parent.width
                    spacing: 24
                    Text { Layout.fillWidth: true; Layout.preferredWidth: 1; text: root.record.home; color: Theme.text; font.pixelSize: Theme.fontPageTitle; font.bold: true; horizontalAlignment: Text.AlignRight }
                    Text { objectName: "footballScoreText"; text: root.record.score; color: Theme.accent; font.pixelSize: 36; font.bold: true }
                    Text { Layout.fillWidth: true; Layout.preferredWidth: 1; text: root.record.away; color: Theme.text; font.pixelSize: Theme.fontPageTitle; font.bold: true }
                }
                Text { width: parent.width; text: root.record.outcome; color: Theme.accent; font.pixelSize: Theme.fontBody; horizontalAlignment: Text.AlignHCenter }
            }
        }

        Text { text: qsTr("首发阵容"); color: Theme.text; font.pixelSize: Theme.fontHeading; font.bold: true }
        RowLayout {
            width: parent.width
            spacing: 12
            Repeater {
                model: root.record.lineups
                delegate: Rectangle {
                    id: lineup
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: lineupContent.implicitHeight + 32
                    color: Theme.managementSurface
                    radius: Theme.radiusSmall
                    border.color: Theme.border
                    Column {
                        id: lineupContent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 16
                        spacing: 10
                        Text { text: lineup.modelData.team; color: Theme.text; font.pixelSize: Theme.fontBody; font.bold: true }
                        Repeater {
                            model: lineup.modelData.players
                            delegate: RowLayout {
                                id: player
                                required property var modelData
                                objectName: "footballLineupPlayer"
                                width: lineupContent.width
                                spacing: 12
                                Rectangle {
                                    Layout.preferredWidth: 28
                                    Layout.preferredHeight: 28
                                    color: Theme.selectedSurface
                                    radius: Theme.radiusSmall
                                    Text { anchors.centerIn: parent; text: player.modelData.position; color: Theme.accent; font.pixelSize: Theme.fontControl }
                                }
                                Text { Layout.fillWidth: true; text: player.modelData.name; color: Theme.text; font.pixelSize: Theme.fontControl; elide: Text.ElideRight }
                            }
                        }
                    }
                }
            }
        }
        Text { width: parent.width; text: qsTr("阵容名称按原始档案展示；省略号为来源中已有的截断。"); color: Theme.mutedText; font.pixelSize: Theme.fontCaption; wrapMode: Text.Wrap }

        Text { text: qsTr("比赛事件"); color: Theme.text; font.pixelSize: Theme.fontHeading; font.bold: true }
        Column {
            width: parent.width
            spacing: 1
            Repeater {
                model: root.record.events
                delegate: Rectangle {
                    id: event
                    required property var modelData
                    required property int index
                    objectName: "footballEventRow"
                    width: parent.width
                    height: Math.max(44, eventText.implicitHeight + 20)
                    color: index % 2 === 0 ? Theme.managementSurface : Theme.canvas
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 12
                        Text { Layout.preferredWidth: 62; text: event.modelData.time; color: Theme.accent; font.pixelSize: Theme.fontControl; font.bold: true }
                        Text { Layout.preferredWidth: 24; text: event.modelData.icon; color: Theme.text; font.pixelSize: Theme.fontBody }
                        Text { id: eventText; Layout.fillWidth: true; text: event.modelData.text; color: Theme.text; font.pixelSize: Theme.fontControl; wrapMode: Text.Wrap }
                    }
                }
            }
        }
        Text { width: parent.width; visible: text.length > 0; text: root.record.eventNote; color: Theme.mutedText; font.pixelSize: Theme.fontCaption; wrapMode: Text.Wrap }

        RowLayout {
            width: parent.width
            Text { Layout.fillWidth: true; text: qsTr("本场 MVP 记录"); color: Theme.text; font.pixelSize: Theme.fontHeading; font.bold: true }
            Text { text: root.record.voteNote; color: Theme.mutedText; font.pixelSize: Theme.fontControl }
        }
        Text { width: parent.width; text: qsTr("历史投票结果 · 此处仅供查阅"); color: Theme.mutedText; font.pixelSize: Theme.fontCaption }
        GridLayout {
            width: parent.width
            columns: 2
            columnSpacing: 12
            rowSpacing: 0
            Repeater {
                model: root.record.votes
                delegate: Rectangle {
                    id: voter
                    required property var modelData
                    objectName: "footballMvpRow"
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 44
                    color: Theme.managementSurface
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 8
                        Text { Layout.preferredWidth: 34; text: voter.modelData.team; color: Theme.mutedText; font.pixelSize: Theme.fontCaption }
                        Text { Layout.preferredWidth: 18; text: voter.modelData.position; color: Theme.mutedText; font.pixelSize: Theme.fontCaption }
                        Text { Layout.fillWidth: true; text: voter.modelData.name; color: Theme.text; font.pixelSize: Theme.fontControl; elide: Text.ElideRight }
                        Text { text: voter.modelData.votes + qsTr(" 票"); color: Theme.accent; font.pixelSize: Theme.fontControl; font.bold: true }
                    }
                }
            }
        }

        Text { width: parent.width; text: root.record.note; color: Theme.mutedText; font.pixelSize: Theme.fontCaption; wrapMode: Text.Wrap }
        Repeater {
            model: root.record.extra
            delegate: Text {
                required property var modelData
                width: parent.width
                text: modelData.title + "\n" + modelData.text
                color: Theme.mutedText
                font.pixelSize: Theme.fontControl
                wrapMode: Text.Wrap
            }
        }
        ConsoleButton {
            objectName: "footballArchiveSourceToggle"
            text: root.showSource ? qsTr("收起原始档案") : qsTr("查看完整原始档案")
            onClicked: root.showSource = !root.showSource
        }
        TextArea {
            objectName: "footballArchiveSourceText"
            width: parent.width
            visible: root.showSource
            text: root.record.raw || qsTr("暂无详细记录")
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.Wrap
            color: Theme.mutedText
            font.pixelSize: Theme.fontControl
            padding: 16
            background: Rectangle { color: Theme.managementSurface; border.color: Theme.border }
        }
    }
}
