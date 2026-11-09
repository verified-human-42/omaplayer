import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import "Format.js" as Format

// The Tab panel: what's playing and how, refreshed twice a second, set as
// bare outlined text on the picture like the corner messages.
Item {
    id: info

    property var rows: []
    readonly property color ink: theme.stageForeground
    readonly property color edge: theme.stage
    readonly property int size: Math.round(Math.max(14, Math.min(30, (Window.window ? Window.window.height : 720) * 0.021)))

    implicitWidth: grid.implicitWidth
    implicitHeight: grid.implicitHeight
    layer.enabled: true
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowColor: info.edge
        shadowBlur: 0.45
        shadowOpacity: 0.95
        shadowVerticalOffset: 1
    }

    function number(value, digits) {
        return value === undefined || value === null ? "" : Number(value).toLocaleString(Qt.locale(), 'f', digits);
    }

    // A value in one of the palette's colours.
    function tint(value, color) {
        if (value === undefined || value === null || value === "")
            return "";
        var text = String(value).replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
        return "<font color=\"" + color + "\">" + text + "</font>";
    }

    function line(label, parts) {
        var shown = parts.filter(function (part) { return part !== undefined && part !== null && part !== ""; });
        return "<b>" + label + ":</b> " + (shown.length > 0 ? shown.join("  ·  ") : "—");
    }

    function kbps(bits) {
        return bits > 0 ? qsTr("%1 kbps").arg(Math.round(bits / 1000)) : "";
    }

    // Like the Windows players' Tab panel: the file first, then where and
    // how it plays, each figure in a colour of its own.
    function refresh() {
        var width = player.get("width"), height = player.get("height");
        var fps = player.get("container-fps");
        var hwdec = player.get("hwdec-current");
        var channels = player.get("audio-params/channel-count");
        var rate = player.get("audio-params/samplerate");
        var cache = player.get("demuxer-cache-duration");
        var dropped = (player.get("frame-drop-count") || 0) + (player.get("decoder-frame-drop-count") || 0);
        var frame = player.get("estimated-frame-number"), frames = player.get("estimated-frame-count");
        var display = player.get("display-fps");
        var subtitle = qsTr("Off");
        player.tracks.forEach(function (track) {
            if (track.type === "sub" && track.selected && track["main-selection"] !== 1)
                subtitle = Format.trackLabel(track, qsTr("Track %1").arg(track.id));
        });
        var percent = player.duration > 0 ? " (" + number(100 * player.position / player.duration, 1) + "%)" : "";
        var c = theme;
        rows = [
            line(qsTr("File"), [tint(player.path.indexOf("://") > 0 ? player.title : Format.fileName(player.path), c.accent)]),
            line(qsTr("Time"), [tint(Format.time(player.position) + " / " + Format.time(player.duration) + percent, c.cyan),
                                frames > 0 ? qsTr("frame %1 of %2").arg(tint(frame, c.green)).arg(tint(frames, c.green)) : "",
                                tint(new Date().toLocaleTimeString(Qt.locale(), Locale.ShortFormat), c.green)]),
            line(qsTr("Video"), [tint(player.get("video-format") || "", c.magenta),
                                 width ? tint(width + "×" + height, c.blue) : "",
                                 fps ? tint(qsTr("%1 fps").arg(number(fps, 3)), c.orange) : "",
                                 tint(kbps(player.get("video-bitrate")), c.orange),
                                 tint(player.get("video-params/pixelformat") || "", c.blue)]),
            line(qsTr("Decoder"), [hwdec && hwdec !== "no" ? tint(qsTr("hardware · %1").arg(hwdec), c.green) : tint(qsTr("software"), c.orange)]),
            line(qsTr("Audio"), [tint(player.get("audio-codec-name") || "", c.magenta),
                                 channels ? tint(qsTr("%1 ch").arg(channels), c.blue) : "",
                                 rate ? tint(qsTr("%1 kHz").arg(number(rate / 1000, 1)), c.blue) : "",
                                 tint(kbps(player.get("audio-bitrate")), c.orange)]),
            line(qsTr("Subtitles"), [tint(subtitle, c.cyan)]),
            line(qsTr("Display"), [display ? tint(qsTr("%1 Hz").arg(number(display, 0)), c.green) : "",
                                   qsTr("%1 dropped").arg(tint(dropped, dropped > 0 ? theme.error : c.green)),
                                   cache !== undefined && cache !== null ? qsTr("%1 s cached").arg(tint(number(cache, 1), c.cyan)) : ""]),
        ];
    }

    Timer {
        interval: 500
        repeat: true
        running: info.visible
        triggeredOnStart: true
        onTriggered: info.refresh()
    }

    Column {
        id: grid
        spacing: Math.round(info.size * 0.2)

        Repeater {
            model: info.rows
            delegate: Label {
                required property var modelData
                text: modelData
                textFormat: Text.StyledText
                color: info.ink
                style: Text.Outline
                styleColor: info.edge
                font.pixelSize: info.size
                width: Math.min(implicitWidth, (Window.window ? Window.window.width : 1200) - 40)
                elide: Text.ElideRight
            }
        }
    }
}
