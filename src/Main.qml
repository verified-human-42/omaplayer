import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import "Format.js" as Format

ApplicationWindow {
    id: win

    width: 1280
    height: 784
    minimumWidth: 520
    minimumHeight: 320
    visible: true
    title: player.idle || player.title === "" ? "OmaPlayer" : player.title + " — OmaPlayer"

    // The clear color. mpv draws the picture over it, under everything else.
    color: "black"
    font.family: "monospace"
    font.pixelSize: 13

    // Menus and sheets follow the Omarchy theme, light or dark; the bar and
    // the messages over the video use the theme's stage colors.
    Material.theme: theme.dark ? Material.Dark : Material.Light
    Material.accent: theme.accent
    Material.background: theme.background
    Material.foreground: theme.foreground
    Material.roundedScale: theme.radius

    readonly property bool fullscreen: visibility === Window.FullScreen
    // Windowed, the controls dock under the picture; in
    // fullscreen they float over it and step aside while you watch.
    readonly property bool docked: !fullscreen && !desktop.pictureInPicture && settings.values["interface/controls"] !== "floating"
    property bool pointerActive: true
    property bool infoVisible: false
    property bool playlistVisible: false
    readonly property bool controlsShown: docked || pointerActive || player.paused || player.idle
                                          || controlBar.hovered || menuOpen
    // The menu, the sheets, and the panels load the first time they're asked
    // for, so a launch only builds what the first frame shows.
    readonly property bool menuOpen: menuLoader.item !== null && menuLoader.item.visible
    readonly property bool sheetOpen: sheetLoader.item !== null && sheetLoader.item.visible
    readonly property bool settingsOpen: settingsLoader.item !== null && settingsLoader.item.visible
    readonly property bool goToOpen: goToLoader.item !== null && goToLoader.item.visible

    function openMenu(source) {
        menuLoader.active = true;
        if (source)
            menuLoader.item.popup(source, 0, -menuLoader.item.implicitHeight);
        else
            menuLoader.item.popup();
    }

    function toggleSheet() {
        sheetLoader.active = true;
        if (sheetLoader.item.visible)
            sheetLoader.item.close();
        else
            sheetLoader.item.open();
    }

    function toggleFullscreen() {
        // Fullscreen from picture in picture goes through the tiled window.
        if (desktop.pictureInPicture)
            desktop.setPictureInPicture(false);
        win.visibility = fullscreen ? Window.Windowed : Window.FullScreen;
    }

    function wake() {
        pointerActive = true;
        idleTimer.restart();
    }

    function runWindowAction(id) {
        switch (id) {
        case "open-file":
            picker.openMedia();
            break;
        case "quit":
            win.close();
            break;
        case "fullscreen":
            toggleFullscreen();
            break;
        case "menu":
            openMenu();
            break;
        case "picture-in-picture":
            if (fullscreen)
                toggleFullscreen();
            desktop.setPictureInPicture(!desktop.pictureInPicture,
                                        player.videoSize.height > 0 ? player.videoSize.width / player.videoSize.height : 16 / 9);
            break;
        case "exit-fullscreen":
            if (desktop.pictureInPicture)
                desktop.setPictureInPicture(false);
            else if (sheetOpen)
                toggleSheet();
            else if (fullscreen)
                toggleFullscreen();
            break;
        case "info":
            infoVisible = !infoVisible;
            break;
        case "playlist":
            playlistVisible = !playlistVisible;
            break;
        case "settings":
            settingsLoader.active = true;
            settingsLoader.item.open();
            break;
        case "shortcuts":
            toggleSheet();
            break;
        case "reopen-last":
            var last = settings.lastFile();
            if (last !== "")
                player.openPaths([last]);
            else
                showOsd("reopen-none");
            break;
        case "normalize":
            settings.set("audio/normalize", !settings.values["audio/normalize"]);
            showOsd("normalize");
            break;
        case "go-to-time":
            if (!player.idle && player.seekable) {
                goToLoader.active = true;
                goToLoader.item.open();
            }
            break;
        }
    }

    function trackName(type) {
        var tracks = player.tracks;
        for (var i = 0; i < tracks.length; ++i) {
            var track = tracks[i];
            if (track.type === type && track.selected)
                return Format.trackLabel(track, qsTr("Track %1").arg(track.id));
        }
        return qsTr("Off");
    }

    // "[2/3] " in a playlist, nothing for a single file.
    function placeInPlaylist() {
        return player.playlistCount > 1 ? "[" + (player.playlistPosition + 1) + "/" + player.playlistCount + "] " : "";
    }

    // The file's name, as on disk; a stream's own title.
    function mediaName() {
        var path = player.path;
        if (path.indexOf("://") >= 0 && !path.startsWith("file://"))
            return player.title || path;
        return decodeURIComponent(path.substring(path.lastIndexOf("/") + 1));
    }

    function signed(value, digits) {
        return (value > 0 ? "+" : "") + Number(value).toLocaleString(Qt.locale(), 'f', digits);
    }

    // What each action reports in the corner, read after mpv has applied it.
    function percent(value) {
        return (value > 0 ? "+" : "") + Math.round(value) + "%";
    }

    function moment(value) {
        return typeof value === "number" ? Format.time(value) : "…";
    }

    function showOsd(kind) {
        // "position:seek-forward": what changed, and the action that changed it.
        var source = kind.indexOf(":") > 0 ? kind.split(":")[1] : "";
        kind = kind.split(":")[0];
        osd.kind = kind;
        switch (kind) {
        case "pause":
            osd.show({ icon: player.paused ? "pause" : "play", value: player.paused ? qsTr("Paused") : qsTr("Playing") });
            break;
        case "position":
            osd.show({ label: source !== "" ? actions.text(source) : "",
                       value: Format.time(player.position) + " / " + Format.time(player.duration)
                              + (player.duration > 0 ? " (" + Math.round(100 * player.position / player.duration) + "%)" : "") });
            break;
        case "frame":
            osd.show({ label: qsTr("Frame"), value: Format.precise(player.position) });
            break;
        case "volume":
            osd.show({ label: qsTr("Volume"), value: Math.round(player.volume) + "%" });
            break;
        case "mute":
            osd.show({ icon: player.muted ? "volume-muted" : "volume", label: qsTr("Mute"),
                       value: player.muted ? qsTr("On") : qsTr("Off") });
            break;
        case "speed":
            osd.show({ icon: "playback", label: qsTr("Speed"),
                       value: Number(player.speed).toLocaleString(Qt.locale(), 'f', 2) + "×" });
            break;
        case "sub-delay":
            osd.show({ icon: "subtitles", label: qsTr("Subtitle sync"), value: qsTr("%1 s").arg(signed(player.subDelay, 1)) });
            break;
        case "audio":
            osd.show({ icon: "volume", label: qsTr("Audio"), value: trackName("audio") });
            break;
        case "subtitle":
            osd.show({ icon: "subtitles", label: qsTr("Subtitles"), value: trackName("sub") });
            break;
        case "sub-visibility":
            osd.show({ icon: "subtitles", label: qsTr("Subtitles"),
                       value: player.get("sub-visibility") ? qsTr("Shown") : qsTr("Hidden") });
            break;
        case "playlist":
            osd.show({ icon: "playlist", label: qsTr("Playlist"),
                       value: qsTr("%1 of %2").arg(player.playlistPosition + 1).arg(player.playlistCount) });
            break;
        case "playlist-edge":
            osd.show({ icon: "playlist", label: qsTr("Playlist"), value: qsTr("No more files") });
            break;
        case "shuffle":
            osd.show({ icon: "shuffle", label: qsTr("Playlist"), value: qsTr("Shuffled") });
            break;
        case "repeat":
            osd.show({ icon: "repeat", label: qsTr("Repeat"),
                       value: player.repeatMode === "file" ? qsTr("This file") : player.repeatMode === "playlist" ? qsTr("Playlist") : qsTr("Off") });
            break;
        case "ab-loop":
            var a = player.get("ab-loop-a"), b = player.get("ab-loop-b");
            osd.show({ icon: "repeat", label: qsTr("A-B repeat"),
                       value: typeof a !== "number" && typeof b !== "number" ? qsTr("Off") : moment(a) + " – " + moment(b) });
            break;
        case "chapter":
            var chapters = player.get("chapter-list") || [], at = player.get("chapter");
            var title = at >= 0 && at < chapters.length ? chapters[at].title : "";
            osd.show({ label: qsTr("Chapter"), value: (at + 1) + " / " + chapters.length + (title ? "  ·  " + title : "") });
            break;
        case "subtitle-line":
            var line = player.get("sub-text") || "";
            osd.show({ icon: "subtitles", label: Format.time(player.position), value: line.replace(/\n/g, " ") });
            break;
        case "subtitle-copied":
            osd.show({ label: qsTr("Subtitle"), value: qsTr("Copied") });
            break;
        case "subtitle-copy-none":
            osd.show({ label: qsTr("Subtitle"), value: qsTr("None on screen") });
            break;
        case "subtitle-line-none":
            osd.show({ label: qsTr("Subtitles"), value: qsTr("No more lines") });
            break;
        case "mirror":
            osd.show({ label: qsTr("Mirror"), value: String(player.get("vf") ? JSON.stringify(player.get("vf")) : "").indexOf("hflip") >= 0 ? qsTr("On") : qsTr("Off") });
            break;
        case "hue":
            osd.show({ label: qsTr("Hue"), value: percent(player.get("hue")) });
            break;
        case "normalize":
            osd.show({ label: qsTr("Even out loudness"), value: settings.values["audio/normalize"] ? qsTr("On") : qsTr("Off") });
            break;
        case "reopen-none":
            osd.show({ label: qsTr("Reopen"), value: qsTr("No recent file") });
            break;
        case "chapter-none":
            osd.show({ label: qsTr("Chapter"), value: qsTr("No more chapters") });
            break;
        case "audio-delay":
            osd.show({ icon: "volume", label: qsTr("Audio sync"), value: qsTr("%1 s").arg(signed(player.get("audio-delay"), 1)) });
            break;
        case "second-subtitle":
            var second = player.get("secondary-sid"), name = qsTr("Off");
            player.tracks.forEach(function (track) {
                if (track.type === "sub" && track.id === second)
                    name = Format.trackLabel(track, qsTr("Track %1").arg(track.id));
            });
            osd.show({ icon: "subtitles", label: qsTr("Second subtitle"), value: name });
            break;
        case "sub-pos":
            osd.show({ icon: "subtitles", label: qsTr("Subtitle height"), value: Math.round(100 - player.get("sub-pos")) + "%" });
            break;
        case "sub-scale":
            osd.show({ icon: "subtitles", label: qsTr("Subtitle size"), value: Math.round(player.get("sub-scale") * 100) + "%" });
            break;
        case "screenshot":
            var shot = player.lastScreenshot();
            osd.show({ icon: "video", label: qsTr("Saved"), value: shot.substring(shot.lastIndexOf("/") + 1), duration: 2500 });
            break;
        case "screenshot-copy":
            osd.show({ value: qsTr("Frame copied") });
            break;
        case "screenshot-failed":
            osd.show({ icon: "video", label: qsTr("Screenshot"), value: qsTr("Couldn't save it") });
            break;
        case "aspect":
            var aspect = player.get("video-aspect-override");
            osd.show({ icon: "video", label: qsTr("Aspect ratio"),
                       value: aspect === "no" || aspect === undefined || aspect < 0 ? qsTr("Original")
                            : ({ "1.778": "16:9", "1.333": "4:3", "2.350": "2.35:1" })[Number(aspect).toFixed(3)] || Number(aspect).toFixed(2) });
            break;
        case "fill":
            osd.show({ icon: "fullscreen", label: qsTr("Fill the screen"), value: player.get("panscan") > 0 ? qsTr("On") : qsTr("Off") });
            break;
        case "rotate":
            osd.show({ icon: "video", label: qsTr("Rotation"), value: player.get("video-rotate") + "°" });
            break;
        case "brightness":
            osd.show({ icon: "video", label: qsTr("Brightness"), value: percent(player.get("brightness")) });
            break;
        case "contrast":
            osd.show({ icon: "video", label: qsTr("Contrast"), value: percent(player.get("contrast")) });
            break;
        case "saturation":
            osd.show({ icon: "video", label: qsTr("Saturation"), value: percent(player.get("saturation")) });
            break;
        case "picture-reset":
            osd.show({ label: qsTr("Picture adjustments"), value: qsTr("Off") });
            break;
        case "picture-back":
            osd.show({ label: qsTr("Picture adjustments"), value: qsTr("Back") });
            break;
        // A file starting: its place in the playlist and its name, and where
        // it picks up when it resumes.
        case "file":
        case "resume":
            osd.show({ value: placeInPlaylist() + mediaName()
                              + (kind === "resume" ? "  ·  " + qsTr("Resumed %1").arg(Format.time(player.position)) : ""),
                       duration: 3000 });
            break;
        // Moving the mouse over the picture shows what's playing, until the
        // controls step aside.
        case "hover":
            osd.show({ value: placeInPlaylist() + mediaName(), duration: settings.values["interface/hideDelay"] * 1000 });
            break;
        case "opening":
            osd.show({ value: qsTr("Opening…"), duration: 15000 });
            break;
        case "clipboard-empty":
            osd.show({ label: qsTr("Clipboard"), value: qsTr("Nothing to open") });
            break;
        }
    }

    Connections {
        target: actions
        function onWindowAction(id) { win.runWindowAction(id); }
    }

    Connections {
        target: player
        function onOsd(kind) { win.showOsd(kind); }
        function onOpeningChanged() {
            if (player.opening)
                win.showOsd("opening");
            else if (osd.kind === "opening")
                osd.hide();
        }
        // Opening something from the start screen goes fullscreen, if asked.
        function onIdleChanged() {
            if (!player.idle && settings.values["interface/startFullscreen"] && !win.fullscreen)
                win.toggleFullscreen();
        }
        // The last file is over: hold it, show the start screen, or close.
        function onFinished() {
            if (settings.values["playback/atEnd"] === "quit")
                win.close();
        }
    }

    // One Shortcut per action, rebuilt when the keymap changes.
    Instantiator {
        model: actions.list
        delegate: Shortcut {
            required property var modelData
            sequences: modelData.keys
            context: Qt.ApplicationShortcut
            enabled: !win.menuOpen && !win.settingsOpen && !win.goToOpen
                     && (!win.sheetOpen || modelData.id === "shortcuts" || modelData.id === "quit")
            onActivated: actions.trigger(modelData.id)
        }
    }

    Timer {
        id: idleTimer
        interval: settings.values["interface/hideDelay"] * 1000
        onTriggered: win.pointerActive = false
    }

    Item {
        id: root
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        // Where the picture goes. mpv fills the whole window, and is told to
        // keep the video inside this rectangle.
        Item {
            id: videoArea
            anchors.left: parent.left
            anchors.right: win.playlistVisible ? playlistLoader.left : parent.right
            anchors.top: parent.top
            anchors.bottom: win.docked ? controlBar.top : parent.bottom

            function sync() {
                if (root.width <= 0 || root.height <= 0)
                    return;
                player.setVideoMargins(x / root.width, y / root.height,
                                       (root.width - x - width) / root.width,
                                       (root.height - y - height) / root.height);
            }

            onXChanged: sync()
            onYChanged: sync()
            onWidthChanged: sync()
            onHeightChanged: sync()
            Component.onCompleted: sync()

            Loader {
                anchors.fill: parent
                active: player.idle && !player.opening
                sourceComponent: IdleScreen {}
            }
        }

        MouseArea {
            id: pointer

            property point pressPoint
            property bool dragged: false
            property real wheelDelta: 0

            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
            cursorShape: win.pointerActive || player.idle || player.paused ? Qt.ArrowCursor : Qt.BlankCursor

            onPositionChanged: (mouse) => {
                win.wake();
                if (!player.idle && (!osd.showing || osd.kind === "hover" || osd.kind === "file"))
                    win.showOsd("hover");
                if (!pressed || dragged || win.fullscreen || !(mouse.buttons & Qt.LeftButton))
                    return;
                if (Math.abs(mouse.x - pressPoint.x) + Math.abs(mouse.y - pressPoint.y) > 6) {
                    dragged = true;
                    win.startSystemMove();
                }
            }
            onPressed: (mouse) => {
                pressPoint = Qt.point(mouse.x, mouse.y);
                dragged = false;
                win.wake();
            }
            onClicked: (mouse) => {
                if (mouse.button === Qt.RightButton) {
                    win.openMenu();
                } else if (mouse.button === Qt.MiddleButton) {
                    actions.trigger("mute");
                } else if (!dragged && !player.idle && settings.values["playback/clickPause"]) {
                    actions.trigger("play-pause");
                }
            }
            // The first click of the pair already paused; take it back.
            onDoubleClicked: (mouse) => {
                if (mouse.button !== Qt.LeftButton)
                    return;
                if (player.idle) {
                    picker.openMedia();
                    return;
                }
                if (settings.values["playback/clickPause"]) {
                    player.command(["cycle", "pause"]);
                    osd.hide();
                }
                win.toggleFullscreen();
            }
            onWheel: (wheel) => {
                win.wake();
                wheelDelta += wheel.angleDelta.y;
                var seeks = settings.values["playback/wheel"] === "seek";
                while (Math.abs(wheelDelta) >= 120) {
                    if (seeks)
                        actions.trigger(wheelDelta > 0 ? "seek-forward" : "seek-back");
                    else
                        actions.trigger(wheelDelta > 0 ? "volume-up" : "volume-down");
                    wheelDelta -= wheelDelta > 0 ? 120 : -120;
                }
            }
        }

        DropArea {
            anchors.fill: parent
            onDropped: (drop) => {
                if (drop.hasUrls) {
                    player.open(drop.urls);
                    drop.accept();
                }
            }
        }

        // Fullscreen only: the title on the left and the time of day on the
        // right, over a fade from the stage color.
        Item {
            id: topStrip
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 56
            visible: opacity > 0
            opacity: win.fullscreen && win.controlsShown && !player.idle ? 1 : 0

            Behavior on opacity {
                NumberAnimation { duration: 180 }
            }

            Rectangle {
                id: fade
                readonly property color tint: theme.stage
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0; color: Qt.rgba(fade.tint.r, fade.tint.g, fade.tint.b, 0.85) }
                    GradientStop { position: 1; color: Qt.rgba(fade.tint.r, fade.tint.g, fade.tint.b, 0) }
                }
            }

            Label {
                anchors.left: parent.left
                anchors.right: clock.left
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                anchors.top: parent.top
                anchors.topMargin: 14
                text: player.title
                elide: Text.ElideRight
                color: theme.stageForeground
                font.pixelSize: 13
            }

            Label {
                id: clock
                anchors.right: parent.right
                anchors.rightMargin: 18
                anchors.top: parent.top
                anchors.topMargin: 14
                color: theme.stageMuted
                font.pixelSize: 13

                Timer {
                    interval: 1000
                    running: topStrip.visible
                    repeat: true
                    triggeredOnStart: true
                    onTriggered: clock.text = new Date().toLocaleTimeString(Qt.locale(), Locale.ShortFormat)
                }
            }
        }

        Osd {
            id: osd
            x: 18
            maxWidth: parent.width - 36
            y: win.fullscreen && topStrip.opacity > 0 ? 50 : 18
        }

        Loader {
            active: win.infoVisible && !player.idle
            x: 18
            y: osd.y + osd.implicitHeight + Math.round(osd.size * 0.8)
            sourceComponent: PlaybackInfo {}
        }

        // Errors stay up until the next file plays or they're clicked away.
        Card {
            id: errorBanner
            visible: player.error !== ""
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: controlBar.top
            anchors.bottomMargin: 18
            width: Math.min(root.width - 48, errorText.implicitWidth + 36)
            height: errorText.implicitHeight + 32
            edge: theme.error

            Label {
                id: errorText
                anchors.centerIn: parent
                width: Math.min(implicitWidth, root.width - 88)
                text: player.error
                color: theme.stageForeground
                wrapMode: Text.WordWrap
                font.pixelSize: 14
                font.bold: true
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: player.clearError()
            }
        }

        Loader {
            id: playlistLoader
            active: win.playlistVisible
            visible: active
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: win.docked ? controlBar.top : parent.bottom
            width: Math.min(340, root.width * 0.38)
            sourceComponent: PlaylistPanel {
                onCloseRequested: win.playlistVisible = false
            }
        }

        ControlBar {
            id: controlBar
            anchors.left: parent.left
            anchors.right: parent.right
            y: win.controlsShown ? root.height - height : root.height
            floating: !win.docked
            fullscreen: win.fullscreen
            playlistOpen: win.playlistVisible
            opacity: win.controlsShown ? 1 : 0
            visible: opacity > 0
            onMenuRequested: (source) => win.openMenu(source)

            Behavior on y {
                enabled: !win.docked
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }
            Behavior on opacity {
                enabled: !win.docked
                NumberAnimation { duration: 180 }
            }
        }
    }

    Loader {
        id: menuLoader
        active: false
        sourceComponent: MasterMenu {}
    }

    Loader {
        id: sheetLoader
        active: false
        sourceComponent: ShortcutSheet {}
    }

    Loader {
        id: goToLoader
        active: false
        sourceComponent: GoToTime {}
    }

    Loader {
        id: settingsLoader
        active: false
        sourceComponent: SettingsPage {}
    }
}
