#include "actions.h"

#include <QCoreApplication>
#include <QKeySequence>

#include "player.h"

namespace {
const char *groupText(const QString &group) {
    if (group == "file") return QT_TRANSLATE_NOOP("Actions", "File");
    if (group == "playback") return QT_TRANSLATE_NOOP("Actions", "Playback");
    if (group == "seek") return QT_TRANSLATE_NOOP("Actions", "Seek");
    if (group == "audio") return QT_TRANSLATE_NOOP("Actions", "Audio");
    if (group == "subtitles") return QT_TRANSLATE_NOOP("Actions", "Subtitles");
    if (group == "video") return QT_TRANSLATE_NOOP("Actions", "Video");
    if (group == "screen") return QT_TRANSLATE_NOOP("Actions", "Screen");
    return QT_TRANSLATE_NOOP("Actions", "OmaPlayer");
}

QString translated(const char *text) {
    return QCoreApplication::translate("Actions", text);
}
}

// Keys as they're printed on the keyboard: arrows as arrows, either Enter as
// Enter, and modifiers spelled the platform's way.
QString Actions::keyLabel(const QString &key) {
    // Portable text: the native one runs key names through the app's catalog,
    // and a catalog that translates "Left" turns the keycap into a word.
    QString label = QKeySequence(key).toString(QKeySequence::PortableText);
    static const QList<QPair<QString, QString>> glyphs = {
        {"Left", "←"}, {"Right", "→"}, {"Up", "↑"}, {"Down", "↓"}, {"Return", "Enter"},
    };
    for (const auto &[name, glyph] : glyphs)
        if (label == name || label.endsWith("+" + name))
            label = label.left(label.size() - name.size()) + glyph;
    return label;
}

Actions::Actions(Player *player, QObject *parent) : QObject(parent), m_player(player) {}

// The first key column is OmaPlayer's own layout: the keys Windows media
// player veterans already have in their fingers, plus the ones every Linux
// app answers to (Ctrl+, Ctrl+Q, F11, Ctrl+L). The second follows mpv's
// default input.conf, for people coming from plain mpv.
const QList<Actions::Spec> &Actions::specs() {
    using K = Kind;
    static const QList<Spec> table = {
        {"open-file", "file", QT_TRANSLATE_NOOP("Actions", "Open File…"), {"Ctrl+O"}, {"Ctrl+O"}, K::Window, {}, {}},
        {"open-clipboard", "file", QT_TRANSLATE_NOOP("Actions", "Open from Clipboard"), {"Ctrl+V"}, {"Ctrl+V"}, K::Player, {}, {}},
        {"reopen-last", "file", QT_TRANSLATE_NOOP("Actions", "Reopen Last File"), {"Ctrl+Y"}, {}, K::Window, {}, {}, false},
        {"previous-file", "file", QT_TRANSLATE_NOOP("Actions", "Previous File"), {"PgUp"}, {"<"}, K::Mpv, {"playlist-prev"}, "playlist"},
        {"next-file", "file", QT_TRANSLATE_NOOP("Actions", "Next File"), {"PgDown"}, {">"}, K::Mpv, {"playlist-next"}, "playlist"},
        {"playlist", "file", QT_TRANSLATE_NOOP("Actions", "Playlist"), {"F6", "Ctrl+L"}, {"F8"}, K::Window, {}, {}},
        {"shuffle", "file", QT_TRANSLATE_NOOP("Actions", "Shuffle Playlist"), {}, {}, K::Mpv, {"playlist-shuffle"}, "shuffle"},
        {"repeat", "file", QT_TRANSLATE_NOOP("Actions", "Repeat: Off, One, All"), {}, {"Shift+L"}, K::Player, {}, {}},
        {"stop", "file", QT_TRANSLATE_NOOP("Actions", "Close File"), {"F4"}, {}, K::Player, {}, {}},
        {"quit", "file", QT_TRANSLATE_NOOP("Actions", "Quit"), {"Ctrl+Q"}, {"Q", "Ctrl+Q"}, K::Window, {}, {}},

        {"play-pause", "playback", QT_TRANSLATE_NOOP("Actions", "Play / Pause"), {"Space"}, {"Space"}, K::Player, {}, {}},
        {"frame-back", "playback", QT_TRANSLATE_NOOP("Actions", "Previous Frame"), {"D"}, {","}, K::Mpv, {"frame-back-step"}, "frame"},
        {"frame-forward", "playback", QT_TRANSLATE_NOOP("Actions", "Next Frame"), {"F"}, {"."}, K::Mpv, {"frame-step"}, "frame"},
        {"speed-down", "playback", QT_TRANSLATE_NOOP("Actions", "Slower"), {"X"}, {"["}, K::Mpv, {"add", "speed", "-0.1"}, "speed"},
        {"speed-up", "playback", QT_TRANSLATE_NOOP("Actions", "Faster"), {"C"}, {"]"}, K::Mpv, {"add", "speed", "0.1"}, "speed"},
        {"speed-reset", "playback", QT_TRANSLATE_NOOP("Actions", "Normal Speed"), {"Z"}, {"Backspace"}, K::Mpv, {"set", "speed", "1"}, "speed"},
        {"loop-start", "playback", QT_TRANSLATE_NOOP("Actions", "A-B Repeat: Set Start"), {"["}, {}, K::Player, {}, {}, false},
        {"loop-end", "playback", QT_TRANSLATE_NOOP("Actions", "A-B Repeat: Set End"), {"]"}, {}, K::Player, {}, {}, false},
        {"loop-clear", "playback", QT_TRANSLATE_NOOP("Actions", "A-B Repeat: Off"), {"\\"}, {"L"}, K::Player, {}, {}, false},

        {"seek-back", "seek", QT_TRANSLATE_NOOP("Actions", "Back %1 Seconds"), {"Left"}, {"Left"}, K::Mpv, {"seek", "-step"}, "position"},
        {"seek-forward", "seek", QT_TRANSLATE_NOOP("Actions", "Forward %1 Seconds"), {"Right"}, {"Right"}, K::Mpv, {"seek", "step"}, "position"},
        {"seek-back-long", "seek", QT_TRANSLATE_NOOP("Actions", "Back %1 Seconds"), {"Ctrl+Left"}, {}, K::Mpv, {"seek", "-long"}, "position"},
        {"seek-forward-long", "seek", QT_TRANSLATE_NOOP("Actions", "Forward %1 Seconds"), {"Ctrl+Right"}, {}, K::Mpv, {"seek", "long"}, "position"},
        {"seek-back-minute", "seek", QT_TRANSLATE_NOOP("Actions", "Back 1 Minute"), {"Shift+Left"}, {"Down"}, K::Mpv, {"seek", "-60"}, "position", false},
        {"seek-forward-minute", "seek", QT_TRANSLATE_NOOP("Actions", "Forward 1 Minute"), {"Shift+Right"}, {"Up"}, K::Mpv, {"seek", "60"}, "position", false},
        {"seek-back-5-minutes", "seek", QT_TRANSLATE_NOOP("Actions", "Back 5 Minutes"), {"Ctrl+Alt+Left"}, {}, K::Mpv, {"seek", "-300"}, "position", false},
        {"seek-forward-5-minutes", "seek", QT_TRANSLATE_NOOP("Actions", "Forward 5 Minutes"), {"Ctrl+Alt+Right"}, {}, K::Mpv, {"seek", "300"}, "position", false},
        {"seek-start", "seek", QT_TRANSLATE_NOOP("Actions", "To the Start"), {"Backspace"}, {}, K::Mpv, {"seek", "0", "absolute"}, "position", false},
        {"seek-middle", "seek", QT_TRANSLATE_NOOP("Actions", "To the Middle"), {"Ctrl+Backspace"}, {}, K::Mpv, {"seek", "50", "absolute-percent"}, "position", false},
        {"seek-near-end", "seek", QT_TRANSLATE_NOOP("Actions", "To 30 Seconds Before the End"), {"Shift+Backspace"}, {}, K::Mpv, {"seek", "-30", "absolute"}, "position", false},
        {"go-to-time", "seek", QT_TRANSLATE_NOOP("Actions", "Go to Time…"), {"G", "Ctrl+G"}, {}, K::Window, {}, {}, false},
        {"chapter-previous", "seek", QT_TRANSLATE_NOOP("Actions", "Previous Chapter"), {"Ctrl+PgUp"}, {"PgDown"}, K::Mpv, {"add", "chapter", "-1"}, "chapter", false},
        {"chapter-next", "seek", QT_TRANSLATE_NOOP("Actions", "Next Chapter"), {"Ctrl+PgDown"}, {"PgUp"}, K::Mpv, {"add", "chapter", "1"}, "chapter", false},

        {"next-subtitle", "subtitles", QT_TRANSLATE_NOOP("Actions", "Next Subtitle Track"), {"L"}, {"J"}, K::Mpv, {"cycle", "sub"}, "subtitle"},
        {"toggle-subtitles", "subtitles", QT_TRANSLATE_NOOP("Actions", "Show / Hide Subtitles"), {"Alt+H"}, {"V"}, K::Mpv, {"cycle", "sub-visibility"}, "sub-visibility"},
        {"next-second-subtitle", "subtitles", QT_TRANSLATE_NOOP("Actions", "Next Second Subtitle"), {}, {}, K::Mpv, {"cycle", "secondary-sid"}, "second-subtitle", false},
        {"sub-line-previous", "subtitles", QT_TRANSLATE_NOOP("Actions", "Previous Subtitle Line"), {"Home"}, {"Ctrl+Left"}, K::Mpv, {"sub-seek", "-1"}, "subtitle-line", false},
        {"sub-line-next", "subtitles", QT_TRANSLATE_NOOP("Actions", "Next Subtitle Line"), {"End"}, {"Ctrl+Right"}, K::Mpv, {"sub-seek", "1"}, "subtitle-line", false},
        {"sub-line-replay", "subtitles", QT_TRANSLATE_NOOP("Actions", "Replay This Subtitle Line"), {"Ctrl+Home"}, {}, K::Mpv, {"sub-seek", "0"}, "subtitle-line", false},
        {"sub-earlier", "subtitles", QT_TRANSLATE_NOOP("Actions", "Subtitles 0.5 s Earlier"), {","}, {"Z"}, K::Mpv, {"add", "sub-delay", "-0.5"}, "sub-delay"},
        {"sub-later", "subtitles", QT_TRANSLATE_NOOP("Actions", "Subtitles 0.5 s Later"), {"."}, {"Shift+Z"}, K::Mpv, {"add", "sub-delay", "0.5"}, "sub-delay"},
        {"sub-sync-reset", "subtitles", QT_TRANSLATE_NOOP("Actions", "Reset Subtitle Sync"), {"/"}, {}, K::Mpv, {"set", "sub-delay", "0"}, "sub-delay", false},
        {"sub-up", "subtitles", QT_TRANSLATE_NOOP("Actions", "Move Subtitles Up"), {"Alt+Up"}, {"R"}, K::Mpv, {"add", "sub-pos", "-1"}, "sub-pos", false},
        {"sub-down", "subtitles", QT_TRANSLATE_NOOP("Actions", "Move Subtitles Down"), {"Alt+Down"}, {"Shift+R"}, K::Mpv, {"add", "sub-pos", "1"}, "sub-pos", false},
        {"sub-position-reset", "subtitles", QT_TRANSLATE_NOOP("Actions", "Subtitles Back in Place"), {"Alt+Home"}, {}, K::Mpv, {"set", "sub-pos", "100"}, "sub-pos", false},
        {"sub-bigger", "subtitles", QT_TRANSLATE_NOOP("Actions", "Bigger Subtitles"), {"Alt+PgUp"}, {}, K::Mpv, {"add", "sub-scale", "0.1"}, "sub-scale", false},
        {"sub-smaller", "subtitles", QT_TRANSLATE_NOOP("Actions", "Smaller Subtitles"), {"Alt+PgDown"}, {}, K::Mpv, {"add", "sub-scale", "-0.1"}, "sub-scale", false},

        {"screenshot", "video", QT_TRANSLATE_NOOP("Actions", "Save Screenshot"), {"Ctrl+E", "K"}, {"S"}, K::Player, {}, {}, false},
        {"copy-frame", "video", QT_TRANSLATE_NOOP("Actions", "Copy Frame"), {"Ctrl+C"}, {}, K::Player, {}, {}, false},
        {"aspect", "video", QT_TRANSLATE_NOOP("Actions", "Aspect Ratio"), {"Ctrl+F6"}, {"A"}, K::Mpv, {"cycle-values", "video-aspect-override", "16:9", "4:3", "2.35:1", "no"}, "aspect", false},
        {"fill", "video", QT_TRANSLATE_NOOP("Actions", "Fill the Screen"), {"Ctrl+Return", "Ctrl+Enter"}, {}, K::Mpv, {"cycle-values", "panscan", "1", "0"}, "fill", false},
        {"rotate", "video", QT_TRANSLATE_NOOP("Actions", "Rotate"), {}, {}, K::Mpv, {"cycle-values", "video-rotate", "90", "180", "270", "0"}, "rotate", false},
        {"mirror", "video", QT_TRANSLATE_NOOP("Actions", "Mirror"), {"Ctrl+Z"}, {}, K::Mpv, {"vf", "toggle", "hflip"}, "mirror", false},
        {"picture-reset", "video", QT_TRANSLATE_NOOP("Actions", "Reset Picture"), {"Q"}, {}, K::Player, {}, {}, false},
        {"brightness-down", "video", QT_TRANSLATE_NOOP("Actions", "Darker"), {"W"}, {"3"}, K::Mpv, {"add", "brightness", "-1"}, "brightness", false},
        {"brightness-up", "video", QT_TRANSLATE_NOOP("Actions", "Brighter"), {"E"}, {"4"}, K::Mpv, {"add", "brightness", "1"}, "brightness", false},
        {"contrast-down", "video", QT_TRANSLATE_NOOP("Actions", "Less Contrast"), {"R"}, {"1"}, K::Mpv, {"add", "contrast", "-1"}, "contrast", false},
        {"contrast-up", "video", QT_TRANSLATE_NOOP("Actions", "More Contrast"), {"T"}, {"2"}, K::Mpv, {"add", "contrast", "1"}, "contrast", false},
        {"saturation-down", "video", QT_TRANSLATE_NOOP("Actions", "Less Saturation"), {"Y"}, {"7"}, K::Mpv, {"add", "saturation", "-1"}, "saturation", false},
        {"saturation-up", "video", QT_TRANSLATE_NOOP("Actions", "More Saturation"), {"U"}, {"8"}, K::Mpv, {"add", "saturation", "1"}, "saturation", false},
        {"hue-down", "video", QT_TRANSLATE_NOOP("Actions", "Hue Down"), {"I"}, {}, K::Mpv, {"add", "hue", "-1"}, "hue", false},
        {"hue-up", "video", QT_TRANSLATE_NOOP("Actions", "Hue Up"), {"O"}, {}, K::Mpv, {"add", "hue", "1"}, "hue", false},

        {"volume-up", "audio", QT_TRANSLATE_NOOP("Actions", "Volume Up"), {"Up"}, {"0", "*"}, K::Mpv, {"add", "volume", "5"}, "volume"},
        {"volume-down", "audio", QT_TRANSLATE_NOOP("Actions", "Volume Down"), {"Down"}, {"9", "/"}, K::Mpv, {"add", "volume", "-5"}, "volume"},
        {"mute", "audio", QT_TRANSLATE_NOOP("Actions", "Mute"), {"M"}, {"M"}, K::Mpv, {"cycle", "mute"}, "mute"},
        {"next-audio", "audio", QT_TRANSLATE_NOOP("Actions", "Next Audio Track"), {"A"}, {"#"}, K::Mpv, {"cycle", "audio"}, "audio"},
        {"audio-earlier", "audio", QT_TRANSLATE_NOOP("Actions", "Audio 0.05 s Earlier"), {"<"}, {"Ctrl+-"}, K::Mpv, {"add", "audio-delay", "-0.05"}, "audio-delay", false},
        {"audio-later", "audio", QT_TRANSLATE_NOOP("Actions", "Audio 0.05 s Later"), {">"}, {"Ctrl++"}, K::Mpv, {"add", "audio-delay", "0.05"}, "audio-delay", false},
        {"audio-sync-reset", "audio", QT_TRANSLATE_NOOP("Actions", "Reset Audio Sync"), {}, {}, K::Mpv, {"set", "audio-delay", "0"}, "audio-delay", false},
        {"normalize", "audio", QT_TRANSLATE_NOOP("Actions", "Even Out Loudness"), {"Shift+N"}, {}, K::Window, {}, {}, false},

        {"fullscreen", "screen", QT_TRANSLATE_NOOP("Actions", "Fullscreen"), {"Return", "Enter", "Alt+Return", "F11"}, {"F", "F11"}, K::Window, {}, {}},
        {"exit-fullscreen", "screen", QT_TRANSLATE_NOOP("Actions", "Exit Fullscreen"), {"Esc"}, {"Esc"}, K::Window, {}, {}},
        {"info", "screen", QT_TRANSLATE_NOOP("Actions", "Playback Info"), {"Tab", "Ctrl+I", "Ctrl+F1"}, {"I"}, K::Window, {}, {}},
        {"picture-in-picture", "screen", QT_TRANSLATE_NOOP("Actions", "Picture in Picture"), {"P"}, {}, K::Window, {}, {}},

        {"settings", "help", QT_TRANSLATE_NOOP("Actions", "Settings…"), {"F5", "Ctrl+,"}, {"Ctrl+,"}, K::Window, {}, {}},
        {"shortcuts", "help", QT_TRANSLATE_NOOP("Actions", "Keyboard Shortcuts"), {"?", "F1", "Ctrl+/"}, {"?", "F1"}, K::Window, {}, {}},
    };
    return table;
}

QStringList Actions::keysFor(const Spec &spec, const QString &keymap) {
    return keymap == "mpv" ? spec.mpvKeys : spec.keys;
}

void Actions::setKeymap(const QString &keymap) {
    const QString next = keymaps().contains(keymap) ? keymap : QString("omaplayer");
    if (next == m_keymap)
        return;
    m_keymap = next;
    emit changed();
}

void Actions::setSeekSteps(int step, int longStep) {
    if (step == m_seekStep && longStep == m_seekLongStep)
        return;
    m_seekStep = step;
    m_seekLongStep = longStep;
    emit changed();
}

QString Actions::textFor(const Spec &spec) const {
    const QString text = translated(spec.text);
    if (spec.id == "seek-back" || spec.id == "seek-forward")
        return text.arg(m_seekStep);
    if (spec.id == "seek-back-long" || spec.id == "seek-forward-long")
        return text.arg(m_seekLongStep);
    return text;
}

// The seek rows say "step" and "long" for the steps chosen in Settings.
QStringList Actions::commandFor(const Spec &spec) const {
    QStringList command = spec.command;
    for (auto &part : command) {
        const QString sign = part.startsWith('-') ? "-" : "";
        const QString word = part.mid(sign.size());
        if (word == "step")
            part = sign + QString::number(m_seekStep);
        else if (word == "long")
            part = sign + QString::number(m_seekLongStep);
    }
    return command;
}

void Actions::setOverrides(const QHash<QString, QStringList> &overrides) {
    if (overrides == m_overrides)
        return;
    m_overrides = overrides;
    emit changed();
}

QStringList Actions::keys(const QString &id) const {
    if (m_overrides.contains(id))
        return m_overrides.value(id);
    for (const auto &spec : specs())
        if (spec.id == id)
            return keysFor(spec, m_keymap);
    return {};
}

QString Actions::idForKey(const QString &key) const {
    const QString wanted = QKeySequence(key).toString(QKeySequence::PortableText);
    for (const auto &spec : specs())
        for (const auto &bound : keys(spec.id))
            if (QKeySequence(bound).toString(QKeySequence::PortableText) == wanted)
                return spec.id;
    return {};
}

QString Actions::sequence(int key, int modifiers, const QString &text) {
    switch (key) {
    case Qt::Key_Control: case Qt::Key_Shift: case Qt::Key_Alt: case Qt::Key_Meta:
    case Qt::Key_AltGr: case Qt::Key_Super_L: case Qt::Key_Super_R: case Qt::Key_unknown:
        return {};
    default:
        break;
    }
    auto mods = Qt::KeyboardModifiers(modifiers) & (Qt::ControlModifier | Qt::ShiftModifier | Qt::AltModifier | Qt::MetaModifier);
    // A symbol typed with Shift ("?", "<") is that symbol, the way the
    // built-in layouts write it, not Shift plus the symbol.
    if (text.size() == 1 && !text.at(0).isLetterOrNumber() && text.at(0).isPrint() && !text.at(0).isSpace())
        mods &= ~Qt::ShiftModifier;
    return QKeySequence(QKeyCombination(mods, Qt::Key(key))).toString(QKeySequence::PortableText);
}

QVariantList Actions::list() const {
    QVariantList list;
    for (const auto &spec : specs()) {
        const QStringList keys = this->keys(spec.id);
        QStringList labels;
        for (const auto &key : keys) {
            const QString label = keyLabel(key);
            if (!labels.contains(label))
                labels.append(label);
        }
        list.append(QVariantMap{
            {"id", spec.id},
            {"group", spec.group},
            {"text", textFor(spec)},
            {"everyday", spec.everyday},
            {"keys", keys},
            {"keyLabels", labels},
            {"custom", m_overrides.contains(spec.id)},
        });
    }
    return list;
}

QVariantList Actions::groups() const {
    QVariantList groups;
    QStringList seen;
    for (const auto &spec : specs()) {
        if (seen.contains(spec.group))
            continue;
        seen.append(spec.group);
        groups.append(QVariantMap{{"id", spec.group}, {"text", translated(groupText(spec.group))}});
    }
    return groups;
}

QString Actions::shortcut(const QString &id) const {
    const QStringList bound = keys(id);
    return bound.isEmpty() ? QString() : keyLabel(bound.first());
}

QString Actions::text(const QString &id) const {
    for (const auto &spec : specs())
        if (spec.id == id)
            return textFor(spec);
    return {};
}

void Actions::trigger(const QString &id) {
    for (const auto &spec : specs()) {
        if (spec.id != id)
            continue;
        switch (spec.kind) {
        case Kind::Mpv: {
            QVariantList command;
            for (const auto &part : commandFor(spec))
                command.append(part);
            // Seeks say which seek in the corner ("position:seek-forward").
            m_player->command(command, spec.osd == "position" ? "position:" + spec.id : spec.osd);
            break;
        }
        case Kind::Player:
            if (id == "play-pause")
                m_player->togglePause();
            else if (id == "stop")
                m_player->stop();
            else if (id == "open-clipboard" && !m_player->openClipboard())
                emit m_player->osd("clipboard-empty");
            else if (id == "repeat")
                m_player->cycleRepeat();
            else if (id == "loop-start" || id == "loop-end" || id == "loop-clear")
                m_player->setLoopPoint(id == "loop-start" ? "a" : id == "loop-end" ? "b" : "clear");
            else if (id == "screenshot" || id == "copy-frame")
                m_player->screenshot(id == "copy-frame");
            else if (id == "picture-reset")
                m_player->resetPicture();
            break;
        case Kind::Window:
            emit windowAction(id);
            break;
        }
        return;
    }
}
