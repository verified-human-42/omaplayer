import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// An icon button on the control bar. Given an action, it runs it, and its
// tooltip names it with its shortcut, both straight from the action table.
ToolButton {
    id: button

    property string iconName
    property string actionId
    property bool lit: false
    property string tip: {
        actions.list; // re-read after a language or keymap change
        return actionId !== "" ? actions.text(actionId) : "";
    }
    readonly property color ink: theme.stageForeground

    focusPolicy: Qt.NoFocus
    implicitWidth: 34
    implicitHeight: 34
    padding: 7
    display: AbstractButton.IconOnly
    icon.source: "qrc:/icons/" + iconName + ".svg"
    icon.width: 20
    icon.height: 20
    icon.color: lit ? theme.accent : hovered ? ink : Qt.rgba(ink.r, ink.g, ink.b, 0.78)

    onClicked: if (actionId !== "") actions.trigger(actionId)

    background: Rectangle {
        radius: Math.min(4, theme.radius)
        color: button.down ? Qt.rgba(button.ink.r, button.ink.g, button.ink.b, 0.16)
             : button.hovered ? Qt.rgba(button.ink.r, button.ink.g, button.ink.b, 0.08)
             : "transparent"
    }

    // Made on first hover; a bar's worth of tooltips is too much to build at launch.
    Loader {
        active: button.hovered && button.tip !== ""
        sourceComponent: Tip {
            visible: true
            text: button.tip
            key: {
                actions.list;
                return button.actionId !== "" ? actions.shortcut(button.actionId) : "";
            }
        }
    }
}
