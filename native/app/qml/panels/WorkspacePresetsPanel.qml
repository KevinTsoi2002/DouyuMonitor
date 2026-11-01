import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import ".."
import "../controls"

Popup {
    id: root

    objectName: "workspacePresetsPanel"
    property var controller: null
    property var workspaceModel: null
    width: Math.min(380, parent ? parent.width - 24 : 380)
    height: Math.min(440, parent ? parent.height - y - 12 : 440)
    x: Math.max(12, (parent ? parent.width : width) - width - 12)
    y: Math.min(48, Math.max(12, (parent ? parent.height : 600) - 360))
    padding: Theme.spaceMedium
    modal: false
    focus: true
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
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    function applyPreset(presetId) {
        if (!root.controller) return
        Qt.callLater(function() {
            if (root.controller) root.controller.applyWorkspacePreset(presetId)
        })
    }

    background: Rectangle {
        color: Theme.managementSurface
        border.color: Theme.border
        border.width: Theme.borderWidth
        radius: Theme.radiusSmall
    }

    contentItem: ColumnLayout {
        spacing: Theme.gap
        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.controlHeight
            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: "工作区预设"
                color: Theme.text
                font.bold: true
                font.pixelSize: Theme.fontHeading
            }
            ConsoleToolButton {
                id: closeWorkspacePresetsButton
                objectName: "closeWorkspacePresetsButton"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.controlHeight
                height: Theme.controlHeight
                Accessible.name: "关闭工作区预设"
                ToolTip.text: Accessible.name
                onClicked: root.close()
                icon.source: Qt.resolvedUrl("../assets/icons/x.svg")
            }
        }
        Text {
            Layout.fillWidth: true
            text: "保存当前房间、布局与音频，随时恢复观看组合。"
            color: Theme.mutedText
            font.pixelSize: Theme.fontControl
            wrapMode: Text.Wrap
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Theme.spaceSmall
            ConsoleTextField {
                id: presetNameInput
                objectName: "presetNameInput"
                Layout.fillWidth: true
                placeholderText: "预设名称"
                selectByMouse: true
            }
            ConsoleButton {
                id: savePresetButton
                objectName: "saveWorkspacePresetButton"
                text: "保存"
                highlighted: true
                enabled: presetNameInput.text.trim().length > 0
                         && presetNameInput.text.trim().length <= 30 && root.controller !== null
                onClicked: {
                    const presetId = root.controller.saveWorkspacePreset(presetNameInput.text.trim())
                    if (presetId) presetNameInput.clear()
                }
            }
        }
        ListView {
            id: presetList
            objectName: "workspacePresetList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.workspaceModel ? root.workspaceModel.presets : []
            spacing: 0
            delegate: Rectangle {
                required property var modelData
                width: presetList.width
                height: 56
                color: Theme.canvas
                radius: Theme.radiusSmall
                border.color: Theme.border
                border.width: Theme.borderWidth
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.spaceSmall
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - applyPresetButton.width - deletePresetButton.width - Theme.spaceLarge
                    text: modelData.name
                    color: Theme.text
                    elide: Text.ElideRight
                    font.pixelSize: Theme.fontBody
                }
                ConsoleButton {
                    id: applyPresetButton
                    objectName: "applyWorkspacePresetButton"
                    anchors.right: parent.right
                    anchors.rightMargin: deletePresetButton.width + Theme.spaceSmall
                    anchors.verticalCenter: parent.verticalCenter
                    text: "应用"
                    implicitWidth: 56
                    height: Theme.controlHeight
                    enabled: root.controller !== null
                    onClicked: root.applyPreset(modelData.id)
                    font.pixelSize: Theme.fontCaption
                }
                ConsoleToolButton {
                    id: deletePresetButton
                    objectName: "deleteWorkspacePresetButton"
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.spaceXS
                    anchors.verticalCenter: parent.verticalCenter
                    width: Theme.controlHeight
                    height: Theme.controlHeight
                    enabled: root.controller !== null
                    Accessible.name: "删除预设"
                    destructive: true
                    ToolTip.text: Accessible.name
                    onClicked: root.controller.deleteWorkspacePreset(modelData.id)
                    icon.source: Qt.resolvedUrl("../assets/icons/x.svg")
                }
            }
            Text {
                anchors.centerIn: parent
                width: parent.width - 24
                visible: presetList.count === 0
                text: "还没有工作区预设\n输入名称，保存当前观看组合"
                horizontalAlignment: Text.AlignHCenter
                color: Theme.mutedText
                font.pixelSize: Theme.fontControl
                lineHeight: 1.5
                wrapMode: Text.Wrap
            }
        }
    }
}
