import QtQuick
import QtQuick.Controls

// Volume, 0–100: click or drag to set it, or use the wheel.
Item {
    id: bar

    readonly property real maximum: settings.values["audio/boost"] ? 150 : 100
    readonly property real fraction: Math.max(0, Math.min(1, player.volume / maximum))
    readonly property color ink: theme.stageForeground
    property real wheelDelta: 0

    implicitWidth: 96
    implicitHeight: 28

    Rectangle {
        id: track
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: 4
        radius: Math.min(2, theme.radius)
        color: Qt.rgba(bar.ink.r, bar.ink.g, bar.ink.b, 0.18)

        Rectangle {
            width: parent.width * bar.fraction
            height: parent.height
            radius: parent.radius
            color: player.muted ? theme.stageMuted : theme.accent
        }
    }

    Rectangle {
        visible: mouse.containsMouse || mouse.pressed
        width: 11
        height: 11
        radius: width / 2
        x: track.width * bar.fraction - width / 2
        anchors.verticalCenter: parent.verticalCenter
        color: player.muted ? theme.stageMuted : theme.accent
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true

        function apply(x) {
            player.set("volume", Math.round(bar.maximum * Math.max(0, Math.min(1, x / width))));
        }

        onPressed: (event) => apply(event.x)
        onPositionChanged: (event) => { if (pressed) apply(event.x) }
        onWheel: (wheel) => {
            bar.wheelDelta += wheel.angleDelta.y;
            while (Math.abs(bar.wheelDelta) >= 120) {
                actions.trigger(bar.wheelDelta > 0 ? "volume-up" : "volume-down");
                bar.wheelDelta -= bar.wheelDelta > 0 ? 120 : -120;
            }
        }
    }

    ToolTip.visible: mouse.containsMouse || mouse.pressed
    ToolTip.text: qsTr("Volume %1%").arg(Math.round(player.volume))
}
