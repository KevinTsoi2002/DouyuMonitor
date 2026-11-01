import QtQuick
import QtQuick.Controls.Basic
import ".."
import "../controls"

Rectangle {
    id: root
    property var controller: null
    property bool watching: true
    signal openTools()
    signal themeToggleRequested()
    signal systemMoveRequested()
    signal toggleMaximizedRequested()
    signal closeRequested()
    function focusWatchTools() {
        if (watchToolsButton.visible) watchToolsButton.forceActiveFocus(Qt.ShortcutFocusReason)
    }
    height: Theme.topBarHeight
    z: 100
    color: Theme.appBar

    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }

    Image {
        id: appIcon
        objectName: "appIcon"
        width: 26
        height: 26
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        source: Qt.resolvedUrl("../assets/brand/icon-app.png")
        sourceSize: Qt.size(52, 52)
        fillMode: Image.PreserveAspectFit
        Accessible.name: qsTr("CSTG特勤终端软件图标")
        Accessible.role: Accessible.Graphic
        Text {
            anchors.centerIn: parent
            visible: appIcon.status === Image.Error
            text: "C"
            color: Theme.accent
            font.bold: true
            font.pixelSize: Theme.fontHeading
        }
    }

    Item {
        id: brandMark
        objectName: "brandMark"
        width: 210
        height: 24
        anchors.left: appIcon.right
        anchors.leftMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        Image {
            objectName: "brandIcon"
            anchors.fill: parent
            source: Theme.isDay ? Qt.resolvedUrl("../assets/brand/wordmark-day.png") : Qt.resolvedUrl("../assets/brand/wordmark-night.png")
            sourceSize: Qt.size(420, 48)
            fillMode: Image.PreserveAspectFit
            Accessible.name: qsTr("CSTG特勤终端")
            Accessible.role: Accessible.Graphic
            Text { anchors.centerIn: parent; visible: parent.status === Image.Error; text: qsTr("CSTG特勤终端"); font.pixelSize: Theme.fontControl; color: Theme.text }
        }
    }

    Item {
        objectName: "titleBarDragArea"
        anchors.left: brandMark.right
        anchors.leftMargin: 12
        anchors.right: actions.left
        anchors.rightMargin: 12
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        DragHandler { target: null; onActiveChanged: if (active) root.systemMoveRequested() }
        TapHandler { acceptedButtons: Qt.LeftButton; onDoubleTapped: root.toggleMaximizedRequested() }
    }

    Row {
        id: actions
        anchors.right: windowControls.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8
        ConsoleToolButton {
            id: watchToolsButton
            objectName: "watchToolsButton"
            height: Theme.headerControlHeight
            width: 88
            padding: 4
            visible: root.watching
            text: qsTr("观看工具")
            Accessible.name: text
            ToolTip.text: qsTr("观看工具（T）")
            onClicked: root.openTools()
        }
        ConsoleToolButton {
            objectName: "themeToggleButton"
            height: Theme.headerControlHeight
            width: 58
            padding: 4
            text: Theme.isDay ? qsTr("夜间") : qsTr("白天")
            Accessible.name: Theme.isDay ? qsTr("切换到夜间模式") : qsTr("切换到白天模式")
            ToolTip.text: Accessible.name
            onClicked: root.themeToggleRequested()
        }
    }

    WindowControls {
        id: windowControls
        objectName: "windowControls"
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        controller: root.controller
        onCloseRequested: root.closeRequested()
    }
}
