import QtQuick
import QtQuick.Controls

// Segmented choices: the picked one filled with the accent.
Rectangle {
    id: group

    property var options: []
    property string value
    readonly property color ink: theme.foreground
    readonly property color accent: theme.accent

    signal picked(string next)

    implicitWidth: row.implicitWidth + 4
    implicitHeight: 26
    radius: Math.max(6, theme.radius)
    color: "transparent"
    border.width: 1
    border.color: Qt.rgba(ink.r, ink.g, ink.b, 0.16)

    Row {
        id: row
        anchors.centerIn: parent
        spacing: 2

        Repeater {
            model: group.options
            delegate: Rectangle {
                id: option

                required property var modelData
                readonly property bool selected: String(modelData.value) === String(group.value)

                implicitWidth: optionText.implicitWidth + 20
                implicitHeight: 22
                radius: Math.max(4, theme.radius - 2)
                color: selected ? group.accent
                     : hover.hovered ? Qt.rgba(group.ink.r, group.ink.g, group.ink.b, 0.08) : "transparent"

                Label {
                    id: optionText
                    anchors.centerIn: parent
                    text: option.modelData.label
                    color: option.selected ? theme.accentForeground
                                           : Qt.rgba(group.ink.r, group.ink.g, group.ink.b, 0.75)
                    font.pixelSize: 11
                    font.weight: option.selected ? Font.DemiBold : Font.Normal
                }

                HoverHandler {
                    id: hover
                    cursorShape: Qt.PointingHandCursor
                }
                TapHandler {
                    onTapped: group.picked(String(option.modelData.value))
                }
            }
        }
    }
}
