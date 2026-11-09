import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl

// Corner messages, drawn like Omarchy's own OSD: a card with an icon, the
// message in bold, and a flat bar for levels. It sits top left, where
// Windows players put it.
Item {
    id: osd

    // What the message is about ("volume", "file", …), so a newer one of
    // another kind isn't replaced by the hover message.
    property string kind
    readonly property bool showing: card.opacity > 0
    property string icon
    property string label
    property string value
    property real meter: -1
    // Long values (file names) elide to fit the window.
    property real maxWidth: 100000
    readonly property color ink: theme.stageForeground
    readonly property int pad: 16
    readonly property int gap: 11

    // message: { icon, label, value, meter (0–1, or none), duration (ms) }
    function show(message) {
        osd.icon = message.icon || "";
        osd.label = message.label || "";
        osd.value = message.value || "";
        osd.meter = message.meter === undefined || message.meter < 0 ? -1 : Math.min(1, message.meter);
        hideTimer.interval = message.duration || 1600;
        card.opacity = 1;
        hideTimer.restart();
    }

    function hide() {
        hideTimer.stop();
        card.opacity = 0;
    }

    implicitWidth: card.width
    implicitHeight: card.height

    Card {
        id: card
        width: content.width + 2 * (osd.pad + border.width)
        height: 24 + 2 * (osd.pad + border.width)
        fill: 0.5
        opacity: 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation { duration: 140 }
        }

        Row {
            id: content
            x: osd.pad + card.border.width
            anchors.verticalCenter: parent.verticalCenter
            spacing: osd.gap

            IconImage {
                visible: osd.icon !== ""
                anchors.verticalCenter: parent.verticalCenter
                source: osd.icon !== "" ? "qrc:/icons/" + osd.icon + ".svg" : ""
                sourceSize: Qt.size(22, 22)
                color: osd.ink
            }

            Label {
                id: labelText
                visible: osd.label !== ""
                anchors.verticalCenter: parent.verticalCenter
                text: osd.label
                color: theme.stageMuted
                font.pixelSize: 14
            }

            Rectangle {
                visible: osd.meter >= 0
                anchors.verticalCenter: parent.verticalCenter
                width: 142
                height: 6
                color: Qt.rgba(osd.ink.r, osd.ink.g, osd.ink.b, 0.45)

                Rectangle {
                    height: parent.height
                    width: parent.width * Math.max(0, osd.meter)
                    color: theme.accent

                    Behavior on width {
                        enabled: card.opacity === 1
                        NumberAnimation { duration: 140; easing.type: Easing.OutCubic }
                    }
                }
            }

            TextMetrics {
                id: valueMetrics
                font: valueText.font
                text: osd.value
            }
            Label {
                id: valueText
                visible: osd.value !== ""
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(Math.ceil(valueMetrics.advanceWidth) + 2,
                                osd.maxWidth - 2 * (osd.pad + card.border.width)
                                - (osd.icon !== "" ? 22 + osd.gap : 0)
                                - (labelText.visible ? labelText.width + osd.gap : 0)
                                - (osd.meter >= 0 ? 142 + osd.gap : 0))
                elide: Text.ElideMiddle
                text: osd.value
                color: osd.ink
                font.pixelSize: 14
                font.bold: true
            }
        }
    }

    Timer {
        id: hideTimer
        interval: 1600
        onTriggered: card.opacity = 0
    }
}
