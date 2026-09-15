import QtQuick
import QtQuick.Controls
import DouyuNative
import ".."

FocusScope {
    id: root

    implicitWidth: 320
    implicitHeight: 180

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
    required property string effectiveQuality
    required property bool muted
    required property int volume
    required property bool danmakuEnabled
    required property string danmakuState
    required property string danmakuErrorCode
    required property int index
    property var availableQualities: []
    property var controller: null
    property color borderColor: Theme.border
    property color accentColor: Theme.accent
    property color textColor: Theme.text
    property color mutedTextColor: Theme.mutedText
    property bool menuOpen: false
    property bool controlsVisible: true
    property string attachedPlayerRoomId: ""
    readonly property string displayAnchorName: root.anchorName.trim().length > 0 ? root.anchorName : root.roomId
    readonly property string displayTitle: root.title.trim().length > 0 ? root.title : "斗鱼直播间"
    readonly property string displayCategory: root.category.trim().length > 0 ? root.category : "未分类"
    readonly property string displayViewerLabel: root.viewerLabel.trim().length > 0 ? root.viewerLabel : "--"
    readonly property var qualityOptions: root.availableQualities && root.availableQualities.length > 0
                                         ? root.availableQualities
                                         : [{"id": "auto", "label": "自动", "quality": "auto"}]

    function qualityIndex(token) {
        for (var i = 0; i < qualityOptions.length; ++i) {
            if (qualityOptions[i].quality === token || qualityOptions[i].id === token) return i
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
        id: controlsTimer
        interval: 2200
        repeat: false
        onTriggered: {
            if (!root.activeFocus && !root.menuOpen) root.controlsVisible = false
        }
    }

    function revealControls() {
        controlsVisible = true
        controlsTimer.restart()
    }

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
        color: Theme.well
        border.width: root.primary ? 2 : 1
        border.color: frameColor
        clip: true

        Loader {
            id: playerLoader
            anchors.fill: parent
            active: root.controller !== null
            sourceComponent: Component {
                MpvQuickItem {
                    id: player
                    Component.onCompleted: root.attachPlayerForCurrentRoom(player)
                    Component.onDestruction: root.detachAttachedPlayer(player)
                }
            }
        }

        DanmakuOverlay {
            anchors.fill: parent
            roomId: root.roomId
            controller: root.controller ? root.controller.danmaku : null
            signalController: root.controller ? root.controller.danmaku : null
            enabled: root.danmakuEnabled && root.controller !== null
                     && root.controller.danmaku.globalEnabled
            topInset: topBar.height
            bottomInset: bottomBar.height
        }

        Rectangle {
            anchors.fill: parent
            visible: root.controller === null
            color: root.index % 3 === 0 ? Theme.controlSurface : root.index % 3 === 1 ? "#29241f" : "#202b2a"
            Text {
                anchors.centerIn: parent
                color: "#657181"
                font.pixelSize: 12
                text: root.playbackState === "offline" ? "未开播" : "等待播放"
            }
        }

        Rectangle {
            id: topBar
            objectName: "roomTopBar"
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 36
            z: 10
            color: Qt.rgba(Theme.appBar.r, Theme.appBar.g, Theme.appBar.b, 0.88)
            opacity: root.controlsVisible ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 150 } }

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
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    height: 19
                    width: statusLabel.width + 12
                    radius: Theme.radiusSmall
                    color: root.liveState === "online"
                           ? Qt.rgba(Theme.online.r, Theme.online.g, Theme.online.b, 0.14)
                           : (root.liveState === "offline"
                              ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.14)
                              : Theme.controlSurface)
                    border.color: root.liveState === "online"
                                  ? Qt.rgba(Theme.online.r, Theme.online.g, Theme.online.b, 0.48)
                                  : (root.liveState === "offline"
                                     ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.48)
                                     : Theme.borderStrong)
                    Text {
                        id: statusLabel
                        objectName: "roomStatusLabel"
                        anchors.centerIn: parent
                        color: root.liveState === "online"
                               ? Theme.online
                               : (root.liveState === "offline" ? Theme.danger : Theme.mutedText)
                        font.bold: true
                        font.pixelSize: 9
                        text: root.liveState === "online" ? "直播中" : root.liveState === "offline" ? "未开播" : "检查中"
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
                    width: visible ? primaryRoomLabel.width + 12 : 0
                    height: 19
                    radius: Theme.radiusSmall
                    color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.16)
                    border.color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.52)
                    Text {
                        id: primaryRoomLabel
                        anchors.centerIn: parent
                        color: root.accentColor
                        font.bold: true
                        font.pixelSize: 9
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
                    color: Theme.text
                    elide: Text.ElideRight
                    font.pixelSize: 10
                    text: root.displayCategory
                }
                Text {
                    id: viewerLabel
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    width: 74
                    color: Theme.mutedText
                    elide: Text.ElideRight
                    font.pixelSize: 9
                    horizontalAlignment: Text.AlignRight
                    text: root.displayViewerLabel + " 人观看"
                }
            }

            ToolButton {
                id: roomTopActions
                objectName: "roomTopActions"
                anchors.right: parent.right
                anchors.rightMargin: 7
                anchors.verticalCenter: parent.verticalCenter
                width: 34
                height: 28
                Accessible.name: "更多操作"
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: { root.menuOpen = !root.menuOpen; root.revealControls() }
                contentItem: Image { anchors.centerIn: parent; width: 16; height: 16; source: Qt.resolvedUrl("../assets/icons/ellipsis.svg") }
                background: Rectangle { radius: Theme.radiusSmall; color: parent.hovered ? Theme.controlSurface : "transparent" }
            }
        }

        Rectangle {
            id: menu
            visible: root.menuOpen
            z: 3
            width: 172
            height: 156
            anchors.top: topBar.bottom
            anchors.right: parent.right
            anchors.margins: 7
            radius: 6
            color: "#141a21"
            border.color: root.borderColor

            Column {
                anchors.fill: parent
                anchors.margins: 6
                spacing: 2
                Text { width: parent.width; height: 27; verticalAlignment: Text.AlignVCenter; color: root.mutedTextColor; font.pixelSize: 10; elide: Text.ElideRight; text: root.playbackState === "error" ? "播放状态异常" : "播放状态正常" }
                ToolButton {
                    objectName: "refreshRoomAction"
                    width: parent.width; height: 27
                    Accessible.name: "刷新房间数据"
                    ToolTip.visible: hovered; ToolTip.text: Accessible.name
                    onClicked: { if (root.controller) root.controller.refreshRoom(root.roomId); root.menuOpen = false }
                    contentItem: Text { text: "刷新房间数据"; color: "#d6dde5"; leftPadding: 6; verticalAlignment: Text.AlignVCenter; font.pixelSize: 10 }
                    background: Rectangle { radius: 4; color: parent.hovered ? "#241b17" : "transparent" }
                }
                ToolButton {
                    visible: !!(root.controller && root.controller.workspace
                                && root.controller.workspace.layoutMode === "primary-two")
                    width: parent.width; height: 27
                    Accessible.name: root.secondaryPrimary ? "当前主画面 2" : "设为主画面 2"
                    onClicked: { if (root.controller) root.controller.setSecondaryPrimaryRoom(root.roomId); root.menuOpen = false }
                    contentItem: Text { text: root.secondaryPrimary ? "当前主画面 2" : "设为主画面 2"; color: "#d6dde5"; leftPadding: 6; verticalAlignment: Text.AlignVCenter; font.pixelSize: 10 }
                    background: Rectangle { radius: 4; color: parent.hovered ? "#241b17" : "transparent" }
                }
                ToolButton {
                    width: parent.width; height: 27
                    Accessible.name: "重新检查播放源"
                    onClicked: { if (root.controller) root.controller.retryPlayback(root.roomId); root.menuOpen = false }
                    contentItem: Text { text: "重新检查播放源"; color: "#d6dde5"; leftPadding: 6; verticalAlignment: Text.AlignVCenter; font.pixelSize: 10 }
                    background: Rectangle { radius: 4; color: parent.hovered ? "#241b17" : "transparent" }
                }
                ToolButton {
                    width: parent.width; height: 27
                    Accessible.name: root.primary ? "当前主画面 1" : "设为主画面 1"
                    onClicked: { if (root.controller) root.controller.setPrimaryRoom(root.roomId); root.menuOpen = false }
                    contentItem: Text { text: root.controller && root.controller.workspace && root.controller.workspace.layoutMode === "primary-two" ? (root.primary ? "当前主画面 1" : "设为主画面 1") : (root.primary ? "当前主画面" : "设为主画面"); color: "#d6dde5"; leftPadding: 6; verticalAlignment: Text.AlignVCenter; font.pixelSize: 10 }
                    background: Rectangle { radius: 4; color: parent.hovered ? "#241b17" : "transparent" }
                }
                ToolButton {
                    width: parent.width; height: 27
                    Accessible.name: "移除房间"
                    onClicked: { root.menuOpen = false; root.requestRoomRemoval(root.roomId) }
                    contentItem: Text { text: "移除房间"; color: parent.hovered ? "#ff9b92" : "#d6dde5"; leftPadding: 6; verticalAlignment: Text.AlignVCenter; font.pixelSize: 10 }
                    background: Rectangle { radius: 4; color: parent.hovered ? "#4b1c1c" : "transparent" }
                }
            }
        }

        Rectangle {
            id: bottomBar
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 58
            color: Qt.rgba(Theme.appBar.r, Theme.appBar.g, Theme.appBar.b, 0.92)
            opacity: root.controlsVisible ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 150 } }

            Column {
                anchors.left: parent.left
                anchors.leftMargin: 52
                anchors.right: tileActions.left
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3
                Text { objectName: "roomAnchorName"; width: parent.width; color: root.textColor; elide: Text.ElideRight; font.bold: true; font.pixelSize: 12; text: root.displayAnchorName }
                Text { objectName: "roomTitleText"; width: parent.width; color: root.mutedTextColor; elide: Text.ElideRight; font.pixelSize: 9; text: root.displayTitle }
                Text { objectName: "roomTitle"; visible: false; text: root.displayTitle }
            }

            Rectangle {
                id: avatarFrame
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                width: 34
                height: 34
                radius: 17
                color: root.primary ? "#3a2820" : "#28313a"

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
                    font.pixelSize: 12
                    text: root.displayAnchorName.slice(0, 1)
                }
            }

            Row {
                id: tileActions
                objectName: "roomActionBar"
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                spacing: 3
                Slider {
                    id: roomVolumeSlider
                    objectName: "roomVolumeSlider"
                    width: 76
                    height: 27
                    from: 0
                    to: 1
                    value: Math.max(0, Math.min(1, root.volume / 100))
                    Accessible.name: "房间音量"
                    ToolTip.visible: hovered
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
                        color: roomVolumeSlider.pressed ? Theme.accent : Theme.text
                        border.color: Theme.borderStrong
                        border.width: 1
                    }
                }
                ToolButton {
                    objectName: "danmakuRetryAction"
                    width: 27; height: 27
                    Accessible.name: root.primary ? "当前主画面" : "设为主画面"
                    ToolTip.visible: hovered; ToolTip.text: Accessible.name
                    onClicked: if (root.controller) root.controller.setPrimaryRoom(root.roomId)
                    contentItem: Image { anchors.centerIn: parent; width: 16; height: 16; source: Qt.resolvedUrl("../assets/icons/pin.svg"); opacity: root.primary ? 1 : 0.55 }
                    background: Rectangle { radius: 4; border.color: parent.hovered || root.primary ? "#9b572f" : "#4a515a"; color: parent.hovered ? "#2a211c" : "#10151b" }
                }
                ToolButton {
                    width: 27; height: 27
                    Accessible.name: root.controller && root.controller.workspace
                                     && root.controller.workspace.audioMode === "multi"
                                     ? (root.audioFocused ? "静音直播间" : "取消直播间静音")
                                     : (root.audioFocused ? "关闭声音焦点" : "播放声音")
                    ToolTip.visible: hovered; ToolTip.text: Accessible.name
                    onClicked: {
                        if (!root.controller) return
                        if (root.controller.workspace && root.controller.workspace.audioMode === "multi") {
                            root.controller.setRoomMuted(root.roomId, !root.muted)
                        } else {
                            root.controller.setAudioRoom(root.audioFocused ? "" : root.roomId)
                        }
                    }
                    contentItem: Image { anchors.centerIn: parent; width: 16; height: 16; source: Qt.resolvedUrl("../assets/icons/volume-2.svg"); opacity: root.audioFocused ? 1 : 0.62 }
                    background: Rectangle { radius: 4; border.color: parent.hovered || root.audioFocused ? "#9b572f" : "#4a515a"; color: parent.hovered ? "#2a211c" : "#10151b" }
                }
                ToolButton {
                    width: 27; height: 27
                    Accessible.name: root.danmakuState === "failed" || root.danmakuState === "platform-blocked"
                                     ? "重试弹幕连接" : root.danmakuEnabled ? "隐藏弹幕" : "显示弹幕"
                    ToolTip.visible: hovered
                    ToolTip.text: root.danmakuErrorCode === "AUTH_REQUIRED"
                                  ? "弹幕连接需要认证，点击重试" : Accessible.name
                    onClicked: {
                        if (!root.controller) return
                        if (root.danmakuState === "failed" || root.danmakuState === "platform-blocked") {
                            root.controller.danmaku.retry(root.roomId)
                        } else {
                            root.controller.toggleDanmaku(root.roomId)
                        }
                    }
                    contentItem: Item {
                        Image {
                            anchors.centerIn: parent
                            width: 16
                            height: 16
                            source: Qt.resolvedUrl("../assets/icons/message-circle.svg")
                            opacity: root.danmakuEnabled ? 1 : 0.62
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
                            color: root.danmakuState === "connected" ? "#55b975" : "#e57062"
                            border.color: "#10151b"
                            border.width: 1
                        }
                    }
                    background: Rectangle {
                        radius: 4
                        border.color: parent.hovered || root.danmakuEnabled ? "#9b572f" : "#4a515a"
                        color: parent.hovered ? "#2a211c" : "#10151b"
                    }
                }
                ComboBox {
                    id: qualityBox
                    width: 52
                    height: 27
                    model: root.qualityOptions
                    textRole: "label"
                    currentIndex: root.qualityIndex(root.requestedQuality)
                    Accessible.name: "清晰度"
                    onActivated: {
                        if (!root.controller || currentIndex < 0 || currentIndex >= root.qualityOptions.length) return
                        root.controller.setQuality(root.roomId,
                                                   root.qualityEnum(root.qualityOptions[currentIndex].quality))
                    }
                    contentItem: Text { leftPadding: 6; rightPadding: 4; color: "#d7dee5"; text: qualityBox.displayText; verticalAlignment: Text.AlignVCenter; font.pixelSize: 9 }
                    background: Rectangle { radius: 4; border.color: root.borderColor; color: "#10151b" }
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            onPositionChanged: root.revealControls()
            onEntered: root.revealControls()
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
