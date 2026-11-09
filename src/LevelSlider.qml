import QtQuick
import QtQuick.Controls

// A value between a minimum and a maximum, with its readout beside it.
Item {
    id: slider

    property real value: 0
    property real minimum: 0
    property real maximum: 1
    property real step: 0.05
    property string readout
    readonly property color ink: theme.foreground
    readonly property real fraction: (Math.max(minimum, Math.min(maximum, value)) - minimum) / (maximum - minimum)

    signal moved(real next)

    implicitWidth: 240
    implicitHeight: 22

    Item {
        id: lane
        anchors.left: parent.left
        anchors.right: readoutText.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        height: parent.height

        Rectangle {
            id: track
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            height: 4
            radius: 2
            color: Qt.rgba(slider.ink.r, slider.ink.g, slider.ink.b, 0.16)

            Rectangle {
                width: parent.width * slider.fraction
                height: parent.height
                radius: parent.radius
                color: theme.accent
            }
        }

        Rectangle {
            width: 14
            height: 14
            radius: 7
            x: track.width * slider.fraction - width / 2
            anchors.verticalCenter: parent.verticalCenter
            color: slider.ink
            border.width: 2
            border.color: theme.background
        }

        MouseArea {
            anchors.fill: parent
            preventStealing: true
            cursorShape: Qt.PointingHandCursor

            function apply(x) {
                var raw = slider.minimum + (slider.maximum - slider.minimum) * Math.max(0, Math.min(1, x / width));
                slider.moved(Math.round(raw / slider.step) * slider.step);
            }

            onPressed: (mouse) => apply(mouse.x)
            onPositionChanged: (mouse) => { if (pressed) apply(mouse.x) }
        }
    }

    Label {
        id: readoutText
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: 48
        horizontalAlignment: Text.AlignRight
        text: slider.readout
        color: Qt.rgba(slider.ink.r, slider.ink.g, slider.ink.b, 0.6)
        font.pixelSize: 12
    }
}
