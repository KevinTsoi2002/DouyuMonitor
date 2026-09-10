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
    palette.window: Theme.controlSurface
    palette.base: Theme.well
    palette.button: Theme.controlSurface
    palette.buttonText: Theme.text
    palette.text: Theme.text
    palette.windowText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.text

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
            objectName: "notificationEnabledCheckBox"
            text: "启用 Windows 通知"
            checked: root.preferences.enabled
            enabled: root.controller !== null
            onClicked: root.savePreferences()
            contentItem: Text {
                objectName: "notificationEnabledLabel"
                text: parent.text
                color: Theme.text
                verticalAlignment: Text.AlignVCenter
                leftPadding: 14
                font.pixelSize: 12
            }
            indicator: Rectangle {
                implicitWidth: 18
                implicitHeight: 18
                x: 0
                y: (parent.height - height) / 2
                radius: 4
                color: parent.checked ? Theme.accent : Theme.well
                border.color: parent.checked ? Theme.accent : Theme.borderStrong
                border.width: 1
                Rectangle {
                    visible: parent.parent.checked
                    anchors.centerIn: parent
                    width: 8
                    height: 4
                    color: "transparent"
                    border.color: Theme.text
                    border.width: 2
                    rotation: -45
                }
            }
        }
        CheckBox {
            id: roomOnlineCheckBox
            objectName: "notificationRoomOnlineCheckBox"
            text: "开播"
            checked: root.preferences.roomOnline
            enabled: enabledCheckBox.checked && root.controller !== null
            onClicked: root.savePreferences()
            contentItem: Text { text: parent.text; color: Theme.text; verticalAlignment: Text.AlignVCenter; leftPadding: 14; font.pixelSize: 12 }
            indicator: Rectangle {
                implicitWidth: 18; implicitHeight: 18; x: 0; y: (parent.height - height) / 2
                radius: 4; color: parent.checked ? Theme.accent : Theme.well
                border.color: parent.checked ? Theme.accent : Theme.borderStrong
                Rectangle { visible: parent.parent.checked; anchors.centerIn: parent; width: 8; height: 4; color: "transparent"; border.color: Theme.text; border.width: 2; rotation: -45 }
            }
        }
        CheckBox {
            id: roomOfflineCheckBox
            objectName: "notificationRoomOfflineCheckBox"
            text: "下播"
            checked: root.preferences.roomOffline
            enabled: enabledCheckBox.checked && root.controller !== null
            onClicked: root.savePreferences()
            contentItem: Text { text: parent.text; color: Theme.text; verticalAlignment: Text.AlignVCenter; leftPadding: 14; font.pixelSize: 12 }
            indicator: Rectangle {
                implicitWidth: 18; implicitHeight: 18; x: 0; y: (parent.height - height) / 2
                radius: 4; color: parent.checked ? Theme.accent : Theme.well
                border.color: parent.checked ? Theme.accent : Theme.borderStrong
                Rectangle { visible: parent.parent.checked; anchors.centerIn: parent; width: 8; height: 4; color: "transparent"; border.color: Theme.text; border.width: 2; rotation: -45 }
            }
        }
        CheckBox {
            id: playbackFailedCheckBox
            objectName: "notificationPlaybackFailedCheckBox"
            text: "播放异常"
            checked: root.preferences.playbackFailed
            enabled: enabledCheckBox.checked && root.controller !== null
            onClicked: root.savePreferences()
            contentItem: Text { text: parent.text; color: Theme.text; verticalAlignment: Text.AlignVCenter; leftPadding: 14; font.pixelSize: 12 }
            indicator: Rectangle {
                implicitWidth: 18; implicitHeight: 18; x: 0; y: (parent.height - height) / 2
                radius: 4; color: parent.checked ? Theme.accent : Theme.well
                border.color: parent.checked ? Theme.accent : Theme.borderStrong
                Rectangle { visible: parent.parent.checked; anchors.centerIn: parent; width: 8; height: 4; color: "transparent"; border.color: Theme.text; border.width: 2; rotation: -45 }
            }
        }
        CheckBox {
            id: playbackRecoveredCheckBox
            objectName: "notificationPlaybackRecoveredCheckBox"
            text: "播放恢复"
            checked: root.preferences.playbackRecovered
            enabled: enabledCheckBox.checked && root.controller !== null
            onClicked: root.savePreferences()
            contentItem: Text { text: parent.text; color: Theme.text; verticalAlignment: Text.AlignVCenter; leftPadding: 14; font.pixelSize: 12 }
            indicator: Rectangle {
                implicitWidth: 18; implicitHeight: 18; x: 0; y: (parent.height - height) / 2
                radius: 4; color: parent.checked ? Theme.accent : Theme.well
                border.color: parent.checked ? Theme.accent : Theme.borderStrong
                Rectangle { visible: parent.parent.checked; anchors.centerIn: parent; width: 8; height: 4; color: "transparent"; border.color: Theme.text; border.width: 2; rotation: -45 }
            }
        }
        CheckBox {
            id: favoriteTitleCheckBox
            objectName: "notificationFavoriteTitleCheckBox"
            text: "收藏标题变化"
            checked: root.preferences.favoriteTitleChanged
            enabled: enabledCheckBox.checked && root.controller !== null
            onClicked: root.savePreferences()
            contentItem: Text { text: parent.text; color: Theme.text; verticalAlignment: Text.AlignVCenter; leftPadding: 14; font.pixelSize: 12 }
            indicator: Rectangle {
                implicitWidth: 18; implicitHeight: 18; x: 0; y: (parent.height - height) / 2
                radius: 4; color: parent.checked ? Theme.accent : Theme.well
                border.color: parent.checked ? Theme.accent : Theme.borderStrong
                Rectangle { visible: parent.parent.checked; anchors.centerIn: parent; width: 8; height: 4; color: "transparent"; border.color: Theme.text; border.width: 2; rotation: -45 }
            }
        }
        Text {
            width: parent.width
            text: enabledCheckBox.checked ? "当前会将状态变化发送到 Windows 通知中心" : "Windows 通知已关闭"
            color: Theme.mutedText
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
    }

    footer: Rectangle {
        objectName: "notificationDialogFooter"
        implicitHeight: 56
        color: Theme.controlSurface
        border.color: Theme.border
        topLeftRadius: 0
        topRightRadius: 0
        Button {
            objectName: "dismissNotificationSettingsButton"
            anchors.right: parent.right
            anchors.rightMargin: 0
            anchors.verticalCenter: parent.verticalCenter
            width: 92
            height: 34
            text: "关闭"
            onClicked: root.close()
            contentItem: Text { text: parent.text; color: Theme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 12 }
            background: Rectangle { radius: Theme.radiusSmall; color: parent.down ? Theme.borderStrong : (parent.hovered ? Theme.border : Theme.well); border.color: Theme.borderStrong }
        }
    }
}
