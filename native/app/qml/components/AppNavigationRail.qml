pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import ".."
import "../controls"

Rectangle {
    id: root
    property string currentView: "monitoring"
    signal pageRequested(string view)
    color: Theme.managementSurface
    width: Theme.navigationRailWidth

    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.border }

    Text {
        anchors.top: parent.top
        anchors.topMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        text: "CSTG"
        color: Theme.accent
        font.pixelSize: 16
        font.bold: true
    }

    component NavigationEntry: ConsoleToolButton {
                id: entry
                required property var modelData
                objectName: modelData.name
                width: 64
                height: 76
                padding: 4
                text: modelData.label
                checked: root.currentView === modelData.key
                Accessible.name: text
                ToolTip.text: text
                onClicked: root.pageRequested(modelData.key)
                contentItem: Column {
                    spacing: 5
                    Item {
                        width: parent.width
                        height: Theme.brandIconSize
                        Image {
                            objectName: entry.modelData.imageName
                            anchors.centerIn: parent
                            width: Theme.brandIconSize
                            height: Theme.brandIconSize
                            visible: entry.modelData.image.length > 0
                            source: visible ? Qt.resolvedUrl("../assets/brand/" + entry.modelData.image) : ""
                            sourceSize: Qt.size(80, 80)
                            fillMode: Image.PreserveAspectFit
                            Accessible.ignored: true
                        }
                        ThemeIcon {
                            anchors.centerIn: parent
                            width: Theme.navigationIconSize
                            height: width
                            visible: entry.modelData.image.length === 0
                            source: Qt.resolvedUrl("../assets/icons/layout-grid.svg")
                            tint: entry.checked ? Theme.accent : Theme.mutedText
                        }
                    }
                    Text {
                        width: parent.width
                        text: entry.text
                        horizontalAlignment: Text.AlignHCenter
                        font.pixelSize: Theme.fontCaption
                        color: entry.checked ? Theme.accent : Theme.mutedText
                    }
                }
    }

    Column {
        anchors.top: parent.top
        anchors.topMargin: 64
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 12
        NavigationEntry { modelData: ({ key: "navigation", name: "hamsterNavigationButton", label: qsTr("主播导航"), image: "icon-navigation.png", imageName: "hamsterNavigationIcon" }) }
        NavigationEntry { modelData: ({ key: "maoziRank", name: "maoziRankButton", label: qsTr("团播榜单"), image: "icon-ranking.png", imageName: "maoziRankIcon" }) }
        NavigationEntry { modelData: ({ key: "monitoring", name: "watchWorkspaceButton", label: qsTr("直播工作区"), image: "", imageName: "watchWorkspaceIcon" }) }
    }
    ConsoleToolButton {
        id: settings
        objectName: "navigationSettingsButton"
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 16
        anchors.horizontalCenter: parent.horizontalCenter
        width: 64
        height: 64
        checked: root.currentView === "settings"
        Accessible.name: qsTr("设置")
        ToolTip.text: Accessible.name
        onClicked: root.pageRequested("settings")
        contentItem: Column {
            spacing: 6
            ThemeIcon {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Theme.navigationIconSize
                height: width
                source: Qt.resolvedUrl("../assets/icons/settings.svg")
                tint: settings.checked ? Theme.accent : Theme.mutedText
            }
            Text { width: parent.width; text: qsTr("设置"); color: Theme.mutedText; font.pixelSize: Theme.fontCaption; horizontalAlignment: Text.AlignHCenter }
        }
    }
}
