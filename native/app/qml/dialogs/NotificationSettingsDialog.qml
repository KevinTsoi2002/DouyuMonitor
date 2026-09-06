import QtQuick
import QtQuick.Controls
import ".."

Dialog {
    id: root

    objectName: "notificationSettingsDialog"
    property var controller: null
    property var monitoringModel: null
    readonly property var preferences: controller ? controller.notificationPreferences : fallbackPreferences
    modal: true
    title: "Windows 通知"
    width: 360
    padding: 16
    anchors.centerIn: parent

    background: Rectangle {
        color: Theme.controlSurface
        border.color: Theme.border
        radius: Theme.radiusLarge
    }

    function savePreferences() {
        if (!controller) return
        controller.setNotificationPreferences(enabledCheckBox.checked,
                                              roomOnlineCheckBox.checked,
                                              roomOfflineCheckBox.checked,
                                              playbackFailedCheckBox.checked,
                                              playbackRecoveredCheckBox.checked)
    }

    QtObject {
        id: fallbackPreferences
        property bool enabled: true
        property bool roomOnline: true
        property bool roomOffline: true
        property bool playbackFailed: true
        property bool playbackRecovered: true
    }

    header: Item {
        implicitHeight: 48
        Text {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.text
            font.bold: true
            font.pixelSize: 16
        }
        ToolButton {
            objectName: "closeNotificationSettingsButton"
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.controlHeight
            height: Theme.controlHeight
            Accessible.name: "关闭通知设置"
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

    contentItem: Column {
        spacing: 12
        CheckBox {
            id: enabledCheckBox
            text: "启用 Windows 通知"
            checked: root.preferences.enabled
            enabled: root.controller !== null
            onClicked: root.savePreferences()
        }
        CheckBox {
            id: roomOnlineCheckBox
            text: "开播"
            checked: root.preferences.roomOnline
            enabled: enabledCheckBox.checked && root.controller !== null
            onClicked: root.savePreferences()
        }
        CheckBox {
            id: roomOfflineCheckBox
            text: "下播"
            checked: root.preferences.roomOffline
            enabled: enabledCheckBox.checked && root.controller !== null
            onClicked: root.savePreferences()
        }
        CheckBox {
            id: playbackFailedCheckBox
            text: "播放异常"
            checked: root.preferences.playbackFailed
            enabled: enabledCheckBox.checked && root.controller !== null
            onClicked: root.savePreferences()
        }
        CheckBox {
            id: playbackRecoveredCheckBox
            text: "播放恢复"
            checked: root.preferences.playbackRecovered
            enabled: enabledCheckBox.checked && root.controller !== null
            onClicked: root.savePreferences()
        }
        Text {
            width: parent.width
            text: enabledCheckBox.checked ? "当前会将状态变化发送到 Windows 通知中心" : "Windows 通知已关闭"
            color: Theme.mutedText
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
    }

    footer: DialogButtonBox {
        Button {
            objectName: "dismissNotificationSettingsButton"
            text: "关闭"
            onClicked: root.close()
        }
    }
}
