import QtQuick
import QtQuick.Controls.Basic as Basic
import ".."

Basic.RadioButton {
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
        radius: width / 2
        color: control.enabled ? Theme.well : Theme.controlDisabled
        border.width: control.activeFocus || control.visualFocus ? Theme.focusBorderWidth : Theme.borderWidth
        border.color: !control.enabled ? Theme.borderStrong
                      : control.activeFocus || control.visualFocus || control.checked ? Theme.accent
                      : control.hovered ? Theme.mutedText : Theme.borderStrong
        Rectangle {
            anchors.centerIn: parent
            width: Theme.indicatorSize - Theme.spaceSmall
            height: width
            radius: width / 2
            color: control.enabled ? (control.down ? Theme.accentPressed : Theme.accent) : Theme.disabledText
            visible: control.checked
        }
    }
    background: Rectangle {
        radius: Theme.radiusSmall
        color: !control.enabled ? "transparent"
               : control.down ? Theme.controlPressed
               : control.hovered ? Theme.controlHover : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.motionQuick } }
    }
}
