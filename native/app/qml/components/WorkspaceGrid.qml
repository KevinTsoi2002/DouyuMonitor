import QtQuick
import QtQuick.Controls
import ".."

Item {
    id: root

    property var controller: null
    signal fullScreenToggleRequested()
    property var roomModel: null
    property string layoutMode: "auto"
    property string primaryRoomId: ""
    property string secondaryPrimaryRoomId: ""
    property color canvasColor: Theme.canvas
    property color surfaceColor: Theme.controlSurface
    property color borderColor: Theme.border
    property color accentColor: Theme.accent
    property color textColor: Theme.text
    property color mutedTextColor: Theme.mutedText
    readonly property int roomCount: roomRepeater.count

    function columnsFor(count) {
        if (count <= 1) return 1
        if (count === 2) return 2
        if (count === 3) return 3
        if (count === 4) return 2
        if (count <= 6) return 3
        if (count <= 9) return 3
        if (count <= 12) return 4
        if (count <= 16) return 4
        if (count <= 20) return 5
        return 6
    }

    function bottomColumnsForDual(count) {
        if (count <= 4) return 2
        if (count <= 12) return 4
        if (count <= 16) return 5
        return 6
    }

    function rowCountsFor(count, targetColumns) {
        if (count <= 0) return []
        const safeColumns = Math.max(1, targetColumns)
        const rowCount = Math.max(1, Math.ceil(count / safeColumns))
        const base = Math.floor(count / rowCount)
        const widerRows = count % rowCount
        const rows = []
        for (let row = 0; row < rowCount; ++row) {
            rows.push(base + (row < widerRows ? 1 : 0))
        }
        return rows
    }

    function primaryIndex() {
        if (primaryRoomId.length === 0) return 0
        for (var i = 0; i < roomRepeater.count; ++i) {
            var tile = roomRepeater.itemAt(i)
            if (tile && tile.roomId === primaryRoomId) return i
        }
        return 0
    }

    function secondaryPrimaryIndex(firstIndex) {
        if (secondaryPrimaryRoomId.length > 0) {
            for (var i = 0; i < roomRepeater.count; ++i) {
                var tile = roomRepeater.itemAt(i)
                if (tile && tile.roomId === secondaryPrimaryRoomId && i !== firstIndex) {
                    return i
                }
            }
        }
        for (var candidate = 0; candidate < roomRepeater.count; ++candidate) {
            if (candidate !== firstIndex) return candidate
        }
        return -1
    }

    function rowAndColumnFor(itemIndex, rows) {
        var remaining = itemIndex
        for (var row = 0; row < rows.length; ++row) {
            if (remaining < rows[row]) return { row: row, column: remaining }
            remaining -= rows[row]
        }
        return { row: 0, column: 0 }
    }

    function geometryFor(index) {
        const width = Math.max(0, layoutSurface.width)
        const height = Math.max(0, layoutSurface.height)
        const count = roomRepeater.count
        if (count === 0) return { x: 0, y: 0, width: 0, height: 0 }

        if (layoutMode === "primary-two") {
            const first = primaryIndex()
            const second = secondaryPrimaryIndex(first)
            if (index === first || index === second) {
                return {
                    x: index === second ? width / 2 : 0,
                    y: 0,
                    width: width / 2,
                    height: height * 0.58,
                }
            }

            let bottomPosition = 0
            for (let candidate = 0; candidate < count; ++candidate) {
                if (candidate === first || candidate === second) continue
                if (candidate === index) break
                ++bottomPosition
            }
            const bottomCount = Math.max(0, count - 2)
            const rowCounts = rowCountsFor(bottomCount, bottomColumnsForDual(count))
            const position = rowAndColumnFor(bottomPosition, rowCounts)
            const topHeight = height * 0.58
            const bottomHeight = height - topHeight
            const rowHeight = rowCounts.length > 0 ? bottomHeight / rowCounts.length : 0
            const columns = rowCounts.length > 0 ? rowCounts[position.row] : 1
            const tileWidth = columns > 0 ? width / columns : 0
            return {
                x: position.column * tileWidth,
                y: topHeight + position.row * rowHeight,
                width: tileWidth,
                height: rowHeight,
            }
        }

        if (layoutMode === "primary") {
            const activePrimaryIndex = primaryIndex()
            if (index === activePrimaryIndex) {
                if (count <= 1) return { x: 0, y: 0, width: width, height: height }
                if (count <= 4) {
                    return { x: 0, y: 0, width: width * 2 / 3, height: height }
                }
                if (count <= 8) {
                    const sideWidth = width / 3.35
                    return { x: sideWidth, y: 0, width: width - sideWidth * 2, height: height }
                }
                return { x: 0, y: 0, width: width * 0.44, height: height }
            }

            const secondaryIndex = index < activePrimaryIndex ? index : index - 1
            if (count <= 4) {
                const rowCounts = rowCountsFor(count - 1, 1)
                const position = rowAndColumnFor(secondaryIndex, rowCounts)
                const rowHeight = rowCounts.length > 0 ? height / rowCounts.length : 0
                const secondaryX = width * 2 / 3
                return {
                    x: secondaryX,
                    y: position.row * rowHeight,
                    width: width - secondaryX,
                    height: rowHeight,
                }
            }

            if (count <= 8) {
                const leftCount = Math.floor((count - 1) / 2)
                const side = secondaryIndex < leftCount ? "left" : "right"
                const sideIndex = side === "left" ? secondaryIndex : secondaryIndex - leftCount
                const sideCount = side === "left" ? leftCount : count - 1 - leftCount
                const sideWidth = width / 3.35
                const rowHeight = sideCount > 0 ? height / sideCount : 0
                return {
                    x: side === "left" ? 0 : width - sideWidth,
                    y: sideIndex * rowHeight,
                    width: sideWidth,
                    height: rowHeight,
                }
            }

            const secondaryX = width * 0.44
            const secondaryWidth = width - secondaryX
            const rowCounts = rowCountsFor(count - 1, 4)
            const position = rowAndColumnFor(secondaryIndex, rowCounts)
            const rowHeight = rowCounts.length > 0 ? height / rowCounts.length : 0
            const columns = rowCounts.length > 0 ? rowCounts[position.row] : 1
            const tileWidth = columns > 0 ? secondaryWidth / columns : 0
            return {
                x: secondaryX + position.column * tileWidth,
                y: position.row * rowHeight,
                width: tileWidth,
                height: rowHeight,
            }
        }

        const rowCounts = rowCountsFor(count, columnsFor(count))
        const position = rowAndColumnFor(index, rowCounts)
        const rowHeight = rowCounts.length > 0 ? height / rowCounts.length : 0
        const columns = rowCounts.length > 0 ? rowCounts[position.row] : 1
        const tileWidth = columns > 0 ? width / columns : 0
        return {
            x: position.column * tileWidth,
            y: position.row * rowHeight,
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
                readonly property var tileGeometry: root.geometryFor(index)
                x: tileGeometry.x
                y: tileGeometry.y
                width: tileGeometry.width
                height: tileGeometry.height
                controller: root.controller
                onFullScreenToggleRequested: root.fullScreenToggleRequested()
                secondaryPrimary: roomId === root.secondaryPrimaryRoomId
                borderColor: root.borderColor
                accentColor: root.accentColor
                textColor: root.textColor
                mutedTextColor: root.mutedTextColor
            }
        }
    }

    Column {
        id: emptyWorkspaceState
        objectName: "emptyWorkspaceState"
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
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            color: root.mutedTextColor
            text: "输入房间号后即可添加。工作区最多容纳 "
                  + (root.controller && root.controller.workspace
                     && root.controller.workspace.maxRooms
                     ? root.controller.workspace.maxRooms : 16)
                  + " 路。"
            font.pixelSize: 11
            lineHeight: 1.4
        }
    }
}
