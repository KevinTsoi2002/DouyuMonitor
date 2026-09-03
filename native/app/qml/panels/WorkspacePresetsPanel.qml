import QtQuick
import QtQuick.Controls

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
        color: "#1f242c"
        border.color: "#343b45"
        border.width: 1
        radius: 6
    }

    contentItem: Column {
        spacing: 10
        Text { text: "工作区预设"; color: "#f4f6f8"; font.bold: true; font.pixelSize: 14 }
        Row {
            width: parent.width
            spacing: 6
            TextField {
                id: presetNameInput
                width: parent.width - savePresetButton.width - 6
                placeholderText: "预设名称"
                selectByMouse: true
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
                color: "#171d25"
                radius: 4
                Text { anchors.left: parent.left; anchors.leftMargin: 9; anchors.verticalCenter: parent.verticalCenter; width: parent.width - applyPresetButton.width - deletePresetButton.width - 28; text: modelData.name; color: "#f4f6f8"; elide: Text.ElideRight; font.pixelSize: 12 }
                Button {
                    id: applyPresetButton
                    objectName: "applyWorkspacePresetButton"
                    anchors.right: parent.right
                    anchors.rightMargin: deletePresetButton.width + 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: "应用"
                    enabled: root.controller !== null
                    onClicked: root.applyPreset(modelData.id)
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
