import QtQuick
import QtQuick.Controls.Basic as Basic
import ".."

Basic.ToolTip {
    id: control
    // Hover/focus owns visibility; outside presses must reach the underlying panel.
    closePolicy: Basic.Popup.CloseOnEscape
    padding: Theme.spaceSmall
    font.pixelSize: Theme.fontControl
    palette.toolTipText: Theme.text
    palette.toolTipBase: Theme.controlSurface
    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motionQuick; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.motionQuick; easing.type: Easing.OutCubic }
    }
    contentItem: Text {
        text: control.text
        font: control.font
        color: Theme.text
        wrapMode: Text.WordWrap
    }
    background: Rectangle {
        radius: Theme.radiusSmall
        color: Theme.controlSurface
        border.color: Theme.borderStrong
        border.width: Theme.borderWidth
    }
}
