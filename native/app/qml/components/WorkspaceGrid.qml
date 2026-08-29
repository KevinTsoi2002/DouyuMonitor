import QtQuick
import QtQuick.Controls

Item {
    id: root

    property var controller: null
    property var roomModel: null
    property string layoutId: "grid-2x2"
    property string layoutMode: "auto"
    property string primaryRoomId: ""
    property real primaryRoomRatio: 0.6
    property color canvasColor: "#16191f"
    property color surfaceColor: "#1f242c"
    property color borderColor: "#343b45"
    property color accentColor: "#ff7a18"
    property color textColor: "#f4f6f8"
    property color mutedTextColor: "#9ba5b1"
    readonly property int roomCount: roomRepeater.count
    readonly property string resolvedLayoutId: resolveLayoutId()
    readonly property bool primaryFocus: resolvedLayoutId === "primary-two" && roomCount > 1
    readonly property bool focusVertical: layoutSurface.width > 0 && layoutSurface.width <= 820
    readonly property int secondaryCount: Math.max(0, roomCount - 1)
    readonly property int secondaryColumns: secondaryCount <= 3 ? 1 : 2
    readonly property int secondaryRows: Math.max(1, Math.ceil(secondaryCount / secondaryColumns))

    function recommendedLayout(count) {
        if (count <= 1) return "single"
        if (count <= 4) return "grid-2x2"
        if (count <= 6) return "grid-3x2"
        return "grid-3x3"
    }

    function resolveLayoutId() {
        var selected = layoutMode === "auto" ? recommendedLayout(roomCount) : layoutMode
        if (selected === "single" && roomCount > 1) return recommendedLayout(roomCount)
        if (selected === "grid-2x2" && roomCount > 4) return recommendedLayout(roomCount)
        if (selected === "grid-3x2" && roomCount > 6) return recommendedLayout(roomCount)
        if ((selected === "split-horizontal" || selected === "split-vertical") && roomCount > 2) return recommendedLayout(roomCount)
        return selected
    }

    function gridColumns() {
        if (resolvedLayoutId === "single") return 1
        return resolvedLayoutId === "grid-3x2" || resolvedLayoutId === "grid-3x3" ? 3 : 2
    }

    function secondaryIndex(index) {
        var result = 0
        for (var i = 0; i < index; ++i) {
            var item = roomRepeater.itemAt(i)
            if (item && item.roomId !== primaryRoomId) ++result
        }
        return result
    }

    function tileX(index, roomId) {
        var gap = 8
        var width = layoutSurface.width
        if (primaryFocus) {
            if (focusVertical) {
                if (roomId === primaryRoomId) return 0
                var verticalSecondaryWidth = Math.max(0, width - gap * (secondaryColumns - 1))
                var verticalCellWidth = Math.max(0, verticalSecondaryWidth / secondaryColumns)
                return (secondaryIndex(index) % secondaryColumns) * (verticalCellWidth + gap)
            }
            var ratioWidth = Math.max(0, width * primaryRoomRatio - gap / 2)
            if (roomId === primaryRoomId) return 0
            var horizontalSecondaryWidth = Math.max(0, width - ratioWidth - gap)
            var horizontalCellWidth = Math.max(0, (horizontalSecondaryWidth - gap * (secondaryColumns - 1)) / secondaryColumns)
            return ratioWidth + gap + (secondaryIndex(index) % secondaryColumns) * (horizontalCellWidth + gap)
        }
        var columns = gridColumns()
        var cellWidth = Math.max(0, (width - gap * (columns - 1)) / columns)
        if (resolvedLayoutId === "split-vertical") return 0
        return (index % columns) * (cellWidth + gap)
    }

    function tileY(index, roomId) {
        var gap = 8
        var height = layoutSurface.height
        if (primaryFocus) {
            if (focusVertical) {
                var ratioHeight = Math.max(0, height * primaryRoomRatio - gap / 2)
                if (roomId === primaryRoomId) return 0
                var secondaryHeight = Math.max(0, height - ratioHeight - gap)
                var cellHeight = Math.max(0, (secondaryHeight - gap * (secondaryRows - 1)) / secondaryRows)
                return ratioHeight + gap + Math.floor(secondaryIndex(index) / secondaryColumns) * (cellHeight + gap)
            }
            if (roomId === primaryRoomId) return 0
            var desktopCellHeight = Math.max(0, (height - gap * (secondaryRows - 1)) / secondaryRows)
            return Math.floor(secondaryIndex(index) / secondaryColumns) * (desktopCellHeight + gap)
        }
        var columns = gridColumns()
        var rows = Math.max(1, Math.ceil(roomCount / columns))
        var cellHeight = Math.max(0, (height - gap * (rows - 1)) / rows)
        if (resolvedLayoutId === "split-vertical") return index * (cellHeight + gap)
        return Math.floor(index / columns) * (cellHeight + gap)
    }

    function tileWidth(index, roomId) {
        var gap = 8
        if (primaryFocus) {
            if (focusVertical) {
                if (roomId === primaryRoomId) return Math.max(0, layoutSurface.width)
                var verticalSecondaryWidth = Math.max(0, layoutSurface.width - gap * (secondaryColumns - 1))
                return Math.max(0, verticalSecondaryWidth / secondaryColumns)
            }
            if (roomId === primaryRoomId) return Math.max(0, layoutSurface.width * primaryRoomRatio - gap / 2)
            var horizontalSecondaryWidth = Math.max(0, layoutSurface.width - layoutSurface.width * primaryRoomRatio - gap)
            return Math.max(0, (horizontalSecondaryWidth - gap * (secondaryColumns - 1)) / secondaryColumns)
        }
        var columns = gridColumns()
        return Math.max(0, (layoutSurface.width - gap * (columns - 1)) / columns)
    }

    function tileHeight(index, roomId) {
        var gap = 8
        if (primaryFocus) {
            if (focusVertical) {
                if (roomId === primaryRoomId) return Math.max(0, layoutSurface.height * primaryRoomRatio - gap / 2)
                var verticalSecondaryHeight = Math.max(0, layoutSurface.height - layoutSurface.height * primaryRoomRatio - gap)
                return Math.max(0, (verticalSecondaryHeight - gap * (secondaryRows - 1)) / secondaryRows)
            }
            if (roomId === primaryRoomId) return Math.max(0, layoutSurface.height)
            return Math.max(0, (layoutSurface.height - gap * (secondaryRows - 1)) / secondaryRows)
        }
        var columns = gridColumns()
        var rows = Math.max(1, Math.ceil(roomCount / columns))
        return Math.max(0, (layoutSurface.height - gap * (rows - 1)) / rows)
    }

    Rectangle { anchors.fill: parent; color: root.canvasColor }

    Item {
        id: layoutSurface
        objectName: "layoutSurface"
        anchors.fill: parent
        anchors.margins: 8
        visible: roomRepeater.count > 0

        Repeater {
            id: roomRepeater
            model: root.roomModel

            delegate: RoomTile {
                x: root.tileX(index, roomId)
                y: root.tileY(index, roomId)
                width: root.tileWidth(index, roomId)
                height: root.tileHeight(index, roomId)
                controller: root.controller
                borderColor: root.borderColor
                accentColor: root.accentColor
                textColor: root.textColor
                mutedTextColor: root.mutedTextColor
            }
        }

        PrimaryRoomDivider {
            id: primaryRoomDivider
            objectName: "primaryRoomDivider"
            visible: root.primaryFocus && root.secondaryCount > 0
            orientation: root.focusVertical ? "horizontal" : "vertical"
            value: root.primaryRoomRatio
            controller: root.controller
            x: root.focusVertical ? 0 : Math.max(0, layoutSurface.width * root.primaryRoomRatio - width / 2)
            y: root.focusVertical ? Math.max(0, layoutSurface.height * root.primaryRoomRatio - height / 2) : 0
            width: root.focusVertical ? layoutSurface.width : 12
            height: root.focusVertical ? 12 : layoutSurface.height
        }
    }

    Column {
        anchors.centerIn: parent
        width: Math.min(340, parent.width - 36)
        visible: roomRepeater.count === 0
        spacing: 9

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            width: 52
            height: 52
            radius: 7
            color: "#2a2418"
            border.color: "#71512e"
            Image { anchors.centerIn: parent; width: 26; height: 26; source: Qt.resolvedUrl("../assets/icons/plus.svg") }
        }
        Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; color: root.textColor; text: "把直播间放进同一张画布"; font.bold: true; font.pixelSize: 16 }
        Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; color: root.mutedTextColor; text: "输入房间号后即可添加。工作区最多容纳 9 路。"; font.pixelSize: 11; lineHeight: 1.4 }
    }
}
