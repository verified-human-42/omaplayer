#include "sami.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMap>
#include <QRegularExpression>

#include "textencoding.h"

namespace {
using Re = QRegularExpression;
constexpr auto Insensitive = Re::CaseInsensitiveOption;

QString decodeEntities(QString text) {
    static const QHash<QString, QString> named = {
        {"nbsp", " "}, {"amp", "&"}, {"lt", "<"}, {"gt", ">"}, {"quot", "\""}, {"apos", "'"},
    };
    static const Re entity("&(#x[0-9a-f]+|#[0-9]+|[a-z]+);?", Insensitive);
    QString out;
    qsizetype last = 0;
    auto matches = entity.globalMatch(text);
    while (matches.hasNext()) {
        const auto match = matches.next();
        const QString code = match.captured(1);
        QString replacement;
        if (code.startsWith("#x", Qt::CaseInsensitive))
            replacement = QString(QChar::fromUcs4(code.mid(2).toUInt(nullptr, 16)));
        else if (code.startsWith('#'))
            replacement = QString(QChar::fromUcs4(code.mid(1).toUInt()));
        else if (named.contains(code.toLower()))
            replacement = named.value(code.toLower());
        else
            continue;
        out += text.mid(last, match.capturedStart() - last) + replacement;
        last = match.capturedEnd();
    }
    return out + text.mid(last);
}

// SAMI's HTML as the SRT that mpv draws: line breaks from <br>, colour, bold,
// italic, and underline kept, every other tag dropped.
QString toSrtText(QString html) {
    html.replace(Re("[\\r\\n\\t]+"), " ");
    html.replace(Re("<br\\s*/?>", Insensitive), "\n");

    static const Re tag("<(/?)([a-z]+)([^>]*)>", Insensitive);
    static const Re color("color\\s*=\\s*[\"']?(#?[0-9a-z]+)", Insensitive);
    static const Re hex("^[0-9a-f]{6}$", Insensitive);
    QString out;
    qsizetype last = 0;
    auto matches = tag.globalMatch(html);
    while (matches.hasNext()) {
        const auto match = matches.next();
        out += html.mid(last, match.capturedStart() - last);
        last = match.capturedEnd();
        const bool closing = !match.captured(1).isEmpty();
        const QString name = match.captured(2).toLower();
        if (name == "b" || name == "i" || name == "u") {
            out += closing ? "</" + name + ">" : "<" + name + ">";
        } else if (name == "font") {
            const auto picked = color.match(match.captured(3));
            if (closing)
                out += "</font>";
            else if (picked.hasMatch()) {
                QString value = picked.captured(1);
                if (hex.match(value).hasMatch())
                    value.prepend('#');
                out += "<font color=\"" + value + "\">";
            } else {
                out += "<font>";
            }
        }
    }
    out += html.mid(last);
    out = decodeEntities(out);

    QStringList lines;
    for (const auto &line : out.split('\n'))
        if (!line.trimmed().isEmpty())
            lines.append(line.trimmed());
    // A line of nothing but tags is still nothing to show.
    QString plain = lines.join('\n');
    plain.remove(tag);
    return plain.trimmed().isEmpty() ? QString() : lines.join('\n');
}

QString timecode(qint64 ms) {
    return QStringLiteral("%1:%2:%3,%4")
        .arg(ms / 3600000, 2, 10, QChar('0'))
        .arg(ms / 60000 % 60, 2, 10, QChar('0'))
        .arg(ms / 1000 % 60, 2, 10, QChar('0'))
        .arg(ms % 1000, 3, 10, QChar('0'));
}

// Class names that say their language even without a style sheet.
QString languageFromClass(const QString &name) {
    static const QHash<QString, QString> known = {
        {"krcc", "ko"}, {"kocc", "ko"}, {"kor", "ko"}, {"encc", "en"}, {"enuscc", "en"}, {"egcc", "en"},
        {"jpcc", "ja"}, {"jacc", "ja"}, {"cncc", "zh"}, {"zhcc", "zh"}, {"chcc", "zh"},
    };
    return known.value(name.toLower());
}
}

QList<Sami::Track> Sami::convert(const QString &path, const QString &forced, const QString &language,
                                 const QString &directory) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    const QString text = TextEncoding::decode(file.readAll(), forced, language).text;

    // Language classes from the style sheet: .KRCC { Name:한국어; lang:ko-KR; }
    QStringList order;
    QHash<QString, QPair<QString, QString>> classes;
    static const Re rule("\\.([a-z0-9_-]+)\\s*\\{([^}]*)\\}", Insensitive);
    static const Re nameProperty("name\\s*:\\s*([^;]+)", Insensitive);
    static const Re langProperty("lang\\s*:\\s*([a-z]{2,3})", Insensitive);
    auto rules = rule.globalMatch(text);
    while (rules.hasNext()) {
        const auto match = rules.next();
        const QString body = match.captured(2);
        if (!body.contains("lang", Qt::CaseInsensitive) && !body.contains("name", Qt::CaseInsensitive))
            continue;
        const QString key = match.captured(1).toLower();
        QString language = langProperty.match(body).captured(1).toLower();
        if (language == "kr")
            language = "ko";
        classes.insert(key, {nameProperty.match(body).captured(1).trimmed(), language});
        if (!order.contains(key))
            order.append(key);
    }

    // Each SYNC starts a moment; each P inside it is one class's text from
    // then until that class's next SYNC.
    QHash<QString, QList<QPair<qint64, QString>>> moments;
    static const Re sync("<sync\\b[^>]*?\\bstart\\s*=\\s*[\"']?(\\d+)[^>]*>", Insensitive);
    static const Re paragraph("<p\\b([^>]*)>", Insensitive);
    static const Re classAttribute("class\\s*=\\s*[\"']?([a-z0-9_-]+)", Insensitive);
    static const Re bodyEnd("</body>", Insensitive);
    QList<QRegularExpressionMatch> syncs;
    auto found = sync.globalMatch(text);
    while (found.hasNext())
        syncs.append(found.next());
    for (int i = 0; i < syncs.size(); ++i) {
        const qint64 start = syncs[i].captured(1).toLongLong();
        qsizetype end = i + 1 < syncs.size() ? syncs[i + 1].capturedStart() : text.indexOf(bodyEnd, syncs[i].capturedEnd());
        if (end < 0)
            end = text.size();
        const QString segment = text.mid(syncs[i].capturedEnd(), end - syncs[i].capturedEnd());

        QList<QRegularExpressionMatch> paragraphs;
        auto ps = paragraph.globalMatch(segment);
        while (ps.hasNext())
            paragraphs.append(ps.next());
        if (paragraphs.isEmpty()) {
            const QString key = order.size() == 1 ? order.first() : QString();
            moments[key].append({start, segment});
            continue;
        }
        for (int p = 0; p < paragraphs.size(); ++p) {
            const qsizetype from = paragraphs[p].capturedEnd();
            const qsizetype to = p + 1 < paragraphs.size() ? paragraphs[p + 1].capturedStart() : segment.size();
            const QString key = classAttribute.match(paragraphs[p].captured(1)).captured(1).toLower();
            moments[key].append({start, segment.mid(from, to - from)});
        }
    }

    // Classes in the order the style sheet lists them, then any it didn't.
    QStringList keys = order;
    for (auto it = moments.cbegin(); it != moments.cend(); ++it)
        if (!keys.contains(it.key()))
            keys.append(it.key());

    QDir().mkpath(directory);
    const QByteArray stamp = (QFileInfo(path).absoluteFilePath() + QFileInfo(path).lastModified().toString(Qt::ISODateWithMs)).toUtf8();
    const QString prefix = QString::fromLatin1(QCryptographicHash::hash(stamp, QCryptographicHash::Sha1).toHex().left(16));

    QList<Track> tracks;
    for (const auto &key : keys) {
        auto list = moments.value(key);
        std::stable_sort(list.begin(), list.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
        QString srt;
        int count = 0;
        for (int i = 0; i < list.size(); ++i) {
            const QString cue = toSrtText(list[i].second);
            if (cue.isEmpty())
                continue;
            // Until the class's next moment; the last one stays five seconds.
            qint64 end = list[i].first + 5000;
            for (int j = i + 1; j < list.size(); ++j)
                if (list[j].first > list[i].first) {
                    end = list[j].first;
                    break;
                }
            srt += QStringLiteral("%1\n%2 --> %3\n%4\n\n").arg(++count).arg(timecode(list[i].first), timecode(end), cue);
        }
        if (count == 0)
            continue;

        Track track;
        track.className = key;
        track.name = classes.value(key).first.isEmpty() ? (key.isEmpty() ? QStringLiteral("SAMI") : key.toUpper())
                                                        : classes.value(key).first;
        track.language = classes.value(key).second.isEmpty() ? languageFromClass(key) : classes.value(key).second;
        track.cues = count;
        track.path = QDir(directory).filePath(prefix + "-" + (key.isEmpty() ? QStringLiteral("default") : key) + ".srt");
        QFile out(track.path + ".part");
        if (!out.open(QIODevice::WriteOnly) || out.write(srt.toUtf8()) < 0)
            continue;
        out.close();
        QFile::remove(track.path);
        if (!QFile::rename(out.fileName(), track.path))
            continue;
        tracks.append(track);
    }
    return tracks;
}
