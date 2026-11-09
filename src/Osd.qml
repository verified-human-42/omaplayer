import QtQuick
import QtQuick.Controls
import QtQuick.Effects

// Corner messages the way Windows players draw them: bare text on the
// picture, outlined and shadowed so it reads over anything, top left. It
// grows with the window, so fullscreen on a big screen reads from the couch.
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
    // The stage colours: light text on the palette's darkest colour, in
    // light themes too.
    readonly property color ink: theme.stageForeground
    readonly property color edge: theme.stage
    readonly property int size: Math.round(Math.max(16, Math.min(36, (Window.window ? Window.window.height : 720) * 0.025)))

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
            spacing: Math.round(osd.size * 0.4)

            Label {
                id: labelText
                visible: osd.label !== ""
                text: osd.label
                color: osd.ink
                style: Text.Outline
                styleColor: osd.edge
                font.pixelSize: osd.size
            }

            TextMetrics {
                id: valueMetrics
                font: valueText.font
                text: osd.value
            }
            Label {
                id: valueText
                visible: osd.value !== ""
                width: Math.min(Math.ceil(valueMetrics.advanceWidth) + 2,
                                osd.maxWidth - (labelText.visible ? labelText.width + row.spacing : 0))
                elide: Text.ElideMiddle
                text: osd.value
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
