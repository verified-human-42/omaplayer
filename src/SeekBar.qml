import QtQuick
import QtQuick.Controls
import "Format.js" as Format

// The timeline. Dragging scrubs on keyframes for speed and lands exactly on
// release; the wheel steps like the arrow keys.
Item {
    id: bar

    readonly property real duration: player.duration
    readonly property bool usable: !player.idle && player.seekable && duration > 0
    readonly property bool active: usable && (mouse.containsMouse || mouse.pressed)
    property real target: 0
    // Shows the target until mpv has caught up, so the playhead never jumps back.
    property bool holding: false
    readonly property real shown: holding ? target : player.position
    readonly property real fraction: duration > 0 ? Math.max(0, Math.min(1, shown / duration)) : 0
    readonly property color ink: theme.stageForeground

    implicitHeight: 22

    Rectangle {
        id: track
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: bar.active ? 6 : 3
        radius: Math.min(height / 2, theme.radius)
        color: Qt.rgba(bar.ink.r, bar.ink.g, bar.ink.b, 0.18)

        Rectangle {
            width: parent.width * bar.fraction
            height: parent.height
            radius: parent.radius
            color: theme.accent
        }
    }

    Rectangle {
        visible: bar.active
        width: 13
        height: 13
        radius: width / 2
        x: track.width * bar.fraction - width / 2
        anchors.verticalCenter: parent.verticalCenter
        color: theme.accent
    }

    Rectangle {
        visible: bar.usable && mouse.containsMouse
        width: hoverTime.implicitWidth + 14
        height: hoverTime.implicitHeight + 8
        x: Math.max(0, Math.min(bar.width - width, mouse.mouseX - width / 2))
        y: -height - 8
        radius: Math.min(4, theme.radius)
        color: theme.stage
        border.color: Qt.rgba(bar.ink.r, bar.ink.g, bar.ink.b, 0.15)

        Label {
            id: hoverTime
            anchors.centerIn: parent
            text: Format.time(bar.duration * Math.max(0, Math.min(1, mouse.mouseX / bar.width)))
            color: theme.stageForeground
            font.family: "monospace"
            font.pixelSize: 12
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        enabled: bar.usable
        hoverEnabled: true
        preventStealing: true
        cursorShape: Qt.PointingHandCursor

        function valueAt(x) {
            return bar.duration * Math.max(0, Math.min(1, x / width));
        }

        onPressed: (event) => {
            bar.target = valueAt(event.x);
            bar.holding = true;
            player.seekTo(bar.target, false);
        }
        onPositionChanged: (event) => {
            if (!pressed)
                return;
            bar.target = valueAt(event.x);
            if (!scrub.running)
                scrub.start();
        }
        onReleased: (event) => {
            bar.target = valueAt(event.x);
            scrub.stop();
            player.seekTo(bar.target, true);
            settle.restart();
        }
        onWheel: (wheel) => actions.trigger(wheel.angleDelta.y > 0 ? "seek-forward" : "seek-back")
    }

    // Keyframe seeks while dragging, at most every 60 ms.
    Timer {
        id: scrub
        interval: 60
        onTriggered: player.seekTo(bar.target, false)
    }

    Timer {
        id: settle
        interval: 800
        onTriggered: bar.holding = false
    }

    Connections {
        target: player
        function onPositionChanged() {
            if (bar.holding && !mouse.pressed && Math.abs(player.position - bar.target) < 0.5)
                bar.holding = false;
        }
    }
}
