import QtQuick
import "DanmakuLaneScheduler.js" as DanmakuLaneScheduler

Item {
    id: root

    required property string roomId
    property var controller: null
    property var signalController: null
    property var activeItems: []
    property var pendingMessage: ({})
    property int activeItemLimit: 64
    property real topInset: 0
    property real bottomInset: 0

    readonly property var displaySettings: controller && controller.displaySettings
                                         ? controller.displaySettings : ({})
    readonly property int durationMs: Math.max(1, Number(displaySettings.durationSeconds || 8) * 1000)
    readonly property int fontSize: Math.max(1, Number(displaySettings.fontSize || 24))
    readonly property real messageOpacity: Math.max(0, Math.min(1, Number(displaySettings.opacity || 0.9)))
    readonly property string region: displaySettings.region || "full"
    readonly property string density: displaySettings.density || "normal"
    readonly property string fontFamily: displaySettings.fontFamily === "simhei" ? "SimHei" : "Microsoft YaHei"
    readonly property string rendering: displaySettings.rendering || "native"
    readonly property bool presentationSuspended: controller && controller.presentationSuspended
                                                  ? controller.presentationSuspended : false
    readonly property int launchInterval: density === "massive" ? 80 : density === "reduced" ? 360 : 180
    readonly property real safeTopInset: Math.max(0, topInset)
    readonly property real usableHeight: Math.max(0, height - safeTopInset - Math.max(0, bottomInset))

    clip: true
    z: 1

    TextMetrics {
        id: metrics
        text: root.pendingMessage.text || ""
        font.family: root.fontFamily
        font.pixelSize: root.fontSize
        font.bold: root.rendering === "advanced"
    }

    Component {
        id: danmakuLineComponent
        DanmakuLine { }
    }

    Timer {
        id: launchTimer
        interval: root.launchInterval
        repeat: true
        running: root.enabled && !root.presentationSuspended && root.controller !== null
                 && root.width > 0 && root.height > 0
        onTriggered: root.launchNextMessage()
    }

    Connections {
        target: root.signalController || root.controller

        function onMessageAvailable(roomId) {
            if (roomId === root.roomId && root.enabled) launchTimer.restart()
        }
    }

    function hasPendingMessage() {
        return pendingMessage && pendingMessage.text && pendingMessage.text.length > 0
    }

    function activeDescriptors() {
        const descriptors = []
        for (let index = 0; index < activeItems.length; ++index) {
            const item = activeItems[index]
            if (!item) continue
            descriptors.push({
                laneIndex: item.laneIndex,
                width: item.lineWidth,
                containerWidth: item.containerWidth,
                launchedAt: item.launchedAt,
                durationMs: item.durationMs,
                fontSize: item.fontSize
            })
        }
        return descriptors
    }

    function removeActiveItem(item) {
        const remaining = []
        for (let index = 0; index < activeItems.length; ++index) {
            const current = activeItems[index]
            if (current && current !== item) remaining.push(current)
        }
        activeItems = remaining
        if (item) item.destroy()
    }

    function clearActiveItems() {
        for (let index = 0; index < activeItems.length; ++index) {
            const item = activeItems[index]
            if (item) item.destroy()
        }
        activeItems = []
        pendingMessage = ({})
    }

    function clearRoom() {
        clearActiveItems()
    }

    function launchNextMessage() {
        if (!enabled || !controller || width <= 0 || usableHeight <= 0) return
        if (activeItems.length >= activeItemLimit) return

        if (!hasPendingMessage()) pendingMessage = controller.takeNextMessage(roomId)
        if (!hasPendingMessage()) return

        const candidate = {
            width: Math.ceil(metrics.advanceWidth) + 4,
            containerWidth: width,
            launchedAt: Date.now(),
            durationMs: durationMs,
            fontSize: fontSize
        }
        const lane = DanmakuLaneScheduler.selectLane(
            DanmakuLaneScheduler.lanes(usableHeight, fontSize, region, density),
            activeDescriptors(), candidate)
        if (lane === null) return

        const line = danmakuLineComponent.createObject(root, {
            messageText: pendingMessage.text,
            laneIndex: lane.index,
            laneTop: safeTopInset + lane.top,
            lineWidth: candidate.width,
            containerWidth: candidate.containerWidth,
            durationMs: candidate.durationMs,
            fontSize: candidate.fontSize,
            fontFamily: fontFamily,
            messageOpacity: messageOpacity,
            rendering: rendering,
            launchedAt: candidate.launchedAt
        })
        if (!line) return

        line.finished.connect(function() { root.removeActiveItem(line) })
        activeItems = activeItems.concat([line])
        pendingMessage = ({})
    }

    onEnabledChanged: {
        if (!enabled) clearRoom()
    }

    onPresentationSuspendedChanged: {
        if (presentationSuspended) {
            launchTimer.stop()
            clearActiveItems()
        } else if (enabled) {
            launchTimer.restart()
        }
    }

    Component.onDestruction: clearActiveItems()
}
