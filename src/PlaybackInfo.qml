import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "Format.js" as Format

// The Tab panel: what's playing and how, refreshed twice a second.
TitledBox {
    id: info

    property var rows: []

    title: qsTr("Playback")
    implicitWidth: grid.implicitWidth + 36
    implicitHeight: grid.implicitHeight + titleHeight / 2 + 34

    readonly property color ink: theme.stageForeground

    function number(value, digits) {
        return value === undefined || value === null ? "" : Number(value).toLocaleString(Qt.locale(), 'f', digits);
    }

    function join(parts) {
        return parts.filter(function (part) { return part !== undefined && part !== null && part !== ""; }).join("  ·  ");
    }

    function refresh() {
        var width = player.get("width"), height = player.get("height");
        var fps = player.get("container-fps");
        var hwdec = player.get("hwdec-current");
        var channels = player.get("audio-params/channel-count");
        var rate = player.get("audio-params/samplerate");
        var cache = player.get("demuxer-cache-duration");
        var dropped = (player.get("frame-drop-count") || 0) + (player.get("decoder-frame-drop-count") || 0);
        var list = [
            qsTr("file"), player.path.indexOf("://") > 0 ? player.title : Format.fileName(player.path),
            qsTr("video"), join([player.get("video-format"), width ? width + "×" + height : "", fps ? qsTr("%1 fps").arg(number(fps, 3)) : "", player.get("video-params/pixelformat")]),
            qsTr("decoder"), hwdec && hwdec !== "no" ? qsTr("hardware · %1").arg(hwdec) : qsTr("software"),
            qsTr("audio"), join([player.get("audio-codec-name"), channels ? qsTr("%1 ch").arg(channels) : "", rate ? qsTr("%1 kHz").arg(number(rate / 1000, 1)) : ""]),
            qsTr("dropped"), qsTr("%1 frames").arg(dropped),
            qsTr("cache"), cache !== undefined && cache !== null ? qsTr("%1 s").arg(number(cache, 1)) : "—",
        ];
        rows = list;
    }

    Timer {
        interval: 500
        repeat: true
        running: info.visible
        triggeredOnStart: true
        onTriggered: info.refresh()
    }

    GridLayout {
        id: grid
        x: 18
        y: info.titleHeight / 2 + 17
        columns: 2
        columnSpacing: 18
        rowSpacing: 4

        Repeater {
            model: info.rows
            delegate: Label {
                required property int index
                required property var modelData
                text: modelData
                color: index % 2 === 0 ? theme.stageMuted : theme.stageForeground
                font.pixelSize: 13
                font.bold: index === 1 // the file name
                elide: Text.ElideMiddle
                Layout.maximumWidth: index % 2 === 0 ? 120 : 460
            }
        }
    }
}
