import QtQuick

Item {
    id: root

    required property string messageText
    required property int laneIndex
    required property real laneTop
    required property real lineWidth
    required property real containerWidth
    required property int durationMs
    required property int fontSize
    required property string fontFamily
    required property real messageOpacity
    required property string rendering
    required property double launchedAt

    objectName: "danmakuLine"
    width: lineWidth
    height: foreground.implicitHeight
    x: containerWidth
    y: laneTop
    enabled: false
    z: 1

    signal finished()

    Text {
        id: shadow
        visible: root.rendering === "advanced"
        anchors.left: foreground.left
        anchors.leftMargin: 1
        anchors.top: foreground.top
        anchors.topMargin: 1
        color: "#070a0d"
        font.family: root.fontFamily
        font.pixelSize: root.fontSize
        font.bold: true
        text: root.messageText
    }

    Text {
        id: foreground
        anchors.left: parent.left
        anchors.top: parent.top
        color: "#f5f7fa"
        opacity: root.messageOpacity
        font.family: root.fontFamily
        font.pixelSize: root.fontSize
        font.bold: root.rendering === "advanced"
        style: Text.Outline
        styleColor: root.rendering === "advanced" ? "#090d12" : "#1c2731"
        text: root.messageText
    }

    NumberAnimation {
        target: root
        property: "x"
        from: root.containerWidth
        to: -root.lineWidth
        duration: root.durationMs
        easing.type: Easing.Linear
        running: true
        onStopped: root.finished()
    }
}
