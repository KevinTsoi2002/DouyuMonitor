import QtQuick
import QtQuick.Controls.Basic
import ".."
import "../controls"

Dialog {
    id: root

    objectName: "notificationSettingsDialog"
    property var controller: null
    property var monitoringModel: null
    readonly property var preferences: controller && controller.notificationPreferences
                                       ? controller.notificationPreferences : fallbackPreferences
    modal: true
    title: "Windows 通知"
    width: Math.min(420, parent ? parent.width - 32 : 420)
    padding: 16
    height: Math.min(implicitHeight, parent ? parent.height - 32 : implicitHeight)
    anchors.centerIn: parent
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
    onOpened: enabledCheckBox.forceActiveFocus()

    background: Rectangle {
        color: Theme.managementSurface
        border.color: Theme.border
        radius: Theme.radiusSmall
    }

    function savePreferences() {
        if (!controller) return
        controller.setNotificationPreferences(enabledCheckBox.checked,
                                              roomOnlineCheckBox.checked,
                                               roomOfflineCheckBox.checked,
                                               playbackFailedCheckBox.checked,
                                               playbackRecoveredCheckBox.checked,
                                               favoriteTitleCheckBox.checked)
    }

    QtObject {
        id: fallbackPreferences
        property bool enabled: true
        property bool roomOnline: true
        property bool roomOffline: true
        property bool playbackFailed: true
        property bool playbackRecovered: true
        property bool favoriteTitleChanged: true
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
            font.pixelSize: Theme.fontHeading
        }
        ConsoleToolButton {
            objectName: "closeNotificationSettingsButton"
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.controlHeight
            height: Theme.controlHeight
            Accessible.name: "关闭通知设置"
            ToolTip.text: Accessible.name
            onClicked: root.close()
            icon.source: Qt.resolvedUrl("../assets/icons/x.svg")
        }
    }

    contentItem: Column {
        spacing: 4
        ConsoleCheckBox {
            width: parent.width
            height: 36
            id: enabledCheckBox
            objectName: "notificationEnabledCheckBox"
            text: "启用 Windows 通知"
            checked: root.preferences.enabled
            enabled: root.controller !== null
            onClicked: root.savePreferences()
            contentItem.objectName: "notificationEnabledLabel"
        }
        Rectangle { width: parent.width; height: 1; color: Theme.border }
        Text { text: "通知事件"; color: Theme.mutedText; font.pixelSize: Theme.fontCaption }
        ConsoleCheckBox {
            width: parent.width
            height: 36
            id: roomOnlineCheckBox
            objectName: "notificationRoomOnlineCheckBox"
            text: "开播"
            checked: root.preferences.roomOnline
            enabled: enabledCheckBox.checked && root.controller !== null
            onClicked: root.savePreferences()
        }
        ConsoleCheckBox {
            width: parent.width
            height: 36
            id: roomOfflineCheckBox
            objectName: "notificationRoomOfflineCheckBox"
            text: "下播"
            checked: root.preferences.roomOffline
            enabled: enabledCheckBox.checked && root.controller !== null
            onClicked: root.savePreferences()
        }
        ConsoleCheckBox {
            width: parent.width
            height: 36
            id: playbackFailedCheckBox
            objectName: "notificationPlaybackFailedCheckBox"
            text: "播放异常"
            checked: root.preferences.playbackFailed
            enabled: enabledCheckBox.checked && root.controller !== null
            onClicked: root.savePreferences()
        }
        ConsoleCheckBox {
            width: parent.width
            height: 36
            id: playbackRecoveredCheckBox
            objectName: "notificationPlaybackRecoveredCheckBox"
            text: "播放恢复"
            checked: root.preferences.playbackRecovered
            enabled: enabledCheckBox.checked && root.controller !== null
            onClicked: root.savePreferences()
        }
        ConsoleCheckBox {
            width: parent.width
            height: 36
            id: favoriteTitleCheckBox
            objectName: "notificationFavoriteTitleCheckBox"
            text: "收藏标题变化"
            checked: root.preferences.favoriteTitleChanged
            enabled: enabledCheckBox.checked && root.controller !== null
            onClicked: root.savePreferences()
        }
        Text {
            objectName: "notificationStatusDescription"
            width: parent.width
            text: {
                if (!enabledCheckBox.checked) return "Windows 通知已关闭"
                if (root.monitoringModel
                    && root.monitoringModel.notificationStatus === "unavailable") {
                    return "Windows 通知后端不可用"
                }
                return "当前会将状态变化发送到 Windows 通知中心"
            }
            color: Theme.mutedText
            font.pixelSize: Theme.fontCaption
            wrapMode: Text.Wrap
        }
    }

    footer: Rectangle {
        objectName: "notificationDialogFooter"
        implicitHeight: 56
        color: Theme.managementSurface
        border.color: Theme.border
        topLeftRadius: 0
        topRightRadius: 0
        ConsoleButton {
            objectName: "dismissNotificationSettingsButton"
            anchors.right: parent.right
            anchors.rightMargin: 0
            anchors.verticalCenter: parent.verticalCenter
            width: 92
            height: 34
            text: "关闭"
            onClicked: root.close()
        }
    }
}
