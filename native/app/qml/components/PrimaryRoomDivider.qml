import QtQuick
import QtQuick.Controls

FocusScope {
    id: root

    property string orientation: "vertical"
    property real value: 0.6
    property var controller: null
    property bool dragging: false
    readonly property var ratios: [0.5, 0.6, 0.67]

    function snap(ratio) {
        var closest = ratios[0]
        for (var i = 1; i < ratios.length; ++i) {
            if (Math.abs(ratios[i] - ratio) < Math.abs(closest - ratio)) closest = ratios[i]
        }
        return closest
    }

    function updateFromPoint(x, y) {
        if (!parent || parent.width <= 0 || parent.height <= 0) return
        var ratio = orientation === "vertical" ? (root.x + x) / parent.width : (root.y + y) / parent.height
        ratio = Math.max(0.42, Math.min(0.7, ratio))
        value = ratio
    }

    function commit() {
        var ratio = snap(value)
        value = ratio
        if (controller) controller.setPrimaryRoomRatio(ratio)
    }

    Rectangle {
        anchors.centerIn: parent
        width: root.orientation === "vertical" ? 4 : parent.width
        height: root.orientation === "vertical" ? parent.height : 4
        radius: 2
        color: root.dragging ? "#ff9a42" : "#7d5336"
        opacity: 0.9
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: root.orientation === "vertical" ? Qt.SizeHorCursor : Qt.SizeVerCursor
        onPressed: {
            root.forceActiveFocus()
            root.dragging = true
            root.updateFromPoint(mouse.x, mouse.y)
        }
        onPositionChanged: if (root.dragging) root.updateFromPoint(mouse.x, mouse.y)
        onReleased: {
            root.dragging = false
            root.commit()
        }
        onCanceled: root.dragging = false
    }

    Keys.onPressed: function(event) {
        var smaller = event.key === Qt.Key_Left || event.key === Qt.Key_Up
        var larger = event.key === Qt.Key_Right || event.key === Qt.Key_Down
        if (smaller || larger) {
            var index = ratios.indexOf(snap(value))
            index = Math.max(0, Math.min(ratios.length - 1, index + (larger ? 1 : -1)))
            value = ratios[index]
            commit()
            event.accepted = true
        } else if (event.key === Qt.Key_Home || event.key === Qt.Key_End) {
            value = event.key === Qt.Key_Home ? ratios[0] : ratios[ratios.length - 1]
            commit()
            event.accepted = true
        }
    }

    ToolTip.visible: activeFocus
    ToolTip.text: "调整主画面大小"
}
