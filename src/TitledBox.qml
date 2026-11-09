import QtQuick
import QtQuick.Controls

// A card with its title set into the top border, the way btop and other
// terminal UIs on Omarchy draw their panels.
Item {
    id: box

    property string title
    property bool stage: true
    property color edge: theme.accent
    property color titleColor: stage ? theme.stageForeground : theme.foreground
    readonly property real titleHeight: titleLabel.implicitHeight

    Card {
        id: card
        anchors.fill: parent
        anchors.topMargin: box.titleHeight / 2
        stage: box.stage
        edge: box.edge
    }

    Rectangle {
        visible: box.title !== ""
        x: Math.max(12, theme.radius + 6)
        width: titleLabel.implicitWidth + 12
        height: titleLabel.implicitHeight
        color: card.surface

        Label {
            id: titleLabel
            anchors.centerIn: parent
            text: box.title
            color: box.titleColor
            font.pixelSize: 13
            font.bold: true
        }
    }
}
