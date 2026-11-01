pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls.Basic as Basic
import ".."

Basic.ComboBox {
    id: control
    property bool videoSurface: false

    implicitWidth: Math.max(Theme.comboMinWidth, implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(Theme.controlHeight, implicitContentHeight + topPadding + bottomPadding)
    padding: Theme.spaceXS
    spacing: Theme.spaceXS
    font.pixelSize: Theme.fontControl
    hoverEnabled: true
    palette.buttonText: videoSurface ? (enabled ? Theme.videoText : Theme.videoMutedText) : (enabled ? Theme.text : Theme.disabledText)
    palette.text: videoSurface ? (enabled ? Theme.videoText : Theme.videoMutedText) : (enabled ? Theme.text : Theme.disabledText)
    palette.highlight: videoSurface ? Theme.videoFocus : Theme.accent
    palette.highlightedText: videoSurface ? Theme.videoBackground : Theme.accentText
    palette.base: videoSurface ? Theme.videoSurface : (enabled ? Theme.well : Theme.controlDisabled)
    palette.dark: videoSurface ? Theme.videoMutedText : (enabled ? Theme.mutedText : Theme.disabledText)
    palette.mid: videoSurface ? Qt.rgba(Theme.videoMutedText.r, Theme.videoMutedText.g, Theme.videoMutedText.b, 0.36) : Theme.border

    delegate: Basic.ItemDelegate {
        id: option
        required property int index
        width: control.popup.availableWidth
        height: Theme.controlHeight
        leftPadding: Theme.spaceMedium
        rightPadding: Theme.spaceMedium
        text: control.textAt(index)
        highlighted: control.highlightedIndex === index
        hoverEnabled: control.hoverEnabled
        font: control.font
        palette.text: control.videoSurface ? Theme.videoText : Theme.text
        palette.highlightedText: control.videoSurface ? Theme.videoText : Theme.text
        background: Rectangle {
            radius: Theme.radiusSmall
            color: control.videoSurface
                   ? Qt.tint(Theme.videoSurface, Qt.rgba(Theme.videoFocus.r, Theme.videoFocus.g, Theme.videoFocus.b,
                                                        option.highlighted ? 0.16 : option.hovered || option.down ? 0.10 : 0))
                   : option.highlighted ? Theme.selectedSurface
                   : option.hovered || option.down ? Theme.controlHover : Theme.well
            border.width: option.visualFocus ? Theme.borderWidth : 0
            border.color: control.videoSurface ? Theme.videoFocus : Theme.accent
        }
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.videoSurface
               ? Qt.tint(Theme.videoSurface, Qt.rgba(Theme.videoText.r, Theme.videoText.g, Theme.videoText.b,
                                                    !control.enabled ? 0 : control.down ? 0.18 : control.hovered ? 0.10 : 0))
               : !control.enabled ? Theme.controlDisabled
               : control.down ? Theme.controlPressed
               : control.hovered ? Theme.controlHover : Theme.controlSurface
        border.width: control.activeFocus || control.visualFocus ? Theme.focusBorderWidth : Theme.borderWidth
        border.color: control.videoSurface
                      ? (control.enabled && (control.activeFocus || control.visualFocus || control.popup.visible)
                         ? Theme.videoFocus : Qt.rgba(Theme.videoMutedText.r, Theme.videoMutedText.g, Theme.videoMutedText.b, 0.36))
                      : !control.enabled ? Theme.border
                      : control.activeFocus || control.visualFocus || control.popup.visible ? Theme.accent
                      : control.hovered ? Theme.accent : Theme.borderStrong
        visible: !control.flat || control.down || control.activeFocus || control.visualFocus
        Behavior on color { ColorAnimation { duration: Theme.motionQuick } }
        Behavior on border.color { ColorAnimation { duration: Theme.motionQuick } }
    }
    popup.enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.motionPopup; easing.type: Easing.OutCubic }
    }
    popup.background: Rectangle {
        radius: Theme.radiusSmall
        color: control.videoSurface ? Theme.videoSurface : Theme.well
        border.color: control.videoSurface ? Qt.rgba(Theme.videoMutedText.r, Theme.videoMutedText.g, Theme.videoMutedText.b, 0.36) : Theme.borderStrong
        border.width: Theme.borderWidth
    }
}
