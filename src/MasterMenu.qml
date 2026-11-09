import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// The right-click master menu: every action, grouped, with its key.
// Built from the action table, so it never falls behind the keymap. The
// app's own entries (Settings, Keyboard Shortcuts) sit at the top level,
// under a divider, instead of in a submenu.
Menu {
    id: master

    topPadding: 6
    bottomPadding: 6
    delegate: MenuEntry {}

    // The aspect chosen from the menu ("no" is the file's own).
    property string aspect: "no"
    function tracks(type) {
        return player.tracks.filter(function (track) { return track.type === type; }).map(function (track) {
            var name = [track.title, track.lang].filter(function (part) { return part; }).join(" · ");
            return { id: track.id, label: name || qsTr("Track %1").arg(track.id) };
        });
    }

    function selected(property) {
        player.tracks; // re-read when the tracks change
        var id = player.get(property);
        return id === false || id === undefined ? "no" : id;
    }


    background: Rectangle {
        implicitWidth: 240
        color: theme.background
        radius: theme.radius
        border.width: 2
        border.color: theme.accent
    }

    Instantiator {
        model: actions.groups.filter(function (group) { return group.id !== "help"; })
        delegate: Menu {
            id: group

            required property var modelData

            title: modelData.text
            // Track lists go first, as in the Windows players; the
            // group's actions follow them.
            readonly property int listsOnTop: modelData.id === "subtitles" ? 2 : modelData.id === "audio" || modelData.id === "video" ? 1 : 0
            topPadding: 6
            bottomPadding: 6
            delegate: MenuEntry {}
            background: Rectangle {
                implicitWidth: 340
                color: theme.background
                radius: theme.radius
                border.width: 2
                border.color: theme.accent
            }

            Instantiator {
                model: actions.list.filter(function (entry) { return entry.group === group.modelData.id; })
                delegate: MenuEntry {
                    required property var modelData
                    text: modelData.text
                    actionId: modelData.id
                    keyLabel: modelData.keyLabels.length > 0 ? modelData.keyLabels[0] : ""
                }
                onObjectAdded: (index, object) => group.insertItem(group.listsOnTop + index, object)
                onObjectRemoved: (index, object) => group.removeItem(object)
            }

            // Above the group's actions, the lists Windows players keep in
            // their menus: pick a track instead of cycling. The menu acts on
            // what's playing; anything that's a preference lives in Settings.
            Instantiator {
                model: group.modelData.id === "subtitles" ? [{ type: "sub", property: "sid", title: qsTr("Subtitle Track"), off: true },
                                                             { type: "sub", property: "secondary-sid", title: qsTr("Second Subtitle"), off: true }]
                     : group.modelData.id === "audio" ? [{ type: "audio", property: "aid", title: qsTr("Audio Track"), off: false }]
                     : []
                delegate: SubMenu {
                    id: trackMenu
                    required property var modelData
                    title: modelData.title
                    enabled: !player.idle

                    Instantiator {
                        model: (trackMenu.modelData.off ? [{ id: "no", label: qsTr("Off") }] : []).concat(master.tracks(trackMenu.modelData.type))
                        delegate: MenuEntry {
                            required property var modelData
                            tickable: true
                            ticked: String(master.selected(trackMenu.modelData.property)) === String(modelData.id)
                            text: modelData.label
                            onTriggered: player.command(["set", trackMenu.modelData.property, modelData.id],
                                                        trackMenu.modelData.type === "audio" ? "audio"
                                                        : trackMenu.modelData.property === "sid" ? "subtitle" : "second-subtitle")
                        }
                        onObjectAdded: (index, object) => trackMenu.insertItem(index, object)
                        onObjectRemoved: (index, object) => trackMenu.removeItem(object)
                    }
                    MenuSeparator {
                        visible: trackMenu.modelData.type === "sub"
                        height: visible ? implicitHeight : 0
                    }
                    MenuEntry {
                        visible: trackMenu.modelData.property === "sid"
                        height: visible ? implicitHeight : 0
                        text: qsTr("Open Subtitle File…")
                        onTriggered: picker.openMedia(false)
                    }
                }
                onObjectAdded: (index, object) => group.insertMenu(index, object)
                onObjectRemoved: (index, object) => group.removeMenu(object)
            }

            Instantiator {
                model: group.modelData.id === "video" ? [1] : []
                delegate: SubMenu {
                    id: aspectMenu
                    title: qsTr("Aspect Ratio")
                    enabled: !player.idle
                    Instantiator {
                        model: [{ value: "no", label: qsTr("Original") }, { value: "16:9", label: "16:9" }, { value: "4:3", label: "4:3" },
                                { value: "16:10", label: "16:10" }, { value: "1.85:1", label: "1.85:1" }, { value: "2.35:1", label: "2.35:1" }]
                        delegate: MenuEntry {
                            required property var modelData
                            tickable: true
                            ticked: master.aspect === modelData.value
                            text: modelData.label
                            onTriggered: {
                                master.aspect = modelData.value;
                                player.command(["set", "video-aspect-override", modelData.value], "aspect");
                            }
                        }
                        onObjectAdded: (index, object) => aspectMenu.insertItem(index, object)
                        onObjectRemoved: (index, object) => aspectMenu.removeItem(object)
                    }
                }
                onObjectAdded: (index, object) => group.insertMenu(index, object)
                onObjectRemoved: (index, object) => group.removeMenu(object)
            }
        }
        onObjectAdded: (index, object) => master.insertMenu(index, object)
        onObjectRemoved: (index, object) => master.removeMenu(object)
    }

    MenuSeparator {
        leftPadding: 10
        rightPadding: 10
        contentItem: Rectangle {
            implicitHeight: 1
            color: theme.selection
        }
    }

    Instantiator {
        model: actions.list.filter(function (entry) { return entry.group === "help" && entry.id !== "menu"; })
        delegate: MenuEntry {
            required property var modelData
            text: modelData.text
            actionId: modelData.id
            keyLabel: modelData.keyLabels.length > 0 ? modelData.keyLabels[0] : ""
        }
        onObjectAdded: (index, object) => master.addItem(object)
        onObjectRemoved: (index, object) => master.removeItem(object)
    }
}
