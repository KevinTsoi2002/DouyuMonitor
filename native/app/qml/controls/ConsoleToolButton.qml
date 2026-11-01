import QtQuick
import QtQuick.Controls.Basic as Basic
import ".."

Basic.ToolButton {
    id: control
    property bool destructive: false
    property bool videoSurface: false
    readonly property color contentColor: videoSurface
                                         ? (enabled ? Theme.videoText : Theme.videoMutedText)
                                         : (enabled ? Theme.text : Theme.disabledText)

    implicitWidth: Math.max(Theme.controlHeight, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(Theme.controlHeight, implicitContentHeight + topPadding + bottomPadding)
    padding: Theme.spaceXS
    spacing: Theme.spaceXS
    font.pixelSize: Theme.fontControl
    hoverEnabled: true
    flat: true
    palette.buttonText: contentColor
    palette.brightText: contentColor
    palette.windowText: contentColor
    palette.text: contentColor
    palette.highlight: videoSurface ? Theme.videoFocus : Theme.accent
    icon.width: Theme.iconSize
    icon.height: Theme.iconSize
    icon.color: contentColor

    background: ControlBackground {
        targetControl: control
        selected: control.checked
        flat: control.flat
        destructive: control.destructive
        videoSurface: control.videoSurface
    }

    ConsoleToolTip {
        objectName: "consoleToolTip"
        parent: control
        visible: control.enabled && control.visualFocus && text.length > 0
        text: control.Basic.ToolTip.text.length > 0 ? control.Basic.ToolTip.text : control.Accessible.name
        delay: 0
    }
}
