import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A submenu of the right-click menu, drawn like the menu itself.
Menu {
    topPadding: 6
    bottomPadding: 6
    delegate: MenuEntry {}

    background: Rectangle {
        implicitWidth: 340
        color: theme.background
        radius: theme.radius
        border.width: 2
        border.color: theme.accent
    }
}
