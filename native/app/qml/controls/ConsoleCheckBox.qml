import QtQuick
import QtQuick.Controls.Basic as Basic
import ".."

Basic.CheckBox {
    id: control

    implicitHeight: Math.max(Theme.controlHeight, implicitContentHeight + topPadding + bottomPadding)
    padding: Theme.spaceXS
    spacing: Theme.spaceSmall
    font.pixelSize: Theme.fontControl
    hoverEnabled: true
    palette.buttonText: enabled ? Theme.text : Theme.disabledText
    palette.windowText: enabled ? Theme.text : Theme.disabledText
    palette.text: enabled ? Theme.text : Theme.disabledText

    indicator: Rectangle {
        implicitWidth: Theme.indicatorSize
        implicitHeight: Theme.indicatorSize
        x: control.text ? (control.mirrored ? control.width - width - control.rightPadding : control.leftPadding)
                        : control.leftPadding + (control.availableWidth - width) / 2
        y: control.topPadding + (control.availableHeight - height) / 2
        radius: Theme.radiusIndicator
        color: !control.enabled ? Theme.controlDisabled
               : control.checkState !== Qt.Unchecked ? (control.down ? Theme.accentPressed : Theme.accent)
               : control.down ? Theme.controlPressed : Theme.well
        border.width: control.activeFocus || control.visualFocus ? Theme.focusBorderWidth : Theme.borderWidth
        border.color: !control.enabled ? Theme.borderStrong
                      : control.activeFocus || control.visualFocus || control.checked ? Theme.accent
                      : control.hovered ? Theme.mutedText : Theme.borderStrong
        Text {
            anchors.centerIn: parent
            text: control.checkState === Qt.PartiallyChecked ? "\u2212" : "\u2713"
            visible: control.checkState !== Qt.Unchecked
            color: control.enabled ? Theme.accentText : Theme.disabledText
            font.pixelSize: Theme.fontBody
            font.bold: true
            Accessible.ignored: true
        }
    }
}
