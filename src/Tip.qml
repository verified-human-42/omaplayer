import QtQuick
import QtQuick.Controls

// A tooltip in the bar's style: the action, then its key in the muted ink.
ToolTip {
    id: tip

    property string key

    delay: 500
    padding: 0
    margins: 8

    contentItem: Row {
        spacing: 10
        leftPadding: 10
        rightPadding: 10
        topPadding: 6
        bottomPadding: 6

        Label {
            text: tip.text
            color: theme.stageForeground
            font.pixelSize: 12
        }
        Label {
            visible: tip.key !== ""
            text: tip.key
            color: theme.stageMuted
            font.pixelSize: 12
        }
    }

    background: Rectangle {
        readonly property color ink: theme.stageForeground
        color: theme.stage
        radius: Math.min(4, theme.radius)
        border.width: 1
        border.color: Qt.rgba(ink.r, ink.g, ink.b, 0.14)
    }
}
