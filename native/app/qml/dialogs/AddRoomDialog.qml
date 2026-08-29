import QtQuick
import QtQuick.Controls

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
    anchors.centerIn: parent

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
                selectByMouse: true
                onAccepted: root.submit()
            }

            Button {
                id: searchButton
                objectName: "searchRoomsButton"
                width: 78
                text: root.status === "searching" ? "搜索中" : "搜索"
                enabled: root.canSearch && root.status !== "searching"
                onClicked: root.submit()
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

    footer: DialogButtonBox {
        Button {
            text: "取消"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
            onClicked: root.close()
        }
    }
}
