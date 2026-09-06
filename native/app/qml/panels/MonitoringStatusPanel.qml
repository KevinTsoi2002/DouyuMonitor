import QtQuick
import QtQuick.Controls
import ".."

Drawer {
    id: root

    objectName: "monitoringDrawer"
    property var controller: null
    property var monitoringModel: null
    signal notificationSettingsRequested()
    edge: Qt.RightEdge
    width: 320
    height: parent ? parent.height : 640
    modal: false
    interactive: true

    background: Rectangle {
        color: Theme.controlSurface
        border.color: Theme.border
        border.width: 1
    }

    contentItem: Column {
        anchors.fill: parent
        anchors.margins: 16
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
                font.pixelSize: 16
            }
            ToolButton {
                id: closeMonitoringStatusButton
                objectName: "closeMonitoringStatusButton"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.controlHeight
                height: Theme.controlHeight
                Accessible.name: "关闭监控状态"
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
        Repeater {
            model: [
                { label: "直播中", value: root.monitoringModel ? root.monitoringModel.onlineCount : 0, color: "#65d391" },
                { label: "未开播", value: root.monitoringModel ? root.monitoringModel.offlineCount : 0, color: "#9ba5b1" },
                { label: "异常", value: root.monitoringModel ? root.monitoringModel.errorCount : 0, color: "#ff9b92" }
            ]
            delegate: Rectangle {
                required property var modelData
                width: parent.width
                height: 44
                color: Theme.well
                radius: Theme.radiusSmall
                border.color: Theme.border
                Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData.label; color: Theme.mutedText; font.pixelSize: 12 }
                Text { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData.value; color: modelData.color; font.bold: true; font.pixelSize: 16 }
            }
        }
        Rectangle { width: parent.width; height: 1; color: Theme.border }
        Text { text: root.monitoringModel ? (root.monitoringModel.notificationStatus === "enabled" ? "Windows 通知已启用" : "Windows 通知已关闭") : "Windows 通知"; color: Theme.mutedText; font.pixelSize: 12 }
        Button {
            objectName: "openNotificationSettingsButton"
            width: parent.width
            height: Theme.controlHeight
            text: "通知设置"
            onClicked: root.notificationSettingsRequested()
            contentItem: Text {
                text: parent.text
                color: "#1a1a1a"
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                font.bold: true
                font.pixelSize: 12
            }
            background: Rectangle {
                radius: Theme.radiusSmall
                color: parent.down ? "#d76018" : (parent.hovered ? "#ff8d43" : Theme.accent)
            }
        }
    }
}
