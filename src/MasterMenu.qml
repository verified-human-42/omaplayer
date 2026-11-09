import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// The right-click master menu: every action, grouped, with its key.
// Built from the action table, so it never falls behind the keymap. The
// app's own entries (Settings, Keyboard Shortcuts) sit at the top level,
// under a divider, instead of in a submenu.
Menu {
    id: menu

    topPadding: 6
    bottomPadding: 6
    delegate: MenuEntry {}

    background: Rectangle {
        implicitWidth: 240
        color: theme.background
        radius: theme.radius
        border.width: 2
        border.color: theme.accent
    }

    Instantiator {
        model: actions.groups.filter(function (group) { return group.id !== "help"; })
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
                radius: theme.radius
                border.width: 2
                border.color: theme.accent
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

    MenuSeparator {
        leftPadding: 10
        rightPadding: 10
        contentItem: Rectangle {
            implicitHeight: 1
            color: theme.selection
        }
    }

    Instantiator {
        model: actions.list.filter(function (entry) { return entry.group === "help"; })
        delegate: MenuEntry {
            required property var modelData
            text: modelData.text
            actionId: modelData.id
            keyLabel: modelData.keyLabels.length > 0 ? modelData.keyLabels[0] : ""
        }
        onObjectAdded: (index, object) => menu.addItem(object)
        onObjectRemoved: (index, object) => menu.removeItem(object)
    }
}
