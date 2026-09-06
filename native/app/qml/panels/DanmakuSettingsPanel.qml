import QtQuick
import QtQuick.Controls
import ".."

Popup {
    id: root

    objectName: "danmakuSettingsPanel"
    property var controller: null
    property var roomModel: null
    readonly property var danmakuController: controller ? controller.danmaku : null
    property string currentTab: "display"
    property string governanceScope: "global"
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
    readonly property var governanceSettings: danmakuController
                                            ? danmakuController.governanceSettings
                                            : fallbackGovernanceSettings
    width: 380
    height: Math.min(560, Math.max(420, (parent ? parent.height : 600) - y - 18))
    x: Math.max(12, (parent ? parent.width : width) - width - 72)
    y: 52
    padding: 14
    modal: false
    focus: true

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

    component TabButton: ToolButton {
        required property string tabId
        text: tabId === "display" ? "显示" : tabId === "governance" ? "治理" : "统计"
        width: (parent.width - parent.spacing * 2) / 3
        height: parent.height
        onClicked: root.currentTab = tabId
        contentItem: Text {
            text: parent.text
            color: root.currentTab === parent.tabId ? "#f4f6f8" : "#9ba5b1"
            font.bold: root.currentTab === parent.tabId
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 11
        }
        background: Rectangle {
            radius: 3
            color: root.currentTab === parent.tabId ? "#3a3025" : parent.hovered ? "#252c34" : "transparent"
        }
    }

    component SegmentButton: ToolButton {
        required property string valueToken
        required property string settingKey
        required property string selectedToken
        width: (parent.width - parent.spacing * (parent.children.length - 1)) / parent.children.length
        height: 30
        enabled: root.danmakuController !== null
        onClicked: root.danmakuController.setDisplaySetting(settingKey, valueToken)
        contentItem: Text {
            text: parent.text
            color: parent.selectedToken === parent.valueToken ? "#f4f6f8" : "#b8c0ca"
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            font.pixelSize: 10
        }
        background: Rectangle {
            radius: 4
            color: parent.selectedToken === parent.valueToken ? "#3a3025" : "#171d25"
            border.color: parent.selectedToken === parent.valueToken ? "#9b572f" : "#343b45"
        }
    }

    background: Rectangle {
        color: Theme.controlSurface
        border.color: Theme.border
        border.width: 1
        radius: Theme.radiusLarge
    }

    contentItem: Column {
        width: root.width - root.leftPadding - root.rightPadding
        height: root.height - root.topPadding - root.bottomPadding
        spacing: 10

        Row {
            width: parent.width
            height: 32
            spacing: 8
            Column {
                width: parent.width - globalEnabledSwitch.width - closeDanmakuSettingsButton.width - parent.spacing * 2
                spacing: 2
                Text { text: "弹幕设置"; color: Theme.text; font.bold: true; font.pixelSize: 14 }
                Text { text: "显示、治理和统计"; color: Theme.mutedText; font.pixelSize: 11 }
            }
            Switch {
                id: globalEnabledSwitch
                objectName: "danmakuGlobalToggle"
                anchors.verticalCenter: parent.verticalCenter
                checked: root.danmakuController ? root.danmakuController.globalEnabled : false
                enabled: root.danmakuController !== null
                onToggled: if (root.danmakuController) root.danmakuController.setGlobalEnabled(checked)
            }
            ToolButton {
                id: closeDanmakuSettingsButton
                objectName: "closeDanmakuSettingsButton"
                width: Theme.controlHeight
                height: Theme.controlHeight
                Accessible.name: "关闭弹幕设置"
                ToolTip.visible: hovered
                ToolTip.text: Accessible.name
                onClicked: root.close()
                contentItem: Image {
                    anchors.centerIn: parent
                    width: 14
                    height: 14
                    source: Qt.resolvedUrl("../assets/icons/x.svg")
                    opacity: parent.hovered ? 1 : 0.78
                }
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: parent.down ? Theme.well : (parent.hovered ? Theme.well : "transparent")
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 34
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
            height: Math.max(120, parent.height - 32 - 34 - 112 - parent.spacing * 3)
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
                    spacing: 10

                    Text { text: "显示方式"; color: "#f4f6f8"; font.bold: true; font.pixelSize: 12 }

                    Column {
                        width: parent.width
                        spacing: 4
                        Row {
                            width: parent.width
                            Text { text: "滚动时长"; color: "#d6dde5"; font.pixelSize: 11 }
                            Item { width: parent.width - durationLabel.width - 64 }
                            Text { id: durationLabel; text: Math.round(15 - durationSlider.value * 11 / 100) + " 秒"; color: "#9ba5b1"; font.pixelSize: 11 }
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
                        spacing: 4
                        Row {
                            width: parent.width
                            Text { text: "字体大小"; color: "#d6dde5"; font.pixelSize: 11 }
                            Item { width: parent.width - fontSizeLabel.width - 64 }
                            Text { id: fontSizeLabel; text: Math.round(fontSizeSlider.value) + " px"; color: "#9ba5b1"; font.pixelSize: 11 }
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
                        spacing: 4
                        Row {
                            width: parent.width
                            Text { text: "不透明度"; color: "#d6dde5"; font.pixelSize: 11 }
                            Item { width: parent.width - opacityLabel.width - 64 }
                            Text { id: opacityLabel; text: Math.round(opacitySlider.value * 100) + "%"; color: "#9ba5b1"; font.pixelSize: 11 }
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

                    Text { text: "显示区域"; color: "#d6dde5"; font.pixelSize: 11 }
                    Row {
                        width: parent.width; spacing: 4
                        SegmentButton { text: "全部"; valueToken: "full"; settingKey: "region"; selectedToken: root.displaySettings.region }
                        SegmentButton { text: "顶部"; valueToken: "top"; settingKey: "region"; selectedToken: root.displaySettings.region }
                        SegmentButton { text: "底部"; valueToken: "bottom"; settingKey: "region"; selectedToken: root.displaySettings.region }
                    }

                    Text { text: "密度"; color: "#d6dde5"; font.pixelSize: 11 }
                    Row {
                        width: parent.width; spacing: 4
                        SegmentButton { text: "密集"; valueToken: "massive"; settingKey: "density"; selectedToken: root.displaySettings.density }
                        SegmentButton { text: "正常"; valueToken: "normal"; settingKey: "density"; selectedToken: root.displaySettings.density }
                        SegmentButton { text: "精简"; valueToken: "reduced"; settingKey: "density"; selectedToken: root.displaySettings.density }
                    }

                    Text { text: "字体"; color: "#d6dde5"; font.pixelSize: 11 }
                    ComboBox {
                        width: parent.width
                        model: [ { "label": "微软雅黑", "value": "microsoft-yahei" }, { "label": "黑体", "value": "simhei" } ]
                        textRole: "label"; valueRole: "value"
                        currentIndex: root.displaySettings.fontFamily === "simhei" ? 1 : 0
                        enabled: root.danmakuController !== null
                        onActivated: root.danmakuController.setDisplaySetting("fontFamily", currentValue)
                    }

                    Text { text: "渲染"; color: "#d6dde5"; font.pixelSize: 11 }
                    Row {
                        width: parent.width; spacing: 4
                        SegmentButton { text: "原生"; valueToken: "native"; settingKey: "rendering"; selectedToken: root.displaySettings.rendering }
                        SegmentButton { text: "增强"; valueToken: "advanced"; settingKey: "rendering"; selectedToken: root.displaySettings.rendering }
                    }
                    ToolButton {
                        objectName: "resetDanmakuDisplaySettings"
                        width: parent.width; height: 30
                        enabled: root.danmakuController !== null
                        text: "恢复显示默认值"
                        Accessible.name: text
                        onClicked: root.resetDisplaySettings()
                        contentItem: Text { text: parent.text; color: parent.enabled ? "#d6dde5" : "#657181"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 10 }
                        background: Rectangle { radius: 4; color: parent.hovered ? "#2a211c" : "#171d25"; border.color: "#343b45" }
                    }
                    Item { width: 1; height: 1 }
                }

                Column {
                    id: governanceSection
                    objectName: "danmakuGovernanceSection"
                    width: parent.width
                    visible: root.currentTab === "governance"
                    height: visible ? implicitHeight : 0
                    spacing: 10

                    Text { text: "治理规则"; color: "#f4f6f8"; font.bold: true; font.pixelSize: 12 }
                    ComboBox {
                        id: governanceScopePicker
                        objectName: "danmakuGovernanceScope"
                        width: parent.width
                        model: [ { "label": "全局规则", "value": "global" }, { "label": "房间覆盖", "value": "room" } ]
                        textRole: "label"; valueRole: "value"
                        currentIndex: root.governanceScope === "room" ? 1 : 0
                        enabled: root.danmakuController !== null
                        onActivated: root.governanceScope = currentValue
                    }
                    ComboBox {
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
                        width: parent.width; spacing: 4
                        Text { text: "关键词黑名单（逗号分隔）"; color: "#d6dde5"; font.pixelSize: 11 }
                        TextField {
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
                        width: parent.width; spacing: 4
                        Row {
                            width: parent.width
                            Text { text: "重复消息窗口"; color: "#d6dde5"; font.pixelSize: 11 }
                            Item { width: parent.width - duplicateLabel.width - 88 }
                            Text { id: duplicateLabel; text: Math.round(duplicateWindowSlider.value) + " 秒"; color: "#9ba5b1"; font.pixelSize: 11 }
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
                    ToolButton {
                        objectName: "clearDanmakuRoomGovernanceOverride"
                        width: parent.width; height: 30
                        visible: root.governanceScope === "room"
                         enabled: root.danmakuController !== null && Boolean(governanceRoomPicker.currentValue)
                        text: "清除当前房间覆盖"
                        onClicked: root.danmakuController.clearRoomGovernanceOverride(governanceRoomPicker.currentValue)
                        contentItem: Text { text: parent.text; color: parent.enabled ? "#d6dde5" : "#657181"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 10 }
                        background: Rectangle { radius: 4; color: parent.hovered ? "#2a211c" : "#171d25"; border.color: "#343b45" }
                    }
                    Item { width: 1; height: 1 }
                }

                Column {
                    id: statsSection
                    objectName: "danmakuStatsSection"
                    width: parent.width
                    visible: root.currentTab === "stats"
                    height: visible ? implicitHeight : 0
                    spacing: 10
                    readonly property var stats: root.statsForSelectedRoom()

                    Text { text: "房间统计"; color: "#f4f6f8"; font.bold: true; font.pixelSize: 12 }
                    ComboBox {
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
                                height: 42
                                radius: 4
                                color: "#171d25"
                                border.color: "#343b45"
                                Column {
                                    anchors.fill: parent
                                    anchors.margins: 7
                                    spacing: 2
                                    Text { text: modelData.label; color: "#9ba5b1"; font.pixelSize: 9 }
                                    Text { text: modelData.value; color: "#f4f6f8"; font.pixelSize: 12; font.bold: true }
                                }
                            }
                        }
                    }
                    ToolButton {
                        objectName: "clearDanmakuStats"
                        width: parent.width; height: 30
                         enabled: root.danmakuController !== null && Boolean(statsRoomPicker.currentValue)
                        text: "清除当前房间统计"
                        onClicked: root.danmakuController.clearStats(statsRoomPicker.currentValue)
                        contentItem: Text { text: parent.text; color: parent.enabled ? "#d6dde5" : "#657181"; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 10 }
                        background: Rectangle { radius: 4; color: parent.hovered ? "#2a211c" : "#171d25"; border.color: "#343b45" }
                    }
                    Item { width: 1; height: 1 }
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 112
            color: "#171d25"
            border.color: "#343b45"
            radius: 4
            Column {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 4
                Text { text: "房间开关"; color: "#d6dde5"; font.pixelSize: 11 }
                ListView {
                    id: roomList
                    width: parent.width
                    height: 72
                    clip: true
                    model: root.roomModel ? root.roomModel : []
                    spacing: 3
                    delegate: Row {
                        required property string roomId
                        required property string anchorName
                        required property bool danmakuEnabled
                        width: roomList.width
                        height: 21
                        spacing: 6
                        Text {
                            width: parent.width - danmakuCheck.width - parent.spacing
                            anchors.verticalCenter: parent.verticalCenter
                            text: anchorName
                            color: "#f4f6f8"
                            elide: Text.ElideRight
                            font.pixelSize: 10
                        }
                        CheckBox {
                            id: danmakuCheck
                            anchors.verticalCenter: parent.verticalCenter
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
