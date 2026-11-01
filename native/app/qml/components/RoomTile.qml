import QtQuick
import QtQuick.Controls.Basic
import DouyuNative
import ".."
import "../controls"

FocusScope {
    id: root

    implicitWidth: 320
    implicitHeight: 180
    z: menuOpen ? 1000 : 0

    required property string roomId
    required property string anchorName
    required property string title
    required property string category
    required property string viewerLabel
    required property url avatarUrl
    required property string liveState
    required property string playbackState
    required property bool primary
    property bool secondaryPrimary: false
    required property bool favorite
    required property bool audioFocused
    required property string requestedQuality
    required property int requestedQualityRate
    required property string effectiveQuality
    required property bool muted
    required property int volume
    required property bool danmakuEnabled
    required property string danmakuState
    required property string danmakuErrorCode
    required property bool renderEnabled
    required property int index
    required property var availableQualities
    property var controller: null
    signal fullScreenToggleRequested()
    property color borderColor: Theme.border
    property color accentColor: Theme.accent
    property color textColor: Theme.videoText
    property color mutedTextColor: Theme.videoMutedText
    property bool menuOpen: false
    property bool qualityPopupOpen: false
    property bool controlsVisible: true
    readonly property color videoStateColor: root.liveState === "online" ? Theme.videoFocus
                                          : root.playbackState === "error" ? "#fe9b93" : Theme.videoMutedText
    readonly property bool hasTransientControls: menuOpen || qualityPopupOpen
    activeFocusOnTab: true
    Keys.onEscapePressed: function(event) {
        if (root.menuOpen) { root.menuOpen = false; event.accepted = true }
        else event.accepted = false
    }
    property string attachedPlayerRoomId: ""
    readonly property string density: root.width >= 320
                                      ? "comfortable"
                                      : (root.width >= 180 ? "compact" : "dense")
    readonly property bool compact: density === "compact"
    readonly property bool dense: density === "dense"
    readonly property string displayAnchorName: root.anchorName.trim().length > 0 ? root.anchorName : root.roomId
    readonly property string displayTitle: root.title.trim().length > 0 ? root.title : "斗鱼直播间"
    readonly property string displayCategory: root.category.trim().length > 0 ? root.category : "未分类"
    readonly property string displayViewerLabel: root.viewerLabel.trim().length > 0 ? root.viewerLabel : "--"
    readonly property var qualityOptions: root.availableQualities && root.availableQualities.length > 0
                                         ? root.availableQualities
                                         : [{"id": "auto", "label": "自动", "quality": "auto"}]

    function qualityIndex(token, rate) {
        for (var i = 0; i < qualityOptions.length; ++i) {
            const optionRate = Number(qualityOptions[i].rate)
            if (Number.isFinite(optionRate)) {
                if (optionRate === rate) return i
            } else if (qualityOptions[i].quality === token || qualityOptions[i].id === token) {
                return i
            }
        }
        return 0
    }

    function qualityEnum(token) {
        switch (token) {
        case "original": return 1
        case "super": return 2
        case "high": return 3
        case "standard": return 4
        default: return 0
        }
    }

    Timer {
        id: pictureDoubleClickGuard
        interval: Qt.styleHints.mouseDoubleClickInterval
        repeat: false
    }

    Timer {
        id: controlsTimer
        objectName: "roomControlsTimer"
        interval: 2200
        repeat: false
        onTriggered: {
            if (root.hasTransientControls || roomVolumeSlider.pressed
                    || root.childControlHasFocus(topBar) || root.childControlHasFocus(bottomBar)) restart()
            else root.controlsVisible = false
        }
    }

    function revealControls() {
        controlsVisible = true
        controlsTimer.restart()
    }

    function childControlHasFocus(item) {
        if (item.activeFocus) return true
        for (const child of item.children) {
            if (childControlHasFocus(child)) return true
        }
        return false
    }

    function clearChildControlFocus(item) {
        for (const child of item.children) clearChildControlFocus(child)
        item.focus = false
    }

    function focusVideoSurface() {
        clearChildControlFocus(topBar)
        clearChildControlFocus(bottomBar)
        root.forceActiveFocus(Qt.MouseFocusReason)
        revealControls()
    }

    Component.onCompleted: controlsTimer.start()

    function attachPlayerForCurrentRoom(player) {
        if (!player || !root.controller || root.roomId.trim().length === 0) return
        if (root.attachedPlayerRoomId === root.roomId) return
        if (root.attachedPlayerRoomId.length > 0) {
            root.controller.detachPlayer(root.attachedPlayerRoomId, player)
        }
        root.controller.attachPlayer(root.roomId, player)
        root.attachedPlayerRoomId = root.roomId
    }

    function detachAttachedPlayer(player) {
        if (!player || !root.controller || root.attachedPlayerRoomId.length === 0) return
        root.controller.detachPlayer(root.attachedPlayerRoomId, player)
        root.attachedPlayerRoomId = ""
    }

    function requestRoomRemoval(roomId) {
        const controller = root.controller
        const requestedRoomId = String(roomId || "")
        if (!controller || requestedRoomId.length === 0) return
        controller.requestRemoveRoom(requestedRoomId)
    }

    Rectangle {
        id: roomCardSurface
        objectName: "roomCardSurface"
        property color frameColor: root.primary ? root.accentColor : Theme.borderStrong
        anchors.fill: parent
        radius: Theme.radiusMedium
        color: Theme.videoBackground
        border.width: 1
        border.color: root.primary
                      ? Qt.rgba(frameColor.r, frameColor.g, frameColor.b, 0.58)
                      : frameColor
        clip: true

        Loader {
            id: playerLoader
            anchors.fill: parent
            active: root.renderEnabled && root.controller !== null
            sourceComponent: Component {
                MpvQuickItem {
                    id: player
                    Component.onCompleted: root.attachPlayerForCurrentRoom(player)
                    Component.onDestruction: root.detachAttachedPlayer(player)
                }
            }
        }

        Rectangle {
            objectName: "roomPrimaryIndicator"
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: root.primary ? 3 : 0
            z: 2
            color: root.accentColor
            visible: root.primary
        }

        DanmakuOverlay {
            anchors.fill: parent
            roomId: root.roomId
            controller: root.controller ? root.controller.danmaku : null
            signalController: root.controller ? root.controller.danmaku : null
            enabled: root.danmakuEnabled && root.controller !== null
                     && root.controller.danmaku.globalEnabled
            topInset: topBar.height
            bottomInset: bottomBar.visible ? bottomBar.height : 0
        }

        Rectangle {
            objectName: "danmakuWaitingLabel"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: topBar.height + 7
            width: waitingLabel.width + 14
            height: 21
            radius: Theme.radiusSmall
            z: 4
            visible: root.danmakuState === "waiting"
            color: Qt.rgba(0.92, 0.68, 0.24, 0.16)
            border.color: Qt.rgba(0.92, 0.68, 0.24, 0.56)
            Text {
                id: waitingLabel
                anchors.centerIn: parent
                color: "#e7b95d"
                font.pixelSize: Theme.fontCaption
                text: "等待弹幕会话"
            }
        }

        Rectangle {
            anchors.fill: parent
            visible: !root.renderEnabled || root.controller === null
            color: Theme.videoSurface
            Text {
                anchors.centerIn: parent
                color: Theme.videoMutedText
                font.pixelSize: 12
                text: !root.renderEnabled
                      ? "预览模式"
                      : (root.playbackState === "offline" ? "未开播" : "等待播放")
            }
        }

        Rectangle {
            id: topBar
            objectName: "roomTopBar"
            parent: root
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: root.dense ? 28 : root.compact ? 32 : 36
            z: 100
            color: Qt.rgba(Theme.videoSurface.r, Theme.videoSurface.g, Theme.videoSurface.b, 0.94)
            opacity: root.controlsVisible ? 1 : 0
            visible: root.controlsVisible || opacity > 0
            enabled: root.controlsVisible
            Behavior on opacity { NumberAnimation { duration: Theme.motionNormal } }

            Rectangle {
                objectName: "roomTopBarRule"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 1
                color: Qt.rgba(Theme.borderStrong.r, Theme.borderStrong.g,
                                Theme.borderStrong.b, 0.42)
            }

            Item {
                id: roomTopMetadata
                objectName: "roomTopMetadata"
                anchors.left: parent.left
                anchors.leftMargin: 9
                anchors.right: roomTopActions.left
                anchors.rightMargin: 4
                anchors.verticalCenter: parent.verticalCenter

                Rectangle {
                    id: liveStatusBadge
                    property color statusColor: root.videoStateColor
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    height: root.dense ? 17 : 19
                    width: statusBadgeContent.implicitWidth + (root.dense ? 8 : 12)
                    radius: Theme.radiusSmall
                    color: Qt.rgba(statusColor.r, statusColor.g, statusColor.b, 0.12)
                    border.color: Qt.rgba(statusColor.r, statusColor.g, statusColor.b, 0.48)
                    Row {
                        id: statusBadgeContent
                        anchors.centerIn: parent
                        spacing: 4
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 5
                            height: 5
                            radius: 3
                            color: liveStatusBadge.statusColor
                        }
                        Text {
                            id: statusLabel
                            objectName: "roomStatusLabel"
                            color: liveStatusBadge.statusColor
                            font.bold: true
                            font.pixelSize: Theme.fontCaption
                            text: root.liveState === "online"
                                  ? "直播中" : root.liveState === "offline" ? "未开播" : "检查中"
                        }
                    }
                }

                Rectangle {
                    id: primaryRoomBadge
                    objectName: "primaryRoomBadge"
                    property string text: root.secondaryPrimary
                        ? "主画面 2"
                        : (root.primary && root.controller && root.controller.workspace
                           && root.controller.workspace.layoutMode === "primary-two"
                           ? "主画面 1" : "主画面")
                    anchors.left: liveStatusBadge.right
                    anchors.leftMargin: visible ? 6 : 0
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.primary || root.secondaryPrimary
                    width: visible ? primaryRoomLabel.width + (root.dense ? 8 : 12) : 0
                    height: root.dense ? 17 : 19
                    radius: Theme.radiusSmall
                    color: Qt.rgba(Theme.videoFocus.r, Theme.videoFocus.g, Theme.videoFocus.b, 0.16)
                    border.color: Qt.rgba(Theme.videoFocus.r, Theme.videoFocus.g, Theme.videoFocus.b, 0.52)
                    Text {
                        id: primaryRoomLabel
                        anchors.centerIn: parent
                        color: Theme.videoFocus
                        font.bold: true
                        font.pixelSize: Theme.fontCaption
                        text: primaryRoomBadge.text
                    }
                }

                Text {
                    id: categoryLabel
                    anchors.left: (root.primary || root.secondaryPrimary) ? primaryRoomBadge.right : liveStatusBadge.right
                    anchors.leftMargin: 7
                    anchors.right: viewerLabel.left
                    anchors.rightMargin: 7
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !root.compact && !root.dense
                    color: Theme.videoText
                    elide: Text.ElideRight
                    font.pixelSize: Theme.fontCaption
                    text: root.displayCategory
                }
                Text {
                    id: denseAnchorLabel
                    anchors.left: (root.primary || root.secondaryPrimary)
                                  ? primaryRoomBadge.right : liveStatusBadge.right
                    anchors.leftMargin: 6
                    anchors.right: viewerLabel.left
                    anchors.rightMargin: 4
                    anchors.verticalCenter: parent.verticalCenter
                    visible: root.dense
                    width: Math.max(0, parent.width - x - anchors.leftMargin
                                    - anchors.rightMargin)
                    color: Theme.videoText
                    elide: Text.ElideRight
                    font.bold: true
                    font.pixelSize: Theme.fontCaption
                    text: root.displayAnchorName
                }
                Text {
                    id: viewerLabel
                    objectName: "roomViewerLabel"
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    visible: !root.compact && !root.dense
                    width: 74
                    color: Theme.videoMutedText
                    elide: Text.ElideRight
                    font.pixelSize: Theme.fontCaption
                    horizontalAlignment: Text.AlignRight
                    text: "热度 " + root.displayViewerLabel
                }
            }

            ConsoleToolButton {
                id: roomTopActions
                objectName: "roomTopActions"
                anchors.right: parent.right
                anchors.rightMargin: root.dense ? 4 : 7
                anchors.verticalCenter: parent.verticalCenter
                width: root.dense ? 30 : 34
                height: root.dense ? 24 : 28
                padding: 4
                Accessible.name: "更多操作"
                ToolTip.text: Accessible.name
                flat: true
                videoSurface: true
                onClicked: { root.menuOpen = !root.menuOpen; root.revealControls() }
                contentItem: ThemeIcon { tint: roomTopActions.contentColor; source: Qt.resolvedUrl("../assets/icons/ellipsis.svg") }
            }
        }

        Popup {
            id: roomMenuPopup
            parent: Overlay.overlay
            property point origin: Qt.point(0, 0)
            width: 172
            height: menuColumn.implicitHeight + 12
            x: parent ? Math.max(7, Math.min(parent.width - width - 7, origin.x - width - 7)) : 0
            y: parent ? Math.max(7, Math.min(parent.height - height - 7, origin.y)) : 0
            padding: 0
            margins: 7
            focus: true
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
            onAboutToShow: origin = root.mapToItem(parent, root.width, topBar.height + 7)
            onClosed: root.menuOpen = false

            contentItem: Rectangle {
                id: menu
                objectName: "roomControlMenu"
                radius: Theme.radiusSmall
                color: Theme.well
                border.color: root.borderColor

                Column {
                    id: menuColumn
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 6
                    spacing: 2
                    Text { width: parent.width; height: 27; verticalAlignment: Text.AlignVCenter; color: Theme.mutedText; font.pixelSize: Theme.fontCaption; elide: Text.ElideRight; text: root.playbackState === "error" ? "播放状态异常" : root.playbackState === "playing" ? "正在播放" : root.playbackState === "loading" ? "加载播放源" : root.playbackState === "reconnecting" ? "正在恢复播放" : root.liveState === "offline" ? "未开播" : "等待播放" }
                    ConsoleToolButton {
                        objectName: "refreshRoomAction"
                        width: parent.width; height: 27
                        padding: 4
                        Accessible.name: "刷新房间数据"
                        ToolTip.text: Accessible.name
                        flat: true
                        onClicked: { if (root.controller) root.controller.refreshRoom(root.roomId); root.menuOpen = false }
                        contentItem: Text { text: "刷新房间数据"; color: Theme.text; leftPadding: 6; verticalAlignment: Text.AlignVCenter; font.pixelSize: Theme.fontCaption }
                    }
                    ConsoleToolButton {
                        objectName: "secondaryPrimaryRoomAction"
                        visible: !!(root.controller && root.controller.workspace
                                    && root.controller.workspace.layoutMode === "primary-two")
                        width: parent.width; height: 27
                        padding: 4
                        Accessible.name: root.secondaryPrimary ? "当前主画面 2" : "设为主画面 2"
                        ToolTip.text: Accessible.name
                        flat: true
                        onClicked: { if (root.controller) root.controller.setSecondaryPrimaryRoom(root.roomId); root.menuOpen = false }
                        contentItem: Text { text: root.secondaryPrimary ? "当前主画面 2" : "设为主画面 2"; color: Theme.text; leftPadding: 6; verticalAlignment: Text.AlignVCenter; font.pixelSize: Theme.fontCaption }
                    }
                    ConsoleToolButton {
                        width: parent.width; height: 27
                        padding: 4
                        Accessible.name: "重新检查播放源"
                        ToolTip.text: Accessible.name
                        flat: true
                        onClicked: { if (root.controller) root.controller.retryPlayback(root.roomId); root.menuOpen = false }
                        contentItem: Text { text: "重新检查播放源"; color: Theme.text; leftPadding: 6; verticalAlignment: Text.AlignVCenter; font.pixelSize: Theme.fontCaption }
                    }
                    ConsoleToolButton {
                        width: parent.width; height: 27
                        padding: 4
                        Accessible.name: root.primary ? "当前主画面 1" : "设为主画面 1"
                        ToolTip.text: Accessible.name
                        flat: true
                        onClicked: { if (root.controller) root.controller.setPrimaryRoom(root.roomId); root.menuOpen = false }
                        contentItem: Text { text: root.controller && root.controller.workspace && root.controller.workspace.layoutMode === "primary-two" ? (root.primary ? "当前主画面 1" : "设为主画面 1") : (root.primary ? "当前主画面" : "设为主画面"); color: Theme.text; leftPadding: 6; verticalAlignment: Text.AlignVCenter; font.pixelSize: Theme.fontCaption }
                    }
                    ConsoleToolButton {
                        id: removeRoomAction
                        width: parent.width; height: 27
                        padding: 4
                        Accessible.name: "移除房间"
                        ToolTip.text: Accessible.name
                        flat: true
                        destructive: true
                        onClicked: { root.menuOpen = false; root.requestRoomRemoval(root.roomId) }
                        contentItem: Text {
                            text: "移除房间"
                            color: removeRoomAction.hovered ? Theme.danger : Theme.text
                            leftPadding: 6
                            verticalAlignment: Text.AlignVCenter
                            font.pixelSize: Theme.fontCaption
                        }
                    }
                }
            }
        }

        Rectangle {
            id: bottomBar
            objectName: "roomBottomBar"
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: root.dense ? 22 : root.compact ? 64 : 58
            visible: !root.dense && (root.controlsVisible || opacity > 0)
            color: Qt.rgba(Theme.videoSurface.r, Theme.videoSurface.g, Theme.videoSurface.b, 0.94)
            opacity: root.controlsVisible ? 1 : 0
            enabled: root.controlsVisible
            Behavior on opacity { NumberAnimation { duration: Theme.motionNormal } }

            Rectangle {
                objectName: "roomBottomBarRule"
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                height: 1
                color: Qt.rgba(Theme.borderStrong.r, Theme.borderStrong.g,
                                Theme.borderStrong.b, 0.48)
            }

            Column {
                anchors.left: parent.left
                anchors.leftMargin: root.compact ? 42 : 52
                anchors.right: root.compact ? parent.right : tileActions.left
                anchors.rightMargin: root.compact ? 8 : 10
                anchors.top: root.compact ? parent.top : undefined
                anchors.topMargin: root.compact ? 4 : 0
                anchors.verticalCenter: root.compact ? undefined : parent.verticalCenter
                spacing: 3
                Text { objectName: "roomAnchorName"; width: parent.width; color: root.textColor; elide: Text.ElideRight; font.bold: true; font.pixelSize: Theme.fontControl; text: root.displayAnchorName }
                Text { objectName: "roomTitleText"; width: parent.width; visible: !root.compact; color: root.mutedTextColor; elide: Text.ElideRight; font.pixelSize: Theme.fontCaption; text: root.displayTitle }
                Text { objectName: "roomTitle"; visible: false; text: root.displayTitle }
            }

            Rectangle {
                id: avatarFrame
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.top: root.compact ? parent.top : undefined
                anchors.topMargin: root.compact ? 3 : 0
                anchors.verticalCenter: root.compact ? undefined : parent.verticalCenter
                width: root.compact ? 26 : 34
                height: root.compact ? 26 : 34
                radius: width / 2
                color: root.primary
                       ? Qt.rgba(Theme.videoFocus.r, Theme.videoFocus.g, Theme.videoFocus.b, 0.18)
                       : Theme.videoSurface
                border.width: 1
                border.color: root.primary
                              ? Qt.rgba(Theme.videoFocus.r, Theme.videoFocus.g,
                                        Theme.videoFocus.b, 0.62)
                              : Qt.rgba(Theme.videoMutedText.r, Theme.videoMutedText.g, Theme.videoMutedText.b, 0.36)

                Image {
                    id: roomAvatarImage
                    objectName: "roomAvatarImage"
                    anchors.fill: parent
                    anchors.margins: 1
                    source: root.avatarUrl
                    visible: root.avatarUrl.toString().length > 0 && status !== Image.Error
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    smooth: true
                    clip: true
                }

                Text {
                    anchors.centerIn: parent
                    visible: !roomAvatarImage.visible
                    color: root.textColor
                    font.bold: true
                    font.pixelSize: Theme.fontControl
                    text: root.displayAnchorName.slice(0, 1)
                }
            }

            Row {
                id: tileActions
                objectName: "roomActionBar"
                anchors.right: parent.right
                anchors.rightMargin: 8
                y: root.compact ? parent.height - height - 5 : (parent.height - height) / 2
                height: root.compact ? 27 : implicitHeight
                spacing: 3

                Rectangle {
                    objectName: "roomActionDivider"
                    visible: !root.compact
                    width: 1
                    height: 22
                    color: Qt.rgba(Theme.borderStrong.r, Theme.borderStrong.g,
                                    Theme.borderStrong.b, 0.62)
                }

                Slider {
                    id: roomVolumeSlider
                    objectName: "roomVolumeSlider"
                    visible: !root.compact
                    width: 76
                    height: 27
                    from: 0
                    to: 1
                    palette.midlight: Qt.rgba(Theme.videoMutedText.r, Theme.videoMutedText.g, Theme.videoMutedText.b, 0.26)
                    palette.dark: Theme.videoFocus
                    value: Math.max(0, Math.min(1, root.volume / 100))
                    Accessible.name: "房间音量"
                    ToolTip.visible: false
                    ToolTip.text: Accessible.name + " " + Math.round(value * 100) + "%"
                    onMoved: {
                        if (root.controller) root.controller.setVolume(root.roomId, Math.round(value * 100))
                    }
                    handle: Rectangle {
                        objectName: "roomVolumeSliderHandle"
                        implicitWidth: 10
                        implicitHeight: 10
                        x: roomVolumeSlider.leftPadding
                           + roomVolumeSlider.visualPosition
                             * (roomVolumeSlider.availableWidth - width)
                        y: roomVolumeSlider.topPadding
                           + (roomVolumeSlider.availableHeight - height) / 2
                        radius: width / 2
                        color: roomVolumeSlider.pressed ? Theme.videoFocus : Theme.videoText
                        border.color: Theme.videoSurface
                        border.width: 1
                    }
                }
                ConsoleToolButton {
                    id: primaryRoomAction
                    objectName: "primaryRoomAction"
                    width: 27; height: 27
                    padding: 4
                    Accessible.name: root.primary ? "当前主画面" : "设为主画面"
                    ToolTip.text: Accessible.name
                    flat: true
                    videoSurface: true
                    checkable: false
                    checked: root.primary
                    onClicked: if (root.controller) root.controller.setPrimaryRoom(root.roomId)
                    contentItem: ThemeIcon { tint: primaryRoomAction.contentColor; source: Qt.resolvedUrl("../assets/icons/pin.svg") }
                }
                ConsoleToolButton {
                    id: roomAudioButton
                    objectName: "roomAudioButton"
                    width: 27; height: 27
                    padding: 4
                    Accessible.name: root.controller && root.controller.workspace
                                     && root.controller.workspace.audioMode === "multi"
                                     ? (root.audioFocused ? "静音直播间" : "取消直播间静音")
                                     : (root.audioFocused ? "关闭声音焦点" : "播放声音")
                    ToolTip.text: Accessible.name
                    flat: true
                    videoSurface: true
                    checkable: false
                    checked: root.audioFocused
                    onClicked: {
                        if (!root.controller) return
                        if (root.controller.workspace && root.controller.workspace.audioMode === "multi") {
                            root.controller.setRoomMuted(root.roomId, !root.muted)
                        } else {
                            const error = root.controller.setAudioRoom(root.audioFocused ? "" : root.roomId)
                            if (error && root.controller.workspace) {
                                root.controller.workspace.setLastMessage(error, "error", 3200)
                            }
                        }
                    }
                    contentItem: ThemeIcon { tint: roomAudioButton.contentColor; source: Qt.resolvedUrl("../assets/icons/volume-2.svg") }
                }
                ConsoleToolButton {
                    id: danmakuRetryAction
                    objectName: "danmakuRetryAction"
                    width: 27; height: 27
                    padding: 4
                    Accessible.name: root.danmakuState === "failed" || root.danmakuState === "platform-blocked"
                                     ? "重试弹幕连接"
                                     : (root.danmakuState === "waiting"
                                        ? "等待弹幕会话"
                                        : (root.danmakuEnabled ? "隐藏弹幕" : "显示弹幕"))
                    ToolTip.text: root.danmakuErrorCode === "AUTH_REQUIRED"
                                  ? "弹幕连接需要认证，点击重试" : Accessible.name
                    flat: true
                    videoSurface: true
                    checkable: false
                    checked: root.danmakuEnabled
                    onClicked: {
                        if (!root.controller) return
                        if (root.danmakuState === "failed" || root.danmakuState === "platform-blocked") {
                            root.controller.danmaku.retry(root.roomId)
                        } else if (root.danmakuState === "waiting") {
                            return
                        } else {
                            root.controller.toggleDanmaku(root.roomId)
                        }
                    }
                    contentItem: Item {
                        ThemeIcon {
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            source: Qt.resolvedUrl("../assets/icons/message-circle.svg")
                            tint: danmakuRetryAction.contentColor
                        }
                        Rectangle {
                            objectName: "danmakuConnectedIndicator"
                            anchors.right: parent.right
                            anchors.top: parent.top
                            width: 6
                            height: 6
                            radius: 3
                            visible: root.danmakuState === "connected"
                                     || root.danmakuState === "failed"
                                     || root.danmakuState === "platform-blocked"
                                     || root.danmakuState === "waiting"
                            color: root.danmakuState === "connected"
                                   ? Theme.videoFocus
                                   : (root.danmakuState === "waiting" ? "#e7b95d" : "#fe9b93")
                            border.color: Theme.videoSurface
                            border.width: 1
                        }
                    }
                }
                ConsoleComboBox {
                    id: qualityBox
                    objectName: "roomQualitySelector"
                    width: Math.max(82, Math.min(112, implicitContentWidth + 16))
                    height: 27
                    padding: 4
                    leftPadding: 8
                    rightPadding: 8
                    videoSurface: true
                    model: root.qualityOptions
                    textRole: "label"
                    currentIndex: root.qualityIndex(root.requestedQuality, root.requestedQualityRate)
                    Accessible.name: "清晰度"
                    indicator: Item { visible: false; width: 0; height: 0 }
                    onActivated: function(optionIndex) {
                        if (!root.controller || optionIndex < 0 || optionIndex >= root.qualityOptions.length) return
                        const option = root.qualityOptions[optionIndex]
                        const optionRate = Number(option.rate)
                        root.controller.setQuality(root.roomId,
                                                   root.qualityEnum(option.quality),
                                                   Number.isFinite(optionRate) ? optionRate : -1)
                    }
                    contentItem: Text {
                        objectName: "roomQualityLabel"
                        color: Theme.videoText
                        text: qualityBox.displayText
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideNone
                    }
                    popup: Popup {
                        objectName: "roomQualityPopup"
                        onVisibleChanged: root.qualityPopupOpen = visible
                        enter: Transition {
                            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motionPopup; easing.type: Easing.OutCubic }
                        }
                        onClosed: pictureDoubleClickGuard.restart()
                        y: qualityBox.height + 2
                        width: qualityBox.width
                        implicitHeight: contentItem.implicitHeight
                        padding: 1
                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: Theme.videoSurface
                            border.color: Qt.rgba(Theme.videoMutedText.r, Theme.videoMutedText.g, Theme.videoMutedText.b, 0.36)
                            border.width: Theme.borderWidth
                        }
                        contentItem: ListView {
                            objectName: "roomQualityPopupList"
                            clip: true
                            implicitHeight: contentHeight
                            model: qualityBox.delegateModel
                            currentIndex: qualityBox.highlightedIndex
                            interactive: false
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AlwaysOff }
                        }
                    }
                    delegate: ItemDelegate {
                        id: qualityOptionDelegate
                        required property var model
                        required property int index

                        width: ListView.view.width
                        height: 27
                        leftPadding: 7
                        rightPadding: 7
                        topPadding: 4
                        bottomPadding: 4
                        highlighted: qualityBox.highlightedIndex === index
                        hoverEnabled: qualityBox.hoverEnabled
                        contentItem: Text {
                            objectName: "roomQualityOptionLabel"
                            text: qualityOptionDelegate.model[qualityBox.textRole] !== undefined
                                  ? qualityOptionDelegate.model[qualityBox.textRole] : qualityBox.textAt(qualityOptionDelegate.index)
                            color: Theme.videoText
                            font.pixelSize: Theme.fontCaption
                            elide: Text.ElideNone
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: Theme.radiusSmall
                            color: qualityOptionDelegate.highlighted
                                   ? Qt.rgba(Theme.videoFocus.r, Theme.videoFocus.g, Theme.videoFocus.b, 0.16)
                                   : qualityOptionDelegate.hovered
                                     ? Qt.rgba(Theme.videoText.r, Theme.videoText.g, Theme.videoText.b, 0.10)
                                     : "transparent"
                        }
                    }
                }
            }
        }

    }

    MouseArea {
        objectName: "roomPictureDoubleClickArea"
        anchors.fill: parent
        anchors.topMargin: root.controlsVisible ? topBar.height : 0
        anchors.bottomMargin: bottomBar.visible ? bottomBar.height : 0
        enabled: !root.menuOpen && !qualityBox.popup.visible
                 && !pictureDoubleClickGuard.running
        acceptedButtons: Qt.LeftButton
        onPressed: root.focusVideoSurface()
        onDoubleClicked: root.fullScreenToggleRequested()
    }

    HoverHandler {
        onHoveredChanged: {
            if (hovered) root.revealControls()
            else controlsTimer.restart()
        }
        onPointChanged: if (hovered) root.revealControls()
    }

    onMenuOpenChanged: {
        if (menuOpen) {
            revealControls()
            roomMenuPopup.open()
        }
        else {
            roomMenuPopup.close()
            controlsTimer.restart()
            pictureDoubleClickGuard.restart()
        }
    }

    onActiveFocusChanged: {
        if (activeFocus) revealControls()
        else if (!menuOpen) controlsTimer.restart()
    }

    onRoomIdChanged: {
        if (playerLoader.item) attachPlayerForCurrentRoom(playerLoader.item)
    }
}
