import QtQuick
import QtQuick.Controls
import ".."

Rectangle {
    id: root

    property var controller: null
    property var workspaceModel: null

    color: Theme.managementSurface
    border.color: Theme.border
    border.width: 1
    clip: true

    Column {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        Text {
            color: Theme.text
            font.bold: true
            font.pixelSize: 15
            text: "仓鼠特工"
        }
    }
}