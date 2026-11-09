import QtQuick

// On/off, drawn like the Omarchy shell's switch: a pill when windows are
// rounded, square when they aren't.
Item {
    id: toggle

    property bool checked: false
    readonly property color ink: theme.foreground
    readonly property color accent: theme.accent
    readonly property bool rounded: theme.radius > 0

    signal toggled()

    implicitWidth: 42
    implicitHeight: 22

    Rectangle {
        id: track
        anchors.fill: parent
        radius: toggle.rounded ? height / 2 : 0
        color: toggle.checked ? toggle.accent : "transparent"
        border.width: 1
        border.color: toggle.checked ? toggle.accent : Qt.rgba(toggle.ink.r, toggle.ink.g, toggle.ink.b, 0.3)

        Behavior on color {
            ColorAnimation { duration: 120 }
        }
    }

    Rectangle {
        width: 16
        height: 16
        radius: toggle.rounded ? width / 2 : 0
        anchors.verticalCenter: parent.verticalCenter
        x: toggle.checked !== toggle.mirrored ? parent.width - width - 3 : 3
        color: toggle.checked ? theme.accentForeground
                              : Qt.rgba(toggle.ink.r, toggle.ink.g, toggle.ink.b, 0.55)

        Behavior on x {
            NumberAnimation { duration: 120; easing.type: Easing.OutCubic }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: toggle.toggled()
    }

    readonly property bool mirrored: Qt.application.layoutDirection === Qt.RightToLeft
}
