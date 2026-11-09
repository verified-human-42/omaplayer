import QtQuick
import QtQuick.Controls
import QtQuick.Effects

// Corner messages the way Windows players draw them: one bold line on the
// picture, top left, in the accent, outlined and shadowed so it reads over
// anything. It grows with the window, so fullscreen reads from the couch.
// Not customizable, on purpose.
Item {
    id: osd

    // What the message is about ("volume", "file", …), so a newer one of
    // another kind isn't replaced by the hover message.
    property string kind
    readonly property bool showing: line.opacity > 0
    property string label
    property string value
    // Long values (file names) elide to fit the window.
    property real maxWidth: 100000
    // The accent, edged in the palette's darkest colour (light themes too).
    readonly property color ink: theme.accent
    readonly property color edge: theme.stage
    readonly property int size: Math.round(Math.max(15, Math.min(32, (Window.window ? Window.window.height : 720) * 0.022)))

    // message: { label, value, duration (ms) }
    function show(message) {
        osd.label = message.label || "";
        osd.value = message.value || "";
        hideTimer.interval = message.duration || 1600;
        line.opacity = 1;
        hideTimer.restart();
    }

    function hide() {
        hideTimer.stop();
        line.opacity = 0;
    }

    implicitWidth: line.width
    implicitHeight: line.height

    Item {
        id: line
        width: row.width
        height: row.height
        opacity: 0
        visible: opacity > 0
        layer.enabled: visible
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: osd.edge
            shadowBlur: 0.45
            shadowOpacity: 0.95
            shadowHorizontalOffset: 0
            shadowVerticalOffset: 1
        }

        Behavior on opacity {
            NumberAnimation { duration: 140 }
        }

        Row {
            id: row

            TextMetrics {
                id: metrics
                font: message.font
                text: message.text
            }
            // One bold line, "Volume: 80%", in the theme's accent, the way
            // Windows players set theirs in their skin's colour.
            Label {
                id: message
                width: Math.min(Math.ceil(metrics.advanceWidth) + 2, osd.maxWidth)
                elide: Text.ElideMiddle
                text: osd.label !== "" && osd.value !== "" ? osd.label + ": " + osd.value : osd.label + osd.value
                color: osd.ink
                style: Text.Outline
                styleColor: osd.edge
                font.pixelSize: osd.size
                font.bold: true
            }
        }
    }

    Timer {
        id: hideTimer
        interval: 1600
        onTriggered: line.opacity = 0
    }
}
