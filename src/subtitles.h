#pragma once

#include <QList>
#include <QString>
#include <QStringList>

// Subtitle files beside a video, made ready for mpv. Text subtitles are
// decoded here, whatever their encoding, and handed over as UTF-8; SAMI files
// are split into a track per language. Image subtitles go through untouched.
namespace Subtitles {

struct Track {
    QString path;
    QString title;
    QString language;
};

bool isSubtitle(const QString &path);

// The subtitle files that belong to a video: mode "exact" takes the ones
// named like it ("Movie.srt", "Movie.ko.srt"), "fuzzy" also those whose
// name contains it, "no" none.
QStringList besides(const QString &video, const QString &mode);

// forced and language are as for TextEncoding::decode.
QList<Track> prepare(const QString &path, const QString &forced, const QString &language, const QString &cacheDirectory);

}
