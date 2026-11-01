import QtQuick
import QtQuick.Controls.Basic as Basic
import ".."

Basic.Button {
    id: control
    property bool destructive: false

    implicitWidth: Math.max(Theme.buttonMinWidth, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(Theme.controlHeight, implicitContentHeight + topPadding + bottomPadding)
    leftPadding: Theme.spaceMedium
    rightPadding: Theme.spaceMedium
    topPadding: Theme.spaceXS
    bottomPadding: Theme.spaceXS
    spacing: Theme.spaceXS
    font.pixelSize: Theme.fontControl
    hoverEnabled: true
    palette.buttonText: !enabled ? Theme.disabledText : highlighted ? Theme.accentText : Theme.text
    palette.brightText: !enabled ? Theme.disabledText : highlighted ? Theme.accentText : Theme.text
    palette.windowText: enabled ? Theme.text : Theme.disabledText
    palette.text: enabled ? Theme.text : Theme.disabledText
    palette.highlight: Theme.accent
    icon.width: Theme.iconSize
    icon.height: Theme.iconSize
    icon.color: !enabled ? Theme.disabledText : highlighted ? Theme.accentText : Theme.text

    background: ControlBackground {
        targetControl: control
        primary: control.highlighted
        selected: control.checked
        flat: control.flat
        destructive: control.destructive
    }
}
