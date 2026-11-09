#pragma once

#include <QByteArray>
#include <QString>

// Turns subtitle bytes of any encoding into text. Subtitles out in the world
// are mostly written on Windows in their language's old codepage (CP949 for
// Korean, CP932 for Japanese, GB18030 and Big5 for Chinese, CP1251 for
// Russian…), and short files defeat generic detectors. Here every plausible
// codepage is tried and the one whose text reads most like its language wins.
namespace TextEncoding {

struct Result {
    QString text;
    QString encoding; // "utf-8", "utf-16le", "cp949", …
};

// forced: an iconv codepage name to use as is, or empty to detect.
// language: Omarchy's language code, which breaks near ties.
Result decode(const QByteArray &bytes, const QString &forced = {}, const QString &language = {});

// Strict conversion through iconv; invalid counts the bytes it had to skip.
QString fromCodepage(const QByteArray &bytes, const QString &codepage, int *invalid = nullptr);

}
