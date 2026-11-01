import QtQuick
import QtQuick.Controls.Basic
import ".."
import "../controls"

Drawer {
    id: root

    objectName: "monitoringDrawer"
    property var controller: null
    property var monitoringModel: null
    signal notificationSettingsRequested()
    edge: Qt.RightEdge
    width: Math.min(360, parent ? parent.width - 24 : 360)
    height: parent ? parent.height : 640
    modal: false
    interactive: true
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
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: Theme.managementSurface
        border.color: Theme.border
        border.width: Theme.borderWidth
    }

    contentItem: Column {
        anchors.fill: parent
        anchors.margins: Theme.spaceLarge
        spacing: Theme.gap
        Item {
            width: parent.width
            height: Theme.controlHeight
            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: "监控状态"
                color: Theme.text
                font.bold: true
                font.pixelSize: Theme.fontHeading
            }
            ConsoleToolButton {
                id: closeMonitoringStatusButton
                objectName: "closeMonitoringStatusButton"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.controlHeight
                height: Theme.controlHeight
                Accessible.name: "关闭监控状态"
                ToolTip.text: Accessible.name
                onClicked: root.close()
                icon.source: Qt.resolvedUrl("../assets/icons/x.svg")
            }
        }
        Text {
            width: parent.width
            text: "当前工作区的房间状态"
            color: Theme.mutedText
            font.pixelSize: Theme.fontControl
            wrapMode: Text.Wrap
        }
        Repeater {
            model: [
                { label: "直播中", value: root.monitoringModel ? root.monitoringModel.onlineCount : 0, color: Theme.online },
                { label: "未开播", value: root.monitoringModel ? root.monitoringModel.offlineCount : 0, color: Theme.mutedText },
                { label: "异常", value: root.monitoringModel ? root.monitoringModel.errorCount : 0, color: Theme.danger }
            ]
            delegate: Rectangle {
                required property var modelData
                width: parent.width
                height: 56
                color: Theme.canvas
                radius: Theme.radiusSmall
                border.color: Theme.border
                border.width: Theme.borderWidth
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.spaceSmall
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.label
                    color: Theme.mutedText
                    font.pixelSize: Theme.fontCaption
                }
                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.spaceSmall
                    anchors.verticalCenter: parent.verticalCenter
                    text: modelData.value
                    color: modelData.color
                    font.bold: true
                    font.pixelSize: Theme.fontHeading
                }
            }
        }
        Text {
            width: parent.width
            text: "状态未知的房间不计入在线或离线；异常可能与在线状态重叠。"
            color: Theme.mutedText
            font.pixelSize: Theme.fontCaption
            wrapMode: Text.Wrap
        }
        Rectangle { width: parent.width; height: Theme.borderWidth; color: Theme.border }
        Text {
            objectName: "notificationStatusLabel"
            width: parent.width
            wrapMode: Text.Wrap
            text: {
                if (!root.monitoringModel) return "Windows 通知"
                if (root.monitoringModel.notificationStatus === "enabled") return "Windows 通知已启用"
                if (root.monitoringModel.notificationStatus === "unavailable") return "Windows 通知后端不可用"
                return "Windows 通知已关闭"
            }
            color: Theme.mutedText
            font.pixelSize: Theme.fontCaption
        }
        ConsoleButton {
            objectName: "openNotificationSettingsButton"
            width: parent.width
            height: Theme.controlHeight
            text: "通知设置"
            onClicked: root.notificationSettingsRequested()
        }
    }
}
