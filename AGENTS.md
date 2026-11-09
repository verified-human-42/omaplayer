# Omaplayer

An Omarchy-native media player: C++ and Qt Quick around libmpv, built with
qmake6 into one binary, following the Omarchy theme and language live, and
installed as an Arch package. It's for every Omarchy user, so nothing may
assume one machine's monitors, GPU, theme, or files.

## The four rules

Check every feature against these.

1. **Habits carry over.** The default keys, the right-click master menu, the
   docked bar, the Tab info panel, wheel-for-volume, and click-to-pause are
   what Windows media player veterans already expect.
2. **The theme is the skin.** No colour is ever hard-coded in the interface.
   Everything comes from `Theme` (the current Omarchy `colors.toml`), and
   corners follow Hyprland's rounding. No yellow, no brand colours.
3. **Never slower than mpv.** We add a face, not a layer. Anything that costs
   frames is opt-in, and startup is measured (see below).
4. **Video first.** No splash and no nags. Controls step aside in fullscreen.

## Naming

The word "PotPlayer" appears only in `README.md` and in promotional copy.
Never use it in the app: not in strings, identifiers, settings values,
comments, the `.desktop` file, or the package metadata. The default keymap is
called "omaplayer"; the alternative is "mpv".

## Layout

```text
omaplayer.pro            qmake project (Qt + pkg-config mpv)
bin/build bin/test bin/install
src/main.cpp             app setup; hands C++ objects to QML
src/player.{h,cpp}       the libmpv handle: state as properties, async commands, folder playlist
src/videorenderer.{h,cpp} draws mpv into the window's own framebuffer, under the QML scene
src/actions.{h,cpp}      the action table: every action, both keymaps, menu groups
src/settings.{h,cpp}     preferences: stored, applied to mpv, undoable per session; key bindings
src/textencoding.{h,cpp} subtitle bytes → text: BOM/UTF-8/UTF-16, then every legacy codepage scored
src/subtitles.{h,cpp}    finds subtitle files beside a video, decodes them to UTF-8 in the cache
src/sami.{h,cpp}         SMI files: one SRT per language class (KRCC, ENCC, …)
src/theme.{h,cpp}        Omarchy palette + Hyprland rounding, followed live
src/translator.{h,cpp}   Omarchy language, followed live; catalogs in src/i18n/
src/portalfilepicker.*   open dialogs through xdg-desktop-portal
src/Main.qml             the window: layout, shortcuts, mouse, OSD wording
src/*.qml                one component per file (ControlBar, SeekBar, Osd, IdleScreen,
                         PlaylistPanel, PlaybackInfo, MasterMenu, ShortcutSheet, SettingsPage…)
src/resources.qrc        every QML file, icon, and catalog, compiled in
tests/                   Qt Test suite, offscreen
pkgbuild/                PKGBUILD, .desktop, icon
```

## How the pieces fit

- **Rendering.** Qt Quick is forced onto OpenGL. `VideoRenderer` creates the
  mpv render context on `sceneGraphInitialized` and renders in
  `beforeRenderPassRecording` into whatever framebuffer Qt has bound, so the
  video sits underneath the controls with no intermediate texture copy.
  Files asked for before the context exists wait in `Player` (`setVideoReady`).
- **Placement.** mpv always fills the window. Docked controls and the playlist
  panel move the picture with `video-margin-ratio-*` (see `videoArea.sync()`),
  instead of resizing a video item.
- **Actions.** Add a row to `Actions::specs()` and the feature is in the
  keymaps, the master menu, the shortcut sheet, the idle cheat sheet, and the
  tooltips. `Kind::Mpv` rows are plain mpv commands; their `osd` names what the
  corner message reports once mpv has applied the command.
- **Settings.** Add a `Spec` in `Settings::specs()`, apply it in
  `Settings::apply()` (or read it from QML via `settings.values[key]`), and add
  a row on the matching page in `SettingsPage.qml`.
- **Subtitles.** mpv never looks for or decodes subtitle files (`sub-auto=no`,
  `sub-codepage=utf-8`). On `FILE_LOADED`, `Player::loadSubtitles` finds them
  (`Subtitles::besides`), `Subtitles::prepare` decodes each with
  `TextEncoding::decode` (SMI through `Sami::convert`) into
  `~/.cache/omaplayer/subtitles`, and they're added with `sub-add`, the one in
  the preferred language selected. The detector's tests are the spec: add a
  row to `encodingsAreDetected` for any file that ever decodes wrong.
  `OMAPLAYER_DEBUG_ENCODING=1` prints every candidate's score.
- **Language.** `Translator` follows Omarchy's language unless Settings ›
  Language overrides it (`interface/language`); subtitle preferences follow
  whichever is in effect. Catalogs: `src/i18n/<code>.json`, keyed by English.
- **Startup.** Menus, sheets, settings, the info panel, the playlist, and
  tooltips load on first use. mpv's built-in Lua scripts are off; the yt-dlp
  hook loads 1.5 s after launch, or at once when a URL is opened.

## Design language

- Everything is set in the Omarchy font (`monospace`).
- Stage colours (`theme.stage*`) for anything on or beside the picture: the bar,
  OSD, info panel, playlist, and idle screen. They're always the palette's darkest
  surface, even in light themes. Menus, sheets, and settings follow the theme
  as it is.
- The settings page copies OmaControl's layout: header strip with changed count
  and Undo all, a sidebar with search and section headers, framed cards, and a
  keycap footer. Colours are alphas over `theme.foreground` (0.04 sidebar,
  0.03 cards, 0.10 dividers, 0.07 chrome, 0.6 muted).
- Accent sparingly: progress, the playing row, toggles that are on, selection.

## Loop

1. `bin/build`, then `build/omaplayer FILE`.
2. `bin/test` after any backend change. Tests use `Player::Output::None`
   (decode without a window) and generate clips with ffmpeg.
3. Look at it. The offscreen platform has no OpenGL, so offscreen screenshots
   go through a harness that uses `Player::Output::None` and grabs the window
   (no picture, everything else real). Real video needs a real screen.
4. Startup timing: `QT_LOGGING_RULES="omaplayer.timing.info=true" build/omaplayer FILE`
   prints `main`, `qml loaded`, `render context`, `file loaded`, and `first frame` timestamps.
   mpv's own log: `OMAPLAYER_MPV_LOG=v`. Extra mpv options: `OMAPLAYER_MPV_OPTIONS="a=b;c=d"`.
   A real frame, video included, without a screen capture tool:
   `OMAPLAYER_GRAB=shot.png:3000 build/omaplayer FILE` saves the window after 3 s and quits.
5. `bin/install` builds and installs the package so it's in the launcher.

## OpenGL trap met here

Qt leaves its own GL state behind between frames (blending above all). mpv
assumes defaults, so `VideoRenderer::render()` resets blend, depth, stencil,
scissor, cull, and the colour mask before every `mpv_render_context_render`.
Without it the picture is black while playing and off-colour when paused,
while mpv's background still draws, which makes it look like a sizing bug.

## Qt Quick traps met here

- A `TextInput`'s `implicitHeight` is read-only; wrap it in an Item.
- JS objects in a Repeater model lose identity; compare rows by `key`.
- `Qt.color()` doesn't exist; take `r/g/b` from a typed `color` property.
- `theme.*` are strings; use a `color` property before reading `hslLightness`.
