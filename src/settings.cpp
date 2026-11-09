#include "settings.h"

#include <QDir>
#include <QFile>
#include <QKeySequence>
#include <QLocale>
#include <QProcess>
#include <QStandardPaths>

#include "actions.h"
#include "player.h"

Settings::Settings(Player *player, Actions *actions, const QString &file, QObject *parent)
    : QObject(parent),
      m_store(file.isEmpty() ? std::make_unique<QSettings>() : std::make_unique<QSettings>(file, QSettings::IniFormat)),
      m_player(player),
      m_actions(actions) {
    // 0.1 had a "hold the last frame" switch where "when the last file ends" is now.
    if (m_store->contains("playback/keepOpen") && !m_store->contains("playback/atEnd"))
        m_store->setValue("playback/atEnd", m_store->value("playback/keepOpen").toString() == "false" ? "clear" : "hold");
    m_store->remove("playback/keepOpen");

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
        {"playback/atEnd", "hold", {"hold", "clear", "quit"}},
        {"playback/seekStep", "5", {"1", "3", "5", "10"}},
        {"playback/seekLongStep", "30", {"10", "30", "60"}},
        {"playback/exactSeek", false},
        {"playback/openIn", "replace", {"replace", "window"}},
        {"playback/clickPause", true},
        {"playback/wheel", "volume", {"volume", "seek"}},

        {"video/hwdec", true},
        {"video/smooth", false},
        {"video/scale", "balanced", {"fast", "balanced", "sharp"}},
        {"video/deband", false},
        {"video/deinterlace", false},
        {"video/keepAwake", true},
        // 1080p by default: "best" is often 4K AV1, which many graphics cards
        // can't decode, and software decoding at that size drops frames.
        {"video/streamQuality", "1080", {"best", "2160", "1440", "1080", "720", "480"}},
        {"video/screenshotFolder", ""},
        {"video/screenshotFormat", "png", {"png", "jpg", "webp"}},
        {"video/screenshotSubtitles", true},

        {"audio/boost", false},
        {"audio/normalize", false},
        {"audio/language", ""},
        {"audio/device", "auto"},
        {"audio/surround", "auto", {"auto", "stereo", "receiver"}},
        {"audio/rememberVolume", true},

        {"subtitles/auto", "fuzzy", {"fuzzy", "exact", "no"}},
        {"subtitles/language", ""},
        {"subtitles/encoding", "auto", {"auto", "utf-8", "japanese", "chinese-simplified", "chinese-traditional", "korean"}},
        {"subtitles/scale", 1.0, {}, 0.5, 2.0},
        {"subtitles/lift", 0.0, {}, 0.0, 0.3},
        {"subtitles/background", false},
        {"subtitles/restyle", false},
        {"subtitles/second", ""},
        {"subtitles/font", ""},

        {"interface/keymap", "omaplayer", {"omaplayer", "mpv"}},
        {"interface/language", "omarchy", {"omarchy", "en", "ko", "ja", "zh-CN", "zh-TW"}},
        {"interface/controls", "docked", {"docked", "floating"}},
        {"interface/remaining", false},
        {"interface/hideDelay", 2, {}, 1, 10},
        {"interface/osd", true},
        {"interface/startFullscreen", false},
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
    // Shortcuts count too, as "keys/<action>", so Undo all takes them back.
    QStringList ids = m_bindings.keys() + m_sessionBindings.keys();
    ids.removeDuplicates();
    ids.sort();
    for (const auto &id : ids)
        if (m_bindings.value(id) != m_sessionBindings.value(id))
            keys.append("keys/" + id);
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
    if (key.startsWith("keys/")) {
        const QString id = key.mid(5);
        if (m_sessionBindings.contains(id))
            m_bindings.insert(id, m_sessionBindings.value(id));
        else
            m_bindings.remove(id);
        storeBindings();
    } else if (m_session.contains(key)) {
        set(key, m_session.value(key));
    }
}

void Settings::undoAll() {
    for (const auto &key : changedKeys())
        if (!key.startsWith("keys/"))
            set(key, m_session.value(key));
    if (m_bindings != m_sessionBindings) {
        m_bindings = m_sessionBindings;
        storeBindings();
    }
}

void Settings::beginSession() {
    m_session = m_values;
    m_sessionBindings = m_bindings;
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
    else if (key == "playback/atEnd")
        // Quitting at the end is the window's to do, on Player::finished.
        m_player->set("keep-open", value.toString() == "hold");
    else if (key == "playback/seekStep" || key == "playback/seekLongStep")
        m_actions->setSeekSteps(m_values.value("playback/seekStep").toInt(), m_values.value("playback/seekLongStep").toInt());
    else if (key == "playback/exactSeek")
        m_player->set("hr-seek", value.toBool() ? "yes" : "default");
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
    else if (key == "video/deinterlace")
        m_player->set("deinterlace", value.toBool() ? "auto" : "no");
    else if (key == "video/streamQuality") {
        // The tallest picture yt-dlp fetches, in VP9 or H.264 over AV1:
        // many graphics cards can't decode AV1, and software decoding drops
        // frames. "Best" takes whatever is best.
        const QString height = value.toString();
        m_player->set("ytdl-format", height == "best" ? QString("bestvideo+bestaudio/best")
                                   : QString("bestvideo[height<=?%1][vcodec!^=av01]+bestaudio/bestvideo[height<=?%1]+bestaudio/best[height<=?%1]/best").arg(height));
    }
    else if (key.startsWith("video/screenshot")) {
        const QString folder = m_values.value("video/screenshotFolder").toString();
        m_player->setScreenshotOptions(folder.isEmpty() ? defaultScreenshotFolder() : folder,
                                       m_values.value("video/screenshotSubtitles").toBool(),
                                       m_values.value("video/screenshotFormat").toString());
    }
    else if (key == "audio/boost")
        m_player->set("volume-max", value.toBool() ? 150 : 100);
    else if (key == "audio/normalize")
        m_player->set("af", value.toBool() ? "lavfi=[dynaudnorm=f=250:g=15:p=0.9]" : "");
    else if (key == "audio/language")
        m_player->set("alang", value.toString());
    else if (key == "audio/device")
        m_player->set("audio-device", value.toString().isEmpty() ? QString("auto") : value.toString());
    else if (key == "audio/surround") {
        // Stereo folds surround into two channels; a receiver gets Dolby and
        // DTS untouched over HDMI or S/PDIF and decodes them itself.
        m_player->set("audio-channels", value.toString() == "stereo" ? "stereo" : "auto-safe");
        m_player->set("audio-spdif", value.toString() == "receiver" ? "ac3,eac3,dts,dts-hd,truehd" : "");
    }

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
    else if (key == "subtitles/lift")
        m_player->set("sub-pos", qRound(100 - value.toDouble() * 100));
    else if (key == "subtitles/font")
        m_player->set("sub-font", value.toString().isEmpty() ? QString("sans-serif") : value.toString());
    else if (key == "subtitles/background")
        m_player->set("sub-border-style", value.toBool() ? "background-box" : "outline-and-shadow");
    else if (key == "subtitles/restyle")
        // Styled (ASS) subtitles bring their own look; this puts ours on them.
        m_player->set("sub-ass-override", value.toBool() ? "force" : "scale");
    else if (key == "subtitles/second") {
        QStringList codes;
        for (const auto &code : value.toString().split(',', Qt::SkipEmptyParts))
            codes += languageCodes(code.trimmed()).toLower().split(',');
        m_player->setSecondLanguage(codes);
    }
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

// Where Omarchy's own screenshots go.
QString Settings::defaultScreenshotFolder() {
    const QString omarchy = qEnvironmentVariable("OMARCHY_SCREENSHOT_DIR");
    if (!omarchy.isEmpty())
        return omarchy;
    const QString pictures = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    return pictures.isEmpty() ? QDir::home().filePath("Pictures") : pictures;
}

void Settings::resetAll() {
    for (const auto &spec : specs())
        set(spec.key, spec.fallback);
    resetAllKeys();
}

// The MIME types OmaPlayer's launcher entry says it plays.
QStringList Settings::mediaTypes() {
    QFile desktop(QStandardPaths::locate(QStandardPaths::ApplicationsLocation, "omaplayer.desktop"));
    if (!desktop.open(QIODevice::ReadOnly))
        return {};
    for (const auto &line : QString::fromUtf8(desktop.readAll()).split('\n'))
        if (line.startsWith("MimeType="))
            return line.mid(9).split(';', Qt::SkipEmptyParts);
    return {};
}

void Settings::checkDefaultPlayer() {
    auto *query = new QProcess(this);
    connect(query, &QProcess::finished, this, [this, query] {
        const bool isDefault = QString::fromUtf8(query->readAllStandardOutput()).trimmed() == "omaplayer.desktop";
        query->deleteLater();
        if (isDefault != m_defaultPlayer) {
            m_defaultPlayer = isDefault;
            emit defaultPlayerChanged();
        }
    });
    query->start("xdg-mime", {"query", "default", "video/mp4"});
}

void Settings::makeDefaultPlayer() {
    const QStringList types = mediaTypes();
    if (types.isEmpty()) {
        m_defaultPlayerError = tr("OmaPlayer's launcher entry wasn't found. Install it as a package first.");
        emit defaultPlayerChanged();
        return;
    }
    auto *assign = new QProcess(this);
    connect(assign, &QProcess::finished, this, [this, assign](int code, QProcess::ExitStatus status) {
        assign->deleteLater();
        m_defaultPlayerError = status == QProcess::NormalExit && code == 0
                                   ? QString() : tr("xdg-mime couldn't set it: %1").arg(QString::fromUtf8(assign->readAllStandardError()).trimmed());
        emit defaultPlayerChanged();
        checkDefaultPlayer();
    });
    connect(assign, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_defaultPlayerError = tr("xdg-mime isn't installed.");
            emit defaultPlayerChanged();
        }
    });
    assign->start("xdg-mime", QStringList{"default", "omaplayer.desktop"} + types);
}
