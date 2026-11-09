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
        leftPadding: 12
        rightPadding: 12
        topPadding: 8
        bottomPadding: 8

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

    background: Card {}
}
