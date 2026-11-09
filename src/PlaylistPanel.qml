import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import "Format.js" as Format

// The playlist, docked beside the picture: what's queued, what's playing,
// and the buttons that reorder the evening.
Rectangle {
    id: panel

    readonly property color ink: theme.stageForeground

    signal closeRequested()

    function shade(alpha) {
        return Qt.rgba(ink.r, ink.g, ink.b, alpha);
    }

    function name(entry) {
        if (entry.title)
            return entry.title;
        var file = String(entry.filename || "");
        return file.indexOf("://") > 0 ? file : Format.fileName(file);
    }

    color: theme.stage

    // Swallows clicks and the wheel so they don't reach the picture.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.AllButtons
        onWheel: (wheel) => wheel.accepted = true
    }

    Rectangle {
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 1
        color: panel.shade(0.08)
    }

    Item {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 44

        Label {
            id: heading
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Playlist")
            color: theme.stageMuted
            font.pixelSize: 10
            font.weight: Font.DemiBold
            font.capitalization: Font.AllUppercase
            font.letterSpacing: 1.4
        }
        Label {
            anchors.left: heading.right
            anchors.leftMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            visible: player.playlistCount > 0
            text: player.playlistPosition >= 0 ? (player.playlistPosition + 1) + " / " + player.playlistCount
                                               : player.playlistCount
            color: theme.stageForeground
            font.pixelSize: 11
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0

            BarButton {
                iconName: "add"
                tip: qsTr("Add Files…")
                implicitWidth: 30
                implicitHeight: 30
                icon.width: 16
                icon.height: 16
                onClicked: picker.openMedia(true)
            }
            BarButton {
                iconName: "shuffle"
                actionId: "shuffle"
                implicitWidth: 30
                implicitHeight: 30
                icon.width: 16
                icon.height: 16
            }
            BarButton {
                iconName: "repeat"
                actionId: "repeat"
                lit: player.repeat
                implicitWidth: 30
                implicitHeight: 30
                icon.width: 16
                icon.height: 16
            }
            BarButton {
                iconName: "close"
                tip: qsTr("Close")
                implicitWidth: 30
                implicitHeight: 30
                icon.width: 16
                icon.height: 16
                onClicked: panel.closeRequested()
            }
        }
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        height: 1
        color: panel.shade(0.08)
    }

    ListView {
        id: list
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: parent.bottom
        anchors.topMargin: 6
        anchors.leftMargin: 1
        clip: true
        model: player.playlist
        boundsBehavior: Flickable.StopAtBounds
        currentIndex: player.playlistPosition

        ScrollBar.vertical: ScrollBar {}

        onCountChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
        onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)

        delegate: Rectangle {
            id: entry

            required property var modelData
            required property int index
            readonly property bool playing: index === player.playlistPosition

            width: list.width
            height: 34
            color: rowHover.hovered ? panel.shade(0.07) : "transparent"

            Rectangle {
                visible: entry.playing
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: 6
                anchors.bottomMargin: 6
                width: 2
                color: theme.accent
            }

            Label {
                id: number
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                width: 26
                text: entry.index + 1
                color: entry.playing ? theme.accent : theme.stageMuted
                font.pixelSize: 11
                horizontalAlignment: Text.AlignRight
            }

            Label {
                anchors.left: number.right
                anchors.leftMargin: 12
                anchors.right: remove.left
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                text: panel.name(entry.modelData)
                elide: Text.ElideMiddle
                color: entry.playing ? theme.stageForeground : panel.shade(0.75)
                font.pixelSize: 12
                font.weight: entry.playing ? Font.DemiBold : Font.Normal
            }

            IconImage {
                id: remove
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                visible: rowHover.hovered
                source: "qrc:/icons/close.svg"
                sourceSize: Qt.size(13, 13)
                color: removeHover.hovered ? theme.stageForeground : theme.stageMuted

                HoverHandler {
                    id: removeHover
                    cursorShape: Qt.PointingHandCursor
                }
                TapHandler {
                    onTapped: player.removeIndex(entry.index)
                }
            }

            HoverHandler {
                id: rowHover
            }
            TapHandler {
                onDoubleTapped: player.playIndex(entry.index)
                onSingleTapped: if (!entry.playing) player.playIndex(entry.index)
            }
        }

        Label {
            anchors.centerIn: parent
            visible: list.count === 0
            width: parent.width - 48
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: qsTr("Nothing queued. Drop files here, or add them with +.")
            color: theme.stageMuted
            font.pixelSize: 12
        }
    }
}
