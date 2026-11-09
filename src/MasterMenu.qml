import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// The right-click master menu: every action, grouped, with its key.
// Built from the action table, so it never falls behind the keymap.
Menu {
    id: menu

    topPadding: 6
    bottomPadding: 6
    delegate: MenuEntry {}

    background: Rectangle {
        implicitWidth: 240
        color: theme.background
        radius: Math.min(6, theme.radius)
        border.width: 1
        border.color: theme.selection
    }

    Instantiator {
        model: actions.groups
        delegate: Menu {
            id: group

            required property var modelData

            title: modelData.text
            topPadding: 6
            bottomPadding: 6
            delegate: MenuEntry {}
            background: Rectangle {
                implicitWidth: 260
                color: theme.background
                radius: Math.min(6, theme.radius)
                border.width: 1
                border.color: theme.selection
            }

            Instantiator {
                model: actions.list.filter(function (entry) { return entry.group === group.modelData.id; })
                delegate: MenuEntry {
                    required property var modelData
                    text: modelData.text
                    actionId: modelData.id
                    keyLabel: modelData.keyLabels.length > 0 ? modelData.keyLabels[0] : ""
                }
                onObjectAdded: (index, object) => group.insertItem(index, object)
                onObjectRemoved: (index, object) => group.removeItem(object)
            }
        }
        onObjectAdded: (index, object) => menu.insertMenu(index, object)
        onObjectRemoved: (index, object) => menu.removeMenu(object)
    }
}
