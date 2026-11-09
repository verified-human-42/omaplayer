#include "subtitles.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>

#include "sami.h"
#include "textencoding.h"

namespace {
const QSet<QString> &textFormats() {
    static const QSet<QString> set = {"srt", "ass", "ssa", "vtt", "sub", "sbv", "lrc", "utf", "utf8", "utf-8", "rt", "scc", "txt"};
    return set;
}

const QSet<QString> &imageFormats() {
    static const QSet<QString> set = {"idx", "sup", "pgs", "mks"};
    return set;
}

bool isSami(const QString &suffix) {
    return suffix == "smi" || suffix == "sami";
}

// "Movie.ko.srt" → "ko"; the usual tags for the usual languages.
QString languageTag(const QString &tag) {
    static const QHash<QString, QString> known = {
        {"ko", "ko"}, {"kor", "ko"}, {"kr", "ko"}, {"korean", "ko"}, {"한국어", "ko"}, {"한글", "ko"},
        {"ja", "ja"}, {"jpn", "ja"}, {"jp", "ja"}, {"japanese", "ja"},
        {"zh", "zh"}, {"chi", "zh"}, {"zho", "zh"}, {"chinese", "zh"}, {"chs", "zh-Hans"}, {"sc", "zh-Hans"},
        {"cht", "zh-Hant"}, {"tc", "zh-Hant"}, {"zh-cn", "zh-CN"}, {"zh-tw", "zh-TW"}, {"zh-hans", "zh-Hans"}, {"zh-hant", "zh-Hant"},
        {"en", "en"}, {"eng", "en"}, {"english", "en"}, {"es", "es"}, {"spa", "es"}, {"fr", "fr"}, {"fre", "fr"},
        {"fra", "fr"}, {"de", "de"}, {"ger", "de"}, {"deu", "de"}, {"ru", "ru"}, {"rus", "ru"}, {"pt", "pt"},
        {"por", "pt"}, {"pt-br", "pt-BR"}, {"it", "it"}, {"ita", "it"}, {"pl", "pl"}, {"pol", "pl"},
    };
    return known.value(tag.toLower());
}

QString cachePath(const QString &directory, const QString &path, const QString &forced, const QString &suffix) {
    const QFileInfo info(path);
    const QByteArray key = (info.absoluteFilePath() + info.lastModified().toString(Qt::ISODateWithMs) + forced).toUtf8();
    return QDir(directory).filePath(QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex().left(16)) + "." + suffix);
}
}

bool Subtitles::isSubtitle(const QString &path) {
    const QString suffix = QFileInfo(path).suffix().toLower();
    return textFormats().contains(suffix) || imageFormats().contains(suffix) || isSami(suffix);
}

QStringList Subtitles::besides(const QString &video, const QString &mode) {
    if (mode == "no" || video.contains("://"))
        return {};
    const QFileInfo info(video);
    const QString base = info.completeBaseName();
    QStringList found;
    for (const auto &entry : info.dir().entryInfoList(QDir::Files | QDir::Readable, QDir::Name)) {
        const QString suffix = entry.suffix().toLower();
        if (!isSubtitle(entry.fileName()) || suffix == "txt")
            continue;
        // A VobSub's .sub is the picture data for its .idx, not text.
        if (suffix == "sub" && QFileInfo::exists(entry.dir().filePath(entry.completeBaseName() + ".idx")))
            continue;
        const QString name = entry.completeBaseName();
        const bool exact = name.compare(base, Qt::CaseInsensitive) == 0
                           || (name.startsWith(base + ".", Qt::CaseInsensitive) && !name.mid(base.size() + 1).contains('.'));
        if (exact || (mode == "fuzzy" && name.contains(base, Qt::CaseInsensitive)))
            found.append(entry.absoluteFilePath());
    }
    return found;
}

QList<Subtitles::Track> Subtitles::prepare(const QString &path, const QString &forced, const QString &language,
                                           const QString &cacheDirectory) {
    const QFileInfo info(path);
    const QString suffix = info.suffix().toLower();
    QDir().mkpath(cacheDirectory);

    if (isSami(suffix)) {
        QList<Track> tracks;
        for (const auto &track : Sami::convert(path, forced, language, cacheDirectory))
            tracks.append({track.path, track.name, track.language});
        return tracks;
    }

    // "Movie.ko.srt": the part after the video's name says the language.
    const QString tag = info.completeBaseName().section('.', -1);
    const QString tagged = info.completeBaseName().contains('.') ? languageTag(tag) : QString();
    const QString title = tagged.isEmpty() ? info.fileName() : tag;

    if (!textFormats().contains(suffix))
        return {{path, title, tagged}};

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const auto decoded = TextEncoding::decode(file.readAll(), forced, language);
    if (decoded.encoding == "utf-8")
        return {{path, title, tagged}};

    // Written as UTF-8 next to the others in the cache, under a name that
    // changes when the file or the chosen encoding does.
    const QString target = cachePath(cacheDirectory, path, forced, suffix);
    if (!QFileInfo::exists(target)) {
        QFile out(target + ".part");
        if (!out.open(QIODevice::WriteOnly) || out.write(decoded.text.toUtf8()) < 0)
            return {{path, title, tagged}};
        out.close();
        if (!QFile::rename(out.fileName(), target))
            return {{path, title, tagged}};
    }
    return {{target, title, tagged}};
}
