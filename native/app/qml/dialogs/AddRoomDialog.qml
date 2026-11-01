import QtQuick
import QtQuick.Controls.Basic
import ".."
import "../controls"

Dialog {
    id: root

    objectName: "addRoomDialog"
    property var controller: null
    readonly property bool canSearch: roomInput.text.trim().length > 0
                                      && roomInput.text.trim().length <= 200
                                      && root.controller !== null
    readonly property var results: root.controller && root.controller.searchResults ? root.controller.searchResults : []
    readonly property string status: root.controller && root.controller.searchStatus ? root.controller.searchStatus : "idle"
    readonly property string error: root.controller && root.controller.searchError ? root.controller.searchError : ""
    modal: true
    title: "添加房间"
    width: Math.min(560, parent ? parent.width - 32 : 560)
    height: Math.min(560, parent ? parent.height - 32 : 560)
    padding: 16
    anchors.centerIn: parent
    palette {
        window: Theme.managementSurface
        base: Theme.well
        alternateBase: Theme.controlSurface
        light: Theme.controlHover
        midlight: Theme.controlSurface
        mid: Theme.border
        dark: Theme.mutedText
        shadow: Theme.canvas
        toolTipBase: Theme.controlSurface
        toolTipText: Theme.text
        link: Theme.accent
        linkVisited: Theme.mutedText
        active {
            windowText: Theme.text
            text: Theme.text
            button: Theme.controlSurface
            buttonText: Theme.text
            brightText: Theme.text
            highlight: Theme.accent
            highlightedText: Theme.accentText
            placeholderText: Theme.mutedText
        }
        inactive {
            windowText: Theme.text
            text: Theme.text
            button: Theme.controlSurface
            buttonText: Theme.text
            brightText: Theme.text
            highlight: Theme.accent
            highlightedText: Theme.accentText
            placeholderText: Theme.mutedText
        }
        disabled {
            windowText: Theme.disabledText
            text: Theme.disabledText
            button: Theme.controlDisabled
            buttonText: Theme.disabledText
            brightText: Theme.disabledText
            highlightedText: Theme.disabledText
            highlight: Theme.borderStrong
            placeholderText: Theme.disabledText
        }
    }

    background: Rectangle {
        color: Theme.managementSurface
        border.color: Theme.border
        radius: Theme.radiusSmall
    }

    function submit() {
        if (!canSearch) return
        feedback.text = ""
        root.controller.searchRooms(roomInput.text.trim())
    }

    function addCandidate(roomId) {
        if (!root.controller) return
        const message = root.controller.addRoomCandidate(roomId)
        if (message.length === 0) {
            roomInput.clear()
            root.close()
        } else {
            feedback.text = message
        }
    }

    onOpened: roomInput.forceActiveFocus()

    header: Item {
        implicitHeight: 48
        Text {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: root.title
            color: Theme.text
            font.bold: true
            font.pixelSize: Theme.fontHeading
        }
        ConsoleToolButton {
            objectName: "closeAddRoomDialogButton"
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.controlHeight
            height: Theme.controlHeight
            Accessible.name: "关闭添加房间"
            ToolTip.text: Accessible.name
            onClicked: root.close()
            icon.source: Qt.resolvedUrl("../assets/icons/x.svg")
        }
    }

    contentItem: Column {
        spacing: Theme.spaceMedium

        Row {
            width: parent.width
            spacing: Theme.spaceXS

            ConsoleTextField {
                id: roomInput
                objectName: "roomSearchInput"
                width: parent.width - searchButton.width - 6
                placeholderText: "输入房间号、斗鱼链接或主播名字"
                selectByMouse: true
                onAccepted: root.submit()
            }

            ConsoleButton {
                id: searchButton
                objectName: "searchRoomsButton"
                width: 78
                highlighted: true
                text: root.status === "searching" ? "搜索中" : "搜索"
                enabled: root.canSearch && root.status !== "searching"
                onClicked: root.submit()
            }
        }

        Text {
            id: feedback
            width: parent.width
            visible: text.length > 0 || root.error.length > 0
            color: Theme.danger
            font.pixelSize: Theme.fontCaption
            wrapMode: Text.Wrap
            text: root.error
        }

        Text {
            width: parent.width
            visible: root.status === "empty"
            color: Theme.mutedText
            font.pixelSize: Theme.fontCaption
            text: "没有找到匹配的直播间，请检查输入。"
        }

        ListView {
            id: searchResultList
            objectName: "searchResultList"
            width: parent.width
            height: Math.max(80, parent.height - 95)
            clip: true
            spacing: 5
            model: root.results
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: resultRow
                required property var modelData
                width: searchResultList.width - 8
                height: 76
                radius: Theme.radiusSmall
                color: resultMouse.containsMouse ? Theme.controlHover : Theme.well
                border.color: resultMouse.containsMouse ? Theme.borderStrong : Theme.border

                Rectangle {
                    id: avatar
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 40
                    height: 40
                    radius: 20
                    color: Theme.managementSurface

                    Image {
                        id: avatarImage
                        anchors.fill: parent
                        anchors.margins: 1
                        source: resultRow.modelData.avatarUrl
                        sourceSize.width: 40
                        sourceSize.height: 40
                        visible: source.toString().length > 0 && status !== Image.Error
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        smooth: true
                        clip: true
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: !avatarImage.visible
                        color: Theme.text
                        font.bold: true
                        font.pixelSize: 14
                        text: resultRow.modelData.anchorName.length > 0
                              ? resultRow.modelData.anchorName.slice(0, 1)
                              : resultRow.modelData.roomId.slice(0, 1)
                    }
                }

                Column {
                    x: avatar.x + avatar.width + 9
                    y: 8
                    width: parent.width - x - addButton.width - 18
                    spacing: 2

                    Text {
                        width: parent.width
                        color: Theme.text
                        font.bold: true
                        font.pixelSize: Theme.fontBody
                        elide: Text.ElideRight
                        text: resultRow.modelData.anchorName
                    }
                    Text {
                        width: parent.width
                        color: Theme.mutedText
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideRight
                        text: resultRow.modelData.title
                    }
                    Text {
                        width: parent.width
                        color: resultRow.modelData.online ? Theme.online : Theme.mutedText
                        font.pixelSize: Theme.fontCaption
                        elide: Text.ElideRight
                        text: (resultRow.modelData.statusKnown === false ? "状态未知"
                               : (resultRow.modelData.online ? "直播中" : "未开播"))
                              + "  ·  " + resultRow.modelData.roomId
                              + "  ·  " + resultRow.modelData.category
                    }
                }

                ConsoleButton {
                    id: addButton
                    objectName: "addSearchResultButton"
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 64
                    height: 30
                    text: "加入"
                    onClicked: root.addCandidate(resultRow.modelData.roomId)
                    font.pixelSize: Theme.fontCaption
                }

                MouseArea {
                    id: resultMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    acceptedButtons: Qt.NoButton
                }
            }

            Text {
                anchors.centerIn: parent
                visible: searchResultList.count === 0 && (root.status === "idle" || root.status === "searching")
                color: Theme.mutedText
                font.pixelSize: Theme.fontCaption
                text: root.status === "searching" ? "正在搜索直播间…" : "输入房间号、链接或主播名开始搜索"
            }
        }
    }

    footer: Rectangle {
        objectName: "addRoomDialogFooter"
        implicitHeight: 56
        color: Theme.managementSurface
        border.color: Theme.border
        ConsoleButton {
            anchors.right: parent.right
            anchors.rightMargin: 0
            anchors.verticalCenter: parent.verticalCenter
            width: 92
            height: 34
            text: "取消"
            onClicked: root.close()
        }
    }
}
