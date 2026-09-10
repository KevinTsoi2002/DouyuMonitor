import QtQuick
import QtQuick.Controls
import ".."

Dialog {
    id: root
    objectName: "closeBehaviorDialog"
    property var controller: null
    modal: true
    title: "关闭窗口"
    width: 360
    padding: 16
    anchors.centerIn: parent

    background: Rectangle {
        color: Theme.controlSurface
        border.color: Theme.border
        radius: Theme.radiusLarge
    }

    contentItem: Column {
        spacing: 14
        Text {
            text: "请选择关闭后的行为"
            color: Theme.text
            font.pixelSize: 13
        }
        CheckBox {
            id: rememberChoice
            objectName: "rememberChoice"
            width: parent.width
            height: 28
            text: "记住我的选择"
            leftPadding: 0
            rightPadding: 0
            indicator: Rectangle {
                objectName: "rememberChoiceIndicator"
                x: 0
                anchors.verticalCenter: parent.verticalCenter
                width: 18
                height: 18
                radius: 4
                color: rememberChoice.checked ? Theme.accent : Theme.well
                border.color: rememberChoice.checked ? Theme.accent : Theme.borderStrong
                border.width: 1
                Text {
                    anchors.centerIn: parent
                    visible: rememberChoice.checked
                    text: "✓"
                    color: "#ffffff"
                    font.pixelSize: 13
                    font.bold: true
                }
            }
            contentItem: Text {
                objectName: "rememberChoiceLabel"
                anchors.left: parent.left
                anchors.leftMargin: 28
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: rememberChoice.text
                color: Theme.text
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    footer: DialogButtonBox {
        standardButtons: DialogButtonBox.NoButton
        alignment: Qt.AlignRight
        spacing: 8

        Button {
            objectName: "cancelCloseButton"
            text: "取消"
            onClicked: root.close()
        }
        Button {
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
        Button {
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
