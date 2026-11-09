import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Every shortcut in the current keymap, grouped as in the menu, plus the mouse.
Popup {
    id: sheet

    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(parent ? parent.width - 48 : 900, 920)
    height: Math.min(parent ? parent.height - 48 : 700, body.implicitHeight + topPadding + bottomPadding)
    topPadding: 30
    bottomPadding: 20
    leftPadding: 24
    rightPadding: 24

    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.45)
    }

    background: TitledBox {
        title: qsTr("Shortcuts · %1").arg(actions.keymap === "mpv" ? qsTr("mpv layout") : qsTr("Omaplayer layout"))
    }

    contentItem: Flickable {
        clip: true
        contentHeight: body.implicitHeight
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: body
            width: parent.width
            spacing: 18

            GridLayout {
                id: groups
                Layout.fillWidth: true
                columns: sheet.width > 700 ? 2 : 1
                columnSpacing: 36
                rowSpacing: 18

                Repeater {
                    model: actions.groups
                    delegate: ColumnLayout {
                        id: group
                        required property var modelData
                        readonly property var entries: actions.list.filter(function (entry) {
                            return entry.group === group.modelData.id && entry.keyLabels.length > 0;
                        })
                        visible: entries.length > 0
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        spacing: 6

                        Label {
                            text: group.modelData.text
                            color: theme.accent
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            font.capitalization: Font.AllUppercase
                            font.letterSpacing: 1.4
                        }

                        Repeater {
                            model: group.entries
                            delegate: RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                spacing: 6

                                Label {
                                    Layout.fillWidth: true
                                    text: modelData.text
                                    color: theme.foreground
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                }
                                Repeater {
                                    model: modelData.keyLabels
                                    delegate: KeyCap {
                                        required property var modelData
                                        text: modelData
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: theme.selection
            }

            Flow {
                Layout.fillWidth: true
                spacing: 22

                Repeater {
                    model: [
                        [qsTr("Click"), qsTr("Play / Pause")],
                        [qsTr("Double-click"), qsTr("Fullscreen")],
                        [qsTr("Wheel"), qsTr("Volume")],
                        [qsTr("Right-click"), qsTr("Menu")],
                        [qsTr("Drag"), qsTr("Move window")],
                        [qsTr("Click the time"), qsTr("Remaining time")],
                    ]
                    delegate: Row {
                        required property var modelData
                        spacing: 8

                        KeyCap {
                            text: modelData[0]
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData[1]
                            color: theme.muted
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }
    }
}
