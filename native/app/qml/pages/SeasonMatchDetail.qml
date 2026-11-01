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
    readonly property var record: Archive.season(matchRecord)
    property bool showSource: false
    property bool showRules: false
    contentWidth: availableWidth
    clip: true
    onRecordChanged: {
        showSource = false
        showRules = record.standings.length === 0 && record.rounds.length === 0
    }

    Column {
        width: root.availableWidth
        spacing: 16

        Rectangle {
            width: parent.width
            height: summary.implicitHeight + 32
            color: Theme.managementSurface
            radius: Theme.radiusSmall
            border.color: Theme.border
            Column {
                id: summary
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 16
                spacing: 8
                Text { text: root.record.date; color: Theme.mutedText; font.pixelSize: Theme.fontControl }
                Text {
                    objectName: "seasonMatchOutcome"
                    width: parent.width
                    text: root.record.outcome || root.record.title
                    color: root.record.outcome ? Theme.accent : Theme.text
                    font.pixelSize: Theme.fontHeading
                    font.bold: true
                    wrapMode: Text.Wrap
                }
                Text {
                    width: parent.width
                    text: qsTr("历史赛程 · 成绩与名次按原始档案展示")
                    color: Theme.mutedText
                    font.pixelSize: Theme.fontCaption
                    wrapMode: Text.Wrap
                }
            }
        }

        Column {
            width: parent.width
            spacing: 8
            visible: root.record.standings.length > 0
            Text { text: root.record.standingsTitle; color: Theme.text; font.pixelSize: Theme.fontHeading; font.bold: true }
            Text {
                width: parent.width
                visible: text.length > 0
                text: root.record.standingsNote
                color: Theme.mutedText
                font.pixelSize: Theme.fontControl
                wrapMode: Text.Wrap
            }
            Rectangle {
                width: parent.width
                height: 32
                color: Theme.controlSurface
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 16
                    Text { Layout.preferredWidth: 48; text: qsTr("名次"); color: Theme.mutedText; font.pixelSize: Theme.fontCaption }
                    Text { Layout.preferredWidth: 88; text: qsTr("队伍"); color: Theme.mutedText; font.pixelSize: Theme.fontCaption }
                    Text { Layout.fillWidth: true; text: qsTr("记录"); color: Theme.mutedText; font.pixelSize: Theme.fontCaption }
                    Text { Layout.preferredWidth: 128; text: qsTr("成绩"); color: Theme.mutedText; font.pixelSize: Theme.fontCaption; horizontalAlignment: Text.AlignRight }
                }
            }
            Repeater {
                model: root.record.standings
                delegate: Rectangle {
                    id: standing
                    required property var modelData
                    required property int index
                    objectName: "seasonStandingRow"
                    width: parent.width
                    height: Math.max(52, standingNote.implicitHeight + 24)
                    color: index === 0 ? Theme.selectedSurface : Theme.managementSurface
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        spacing: 8
                        Text { Layout.preferredWidth: 48; text: standing.modelData.rank; color: Theme.accent; font.pixelSize: Theme.fontHeading; font.bold: true }
                        Text { Layout.preferredWidth: 88; text: standing.modelData.team; color: Theme.text; font.pixelSize: Theme.fontBody; font.bold: true }
                        Text { id: standingNote; Layout.fillWidth: true; text: standing.modelData.note; color: Theme.mutedText; font.pixelSize: Theme.fontControl; wrapMode: Text.Wrap }
                        Text { Layout.preferredWidth: 128; text: standing.modelData.value; color: Theme.text; font.pixelSize: Theme.fontHeading; font.bold: true; horizontalAlignment: Text.AlignRight }
                    }
                }
            }
        }

        Column {
            width: parent.width
            spacing: 8
            visible: root.record.rounds.length > 0
            Text { text: qsTr("回合与对阵"); color: Theme.text; font.pixelSize: Theme.fontHeading; font.bold: true }
            Repeater {
                model: root.record.rounds
                delegate: Rectangle {
                    id: roundCard
                    required property var modelData
                    required property int index
                    property bool expanded: index === 0
                    objectName: "seasonRoundCard"
                    width: parent.width
                    height: roundContent.implicitHeight + 24
                    color: Theme.managementSurface
                    radius: Theme.radiusSmall
                    border.color: Theme.border
                    Column {
                        id: roundContent
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        spacing: 12
                        ConsoleToolButton {
                            width: parent.width
                            height: Math.max(40, roundHeading.implicitHeight + 12)
                            Accessible.name: qsTr("%1 %2，展开或收起记录").arg(roundCard.modelData.label).arg(roundCard.modelData.title)
                            onClicked: roundCard.expanded = !roundCard.expanded
                            contentItem: RowLayout {
                                spacing: 12
                                Text { text: roundCard.expanded ? "⌄" : "›"; color: Theme.accent; font.pixelSize: 20 }
                                Text { text: roundCard.modelData.label; color: Theme.mutedText; font.pixelSize: Theme.fontControl }
                                Text { id: roundHeading; Layout.fillWidth: true; text: roundCard.modelData.title || qsTr("回合记录"); color: Theme.text; font.pixelSize: Theme.fontBody; font.bold: true; wrapMode: Text.Wrap }
                                Text { Layout.maximumWidth: roundContent.width * 0.4; text: roundCard.modelData.outcome; color: Theme.accent; font.pixelSize: Theme.fontControl; wrapMode: Text.Wrap }
                            }
                        }
                        GridLayout {
                            width: parent.width
                            visible: roundCard.expanded && roundCard.modelData.teams.length > 0
                            columns: width >= 900 ? 4 : 2
                            columnSpacing: 8
                            rowSpacing: 8
                            Repeater {
                                model: roundCard.modelData.teams
                                delegate: Rectangle {
                                    id: teamCard
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 200
                                    Layout.preferredHeight: teamContent.implicitHeight + 24
                                    Layout.alignment: Qt.AlignTop
                                    color: Theme.canvas
                                    radius: Theme.radiusSmall
                                    Column {
                                        id: teamContent
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 12
                                        spacing: 8
                                        Text { text: teamCard.modelData.team; color: Theme.text; font.pixelSize: Theme.fontBody; font.bold: true }
                                        Text { visible: text.length > 0; text: teamCard.modelData.value; color: Theme.accent; font.pixelSize: Theme.fontHeading; font.bold: true }
                                        Text { width: parent.width; text: teamCard.modelData.detail; color: Theme.mutedText; font.pixelSize: Theme.fontControl; wrapMode: Text.Wrap }
                                    }
                                }
                            }
                        }
                        Text {
                            width: parent.width
                            visible: roundCard.expanded && text.length > 0
                            text: roundCard.modelData.notes.join("\n")
                            color: Theme.mutedText
                            font.pixelSize: Theme.fontControl
                            lineHeight: 1.4
                            wrapMode: Text.Wrap
                        }
                    }
                }
            }
        }

        ConsoleButton {
            visible: root.record.sections.length > 0
            text: root.showRules ? qsTr("收起赛制与附加记录") : qsTr("赛制与附加记录")
            onClicked: root.showRules = !root.showRules
        }
        Column {
            width: parent.width
            spacing: 8
            visible: root.showRules
            Repeater {
                model: root.record.sections
                delegate: Rectangle {
                    id: section
                    required property var modelData
                    width: parent.width
                    height: sectionBody.implicitHeight + 32
                    color: Theme.managementSurface
                    border.color: Theme.border
                    radius: Theme.radiusSmall
                    Column {
                        id: sectionBody
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 16
                        spacing: 10
                        Text { width: parent.width; text: section.modelData.title; color: Theme.text; font.pixelSize: Theme.fontBody; font.bold: true; wrapMode: Text.Wrap }
                        Text { width: parent.width; text: section.modelData.text; color: Theme.mutedText; font.pixelSize: Theme.fontControl; lineHeight: 1.4; wrapMode: Text.Wrap }
                    }
                }
            }
        }
        ConsoleButton {
            objectName: "seasonArchiveSourceToggle"
            text: root.showSource ? qsTr("收起原始档案") : qsTr("查看完整原始档案")
            onClicked: root.showSource = !root.showSource
        }
        TextArea {
            objectName: "seasonArchiveSourceText"
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
