import QtQuick
import QtQuick.Controls
import QtQuick.Controls.impl
import QtQuick.Layouts

// Settings, laid out the way OmaControl lays out Omarchy's: a sidebar with
// search, a page of framed groups, a strip of what changed above and the keys
// below. Everything is reachable from the keyboard.
Popup {
    id: page

    property int pageIndex: 0
    property int rowIndex: -1
    property string query: ""
    // The action whose key binding is listening for a key, and a line for
    // the row it last changed ("Taken from Mute").
    property string capturing: ""
    property string noteFor: ""
    property string noteText: ""

    function startCapture(id) {
        capturing = id;
        noteFor = "";
        body.forceActiveFocus();
    }

    function showNote(id, text) {
        noteFor = text !== "" ? id : "";
        noteText = text;
        noteTimer.restart();
    }

    Timer {
        id: noteTimer
        interval: 4000
        onTriggered: page.noteFor = ""
    }
    readonly property color ink: theme.foreground

    function shade(alpha) {
        return Qt.rgba(ink.r, ink.g, ink.b, alpha);
    }

    // A key for an action in the layout in effect, for descriptions that
    // name keys, or "" when the layout gives it none.
    function keyFor(id) {
        actions.list;
        return actions.shortcut(id);
    }

    function seekKeys(back, forward) {
        var a = keyFor(back), b = keyFor(forward);
        return a !== "" && b !== "" ? qsTr("%1 and %2.").arg(a).arg(b)
                                    : qsTr("No key in this layout. Give it one on the Keyboard page.");
    }

    readonly property var pages: {
        actions.list; // re-read after a language or keymap change
        return [
            {
                id: "playback", section: qsTr("Player"), title: qsTr("Playback"), icon: "playback",
                groups: [
                    {
                        title: qsTr("Files"), note: qsTr("What happens when a file opens and when it ends."),
                        rows: [
                            settings.defaultPlayer
                                ? { type: "fact", label: qsTr("Default player"), value: qsTr("OmaPlayer is the default"),
                                    description: qsTr("Videos and music open here from the file manager.") }
                                : { key: "action:default-player", type: "action", command: "makeDefaultPlayer",
                                    button: qsTr("Make default"), label: qsTr("Default player"),
                                    description: settings.defaultPlayerError !== "" ? settings.defaultPlayerError
                                               : qsTr("Open videos and music in OmaPlayer from the file manager.") },
                            { key: "playback/openIn", type: "choice", label: qsTr("When another video is opened"),
                              description: qsTr("From the file manager or a terminal, while OmaPlayer is already playing."),
                              options: [{ value: "replace", label: qsTr("Replace this one") }, { value: "window", label: qsTr("New window") }] },
                            { key: "playback/resume", type: "switch", label: qsTr("Resume where you left off"),
                              description: qsTr("A file reopens at the moment you stopped watching it.") },
                            { key: "playback/folderPlaylist", type: "switch", label: qsTr("Play the whole folder"),
                              description: qsTr("Opening one file queues the videos and music beside it.") },
                            { key: "playback/atEnd", type: "choice", label: qsTr("When the last file ends"),
                              description: qsTr("With Repeat on, from the playlist or the right-click menu, it loops instead."),
                              options: [{ value: "hold", label: qsTr("Hold the frame") }, { value: "clear", label: qsTr("Start screen") },
                                        { value: "quit", label: qsTr("Quit") }] },
                        ]
                    },
                    {
                        title: qsTr("Seeking"), note: qsTr("How far each seek jumps."),
                        rows: [
                            { key: "playback/seekStep", type: "choice", label: qsTr("Short seek"),
                              description: seekKeys("seek-back", "seek-forward"),
                              options: ["1", "3", "5", "10"].map(function (n) { return { value: n, label: qsTr("%1 s").arg(n) }; }) },
                            { key: "playback/seekLongStep", type: "choice", label: qsTr("Long seek"),
                              description: seekKeys("seek-back-long", "seek-forward-long"),
                              options: ["10", "30", "60"].map(function (n) { return { value: n, label: qsTr("%1 s").arg(n) }; }) },
                            { key: "playback/exactSeek", type: "switch", label: qsTr("Exact seeking"),
                              description: qsTr("Lands on the exact frame instead of the nearest keyframe. A little slower.") },
                        ]
                    },
                ]
            },
            {
                id: "video", section: qsTr("Player"), title: qsTr("Video"), icon: "video",
                groups: [
                    {
                        title: qsTr("Picture"), note: qsTr("How frames are made and drawn."),
                        rows: [
                            { key: "video/hwdec", type: "switch", label: qsTr("Hardware decoding"),
                              description: qsTr("Decode on the graphics card when it can. Cooler, quieter, and longer on battery.") },
                            { key: "video/smooth", type: "switch", label: qsTr("Smooth motion"),
                              description: qsTr("Matches the display's refresh rate and blends frames, so film doesn't judder.") },
                            { key: "video/scale", type: "choice", label: qsTr("Upscaling"),
                              description: qsTr("How video smaller than the window is enlarged."),
                              options: [{ value: "fast", label: qsTr("Fast") }, { value: "balanced", label: qsTr("Balanced") },
                                        { value: "sharp", label: qsTr("Sharp") }] },
                            { key: "video/deband", type: "switch", label: qsTr("Deband"),
                              description: qsTr("Smooths the color steps that show in dark, compressed scenes.") },
                            { key: "video/deinterlace", type: "switch", label: qsTr("Deinterlace"),
                              description: qsTr("Removes the comb lines from TV recordings and DVDs. Other video is left alone.") },
                        ]
                    },
                    {
                        title: qsTr("Streams"), note: qsTr("Videos opened from YouTube and other sites."),
                        rows: [
                            { key: "video/streamQuality", type: "choice", label: qsTr("Quality"),
                              description: qsTr("The highest resolution fetched. Above 1080p, older graphics cards may stutter."),
                              options: [{ value: "best", label: qsTr("Best") }].concat(["2160", "1440", "1080", "720", "480"].map(function (n) {
                                  return { value: n, label: n + "p" }; })) },
                        ]
                    },
                    {
                        title: qsTr("Screenshots"),
                        note: keyFor("screenshot") !== "" && keyFor("copy-frame") !== ""
                              ? qsTr("%1 saves the frame on screen, %2 copies it.").arg(keyFor("screenshot")).arg(keyFor("copy-frame"))
                              : qsTr("Save or copy the frame on screen from the right-click menu."),
                        rows: [
                            { key: "video/screenshotFormat", type: "choice", label: qsTr("Format"),
                              description: qsTr("PNG keeps every pixel; JPEG and WebP are smaller."),
                              options: [{ value: "png", label: "PNG" }, { value: "jpg", label: "JPEG" }, { value: "webp", label: "WebP" }] },
                            { key: "video/screenshotSubtitles", type: "switch", label: qsTr("Include subtitles"),
                              description: qsTr("The frame as you see it, subtitles and all.") },
                            { key: "video/screenshotFolder", type: "text", label: qsTr("Folder"),
                              description: qsTr("Empty saves where Omarchy's screenshots go. A missing folder is made."),
                              placeholder: settings.defaultScreenshotFolder() },
                        ]
                    },
                ]
            },
            {
                id: "audio", section: qsTr("Player"), title: qsTr("Audio"), icon: "volume",
                groups: [
                    {
                        title: qsTr("Volume"), note: qsTr("Loudness across files."),
                        rows: [
                            { key: "audio/boost", type: "switch", label: qsTr("Allow up to 150%"),
                              description: qsTr("For quiet recordings. Past 100% the sound can clip.") },
                            { key: "audio/normalize", type: "switch", label: qsTr("Even out loudness"),
                              description: qsTr("Lifts quiet dialogue and tames loud scenes.") },
                            { key: "audio/rememberVolume", type: "switch", label: qsTr("Remember the volume"),
                              description: qsTr("Each launch starts at the volume you left it at.") },
                        ]
                    },
                    {
                        title: qsTr("Output"), note: qsTr("Where the sound goes."),
                        rows: [
                            { key: "audio/device", type: "menu", label: qsTr("Output device"),
                              description: qsTr("System default follows the output picked in Omarchy."), options: audioDevices() },
                            { key: "audio/surround", type: "choice", label: qsTr("Surround sound"),
                              description: settings.values["audio/surround"] === "receiver"
                                           ? qsTr("Dolby and DTS go untouched over HDMI or S/PDIF to an AV receiver. Without one, they're silent.")
                                           : qsTr("Stereo folds surround into two channels, so dialogue stays clear on headphones and laptops."),
                              options: [{ value: "auto", label: qsTr("Auto") }, { value: "stereo", label: qsTr("Stereo") },
                                        { value: "receiver", label: qsTr("Receiver") }] },
                        ]
                    },
                    {
                        title: qsTr("Tracks"), note: qsTr("Which audio track plays first."),
                        rows: [
                            { key: "audio/language", type: "text", label: qsTr("Preferred languages"),
                              description: qsTr("Language codes like en or ja, in order, separated by commas."),
                              placeholder: qsTr("The file's default") },
                        ]
                    },
                ]
            },
            {
                id: "subtitles", section: qsTr("Player"), title: qsTr("Subtitles"), icon: "subtitles",
                groups: [
                    {
                        title: qsTr("Loading"), note: qsTr("Which subtitles come along with a video."),
                        rows: [
                            { key: "subtitles/auto", type: "choice", label: qsTr("Subtitle files"),
                              description: qsTr("Loads subtitle files that sit next to the video."),
                              options: [{ value: "fuzzy", label: qsTr("Similar name") }, { value: "exact", label: qsTr("Same name") },
                                        { value: "no", label: qsTr("Off") }] },
                            { key: "subtitles/language", type: "text", label: qsTr("Preferred languages"),
                              description: qsTr("Language codes like ko or en, in order, separated by commas."),
                              placeholder: qsTr("Omarchy's language (%1)").arg(settingsLanguageHint) },
                            { key: "subtitles/second", type: "text", label: qsTr("Second subtitle"),
                              description: qsTr("A language code, like en, for a second subtitle at the top. For watching in two languages."),
                              placeholder: qsTr("Off") },
                            { key: "subtitles/encoding", type: "menu", label: qsTr("Text encoding"),
                              description: qsTr("Auto works out each file's encoding. Pick one only if a file still comes out as gibberish."),
                              options: [{ value: "auto", label: qsTr("Auto (default)") }, { value: "utf-8", label: "UTF-8" },
                                        { value: "korean", label: "한국어 · CP949" }, { value: "japanese", label: "日本語 · Shift-JIS" },
                                        { value: "chinese-simplified", label: "简体中文 · GB18030" },
                                        { value: "chinese-traditional", label: "繁體中文 · Big5" }] },
                        ]
                    },
                    {
                        title: qsTr("Look"), note: qsTr("How subtitles are drawn."),
                        rows: [
                            { key: "subtitles/scale", type: "level", label: qsTr("Size"),
                              description: qsTr("Relative to the subtitles' own size."), minimum: 0.5, maximum: 2, step: 0.05 },
                            { key: "subtitles/lift", type: "level", label: qsTr("Height"),
                              description: qsTr("How far above the bottom of the picture they sit."), minimum: 0, maximum: 0.3, step: 0.02 },
                            { key: "subtitles/font", type: "menu", label: qsTr("Font"),
                              description: qsTr("Any font installed on this computer."),
                              options: [{ value: "", label: qsTr("System default") }].concat(Qt.fontFamilies().map(function (family) {
                                  return { value: family, label: family }; })) },
                            { key: "subtitles/background", type: "switch", label: qsTr("Box behind the text"),
                              description: qsTr("A dark box instead of an outline, for subtitles over bright scenes.") },
                            { key: "subtitles/restyle", type: "switch", label: qsTr("Restyle styled subtitles"),
                              description: qsTr("Styled (ASS) subtitles, like most anime releases, take the look above too.") },
                        ]
                    },
                ]
            },
            {
                id: "controls", section: qsTr("Interface"), title: qsTr("Controls"), icon: "controls",
                groups: [
                    {
                        title: qsTr("Bar"), note: qsTr("The seek bar and buttons."),
                        rows: [
                            { key: "interface/controls", type: "choice", label: qsTr("Bar in a window"),
                              description: qsTr("Fullscreen always floats it over the picture."),
                              options: [{ value: "docked", label: qsTr("Below the picture") }, { value: "floating", label: qsTr("Over the picture") }] },
                            { key: "interface/hideDelay", type: "choice", label: qsTr("Hide after"),
                              description: qsTr("How long a still pointer waits before the controls step aside."),
                              options: [1, 2, 3, 5].map(function (n) { return { value: n, label: qsTr("%1 s").arg(n) }; }) },
                            { key: "interface/remaining", type: "switch", label: qsTr("Show time left"),
                              description: qsTr("The clock counts down. Clicking the clock switches it too.") },
                        ]
                    },
                    {
                        title: qsTr("Window"), note: qsTr("OmaPlayer on the desktop."),
                        rows: [
                            { key: "interface/startFullscreen", type: "switch", label: qsTr("Start in fullscreen"),
                              description: qsTr("Opening a file fills the screen. Esc goes back to the window.") },
                            { key: "video/keepAwake", type: "switch", label: qsTr("Keep the screen awake"),
                              description: qsTr("The screen doesn't dim or turn off while a video plays.") },
                        ]
                    },
                    {
                        title: qsTr("Mouse"), note: qsTr("On the picture itself."),
                        rows: [
                            { key: "playback/clickPause", type: "switch", label: qsTr("Click to pause"),
                              description: qsTr("A single click plays or pauses. A double-click is always fullscreen.") },
                            { key: "playback/wheel", type: "choice", label: qsTr("Mouse wheel"),
                              description: qsTr("What scrolling over the picture changes."),
                              options: [{ value: "volume", label: qsTr("Volume") }, { value: "seek", label: qsTr("Seek") }] },
                        ]
                    },
                ]
            },
            {
                id: "keyboard", section: qsTr("Interface"), title: qsTr("Keyboard"), icon: "keyboard",
                groups: [
                    {
                        title: qsTr("Layout"), note: qsTr("Every shortcut in the app follows the layout."),
                        rows: [
                            { key: "interface/keymap", type: "choice", label: qsTr("Shortcuts"),
                              description: qsTr("OmaPlayer's own keys, or mpv's for those coming from it."),
                              options: [{ value: "omaplayer", label: "OmaPlayer" }, { value: "mpv", label: "mpv" }] },
                            { key: "action:reset-keys", type: "action", command: "resetAllKeys", button: qsTr("Reset all"),
                              label: qsTr("Your shortcuts"),
                              description: qsTr("%n shortcuts changed from the layout.", "", settings.customBindings) },
                        ]
                    },
                ].concat(actions.groups.map(function (group, index) {
                    // One card per group, as in the right-click menu.
                    return {
                        title: group.text,
                        note: index === 0 ? qsTr("Click + or press Enter, then the new key. Backspace removes the last one.") : "",
                        rows: actions.list.filter(function (entry) { return entry.group === group.id; }).map(function (entry) {
                            return { key: "binding:" + entry.id, type: "binding", actionId: entry.id, label: entry.text,
                                     keys: entry.keys, labels: entry.keyLabels, custom: entry.custom };
                        })
                    };
                }))
            },
            {
                // In English too, so anyone can find their way back from a
                // language they can't read.
                id: "language", section: qsTr("Interface"),
                title: qsTr("Language") === "Language" ? "Language" : qsTr("Language") + " / Language", icon: "language",
                groups: [
                    {
                        title: qsTr("Interface language"), note: qsTr("The language of menus, messages, and settings."),
                        rows: [
                            { key: "radio:language:omarchy", type: "radio", setting: "interface/language", value: "omarchy",
                              label: qsTr("Follow Omarchy"), description: qsTr("Switches along with Omarchy's language.") },
                            { key: "radio:language:en", type: "radio", setting: "interface/language", value: "en", label: "English" },
                            { key: "radio:language:ko", type: "radio", setting: "interface/language", value: "ko", label: "한국어", description: "Korean" },
                            { key: "radio:language:ja", type: "radio", setting: "interface/language", value: "ja", label: "日本語", description: "Japanese" },
                            { key: "radio:language:zh-CN", type: "radio", setting: "interface/language", value: "zh-CN", label: "简体中文", description: "Chinese (Simplified)" },
                            { key: "radio:language:zh-TW", type: "radio", setting: "interface/language", value: "zh-TW", label: "繁體中文", description: "Chinese (Traditional)" },
                        ]
                    },
                ]
            },
            {
                id: "about", section: qsTr("OmaPlayer"), title: qsTr("About"), icon: "info",
                groups: [
                    {
                        title: "OmaPlayer", note: qsTr("A media player for Omarchy, built on mpv."),
                        rows: [
                            { type: "fact", label: qsTr("Version"), value: Qt.application.version,
                              url: "https://github.com/verified-human-42/omaplayer/releases" },
                            { type: "fact", label: qsTr("Updates"),
                              value: settings.updatesWithOmarchy() ? qsTr("With omarchy-update") : qsTr("Run the install line again") },
                            { type: "fact", label: qsTr("Website"), value: "github.com/verified-human-42/omaplayer",
                              url: "https://github.com/verified-human-42/omaplayer" },
                            { type: "fact", label: qsTr("License"), value: "GPL-3.0",
                              url: "https://github.com/verified-human-42/omaplayer/blob/main/LICENSE" },
                            { type: "fact", label: "mpv", value: player.get("mpv-version") },
                            { type: "fact", label: "FFmpeg", value: player.get("ffmpeg-version") },
                            // Paths open their folder in the file manager.
                            { type: "fact", label: qsTr("Settings file"), value: settings.fileName(),
                              url: folderUrl(settings.fileName()) },
                            { type: "fact", label: qsTr("Resume positions"), value: player.get("watch-later-dir"),
                              url: folderUrl(player.get("watch-later-dir") + "/") },
                        ]
                    },
                    {
                        title: qsTr("Start over"), note: "",
                        rows: [
                            { key: "action:reset-all", type: "action", command: "resetAll", button: qsTr("Reset"),
                              label: qsTr("Reset all settings"),
                              description: qsTr("Every setting and shortcut back to how it came. Undo all takes it back until you close this page.") },
                        ]
                    },
                ]
            },
        ];
    }

    // Rows matching every word of the search, from every page, under
    // "Page › Group" so labels like "Folder" keep their context.
    readonly property var groups: {
        if (query.trim() === "")
            return pages[pageIndex].groups;
        var words = query.trim().toLowerCase().split(/\s+/), found = [];
        for (var p = 0; p < pages.length; ++p)
            for (var g = 0; g < pages[p].groups.length; ++g) {
                var group = pages[p].groups[g];
                var rows = group.rows.filter(function (row) {
                    var haystack = [row.label, row.description || "", row.value || "", pages[p].title, group.title]
                        .concat((row.options || []).map(function (option) { return option.label; }))
                        .concat(row.labels || []).join(" ").toLowerCase();
                    return words.every(function (word) { return haystack.indexOf(word) >= 0; });
                });
                if (rows.length > 0)
                    found.push({ title: pages[p].title + " › " + group.title, note: "", rows: rows });
            }
        return found;
    }

    // The rows the keyboard moves between, in order.
    readonly property var stops: {
        var list = [];
        groups.forEach(function (group) {
            group.rows.forEach(function (row) {
                if (["switch", "choice", "menu", "level", "text", "binding", "action", "radio"].indexOf(row.type) >= 0)
                    list.push(row);
            });
        });
        return list;
    }

    signal stepRequested(string key, int delta)
    signal activateRequested(string key)

    function stopIndex(key) {
        for (var i = 0; i < stops.length; ++i)
            if (stops[i].key === key)
                return i;
        return -1;
    }

    function goToPage(index) {
        query = "";
        searchInput.text = "";
        pageIndex = (index + pages.length) % pages.length;
        rowIndex = -1;
        scroller.contentY = 0;
    }

    function moveRow(delta) {
        if (stops.length === 0)
            return;
        rowIndex = rowIndex < 0 ? (delta > 0 ? 0 : stops.length - 1)
                                : Math.max(0, Math.min(stops.length - 1, rowIndex + delta));
    }

    function reveal(item) {
        var top = item.mapToItem(scroller.contentItem, 0, 0).y;
        if (top < scroller.contentY + 12)
            scroller.contentY = Math.max(0, top - 12);
        else if (top + item.height > scroller.contentY + scroller.height - 12)
            scroller.contentY = Math.min(scroller.contentHeight - scroller.height, top + item.height - scroller.height + 12);
    }

    property double lastKeyAt: 0

    function handleKey(event) {
        lastKeyAt = Date.now();
        // A binding that's listening takes the next key, whatever it is.
        if (capturing !== "") {
            event.accepted = true;
            if (event.key === Qt.Key_Escape) {
                capturing = "";
                return;
            }
            var pressed = actions.sequence(event.key, event.modifiers, event.text);
            if (pressed === "")
                return;
            var from = settings.bindKey(capturing, pressed);
            showNote(capturing, from !== "" ? qsTr("Taken from %1").arg(actions.text(from)) : "");
            capturing = "";
            return;
        }
        var alt = event.modifiers & Qt.AltModifier;
        var entry = rowIndex >= 0 && rowIndex < stops.length ? stops[rowIndex] : null;
        switch (event.key) {
        case Qt.Key_Backspace:
        case Qt.Key_Delete:
            if (entry && entry.type === "binding" && entry.keys.length > 0)
                settings.unbindKey(entry.actionId, entry.keys[entry.keys.length - 1]);
            else
                return;
            break;
        case Qt.Key_Escape:
            if (query !== "")
                goToPage(pageIndex);
            else
                close();
            break;
        case Qt.Key_F5:
            close();
            break;
        case Qt.Key_Comma:
            if (event.modifiers & Qt.ControlModifier)
                close();
            else
                return;
            break;
        case Qt.Key_Slash:
            searchInput.forceActiveFocus();
            break;
        case Qt.Key_Tab:
            moveRow(1);
            break;
        case Qt.Key_Backtab:
            moveRow(-1);
            break;
        case Qt.Key_Home:
            rowIndex = stops.length > 0 ? 0 : -1;
            break;
        case Qt.Key_End:
            rowIndex = stops.length - 1;
            break;
        case Qt.Key_PageUp:
            moveRow(-8);
            break;
        case Qt.Key_PageDown:
            moveRow(8);
            break;
        case Qt.Key_Up:
        case Qt.Key_K:
            if (alt)
                goToPage(pageIndex - 1);
            else
                moveRow(-1);
            break;
        case Qt.Key_Down:
        case Qt.Key_J:
            if (alt)
                goToPage(pageIndex + 1);
            else
                moveRow(1);
            break;
        case Qt.Key_Left:
        case Qt.Key_H:
            if (entry)
                stepRequested(entry.key, -1);
            break;
        case Qt.Key_Right:
        case Qt.Key_L:
            if (entry)
                stepRequested(entry.key, 1);
            break;
        case Qt.Key_Space:
        case Qt.Key_Return:
        case Qt.Key_Enter:
            if (entry)
                activateRequested(entry.key);
            break;
        default:
            return;
        }
        event.accepted = true;
    }

    parent: Overlay.overlay
    x: 0
    y: 0
    width: parent ? parent.width : 0
    height: parent ? parent.height : 0
    padding: 0
    modal: true
    focus: true
    closePolicy: Popup.NoAutoClose

    onClosed: capturing = ""

    // The sound outputs, for a menu. mpv lists each output once per
    // sound system (PipeWire, Pulse, ALSA); Omarchy plays through PipeWire,
    // so its outputs are the ones listed, unless there are none.
    function audioDevices() {
        var all = player.get("audio-device-list") || [];
        var prefix = ["pipewire/", "pulse/", "alsa/"].find(function (system) {
            return all.some(function (device) { return device.name.startsWith(system); });
        }) || "";
        var devices = all.filter(function (device) {
            return device.name === "auto" || (prefix !== "" && device.name.startsWith(prefix));
        });
        var options = devices.map(function (device) {
            return { value: device.name, label: device.name === "auto" ? qsTr("System default") : device.description };
        });
        // One that was picked and is now unplugged still shows as picked.
        var chosen = settings.values["audio/device"];
        if (!options.some(function (option) { return option.value === chosen; }))
            options.push({ value: chosen, label: qsTr("%1 (not connected)").arg(chosen.substring(chosen.indexOf("/") + 1)) });
        return options;
    }

    function folderUrl(path) {
        return "file://" + String(path).substring(0, String(path).lastIndexOf("/"));
    }

    // Placeholder for the subtitle languages: what an empty field follows.
    readonly property string settingsLanguageHint: translator.language

    onOpened: {
        settings.beginSession();
        settings.checkDefaultPlayer();
        goToPage(pageIndex);
        body.forceActiveFocus();
    }

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 120 }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 100 }
    }

    background: Rectangle {
        color: theme.background
    }

    contentItem: Item {
        id: body

        focus: true
        Keys.onPressed: (event) => page.handleKey(event)

        // What changed, and the way back.
        Rectangle {
            id: header
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 40
            color: page.shade(0.07)

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8

                Label {
                    text: "OmaPlayer"
                    color: page.ink
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
                Label {
                    anchors.baseline: parent.children[0].baseline
                    text: Qt.application.version
                    color: page.shade(0.5)
                    font.pixelSize: 10
                }
            }

            Row {
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                spacing: 10

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: settings.changedKeys.length > 0
                    text: qsTr("%n settings changed", "", settings.changedKeys.length)
                    color: page.shade(0.6)
                    font.pixelSize: 11
                }
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: settings.changedKeys.length > 0
                    width: undoText.implicitWidth + 20
                    height: 22
                    radius: theme.radius > 0 ? height / 2 : 0
                    color: undoHover.hovered ? page.shade(0.08) : "transparent"
                    border.width: 1
                    border.color: page.shade(0.35)

                    Label {
                        id: undoText
                        anchors.centerIn: parent
                        text: qsTr("Undo all")
                        color: page.ink
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }
                    HoverHandler {
                        id: undoHover
                        cursorShape: Qt.PointingHandCursor
                    }
                    TapHandler {
                        onTapped: settings.undoAll()
                    }
                }
                KeyCap {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "esc"

                    TapHandler {
                        onTapped: page.close()
                    }
                }
            }
        }

        // The sidebar: search, then the pages under their sections.
        Rectangle {
            id: sidebar
            anchors.left: parent.left
            anchors.top: header.bottom
            anchors.bottom: footer.top
            width: Math.min(250, parent.width * 0.32)
            color: page.shade(0.04)

            Rectangle {
                id: search
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                height: 32
                radius: Math.max(8, theme.radius)
                color: "transparent"
                border.width: 1
                border.color: searchInput.activeFocus ? theme.accent : page.shade(0.22)

                IconImage {
                    id: searchIcon
                    anchors.left: parent.left
                    anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    source: "qrc:/icons/search.svg"
                    sourceSize: Qt.size(14, 14)
                    color: page.shade(0.55)
                }

                TextInput {
                    id: searchInput
                    anchors.left: searchIcon.right
                    anchors.right: parent.right
                    anchors.leftMargin: 8
                    anchors.rightMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    color: page.ink
                    selectionColor: theme.accent
                    selectedTextColor: theme.accentForeground
                    font.pixelSize: 12
                    clip: true
                    onTextChanged: {
                        page.query = text;
                        page.rowIndex = -1;
                        scroller.contentY = 0;
                    }
                    Keys.onEscapePressed: (event) => {
                        text = "";
                        body.forceActiveFocus();
                        event.accepted = true;
                    }
                    Keys.onDownPressed: (event) => {
                        body.forceActiveFocus();
                        page.moveRow(1);
                        event.accepted = true;
                    }
                    Keys.onReturnPressed: (event) => {
                        body.forceActiveFocus();
                        page.moveRow(1);
                        event.accepted = true;
                    }

                    Label {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: searchInput.text === ""
                        text: qsTr("Search settings")
                        color: page.shade(0.45)
                        font.pixelSize: 12
                    }
                }
            }

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: search.bottom
                anchors.topMargin: 10
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                spacing: 2

                Repeater {
                    model: page.pages
                    delegate: Column {
                        id: navGroup

                        required property var modelData
                        required property int index
                        readonly property bool opensSection: index === 0 || page.pages[index - 1].section !== modelData.section
                        readonly property bool selected: page.query === "" && page.pageIndex === index

                        width: parent.width
                        spacing: 2

                        Item {
                            visible: navGroup.opensSection
                            width: parent.width
                            height: navGroup.index === 0 ? 24 : 36

                            Rectangle {
                                visible: navGroup.index > 0
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.topMargin: 4
                                height: 1
                                color: page.shade(0.1)
                            }
                            Label {
                                anchors.left: parent.left
                                anchors.leftMargin: 10
                                anchors.bottom: parent.bottom
                                anchors.bottomMargin: 4
                                text: navGroup.modelData.section
                                color: page.shade(0.5)
                                font.pixelSize: 10
                                font.weight: Font.DemiBold
                                font.capitalization: Font.AllUppercase
                                font.letterSpacing: /^(ko|ja|zh)/.test(translator.language) ? 0 : 1.4 // Hangul and kanji read wrong spread
                            }
                        }

                        Rectangle {
                            width: parent.width
                            height: 32
                            radius: Math.max(8, theme.radius)
                            color: navGroup.selected ? theme.accent : navHover.hovered ? page.shade(0.08) : "transparent"

                            IconImage {
                                id: navIcon
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                source: "qrc:/icons/" + navGroup.modelData.icon + ".svg"
                                sourceSize: Qt.size(15, 15)
                                color: navGroup.selected ? theme.accentForeground : page.shade(0.7)
                            }
                            Label {
                                anchors.left: navIcon.right
                                anchors.leftMargin: 12
                                anchors.verticalCenter: parent.verticalCenter
                                text: navGroup.modelData.title
                                color: navGroup.selected ? theme.accentForeground : page.ink
                                font.pixelSize: 13
                                font.weight: navGroup.selected ? Font.DemiBold : Font.Normal
                            }

                            HoverHandler {
                                id: navHover
                                cursorShape: Qt.PointingHandCursor
                            }
                            TapHandler {
                                onTapped: {
                                    page.goToPage(navGroup.index);
                                    body.forceActiveFocus();
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            anchors.left: sidebar.right
            anchors.top: sidebar.top
            anchors.bottom: sidebar.bottom
            width: 1
            color: page.shade(0.1)
        }

        // The page: its title, then each group with its heading, its note, and
        // its settings in a framed card.
        Flickable {
            id: scroller
            anchors.left: sidebar.right
            anchors.right: parent.right
            anchors.top: header.bottom
            anchors.bottom: footer.top
            anchors.leftMargin: 1
            contentHeight: content.implicitHeight + 48
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar {}

            Column {
                id: content
                x: 28
                y: 22
                width: Math.min(scroller.width - 56, 860)
                spacing: 0

                Label {
                    text: page.query === "" ? page.pages[page.pageIndex].title : qsTr("Results for “%1”").arg(page.query.trim())
                    color: page.ink
                    font.pixelSize: 24
                    font.weight: Font.DemiBold
                    bottomPadding: 14
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: page.shade(0.1)
                }

                Label {
                    visible: page.query !== "" && page.groups.length === 0
                    topPadding: 24
                    text: qsTr("Nothing matches. Try another word.")
                    color: page.shade(0.6)
                    font.pixelSize: 13
                }

                Repeater {
                    model: page.groups
                    delegate: Column {
                        id: group

                        required property var modelData

                        width: content.width
                        topPadding: 22
                        spacing: 6

                        Label {
                            text: group.modelData.title
                            color: page.ink
                            font.pixelSize: 16
                            font.weight: Font.Bold
                        }
                        Rectangle {
                            width: parent.width
                            height: 1
                            color: page.shade(0.1)
                        }
                        Label {
                            visible: group.modelData.note !== ""
                            width: parent.width
                            wrapMode: Text.WordWrap
                            text: group.modelData.note
                            color: page.shade(0.6)
                            font.pixelSize: 11
                            bottomPadding: 4
                        }

                        Rectangle {
                            width: parent.width
                            height: rows.implicitHeight + 8
                            radius: Math.max(8, theme.radius)
                            color: page.shade(0.03)
                            border.width: 1
                            border.color: page.shade(0.1)

                            Column {
                                id: rows
                                y: 4
                                width: parent.width

                                Repeater {
                                    model: group.modelData.rows
                                    delegate: Column {
                                        id: rowHolder

                                        required property var modelData
                                        required property int index

                                        width: rows.width

                                        Rectangle {
                                            visible: rowHolder.index > 0
                                            x: 16
                                            width: parent.width - 32
                                            height: 1
                                            color: page.shade(0.08)
                                        }

                                        SettingRow {
                                            id: settingRow
                                            width: parent.width
                                            entry: rowHolder.modelData
                                            capturing: page.capturing !== "" && page.capturing === (rowHolder.modelData.actionId || "")
                                            note: page.noteFor !== "" && page.noteFor === (rowHolder.modelData.actionId || "") ? page.noteText : ""
                                            onCaptureRequested: page.startCapture(rowHolder.modelData.actionId)
                                            current: page.rowIndex >= 0 && page.rowIndex < page.stops.length
                                                     && (rowHolder.modelData.key || "") !== ""
                                                     && page.stops[page.rowIndex].key === rowHolder.modelData.key
                                            onCurrentChanged: if (current) page.reveal(settingRow)
                                            // Rows passing under a still pointer while the
                                            // keyboard scrolls don't take the cursor.
                                            onPointed: if (Date.now() - page.lastKeyAt > 500)
                                                           page.rowIndex = page.stopIndex(rowHolder.modelData.key)
                                            onEditingDone: body.forceActiveFocus()

                                            Connections {
                                                target: page
                                                function onStepRequested(key, delta) {
                                                    if (key === rowHolder.modelData.key)
                                                        settingRow.step(delta);
                                                }
                                                function onActivateRequested(key) {
                                                    if (key === rowHolder.modelData.key)
                                                        settingRow.activate();
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // The keys, along the foot of the window.
        Rectangle {
            id: footer
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 32
            color: page.shade(0.07)

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 20

                Repeater {
                    model: [
                        { keys: ["↑", "↓"], text: qsTr("Move") },
                        { keys: ["←", "→"], text: qsTr("Change") },
                        { keys: ["alt", "↑", "↓"], text: qsTr("Change page") },
                        { keys: ["/"], text: qsTr("Search") },
                        { keys: ["esc"], text: qsTr("Close") },
                    ]
                    delegate: Row {
                        required property var modelData
                        spacing: 4

                        Repeater {
                            model: modelData.keys
                            delegate: KeyCap {
                                required property var modelData
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData
                            }
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            leftPadding: 4
                            text: modelData.text
                            color: page.shade(0.6)
                            font.pixelSize: 11
                        }
                    }
                }
            }
        }
    }
}
