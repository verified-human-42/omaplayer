import QtQuick
import QtQuick.Controls

// A box with its title set into the top border, the way terminal UIs draw
// panels. Drawn opaque inside a layer, so the whole thing fades as one.
Item {
    id: box

    property string title
    property color fillColor: theme.background
    property color lineColor: theme.selection
    property color titleColor: theme.accent
    property real fillOpacity: 1
    readonly property real titleHeight: titleLabel.implicitHeight

    layer.enabled: fillOpacity < 1
    opacity: fillOpacity

    Rectangle {
        anchors.fill: parent
        anchors.topMargin: box.titleHeight / 2
        color: box.fillColor
        radius: Math.min(6, theme.radius)
        border.width: 1
        border.color: box.lineColor
    }

    Rectangle {
        visible: box.title !== ""
        x: 14
        width: titleLabel.implicitWidth + 12
        height: titleLabel.implicitHeight
        color: box.fillColor

        Label {
            id: titleLabel
            anchors.centerIn: parent
            text: box.title
            color: box.titleColor
            font.pixelSize: 11
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1.4
        }
    }
}
