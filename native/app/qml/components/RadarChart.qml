pragma ComponentBehavior: Bound

import QtQuick
import ".."

Item {
    id: root
    objectName: "radarChart"
    property var dimensions: []
    property real maximumValue: 20
    readonly property var normalizedDimensions: root.normalizeDimensions(root.dimensions)
    readonly property int validValueCount: normalizedDimensions.filter(function(dimension) { return dimension.value !== null; }).length
    readonly property bool hasAxes: normalizedDimensions.length >= 3
    readonly property bool completeData: hasAxes && validValueCount === normalizedDimensions.length
    readonly property real chartRadius: Math.max(0, Math.min((width - 148) / 2, (height - 102) / 2))
    readonly property color gridColor: Theme.border
    readonly property color plotColor: Theme.accent
    implicitWidth: 340
    implicitHeight: 300
    Accessible.role: Accessible.Graphic
    Accessible.name: qsTr("能力雷达图，满分 %1").arg(maximumValue)
    Accessible.description: normalizedDimensions.map(function(dimension) {
        return dimension.name + " " + root.valueText(dimension.value);
    }).join("，")

    function normalizeDimensions(values) {
        const result = [];
        for (const dimension of values || []) {
            const raw = dimension.average !== undefined ? dimension.average : dimension.value;
            const numeric = (typeof raw === "number" || typeof raw === "string")
                    && String(raw).trim().length > 0 ? Number(raw) : NaN;
            result.push({ name: String(dimension.name || dimension.label || ""),
                          value: isFinite(numeric) && numeric >= 0 ? numeric : null });
        }
        return result;
    }

    function valueText(value) {
        return value === null ? "—" : Number(value).toFixed(1);
    }

    function pointFor(index, ratio) {
        const angle = -Math.PI / 2 + index * 2 * Math.PI / root.normalizedDimensions.length;
        return Qt.point(root.width / 2 + Math.cos(angle) * root.chartRadius * ratio,
                        root.height / 2 + Math.sin(angle) * root.chartRadius * ratio);
    }

    onNormalizedDimensionsChanged: plot.requestPaint()
    onMaximumValueChanged: plot.requestPaint()
    onChartRadiusChanged: plot.requestPaint()
    onGridColorChanged: plot.requestPaint()
    onPlotColorChanged: plot.requestPaint()

    Canvas {
        id: plot
        objectName: "radarPlot"
        anchors.fill: parent
        visible: root.hasAxes
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onVisibleChanged: if (visible) requestPaint()
        onPaint: {
            const ctx = getContext("2d");
            ctx.clearRect(0, 0, width, height);
            if (!root.hasAxes || root.chartRadius <= 0 || root.maximumValue <= 0) return;
            const count = root.normalizedDimensions.length;
            ctx.lineWidth = 1;
            ctx.strokeStyle = root.gridColor;
            for (let ring = 1; ring <= 4; ++ring) {
                ctx.beginPath();
                for (let index = 0; index <= count; ++index) {
                    const point = root.pointFor(index % count, ring / 4);
                    if (index === 0) ctx.moveTo(point.x, point.y);
                    else ctx.lineTo(point.x, point.y);
                }
                ctx.stroke();
            }
            for (let index = 0; index < count; ++index) {
                const point = root.pointFor(index, 1);
                ctx.beginPath();
                ctx.moveTo(width / 2, height / 2);
                ctx.lineTo(point.x, point.y);
                ctx.stroke();
            }
            ctx.strokeStyle = root.plotColor;
            ctx.lineWidth = 2;
            if (root.completeData) {
                ctx.beginPath();
                for (let index = 0; index < count; ++index) {
                    const point = root.pointFor(index, Math.min(1, root.normalizedDimensions[index].value / root.maximumValue));
                    if (index === 0) ctx.moveTo(point.x, point.y);
                    else ctx.lineTo(point.x, point.y);
                }
                ctx.closePath();
                ctx.fillStyle = Qt.rgba(root.plotColor.r, root.plotColor.g, root.plotColor.b, 0.15);
                ctx.fill();
                ctx.stroke();
            }
            ctx.fillStyle = root.plotColor;
            for (let index = 0; index < count; ++index) {
                const value = root.normalizedDimensions[index].value;
                if (value === null) continue;
                const point = root.pointFor(index, Math.min(1, value / root.maximumValue));
                if (!root.completeData) {
                    const next = root.normalizedDimensions[(index + 1) % count].value;
                    if (next !== null) {
                        const nextPoint = root.pointFor((index + 1) % count, Math.min(1, next / root.maximumValue));
                        ctx.beginPath();
                        ctx.moveTo(point.x, point.y);
                        ctx.lineTo(nextPoint.x, nextPoint.y);
                        ctx.stroke();
                    }
                }
                ctx.beginPath();
                ctx.arc(point.x, point.y, 3, 0, Math.PI * 2);
                ctx.fill();
            }
        }
    }
    Repeater {
        model: root.hasAxes ? root.normalizedDimensions : []
        delegate: Text {
            required property int index
            required property var modelData
            readonly property real angle: -Math.PI / 2 + index * 2 * Math.PI / root.normalizedDimensions.length
            x: root.width / 2 + Math.cos(angle) * (root.chartRadius + 26) - width / 2
            y: root.height / 2 + Math.sin(angle) * (root.chartRadius + 26) - height / 2
            width: 96
            height: 34
            text: modelData.name + "\n" + root.valueText(modelData.value)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            color: modelData.value === null ? Theme.mutedText : Theme.text
            font.pixelSize: Theme.fontCaption
            Accessible.ignored: true
        }
    }
    Text {
        x: root.width / 2 + 5
        y: root.height / 2 - root.chartRadius / 2 - height
        visible: root.hasAxes
        text: String(root.maximumValue / 2)
        color: Theme.mutedText
        font.pixelSize: 10
        Accessible.ignored: true
    }
    Text {
        anchors.centerIn: parent
        visible: !root.hasAxes || root.validValueCount === 0
        text: qsTr("暂无维度数据")
        color: Theme.mutedText
        font.pixelSize: Theme.fontControl
    }
}
