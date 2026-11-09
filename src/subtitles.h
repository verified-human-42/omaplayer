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

// Korean wraps at spaces, but the renderer may break between any two
// syllables ("있 / 다면"). This glues each word's syllables with an invisible
// word joiner (U+2060). Words over 12 syllables are left breakable, so a
// strange long line still wraps instead of running off the screen. ass: only
// the Dialogue text, outside {\override} tags, is touched.
QString keepKoreanWords(const QString &text, bool ass = false);

// forced and language are as for TextEncoding::decode.
QList<Track> prepare(const QString &path, const QString &forced, const QString &language, const QString &cacheDirectory);

}
