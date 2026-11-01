import QtQuick
import QtQuick.Controls.Basic
import ".."
import "../controls"

Popup {
    id: root

    objectName: "danmakuSettingsPanel"
    property var controller: null
    property var roomModel: null
    readonly property var danmakuController: controller && controller.danmaku ? controller.danmaku : null
    property string currentTab: "display"
    property string governanceScope: "global"
    property int statsRevision: 0
    property var fallbackDisplaySettings: ({
        "durationSeconds": 8, "fontSize": 24, "opacity": 0.9,
        "region": "full", "density": "normal", "fontFamily": "microsoft-yahei",
        "rendering": "native"
    })
    property var fallbackGovernanceSettings: ({
        "enabled": true, "keywordBlacklist": [], "duplicateWindowSeconds": 3,
        "peakProtectionEnabled": true
    })
    property var emptyStats: ({
        "level": "normal", "recentRate": 0, "peakRate": 0, "filtered": 0,
        "duplicates": 0, "rateLimited": 0, "queueOverflow": 0, "upstreamDropped": 0
    })
    readonly property var displaySettings: danmakuController
                                         ? danmakuController.displaySettings : fallbackDisplaySettings
    readonly property var governanceSettings: {
        if (!danmakuController) return fallbackGovernanceSettings
        // Keep the global settings dependency while resolving the selected room.
        const globalSettings = danmakuController.governanceSettings
        const roomId = governanceScope === "room" && governanceRoomPicker
                ? governanceRoomPicker.currentValue : ""
        return danmakuController.governanceSettingsForRoom(roomId || "")
    }
    width: Math.min(400, parent ? parent.width - 24 : 400)
    height: Math.min(600, (parent ? parent.height : 640) - y - 12)
    x: Math.max(12, (parent ? parent.width : width) - width - 12)
    y: 48
    padding: Theme.spaceMedium
    modal: false
    focus: true
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
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function targetRoomId() {
        return governanceScope === "room" ? governanceRoomPicker.currentValue : ""
    }

    function applyGovernanceSetting(key, value) {
        if (!danmakuController) return
        const roomId = targetRoomId()
        if (governanceScope === "room" && !roomId) return
        danmakuController.setGovernanceSetting(roomId, key, value)
    }

    function keywordList(text) {
        const values = text.split(",")
        const result = []
        for (let index = 0; index < values.length; ++index) {
            const keyword = values[index].trim()
            if (keyword.length > 0) result.push(keyword)
        }
        return result
    }

    function statsForSelectedRoom() {
        if (!danmakuController || !statsRoomPicker.currentValue) return emptyStats
        return danmakuController.statsForRoom(statsRoomPicker.currentValue)
    }

    function resetDisplaySettings() {
        if (!danmakuController) return
        danmakuController.setDisplaySetting("durationSeconds", 8)
        danmakuController.setDisplaySetting("fontSize", 24)
        danmakuController.setDisplaySetting("opacity", 0.9)
        danmakuController.setDisplaySetting("region", "full")
        danmakuController.setDisplaySetting("density", "normal")
        danmakuController.setDisplaySetting("fontFamily", "microsoft-yahei")
        danmakuController.setDisplaySetting("rendering", "native")
    }

    Connections {
        target: root.danmakuController

        function onStatisticsChanged(roomId) {
            if (roomId === statsRoomPicker.currentValue) ++root.statsRevision
        }
    }

    component TabButton: ConsoleToolButton {
        required property string tabId
        text: tabId === "display" ? "显示" : tabId === "governance" ? "治理" : "统计"
        width: (parent.width - parent.spacing * 2) / 3
        height: parent.height
        onClicked: root.currentTab = tabId
        checked: root.currentTab === tabId
        font.bold: checked
        font.pixelSize: Theme.fontCaption
    }

    component SegmentButton: ConsoleToolButton {
        required property string valueToken
        required property string settingKey
        required property string selectedToken
        width: (parent.width - parent.spacing * (parent.children.length - 1)) / parent.children.length
        height: 30
        enabled: root.danmakuController !== null
        onClicked: root.danmakuController.setDisplaySetting(settingKey, valueToken)
        checked: selectedToken === valueToken
        flat: false
        font.pixelSize: Theme.fontCaption
    }

    background: Rectangle {
        color: Theme.managementSurface
        border.color: Theme.border
        border.width: Theme.borderWidth
        radius: Theme.radiusSmall
    }

    contentItem: Column {
        width: root.width - root.leftPadding - root.rightPadding
        height: root.height - root.topPadding - root.bottomPadding
        spacing: Theme.gap

        Row {
            width: parent.width
            height: 32
            spacing: Theme.spaceSmall
            Column {
                width: parent.width - globalEnabledSwitch.width - closeDanmakuSettingsButton.width - parent.spacing * 2
                spacing: Theme.spaceXXS
                Text { text: "弹幕设置"; color: Theme.text; font.bold: true; font.pixelSize: Theme.fontHeading }
                Text { text: "显示、治理和统计"; color: Theme.mutedText; font.pixelSize: Theme.fontCaption }
            }
            Switch {
                id: globalEnabledSwitch
                objectName: "danmakuGlobalToggle"
                anchors.verticalCenter: parent.verticalCenter
                checked: root.danmakuController ? root.danmakuController.globalEnabled : false
                enabled: root.danmakuController !== null
                onToggled: if (root.danmakuController) root.danmakuController.setGlobalEnabled(checked)
            }
            ConsoleToolButton {
                id: closeDanmakuSettingsButton
                objectName: "closeDanmakuSettingsButton"
                width: Theme.controlHeight
                height: Theme.controlHeight
                Accessible.name: "关闭弹幕设置"
                ToolTip.text: Accessible.name
                onClicked: root.close()
                icon.source: Qt.resolvedUrl("../assets/icons/x.svg")
            }
        }

        Rectangle {
            width: parent.width
            height: 38
            color: Theme.well
            radius: Theme.radiusSmall
            border.color: Theme.border
            Row {
                anchors.fill: parent
                anchors.margins: 3
                spacing: 3
                TabButton { objectName: "danmakuDisplayTab"; tabId: "display" }
                TabButton { objectName: "danmakuGovernanceTab"; tabId: "governance" }
                TabButton { objectName: "danmakuStatsTab"; tabId: "stats" }
            }
        }

        Flickable {
            id: contentFlickable
            width: parent.width
            height: Math.max(0, parent.height - 32 - 38 - 136 - parent.spacing * 3)
            contentWidth: width
            contentHeight: settingsStack.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Item {
                id: settingsStack
                width: contentFlickable.width
                height: Math.max(displaySection.height, governanceSection.height, statsSection.height)

                Column {
                    id: displaySection
                    objectName: "danmakuDisplaySection"
                    width: parent.width
                    visible: root.currentTab === "display"
                    height: visible ? implicitHeight : 0
                    spacing: Theme.gap

                    Text { text: "显示方式"; color: Theme.text; font.bold: true; font.pixelSize: Theme.fontControl }

                    Column {
                        width: parent.width
                        spacing: Theme.spaceXS
                        Row {
                            width: parent.width
                            Text { text: "滚动时长"; color: Theme.text; font.pixelSize: Theme.fontCaption }
                            Item { width: parent.width - durationLabel.width - 64 }
                            Text { id: durationLabel; text: Math.round(15 - durationSlider.value * 11 / 100) + " 秒"; color: Theme.mutedText; font.pixelSize: Theme.fontCaption }
                        }
                        Slider {
                            id: durationSlider
                            width: parent.width
                            from: 0; to: 100; stepSize: 1
                            value: (15 - Number(root.displaySettings.durationSeconds)) * 100 / 11
                            enabled: root.danmakuController !== null
                            onMoved: root.danmakuController.setDisplaySetting("durationSeconds", Math.round(15 - value * 11 / 100))
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: Theme.spaceXS
                        Row {
                            width: parent.width
                            Text { text: "字体大小"; color: Theme.text; font.pixelSize: Theme.fontCaption }
                            Item { width: parent.width - fontSizeLabel.width - 64 }
                            Text { id: fontSizeLabel; text: Math.round(fontSizeSlider.value) + " px"; color: Theme.mutedText; font.pixelSize: Theme.fontCaption }
                        }
                        Slider {
                            id: fontSizeSlider
                            width: parent.width
                            from: 14; to: 36; stepSize: 1
                            value: Number(root.displaySettings.fontSize)
                            enabled: root.danmakuController !== null
                            onMoved: root.danmakuController.setDisplaySetting("fontSize", Math.round(value))
                        }
                    }

                    Column {
                        width: parent.width
                        spacing: Theme.spaceXS
                        Row {
                            width: parent.width
                            Text { text: "不透明度"; color: Theme.text; font.pixelSize: Theme.fontCaption }
                            Item { width: parent.width - opacityLabel.width - 64 }
                            Text { id: opacityLabel; text: Math.round(opacitySlider.value * 100) + "%"; color: Theme.mutedText; font.pixelSize: Theme.fontCaption }
                        }
                        Slider {
                            id: opacitySlider
                            width: parent.width
                            from: 0.3; to: 1.0; stepSize: 0.05
                            value: Number(root.displaySettings.opacity)
                            enabled: root.danmakuController !== null
                            onMoved: root.danmakuController.setDisplaySetting("opacity", value)
                        }
                    }

                    Text { text: "显示区域"; color: Theme.text; font.pixelSize: Theme.fontCaption }
                    Row {
                        width: parent.width; spacing: Theme.spaceXS
                        SegmentButton { text: "全部"; valueToken: "full"; settingKey: "region"; selectedToken: root.displaySettings.region }
                        SegmentButton { text: "顶部"; valueToken: "top"; settingKey: "region"; selectedToken: root.displaySettings.region }
                        SegmentButton { text: "底部"; valueToken: "bottom"; settingKey: "region"; selectedToken: root.displaySettings.region }
                    }

                    Text { text: "密度"; color: Theme.text; font.pixelSize: Theme.fontCaption }
                    Row {
                        width: parent.width; spacing: Theme.spaceXS
                        SegmentButton { text: "密集"; valueToken: "massive"; settingKey: "density"; selectedToken: root.displaySettings.density }
                        SegmentButton { text: "正常"; valueToken: "normal"; settingKey: "density"; selectedToken: root.displaySettings.density }
                        SegmentButton { text: "精简"; valueToken: "reduced"; settingKey: "density"; selectedToken: root.displaySettings.density }
                    }

                    Text { text: "字体"; color: Theme.text; font.pixelSize: Theme.fontCaption }
                    ConsoleComboBox {
                        width: parent.width
                        model: [ { "label": "微软雅黑", "value": "microsoft-yahei" }, { "label": "黑体", "value": "simhei" } ]
                        textRole: "label"; valueRole: "value"
                        currentIndex: root.displaySettings.fontFamily === "simhei" ? 1 : 0
                        enabled: root.danmakuController !== null
                        onActivated: root.danmakuController.setDisplaySetting("fontFamily", currentValue)
                    }

                    Text { text: "渲染"; color: Theme.text; font.pixelSize: Theme.fontCaption }
                    Row {
                        width: parent.width; spacing: Theme.spaceXS
                        SegmentButton { text: "原生"; valueToken: "native"; settingKey: "rendering"; selectedToken: root.displaySettings.rendering }
                        SegmentButton { text: "增强"; valueToken: "advanced"; settingKey: "rendering"; selectedToken: root.displaySettings.rendering }
                    }
                    ConsoleToolButton {
                        objectName: "resetDanmakuDisplaySettings"
                        width: parent.width; height: 30
                        enabled: root.danmakuController !== null
                        text: "恢复显示默认值"
                        Accessible.name: text
                        onClicked: root.resetDisplaySettings()
                        flat: false
                        font.pixelSize: Theme.fontCaption
                    }
                    Item { width: 1; height: 1 }
                }

                Column {
                    id: governanceSection
                    objectName: "danmakuGovernanceSection"
                    width: parent.width
                    visible: root.currentTab === "governance"
                    height: visible ? implicitHeight : 0
                    spacing: Theme.gap

                    Text { text: "治理规则"; color: Theme.text; font.bold: true; font.pixelSize: Theme.fontControl }
                    ConsoleComboBox {
                        id: governanceScopePicker
                        objectName: "danmakuGovernanceScope"
                        width: parent.width
                        model: [ { "label": "全局规则", "value": "global" }, { "label": "房间覆盖", "value": "room" } ]
                        textRole: "label"; valueRole: "value"
                        currentIndex: root.governanceScope === "room" ? 1 : 0
                        enabled: root.danmakuController !== null
                        onActivated: root.governanceScope = currentValue
                    }
                    ConsoleComboBox {
                        id: governanceRoomPicker
                        objectName: "danmakuGovernanceRoom"
                        width: parent.width
                        visible: root.governanceScope === "room"
                        height: visible ? implicitHeight : 0
                        model: root.roomModel ? root.roomModel : []
                        textRole: "anchorName"; valueRole: "roomId"
                        enabled: root.danmakuController !== null && count > 0
                    }
                    Switch {
                        width: parent.width
                        text: "启用过滤与限流"
                        checked: Boolean(root.governanceSettings.enabled)
                        enabled: root.danmakuController !== null && (root.governanceScope === "global" || governanceRoomPicker.currentValue)
                        onToggled: root.applyGovernanceSetting("enabled", checked)
                    }
                    Column {
                        width: parent.width; spacing: Theme.spaceXS
                        Text { text: "关键词黑名单（逗号分隔）"; color: Theme.text; font.pixelSize: Theme.fontCaption }
                        ConsoleTextField {
                            id: keywordField
                            width: parent.width
                            text: root.governanceSettings.keywordBlacklist.join(", ")
                            placeholderText: "输入需要过滤的关键词"
                            enabled: root.danmakuController !== null && (root.governanceScope === "global" || governanceRoomPicker.currentValue)
                            selectByMouse: true
                            onEditingFinished: root.applyGovernanceSetting("keywordBlacklist", root.keywordList(text))
                        }
                    }
                    Column {
                        width: parent.width; spacing: Theme.spaceXS
                        Row {
                            width: parent.width
                            Text { text: "重复消息窗口"; color: Theme.text; font.pixelSize: Theme.fontCaption }
                            Item { width: parent.width - duplicateLabel.width - 88 }
                            Text { id: duplicateLabel; text: Math.round(duplicateWindowSlider.value) + " 秒"; color: Theme.mutedText; font.pixelSize: Theme.fontCaption }
                        }
                        Slider {
                            id: duplicateWindowSlider
                            width: parent.width
                            from: 1; to: 10; stepSize: 1
                            value: Number(root.governanceSettings.duplicateWindowSeconds)
                            enabled: root.danmakuController !== null && (root.governanceScope === "global" || governanceRoomPicker.currentValue)
                            onMoved: root.applyGovernanceSetting("duplicateWindowSeconds", Math.round(value))
                        }
                    }
                    Switch {
                        width: parent.width
                        text: "峰值保护"
                        checked: Boolean(root.governanceSettings.peakProtectionEnabled)
                        enabled: root.danmakuController !== null && (root.governanceScope === "global" || governanceRoomPicker.currentValue)
                        onToggled: root.applyGovernanceSetting("peakProtectionEnabled", checked)
                    }
                    ConsoleToolButton {
                        objectName: "clearDanmakuRoomGovernanceOverride"
                        width: parent.width; height: 30
                        visible: root.governanceScope === "room"
                         enabled: root.danmakuController !== null && Boolean(governanceRoomPicker.currentValue)
                        text: "清除当前房间覆盖"
                        onClicked: root.danmakuController.clearRoomGovernanceOverride(governanceRoomPicker.currentValue)
                        flat: false
                        font.pixelSize: Theme.fontCaption
                    }
                    Item { width: 1; height: 1 }
                }

                Column {
                    id: statsSection
                    objectName: "danmakuStatsSection"
                    width: parent.width
                    visible: root.currentTab === "stats"
                    height: visible ? implicitHeight : 0
                    spacing: Theme.gap
                    readonly property var stats: {
                        const revision = root.statsRevision
                        return root.statsForSelectedRoom()
                    }

                    Text { text: "房间统计"; color: Theme.text; font.bold: true; font.pixelSize: Theme.fontControl }
                    ConsoleComboBox {
                        id: statsRoomPicker
                        objectName: "danmakuStatsRoom"
                        width: parent.width
                        model: root.roomModel ? root.roomModel : []
                        textRole: "anchorName"; valueRole: "roomId"
                        enabled: root.danmakuController !== null && count > 0
                    }
                    Grid {
                        width: parent.width
                        columns: 2
                        columnSpacing: 6; rowSpacing: 6
                        Repeater {
                            model: [
                                { "label": "等级", "value": statsSection.stats.level },
                                { "label": "实时速率", "value": statsSection.stats.recentRate },
                                { "label": "峰值速率", "value": statsSection.stats.peakRate },
                                { "label": "已过滤", "value": statsSection.stats.filtered },
                                { "label": "重复拦截", "value": statsSection.stats.duplicates },
                                { "label": "限流", "value": statsSection.stats.rateLimited },
                                { "label": "队列溢出", "value": statsSection.stats.queueOverflow },
                                { "label": "上游丢弃", "value": statsSection.stats.upstreamDropped }
                            ]
                            delegate: Rectangle {
                                required property var modelData
                                width: (statsSection.width - 6) / 2
                                height: 64
                                radius: Theme.radiusIndicator
                                color: Theme.well
                                border.color: Theme.border
                                border.width: Theme.borderWidth
                                Column {
                                    anchors.fill: parent
                                    anchors.margins: Theme.spaceSmall
                                    spacing: Theme.spaceXXS
                                    Text { text: modelData.label; color: Theme.mutedText; font.pixelSize: Theme.fontCaption }
                                    Text { text: modelData.value; color: Theme.text; font.pixelSize: Theme.fontControl; font.bold: true }
                                }
                            }
                        }
                    }
                    ConsoleToolButton {
                        objectName: "clearDanmakuStats"
                        width: parent.width; height: 30
                         enabled: root.danmakuController !== null && Boolean(statsRoomPicker.currentValue)
                        text: "清除当前房间统计"
                        onClicked: root.danmakuController.clearStats(statsRoomPicker.currentValue)
                        flat: false
                        font.pixelSize: Theme.fontCaption
                    }
                    Item { width: 1; height: 1 }
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 136
            color: Theme.well
            border.color: Theme.border
            border.width: Theme.borderWidth
            radius: Theme.radiusIndicator
            Column {
                anchors.fill: parent
                anchors.margins: Theme.spaceSmall
                spacing: Theme.spaceXS
                Text { text: "房间开关"; color: Theme.text; font.pixelSize: Theme.fontCaption }
                ListView {
                    id: roomList
                    width: parent.width
                    height: 96
                    clip: true
                    model: root.roomModel ? root.roomModel : []
                    spacing: Theme.spaceXXS
                    delegate: Row {
                        required property string roomId
                        required property string anchorName
                        required property bool danmakuEnabled
                        width: roomList.width
                        height: 32
                        spacing: Theme.spaceSmall
                        Text {
                            width: parent.width - danmakuCheck.width - parent.spacing
                            anchors.verticalCenter: parent.verticalCenter
                            text: anchorName
                            color: Theme.text
                            elide: Text.ElideRight
                            font.pixelSize: Theme.fontCaption
                        }
                        ConsoleCheckBox {
                            id: danmakuCheck
                            anchors.verticalCenter: parent.verticalCenter
                            width: 28
                            height: 32
                            padding: Theme.spaceXXS
                            checked: danmakuEnabled
                            enabled: root.controller !== null
                            onClicked: root.controller.toggleDanmaku(roomId)
                        }
                    }
                }
            }
        }
    }
}
