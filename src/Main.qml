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
    title: player.idle || player.title === "" ? "Omaplayer" : player.title + " — Omaplayer"

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
    readonly property bool docked: !fullscreen && settings.values["interface/controls"] !== "floating"
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
        case "exit-fullscreen":
            if (sheetOpen)
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
        }
    }

    function trackName(type) {
        var tracks = player.tracks;
        for (var i = 0; i < tracks.length; ++i) {
            var track = tracks[i];
            if (track.type === type && track.selected) {
                var parts = [track.title, track.lang].filter(function (part) { return part; });
                return parts.length > 0 ? parts.join(" · ") : qsTr("Track %1").arg(track.id);
            }
        }
        return qsTr("Off");
    }

    function signed(value, digits) {
        return (value > 0 ? "+" : "") + Number(value).toLocaleString(Qt.locale(), 'f', digits);
    }

    // What each action reports in the corner, read after mpv has applied it.
    function showOsd(kind) {
        switch (kind) {
        case "pause":
            osd.show(player.paused ? qsTr("Paused") : qsTr("Playing"));
            break;
        case "position":
            osd.show(qsTr("Seek"), Format.time(player.position) + " / " + Format.time(player.duration),
                     player.duration > 0 ? player.position / player.duration : -1);
            break;
        case "frame":
            osd.show(qsTr("Frame"), Format.precise(player.position));
            break;
        case "volume":
            osd.show(qsTr("Volume"), Math.round(player.volume) + "%",
                     player.volume / (settings.values["audio/boost"] ? 150 : 100));
            break;
        case "mute":
            osd.show(qsTr("Mute"), player.muted ? qsTr("On") : qsTr("Off"));
            break;
        case "speed":
            osd.show(qsTr("Speed"), Number(player.speed).toLocaleString(Qt.locale(), 'f', 2) + "×");
            break;
        case "sub-delay":
            osd.show(qsTr("Subtitle sync"), qsTr("%1 s").arg(signed(player.subDelay, 1)));
            break;
        case "audio":
            osd.show(qsTr("Audio"), trackName("audio"));
            break;
        case "subtitle":
            osd.show(qsTr("Subtitles"), trackName("sub"));
            break;
        case "sub-visibility":
            osd.show(qsTr("Subtitles"), player.get("sub-visibility") ? qsTr("Shown") : qsTr("Hidden"));
            break;
        case "playlist":
            osd.show(qsTr("Playlist"), qsTr("%1 of %2").arg(player.playlistPosition + 1).arg(player.playlistCount));
            break;
        case "playlist-edge":
            osd.show(qsTr("Playlist"), qsTr("No more files"));
            break;
        case "shuffle":
            osd.show(qsTr("Playlist"), qsTr("Shuffled"));
            break;
        case "repeat":
            osd.show(qsTr("Repeat"), player.repeat ? qsTr("On") : qsTr("Off"));
            break;
        case "resume":
            osd.show(qsTr("Resumed"), Format.time(player.position));
            break;
        case "clipboard-empty":
            osd.show(qsTr("Clipboard"), qsTr("Nothing to open"));
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
    }

    // One Shortcut per action, rebuilt when the keymap changes.
    Instantiator {
        model: actions.list
        delegate: Shortcut {
            required property var modelData
            sequences: modelData.keys
            context: Qt.ApplicationShortcut
            enabled: !win.menuOpen && !win.settingsOpen
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
            y: win.fullscreen && topStrip.opacity > 0 ? 50 : 18
        }

        Loader {
            active: win.infoVisible && !player.idle
            x: 18
            y: osd.y + osd.implicitHeight + 16
            sourceComponent: PlaybackInfo {}
        }

        // Errors stay up until the next file plays or they're clicked away.
        Rectangle {
            id: errorBanner
            visible: player.error !== ""
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: controlBar.top
            anchors.bottomMargin: 18
            width: Math.min(root.width - 48, errorText.implicitWidth + 40)
            height: errorText.implicitHeight + 20
            radius: Math.min(4, theme.radius)
            color: theme.stage
            border.width: 1
            border.color: theme.error

            Label {
                id: errorText
                anchors.centerIn: parent
                width: Math.min(implicitWidth, root.width - 88)
                text: player.error
                color: theme.stageForeground
                wrapMode: Text.WordWrap
                font.pixelSize: 13
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
        id: settingsLoader
        active: false
        sourceComponent: SettingsPage {}
    }
}
