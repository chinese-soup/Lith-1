import QtQuick 2.0

Rectangle {
    color: mouse.containsPress ? "gray" : mouse.containsMouse ? Qt.lighter("gray") : index == stuff.selectedIndex ? Qt.darker("light gray") : "light gray"
    Behavior on color { ColorAnimation { duration: 60 } }
    width: parent.width
    height: childrenRect.height
    Text {
        font.family: "Consolas"
        text: buffer.name.split(".")[buffer.name.split(".").length - 1]
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        onClicked: stuff.selectedIndex = index
    }
}
