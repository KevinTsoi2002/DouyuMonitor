import QtQuick
import QtQuick.Controls.Basic as Basic
import ".."

Basic.TextField {
    id: control

    implicitWidth: Theme.fieldMinWidth
    implicitHeight: Math.max(Theme.controlHeight, contentHeight + topPadding + bottomPadding)
    leftPadding: Theme.spaceMedium
    rightPadding: Theme.spaceMedium
    topPadding: Theme.spaceXS
    bottomPadding: Theme.spaceXS
    font.pixelSize: Theme.fontControl
    hoverEnabled: true
    palette.text: enabled ? Theme.text : Theme.disabledText
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.accentText
    palette.placeholderText: enabled ? Theme.mutedText : Theme.disabledText
    color: enabled ? Theme.text : Theme.disabledText
    placeholderTextColor: enabled ? Theme.mutedText : Theme.disabledText

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.enabled ? Theme.well : Theme.controlDisabled
        border.width: control.activeFocus ? Theme.focusBorderWidth : Theme.borderWidth
        border.color: !control.enabled ? Theme.border
                      : control.activeFocus ? Theme.accent
                      : control.hovered ? Theme.accent : Theme.borderStrong
    }
}
