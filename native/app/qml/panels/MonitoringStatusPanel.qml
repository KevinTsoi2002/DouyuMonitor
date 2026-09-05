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
        spacing: 12
        Text { text: "监控状态"; color: "#f4f6f8"; font.bold: true; font.pixelSize: 16 }
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
                color: "#171d25"
                radius: 4
                Text { anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData.label; color: "#aeb7c1"; font.pixelSize: 12 }
                Text { anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter; text: modelData.value; color: modelData.color; font.bold: true; font.pixelSize: 16 }
            }
        }
        Rectangle { width: parent.width; height: 1; color: "#343b45" }
        Text { text: root.monitoringModel ? (root.monitoringModel.notificationStatus === "enabled" ? "Windows 通知已启用" : "Windows 通知已关闭") : "Windows 通知"; color: "#9ba5b1"; font.pixelSize: 12 }
        Button {
            text: "通知设置"
            onClicked: root.notificationSettingsRequested()
        }
    }
}
