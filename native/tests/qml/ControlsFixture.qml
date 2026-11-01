import QtQuick
import QtQuick.Controls.Basic
import "../../app/qml"
import "../../app/qml/controls"

ApplicationWindow {
    width: 640
    height: 480
    visible: true
    color: Theme.canvas
    palette: Theme.palette
    property color primaryText: Theme.text
    property color secondaryText: Theme.mutedText
    property color onlineColor: Theme.online
    property bool reducedMotion: false
    onReducedMotionChanged: Theme.reducedMotion = reducedMotion
    property int motionDuration: Theme.motionNormal

    Column {
        x: 24
        y: 24
        spacing: Theme.gap
        Row {
            spacing: Theme.gap
            ConsoleButton { objectName: "button"; text: "Action"; icon.source: "../../app/qml/assets/icons/plus.svg" }
            ConsoleButton { objectName: "primary"; text: "Primary"; highlighted: true; icon.source: "../../app/qml/assets/icons/plus.svg" }
            ConsoleButton { objectName: "selected"; text: "Selected"; checkable: true; checked: true }
            ConsoleButton { objectName: "disabledPrimary"; text: "Disabled"; highlighted: true; enabled: false }
        }
        Row {
            spacing: Theme.gap
            ConsoleToolButton {
                objectName: "tool"
                icon.source: "../../app/qml/assets/icons/x.svg"
                Accessible.name: "Close"
                ToolTip.text: Accessible.name
            }
            ConsoleToolButton { objectName: "selectedTool"; text: "Selected tool"; checked: true }
            ConsoleToolButton { objectName: "disabledTool"; text: "Disabled tool"; enabled: false }
        }
        ConsoleTextField { objectName: "field"; width: 240; placeholderText: "Search" }
        ConsoleTextField { width: 240; placeholderText: "Disabled input"; enabled: false }
        ConsoleComboBox {
            objectName: "combo"
            width: 240
            model: [{ label: "First", value: "first" }, { label: "Second", value: "second" }]
            textRole: "label"
            valueRole: "value"
            currentIndex: 1
        }
        Row {
            spacing: Theme.gap
            ConsoleCheckBox { objectName: "check"; text: "Check" }
            ConsoleCheckBox { objectName: "partial"; text: "Partial"; tristate: true; checkState: Qt.PartiallyChecked }
            ConsoleCheckBox { text: "Disabled"; checked: true; enabled: false }
        }
        Row {
            spacing: Theme.gap
            ConsoleRadioButton { objectName: "radioA"; text: "Option A"; checked: true }
            ConsoleRadioButton { objectName: "radioB"; text: "Option B" }
        }
    }
    ConsoleToolTip {
        objectName: "tooltip"
        x: 370
        y: 170
        text: "Control tooltip"
        visible: true
    }
}
