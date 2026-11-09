import QtQuick
import QtQuick.Controls

// A line of text for a setting, saved when you press Enter or leave it.
Rectangle {
    id: field

    property string value
    property string placeholder
    readonly property color ink: theme.foreground

    signal committed(string next)
    // Enter, Esc, or Tab: the keyboard goes back to the page.
    signal done()

    function forceActiveFocus() {
        input.forceActiveFocus();
    }

    implicitWidth: 220
    implicitHeight: 28
    color: "transparent"
    radius: Math.max(6, theme.radius)
    border.width: 1
    border.color: input.activeFocus ? theme.accent : Qt.rgba(ink.r, ink.g, ink.b, 0.16)

    TextInput {
        id: input
        anchors.fill: parent
        leftPadding: 10
        rightPadding: 10
        verticalAlignment: TextInput.AlignVCenter
        color: field.ink
        selectionColor: theme.accent
        selectedTextColor: theme.accentForeground
        font.pixelSize: 12
        clip: true
        text: field.value
        onEditingFinished: if (text !== field.value) field.committed(text)
        Keys.onReturnPressed: (event) => { field.committed(text); field.done(); }
        Keys.onEnterPressed: (event) => { field.committed(text); field.done(); }
        Keys.onTabPressed: (event) => { field.committed(text); field.done(); }
        Keys.onEscapePressed: (event) => {
            text = field.value;
            event.accepted = true;
            field.done();
        }
        // Arrows stay in the field rather than moving through the page.
        Keys.onUpPressed: (event) => event.accepted = true
        Keys.onDownPressed: (event) => event.accepted = true
        Keys.onLeftPressed: (event) => { cursorPosition = Math.max(0, cursorPosition - 1); event.accepted = true; }
        Keys.onRightPressed: (event) => { cursorPosition = Math.min(text.length, cursorPosition + 1); event.accepted = true; }

        Connections {
            target: field
            function onValueChanged() {
                if (!input.activeFocus)
                    input.text = field.value;
            }
        }
    }

    Label {
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        visible: input.text === "" && !input.activeFocus
        text: field.placeholder
        color: Qt.rgba(field.ink.r, field.ink.g, field.ink.b, 0.4)
        font.pixelSize: 12
    }
}
