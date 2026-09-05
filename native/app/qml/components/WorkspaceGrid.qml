import QtQuick
import QtQuick.Controls
import ".."

Item {
    id: root

    property var controller: null
    property var roomModel: null
    property string layoutMode: "auto"
    property string primaryRoomId: ""
    property color canvasColor: Theme.canvas
    property color surfaceColor: Theme.controlSurface
    property color borderColor: Theme.border
    property color accentColor: Theme.accent
    property color textColor: Theme.text
    property color mutedTextColor: Theme.mutedText
    readonly property int roomCount: roomRepeater.count
    function autoRowCounts(count) {
        const rows = [[], [1], [2], [3], [2, 2], [3, 2], [3, 3], [4, 3], [4, 4], [3, 3, 3]]
        return rows[Math.max(0, Math.min(9, count))]
    }

    function primarySecondaryRowCounts(count) {
        const rows = [[], [], [1], [1, 1], [1, 1, 1], [2, 2], [3, 2], [3, 3], [4, 3], [4, 4]]
        return rows[Math.max(0, Math.min(9, count))]
    }

    function primaryWidthFor(count) {
        if (count <= 1) return 1
        if (count <= 4) return 2 / 3
        return count >= 8 ? 3 / 5 : 1 / 2
    }

    function primaryZoneWidths(count) {
        if (count <= 4) return { left: 0, center: primaryWidthFor(count), right: 1 - primaryWidthFor(count) }
        const side = count >= 8 ? 1 : 1
        const center = count >= 8 ? 3 : 2
        const total = side * 2 + center
        return { left: side / total, center: center / total, right: side / total }
    }

    function primarySideCounts(count) {
        if (count <= 4) return { left: 0, right: Math.max(0, count - 1) }
        if (count === 5) return { left: 2, right: 2 }
        if (count === 6) return { left: 2, right: 3 }
        if (count === 7) return { left: 3, right: 3 }
        if (count === 8) return { left: 3, right: 4 }
        return { left: 4, right: 4 }
    }

    function primaryIndex() {
        if (primaryRoomId.length === 0) return 0
        for (var i = 0; i < roomRepeater.count; ++i) {
            var tile = roomRepeater.itemAt(i)
            if (tile && tile.roomId === primaryRoomId) return i
        }
        return 0
    }

    function rowAndColumnFor(itemIndex, rows) {
        var remaining = itemIndex
        for (var row = 0; row < rows.length; ++row) {
            if (remaining < rows[row]) return { row: row, column: remaining }
            remaining -= rows[row]
        }
        return { row: 0, column: 0 }
    }

    function geometryFor(index, roomId) {
        const gap = 8
        const width = Math.max(0, layoutSurface.width)
        const height = Math.max(0, layoutSurface.height)
        const count = Math.min(9, roomRepeater.count)
        if (count === 0) return { x: 0, y: 0, width: 0, height: 0 }

        if (layoutMode === "primary") {
            const activePrimaryIndex = primaryIndex()
            const zones = primaryZoneWidths(count)
            if (index === activePrimaryIndex) {
                if (count <= 4) {
                    const primaryWidth = count === 1
                        ? width
                        : Math.max(0, width - gap) * primaryWidthFor(count)
                    return { x: 0, y: 0, width: primaryWidth, height: height }
                }
                const availableWidth = Math.max(0, width - gap * 2)
                const leftWidth = availableWidth * zones.left
                return {
                    x: leftWidth + gap,
                    y: 0,
                    width: availableWidth * zones.center,
                    height: height,
                }
            }

            const secondaryIndex = index < activePrimaryIndex ? index : index - 1
            if (count <= 4) {
                const rows = primarySecondaryRowCounts(count)
                const position = rowAndColumnFor(secondaryIndex, rows)
                const rowHeight = Math.max(0, (height - gap * (rows.length - 1)) / rows.length)
                const columns = rows[position.row]
                const primaryWidth = Math.max(0, width - gap) * primaryWidthFor(count)
                const secondaryX = primaryWidth + gap
                const secondaryWidth = Math.max(0, width - secondaryX)
                const tileWidth = Math.max(0, (secondaryWidth - gap * (columns - 1)) / columns)
                return {
                    x: secondaryX + position.column * (tileWidth + gap),
                    y: position.row * (rowHeight + gap),
                    width: tileWidth,
                    height: rowHeight,
                }
            }

            const sides = primarySideCounts(count)
            const layoutZones = primaryZoneWidths(count)
            const availableWidth = Math.max(0, width - gap * 2)
            const leftWidth = availableWidth * layoutZones.left
            const centerWidth = availableWidth * layoutZones.center
            const rightWidth = availableWidth * layoutZones.right
            const leftX = 0
            const centerX = leftWidth + gap
            const rightX = centerX + centerWidth + gap
            const leftCount = sides.left
            const rightIndex = secondaryIndex - leftCount
            const side = secondaryIndex < leftCount ? "left" : "right"
            const sideCount = side === "left" ? sides.left : sides.right
            const sideIndex = side === "left" ? secondaryIndex : rightIndex
            const totalUnits = side === "left" ? sides.left : sides.right
            const unitHeight = Math.max(0, (height - gap * (totalUnits - 1)) / totalUnits)
            return {
                x: side === "left" ? leftX : rightX,
                y: sideIndex * (unitHeight + gap),
                width: side === "left" ? leftWidth : rightWidth,
                height: unitHeight,
            }
        }

        const rows = autoRowCounts(count)
        const position = rowAndColumnFor(index, rows)
        const rowHeight = Math.max(0, (height - gap * (rows.length - 1)) / rows.length)
        const columns = rows[position.row]
        const tileWidth = Math.max(0, (width - gap * (columns - 1)) / columns)
        return {
            x: position.column * (tileWidth + gap),
            y: position.row * (rowHeight + gap),
            width: tileWidth,
            height: rowHeight,
        }
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
                readonly property var tileGeometry: root.geometryFor(index, roomId)
                x: tileGeometry.x
                y: tileGeometry.y
                width: tileGeometry.width
                height: tileGeometry.height
                controller: root.controller
                borderColor: root.borderColor
                accentColor: root.accentColor
                textColor: root.textColor
                mutedTextColor: root.mutedTextColor
            }
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
            color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.16)
            border.color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.46)
            Image { anchors.centerIn: parent; width: 26; height: 26; source: Qt.resolvedUrl("../assets/icons/plus.svg") }
        }
        Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; color: root.textColor; text: "把直播间放进同一张画布"; font.bold: true; font.pixelSize: 16 }
        Text { width: parent.width; horizontalAlignment: Text.AlignHCenter; wrapMode: Text.Wrap; color: root.mutedTextColor; text: "输入房间号后即可添加。工作区最多容纳 9 路。"; font.pixelSize: 11; lineHeight: 1.4 }
    }
}
