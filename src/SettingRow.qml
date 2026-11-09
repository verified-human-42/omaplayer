import QtQuick
import QtQuick.Controls

// One setting: its name and what it does on the left, the control on the
// right. A setting changed since the page opened is marked, with a way back.
Item {
    id: row

    property var entry: ({})
    property bool current: false
    readonly property string key: entry.key || ""
    readonly property var value: key !== "" ? settings.values[key] : undefined
    readonly property bool changed: key !== "" && settings.changedKeys.indexOf(key) >= 0
    readonly property bool interactive: ["switch", "choice", "level", "text"].indexOf(entry.type) >= 0
    readonly property color ink: theme.foreground

    signal pointed()

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
        }
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
                    onClicked: settings.undo(row.key)
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
