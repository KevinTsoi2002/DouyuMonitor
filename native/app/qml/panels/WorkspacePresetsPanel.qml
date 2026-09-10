import QtQuick
import QtQuick.Controls
import ".."

Popup {
    id: root

    objectName: "workspacePresetsPanel"
    property var controller: null
    property var workspaceModel: null
    width: 360
    height: 380
    x: Math.max(12, (parent ? parent.width : width) - width - 18)
    y: 52
    padding: 14
    modal: false
    focus: true

    function applyPreset(presetId) {
        if (!root.controller) return
        Qt.callLater(function() {
            if (root.controller) root.controller.applyWorkspacePreset(presetId)
        })
    }

    background: Rectangle {
        color: Theme.controlSurface
        border.color: Theme.border
        border.width: 1
        radius: Theme.radiusLarge
    }

    contentItem: Column {
        spacing: Theme.gap
        Item {
            width: parent.width
            height: Theme.controlHeight
            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: "工作区预设"
                color: Theme.text
                font.bold: true
                font.pixelSize: 14
            }
            ToolButton {
                id: closeWorkspacePresetsButton
                objectName: "closeWorkspacePresetsButton"
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.controlHeight
                height: Theme.controlHeight
                Accessible.name: "关闭工作区预设"
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
        Row {
            width: parent.width
            spacing: 6
            TextField {
                id: presetNameInput
                objectName: "presetNameInput"
                width: parent.width - savePresetButton.width - 6
                placeholderText: "预设名称"
                color: Theme.text
                placeholderTextColor: Theme.mutedText
                selectByMouse: true
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: Theme.well
                    border.color: Theme.border
                }
            }
            Button {
                id: savePresetButton
                text: "保存"
                enabled: presetNameInput.text.trim().length > 0
                         && presetNameInput.text.trim().length <= 30 && root.controller !== null
                onClicked: {
                    root.controller.saveWorkspacePreset(presetNameInput.text.trim())
                    presetNameInput.clear()
                }
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
        ListView {
            id: presetList
            objectName: "workspacePresetList"
            width: parent.width
            height: 230
            clip: true
            model: root.workspaceModel ? root.workspaceModel.presets : []
            spacing: 4
            delegate: Rectangle {
                required property var modelData
                width: presetList.width
                height: 42
                color: Theme.well
                radius: Theme.radiusSmall
                border.color: Theme.border
                Text { anchors.left: parent.left; anchors.leftMargin: 9; anchors.verticalCenter: parent.verticalCenter; width: parent.width - applyPresetButton.width - deletePresetButton.width - 28; text: modelData.name; color: Theme.text; elide: Text.ElideRight; font.pixelSize: 12 }
                Button {
                    id: applyPresetButton
                    objectName: "applyWorkspacePresetButton"
                    anchors.right: parent.right
                    anchors.rightMargin: deletePresetButton.width + 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "应用"
                    enabled: root.controller !== null
                    onClicked: root.applyPreset(modelData.id)
                    contentItem: Text { text: parent.text; color: Theme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 11 }
                    background: Rectangle { radius: Theme.radiusSmall; color: parent.down ? Theme.borderStrong : (parent.hovered ? Theme.border : Theme.well); border.color: Theme.borderStrong }
                }
                ToolButton {
                    id: deletePresetButton
                    objectName: "deleteWorkspacePresetButton"
                    anchors.right: parent.right
                    anchors.rightMargin: 5
                    anchors.verticalCenter: parent.verticalCenter
                    width: 28
                    height: 28
                    enabled: root.controller !== null
                    Accessible.name: "删除预设"
                    ToolTip.visible: hovered
                    ToolTip.text: Accessible.name
                    onClicked: root.controller.deleteWorkspacePreset(modelData.id)
                    contentItem: Image { anchors.centerIn: parent; width: 15; height: 15; source: Qt.resolvedUrl("../assets/icons/x.svg"); opacity: parent.enabled ? 0.72 : 0.35 }
                    background: Rectangle { radius: 4; color: parent.hovered && parent.enabled ? "#4b1c1c" : "transparent" }
                }
            }
        }
    }
}
