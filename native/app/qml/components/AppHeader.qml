import QtQuick
import QtQuick.Controls
import ".."

Rectangle {
    id: root

    property var controller: null
    property bool sidebarVisible: true
    signal toggleSidebar()
    signal openDanmaku()
    signal openMonitoring()
    signal openWorkspace()
    signal systemMoveRequested()
    signal toggleMaximizedRequested()
    signal toggleFullScreenRequested()
    readonly property var workspaceModel: root.controller ? root.controller.workspace : null

    height: Theme.topBarHeight
    z: 100
    color: Theme.appBar
    border.color: Theme.border
    border.width: 1

    Row {
        id: brandRow
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        ToolButton {
            objectName: "sidebarToggleButton"
            width: 28
            height: 28
            Accessible.name: root.sidebarVisible ? "收起房间列表" : "展开房间列表"
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name
            onClicked: root.toggleSidebar()
            contentItem: Image {
                objectName: "headerSidebarIcon"
                anchors.centerIn: parent
                width: 16
                height: 16
                source: Qt.resolvedUrl("../assets/icons/menu.svg")
                opacity: parent.hovered ? 1 : 0.82
            }
            background: Rectangle { radius: Theme.radiusSmall; color: parent.hovered ? Theme.controlSurface : "transparent" }
        }

        Rectangle {
            objectName: "brandMark"
            width: 28
            height: 28
            radius: 6
            color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14)
            border.color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.45)

            Image {
                objectName: "brandIcon"
                anchors.centerIn: parent
                width: 18
                height: 18
                source: Qt.resolvedUrl("../assets/douyu_monitor.svg")
                fillMode: Image.PreserveAspectFit
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Text {
                color: Theme.text
                font.bold: true
                font.pixelSize: 14
                text: "斗鱼多房间监控"
            }
        }
    }

    Item {
        objectName: "titleBarDragArea"
        anchors.left: brandRow.right
        anchors.leftMargin: 16
        anchors.right: actionButtons.left
        anchors.rightMargin: 16
        anchors.top: parent.top
        anchors.bottom: parent.bottom

        DragHandler {
            target: null
            onActiveChanged: if (active) root.systemMoveRequested()
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            onDoubleTapped: root.toggleMaximizedRequested()
        }
    }

    Row {
        id: actionButtons
        anchors.right: windowControls.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 3

        ToolButton {
            objectName: "danmakuButton"
            Accessible.name: "弹幕设置"
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name
            onClicked: root.openDanmaku()
            contentItem: Image { anchors.centerIn: parent; width: 16; height: 16; source: Qt.resolvedUrl("../assets/icons/message-circle.svg"); opacity: parent.hovered ? 1 : 0.82 }
            background: Rectangle { radius: Theme.radiusSmall; color: parent.hovered ? Theme.controlSurface : "transparent" }
        }
        ToolButton {
            objectName: "monitoringButton"
            Accessible.name: "监控状态"
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name
            onClicked: root.openMonitoring()
            contentItem: Image { anchors.centerIn: parent; width: 16; height: 16; source: Qt.resolvedUrl("../assets/icons/activity.svg"); opacity: parent.hovered ? 1 : 0.82 }
            background: Rectangle { radius: Theme.radiusSmall; color: parent.hovered ? Theme.controlSurface : "transparent" }
        }
        ToolButton {
            objectName: "workspaceButton"
            Accessible.name: "工作区预设"
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name
            onClicked: root.openWorkspace()
            contentItem: Image { objectName: "workspacePresetIcon"; anchors.centerIn: parent; width: 16; height: 16; source: Qt.resolvedUrl("../assets/icons/star.svg"); opacity: parent.hovered ? 1 : 0.82 }
            background: Rectangle { radius: Theme.radiusSmall; color: parent.hovered ? Theme.controlSurface : "transparent" }
        }
        ToolButton {
            objectName: "layoutMenuButton"
            Accessible.name: "选择布局"
            ToolTip.visible: hovered
            ToolTip.text: "选择布局"
            onClicked: layoutMenu.open()
            contentItem: Image { objectName: "layoutMenuIcon"; anchors.centerIn: parent; width: 16; height: 16; source: Qt.resolvedUrl("../assets/icons/layout-grid.svg"); opacity: parent.hovered ? 1 : 0.82 }
            background: Rectangle { radius: 4; color: parent.hovered ? "#252c34" : "transparent" }
        }
        ToolButton {
            id: soundMasterButton
            objectName: "soundMasterButton"
            Accessible.name: "声音总控"
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name
            onClicked: soundMasterPopover.visible
                       ? soundMasterPopover.close()
                       : soundMasterPopover.open()
            contentItem: Image {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: Qt.resolvedUrl("../assets/icons/volume-2.svg")
                opacity: parent.hovered ? 1 : 0.82
            }
            background: Rectangle {
                radius: Theme.radiusSmall
                color: parent.hovered ? Theme.controlSurface : "transparent"
            }
        }
        ToolButton {
            objectName: "fullscreenButton"
            Accessible.name: "全屏播放"
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name
            onClicked: {
                if (root.controller && root.controller.toggleFullScreen) root.controller.toggleFullScreen()
                else root.toggleFullScreenRequested()
            }
            contentItem: Image {
                objectName: "fullscreenIcon"
                anchors.centerIn: parent
                width: 16
                height: 16
                source: Qt.resolvedUrl("../assets/icons/window-fullscreen.svg")
                opacity: parent.hovered ? 1 : 0.82
            }
            background: Rectangle {
                radius: Theme.radiusSmall
                color: parent.hovered ? Theme.controlSurface : "transparent"
            }
        }
    }

    WindowControls {
        id: windowControls
        objectName: "windowControls"
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        controller: root.controller
    }

    Rectangle {
        id: soundMasterPopover
        objectName: "soundMasterPopover"
        width: 176
        height: audioModeControl.implicitHeight + 16
        visible: false
        z: 100
        color: Theme.controlSurface
        border.color: Theme.border
        radius: 6
        property point soundAnchor: Qt.point(actionButtons.x + soundMasterButton.x,
                                             root.height + 4)
        x: Math.max(12, root.width - width - 72)
        y: root.height + 8

        function open() { visible = true }
        function close() { visible = false }

        Column {
            id: audioModeControl
            objectName: "audioModeControl"
            anchors.fill: parent
            anchors.margins: 8
            spacing: 3

            ToolButton {
                objectName: "globalMuteButton"
                width: parent.width
                height: 28
                checkable: false
                checked: root.workspaceModel ? root.workspaceModel.globalMuted : false
                Accessible.name: checked ? "取消全局静音" : "全局静音"
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: if (root.controller) root.controller.setGlobalMuted(
                    root.workspaceModel ? !root.workspaceModel.globalMuted : checked)
                contentItem: Text {
                    text: parent.checked ? "取消全局静音" : "全局静音"
                    color: "#d6dde5"
                    leftPadding: 8
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 10
                }
                background: Rectangle { radius: 4; color: parent.hovered ? "#241b17" : "transparent" }
            }
            Row {
                width: parent.width
                spacing: 2
                ToolButton {
                    objectName: "audioSingleButton"
                    width: (parent.width - parent.spacing) / 2
                    height: 28
                    text: "单声道"
                    checked: root.workspaceModel ? root.workspaceModel.audioMode === "single" : true
                    Accessible.name: "单声道"
                    onClicked: if (root.controller) root.controller.setAudioMode("single")
                    contentItem: Text { text: parent.text; color: parent.checked ? "#f4f6f8" : "#9ba5b1"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 10 }
                    background: Rectangle { radius: 3; color: parent.checked ? "#3a3025" : (parent.hovered ? "#252c34" : "transparent"); border.color: parent.checked ? "#9b572f" : "transparent" }
                }
                ToolButton {
                    objectName: "audioMultiButton"
                    width: (parent.width - parent.spacing) / 2
                    height: 28
                    text: "多声道"
                    checked: root.workspaceModel ? root.workspaceModel.audioMode === "multi" : false
                    Accessible.name: "多声道"
                    onClicked: if (root.controller) root.controller.setAudioMode("multi")
                    contentItem: Text { text: parent.text; color: parent.checked ? "#f4f6f8" : "#9ba5b1"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 10 }
                    background: Rectangle { radius: 3; color: parent.checked ? "#3a3025" : (parent.hovered ? "#252c34" : "transparent"); border.color: parent.checked ? "#9b572f" : "transparent" }
                }
            }
        }
    }

    Menu {
        id: layoutMenu
        objectName: "layoutMenu"
        y: root.height + 4
        x: Math.max(8, root.width - width - 150)
        title: "选择布局"

        MenuItem {
            objectName: "autoLayoutOption"
            text: "自动布局"
            checkable: true
            checked: root.workspaceModel ? root.workspaceModel.layoutMode === "auto" : true
            onTriggered: if (root.controller) root.controller.setLayout("auto")
        }
        MenuItem {
            objectName: "primaryLayoutOption"
            text: "主直播间 + 辅直播间"
            checkable: true
            checked: root.workspaceModel ? root.workspaceModel.layoutMode === "primary" : false
            onTriggered: if (root.controller) root.controller.setLayout("primary")
        }
    }
}
