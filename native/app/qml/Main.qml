import QtQuick
import QtQuick.Controls
import "components"
import "dialogs"
import "panels"

ApplicationWindow {
    id: root

    objectName: "douyuQuickWindow"
    width: 1280
    height: 720
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    color: canvasColor
    title: "斗鱼多房间监控"
    flags: Qt.Window | Qt.FramelessWindowHint

    property var appController: null
    property bool refreshRequested: false
    property bool editableFocus: false
    readonly property color canvasColor: "#16191f"
    readonly property color surfaceColor: "#1f242c"
    readonly property color borderColor: "#343b45"
    readonly property color accentColor: "#ff7a18"
    readonly property color textColor: "#f4f6f8"
    readonly property color mutedTextColor: "#9ba5b1"
    readonly property int headerHeight: 44
    property bool sidebarVisible: appController
                                  ? appController.workspace.sidebarVisible
                                  : true
    readonly property int sidebarWidth: sidebarVisible ? 268 : 0
    readonly property string layoutId: appController
                                      ? appController.workspace.layoutId
                                      : "grid-2x2"
    readonly property var roomModel: appController ? appController.rooms : previewRooms
    readonly property var libraryRooms: appController ? appController.libraryRooms : []
    readonly property var workspaceModel: appController ? appController.workspace : null

    function toggleSidebarVisibility() {
        if (appController) {
            appController.setSidebarVisible(!sidebarVisible)
        } else {
            sidebarVisible = !sidebarVisible
        }
    }

    onClosing: function(close) {
        if (appController) appController.shutdown()
    }

    function openAddRoom() {
        addRoomDialog.open()
    }

    function requestRefresh() {
        refreshRequested = true
        if (appController && appController.workspace.primaryRoomId.length > 0) {
            appController.refreshRoom(appController.workspace.primaryRoomId)
        }
    }

    function updateEditableFocus() {
        var item = root.activeFocusItem
        root.editableFocus = false
        while (item) {
            var name = item.objectName || ""
            if (name === "roomSearchInput"
                    || name === "presetNameInput"
                    || name === "groupNameInput"
                    || name === "notificationTitleInput") {
                root.editableFocus = true
                return
            }
            item = item.parent
        }
    }

    onActiveFocusItemChanged: updateEditableFocus()

    function toggleWindowMaximized() {
        if (root.visibility === Window.Maximized) {
            root.showNormal()
        } else {
            root.showMaximized()
        }
    }

    function toggleFullScreen() {
        if (root.visibility === Window.FullScreen) {
            if (appController && appController.exitFullScreen) appController.exitFullScreen()
            else root.showNormal()
        } else if (appController && appController.toggleFullScreen) {
            appController.toggleFullScreen()
        } else {
            root.showFullScreen()
        }
    }

    function exitFullScreen() {
        if (root.visibility !== Window.FullScreen) return
        if (appController && appController.exitFullScreen) appController.exitFullScreen()
        else root.showNormal()
    }

    ListModel {
        id: previewRooms

        ListElement {
            roomId: "preview-1"
            anchorName: "直播间预览"
            title: "等待播放服务连接"
            category: "游戏直播"
            viewerLabel: "--"
            avatarUrl: ""
            liveState: "online"
            playbackState: "idle"
            primary: true
            favorite: true
            audioFocused: true
            requestedQuality: "自动"
            effectiveQuality: "自动"
            muted: false
            volume: 100
            danmakuEnabled: false
            danmakuState: "idle"
            danmakuErrorCode: "NONE"
            danmakuRecentRate: 0
            danmakuPeakRate: 0
            danmakuFiltered: 0
            danmakuDuplicates: 0
            danmakuRateLimited: 0
            danmakuQueueOverflow: 0
            danmakuUpstreamDropped: 0
        }
        ListElement {
            roomId: "preview-2"
            anchorName: "房间列表"
            title: "左侧集中管理直播间"
            category: "多路监控"
            viewerLabel: "--"
            avatarUrl: ""
            liveState: "offline"
            playbackState: "idle"
            primary: false
            favorite: false
            audioFocused: false
            requestedQuality: "高清"
            effectiveQuality: "高清"
            muted: true
            volume: 0
            danmakuEnabled: false
            danmakuState: "idle"
            danmakuErrorCode: "NONE"
            danmakuRecentRate: 0
            danmakuPeakRate: 0
            danmakuFiltered: 0
            danmakuDuplicates: 0
            danmakuRateLimited: 0
            danmakuQueueOverflow: 0
            danmakuUpstreamDropped: 0
        }
        ListElement {
            roomId: "preview-3"
            anchorName: "九路布局"
            title: "最多容纳 9 个房间"
            category: "工作区"
            viewerLabel: "--"
            avatarUrl: ""
            liveState: "offline"
            playbackState: "idle"
            primary: false
            favorite: false
            audioFocused: false
            requestedQuality: "自动"
            effectiveQuality: "自动"
            muted: true
            volume: 0
            danmakuEnabled: false
            danmakuState: "idle"
            danmakuErrorCode: "NONE"
            danmakuRecentRate: 0
            danmakuPeakRate: 0
            danmakuFiltered: 0
            danmakuDuplicates: 0
            danmakuRateLimited: 0
            danmakuQueueOverflow: 0
            danmakuUpstreamDropped: 0
        }
    }

    AppHeader {
        id: header
        objectName: "appHeader"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        controller: root.appController
        sidebarVisible: root.sidebarVisible
        onToggleSidebar: root.toggleSidebarVisibility()
        onOpenDanmaku: danmakuSettingsPanel.open()
        onOpenMonitoring: monitoringDrawer.open()
        onOpenWorkspace: workspacePresetsPanel.open()
        onSystemMoveRequested: root.startSystemMove()
        onToggleMaximizedRequested: root.toggleWindowMaximized()
        onToggleFullScreenRequested: root.toggleFullScreen()
    }

    RoomSidebar {
        id: sidebar
        objectName: "roomSidebar"
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: root.sidebarWidth
        visible: width > 0
        controller: root.appController
        roomModel: root.roomModel
        libraryRooms: root.libraryRooms
        workspaceModel: root.workspaceModel
        accentColor: root.accentColor
        surfaceColor: root.surfaceColor
        borderColor: root.borderColor
        textColor: root.textColor
        mutedTextColor: root.mutedTextColor
        onAddRoomRequested: root.openAddRoom()
        onGroupManagementRequested: groupManagerDialog.open()
    }

    WorkspaceGrid {
        id: grid
        objectName: "workspaceGrid"
        anchors.top: header.bottom
        anchors.left: sidebar.right
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        controller: root.appController
        roomModel: root.roomModel
        layoutId: root.layoutId
        layoutMode: root.workspaceModel ? root.workspaceModel.layoutMode : "auto"
        primaryRoomId: root.workspaceModel ? root.workspaceModel.primaryRoomId : "preview-1"
        primaryRoomRatio: root.workspaceModel ? root.workspaceModel.primaryRoomRatio : 0.6
        canvasColor: root.canvasColor
        surfaceColor: root.surfaceColor
        borderColor: root.borderColor
        accentColor: root.accentColor
        textColor: root.textColor
        mutedTextColor: root.mutedTextColor
    }

    ToastViewport {
        objectName: "toastViewport"
        anchors.top: header.bottom
        anchors.topMargin: 10
        anchors.right: parent.right
        anchors.rightMargin: 18
        workspaceModel: root.workspaceModel
        borderColor: root.borderColor
        accentColor: root.accentColor
        textColor: root.textColor
        mutedTextColor: root.mutedTextColor
    }

    DanmakuSettingsPanel {
        id: danmakuSettingsPanel
        controller: root.appController
        roomModel: root.roomModel
    }

    MonitoringStatusPanel {
        id: monitoringDrawer
        controller: root.appController
        monitoringModel: root.appController ? root.appController.monitoring : null
        onNotificationSettingsRequested: notificationSettingsDialog.open()
    }

    WorkspacePresetsPanel {
        id: workspacePresetsPanel
        controller: root.appController
        workspaceModel: root.workspaceModel
        onGroupManagementRequested: groupManagerDialog.open()
    }

    AddRoomDialog {
        id: addRoomDialog
        controller: root.appController
    }

    GroupManagerDialog {
        id: groupManagerDialog
        controller: root.appController
        workspaceModel: root.workspaceModel
        roomModel: root.roomModel
        libraryRooms: root.libraryRooms
    }

    NotificationSettingsDialog {
        id: notificationSettingsDialog
        controller: root.appController
        monitoringModel: root.appController ? root.appController.monitoring : null
    }

    Shortcut {
        sequence: "Ctrl+Shift+A"
        context: Qt.ApplicationShortcut
        enabled: !root.editableFocus
        onActivated: root.openAddRoom()
    }

    Shortcut {
        sequence: "Ctrl+Shift+D"
        context: Qt.ApplicationShortcut
        enabled: !root.editableFocus
        onActivated: danmakuSettingsPanel.visible ? danmakuSettingsPanel.close() : danmakuSettingsPanel.open()
    }

    Shortcut {
        sequence: "Ctrl+Shift+M"
        context: Qt.ApplicationShortcut
        enabled: !root.editableFocus
        onActivated: monitoringDrawer.visible ? monitoringDrawer.close() : monitoringDrawer.open()
    }

    Shortcut {
        sequence: "Ctrl+Shift+W"
        context: Qt.ApplicationShortcut
        enabled: !root.editableFocus
        onActivated: workspacePresetsPanel.visible ? workspacePresetsPanel.close() : workspacePresetsPanel.open()
    }

    Shortcut {
        sequence: "Ctrl+Shift+S"
        context: Qt.ApplicationShortcut
        enabled: !root.editableFocus
        onActivated: root.toggleSidebarVisibility()
    }

    Shortcut {
        sequence: "Ctrl+Shift+R"
        context: Qt.ApplicationShortcut
        enabled: !root.editableFocus
        onActivated: root.requestRefresh()
    }

    Shortcut {
        sequence: "F11"
        context: Qt.ApplicationShortcut
        enabled: !root.editableFocus
        onActivated: root.toggleFullScreen()
    }

    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: !root.editableFocus && root.visibility === Window.FullScreen
        onActivated: root.exitFullScreen()
    }

}
