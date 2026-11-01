import QtQuick
import QtQuick.Controls.Basic
import ".."
import "../controls"

Dialog {
    id: root
    objectName: "closeBehaviorDialog"
    property var controller: null
    modal: true
    title: "关闭窗口"
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
    onOpened: cancelCloseButton.forceActiveFocus()

    header: Item {
        implicitHeight: 56
        Text {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.text
            font.bold: true
            font.pixelSize: Theme.fontHeading
        }
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
    }

    background: Rectangle {
        color: Theme.managementSurface
        border.color: Theme.border
        radius: Theme.radiusSmall
    }

    contentItem: Column {
        spacing: Theme.spaceMedium
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: "进入后台将继续托管工作区；完全退出会结束播放。"
            color: Theme.text
            font.pixelSize: Theme.fontBody
        }
        ConsoleCheckBox {
            id: rememberChoice
            objectName: "rememberChoice"
            width: parent.width
            height: 28
            text: "记住我的选择"
            leftPadding: 0
            rightPadding: 0
            indicator.objectName: "rememberChoiceIndicator"
            contentItem: Text {
                objectName: "rememberChoiceLabel"
                anchors.left: parent.left
                anchors.leftMargin: 28
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: rememberChoice.text
                color: rememberChoice.enabled ? Theme.text : Theme.disabledText
                font: rememberChoice.font
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    footer: DialogButtonBox {
        standardButtons: DialogButtonBox.NoButton
        alignment: Qt.AlignRight
        spacing: Theme.spaceSmall
        padding: 12

        ConsoleButton {
            id: cancelCloseButton
            objectName: "cancelCloseButton"
            text: "取消"
            onClicked: root.close()
        }
        ConsoleButton {
            objectName: "enterBackgroundButton"
            text: "进入后台"
            onClicked: {
                if (root.controller) {
                    root.controller.setCloseBehavior("background", rememberChoice.checked)
                    root.controller.closeToTray()
                    if (!rememberChoice.checked) root.controller.setCloseBehavior("ask", false)
                }
                root.close()
            }
        }
        ConsoleButton {
            objectName: "quitApplicationButton"
            text: "完全退出"
            highlighted: true
            onClicked: {
                if (root.controller) {
                    root.controller.setCloseBehavior("quit", rememberChoice.checked)
                    root.controller.requestQuit()
                    if (!rememberChoice.checked) root.controller.setCloseBehavior("ask", false)
                }
                root.close()
            }
        }
    }
}
