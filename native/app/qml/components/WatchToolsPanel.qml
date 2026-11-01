import QtQuick
import QtQuick.Controls.Basic
import ".."
import "../controls"

Popup {
    id: root
    objectName: "watchToolsPanel"
    property var controller: null
    readonly property var workspaceModel: controller && controller.workspace ? controller.workspace : null
    property string currentSection: "overview"
    signal roomsRequested()
    signal openDanmaku()
    signal openMonitoring()
    signal openWorkspace()
    signal toggleFullScreenRequested()
    parent: Overlay.overlay
    width: Math.min(360, parent ? parent.width - 24 : 360)
    height: Math.min(implicitHeight, parent ? parent.height - y - 12 : implicitHeight)
    implicitHeight: header.height + 12 + sectionColumn.implicitHeight + topPadding + bottomPadding
    x: parent ? parent.width - width - 12 : 0
    y: 44
    padding: 16
    modal: false
    focus: true
    closePolicy: currentSection === "overview"
                 ? Popup.CloseOnEscape | Popup.CloseOnPressOutside : Popup.CloseOnPressOutside
    onOpened: sidebarToggleButton.forceActiveFocus(Qt.PopupFocusReason)
    onClosed: currentSection = "overview"
    background: Rectangle { color: Theme.managementSurface; border.color: Theme.border; radius: Theme.radiusSmall }

    function showSection(section) {
        currentSection = section
        watchToolsBackButton.forceActiveFocus(Qt.OtherFocusReason)
    }
    function showOverview() {
        const previous = currentSection
        currentSection = "overview"
        if (previous === "sound") soundMasterButton.forceActiveFocus(Qt.OtherFocusReason)
        else if (previous === "layout") layoutMenuButton.forceActiveFocus(Qt.OtherFocusReason)
    }
    Shortcut {
        sequence: "Escape"
        enabled: root.visible && root.currentSection !== "overview"
        onActivated: root.showOverview()
    }
    contentItem: Item {
        Item {
            id: header
            width: parent.width
            height: 32
            ConsoleToolButton {
                id: watchToolsBackButton
                objectName: "watchToolsBackButton"
                width: 32; height: 32
                visible: root.currentSection !== "overview"
                text: "‹"
                font.pixelSize: 24
                Accessible.name: qsTr("返回观看工具")
                onClicked: root.showOverview()
            }
            Text {
                anchors.left: parent.left
                anchors.leftMargin: root.currentSection === "overview" ? 0 : 40
                anchors.verticalCenter: parent.verticalCenter
                text: root.currentSection === "sound" ? qsTr("声音总控")
                      : root.currentSection === "layout" ? qsTr("选择布局") : qsTr("观看工具")
                color: Theme.text
                font.pixelSize: Theme.fontHeading
                font.bold: true
            }
            ConsoleToolButton {
                objectName: "closeWatchToolsButton"
                anchors.right: parent.right
                width: 32; height: 32
                icon.source: "../assets/icons/x.svg"
                Accessible.name: qsTr("关闭观看工具")
                onClicked: root.close()
            }
        }
        ScrollView {
            id: scrollView
            anchors.top: header.bottom
            anchors.topMargin: 12
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            contentWidth: availableWidth
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            Column {
                id: sectionColumn
                width: scrollView.availableWidth
                Column {
                    id: overview
                    objectName: "watchToolsOverview"
                    width: parent.width
                    spacing: 8
                    visible: root.currentSection === "overview"
                    ConsoleButton { id: sidebarToggleButton; objectName: "sidebarToggleButton"; width: parent.width; text: qsTr("房间 · 历史 · 收藏"); icon.source: "../assets/icons/menu.svg"; onClicked: root.roomsRequested() }
                    Rectangle { width: parent.width; height: 1; color: Theme.border }
                    ConsoleButton { id: layoutMenuButton; objectName: "layoutMenuButton"; width: parent.width; text: qsTr("选择布局"); icon.source: "../assets/icons/layout-grid.svg"; onClicked: root.showSection("layout") }
                    ConsoleButton { id: soundMasterButton; objectName: "soundMasterButton"; width: parent.width; text: qsTr("声音总控"); icon.source: "../assets/icons/volume-2.svg"; onClicked: root.showSection("sound") }
                    ConsoleButton { objectName: "danmakuButton"; width: parent.width; text: qsTr("弹幕设置"); icon.source: "../assets/icons/message-circle.svg"; onClicked: root.openDanmaku() }
                    ConsoleButton { objectName: "monitoringButton"; width: parent.width; text: qsTr("监控状态"); icon.source: "../assets/icons/activity.svg"; onClicked: root.openMonitoring() }
                    ConsoleButton { objectName: "workspaceButton"; width: parent.width; text: qsTr("工作区预设"); icon.source: "../assets/icons/star.svg"; onClicked: root.openWorkspace() }
                    Rectangle { width: parent.width; height: 1; color: Theme.border }
                    ConsoleButton { objectName: "fullscreenButton"; property string accessibilityLabel: qsTr("全屏播放"); width: parent.width; text: accessibilityLabel; Accessible.name: text; icon.source: "../assets/icons/window-fullscreen.svg"; onClicked: { root.close(); if (root.controller && root.controller.toggleFullScreen) root.controller.toggleFullScreen(); else root.toggleFullScreenRequested() } }
                }
                Column {
                    id: soundMasterPopover
                    objectName: "soundMasterPopover"
                    width: parent.width
                    spacing: 12
                    visible: root.currentSection === "sound"
                    function close() { root.showOverview() }
                    Text { width: parent.width; text: qsTr("选择哪些直播间播放声音，或暂时静音所有房间。"); wrapMode: Text.WordWrap; color: Theme.mutedText; font.pixelSize: Theme.fontControl }
                    Column {
                        objectName: "audioModeControl"
                        width: parent.width
                        spacing: 8
                        ConsoleButton {
                            objectName: "globalMuteButton"
                            width: parent.width
                            checked: root.workspaceModel ? root.workspaceModel.globalMuted : false
                            text: checked ? qsTr("取消全局静音") : qsTr("全局静音")
                            Accessible.name: text
                            onClicked: if (root.controller) root.controller.setGlobalMuted(root.workspaceModel ? !root.workspaceModel.globalMuted : checked)
                        }
                        Row {
                            width: parent.width
                            spacing: 8
                            ConsoleToolButton {
                                objectName: "audioSingleButton"
                                width: (parent.width - parent.spacing) / 2
                                height: 36
                                text: qsTr("单路声音")
                                checked: root.workspaceModel ? root.workspaceModel.audioMode === "single" : true
                                Accessible.name: text
                                onClicked: if (root.controller) root.controller.setAudioMode("single")
                            }
                            ConsoleToolButton {
                                objectName: "audioMultiButton"
                                width: (parent.width - parent.spacing) / 2
                                height: 36
                                text: qsTr("多路混音")
                                checked: root.workspaceModel ? root.workspaceModel.audioMode === "multi" : false
                                Accessible.name: text
                                onClicked: if (root.controller) root.controller.setAudioMode("multi")
                            }
                        }
                        Text { width: parent.width; text: qsTr("单路声音：只听选中的房间。\n多路混音：同时听多个未静音房间。"); wrapMode: Text.WordWrap; color: Theme.mutedText; font.pixelSize: Theme.fontCaption; lineHeight: 1.4 }
                    }
                }
                Item {
                    id: layoutMenu
                    objectName: "layoutMenu"
                    width: parent.width
                    implicitHeight: layoutOptions.implicitHeight
                    height: implicitHeight
                    visible: root.currentSection === "layout"
                    function close() { root.showOverview() }
                    Rectangle { objectName: "layoutMenuBackground"; anchors.fill: parent; color: Theme.controlSurface; radius: Theme.radiusSmall; z: -1 }
                    Column {
                        id: layoutOptions
                        width: parent.width
                        spacing: 8
        MenuItem {
            id: autoLayoutOption
            hoverEnabled: true
            focusPolicy: Qt.StrongFocus
            objectName: "autoLayoutOption"
            width: layoutMenu.width
            height: 40
            leftPadding: Theme.spaceSmall + Theme.indicatorSize + Theme.spaceXS
            text: "自动布局"
            checkable: true
            autoExclusive: true
            checked: root.workspaceModel ? root.workspaceModel.layoutMode === "auto" : true
            onTriggered: if (root.controller) root.controller.setLayout("auto")
            indicator: Text {
                x: Theme.spaceSmall
                width: Theme.indicatorSize
                height: parent.height
                text: "✓"
                visible: autoLayoutOption.checked
                color: Theme.accent
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontControl
            }
            contentItem: Text {
                objectName: "layoutMenuItemLabel"
                text: autoLayoutOption.text
                color: Theme.text
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: Theme.fontControl
            }
            background: Rectangle {
                color: autoLayoutOption.checked ? Theme.selectedSurface
                                      : (autoLayoutOption.highlighted || autoLayoutOption.hovered) ? Theme.well : "transparent"
                border.width: autoLayoutOption.visualFocus ? Theme.focusBorderWidth : autoLayoutOption.checked ? Theme.borderWidth : 0
                border.color: (autoLayoutOption.checked || autoLayoutOption.visualFocus) ? Theme.accent : "transparent"
                radius: Theme.radiusSmall
            }
        }
        MenuItem {
            id: primaryLayoutOption
            hoverEnabled: true
            focusPolicy: Qt.StrongFocus
            objectName: "primaryLayoutOption"
            width: layoutMenu.width
            height: 40
            leftPadding: Theme.spaceSmall + Theme.indicatorSize + Theme.spaceXS
            text: "主直播间 + 辅直播间"
            checkable: true
            autoExclusive: true
            checked: root.workspaceModel ? root.workspaceModel.layoutMode === "primary" : false
            onTriggered: if (root.controller) root.controller.setLayout("primary")
            indicator: Text {
                x: Theme.spaceSmall
                width: Theme.indicatorSize
                height: parent.height
                text: "✓"
                visible: primaryLayoutOption.checked
                color: Theme.accent
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontControl
            }
            contentItem: Text {
                text: primaryLayoutOption.text
                color: Theme.text
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: Theme.fontControl
            }
            background: Rectangle {
                color: primaryLayoutOption.checked ? Theme.selectedSurface
                                      : (primaryLayoutOption.highlighted || primaryLayoutOption.hovered) ? Theme.well : "transparent"
                border.width: primaryLayoutOption.visualFocus ? Theme.focusBorderWidth : primaryLayoutOption.checked ? Theme.borderWidth : 0
                border.color: (primaryLayoutOption.checked || primaryLayoutOption.visualFocus) ? Theme.accent : "transparent"
                radius: Theme.radiusSmall
            }
        }
        MenuItem {
            id: dualPrimaryLayoutOption
            hoverEnabled: true
            focusPolicy: Qt.StrongFocus
            objectName: "dualPrimaryLayoutOption"
            width: layoutMenu.width
            height: 40
            leftPadding: Theme.spaceSmall + Theme.indicatorSize + Theme.spaceXS
            text: "双主直播间"
            checkable: true
            autoExclusive: true
            enabled: root.controller && root.controller.rooms
                     ? root.controller.rooms.roomCount >= 4
                     : false
            checked: root.workspaceModel ? root.workspaceModel.layoutMode === "primary-two" : false
            ToolTip.visible: false
            ToolTip.text: "双主布局至少需要 4 路直播间"
            onTriggered: if (root.controller) root.controller.setLayout("primary-two")
            indicator: Text {
                x: Theme.spaceSmall
                width: Theme.indicatorSize
                height: parent.height
                text: "✓"
                visible: dualPrimaryLayoutOption.checked
                color: Theme.accent
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: Text.AlignHCenter
                font.pixelSize: Theme.fontControl
            }
            contentItem: Text {
                text: dualPrimaryLayoutOption.text
                color: dualPrimaryLayoutOption.enabled ? Theme.text : Theme.mutedText
                verticalAlignment: Text.AlignVCenter
                font.pixelSize: Theme.fontControl
            }
            background: Rectangle {
                objectName: "dualPrimaryLayoutOptionBackground"
                color: dualPrimaryLayoutOption.checked ? Theme.selectedSurface
                                      : (dualPrimaryLayoutOption.highlighted || dualPrimaryLayoutOption.hovered) ? Theme.well : "transparent"
                border.width: dualPrimaryLayoutOption.visualFocus ? Theme.focusBorderWidth : dualPrimaryLayoutOption.checked ? Theme.borderWidth : 0
                border.color: (dualPrimaryLayoutOption.checked || dualPrimaryLayoutOption.visualFocus) ? Theme.accent : "transparent"
                radius: Theme.radiusSmall
            }
        }
                    Text { width: parent.width; text: qsTr("双主布局至少需要 4 路直播间。"); wrapMode: Text.WordWrap; color: Theme.mutedText; font.pixelSize: Theme.fontCaption; padding: 8 }
                    }
                }
            }
        }
    }
}
