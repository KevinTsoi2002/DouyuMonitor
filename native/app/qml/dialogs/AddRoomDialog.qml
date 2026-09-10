import QtQuick
import QtQuick.Controls
import ".."

Dialog {
    id: root

    objectName: "addRoomDialog"
    property var controller: null
    readonly property bool canSearch: roomInput.text.trim().length > 0
                                      && roomInput.text.trim().length <= 200
                                      && root.controller !== null
    readonly property var results: root.controller ? root.controller.searchResults : []
    readonly property string status: root.controller ? root.controller.searchStatus : "idle"
    readonly property string error: root.controller ? root.controller.searchError : ""
    modal: true
    title: "添加房间"
    width: 500
    height: 560
    padding: 16
    anchors.centerIn: parent
    palette.window: Theme.controlSurface
    palette.base: Theme.well
    palette.button: Theme.controlSurface
    palette.buttonText: Theme.text
    palette.text: Theme.text
    palette.windowText: Theme.text
    palette.highlight: Theme.accent
    palette.highlightedText: Theme.text

    background: Rectangle {
        color: Theme.controlSurface
        border.color: Theme.border
        radius: Theme.radiusLarge
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
            font.pixelSize: 16
        }
        ToolButton {
            objectName: "closeAddRoomDialogButton"
            anchors.right: parent.right
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            width: Theme.controlHeight
            height: Theme.controlHeight
            Accessible.name: "关闭添加房间"
            ToolTip.visible: hovered
            ToolTip.text: Accessible.name
            onClicked: root.close()
            contentItem: Image {
                anchors.centerIn: parent
                width: 14
                height: 14
                source: Qt.resolvedUrl("../assets/icons/x.svg")
                opacity: parent.hovered ? 1 : 0.78
            }
            background: Rectangle {
                radius: Theme.radiusSmall
                color: parent.down ? Theme.well : (parent.hovered ? Theme.well : "transparent")
            }
        }
    }

    contentItem: Column {
        spacing: 10

        Row {
            width: parent.width
            spacing: 6

            TextField {
                id: roomInput
                objectName: "roomSearchInput"
                width: parent.width - searchButton.width - 6
                placeholderText: "输入房间号、斗鱼链接或主播名字"
                color: Theme.text
                placeholderTextColor: Theme.mutedText
                selectByMouse: true
                onAccepted: root.submit()
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: Theme.well
                    border.color: Theme.border
                }
            }

            Button {
                id: searchButton
                objectName: "searchRoomsButton"
                width: 78
                text: root.status === "searching" ? "搜索中" : "搜索"
                enabled: root.canSearch && root.status !== "searching"
                onClicked: root.submit()
                contentItem: Text {
                    text: parent.text
                    color: "#1a1a1a"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.bold: true
                    font.pixelSize: 12
                }
                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: parent.down ? "#d76018" : (parent.hovered ? "#ff8d43" : Theme.accent)
                }
            }
        }

        Text {
            id: feedback
            width: parent.width
            visible: text.length > 0 || root.error.length > 0
            color: "#ff9b92"
            font.pixelSize: 12
            wrapMode: Text.Wrap
            text: root.error
        }

        Text {
            width: parent.width
            visible: root.status === "empty"
            color: "#9ba5b1"
            font.pixelSize: 12
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
                height: 66
                radius: 5
                color: resultMouse.containsMouse ? "#252c34" : "#171d25"
                border.color: resultMouse.containsMouse ? "#75462f" : "#343b45"

                Rectangle {
                    id: avatar
                    x: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 40
                    height: 40
                    radius: 20
                    color: "#28313a"

                    Image {
                        id: avatarImage
                        anchors.fill: parent
                        anchors.margins: 1
                        source: resultRow.modelData.avatarUrl
                        visible: source.toString().length > 0 && status !== Image.Error
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        smooth: true
                        clip: true
                    }

                    Text {
                        anchors.centerIn: parent
                        visible: !avatarImage.visible
                        color: "#f4f6f8"
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
                        color: "#f4f6f8"
                        font.bold: true
                        font.pixelSize: 12
                        elide: Text.ElideRight
                        text: resultRow.modelData.anchorName
                    }
                    Text {
                        width: parent.width
                        color: "#c0c7cf"
                        font.pixelSize: 10
                        elide: Text.ElideRight
                        text: resultRow.modelData.title
                    }
                    Text {
                        width: parent.width
                        color: resultRow.modelData.online ? "#65d391" : "#9ba5b1"
                        font.pixelSize: 9
                        elide: Text.ElideRight
                        text: (resultRow.modelData.online ? "直播中" : "未开播")
                              + "  ·  " + resultRow.modelData.roomId
                              + "  ·  " + resultRow.modelData.category
                    }
                }

                Button {
                    id: addButton
                    objectName: "addSearchResultButton"
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 64
                    height: 30
                    text: "加入"
                    onClicked: root.addCandidate(resultRow.modelData.roomId)
                    contentItem: Text { text: parent.text; color: Theme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 11 }
                    background: Rectangle { radius: Theme.radiusSmall; color: parent.down ? Theme.borderStrong : (parent.hovered ? Theme.border : Theme.well); border.color: Theme.borderStrong }
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
                visible: searchResultList.count === 0 && root.status === "idle"
                color: "#9ba5b1"
                font.pixelSize: 11
                text: "输入条件后搜索直播间"
            }
        }
    }

    footer: Rectangle {
        objectName: "addRoomDialogFooter"
        implicitHeight: 56
        color: Theme.controlSurface
        border.color: Theme.border
        Button {
            anchors.right: parent.right
            anchors.rightMargin: 0
            anchors.verticalCenter: parent.verticalCenter
            width: 92
            height: 34
            text: "取消"
            onClicked: root.close()
            contentItem: Text { text: parent.text; color: Theme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font.pixelSize: 12 }
            background: Rectangle { radius: Theme.radiusSmall; color: parent.down ? Theme.borderStrong : (parent.hovered ? Theme.border : Theme.well); border.color: Theme.borderStrong }
        }
    }
}
