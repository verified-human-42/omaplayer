import QtQuick
import QtQuick.Controls

// Corner messages, set like a status line: an accent rule, the
// subject in small capitals, the value, and a segmented meter for levels.
Item {
    id: osd

    property string label
    property string value
    property real meter: -1
    readonly property color ink: theme.stageForeground
    readonly property color base: theme.stage

    function show(label, value, meter) {
        osd.label = label;
        osd.value = value || "";
        osd.meter = meter === undefined ? -1 : meter;
        panel.opacity = 1;
        hideTimer.restart();
    }

    function hide() {
        hideTimer.stop();
        panel.opacity = 0;
    }

    implicitWidth: panel.width
    implicitHeight: panel.height

    Rectangle {
        id: panel
        width: content.implicitWidth + 30
        height: content.implicitHeight + 18
        opacity: 0
        visible: opacity > 0
        radius: Math.min(4, theme.radius)
        color: Qt.rgba(osd.base.r, osd.base.g, osd.base.b, 0.88)
        border.width: 1
        border.color: Qt.rgba(osd.ink.r, osd.ink.g, osd.ink.b, 0.08)

        Behavior on opacity {
            NumberAnimation { duration: 140 }
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 1
            width: 3
            color: theme.accent
        }

        Row {
            id: content
            anchors.verticalCenter: parent.verticalCenter
            x: 18
            spacing: 14

            Label {
                anchors.baseline: valueText.baseline
                text: osd.label
                color: osd.value === "" ? theme.stageForeground : theme.stageMuted
                font.pixelSize: osd.value === "" ? 18 : 11
                font.capitalization: Font.AllUppercase
                font.letterSpacing: osd.value === "" ? 1.5 : 1.2
                font.weight: Font.DemiBold
            }

            Row {
                visible: osd.meter >= 0
                anchors.verticalCenter: valueText.verticalCenter
                spacing: 2

                Repeater {
                    model: 20
                    delegate: Rectangle {
                        required property int index
                        width: 5
                        height: 12
                        radius: Math.min(1, theme.radius)
                        color: (index + 0.5) / 20 <= osd.meter ? theme.accent
                             : Qt.rgba(osd.ink.r, osd.ink.g, osd.ink.b, 0.16)
                    }
                }
            }

            Label {
                id: valueText
                visible: osd.value !== ""
                text: osd.value
                color: theme.stageForeground
                font.pixelSize: 18
                font.weight: Font.DemiBold
            }
        }
    }

    Timer {
        id: hideTimer
        interval: 1600
        onTriggered: panel.opacity = 0
    }
}
