import QtQuick
import QtQuick.Controls.Basic
import ".."
import "../controls"

ConsoleToolButton {
    id: root
    objectName: "immersiveEdgeButton"
    signal toolsRequested()
    width: 148
    height: 32
    Accessible.name: qsTr("观看工具，快捷键 T")
    ToolTip.text: Accessible.name
    onClicked: root.toolsRequested()
    text: qsTr("观看工具  T")
    contentItem: Text {
        text: root.text
        visible: root.hovered || root.activeFocus
        color: Theme.videoText
        font.pixelSize: Theme.fontControl
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
    palette.buttonText: Theme.videoText
    background: Rectangle {
        radius: Theme.radiusSmall
        color: root.hovered || root.activeFocus ? Theme.videoSurface : "transparent"
        border.width: root.activeFocus ? Theme.focusBorderWidth : 0
        border.color: Theme.videoFocus
        Rectangle { anchors.top: parent.top; anchors.horizontalCenter: parent.horizontalCenter; width: 44; height: 2; color: Theme.videoFocus }
    }
}
