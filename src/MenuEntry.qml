import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// A menu row: the action on the left, its key on the right, an accent rule
// on the highlighted row.
MenuItem {
    id: entry

    property string actionId
    property string keyLabel
    // Part of a list (tracks, devices) or an on/off option: room for a tick,
    // shown when this one is chosen or on. Not Qt's checkable, which flips
    // itself and drops the binding.
    property bool tickable: false
    property bool ticked: false

    implicitHeight: 30
    leftPadding: entry.tickable ? 30 : 14
    rightPadding: 14
    onTriggered: if (actionId !== "") actions.trigger(actionId)

    contentItem: Item {
        implicitWidth: label.implicitWidth + (entry.keyLabel !== "" ? key.implicitWidth + 36 : 0) + (entry.subMenu ? 16 : 0)
        implicitHeight: label.implicitHeight

        Label {
            id: label
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            text: entry.text
            color: entry.enabled ? theme.foreground : theme.muted
            font.pixelSize: 13
        }
        Label {
            id: key
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: entry.keyLabel
            color: theme.muted
            font.pixelSize: 12
        }
    }

    arrow: Label {
        x: entry.mirrored ? entry.leftPadding : entry.width - width - entry.rightPadding
        anchors.verticalCenter: parent.verticalCenter
        visible: entry.subMenu
        text: entry.mirrored ? "‹" : "›"
        color: theme.muted
        font.pixelSize: 14
    }

    // A tick for what's chosen in a list or switched on.
    indicator: Label {
        x: 11
        anchors.verticalCenter: parent.verticalCenter
        visible: entry.tickable && entry.ticked
        text: "✓"
        color: theme.accent
        font.pixelSize: 13
        font.bold: true
    }

    background: Rectangle {
        implicitWidth: 220
        color: entry.highlighted ? theme.selection : "transparent"

        Rectangle {
            visible: entry.highlighted
            width: 2
            height: parent.height
            color: theme.accent
        }
    }
}
