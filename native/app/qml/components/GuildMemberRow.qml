import QtQuick
import QtQuick.Controls
import ".."

Item {
    id: root

    property var member: null
    property string roomStatus: ""
    property bool active: false
    property bool canAdd: false
    signal addRequested(string memberId)
    signal roomIdSubmitted(string memberId, string roomId)
}