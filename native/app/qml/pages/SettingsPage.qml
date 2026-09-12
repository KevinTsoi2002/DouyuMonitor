import QtQuick
import QtQuick.Controls
import ".."

Item {
    id: root

    objectName: "settingsPage"
    property var controller: null
    signal backRequested()

    readonly property string closeBehavior: controller ? controller.closeBehavior : "ask"
    function chooseCloseBehavior(behavior) {
        if (!controller) return
        if (behavior === "ask") controller.clearCloseBehavior()
        else controller.setCloseBehavior(behavior, true)
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.canvas
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: 24
        contentWidth: width
        contentHeight: settingsContent.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: settingsContent
            width: Math.min(parent.width, 780)
            spacing: 18

            Row {
                width: parent.width
                height: 36
                spacing: 10

                ToolButton {
                    id: settingsBackButton
                    objectName: "settingsBackButton"
                    width: Theme.controlHeight
                    height: Theme.controlHeight
                    Accessible.name: "返回监控工作区"
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: root.backRequested()
                    contentItem: Text {
                        anchors.centerIn: parent
                        text: "‹"
                        color: Theme.text
                        font.pixelSize: 28
                    }
                    background: Rectangle {
                        radius: Theme.radiusSmall
                        color: settingsBackButton.hovered ? Theme.controlSurface : "transparent"
                    }
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text {
                        text: "设置"
                        color: Theme.text
                        font.bold: true
                        font.pixelSize: 20
                    }
                    Text {
                        text: "窗口与应用行为"
                        color: Theme.mutedText
                        font.pixelSize: 12
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: closeBehaviorSection.height + 32
                radius: Theme.radiusMedium
                color: Theme.controlSurface
                border.color: Theme.border

                Column {
                    id: closeBehaviorSection
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 12

                    Text {
                        text: "关闭按钮行为"
                        color: Theme.text
                        font.bold: true
                        font.pixelSize: 14
                    }
                    Text {
                        text: "选择关闭窗口时的默认动作。每次询问不会保存默认选择。"
                        color: Theme.mutedText
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        width: parent.width
                    }

                    ButtonGroup { id: closeBehaviorGroup }

                    Repeater {
                        model: [
                            { value: "ask", title: "每次询问", detail: "关闭时显示操作选择" },
                            { value: "background", title: "进入后台", detail: "隐藏窗口并保留直播播放" },
                            { value: "quit", title: "完全退出", detail: "停止播放并退出应用" }
                        ]

                        delegate: RadioButton {
                            id: option
                            required property var modelData
                            objectName: "closeBehaviorOption_" + modelData.value
                            width: closeBehaviorSection.width
                            height: 48
                            text: modelData.title
                            checked: root.closeBehavior === modelData.value
                            ButtonGroup.group: closeBehaviorGroup
                            onClicked: root.chooseCloseBehavior(modelData.value)

                            indicator: Rectangle {
                                x: 0
                                anchors.verticalCenter: parent.verticalCenter
                                width: 18
                                height: 18
                                radius: 9
                                color: Theme.well
                                border.color: option.checked ? Theme.accent : Theme.borderStrong
                                border.width: option.checked ? 5 : 1
                            }
                            contentItem: Column {
                                anchors.left: parent.left
                                anchors.leftMargin: 28
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: 2
                                Text {
                                    text: option.text
                                    color: Theme.text
                                    font.pixelSize: 12
                                }
                                Text {
                                    text: option.modelData.detail
                                    color: Theme.mutedText
                                    font.pixelSize: 10
                                }
                            }
                            background: Rectangle {
                                radius: Theme.radiusSmall
                                color: option.hovered ? Theme.well : "transparent"
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: updateSection.height + 32
                radius: Theme.radiusMedium
                color: Theme.controlSurface
                border.color: Theme.border

                Column {
                    id: updateSection
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 10

                    Text {
                        text: "更新"
                        color: Theme.text
                        font.bold: true
                        font.pixelSize: 14
                    }
                    Text {
                        text: "检查应用是否有新版本。"
                        color: Theme.mutedText
                        font.pixelSize: 11
                    }
                    Column {
                        width: parent.width
                        spacing: 8

                        Row {
                            spacing: 10
                            Button {
                                objectName: "checkUpdateButton"
                                text: "检查更新"
                                enabled: !!root.controller && root.controller.updateState !== "checking"
                                onClicked: if (root.controller) root.controller.checkForUpdates()
                            }
                            Button {
                                objectName: "openReleaseButton"
                                visible: !!root.controller
                                         && root.controller.updateState === "updateAvailable"
                                         && root.controller.updateReleaseUrl.toString().length > 0
                                text: "打开发布页"
                                onClicked: root.controller.openLatestRelease()
                            }
                        }

                        Text {
                            visible: !!root.controller && root.controller.updateMessage.length > 0
                            width: parent.width
                            text: root.controller ? root.controller.updateMessage : ""
                            color: Theme.mutedText
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }
}
