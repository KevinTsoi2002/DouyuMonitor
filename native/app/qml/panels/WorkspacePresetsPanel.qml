import QtQuick
import QtQuick.Controls

Popup {
    id: root

    objectName: "workspacePresetsPanel"
    property var controller: null
    property var workspaceModel: null
    signal groupManagementRequested()
    width: 360
    height: 380
    x: Math.max(12, (parent ? parent.width : width) - width - 18)
    y: 52
    padding: 14
    modal: false
    focus: true

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
                Text { anchors.left: parent.left; anchors.leftMargin: 9; anchors.verticalCenter: parent.verticalCenter; width: parent.width - applyPresetButton.width - 18; text: modelData.name; color: "#f4f6f8"; elide: Text.ElideRight; font.pixelSize: 12 }
                Button {
                    id: applyPresetButton
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    text: "应用"
                    enabled: root.controller !== null
                    onClicked: root.controller.applyWorkspacePreset(modelData.id)
                }
            }
        }
        Button {
            text: "管理分组"
            onClicked: root.groupManagementRequested()
        }
    }
}
