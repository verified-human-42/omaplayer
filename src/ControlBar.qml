import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "Format.js" as Format

// The bar: the timeline across the top, transport on the left, the
// clock beside it, and sound, screen, and the menu on the right. Docked under
// the video in a window; floating over it in fullscreen.
Item {
    id: bar

    property bool floating: false
    property bool fullscreen: false
    property bool playlistOpen: false
    readonly property bool showRemaining: settings.values["interface/remaining"] === true
    readonly property bool hovered: hover.hovered
    readonly property color ink: theme.stageForeground

    signal menuRequested(Item source)

    implicitHeight: 64

    Material.theme: Material.Dark
    Material.foreground: theme.stageForeground
    Material.accent: theme.accent

    HoverHandler {
        id: hover
    }

    // Swallows clicks so they don't reach the video underneath.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: (wheel) => wheel.accepted = true
    }

    Rectangle {
        anchors.fill: parent
        color: theme.stage
        opacity: bar.floating ? 0.9 : 1
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Qt.rgba(bar.ink.r, bar.ink.g, bar.ink.b, 0.08)
    }

    SeekBar {
        id: seekBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.topMargin: 4
    }

    RowLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 8
        anchors.rightMargin: 8
        anchors.bottomMargin: 4
        height: 36
        spacing: 2

        BarButton {
            iconName: player.paused || player.idle ? "play" : "pause"
            actionId: "play-pause"
        }
        BarButton {
            iconName: "stop"
            actionId: "stop"
        }
        BarButton {
            iconName: "previous"
            actionId: "previous-file"
        }
        BarButton {
            iconName: "next"
            actionId: "next-file"
        }
        BarButton {
            iconName: "open"
            actionId: "open-file"
        }

        // Elapsed and total; a click swaps elapsed for remaining.
        Row {
            Layout.leftMargin: 12
            spacing: 0

            Label {
                text: bar.showRemaining && player.duration > 0
                      ? "-" + Format.time(player.duration - player.position)
                      : Format.time(player.position)
                color: player.idle ? theme.stageMuted : theme.stageForeground
                font.pixelSize: 13
                font.features: { "tnum": 1 }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: settings.set("interface/remaining", !bar.showRemaining)
                }
            }
            Label {
                text: " / " + Format.time(player.duration)
                color: theme.stageMuted
                font.pixelSize: 13
                font.features: { "tnum": 1 }
            }
        }

        // Only what's off its default shows here, and a click puts it back.
        Repeater {
            model: [
                { shown: Math.abs(player.speed - 1) > 0.001, text: Number(player.speed).toLocaleString(Qt.locale(), 'f', 2) + "×", action: "speed-reset" },
                { shown: Math.abs(player.subDelay) > 0.001, text: qsTr("sub %1 s").arg((player.subDelay > 0 ? "+" : "") + Number(player.subDelay).toLocaleString(Qt.locale(), 'f', 1)), action: "" },
            ]
            delegate: Rectangle {
                required property var modelData
                visible: modelData.shown
                Layout.leftMargin: 8
                implicitWidth: chipText.implicitWidth + 12
                implicitHeight: 20
                radius: Math.min(3, theme.radius)
                color: "transparent"
                border.width: 1
                border.color: theme.accent

                Label {
                    id: chipText
                    anchors.centerIn: parent
                    text: modelData.text
                    color: theme.accent
                    font.pixelSize: 11
                }
                MouseArea {
                    anchors.fill: parent
                    enabled: modelData.action !== ""
                    cursorShape: Qt.PointingHandCursor
                    onClicked: actions.trigger(modelData.action)
                }
            }
        }

        Item {
            Layout.fillWidth: true
        }

        BarButton {
            iconName: player.muted ? "volume-muted" : "volume"
            actionId: "mute"
            lit: player.muted
        }
        VolumeBar {
            Layout.preferredWidth: 88
        }
        Label {
            Layout.preferredWidth: 30
            Layout.leftMargin: 6
            text: Math.round(player.volume)
            color: theme.stageMuted
            font.pixelSize: 12
            font.features: { "tnum": 1 }
        }
        BarButton {
            iconName: "playlist"
            actionId: "playlist"
            lit: bar.playlistOpen
        }
        BarButton {
            iconName: bar.fullscreen ? "fullscreen-exit" : "fullscreen"
            actionId: "fullscreen"
        }
        BarButton {
            id: menuButton
            iconName: "menu"
            tip: qsTr("Menu")
            onClicked: bar.menuRequested(menuButton)
        }
    }
}
