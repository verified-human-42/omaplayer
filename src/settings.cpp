#include "settings.h"

#include <QKeySequence>
#include <QLocale>

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

    // One line per key, so any key, ";" and "," included, round-trips.
    m_store->beginGroup("keys");
    for (const auto &id : m_store->childKeys())
        m_bindings.insert(id, m_store->value(id).toString().split('\n', Qt::SkipEmptyParts));
    m_store->endGroup();
    m_actions->setOverrides(m_bindings);
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
        {"subtitles/encoding", "auto", {"auto", "utf-8", "japanese", "chinese-simplified", "chinese-traditional", "korean"}},
        {"subtitles/scale", 1.0, {}, 0.5, 2.0},

        {"interface/keymap", "omaplayer", {"omaplayer", "mpv"}},
        {"interface/language", "omarchy", {"omarchy", "en", "ko", "ja", "zh-CN", "zh-TW"}},
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
        m_player->setSubtitleMode(value.toString());
    else if (key == "subtitles/language") {
        const QString chosen = value.toString().trimmed();
        m_player->set("slang", chosen.isEmpty() ? languageCodes(m_language) : chosen);
        // Subtitles in the language being spoken only come up when forced.
        m_player->set("subs-with-matching-audio", "forced");
    } else if (key == "subtitles/encoding") {
        // Auto tells each file's encoding from its text (TextEncoding); the
        // others force one, for the rare file that's ambiguous.
        static const QHash<QString, QString> codepages = {
            {"utf-8", "utf-8"}, {"japanese", "cp932"}, {"chinese-simplified", "gb18030"},
            {"chinese-traditional", "big5"}, {"korean", "cp949"},
        };
        m_player->setSubtitleEncoding(codepages.value(value.toString()));
    }
    else if (key == "subtitles/scale")
        m_player->set("sub-scale", value.toDouble());
    else if (key == "interface/keymap")
        m_actions->setKeymap(value.toString());
}

void Settings::setSystemLanguage(const QString &language) {
    if (language.isEmpty() || language == m_language)
        return;
    m_language = language;
    m_player->setLanguage(language);
    apply("subtitles/language");
}

// What subtitle files in this language were written in before UTF-8, or
// "auto" to let mpv detect it.
QString Settings::legacyCodepage(const QString &language) {
    static const QHash<QString, QString> codepages = {
        {"ko", "cp949"}, {"ja", "cp932"}, {"zh-CN", "gb18030"}, {"zh-TW", "big5"},
        {"ru", "cp1251"}, {"uk", "cp1251"}, {"bg", "cp1251"}, {"sr", "cp1251"},
        {"pl", "cp1250"}, {"cs", "cp1250"}, {"sk", "cp1250"}, {"hr", "cp1250"}, {"sl", "cp1250"}, {"ro", "cp1250"},
        {"el", "cp1253"}, {"tr", "cp1254"}, {"he", "cp1255"}, {"ar", "cp1256"}, {"fa", "cp1256"},
        {"lt", "cp1257"}, {"lv", "cp1257"}, {"vi", "cp1258"}, {"th", "cp874"},
    };
    return codepages.value(language, "auto");
}

// The codes a subtitle track might carry for a language, most specific
// first: "zh-TW" gives zh-TW, zh, zh-Hant, chi, zho, cht.
QString Settings::languageCodes(const QString &language) {
    QStringList codes{language};
    const QString base = language.section('-', 0, 0);
    if (base != language)
        codes.append(base);
    if (base == "zh")
        codes.append(language == "zh-TW" ? QString("zh-Hant") : QString("zh-Hans"));
    const QLocale::Language spoken = QLocale(language).language();
    if (spoken != QLocale::C && spoken != QLocale::AnyLanguage)
        for (auto type : {QLocale::ISO639Part2B, QLocale::ISO639Part2T}) {
            const QString code = QLocale::languageToCode(spoken, type);
            if (!code.isEmpty() && !codes.contains(code))
                codes.append(code);
        }
    if (base == "zh")
        codes.append(language == "zh-TW" ? QString("cht") : QString("chs"));
    return codes.join(',');
}

QString Settings::bindKey(const QString &id, const QString &key) {
    if (key.isEmpty())
        return {};
    const QString owner = m_actions->idForKey(key);
    if (owner == id)
        return {};
    const QString portable = QKeySequence(key).toString(QKeySequence::PortableText);
    if (!owner.isEmpty()) {
        QStringList rest = m_actions->keys(owner);
        rest.removeIf([&](const QString &bound) {
            return QKeySequence(bound).toString(QKeySequence::PortableText) == portable;
        });
        m_bindings.insert(owner, rest);
    }
    QStringList mine = m_actions->keys(id);
    mine.append(portable);
    m_bindings.insert(id, mine);
    storeBindings();
    return owner;
}

void Settings::unbindKey(const QString &id, const QString &key) {
    const QString portable = QKeySequence(key).toString(QKeySequence::PortableText);
    QStringList mine = m_actions->keys(id);
    mine.removeIf([&](const QString &bound) {
        return QKeySequence(bound).toString(QKeySequence::PortableText) == portable;
    });
    m_bindings.insert(id, mine);
    storeBindings();
}

void Settings::resetKeys(const QString &id) {
    if (m_bindings.remove(id))
        storeBindings();
}

void Settings::resetAllKeys() {
    if (m_bindings.isEmpty())
        return;
    m_bindings.clear();
    storeBindings();
}

void Settings::storeBindings() {
    // A binding that ends up the same as the layout's isn't a customization.
    m_actions->setOverrides({});
    for (auto it = m_bindings.begin(); it != m_bindings.end();) {
        if (m_actions->keys(it.key()) == it.value())
            it = m_bindings.erase(it);
        else
            ++it;
    }
    m_store->remove("keys");
    m_store->beginGroup("keys");
    for (auto it = m_bindings.cbegin(); it != m_bindings.cend(); ++it)
        m_store->setValue(it.key(), it.value().join('\n'));
    m_store->endGroup();
    m_actions->setOverrides(m_bindings);
    emit changed();
}
