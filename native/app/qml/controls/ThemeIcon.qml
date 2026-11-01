pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import ".."

Image {
    id: root
    property color tint: Theme.text
    sourceSize: Qt.size(width, height)
    fillMode: Image.PreserveAspectFit
    Accessible.ignored: true
    layer.enabled: true
    layer.effect: MultiEffect {
        colorization: 1
        colorizationColor: root.tint
    }
}
