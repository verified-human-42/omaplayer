import QtQuick
import QtQuick.Controls

// A pick-one list that stays one row tall however long the list gets: the
// current choice, and a menu of all of them. For lists the machine decides,
// like sound outputs.
Rectangle {
    id: picker

    property var options: [] // [{ value, label }]
    property string value
    readonly property color ink: theme.foreground
    readonly property int current: {
        for (var i = 0; i < options.length; ++i)
            if (String(options[i].value) === value)
                return i;
        return 0;
    }

    signal picked(string next)

    function open() {
        menu.open();
    }

    implicitWidth: 300
    implicitHeight: 24
    color: hover.hovered ? Qt.rgba(ink.r, ink.g, ink.b, 0.05) : "transparent"
    radius: Math.max(6, theme.radius)
    border.width: 1
    border.color: menu.visible ? theme.accent : Qt.rgba(ink.r, ink.g, ink.b, 0.16)

    Label {
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.right: chevron.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: picker.options.length > 0 ? picker.options[picker.current].label : ""
        color: picker.ink
        font.pixelSize: 11
        elide: Text.ElideRight
    }
    Label {
        id: chevron
        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        text: "▾"
        color: Qt.rgba(picker.ink.r, picker.ink.g, picker.ink.b, 0.6)
        font.pixelSize: 12
    }

    HoverHandler {
        id: hover
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        onTapped: menu.visible ? menu.close() : menu.open()
    }

    Popup {
        id: menu
        y: picker.height + 4
        x: picker.width - width
        width: Math.max(picker.width, 340)
        margins: 8
        padding: 4

        background: Card {
            stage: false
        }

        contentItem: ListView {
            id: list
            implicitHeight: Math.min(contentHeight, 300)
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            model: picker.options

            delegate: Rectangle {
                id: option

                required property var modelData
                required property int index

                width: list.width
                height: 30
                radius: Math.min(4, theme.radius)
                color: optionHover.hovered ? Qt.rgba(picker.ink.r, picker.ink.g, picker.ink.b, 0.07) : "transparent"

                Label {
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.right: parent.right
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    text: option.modelData.label
                    color: option.index === picker.current ? theme.accent : picker.ink
                    font.pixelSize: 11
                    font.bold: option.index === picker.current
                    elide: Text.ElideRight
                }
                HoverHandler {
                    id: optionHover
                    cursorShape: Qt.PointingHandCursor
                }
                TapHandler {
                    onTapped: {
                        picker.picked(String(option.modelData.value));
                        menu.close();
                    }
                }
            }
        }
    }
}
