#include "textencoding.h"

#include <QHash>
#include <QSet>
#include <QStringDecoder>
#include <cerrno>
#include <iconv.h>

namespace {

// The codepages subtitles are written in, most widespread first, so a tie
// goes to the likelier one.
const QStringList &candidates() {
    static const QStringList list = {
        "cp1252", "cp949", "cp932", "gb18030", "big5", "cp1251", "cp1250", "cp1254",
        "cp1253", "cp1256", "cp1255", "cp1257", "koi8-r",
    };
    return list;
}

// Thai and Vietnamese codepages turn almost any bytes into letters that look
// fine one by one, so they're only tried when Omarchy runs in that language.
const QStringList &hintedOnly() {
    static const QStringList list = {"cp874", "cp1258"};
    return list;
}

QString legacyFor(const QString &language) {
    static const QHash<QString, QString> codepages = {
        {"ko", "cp949"}, {"ja", "cp932"}, {"zh-CN", "gb18030"}, {"zh-TW", "big5"},
        {"ru", "cp1251"}, {"uk", "cp1251"}, {"bg", "cp1251"}, {"sr", "cp1251"},
        {"pl", "cp1250"}, {"cs", "cp1250"}, {"sk", "cp1250"}, {"hr", "cp1250"}, {"sl", "cp1250"},
        {"ro", "cp1250"}, {"hu", "cp1250"}, {"el", "cp1253"}, {"tr", "cp1254"}, {"he", "cp1255"},
        {"ar", "cp1256"}, {"fa", "cp1256"}, {"ur", "cp1256"}, {"lt", "cp1257"}, {"lv", "cp1257"},
        {"et", "cp1257"}, {"vi", "cp1258"}, {"th", "cp874"},
    };
    return codepages.value(language);
}

QSet<char32_t> setOf(const char *utf8) {
    QSet<char32_t> set;
    for (char32_t c : QString::fromUtf8(utf8).toUcs4())
        if (c != ' ')
            set.insert(c);
    return set;
}

// The syllables and characters that make up most of everyday speech. Real
// text hits them constantly; text decoded with the wrong codepage lands on
// valid but rare characters instead.
const QSet<char32_t> &korean() {
    static const auto set = setOf(
        "이 다 는 하 고 가 에 의 을 를 지 요 서 한 사 어 자 아 니 그 로 나 기 도 리 시 해 거 게 있 수 대 것 라 "
        "면 마 들 주 내 보 무 우 일 정 인 전 만 상 소 부 습 까 야 네 죠 했 데 은 오 말 저 안 여 미 진 제 생 장 "
        "성 위 신 경 동 원 화 구 세 비 모 실 계 너 냐 잖 줘 왜 뭐 됐 걸 좋 못 알 같 봐 와 과 께 더 또 잘 겠 어 "
        "요 입 니 런 렇 래 랑 때 날 할 될 된 났 갔 왔 았 었 엇 음 운 근 금 들 듯 든 단 달 당 돼 둘 뭘 몰 물 "
        "바 반 받 발 밖 번 벌 별 본 봤 빨 빠 쁘 싶 씨 아 앞 애 얘 얼 엄 없 엔 영 예 온 올 외 요 웃 응 이 잠 "
        "좀 줄 중 집 짜 찾 처 천 친 침 크 큰 타 터 토 통 투 트 파 편 포 하 학 함 합 해 했 행 허 헤 혼 후 흐");
    return set;
}

// The 2,350 syllables of KS X 1001, the ones EUC-KR can write: everyday
// Korean. CP949's other 8,822 are real but rare, and a wrong codepage lands
// on them all the time.
const QSet<char32_t> &everydayHangul() {
    static const QSet<char32_t> set = [] {
        QSet<char32_t> syllables;
        iconv_t converter = iconv_open("EUC-KR", "UTF-32LE");
        if (converter == iconv_t(-1))
            return syllables;
        for (char32_t c = 0xAC00; c <= 0xD7A3; ++c) {
            char in[4] = {char(c & 0xFF), char((c >> 8) & 0xFF), 0, 0}, out[8];
            char *source = in, *target = out;
            size_t left = 4, room = sizeof out;
            if (iconv(converter, &source, &left, &target, &room) != size_t(-1))
                syllables.insert(c);
        }
        iconv_close(converter);
        return syllables;
    }();
    return set;
}

const QSet<char32_t> &simplified() {
    static const auto set = setOf(
        "的 一 是 不 了 人 我 在 有 他 这 个 们 中 来 上 大 为 和 国 地 到 以 说 时 要 就 出 会 可 也 你 对 生 能 而 "
        "子 那 得 于 着 下 自 之 年 过 发 后 作 里 用 道 行 所 然 家 种 事 成 方 多 经 么 去 法 学 如 都 同 现 当 没 "
        "动 面 起 看 定 天 分 还 进 好 小 部 其 些 主 样 理 心 她 本 前 开 但 因 只 从 想 实 吗 呢 吧 啊 嗯 怎 什 谁 "
        "哪 别 让 给 把 被 很 太 真 再 知 走 快 点 情 她 爸 妈 哥 姐 儿 女 男 谢 对 吃 喝 等 叫 告 诉 觉 得 应 该 "
        "已 经 因 为 所 以 如 果 虽 然 但 是 事 情 时 候 东 西 问 题 们 吗 呀 嘛 啦");
    return set;
}

const QSet<char32_t> &traditional() {
    static const auto set = setOf(
        "的 一 是 不 了 人 我 在 有 他 這 個 們 中 來 上 大 為 和 國 地 到 以 說 時 要 就 出 會 可 也 你 對 生 能 而 "
        "子 那 得 於 著 下 自 之 年 過 發 後 作 裡 用 道 行 所 然 家 種 事 成 方 多 經 麼 去 法 學 如 都 同 現 當 沒 "
        "動 面 起 看 定 天 分 還 進 好 小 部 其 些 主 樣 理 心 她 本 前 開 但 因 只 從 想 實 嗎 呢 吧 啊 嗯 怎 什 誰 "
        "哪 別 讓 給 把 被 很 太 真 再 知 走 快 點 情 爸 媽 哥 姐 兒 女 男 謝 吃 喝 等 叫 告 訴 覺 應 該 已 東 西 問 "
        "題 呀 嘛 啦 喔 耶");
    return set;
}

const QSet<char32_t> &japaneseKanji() {
    static const auto set = setOf(
        "日 一 人 年 大 十 二 本 中 長 出 三 時 行 見 月 分 後 前 生 五 間 上 東 四 今 金 九 入 学 高 円 子 外 八 "
        "六 下 来 気 小 七 山 話 女 北 午 百 書 先 名 川 千 水 半 男 西 電 校 語 土 木 聞 食 車 何 南 万 毎 白 天 "
        "母 火 右 読 友 左 休 父 雨 私 彼 僕 君 俺 事 自 分 思 言 当 早 帰 待 願");
    return set;
}

const QSet<char32_t> &russian() {
    static const auto set = setOf("о е а и н т с р в л к м д п у я ы ь г з б ч й");
    return set;
}

const QSet<char32_t> &greek() {
    static const auto set = setOf("α ε ο ι τ ν σ η κ π ρ μ λ υ ς ω ά έ ί ό ή ύ ώ");
    return set;
}

const QSet<char32_t> &hebrew() {
    static const auto set = setOf("י ו ה א ל ר מ ת ב נ ש כ ע ד");
    return set;
}

enum class Script { None, Latin, Hangul, Kana, Han, Cyrillic, Greek, Hebrew, Arabic, Thai };

Script scriptOf(char32_t c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= 0xC0 && c <= 0x24F && c != 0xD7 && c != 0xF7))
        return Script::Latin;
    if ((c >= 0xAC00 && c <= 0xD7A3) || (c >= 0x3130 && c <= 0x318F))
        return Script::Hangul;
    if (c >= 0x3041 && c <= 0x30FF)
        return Script::Kana;
    if ((c >= 0x4E00 && c <= 0x9FFF) || (c >= 0x3400 && c <= 0x4DBF) || (c >= 0xF900 && c <= 0xFAFF))
        return Script::Han;
    if (c >= 0x400 && c <= 0x4FF)
        return Script::Cyrillic;
    if (c >= 0x370 && c <= 0x3FF)
        return Script::Greek;
    if (c >= 0x5D0 && c <= 0x5EA)
        return Script::Hebrew;
    if (c >= 0x600 && c <= 0x6FF)
        return Script::Arabic;
    if (c >= 0xE00 && c <= 0xE7F)
        return Script::Thai;
    return Script::None;
}

// How much one character looks like real subtitle text in that codepage's
// language: 1 for the everyday core, a little for the merely valid, and a
// penalty for what only a wrong guess produces.
double weight(char32_t c, const QString &codepage) {
    if (c == 0xFFFD || (c >= 0x80 && c <= 0x9F) || (c >= 0xE000 && c <= 0xF8FF))
        return -1;
    if (c >= 0xAC00 && c <= 0xD7A3)
        return korean().contains(c) ? 1.0 : everydayHangul().contains(c) ? 0.5 : 0.03;
    if (c >= 0x3130 && c <= 0x318F) // ㅋㅋ, ㅠㅠ
        return 0.4;
    if (c >= 0x3041 && c <= 0x309F)
        return 1.0;
    if (c >= 0x30A0 && c <= 0x30FF)
        return 0.9;
    if (c >= 0xFF61 && c <= 0xFF9F) // half-width katakana: what CP949 text looks like read as CP932
        return 0.05;
    if (c >= 0x4E00 && c <= 0x9FFF) {
        if (codepage == "big5")
            return traditional().contains(c) ? 1.0 : 0.3;
        if (codepage == "cp932")
            return japaneseKanji().contains(c) || simplified().contains(c) ? 0.9 : 0.3;
        if (codepage == "cp949") // Hanja are rare in Korean subtitles
            return 0.15;
        return simplified().contains(c) || traditional().contains(c) ? 1.0 : 0.3;
    }
    if ((c >= 0x3400 && c <= 0x4DBF) || (c >= 0xF900 && c <= 0xFAFF))
        return 0.02;
    if (c >= 0x400 && c <= 0x4FF) {
        const QChar ch(static_cast<char16_t>(c));
        if (russian().contains(c))
            return 0.6;
        return ch.isLower() ? 0.4 : 0.25;
    }
    if (c >= 0x370 && c <= 0x3FF) {
        if (greek().contains(c))
            return 0.95;
        return QChar(static_cast<char16_t>(c)).isLower() ? 0.5 : 0.3;
    }
    if (c >= 0x5D0 && c <= 0x5EA)
        return hebrew().contains(c) ? 1.0 : 0.65;
    if (c >= 0x591 && c <= 0x5C7) // Hebrew points: absent from subtitles
        return -0.5;
    if (c >= 0x620 && c <= 0x64A)
        return 0.85;
    if (c >= 0xE01 && c <= 0xE2E)
        return 0.85;
    if (c >= 0xE30 && c <= 0xE5B)
        return 0.6;
    if (c >= 0xC0 && c <= 0x24F && c != 0xD7 && c != 0xF7)
        return 0.7;
    if ((c >= 0x2010 && c <= 0x2027) || (c >= 0x3000 && c <= 0x303F) || (c >= 0xFF01 && c <= 0xFF5E) || c == 0xA0)
        return 0.6;
    if (c == 0xAB || c == 0xBB || c == 0xB0 || c == 0xA9 || c == 0xB7 || c == 0xBF || c == 0xA1)
        return 0.4;
    if (c >= 0xA2 && c <= 0xBF)
        return 0.1;
    return 0.05;
}

struct Candidate {
    QString codepage;
    QString text;
    double score = -10;
};

double score(const QString &text, const QString &codepage, int invalid) {
    double total = 0;
    int considered = 0, asciiLetters = 0, accented = 0, mixed = 0, pairs = 0;
    Script previous = Script::None;
    const QList<uint> chars = text.toUcs4();
    for (qsizetype i = 0; i < chars.size(); ++i) {
        const char32_t c = chars[i];
        const Script script = scriptOf(c);
        if (script != Script::None) {
            if (c < 0x80)
                ++asciiLetters;
            if (previous != Script::None) {
                ++pairs;
                // Inside a word, scripts don't change, except that Japanese runs
                // kanji into kana and Korean can run Hanja into Hangul; a wrong
                // codepage mixes them.
                const bool asciiPair = (previous == Script::Latin && c < 0x80) || (script == Script::Latin && chars[i - 1] < 0x80);
                const auto together = [&](Script a, Script b) {
                    return (previous == a && script == b) || (previous == b && script == a);
                };
                if (script != previous && !asciiPair && !together(Script::Han, Script::Kana)
                    && !together(Script::Han, Script::Hangul))
                    ++mixed;
            }
        }
        previous = script;
        if (c < 0x80)
            continue;
        ++considered;
        if (script == Script::Latin)
            ++accented;
        // Russian reads in familiar pairs; another script read as CP1251
        // makes letters, but rarely these pairs.
        if (i > 0 && c >= 0x430 && c <= 0x44F && chars[i - 1] >= 0x430 && chars[i - 1] <= 0x44F) {
            static const QSet<QString> pairs = [] {
                QSet<QString> set;
                for (const auto &pair : QStringLiteral(
                         "ст но то на ен ов ни ра во ко ет пр ал ли ер ан по ре ос ро ть го ка ол ит ло ес ор да ва "
                         "де ой те ле ак ом ел ем им ск ин ег ие од ма ла ат ам ри ми ты ая ую ой ей ий ых их не "
                         "ет вы ди ду же за зд ка ке ки ко ку ле лю мо мы на не ни но ну об од ок он ор от па пе "
                         "пи по ре ри ро ру са се си сл см со сп ст та те ти то тр ту ты уж хо че чт чи шь ще эт "
                         "юб яз ят ве вн вс вс дн до дру ев ед ез ел ем ен ер ес ет еч ещ жи ит ич ко кт ла ль ня ря").split(' '))
                    set.insert(pair);
                return set;
            }();
            QString pair;
            pair += QChar(static_cast<char16_t>(chars[i - 1]));
            pair += QChar(static_cast<char16_t>(c));
            if (pairs.contains(pair))
                total += 0.45;
        }
        // Hebrew final letters only end words.
        if ((c == 0x5DA || c == 0x5DD || c == 0x5DF || c == 0x5E3 || c == 0x5E5) && i + 1 < chars.size()
            && chars[i + 1] >= 0x5D0 && chars[i + 1] <= 0x5EA)
            total -= 1;
        total += weight(c, codepage);
    }
    if (considered == 0)
        return 1;
    double result = total / considered;
    // Accented letters season real Latin-script text; when they're most of
    // the letters, it's another script read with a Latin codepage.
    if (accented > 0 && accented > asciiLetters * 0.4)
        result *= 0.3;
    // A text that's nearly all ASCII letters is Latin script; a stray letter
    // of another alphabet in it is a wrong guess at an accented letter.
    const int otherScript = considered - accented;
    if (otherScript > 0 && otherScript <= 3 && asciiLetters >= 8 * otherScript && accented == 0)
        result -= 0.8;
    if (pairs > 0)
        result -= 2.0 * mixed / pairs;
    result -= 3.0 * invalid / considered;
    return result;
}

bool looksLikeUtf16(const QByteArray &bytes, bool littleEndian) {
    if (bytes.size() < 8)
        return false;
    int zeros = 0, checked = 0;
    for (qsizetype i = littleEndian ? 1 : 0; i < bytes.size() && checked < 4000; i += 2, ++checked)
        if (bytes[i] == 0)
            ++zeros;
    return checked > 0 && zeros > checked * 0.3;
}

// Valid UTF-8, or UTF-8 with a stray bad byte or two among plenty of good
// multi-byte sequences (an edit in another program).
bool isUtf8(const QByteArray &bytes, QString *text) {
    QStringDecoder decoder(QStringConverter::Utf8);
    *text = decoder(bytes);
    if (!decoder.hasError())
        return true;
    const int bad = text->count(QChar(0xFFFD));
    int multibyte = 0;
    for (const char byte : bytes)
        if ((static_cast<unsigned char>(byte) & 0xC0) == 0xC0)
            ++multibyte;
    return bad <= 2 && multibyte >= 20 * bad;
}
}

QString TextEncoding::fromCodepage(const QByteArray &bytes, const QString &codepage, int *invalid) {
    if (invalid)
        *invalid = 0;
    iconv_t converter = iconv_open("UTF-8", codepage.toLatin1().constData());
    if (converter == iconv_t(-1)) {
        if (invalid)
            *invalid = bytes.size();
        return QString::fromLatin1(bytes);
    }
    QByteArray out;
    char buffer[16384];
    char *in = const_cast<char *>(bytes.constData());
    size_t left = bytes.size();
    while (left > 0) {
        char *cursor = buffer;
        size_t room = sizeof buffer;
        const size_t result = iconv(converter, &in, &left, &cursor, &room);
        out.append(buffer, cursor - buffer);
        if (result == size_t(-1) && errno != E2BIG) {
            // A byte the codepage doesn't know (or a truncated sequence at
            // the end): mark it and carry on.
            out.append("\xEF\xBF\xBD");
            ++in;
            --left;
            if (invalid)
                ++*invalid;
        }
    }
    char *cursor = buffer;
    size_t room = sizeof buffer;
    iconv(converter, nullptr, nullptr, &cursor, &room);
    out.append(buffer, cursor - buffer);
    iconv_close(converter);
    return QString::fromUtf8(out);
}

TextEncoding::Result TextEncoding::decode(const QByteArray &bytes, const QString &forced, const QString &language) {
    if (bytes.startsWith("\xEF\xBB\xBF"))
        return {QString::fromUtf8(bytes.mid(3)), "utf-8"};
    if (bytes.startsWith("\xFF\xFE"))
        return {QStringDecoder(QStringConverter::Utf16LE)(bytes.mid(2)), "utf-16le"};
    if (bytes.startsWith("\xFE\xFF"))
        return {QStringDecoder(QStringConverter::Utf16BE)(bytes.mid(2)), "utf-16be"};
    if (looksLikeUtf16(bytes, true))
        return {QStringDecoder(QStringConverter::Utf16LE)(bytes), "utf-16le"};
    if (looksLikeUtf16(bytes, false))
        return {QStringDecoder(QStringConverter::Utf16BE)(bytes), "utf-16be"};

    QString utf8;
    if (isUtf8(bytes, &utf8))
        return {utf8, "utf-8"};
    if (!forced.isEmpty() && forced != "auto" && forced != "utf-8")
        return {fromCodepage(bytes, forced), forced};

    // Every codepage that reads the bytes at all, scored by how much the
    // result reads like its language. The language Omarchy runs in gets a
    // nudge, enough to settle a near tie and no more.
    const QByteArray sample = bytes.size() > 65536 ? bytes.left(bytes.indexOf('\n', 65536) + 1) : bytes;
    const QString preferred = legacyFor(language);
    QStringList tried = candidates();
    if (hintedOnly().contains(preferred))
        tried.prepend(preferred);
    Candidate best;
    for (const auto &codepage : tried) {
        int invalid = 0;
        const QString text = fromCodepage(sample, codepage, &invalid);
        double value = score(text, codepage, invalid);
        if (codepage == preferred)
            value += 0.04;
        // OMAPLAYER_DEBUG_ENCODING=1 shows every candidate's score.
        static const bool debug = qEnvironmentVariableIsSet("OMAPLAYER_DEBUG_ENCODING");
        if (debug)
            qInfo("encoding %-8s %6.3f  %s", qPrintable(codepage), value, qPrintable(text.left(40).replace('\n', ' ')));
        if (value > best.score)
            best = {codepage, text, value};
    }
    return {sample.size() == bytes.size() ? best.text : fromCodepage(bytes, best.codepage), best.codepage};
}
