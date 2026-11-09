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
    if (group == "screen") return QT_TRANSLATE_NOOP("Actions", "Screen");
    return QT_TRANSLATE_NOOP("Actions", "Omaplayer");
}

QString translated(const char *text) {
    return QCoreApplication::translate("Actions", text);
}

// Keys as they're printed on the keyboard: arrows as arrows, either Enter as
// Enter, and modifiers spelled the platform's way.
QString keyLabel(const QString &key) {
    QString label = QKeySequence(key).toString(QKeySequence::NativeText);
    static const QList<QPair<QString, QString>> glyphs = {
        {"Left", "←"}, {"Right", "→"}, {"Up", "↑"}, {"Down", "↓"}, {"Return", "Enter"},
    };
    for (const auto &[name, glyph] : glyphs)
        if (label == name || label.endsWith("+" + name))
            label = label.left(label.size() - name.size()) + glyph;
    return label;
}
}

Actions::Actions(Player *player, QObject *parent) : QObject(parent), m_player(player) {}

// The first key column is Omaplayer's own layout, the one Windows media
// player veterans already have in their fingers; the second follows mpv's
// default input.conf, for people coming from plain mpv.
const QList<Actions::Spec> &Actions::specs() {
    using K = Kind;
    static const QList<Spec> table = {
        {"open-file", "file", QT_TRANSLATE_NOOP("Actions", "Open File…"), {"Ctrl+O"}, {"Ctrl+O"}, K::Window, {}, {}},
        {"open-clipboard", "file", QT_TRANSLATE_NOOP("Actions", "Open from Clipboard"), {"Ctrl+V"}, {"Ctrl+V"}, K::Player, {}, {}},
        {"previous-file", "file", QT_TRANSLATE_NOOP("Actions", "Previous File"), {"PgUp"}, {"<"}, K::Mpv, {"playlist-prev"}, "playlist"},
        {"next-file", "file", QT_TRANSLATE_NOOP("Actions", "Next File"), {"PgDown"}, {">"}, K::Mpv, {"playlist-next"}, "playlist"},
        {"playlist", "file", QT_TRANSLATE_NOOP("Actions", "Playlist"), {"F6"}, {"F8"}, K::Window, {}, {}},
        {"shuffle", "file", QT_TRANSLATE_NOOP("Actions", "Shuffle Playlist"), {}, {}, K::Mpv, {"playlist-shuffle"}, "shuffle"},
        {"repeat", "file", QT_TRANSLATE_NOOP("Actions", "Repeat Playlist"), {}, {"Shift+L"}, K::Mpv, {"cycle-values", "loop-playlist", "inf", "no"}, "repeat"},
        {"quit", "file", QT_TRANSLATE_NOOP("Actions", "Quit"), {"Q"}, {"Q"}, K::Window, {}, {}},

        {"play-pause", "playback", QT_TRANSLATE_NOOP("Actions", "Play / Pause"), {"Space"}, {"Space"}, K::Player, {}, {}},
        {"stop", "playback", QT_TRANSLATE_NOOP("Actions", "Stop"), {}, {}, K::Player, {}, {}},
        {"frame-back", "playback", QT_TRANSLATE_NOOP("Actions", "Previous Frame"), {"D"}, {","}, K::Mpv, {"frame-back-step"}, "frame"},
        {"frame-forward", "playback", QT_TRANSLATE_NOOP("Actions", "Next Frame"), {"F"}, {"."}, K::Mpv, {"frame-step"}, "frame"},
        {"speed-down", "playback", QT_TRANSLATE_NOOP("Actions", "Slower"), {"X"}, {"["}, K::Mpv, {"add", "speed", "-0.1"}, "speed"},
        {"speed-up", "playback", QT_TRANSLATE_NOOP("Actions", "Faster"), {"C"}, {"]"}, K::Mpv, {"add", "speed", "0.1"}, "speed"},
        {"speed-reset", "playback", QT_TRANSLATE_NOOP("Actions", "Normal Speed"), {"Z"}, {"Backspace"}, K::Mpv, {"set", "speed", "1"}, "speed"},

        {"seek-back", "seek", QT_TRANSLATE_NOOP("Actions", "Back 5 Seconds"), {"Left"}, {"Left"}, K::Mpv, {"seek", "-5"}, "position"},
        {"seek-forward", "seek", QT_TRANSLATE_NOOP("Actions", "Forward 5 Seconds"), {"Right"}, {"Right"}, K::Mpv, {"seek", "5"}, "position"},
        {"seek-back-long", "seek", QT_TRANSLATE_NOOP("Actions", "Back 30 Seconds"), {"Ctrl+Left"}, {}, K::Mpv, {"seek", "-30"}, "position"},
        {"seek-forward-long", "seek", QT_TRANSLATE_NOOP("Actions", "Forward 30 Seconds"), {"Ctrl+Right"}, {}, K::Mpv, {"seek", "30"}, "position"},
        {"seek-back-minute", "seek", QT_TRANSLATE_NOOP("Actions", "Back 1 Minute"), {}, {"Down"}, K::Mpv, {"seek", "-60"}, "position"},
        {"seek-forward-minute", "seek", QT_TRANSLATE_NOOP("Actions", "Forward 1 Minute"), {}, {"Up"}, K::Mpv, {"seek", "60"}, "position"},

        {"volume-up", "audio", QT_TRANSLATE_NOOP("Actions", "Volume Up"), {"Up"}, {"0", "*"}, K::Mpv, {"add", "volume", "5"}, "volume"},
        {"volume-down", "audio", QT_TRANSLATE_NOOP("Actions", "Volume Down"), {"Down"}, {"9", "/"}, K::Mpv, {"add", "volume", "-5"}, "volume"},
        {"mute", "audio", QT_TRANSLATE_NOOP("Actions", "Mute"), {"M"}, {"M"}, K::Mpv, {"cycle", "mute"}, "mute"},
        {"next-audio", "audio", QT_TRANSLATE_NOOP("Actions", "Next Audio Track"), {}, {"#"}, K::Mpv, {"cycle", "audio"}, "audio"},

        {"sub-earlier", "subtitles", QT_TRANSLATE_NOOP("Actions", "Subtitles 0.5 s Earlier"), {","}, {"Z"}, K::Mpv, {"add", "sub-delay", "-0.5"}, "sub-delay"},
        {"sub-later", "subtitles", QT_TRANSLATE_NOOP("Actions", "Subtitles 0.5 s Later"), {"."}, {"Shift+Z"}, K::Mpv, {"add", "sub-delay", "0.5"}, "sub-delay"},
        {"next-subtitle", "subtitles", QT_TRANSLATE_NOOP("Actions", "Next Subtitle Track"), {}, {"J"}, K::Mpv, {"cycle", "sub"}, "subtitle"},
        {"toggle-subtitles", "subtitles", QT_TRANSLATE_NOOP("Actions", "Show / Hide Subtitles"), {"Alt+H"}, {"V"}, K::Mpv, {"cycle", "sub-visibility"}, "sub-visibility"},

        {"fullscreen", "screen", QT_TRANSLATE_NOOP("Actions", "Fullscreen"), {"Return", "Enter"}, {"F"}, K::Window, {}, {}},
        {"exit-fullscreen", "screen", QT_TRANSLATE_NOOP("Actions", "Exit Fullscreen"), {"Esc"}, {"Esc"}, K::Window, {}, {}},
        {"info", "screen", QT_TRANSLATE_NOOP("Actions", "Playback Info"), {"Tab"}, {"I"}, K::Window, {}, {}},

        {"settings", "help", QT_TRANSLATE_NOOP("Actions", "Settings…"), {"F5"}, {"Ctrl+,"}, K::Window, {}, {}},
        {"shortcuts", "help", QT_TRANSLATE_NOOP("Actions", "Keyboard Shortcuts"), {"?", "F1"}, {"?"}, K::Window, {}, {}},
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

QVariantList Actions::list() const {
    QVariantList list;
    for (const auto &spec : specs()) {
        const QStringList keys = keysFor(spec, m_keymap);
        QStringList labels;
        for (const auto &key : keys) {
            const QString label = keyLabel(key);
            if (!labels.contains(label))
                labels.append(label);
        }
        list.append(QVariantMap{
            {"id", spec.id},
            {"group", spec.group},
            {"text", translated(spec.text)},
            {"keys", keys},
            {"keyLabels", labels},
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
    for (const auto &spec : specs())
        if (spec.id == id) {
            const QStringList keys = keysFor(spec, m_keymap);
            return keys.isEmpty() ? QString() : keyLabel(keys.first());
        }
    return {};
}

QString Actions::text(const QString &id) const {
    for (const auto &spec : specs())
        if (spec.id == id)
            return translated(spec.text);
    return {};
}

void Actions::trigger(const QString &id) {
    for (const auto &spec : specs()) {
        if (spec.id != id)
            continue;
        switch (spec.kind) {
        case Kind::Mpv: {
            QVariantList command;
            for (const auto &part : spec.command)
                command.append(part);
            m_player->command(command, spec.osd);
            break;
        }
        case Kind::Player:
            if (id == "play-pause")
                m_player->togglePause();
            else if (id == "stop")
                m_player->stop();
            else if (id == "open-clipboard" && !m_player->openClipboard())
                emit m_player->osd("clipboard-empty");
            break;
        case Kind::Window:
            emit windowAction(id);
            break;
        }
        return;
    }
}
