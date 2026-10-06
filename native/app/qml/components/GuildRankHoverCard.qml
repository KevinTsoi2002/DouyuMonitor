import QtQuick
import QtQuick.Controls
import ".."

Popup {
    id: root

    property var member: null
    readonly property bool hasRankData: member && member.rankMatched

    width: 320
    height: 258
    padding: 12
    closePolicy: Popup.NoAutoClose
    focus: false
    modal: false
    spacing: 8
    background: Rectangle {
        radius: 6
        color: Theme.managementSurface
        border.color: Theme.border
    }

    contentItem: Column {
        spacing: 8

        Row {
            width: parent.width
            spacing: 8

            Rectangle {
                width: 42
                height: 42
                radius: 21
                color: Theme.well
                Image {
                    anchors.fill: parent
                    anchors.margins: 1
                    source: root.member ? String(root.member.avatarUrl || "") : ""
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    sourceSize: Qt.size(84, 84)
                    clip: true
                }
            }

            Column {
                width: parent.width - 50
                spacing: 1
                Text {
                    width: parent.width
                    text: root.member ? String(root.member.anchorName || "") : ""
                    color: Theme.text
                    font.bold: true
                    font.pixelSize: 14
                    elide: Text.ElideRight
                }
                Text {
                    width: parent.width
                    text: root.member
                          ? (root.member.role === "leader" ? "团长"
                             : root.member.role === "captain" ? "队长"
                             : root.member.role === "member" ? "队员" : "其他")
                          : ""
                    color: Theme.mutedText
                    font.pixelSize: 10
                }
            }
        }

        Canvas {
            id: radar
            width: parent.width
            height: 120
            visible: root.hasRankData
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onVisibleChanged: if (visible) requestPaint()
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                const dims = root.member && root.member.radarDimensions
                           ? root.member.radarDimensions : []
                if (dims.length < 3 || width <= 0 || height <= 0) return
                const cx = width / 2
                const cy = height / 2
                const radius = Math.min(width, height) * 0.38
                ctx.strokeStyle = "#4a515a"
                ctx.lineWidth = 1
                for (let ring = 1; ring <= 3; ++ring) {
                    ctx.beginPath()
                    for (let index = 0; index <= dims.length; ++index) {
                        const angle = -Math.PI / 2 + index * 2 * Math.PI / dims.length
                        const r = radius * ring / 3
                        const x = cx + Math.cos(angle) * r
                        const y = cy + Math.sin(angle) * r
                        if (index === 0) ctx.moveTo(x, y)
                        else ctx.lineTo(x, y)
                    }
                    ctx.stroke()
                }
                ctx.beginPath()
                for (let index = 0; index <= dims.length; ++index) {
                    const angle = -Math.PI / 2 + index * 2 * Math.PI / dims.length
                    const x = cx + Math.cos(angle) * radius
                    const y = cy + Math.sin(angle) * radius
                    if (index === 0) ctx.moveTo(x, y)
                    else ctx.lineTo(x, y)
                }
                ctx.stroke()
                ctx.beginPath()
                for (let index = 0; index <= dims.length; ++index) {
                    const dim = dims[index % dims.length]
                    const value = Math.max(0, Math.min(20, Number(dim.average || 0)))
                    const angle = -Math.PI / 2 + index * 2 * Math.PI / dims.length
                    const r = radius * value / 20
                    const x = cx + Math.cos(angle) * r
                    const y = cy + Math.sin(angle) * r
                    if (index === 0) ctx.moveTo(x, y)
                    else ctx.lineTo(x, y)
                }
                ctx.closePath()
                ctx.fillStyle = "rgba(255, 141, 67, 0.28)"
                ctx.strokeStyle = Theme.accent
                ctx.lineWidth = 2
                ctx.fill()
                ctx.stroke()
            }
        }

        Text {
            width: parent.width
            visible: !root.hasRankData
            text: root.member ? "暂无野榜数据" : ""
            color: Theme.mutedText
            font.pixelSize: 11
        }

        Grid {
            width: parent.width
            columns: 2
            spacing: 6
            visible: root.hasRankData
            Text { text: "综合"; color: Theme.mutedText; font.pixelSize: 10 }
            Text {
                text: root.member && root.member.score >= 0
                      ? Number(root.member.score).toFixed(1) : "—"
                color: Theme.text
                font.bold: true
                font.pixelSize: 10
            }
            Text { text: "定级赛总评"; color: Theme.mutedText; font.pixelSize: 10 }
            Text {
                text: root.member && root.member.placementAverage >= 0
                      ? Number(root.member.placementAverage).toFixed(2) : "—"
                color: Theme.text
                font.bold: true
                font.pixelSize: 10
            }
            Text { text: "游乐值"; color: Theme.mutedText; font.pixelSize: 10 }
            Text {
                text: root.member && root.member.playValue !== null
                      && root.member.playValue !== undefined
                      ? Number(root.member.playValue).toFixed(1) : "—"
                color: Theme.text
                font.bold: true
                font.pixelSize: 10
            }
        }
    }
}
