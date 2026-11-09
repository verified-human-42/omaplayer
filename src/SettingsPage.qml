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

    readonly property var pages: {
        actions.list; // re-read after a language or keymap change
        return [
            {
                id: "playback", section: qsTr("Player"), title: qsTr("Playback"), icon: "playback",
                groups: [
                    {
                        title: qsTr("Files"), note: qsTr("What happens when a file opens and when it ends."),
                        rows: [
                            { key: "playback/resume", type: "switch", label: qsTr("Resume where you left off"),
                              description: qsTr("A file reopens at the moment you stopped watching it.") },
                            { key: "playback/folderPlaylist", type: "switch", label: qsTr("Play the whole folder"),
                              description: qsTr("Opening one file queues the videos and music beside it.") },
                            { key: "playback/keepOpen", type: "switch", label: qsTr("Hold the last frame"),
                              description: qsTr("The last file stays on its final frame instead of closing.") },
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
                id: "video", section: qsTr("Player"), title: qsTr("Video"), icon: "video",
                groups: [
                    {
                        title: qsTr("Decoding"), note: qsTr("How frames are made."),
                        rows: [
                            { key: "video/hwdec", type: "switch", label: qsTr("Hardware decoding"),
                              description: qsTr("Decode on the graphics card when it can. Cooler, quieter, and longer on battery.") },
                        ]
                    },
                    {
                        title: qsTr("Picture"), note: qsTr("How frames are drawn."),
                        rows: [
                            { key: "video/smooth", type: "switch", label: qsTr("Smooth motion"),
                              description: qsTr("Matches the display's refresh rate and blends frames, so film doesn't judder.") },
                            { key: "video/scale", type: "choice", label: qsTr("Upscaling"),
                              description: qsTr("How video smaller than the window is enlarged."),
                              options: [{ value: "fast", label: qsTr("Fast") }, { value: "balanced", label: qsTr("Balanced") },
                                        { value: "sharp", label: qsTr("Sharp") }] },
                            { key: "video/deband", type: "switch", label: qsTr("Deband"),
                              description: qsTr("Smooths the color steps that show in dark, compressed scenes.") },
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
                        ]
                    },
                    {
                        title: qsTr("Tracks"), note: qsTr("Which audio track plays first."),
                        rows: [
                            { key: "audio/language", type: "text", label: qsTr("Preferred languages"),
                              description: qsTr("Language codes in order, separated by commas."), placeholder: "en,ja" },
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
                              description: qsTr("Language codes in order, separated by commas. Empty follows Omarchy's language."),
                              placeholder: settingsLanguageHint },
                        ]
                    },
                    {
                        title: qsTr("Text encoding"),
                        note: qsTr("Auto reads each subtitle file and works out how it was written. Pick one only if a file still comes out as gibberish."),
                        rows: [
                            { key: "radio:encoding:auto", type: "radio", setting: "subtitles/encoding", value: "auto",
                              label: qsTr("Auto"), description: qsTr("Recommended") },
                            { key: "radio:encoding:utf-8", type: "radio", setting: "subtitles/encoding", value: "utf-8", label: "UTF-8", description: "Unicode" },
                            { key: "radio:encoding:korean", type: "radio", setting: "subtitles/encoding", value: "korean", label: "한국어", description: qsTr("Korean") + " · CP949" },
                            { key: "radio:encoding:japanese", type: "radio", setting: "subtitles/encoding", value: "japanese", label: "日本語", description: qsTr("Japanese") + " · Shift-JIS" },
                            { key: "radio:encoding:chinese-simplified", type: "radio", setting: "subtitles/encoding", value: "chinese-simplified", label: "简体中文", description: qsTr("Chinese (Simplified)") + " · GB18030" },
                            { key: "radio:encoding:chinese-traditional", type: "radio", setting: "subtitles/encoding", value: "chinese-traditional", label: "繁體中文", description: qsTr("Chinese (Traditional)") + " · Big5" },
                        ]
                    },
                    {
                        title: qsTr("Look"), note: qsTr("How subtitles are drawn."),
                        rows: [
                            { key: "subtitles/scale", type: "level", label: qsTr("Size"),
                              description: qsTr("Relative to the subtitles' own size."), minimum: 0.5, maximum: 2, step: 0.05 },
                        ]
                    },
                ]
            },
            {
                id: "controls", section: qsTr("Interface"), title: qsTr("Controls"), icon: "controls",
                groups: [
                    {
                        title: qsTr("Bar"), note: qsTr("The controls under the picture."),
                        rows: [
                            { key: "interface/controls", type: "choice", label: qsTr("In a window"),
                              description: qsTr("Fullscreen always floats the bar over the picture."),
                              options: [{ value: "docked", label: qsTr("Under the picture") }, { value: "floating", label: qsTr("Over it") }] },
                            { key: "interface/hideDelay", type: "choice", label: qsTr("Hide after"),
                              description: qsTr("How long a still pointer waits before the controls step aside."),
                              options: [1, 2, 3, 5].map(function (n) { return { value: n, label: qsTr("%1 s").arg(n) }; }) },
                            { key: "interface/remaining", type: "switch", label: qsTr("Count down"),
                              description: qsTr("The clock shows the time left. Clicking the clock switches it too.") },
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
                              description: qsTr("Omaplayer's own keys, or mpv's for those coming from it."),
                              options: [{ value: "omaplayer", label: "Omaplayer" }, { value: "mpv", label: "mpv" }] },
                        ]
                    },
                    {
                        title: qsTr("Keys"), note: qsTr("Click + or press Enter, then the new key. Backspace removes the last one."),
                        rows: [{ key: "action:reset-keys", type: "action", command: "resetAllKeys", button: qsTr("Reset all"),
                                 label: qsTr("Your shortcuts"),
                                 description: qsTr("%n shortcuts changed from the layout.", "", settings.customBindings) }]
                              .concat(actions.list.map(function (entry) {
                                  return { key: "binding:" + entry.id, type: "binding", actionId: entry.id, label: entry.text,
                                           keys: entry.keys, labels: entry.keyLabels, custom: entry.custom };
                              }))
                    },
                ]
            },
            {
                id: "language", section: qsTr("Interface"), title: qsTr("Language"), icon: "language",
                groups: [
                    {
                        title: qsTr("Language"), note: qsTr("The language of menus, messages, and settings."),
                        rows: [
                            { key: "radio:language:omarchy", type: "radio", setting: "interface/language", value: "omarchy",
                              label: qsTr("Follow Omarchy"), description: qsTr("Whatever Omarchy is set to, switching along with it.") },
                            { key: "radio:language:en", type: "radio", setting: "interface/language", value: "en", label: "English", description: "English" },
                            { key: "radio:language:ko", type: "radio", setting: "interface/language", value: "ko", label: "한국어", description: "Korean" },
                            { key: "radio:language:ja", type: "radio", setting: "interface/language", value: "ja", label: "日本語", description: "Japanese" },
                            { key: "radio:language:zh-CN", type: "radio", setting: "interface/language", value: "zh-CN", label: "简体中文", description: "Chinese (Simplified)" },
                            { key: "radio:language:zh-TW", type: "radio", setting: "interface/language", value: "zh-TW", label: "繁體中文", description: "Chinese (Traditional)" },
                        ]
                    },
                ]
            },
            {
                id: "about", section: qsTr("Omaplayer"), title: qsTr("About"), icon: "info",
                groups: [
                    {
                        title: "Omaplayer", note: qsTr("A media player for Omarchy, built on mpv."),
                        rows: [
                            { type: "fact", label: qsTr("Version"), value: Qt.application.version },
                            { type: "fact", label: "mpv", value: player.get("mpv-version") },
                            { type: "fact", label: "FFmpeg", value: player.get("ffmpeg-version") },
                            { type: "fact", label: qsTr("Settings file"), value: settings.fileName() },
                            { type: "fact", label: qsTr("Resume positions"), value: player.get("watch-later-dir") },
                        ]
                    },
                ]
            },
        ];
    }

    // Rows matching the search, from every page, grouped under their page.
    readonly property var groups: {
        if (query.trim() === "")
            return pages[pageIndex].groups;
        var needle = query.trim().toLowerCase(), found = [];
        for (var p = 0; p < pages.length; ++p) {
            var rows = [];
            for (var g = 0; g < pages[p].groups.length; ++g)
                pages[p].groups[g].rows.forEach(function (row) {
                    var haystack = [row.label, row.description || "", pages[p].title, pages[p].groups[g].title].join(" ").toLowerCase();
                    if (haystack.indexOf(needle) >= 0)
                        rows.push(row);
                });
            if (rows.length > 0)
                found.push({ title: pages[p].title, note: "", rows: rows });
        }
        return found;
    }

    // The rows the keyboard moves between, in order.
    readonly property var stops: {
        var list = [];
        groups.forEach(function (group) {
            group.rows.forEach(function (row) {
                if (["switch", "choice", "level", "text", "binding", "action", "radio"].indexOf(row.type) >= 0)
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

    function handleKey(event) {
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

    // Placeholder for the subtitle languages: what an empty field follows.
    readonly property string settingsLanguageHint: translator.language

    onOpened: {
        settings.beginSession();
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
                    text: "Omaplayer"
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
                                font.letterSpacing: 1.4
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
                                            onPointed: page.rowIndex = page.stopIndex(rowHolder.modelData.key)

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
