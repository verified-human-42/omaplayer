#pragma once

#include <QList>
#include <QString>

// SAMI (.smi) subtitles, the format most Korean subtitles come in, written on
// Windows and usually in CP949. One file often carries several languages as
// classes (KRCC for Korean, ENCC for English), which FFmpeg collapses into
// one; here each class becomes its own UTF-8 SRT file, so each language is a
// track of its own, the way Windows players offer them.
namespace Sami {

struct Track {
    QString className;
    QString name;     // as the file names it ("한국어"), or the class
    QString language; // "ko", "en", … or empty when unknown
    QString path;     // the SRT written for it
    int cues = 0;
};

// Writes one SRT per language class into directory and returns them in the
// order the file lists them. Classes with no text are left out.
// forced and language are as for TextEncoding::decode.
QList<Track> convert(const QString &path, const QString &forced, const QString &language, const QString &directory);

}
