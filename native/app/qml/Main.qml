import QtQuick
import QtQuick.Controls
import "."
import "components"
import "dialogs"
import "pages"
import "panels"

ApplicationWindow {
    id: root

    objectName: "douyuQuickWindow"
    width: 1280
    height: 720
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    color: Theme.canvas
    title: "斗鱼多房间监控"
    flags: Qt.Window | Qt.FramelessWindowHint

    property var appController: null
    property bool refreshRequested: false
    property bool editableFocus: false
    property bool showPreviewRooms: false
    property string currentView: "monitoring"
    readonly property color canvasColor: Theme.canvas
    readonly property color surfaceColor: Theme.controlSurface
    readonly property color borderColor: Theme.border
    readonly property color accentColor: Theme.accent
    readonly property color textColor: Theme.text
    readonly property color mutedTextColor: Theme.mutedText
    readonly property int headerHeight: Theme.topBarHeight
    property bool sidebarVisible: appController
                                  ? appController.workspace.sidebarVisible
                                  : true
    property bool navigationFallbackVisible: false
    readonly property bool navigationVisible: workspaceModel
                                             ? workspaceModel.navigationVisible
                                             : navigationFallbackVisible
    readonly property int roomSidebarWidth: sidebarVisible && !navigationVisible ? 268 : 0
    readonly property int navigationWidth: navigationVisible ? 284 : 0
    readonly property int leftPanelWidth: Math.max(roomSidebarWidth, navigationWidth)
    readonly property int sidebarWidth: roomSidebarWidth
    readonly property var roomModel: appController
                                    ? appController.rooms
                                    : (showPreviewRooms ? previewRooms : emptyPreviewRooms)
    readonly property var libraryRooms: appController ? appController.libraryRooms : []
    readonly property var workspaceModel: appController ? appController.workspace : null
    readonly property var monitoringModel: appController ? appController.monitoring : null

    function toggleSidebarVisibility() {
        const nextVisible = !sidebarVisible
        if (appController) {
            if (nextVisible) appController.setNavigationVisible(false)
            appController.setSidebarVisible(nextVisible)
        } else {
            sidebarVisible = nextVisible
            if (nextVisible) navigationFallbackVisible = false
        }
    }

    function toggleNavigationVisibility() {
        const nextVisible = !navigationVisible
        if (appController) {
            appController.setNavigationVisible(nextVisible)
            if (nextVisible && sidebarVisible) appController.setSidebarVisible(false)
        } else {
            navigationFallbackVisible = nextVisible
            if (nextVisible && sidebarVisible) sidebarVisible = false
        }
    }

    onClosing: function(close) {
        if (appController && appController.quitRequested) {
            close.accepted = true
            return
        }
        close.accepted = false
        if (!appController) return
        if (appController.closeBehavior === "ask") closeBehaviorDialog.open()
        else if (appController.requestClose) appController.requestClose()
    }

    onVisibilityChanged: function(newVisibility) {
        if (!appController) return
        if (newVisibility === Window.Minimized && !appController.backgroundHosted
                && appController.minimizeWindow) {
            appController.minimizeWindow()
        } else if (newVisibility !== Window.Minimized && appController.windowMinimized
                   && appController.restoreFromMinimized) {
            appController.restoreFromMinimized()
        }
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
                    || name === "teamNameInput"
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
            requestedQualityRate: -1
            effectiveQuality: "自动"
            availableQualities: []
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
            requestedQualityRate: 3
            effectiveQuality: "高清"
            availableQualities: []
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
            requestedQualityRate: -1
            effectiveQuality: "自动"
            availableQualities: []
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

    ListModel {
        id: emptyPreviewRooms
    }

    AppHeader {
        id: header
        objectName: "appHeader"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        controller: root.appController
        onCloseRequested: closeBehaviorDialog.open()
        sidebarVisible: root.sidebarVisible
        navigationVisible: root.navigationVisible
        onToggleSidebar: root.toggleSidebarVisibility()
        onToggleNavigation: root.toggleNavigationVisibility()
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
        width: root.roomSidebarWidth
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
        onSettingsRequested: root.currentView = "settings"
    }

    GuildNavigationPanel {
        id: guildNavigation
        objectName: "guildNavigationPanel"
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: root.navigationWidth
        visible: width > 0
        controller: root.appController
        workspaceModel: root.workspaceModel
    }

    Item {
        id: leftPanelBoundary
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: root.leftPanelWidth
        visible: false
    }

    WorkspaceGrid {
        id: grid
        objectName: "workspaceGrid"
        anchors.top: header.bottom
        anchors.left: leftPanelBoundary.right
        anchors.right: parent.right
        anchors.bottom: workspaceStatusBar.top
        anchors.bottomMargin: Theme.gap
        controller: root.appController
        roomModel: root.roomModel
        layoutMode: root.workspaceModel ? root.workspaceModel.layoutMode : "auto"
        primaryRoomId: root.workspaceModel ? root.workspaceModel.primaryRoomId : "preview-1"
        secondaryPrimaryRoomId: root.workspaceModel ? root.workspaceModel.secondaryPrimaryRoomId : "preview-2"
        canvasColor: root.canvasColor
        surfaceColor: root.surfaceColor
        borderColor: root.borderColor
        accentColor: root.accentColor
        textColor: root.textColor
        mutedTextColor: root.mutedTextColor
        visible: root.currentView === "monitoring"
    }

    WorkspaceStatusBar {
        id: workspaceStatusBar
        anchors.left: leftPanelBoundary.right
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.gap
        anchors.rightMargin: Theme.gap
        anchors.bottomMargin: Theme.gap
        onlineCount: root.monitoringModel ? root.monitoringModel.onlineCount : 0
        offlineCount: root.monitoringModel ? root.monitoringModel.offlineCount : 0
        danmakuCount: 0
        audioLabel: root.workspaceModel
                    ? (root.workspaceModel.globalMuted
                       ? "全局静音"
                       : (root.workspaceModel.audioMode === "multi"
                          ? "多声道"
                          : (root.workspaceModel.audioRoomId.length > 0
                          ? root.workspaceModel.audioRoomId
                          : "无")))
                    : "无"
        visible: root.currentView === "monitoring"
    }

    SettingsPage {
        id: settingsPage
        objectName: "settingsPage"
        anchors.top: header.bottom
        anchors.left: leftPanelBoundary.right
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: root.currentView === "settings"
        z: 20
        controller: root.appController
        onBackRequested: root.currentView = "monitoring"
        onTeamManagerRequested: teamManagerDialog.open()
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
        monitoringModel: root.monitoringModel
        onNotificationSettingsRequested: notificationSettingsDialog.open()
    }

    WorkspacePresetsPanel {
        id: workspacePresetsPanel
        controller: root.appController
        workspaceModel: root.workspaceModel
    }

    AddRoomDialog {
        id: addRoomDialog
        controller: root.appController
    }

    NotificationSettingsDialog {
        id: notificationSettingsDialog
        controller: root.appController
        monitoringModel: root.monitoringModel
    }

    CloseBehaviorDialog {
        id: closeBehaviorDialog
        controller: root.appController
    }

    TeamManagerDialog {
        id: teamManagerDialog
        controller: root.appController
        workspaceModel: root.workspaceModel
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
