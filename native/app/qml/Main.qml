import QtQuick
import QtQuick.Controls.Basic
import "."
import "components"
import "dialogs"
import "pages"
import "panels"
import "controls"

ApplicationWindow {
    id: root

    objectName: "douyuQuickWindow"
    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    color: Theme.canvas
    palette {
        window: Theme.managementSurface
        base: Theme.well
        alternateBase: Theme.controlSurface
        light: Theme.controlHover
        midlight: Theme.controlSurface
        mid: Theme.border
        dark: Theme.mutedText
        shadow: Theme.canvas
        toolTipBase: Theme.controlSurface
        toolTipText: Theme.text
        link: Theme.accent
        linkVisited: Theme.mutedText
        active {
            windowText: Theme.text
            text: Theme.text
            button: Theme.controlSurface
            buttonText: Theme.text
            brightText: Theme.text
            highlight: Theme.accent
            highlightedText: Theme.accentText
            placeholderText: Theme.mutedText
        }
        inactive {
            windowText: Theme.text
            text: Theme.text
            button: Theme.controlSurface
            buttonText: Theme.text
            brightText: Theme.text
            highlight: Theme.accent
            highlightedText: Theme.accentText
            placeholderText: Theme.mutedText
        }
        disabled {
            windowText: Theme.disabledText
            text: Theme.disabledText
            button: Theme.controlDisabled
            buttonText: Theme.disabledText
            brightText: Theme.disabledText
            highlightedText: Theme.disabledText
            highlight: Theme.borderStrong
            placeholderText: Theme.disabledText
        }
    }
    property bool reducedMotion: false
    onReducedMotionChanged: Theme.reducedMotion = reducedMotion
    title: "CSTG特勤终端"
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowMinimizeButtonHint

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
    readonly property bool immersive: visibility === Window.FullScreen
    readonly property int headerHeight: immersive ? 0 : Theme.topBarHeight
    readonly property int railWidth: immersive ? 0 : Theme.navigationRailWidth
    readonly property bool popupOpen: roomDrawer.visible || watchToolsPanel.visible
                                      || danmakuSettingsPanel.visible || monitoringDrawer.visible
                                      || workspacePresetsPanel.visible || addRoomDialog.visible
                                      || notificationSettingsDialog.visible || closeBehaviorDialog.visible
                                      || teamManagerDialog.visible || pagePopupOpen
    readonly property bool pagePopupOpen: hasOpenControlPopup(settingsPage)
                                          || hasOpenControlPopup(maoziRankPage)
                                          || hasOpenControlPopup(guildNavigation)
    readonly property bool roomPopupOpen: hasRoomPopup(grid)
    property var returnFocusItem: null
    property bool shellReady: false
    property bool sidebarVisible: workspaceModel
                                  ? workspaceModel.sidebarVisible
                                  : false
    property bool navigationFallbackVisible: false
    readonly property bool navigationVisible: workspaceModel
                                             ? workspaceModel.navigationVisible
                                             : navigationFallbackVisible
    readonly property int roomSidebarWidth: sidebarVisible ? 360 : 0
    readonly property int navigationWidth: navigationVisible ? width - railWidth : 0
    readonly property int leftPanelWidth: railWidth
    readonly property int sidebarWidth: 0
    readonly property var roomModel: appController
                                    ? appController.rooms
                                    : (showPreviewRooms ? previewRooms : emptyPreviewRooms)
    readonly property var libraryRooms: appController ? appController.libraryRooms : []
    readonly property var workspaceModel: appController ? appController.workspace : null
    readonly property var monitoringModel: appController ? appController.monitoring : null
    onNavigationVisibleChanged: {
        if (navigationVisible) currentView = "navigation"
        else if (currentView === "navigation") currentView = "monitoring"
    }
    onSidebarVisibleChanged: syncRoomDrawer()
    onAppControllerChanged: if (shellReady) initializeShell()
    Component.onCompleted: { shellReady = true; initializeShell() }
    Binding { target: Theme; property: "mode"; value: root.appController && root.appController.themeMode !== undefined ? root.appController.themeMode : "night"; when: !!root.appController && root.appController.themeMode !== undefined }

    function hasRoomPopup(item) {
        if (!item) return false
        if (item.hasTransientControls !== undefined) return item.hasTransientControls
        for (const child of item.children) {
            if (hasRoomPopup(child)) return true
        }
        return false
    }
    function hasOpenControlPopup(item) {
        if (!item || !item.visible) return false
        if (item.popup !== undefined && item.popup && item.popup.visible) return true
        for (const child of item.children) {
            if (hasOpenControlPopup(child)) return true
        }
        return false
    }

    function initializeShell() {
        if (navigationVisible) currentView = "navigation"
        syncRoomDrawer()
    }
    function syncRoomDrawer() {
        if (!shellReady) return
        if (sidebarVisible && currentView === "monitoring") roomDrawer.open()
        else roomDrawer.close()
    }
    function setSidebarOpen(opened) {
        if (appController && appController.setSidebarVisible) appController.setSidebarVisible(opened)
        else sidebarVisible = opened
    }
    function showPage(view) {
        currentView = view
        if (appController && appController.setNavigationVisible) appController.setNavigationVisible(view === "navigation")
        else navigationFallbackVisible = view === "navigation"
        if (view !== "monitoring") { setSidebarOpen(false); watchToolsPanel.close() }
    }
    function rememberFocus() {
        if (!popupOpen) returnFocusItem = activeFocusItem
    }
    function restoreFocus() {
        Qt.callLater(function() {
            if (root.popupOpen) return
            if (root.returnFocusItem && root.returnFocusItem.visible && root.returnFocusItem.enabled)
                root.returnFocusItem.forceActiveFocus(Qt.ShortcutFocusReason)
            else if (root.immersive) immersiveEdge.forceActiveFocus(Qt.ShortcutFocusReason)
            else watchFocus.forceActiveFocus(Qt.ShortcutFocusReason)
        })
    }
    function openSurface(surface) {
        rememberFocus()
        watchToolsPanel.close()
        setSidebarOpen(false)
        for (const panel of [danmakuSettingsPanel, monitoringDrawer, workspacePresetsPanel]) {
            if (panel !== surface) panel.close()
        }
        surface.open()
    }
    function toggleWatchTools() {
        if (watchToolsPanel.visible) { watchToolsPanel.close(); return }
        rememberFocus()
        setSidebarOpen(false)
        for (const panel of [danmakuSettingsPanel, monitoringDrawer, workspacePresetsPanel])
            panel.close()
        watchToolsPanel.open()
    }
    function toggleTheme() {
        const next = Theme.isDay ? "night" : "day"
        if (appController && appController.setThemeMode) appController.setThemeMode(next)
        else Theme.mode = next
    }
    onCurrentViewChanged: if (shellReady) syncRoomDrawer()

    function toggleSidebarVisibility() {
        const nextVisible = !sidebarVisible || currentView !== "monitoring"
        showPage("monitoring")
        rememberFocus()
        watchToolsPanel.close()
        for (const panel of [danmakuSettingsPanel, monitoringDrawer, workspacePresetsPanel])
            panel.close()
        setSidebarOpen(nextVisible)
        if (nextVisible) roomDrawer.open()
    }

    function toggleNavigationVisibility() {
        showPage(currentView === "navigation" ? "monitoring" : "navigation")
    }
    function toggleMaoziRank() {
        root.showPage(root.currentView === "maoziRank" ? "monitoring" : "maoziRank")
        if (root.currentView === "maoziRank"
                && root.appController
                && root.appController.maoziRank) {
            root.appController.maoziRank.refresh()
        }
    }

    onClosing: function(close) {
        if (appController && appController.quitRequested) {
            close.accepted = true
            return
        }
        close.accepted = false
        if (!appController) return
        if (appController.closeBehavior === "ask") openCloseBehaviorDialog()
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
        openSurface(addRoomDialog)
    }

    function openCloseBehaviorDialog() {
        rememberFocus()
        closeBehaviorDialog.open()
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
            const input = item as TextInput
            const editor = item as TextEdit
            if ((input && (!input.readOnly || input.inputMethodComposing))
                    || (editor && (!editor.readOnly || editor.inputMethodComposing))) {
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

    onImmersiveChanged: {
        if (immersive) {
            watchToolsPanel.close()
            roomDrawer.close()
            danmakuSettingsPanel.close()
            monitoringDrawer.close()
            workspacePresetsPanel.close()
            Qt.callLater(function() { watchFocus.forceActiveFocus(Qt.ShortcutFocusReason) })
        } else {
            Qt.callLater(function() {
                if (!root.immersive && root.currentView === "monitoring" && !root.popupOpen)
                    shellHeader.focusWatchTools()
            })
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
            renderEnabled: true
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
            renderEnabled: true
        }
        ListElement {
            roomId: "preview-3"
            anchorName: "十六路布局"
            title: "最多容纳 16 个房间"
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
            renderEnabled: true
        }
    }

    ListModel {
        id: emptyPreviewRooms
    }

    AppHeader {
        id: shellHeader
        objectName: "appHeader"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        visible: !root.immersive
        controller: root.appController
        watching: root.currentView === "monitoring"
        onCloseRequested: root.openCloseBehaviorDialog()
        onOpenTools: root.toggleWatchTools()
        onThemeToggleRequested: root.toggleTheme()
        onSystemMoveRequested: root.startSystemMove()
        onToggleMaximizedRequested: root.toggleWindowMaximized()
    }

    AppNavigationRail {
        id: navigationRail
        objectName: "appNavigationRail"
        x: 0
        y: root.headerHeight
        height: parent.height - y
        visible: !root.immersive
        currentView: root.currentView
        onPageRequested: function(view) {
            if (view === "maoziRank") root.toggleMaoziRank()
            else root.showPage(view)
        }
    }

    FocusScope {
        id: watchFocus
        objectName: "watchFocusScope"
        x: root.railWidth
        y: root.headerHeight
        width: root.width - x
        height: root.height - y
        visible: root.currentView === "monitoring"
    }

    Popup {
        id: roomDrawer
        objectName: "roomToolsDrawer"
        parent: Overlay.overlay
        x: parent ? parent.width - width : 0
        y: root.headerHeight
        width: Math.min(360, root.width - 24)
        height: root.height - y
        padding: 0
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        onClosed: { if (root.sidebarVisible) root.setSidebarOpen(false); root.restoreFocus() }
        background: Rectangle { color: Theme.managementSurface; border.color: Theme.border }
        contentItem: RoomSidebar {
                id: sidebar
                objectName: "roomSidebar"
                controller: root.appController
                roomModel: root.roomModel
                libraryRooms: root.libraryRooms
                workspaceModel: root.workspaceModel
                onAddRoomRequested: root.openAddRoom()
                onSettingsRequested: root.showPage("settings")
                onCloseRequested: roomDrawer.close()
        }
    }

    WatchToolsPanel {
        id: watchToolsPanel
        controller: root.appController
        y: root.immersive ? 12 : root.headerHeight + 8
        onRoomsRequested: root.toggleSidebarVisibility()
        onOpenDanmaku: root.openSurface(danmakuSettingsPanel)
        onOpenMonitoring: root.openSurface(monitoringDrawer)
        onOpenWorkspace: root.openSurface(workspacePresetsPanel)
        onToggleFullScreenRequested: root.toggleFullScreen()
        onClosed: root.restoreFocus()
    }

    ImmersiveEdge {
        id: immersiveEdge
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.rightMargin: 12
        visible: root.immersive && root.currentView === "monitoring"
        z: 110
        onToolsRequested: root.toggleWatchTools()
    }
    GuildNavigationPanel {
        id: guildNavigation
        objectName: "guildNavigationPanel"
        anchors.top: parent.top
        anchors.topMargin: root.headerHeight
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.leftMargin: root.railWidth
        visible: root.currentView === "navigation"
        controller: root.appController
        workspaceModel: root.workspaceModel
        onAddRoomRequested: root.openSurface(addRoomDialog)

    }

    Item {
        id: leftPanelBoundary
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: root.leftPanelWidth
    }

    WorkspaceGrid {
        id: grid
        objectName: "workspaceGrid"
        onAddRoomRequested: root.openAddRoom()
        onNavigationRequested: root.showPage("navigation")
        onFullScreenToggleRequested: root.toggleFullScreen()
        anchors.top: parent.top
        anchors.topMargin: root.headerHeight
        anchors.left: leftPanelBoundary.right
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: root.immersive ? 0 : workspaceStatusBar.height + Theme.gap * 2
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
        danmakuCount: grid.connectedDanmakuCount
        audioLabel: root.workspaceModel
                    ? (root.workspaceModel.globalMuted
                       ? "全局静音"
                       : (root.workspaceModel.audioMode === "multi"
                          ? "多声道"
                          : (root.workspaceModel.audioRoomId.length > 0
                          ? root.workspaceModel.audioRoomId
                          : "无")))
                    : "无"
        visible: root.currentView === "monitoring" && !root.immersive
        height: root.immersive ? 0 : 28
    }

    SettingsPage {
        id: settingsPage
        objectName: "settingsPage"
        anchors.top: parent.top
        anchors.topMargin: root.headerHeight
        anchors.left: leftPanelBoundary.right
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: root.currentView === "settings"
        z: 20
        controller: root.appController
        onBackRequested: root.showPage("monitoring")
        onTeamManagerRequested: root.openSurface(teamManagerDialog)
    }

    MaoziRankPage {
        id: maoziRankPage
        objectName: "maoziRankPage"
        anchors.top: parent.top
        anchors.topMargin: root.headerHeight
        anchors.left: leftPanelBoundary.right
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: root.currentView === "maoziRank"
        z: 20
        controller: root.appController
        onBackRequested: root.showPage("monitoring")
    }

    Rectangle {
        id: workspaceSaveWarning
        objectName: "workspaceUnsavedWarning"
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 42
        width: workspaceSaveWarningContent.width + 12
        height: workspaceSaveWarningContent.height + 12
        color: Theme.controlSurface
        border.color: Theme.warning
        z: 150
        visible: !!root.appController && root.appController.workspaceUnsaved === true
        Row {
            id: workspaceSaveWarningContent
            x: 6
            y: 6
            spacing: 12
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "更改尚未保存，退出可能丢失"
                color: Theme.text
                font.pixelSize: 12
            }
            Button {
                objectName: "retryWorkspaceSaveButton"
                text: "重试保存"
                onClicked: if (root.appController) root.appController.retryWorkspaceSave()
            }
        }
    }

    ToastViewport {
        objectName: "toastViewport"
        anchors.top: parent.top
        anchors.topMargin: root.headerHeight + 10
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
        onClosed: root.restoreFocus()
        controller: root.appController
        roomModel: root.roomModel
    }

    MonitoringStatusPanel {
        id: monitoringDrawer
        onClosed: root.restoreFocus()
        controller: root.appController
        monitoringModel: root.monitoringModel
        onNotificationSettingsRequested: root.openSurface(notificationSettingsDialog)
    }

    WorkspacePresetsPanel {
        id: workspacePresetsPanel
        onClosed: root.restoreFocus()
        controller: root.appController
        workspaceModel: root.workspaceModel
    }

    AddRoomDialog {
        id: addRoomDialog
        onClosed: root.restoreFocus()
        controller: root.appController
    }

    NotificationSettingsDialog {
        id: notificationSettingsDialog
        onClosed: root.restoreFocus()
        controller: root.appController
        monitoringModel: root.monitoringModel
    }

    CloseBehaviorDialog {
        id: closeBehaviorDialog
        onClosed: root.restoreFocus()
        controller: root.appController
    }

    TeamManagerDialog {
        id: teamManagerDialog
        onClosed: root.restoreFocus()
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
        onActivated: danmakuSettingsPanel.visible ? danmakuSettingsPanel.close() : root.openSurface(danmakuSettingsPanel)
    }

    Shortcut {
        sequence: "Ctrl+Shift+M"
        context: Qt.ApplicationShortcut
        enabled: !root.editableFocus
        onActivated: monitoringDrawer.visible ? monitoringDrawer.close() : root.openSurface(monitoringDrawer)
    }

    Shortcut {
        sequence: "Ctrl+Shift+W"
        context: Qt.ApplicationShortcut
        enabled: !root.editableFocus
        onActivated: workspacePresetsPanel.visible ? workspacePresetsPanel.close() : root.openSurface(workspacePresetsPanel)
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
        sequence: "T"
        context: Qt.WindowShortcut
        enabled: root.currentView === "monitoring" && !root.editableFocus && !root.roomPopupOpen
                 && (!root.popupOpen || watchToolsPanel.visible)
        onActivated: root.toggleWatchTools()
    }

    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: !root.editableFocus && !root.popupOpen && !root.roomPopupOpen
                 && root.visibility === Window.FullScreen
        onActivated: root.exitFullScreen()
    }

}
