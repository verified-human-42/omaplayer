import QtQuick
import QtQuick.Controls

// One setting: its name and what it does on the left, the control on the
// right. A setting changed since the page opened is marked, with a way back.
Item {
    id: row

    property var entry: ({})
    property bool current: false
    // While a key binding listens for its new key.
    property bool capturing: false
    property string note
    readonly property string key: entry.key || ""
    readonly property bool binding: entry.type === "binding"
    readonly property bool radio: entry.type === "radio"
    readonly property var value: radio ? settings.values[entry.setting]
                               : key !== "" && !binding ? settings.values[key] : undefined
    readonly property bool changed: binding ? entry.custom === true
                                  : radio ? false
                                  : key !== "" && settings.changedKeys.indexOf(key) >= 0
    readonly property bool interactive: ["switch", "choice", "level", "text", "binding", "action", "radio"].indexOf(entry.type) >= 0
    readonly property color ink: theme.foreground

    signal pointed()
    signal captureRequested()

    implicitHeight: Math.max(labels.implicitHeight, control.implicitHeight) + 22

    // Arrow keys and h/l nudge the value; Space and Enter flip or advance it.
    function step(delta) {
        if (entry.type === "choice") {
            var options = entry.options, at = 0;
            for (var i = 0; i < options.length; ++i)
                if (String(options[i].value) === String(value))
                    at = i;
            var next = Math.max(0, Math.min(options.length - 1, at + delta));
            if (next !== at)
                settings.set(key, options[next].value);
        } else if (entry.type === "level") {
            settings.set(key, Math.max(entry.minimum, Math.min(entry.maximum, value + delta * entry.step)));
        } else if (entry.type === "switch" && (delta > 0) !== value) {
            settings.set(key, delta > 0);
        }
    }

    function activate() {
        if (entry.type === "switch") {
            settings.set(key, !value);
        } else if (entry.type === "choice") {
            var options = entry.options;
            for (var i = 0; i < options.length; ++i)
                if (String(options[i].value) === String(value)) {
                    settings.set(key, options[(i + 1) % options.length].value);
                    return;
                }
        } else if (entry.type === "text" && control.item) {
            control.item.forceActiveFocus();
        } else if (binding) {
            captureRequested();
        } else if (radio) {
            settings.set(entry.setting, entry.value);
        } else if (entry.type === "action" && entry.command === "resetAllKeys") {
            settings.resetAllKeys();
        }
    }

    function reset() {
        if (binding)
            settings.resetKeys(entry.actionId);
        else
            settings.undo(key);
    }

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        anchors.topMargin: 3
        anchors.bottomMargin: 3
        radius: Math.max(6, theme.radius)
        color: row.current && row.interactive ? Qt.rgba(row.ink.r, row.ink.g, row.ink.b, 0.07) : "transparent"
    }

    HoverHandler {
        onHoveredChanged: if (hovered && row.interactive) row.pointed()
    }

    Column {
        id: labels
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        width: parent.width * 0.5
        spacing: 3

        Row {
            spacing: 6

            Label {
                anchors.baseline: title.baseline
                visible: row.changed
                text: "●"
                color: theme.accent
                font.pixelSize: 11
            }
            Label {
                id: title
                text: row.entry.label || ""
                color: row.ink
                font.pixelSize: 13
            }
            Label {
                anchors.baseline: title.baseline
                visible: row.changed
                text: qsTr("(reset)")
                color: resetMouse.containsMouse ? theme.accent : Qt.rgba(row.ink.r, row.ink.g, row.ink.b, 0.6)
                font.pixelSize: 10
                font.underline: resetMouse.containsMouse

                MouseArea {
                    id: resetMouse
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: row.reset()
                }
            }
        }

        Label {
            visible: (row.entry.description || "") !== ""
            width: parent.width
            text: row.entry.description || ""
            color: Qt.rgba(row.ink.r, row.ink.g, row.ink.b, 0.6)
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }

        Label {
            visible: row.note !== ""
            width: parent.width
            text: row.note
            color: theme.accent
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
    }

    Loader {
        id: control
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        width: item ? Math.min(item.implicitWidth, row.width * 0.44) : 0
        sourceComponent: {
            switch (row.entry.type) {
            case "switch": return switchControl;
            case "choice": return choiceControl;
            case "level": return levelControl;
            case "text": return textControl;
            case "keys": return keysControl;
            case "binding": return bindingControl;
            case "action": return actionControl;
            case "radio": return radioControl;
            default: return factControl;
            }
        }
    }

    Component {
        id: switchControl
        ToggleSwitch {
            checked: row.value === true
            onToggled: settings.set(row.key, !row.value)
        }
    }

    Component {
        id: choiceControl
        ChoiceGroup {
            options: row.entry.options
            value: String(row.value)
            onPicked: (next) => settings.set(row.key, next)
        }
    }

    Component {
        id: levelControl
        LevelSlider {
            implicitWidth: 260
            value: row.value
            minimum: row.entry.minimum
            maximum: row.entry.maximum
            step: row.entry.step
            readout: Math.round(row.value * 100) + "%"
            onMoved: (next) => settings.set(row.key, next)
        }
    }

    Component {
        id: textControl
        SettingField {
            implicitWidth: 240
            value: String(row.value)
            placeholder: row.entry.placeholder || ""
            onCommitted: (next) => settings.set(row.key, next)
        }
    }

    Component {
        id: keysControl
        Row {
            layoutDirection: Qt.RightToLeft
            spacing: 4
            Repeater {
                model: row.entry.keys
                delegate: KeyCap {
                    required property var modelData
                    text: modelData
                }
            }
        }
    }

    // The action's keys, each removable on hover, then + to add one. While
    // listening, the keys give way to the prompt.
    Component {
        id: bindingControl
        Row {
            spacing: 4

            Label {
                visible: row.capturing
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Press a key… (Esc cancels)")
                color: theme.accent
                font.pixelSize: 11
            }

            Repeater {
                model: row.capturing ? [] : row.entry.keys
                delegate: Item {
                    id: bound

                    required property var modelData
                    required property int index

                    anchors.verticalCenter: parent ? parent.verticalCenter : undefined
                    implicitWidth: cap.implicitWidth + (boundHover.hovered ? 16 : 0)
                    implicitHeight: cap.implicitHeight

                    KeyCap {
                        id: cap
                        text: row.entry.labels ? (row.entry.labels[bound.index] || "") : ""
                    }
                    Label {
                        anchors.left: cap.right
                        anchors.leftMargin: 3
                        anchors.verticalCenter: cap.verticalCenter
                        visible: boundHover.hovered
                        text: "×"
                        color: removeHover.hovered ? theme.accent : Qt.rgba(row.ink.r, row.ink.g, row.ink.b, 0.6)
                        font.pixelSize: 13

                        HoverHandler {
                            id: removeHover
                            cursorShape: Qt.PointingHandCursor
                        }
                        TapHandler {
                            onTapped: settings.unbindKey(row.entry.actionId, bound.modelData)
                        }
                    }
                    HoverHandler {
                        id: boundHover
                    }
                }
            }

            Rectangle {
                visible: !row.capturing
                anchors.verticalCenter: parent.verticalCenter
                width: 22
                height: 18
                radius: Math.max(4, Math.min(6, theme.radius))
                color: addHover.hovered ? Qt.rgba(row.ink.r, row.ink.g, row.ink.b, 0.08) : "transparent"
                border.width: 1
                border.color: Qt.rgba(row.ink.r, row.ink.g, row.ink.b, 0.22)

                Label {
                    anchors.centerIn: parent
                    text: "+"
                    color: Qt.rgba(row.ink.r, row.ink.g, row.ink.b, 0.8)
                    font.pixelSize: 12
                }
                HoverHandler {
                    id: addHover
                    cursorShape: Qt.PointingHandCursor
                }
                TapHandler {
                    onTapped: row.captureRequested()
                }
            }
        }
    }

    Component {
        id: actionControl
        Rectangle {
            readonly property bool usable: row.entry.command !== "resetAllKeys" || settings.customBindings > 0
            implicitWidth: actionText.implicitWidth + 24
            implicitHeight: 24
            radius: theme.radius > 0 ? height / 2 : 0
            opacity: usable ? 1 : 0.4
            color: actionHover.hovered && usable ? Qt.rgba(row.ink.r, row.ink.g, row.ink.b, 0.08) : "transparent"
            border.width: 1
            border.color: Qt.rgba(row.ink.r, row.ink.g, row.ink.b, 0.35)

            Label {
                id: actionText
                anchors.centerIn: parent
                text: row.entry.button || ""
                color: row.ink
                font.pixelSize: 11
                font.weight: Font.DemiBold
            }
            HoverHandler {
                id: actionHover
                cursorShape: Qt.PointingHandCursor
            }
            TapHandler {
                onTapped: if (parent.usable) row.activate()
            }
        }
    }

    Component {
        id: radioControl
        Rectangle {
            readonly property bool on: String(row.value) === String(row.entry.value)
            implicitWidth: 18
            implicitHeight: 18
            radius: 9
            color: "transparent"
            border.width: 1
            border.color: on ? theme.accent : Qt.rgba(row.ink.r, row.ink.g, row.ink.b, 0.35)

            Rectangle {
                anchors.centerIn: parent
                width: 10
                height: 10
                radius: 5
                visible: parent.on
                color: theme.accent
            }
            TapHandler {
                onTapped: settings.set(row.entry.setting, row.entry.value)
            }
        }
    }

    Component {
        id: factControl
        Label {
            text: row.entry.value === undefined ? "" : String(row.entry.value)
            color: Qt.rgba(row.ink.r, row.ink.g, row.ink.b, 0.75)
            font.pixelSize: 12
            horizontalAlignment: Text.AlignRight
            elide: Text.ElideMiddle
        }
    }
}
