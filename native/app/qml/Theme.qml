pragma Singleton
import QtQuick

QtObject {
    id: theme
    property bool reducedMotion: false
    readonly property int motionQuick: reducedMotion ? 0 : 120
    readonly property int motionNormal: reducedMotion ? 0 : 150
    readonly property int motionPopup: reducedMotion ? 0 : 180

    property string mode: "night"
    readonly property bool isDay: mode === "day"
    readonly property color appBar: isDay ? "#fbfcfa" : "#161c1b"
    readonly property color managementSurface: appBar
    readonly property color canvas: isDay ? "#f0f3ed" : "#0e1312"
    readonly property color controlSurface: isDay ? "#e8ede4" : "#1e2624"
    readonly property color well: canvas
    readonly property color border: isDay ? "#bcc4b6" : "#323d3b"
    readonly property color borderStrong: isDay ? "#778574" : "#627772"
    readonly property color text: isDay ? "#1b281b" : "#e9f0ee"
    readonly property color mutedText: isDay ? "#465446" : "#adbbb5"
    readonly property color disabledText: isDay ? "#525f52" : "#9aa8a3"
    readonly property color accent: isDay ? "#295a2c" : "#59dbb9"
    readonly property color accentHover: isDay ? Qt.lighter(accent, 1.12) : Qt.lighter(accent, 1.06)
    readonly property color accentPressed: Qt.darker(accent, 1.12)
    readonly property color accentText: isDay ? "#fbfcfa" : "#071713"
    readonly property color selectedSurface: isDay ? "#d4e4cd" : "#1a342c"
    readonly property color controlHover: isDay ? "#dde4d6" : "#25312e"
    readonly property color controlPressed: managementSurface
    readonly property color controlDisabled: controlSurface
    readonly property color dangerSurface: isDay ? "#f6dfdc" : "#402b29"
    readonly property color online: isDay ? "#1a582d" : "#76d9a0"
    readonly property color warning: isDay ? "#714e14" : "#eac578"
    readonly property color danger: isDay ? "#9e2c2c" : "#fe9b93"
    readonly property color videoBackground: "#020403"
    readonly property color videoSurface: "#0a1411"
    readonly property color videoText: "#edf3f0"
    readonly property color videoMutedText: "#bcc7c1"
    readonly property color videoFocus: "#85e9b6"

    readonly property int topBarHeight: 36
    readonly property int navigationRailWidth: 80
    readonly property int headerControlHeight: 28
    readonly property int navigationIconSize: 26
    readonly property int brandIconSize: 40
    readonly property int controlHeight: 32
    readonly property int buttonMinWidth: 100
    readonly property int fieldMinWidth: 200
    readonly property int comboMinWidth: 140
    readonly property int indicatorSize: 18
    readonly property int iconSize: 16
    readonly property int roomRowHeight: 60
    readonly property int borderWidth: 1
    readonly property int focusBorderWidth: 2
    readonly property int radiusIndicator: 3
    readonly property int radiusSmall: 3
    readonly property int radiusMedium: 3
    readonly property int radiusLarge: 3
    readonly property int spaceXXS: 4
    readonly property int spaceXS: 8
    readonly property int spaceSmall: 8
    readonly property int spaceMedium: 12
    readonly property int spaceLarge: 16
    readonly property int gap: spaceSmall
    readonly property int fontCaption: 11
    readonly property int fontControl: 12
    readonly property int fontBody: 14
    readonly property int fontHeading: 16
    readonly property int fontPageTitle: 24
    readonly property int toolTipDelay: 500

    readonly property Palette palette: Palette {
        window: theme.managementSurface
        windowText: theme.text
        base: theme.well
        alternateBase: theme.controlSurface
        text: theme.text
        button: theme.controlSurface
        buttonText: theme.text
        brightText: theme.text
        highlight: theme.accent
        highlightedText: theme.accentText
        placeholderText: theme.mutedText
        light: theme.controlHover
        midlight: theme.controlSurface
        mid: theme.border
        dark: theme.mutedText
        shadow: theme.canvas
        toolTipBase: theme.controlSurface
        toolTipText: theme.text
        link: theme.accent
        linkVisited: theme.mutedText
        disabled {
            windowText: theme.disabledText
            text: theme.disabledText
            button: theme.controlDisabled
            buttonText: theme.disabledText
            brightText: theme.disabledText
            highlightedText: theme.disabledText
            highlight: theme.borderStrong
            placeholderText: theme.disabledText
        }
    }

    readonly property Palette primaryButtonPalette: Palette {
        buttonText: theme.accentText
        brightText: theme.accentText
        disabled {
            buttonText: theme.disabledText
            brightText: theme.disabledText
        }
    }

    readonly property Palette disabledPalette: Palette {
        windowText: theme.disabledText
        text: theme.disabledText
        buttonText: theme.disabledText
        brightText: theme.disabledText
        highlightedText: theme.disabledText
        placeholderText: theme.disabledText
        button: theme.controlDisabled
        base: theme.controlDisabled
        highlight: theme.borderStrong
        dark: theme.disabledText
    }
}
