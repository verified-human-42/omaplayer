import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import "Format.js" as Format

// Jump to a moment: 1:23:45, 23:45, 90 (seconds), or 50%.
Popup {
    id: dialog

    // Seconds for what's typed, or -1 when it isn't a time.
    function parse(text) {
        var value = text.trim();
        if (/^\d+(\.\d+)?%$/.test(value))
            return player.duration > 0 ? player.duration * Math.min(100, parseFloat(value)) / 100 : -1;
        if (!/^\d+(:\d{1,2}){0,2}(\.\d+)?$/.test(value))
            return -1;
        var seconds = 0;
        value.split(":").forEach(function (part) { seconds = seconds * 60 + parseFloat(part); });
        return seconds;
    }

    function go() {
        var seconds = parse(field.text);
        if (seconds < 0 || (player.duration > 0 && seconds > player.duration)) {
            field.invalid = true;
            return;
        }
        player.seekTo(seconds, true);
        close();
    }

    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    modal: true
    focus: true
    padding: 18
    width: 340

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.25)
    }

    background: Card {
        stage: false
    }

    onOpened: {
        field.invalid = false;
        field.text = Format.time(player.position);
        field.selectAll();
        field.forceActiveFocus();
    }

    contentItem: Column {
        spacing: 12

        Label {
            text: qsTr("Go to time")
            color: theme.foreground
            font.pixelSize: 14
            font.bold: true
        }

        TextField {
            id: field

            property bool invalid: false

            width: parent.width
            Material.accent: invalid ? theme.error : theme.accent
            color: theme.foreground
            font.pixelSize: 16
            inputMethodHints: Qt.ImhPreferNumbers
            onTextEdited: invalid = false
            onAccepted: dialog.go()
            Keys.onEscapePressed: dialog.close()
        }

        Label {
            width: parent.width
            text: field.invalid ? qsTr("That isn't a time in this file.")
                                : qsTr("Length %1. Also takes 23:45, 90 (seconds), or 50%.").arg(Format.time(player.duration))
            color: field.invalid ? theme.error : theme.muted
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
