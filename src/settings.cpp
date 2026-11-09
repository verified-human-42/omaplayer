#include "settings.h"

#include "actions.h"
#include "player.h"

Settings::Settings(Player *player, Actions *actions, const QString &file, QObject *parent)
    : QObject(parent),
      m_store(file.isEmpty() ? std::make_unique<QSettings>() : std::make_unique<QSettings>(file, QSettings::IniFormat)),
      m_player(player),
      m_actions(actions) {
    for (const auto &spec : specs())
        m_values.insert(spec.key, normalize(spec, m_store->value(spec.key, spec.fallback)));
    m_session = m_values;
}

const QList<Settings::Spec> &Settings::specs() {
    static const QList<Spec> table = {
        {"playback/resume", true},
        {"playback/folderPlaylist", true},
        {"playback/keepOpen", true},
        {"playback/clickPause", true},
        {"playback/wheel", "volume", {"volume", "seek"}},

        {"video/hwdec", true},
        {"video/smooth", false},
        {"video/scale", "balanced", {"fast", "balanced", "sharp"}},
        {"video/deband", false},

        {"audio/boost", false},
        {"audio/normalize", false},
        {"audio/language", ""},

        {"subtitles/auto", "fuzzy", {"fuzzy", "exact", "no"}},
        {"subtitles/language", ""},
        {"subtitles/scale", 1.0, {}, 0.5, 2.0},

        {"interface/keymap", "omaplayer", {"omaplayer", "mpv"}},
        {"interface/controls", "docked", {"docked", "floating"}},
        {"interface/remaining", false},
        {"interface/hideDelay", 2, {}, 1, 10},
    };
    return table;
}

const Settings::Spec *Settings::find(const QString &key) {
    for (const auto &spec : specs())
        if (spec.key == key)
            return &spec;
    return nullptr;
}

// Stored values come back as strings from the ini file and as anything at all
// from QML, so every value is brought back to its setting's type and range.
QVariant Settings::normalize(const Spec &spec, const QVariant &value) {
    switch (spec.fallback.typeId()) {
    case QMetaType::Bool:
        if (value.typeId() == QMetaType::QString)
            return value.toString() == "true" || value.toString() == "1";
        return value.toBool();
    case QMetaType::Int: {
        bool ok = false;
        const int number = value.toInt(&ok);
        return ok ? qBound(int(spec.minimum), number, int(spec.maximum)) : spec.fallback;
    }
    case QMetaType::Double: {
        bool ok = false;
        const double number = value.toDouble(&ok);
        return ok ? qBound(spec.minimum, number, spec.maximum) : spec.fallback;
    }
    default: {
        const QString text = value.toString().trimmed();
        if (!spec.choices.isEmpty() && !spec.choices.contains(text))
            return spec.fallback;
        return text;
    }
    }
}

QStringList Settings::changedKeys() const {
    QStringList keys;
    for (const auto &spec : specs())
        if (m_values.value(spec.key) != m_session.value(spec.key))
            keys.append(spec.key);
    return keys;
}

void Settings::set(const QString &key, const QVariant &value) {
    const Spec *spec = find(key);
    if (!spec)
        return;
    const QVariant next = normalize(*spec, value);
    if (next == m_values.value(key))
        return;
    m_values.insert(key, next);
    m_store->setValue(key, next);
    apply(key);
    emit changed();
}

void Settings::undo(const QString &key) {
    if (m_session.contains(key))
        set(key, m_session.value(key));
}

void Settings::undoAll() {
    for (const auto &key : changedKeys())
        set(key, m_session.value(key));
}

void Settings::beginSession() {
    m_session = m_values;
    emit changed();
}

void Settings::applyAll() {
    for (const auto &spec : specs())
        apply(spec.key);
}

void Settings::apply(const QString &key) {
    const QVariant value = m_values.value(key);
    if (key == "playback/resume")
        m_player->set("resume-playback", value.toBool());
    else if (key == "playback/folderPlaylist")
        m_player->setFolderPlaylist(value.toBool());
    else if (key == "playback/keepOpen")
        m_player->set("keep-open", value.toBool());
    else if (key == "video/hwdec")
        m_player->set("hwdec", value.toBool() ? "auto-safe" : "no");
    else if (key == "video/smooth") {
        // Resampling to the display's refresh, with interpolation, removes
        // the judder of 24 fps film on 60 Hz screens.
        m_player->set("video-sync", value.toBool() ? "display-resample" : "audio");
        m_player->set("interpolation", value.toBool());
    } else if (key == "video/scale") {
        const QString scale = value.toString();
        m_player->set("scale", scale == "fast" ? "bilinear" : scale == "sharp" ? "ewa_lanczossharp" : "spline36");
    } else if (key == "video/deband")
        m_player->set("deband", value.toBool());
    else if (key == "audio/boost")
        m_player->set("volume-max", value.toBool() ? 150 : 100);
    else if (key == "audio/normalize")
        m_player->set("af", value.toBool() ? "lavfi=[dynaudnorm=f=250:g=15:p=0.9]" : "");
    else if (key == "audio/language")
        m_player->set("alang", value.toString());
    else if (key == "subtitles/auto")
        m_player->set("sub-auto", value.toString());
    else if (key == "subtitles/language")
        m_player->set("slang", value.toString());
    else if (key == "subtitles/scale")
        m_player->set("sub-scale", value.toDouble());
    else if (key == "interface/keymap")
        m_actions->setKeymap(value.toString());
}
