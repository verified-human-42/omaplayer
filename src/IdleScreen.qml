import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The empty window is the cheat sheet: the mark, a terminal cursor, and every
// shortcut in the current layout, grouped as in the menu.
Rectangle {
    id: idle

    readonly property color ink: theme.stageForeground
    readonly property int columns: width > 1120 ? 3 : width > 760 ? 2 : 1
    // One blink drives both cursors, the mark's and the name's, like a terminal.
    property bool cursorOn: true

    Timer {
        interval: 530
        repeat: true
        running: idle.visible
        onTriggered: idle.cursorOn = !idle.cursorOn
    }

    color: theme.stage

    Flickable {
        anchors.fill: parent
        contentHeight: Math.max(height, page.implicitHeight + 64)
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: page
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.max(32, (parent.height - implicitHeight) / 2)
            width: Math.min(idle.width - 64, idle.columns * 330 + (idle.columns - 1) * 40)
            spacing: 0

            Row {
                Layout.alignment: Qt.AlignHCenter
                spacing: 14

                LogoMark {
                    anchors.verticalCenter: parent.verticalCenter
                    size: 52
                    cursorOn: idle.cursorOn
                    ink: idle.ink
                }

                Row {
                    anchors.verticalCenter: parent.verticalCenter

                    Label {
                        text: "OmaPlayer"
                        color: idle.ink
                        font.pixelSize: 24
                        font.weight: Font.DemiBold
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 5
                        width: 11
                        height: 20
                        color: theme.accent
                        opacity: idle.cursorOn ? 1 : 0
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                Layout.topMargin: 40
                columns: idle.columns
                columnSpacing: 40
                rowSpacing: 26

                Repeater {
                    model: actions.groups
                    delegate: ColumnLayout {
                        id: group

                        required property var modelData
                        readonly property var entries: actions.list.filter(function (entry) {
                            return entry.group === group.modelData.id && entry.keyLabels.length > 0 && entry.everyday;
                        })

                        visible: entries.length > 0
                        Layout.fillWidth: true
                        Layout.preferredWidth: 330
                        Layout.alignment: Qt.AlignTop
                        spacing: 7

                        Label {
                            text: group.modelData.text
                            color: theme.stageMuted
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            font.capitalization: Font.AllUppercase
                            font.letterSpacing: 1.4
                            bottomPadding: 2
                        }

                        Repeater {
                            model: group.entries
                            delegate: RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: 4

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.text
                                    color: Qt.rgba(idle.ink.r, idle.ink.g, idle.ink.b, 0.86)
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                                Repeater {
                                    model: modelData.keyLabels
                                    delegate: KeyCap {
                                        required property var modelData
                                        text: modelData
                                        ink: idle.ink
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: 36
                text: qsTr("Drop files anywhere to play them")
                color: theme.stageMuted
                font.pixelSize: 11
            }
        }
    }
}
