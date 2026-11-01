import QtQuick
import ".."

Rectangle {
    required property var targetControl
    property bool primary: false
    property bool selected: false
    property bool flat: false
    property bool destructive: false
    property bool videoSurface: false
    readonly property bool focusVisible: targetControl.activeFocus || targetControl.visualFocus

    radius: Theme.radiusSmall
    color: videoSurface ? (!targetControl.enabled ? (flat ? "transparent" : Theme.videoSurface)
                          : targetControl.down ? Qt.tint(Theme.videoSurface, Qt.rgba(Theme.videoText.r, Theme.videoText.g, Theme.videoText.b, 0.18))
                          : selected ? Qt.tint(Theme.videoSurface, Qt.rgba(Theme.videoFocus.r, Theme.videoFocus.g, Theme.videoFocus.b, 0.16))
                          : targetControl.hovered ? Qt.tint(Theme.videoSurface, Qt.rgba(Theme.videoText.r, Theme.videoText.g, Theme.videoText.b, 0.10))
                          : flat ? "transparent" : Theme.videoSurface)
           : !targetControl.enabled ? (flat ? "transparent" : Theme.controlDisabled)
           : destructive && (targetControl.down || targetControl.hovered) ? Theme.dangerSurface
           : primary ? (targetControl.down ? Theme.accentPressed : targetControl.hovered ? Theme.accentHover : Theme.accent)
           : targetControl.down ? Theme.controlPressed
           : selected ? Theme.selectedSurface
           : targetControl.hovered ? Theme.controlHover
           : flat ? "transparent" : Theme.controlSurface
    border.width: focusVisible ? Theme.focusBorderWidth : Theme.borderWidth
    border.color: videoSurface ? (!targetControl.enabled ? "transparent"
                                 : focusVisible || selected ? Theme.videoFocus
                                 : flat ? "transparent"
                                 : Qt.rgba(Theme.videoMutedText.r, Theme.videoMutedText.g, Theme.videoMutedText.b, 0.36))
                  : !targetControl.enabled ? (flat ? "transparent" : Theme.border)
                  : focusVisible || selected ? Theme.accent
                  : destructive && (targetControl.down || targetControl.hovered) ? Theme.danger
                  : primary ? color
                  : flat ? "transparent"
                  : targetControl.hovered ? Theme.accent : Theme.borderStrong
    Behavior on color { ColorAnimation { duration: Theme.motionQuick } }
    Behavior on border.color { ColorAnimation { duration: Theme.motionQuick } }
}
