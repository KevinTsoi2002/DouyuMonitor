import QtQuick
import QtQuick.Controls

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

    height: 44
    color: "#0f141a"
    border.color: "#343b45"
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
            background: Rectangle { radius: 4; color: parent.hovered ? "#252c34" : "transparent" }
        }

        Rectangle {
            objectName: "brandMark"
            width: 28
            height: 28
            radius: 6
            color: "#2b1b18"
            border.color: "#79462f"

            Text {
                anchors.centerIn: parent
                color: "#ff7a18"
                font.bold: true
                font.pixelSize: 14
                text: "D"
            }
        }

        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 1

            Text {
                color: "#f4f6f8"
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
            background: Rectangle { radius: 4; color: parent.hovered ? "#252c34" : "transparent" }
        }
        ToolButton {
            objectName: "monitoringButton"
            Accessible.name: "监控状态"
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name
            onClicked: root.openMonitoring()
            contentItem: Image { anchors.centerIn: parent; width: 16; height: 16; source: Qt.resolvedUrl("../assets/icons/activity.svg"); opacity: parent.hovered ? 1 : 0.82 }
            background: Rectangle { radius: 4; color: parent.hovered ? "#252c34" : "transparent" }
        }
        ToolButton {
            objectName: "workspaceButton"
            Accessible.name: "工作区预设"
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name
            onClicked: root.openWorkspace()
            contentItem: Image { objectName: "workspacePresetIcon"; anchors.centerIn: parent; width: 16; height: 16; source: Qt.resolvedUrl("../assets/icons/star.svg"); opacity: parent.hovered ? 1 : 0.82 }
            background: Rectangle { radius: 4; color: parent.hovered ? "#252c34" : "transparent" }
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
            onClicked: soundMasterPopover.open()
            contentItem: Image {
                anchors.centerIn: parent
                width: 16
                height: 16
                source: Qt.resolvedUrl("../assets/icons/volume-2.svg")
                opacity: parent.hovered ? 1 : 0.82
            }
            background: Rectangle {
                radius: 4
                color: parent.hovered ? "#252c34" : "transparent"
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
            contentItem: Image { anchors.centerIn: parent; width: 16; height: 16; source: Qt.resolvedUrl("../assets/icons/square.svg"); opacity: parent.hovered ? 1 : 0.82 }
            background: Rectangle { radius: 4; color: parent.hovered ? "#252c34" : "transparent" }
        }
    }

    WindowControls {
        id: windowControls
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        controller: root.controller
    }

    Popup {
        id: soundMasterPopover
        objectName: "soundMasterPopover"
        width: 176
        padding: 8
        x: Math.max(8, soundMasterButton.x - width + soundMasterButton.width)
        y: root.height + 4
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        background: Rectangle {
            radius: 6
            color: "#141a21"
            border.color: "#343b45"
        }

        contentItem: Column {
            objectName: "audioModeControl"
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
            text: "自动推荐"
            checkable: true
            checked: root.workspaceModel ? root.workspaceModel.layoutMode === "auto" : true
            onTriggered: if (root.controller) root.controller.setLayout("auto")
        }
        MenuItem {
            objectName: "singleLayoutOption"
            text: "单画面"
            checkable: true
            checked: root.workspaceModel ? root.workspaceModel.layoutMode === "single" : false
            onTriggered: if (root.controller) root.controller.setLayout("single")
        }
        MenuItem {
            objectName: "grid2LayoutOption"
            text: "2×2 网格"
            checkable: true
            checked: root.workspaceModel ? root.workspaceModel.layoutMode === "grid-2x2" : false
            onTriggered: if (root.controller) root.controller.setLayout("grid-2x2")
        }
        MenuItem {
            objectName: "grid3LayoutOption"
            text: "3×2 网格"
            checkable: true
            checked: root.workspaceModel ? root.workspaceModel.layoutMode === "grid-3x2" : false
            onTriggered: if (root.controller) root.controller.setLayout("grid-3x2")
        }
        MenuItem {
            objectName: "grid3x3LayoutOption"
            text: "3×3 网格"
            checkable: true
            checked: root.workspaceModel ? root.workspaceModel.layoutMode === "grid-3x3" : false
            onTriggered: if (root.controller) root.controller.setLayout("grid-3x3")
        }
        MenuItem {
            objectName: "primaryTwoLayoutOption"
            text: "主画面 + 辅助画面"
            checkable: true
            checked: root.workspaceModel ? root.workspaceModel.layoutMode === "primary-two" : false
            onTriggered: if (root.controller) root.controller.setLayout("primary-two")
        }
        MenuItem {
            objectName: "splitHorizontalLayoutOption"
            text: "横向两分屏"
            checkable: true
            checked: root.workspaceModel ? root.workspaceModel.layoutMode === "split-horizontal" : false
            onTriggered: if (root.controller) root.controller.setLayout("split-horizontal")
        }
        MenuItem {
            objectName: "splitVerticalLayoutOption"
            text: "纵向两分屏"
            checkable: true
            checked: root.workspaceModel ? root.workspaceModel.layoutMode === "split-vertical" : false
            onTriggered: if (root.controller) root.controller.setLayout("split-vertical")
        }
    }
}
