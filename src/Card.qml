import QtQuick

// A box drawn the way Omarchy draws its own popups (the OSD, notifications,
// menus): a near-solid surface, a 2px accent border, and Hyprland's rounding.
Rectangle {
    // On or beside the picture, cards take the stage colours; elsewhere the
    // theme as it is.
    property bool stage: true
    property color edge: theme.accent
    readonly property color surface: stage ? theme.stage : theme.background
    // How solid the surface is; the border and anything inside stay opaque.
    property real fill: 0.97

    color: Qt.rgba(surface.r, surface.g, surface.b, fill)
    radius: theme.radius
    border.width: 2
    border.color: edge
}
