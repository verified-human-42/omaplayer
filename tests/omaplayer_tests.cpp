#include <QDir>
#include <QFile>
#include <QKeySequence>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <clocale>

#include <iconv.h>

#include "actions.h"
#include "sami.h"
#include "textencoding.h"
#include "player.h"
#include "settings.h"
#include "theme.h"
#include "timing.h"
#include "translator.h"

Q_LOGGING_CATEGORY(lcTiming, "omaplayer.timing", QtWarningMsg)

class OmaplayerTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        // Resume positions and settings land in a test location, not the user's.
        QStandardPaths::setTestModeEnabled(true);
        std::setlocale(LC_NUMERIC, "C");
    }

    // Theme

    void themeFallsBackToOmarchysDefault()
    {
        QTemporaryDir dir;
        Theme theme(dir.path());
        QVERIFY(theme.dark());
        QCOMPARE(theme.accent(), QString("#7aa2f7"));
        QCOMPARE(theme.stage(), theme.background());
        // A light accent carries the theme's background as its text.
        QCOMPARE(theme.accentForeground(), theme.background());
    }

    void themeReadsThePalette()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml",
              "mode = \"light\"\naccent = \"#5e81ac\"\nbackground = \"#eceff4\"\n"
              "foreground = \"#3b4252\"\nbright_foreground = \"#2e3440\"\n");

        Theme theme(dir.path());
        QVERIFY(!theme.dark());
        QCOMPARE(theme.accent(), QString("#5e81ac"));
        QCOMPARE(theme.accentForeground(), QString("#ffffff"));
        // A light theme still puts the controls on its darkest color.
        QCOMPARE(theme.stage(), QString("#2e3440"));
        QCOMPARE(theme.stageForeground(), QString("#eceff4"));
    }

    void themeIgnoresMalformedColors()
    {
        QTemporaryDir dir;
        QDir(dir.path()).mkpath("theme");
        write(dir.path() + "/theme/colors.toml", "accent = \"yellowish\"\nbackground = \"#12\"\n");

        Theme theme(dir.path());
        QCOMPARE(theme.accent(), QString("#7aa2f7"));
        QCOMPARE(theme.background(), QString("#1a1b26"));
    }

    void themeFollowsHyprlandRounding()
    {
        QTemporaryDir dir;
        const QString response = dir.filePath("response.json"), command = dir.filePath("hyprctl");
        write(command, "#!/bin/sh\ncat '" + response.toUtf8() + "'\n");
        QVERIFY(QFile::setPermissions(command, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner));
        write(response, "{\"int\":0}");

        Theme theme(dir.path(), nullptr, command);
        QCOMPARE(theme.radius(), 0);
        write(response, "{\"int\":6}");
        QTRY_COMPARE(theme.radius(), 6);
    }

    // Translator

    void translatorFallsBackToEnglish()
    {
        QTemporaryDir dir;
        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.language(), QString("en"));
        QVERIFY(translator.translate("Main", "Save").isNull());
    }

    void translatorFollowsTheLanguageFile()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "pt-BR\n");
        write(dir.path() + "/pt.json", R"({"Save": "Guardar", "Open": "Abrir"})");
        write(dir.path() + "/pt-BR.json", R"({"Save": "Salvar"})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "Save"), QString("Salvar"));
        QCOMPARE(translator.translate("Main", "Open"), QString("Abrir"));
        QVERIFY(translator.translate("Main", "Quit").isNull());
    }

    void translatorPicksPluralForms()
    {
        QTemporaryDir dir;
        write(dir.path() + "/language", "ru\n");
        write(dir.path() + "/ru.json", R"({"%n clips": {"one": "%n клип", "few": "%n клипа", "many": "%n клипов", "other": "%n клипа"}})");

        Translator translator(dir.path() + "/language", dir.path());
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 21), QString("%n клип"));
        QCOMPARE(translator.translate("Main", "%n clips", nullptr, 25), QString("%n клипов"));
    }

    // Actions

    void actionsAreWellFormed()
    {
        QStringList ids;
        for (const auto &spec : Actions::specs()) {
            QVERIFY2(!ids.contains(spec.id), qPrintable(spec.id));
            ids.append(spec.id);
            QVERIFY(QByteArray(spec.text).size() > 0);
            if (spec.kind == Actions::Kind::Mpv)
                QVERIFY2(!spec.command.isEmpty(), qPrintable(spec.id));
        }
    }

    void noKeyDoesTwoThings()
    {
        for (const auto &keymap : Actions::keymaps()) {
            QHash<QString, QString> owners;
            for (const auto &spec : Actions::specs())
                for (const auto &key : Actions::keysFor(spec, keymap)) {
                    const QString normalized = QKeySequence(key).toString(QKeySequence::PortableText);
                    QVERIFY2(!normalized.isEmpty(), qPrintable(key));
                    QVERIFY2(!owners.contains(normalized),
                             qPrintable(QString("%1: %2 is both %3 and %4").arg(keymap, key, owners.value(normalized), spec.id)));
                    owners.insert(normalized, spec.id);
                }
        }
    }

    void keymapSwitchesAndLabelsKeys()
    {
        Player player(Player::Output::None);
        Actions actions(&player);
        QCOMPARE(actions.shortcut("seek-back"), QString("←"));
        QCOMPARE(actions.shortcut("fullscreen"), QString("Enter"));
        QCOMPARE(actions.shortcut("frame-forward"), QString("F"));
        actions.setKeymap("mpv");
        QCOMPARE(actions.shortcut("frame-forward"), QString("."));
        actions.setKeymap("nonsense");
        QCOMPARE(actions.keymap(), QString("omaplayer"));
    }

    // Settings

    void settingsNormalizeAndPersist()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("settings.ini");
        Player player(Player::Output::None);
        Actions actions(&player);
        {
            Settings settings(&player, &actions, file);
            settings.set("subtitles/scale", 9.0);
            settings.set("video/scale", "ultra");
            settings.set("interface/hideDelay", "3");
            settings.set("playback/resume", "false");
            QCOMPARE(settings.value("subtitles/scale").toDouble(), 2.0);
            QCOMPARE(settings.value("video/scale").toString(), QString("balanced"));
            QCOMPARE(settings.value("interface/hideDelay").toInt(), 3);
            QCOMPARE(settings.value("playback/resume").toBool(), false);
        }
        Settings reopened(&player, &actions, file);
        QCOMPARE(reopened.value("subtitles/scale").toDouble(), 2.0);
        QCOMPARE(reopened.value("playback/resume").toBool(), false);
    }

    void settingsUndoWhatChanged()
    {
        QTemporaryDir dir;
        Player player(Player::Output::None);
        Actions actions(&player);
        Settings settings(&player, &actions, dir.filePath("settings.ini"));
        settings.set("interface/keymap", "mpv");
        settings.set("video/smooth", true);
        QCOMPARE(settings.changedKeys().size(), 2);
        QCOMPARE(actions.keymap(), QString("mpv"));

        settings.undo("video/smooth");
        QCOMPARE(settings.changedKeys(), QStringList{"interface/keymap"});
        settings.undoAll();
        QVERIFY(settings.changedKeys().isEmpty());
        QCOMPARE(actions.keymap(), QString("omaplayer"));
    }

    void keyBindingsMoveAndPersist()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("settings.ini");
        Player player(Player::Output::None);
        Actions actions(&player);
        {
            Settings settings(&player, &actions, file);
            // M belongs to Mute; giving it to Stop takes it from Mute.
            QCOMPARE(settings.bindKey("stop", "M"), QString("mute"));
            QCOMPARE(actions.keys("stop"), QStringList{"M"});
            QVERIFY(actions.keys("mute").isEmpty());
            settings.bindKey("open-file", "Ctrl+;");
            QCOMPARE(settings.customBindings(), 3);
        }
        Settings reopened(&player, &actions, file);
        QCOMPARE(actions.keys("stop"), QStringList{"M"});
        QCOMPARE(actions.keys("open-file"), (QStringList{"Ctrl+O", "Ctrl+;"}));
        QCOMPARE(actions.shortcut("stop"), QString("M"));

        // Taking a binding back to the layout's keys stops counting as custom.
        reopened.unbindKey("open-file", "Ctrl+;");
        QCOMPARE(reopened.customBindings(), 2);
        reopened.resetAllKeys();
        QCOMPARE(reopened.customBindings(), 0);
        QCOMPARE(actions.keys("mute"), QStringList{"M"});
    }

    void keyPressesBecomeSequences()
    {
        QCOMPARE(Actions::sequence(Qt::Key_L, Qt::ControlModifier, "\x0c"), QString("Ctrl+L"));
        QCOMPARE(Actions::sequence(Qt::Key_Question, Qt::ShiftModifier, "?"), QString("?"));
        QCOMPARE(Actions::sequence(Qt::Key_F7, Qt::NoModifier, ""), QString("F7"));
        QVERIFY(Actions::sequence(Qt::Key_Control, Qt::ControlModifier, "").isEmpty());
    }

    void subtitlesFollowTheLanguage()
    {
        QCOMPARE(Settings::languageCodes("ja"), QString("ja,jpn"));
        QCOMPARE(Settings::languageCodes("ko"), QString("ko,kor"));
        QVERIFY(Settings::languageCodes("zh-TW").startsWith("zh-TW,zh,zh-Hant,chi,zho"));
        QVERIFY(Settings::languageCodes("zh-CN").contains("zh-Hans"));
        QCOMPARE(Settings::languageCodes("pt-BR"), QString("pt-BR,pt,por"));
        QCOMPARE(Settings::legacyCodepage("ko"), QString("cp949"));
        QCOMPARE(Settings::legacyCodepage("zh-TW"), QString("big5"));
        QCOMPARE(Settings::legacyCodepage("en"), QString("auto"));
    }

    // Subtitle text in every encoding it turns up in, decoded with no hint
    // from the language: short, like a real subtitle line or two.
    void encodingsAreDetected_data()
    {
        QTest::addColumn<QString>("codepage");
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("language");
        QTest::newRow("korean") << "cp949" << "안녕하세요. 오늘 날씨가 정말 좋네요.\n어디 가세요?" << "en";
        QTest::newRow("korean short") << "cp949" << "고마워요" << "en";
        QTest::newRow("japanese") << "cp932" << "こんにちは。今日はいい天気ですね。\nどこへ行くの？" << "en";
        QTest::newRow("japanese short") << "cp932" << "ありがとう" << "en";
        QTest::newRow("chinese simplified") << "gb18030" << "你好，今天天气真好。\n你要去哪里？" << "en";
        QTest::newRow("chinese traditional") << "big5" << "你好，今天天氣真好。\n你要去哪裡？" << "en";
        QTest::newRow("russian") << "cp1251" << "Привет! Как дела?\nЯ давно тебя не видел." << "en";
        QTest::newRow("russian koi8") << "koi8-r" << "Привет! Как дела?\nЯ давно тебя не видел." << "en";
        QTest::newRow("greek") << "cp1253" << "Γεια σου! Τι κάνεις;\nΠού πας;" << "en";
        QTest::newRow("hebrew") << "cp1255" << "שלום! מה שלומך?\nלאן אתה הולך?" << "en";
        QTest::newRow("arabic") << "cp1256" << "مرحبا! كيف حالك؟\nإلى أين تذهب؟" << "en";
        QTest::newRow("thai") << "cp874" << "สวัสดีครับ วันนี้อากาศดีมาก" << "th";
        QTest::newRow("french") << "cp1252" << "Ça va? Très bien, merci.\nOù est la gare?" << "en";
        QTest::newRow("german") << "cp1252" << "Übermorgen gehen wir in die Stadt. Schöne Grüße!" << "en";
        QTest::newRow("polish") << "cp1250" << "Cześć! Właśnie wróciłem do domu.\nJak się masz?" << "en";
        QTest::newRow("turkish") << "cp1254" << "Merhaba! Nasılsın?\nBugün hava çok güzel, değil mi?" << "tr";
        QTest::newRow("czech") << "cp1250" << "Děkuji, mám se dobře. Příště přijdu." << "cs";

        // Typical lines, short ones included, still with no language hint
        // except for the Latin-script codepages that share most letters.
        const QList<std::tuple<const char *, const char *, const char *>> lines = {
            {"cp949", "뭐 하는 거야? 빨리 와!", "en"}, {"cp949", "알았어. 내일 보자.", "en"},
            {"cp949", "<i>그 사람은 누구야?</i>", "en"}, {"cp949", "- 정말이야?\n- 그래, 진짜야.", "en"},
            {"cp949", "네", "en"}, {"cp949", "사랑해", "ja"},
            {"cp932", "何をしているの？早く来て！", "en"}, {"cp932", "わかった。また明日ね。", "en"},
            {"cp932", "- 本当に？\n- うん、本当だよ。", "en"}, {"cp932", "はい", "en"}, {"cp932", "ありがとう", "ko"},
            {"gb18030", "你在干什么？快点来！", "en"}, {"gb18030", "好的，明天见。", "en"},
            {"gb18030", "- 真的吗？\n- 是的，真的。", "en"}, {"gb18030", "谢谢你", "ko"},
            {"big5", "你在幹什麼？快點來！", "en"}, {"big5", "好的，明天見。", "en"},
            {"big5", "- 真的嗎？\n- 是的，真的。", "en"}, {"big5", "謝謝你", "ko"},
            {"cp1251", "Что ты делаешь? Иди сюда!", "en"}, {"cp1251", "Ладно, увидимся завтра.", "en"},
            {"cp1251", "- Правда?\n- Да, правда.", "en"},
            {"cp1253", "Τι κάνεις; Έλα γρήγορα!", "en"}, {"cp1253", "Εντάξει, τα λέμε αύριο.", "en"},
            {"cp1255", "מה אתה עושה? בוא מהר!", "en"}, {"cp1255", "בסדר, נתראה מחר.", "en"},
            {"cp1256", "ماذا تفعل؟ تعال بسرعة!", "en"}, {"cp1256", "حسنا، أراك غدا.", "en"},
            {"cp1252", "Qu'est-ce que tu fais? Viens vite!", "en"}, {"cp1252", "D'accord, à demain.", "en"},
            {"cp1252", "¿Qué estás haciendo? ¡Ven rápido!", "en"}, {"cp1252", "Está bien, hasta mañana.", "ko"},
            {"cp1252", "O que você está fazendo? Venha rápido!", "en"}, {"cp1252", "Gut, bis morgen. Schöne Grüße.", "en"},
            {"cp1250", "Co robisz? Chodź szybko!", "pl"}, {"cp1250", "Właśnie wróciłem.", "en"},
            {"cp1254", "Ne yapıyorsun? Çabuk gel!", "tr"},
        };
        for (const auto &[codepage, text, language] : lines)
            QTest::newRow(qPrintable(QString("%1: %2").arg(codepage, QString::fromUtf8(text).left(12))))
                << QString(codepage) << QString::fromUtf8(text) << QString(language);
    }

    void encodingsAreDetected()
    {
        QFETCH(QString, codepage);
        QFETCH(QString, text);
        QFETCH(QString, language);
        const QByteArray bytes = encode(text, codepage);
        const auto result = TextEncoding::decode(bytes, {}, language);
        QCOMPARE(result.text, text);
        // Plain ASCII is the same in every codepage, and valid UTF-8.
        const bool ascii = std::all_of(text.begin(), text.end(), [](QChar c) { return c.unicode() < 0x80; });
        QCOMPARE(result.encoding, ascii ? QString("utf-8") : codepage);
    }

    void unicodeIsRecognised()
    {
        const QString text = "한글 자막 · 日本語 · 中文";
        QCOMPARE(TextEncoding::decode(text.toUtf8()).encoding, QString("utf-8"));
        QCOMPARE(TextEncoding::decode("\xEF\xBB\xBF" + text.toUtf8()).text, text);
        QByteArray utf16("\xFF\xFE");
        utf16.append(reinterpret_cast<const char *>(text.utf16()), text.size() * 2);
        QCOMPARE(TextEncoding::decode(utf16).text, text);
        // Notepad's "Unicode" without its mark is still UTF-16.
        const QByteArray bare(reinterpret_cast<const char *>(QString("Hello 안녕").utf16()), 16);
        QCOMPARE(TextEncoding::decode(bare).encoding, QString("utf-16le"));
        // A forced codepage is taken at its word.
        QCOMPARE(TextEncoding::decode(encode("고마워요", "cp949"), "cp949").text, QString("고마워요"));
    }

    void samiSplitsLanguages()
    {
        QTemporaryDir dir;
        const QString smi = dir.filePath("drama.smi");
        write(smi, encode(
            "<SAMI><HEAD><STYLE TYPE=\"text/css\"><!--\n"
            ".KRCC { Name:한국어; lang:ko-KR; SAMIType:CC; }\n"
            ".ENCC { Name:English; lang:en-US; SAMIType:CC; }\n--></STYLE></HEAD><BODY>\n"
            "<SYNC Start=1000><P Class=KRCC>안녕하세요<br>두 줄 &amp; 자막\n"
            "<SYNC Start=1000><P Class=ENCC>Hello there\n"
            "<SYNC Start=4000><P Class=KRCC>&nbsp;\n"
            "<SYNC Start=4500><P Class=ENCC>&nbsp;\n"
            "<SYNC Start=7000><P Class=KRCC><font color=\"ffff00\">노란색</font> 자막\n"
            "</BODY></SAMI>\n", "cp949"));

        const auto tracks = Sami::convert(smi, {}, "en", dir.filePath("out"));
        QCOMPARE(tracks.size(), 2);
        QCOMPARE(tracks[0].name, QString("한국어"));
        QCOMPARE(tracks[0].language, QString("ko"));
        QCOMPARE(tracks[0].cues, 2);
        QCOMPARE(tracks[1].language, QString("en"));

        QFile korean(tracks[0].path);
        QVERIFY(korean.open(QIODevice::ReadOnly));
        const QString srt = QString::fromUtf8(korean.readAll());
        QVERIFY(srt.contains("00:00:01,000 --> 00:00:04,000\n안녕하세요\n두 줄 & 자막"));
        QVERIFY(srt.contains("<font color=\"#ffff00\">노란색</font> 자막"));
    }

    // Player

    void playerPlaysAndReports()
    {
        QTemporaryDir dir;
        const QString clip = makeClip(dir.filePath("clip.mkv"));
        Player player(Player::Output::None);
        QSignalSpy osd(&player, &Player::osd);

        player.openPaths({clip});
        QTRY_VERIFY_WITH_TIMEOUT(!player.idle() && player.duration() > 1.5, 10000);
        QVERIFY(!player.opening());
        QVERIFY(player.error().isEmpty());
        // A file starting shows its name in the corner.
        QTRY_VERIFY(osd.count() > 0);
        QCOMPARE(osd.first().first().toString(), QString("file"));
        osd.clear();

        player.command({"add", "volume", "-10"}, "volume");
        QTRY_VERIFY(osd.count() > 0);
        QCOMPARE(osd.last().first().toString(), QString("volume"));
        QCOMPARE(player.volume(), 90.0);

        player.seekTo(1.0, true);
        QTRY_VERIFY(qAbs(player.position() - 1.0) < 0.2);
    }

    void playerQueuesTheFolderInNaturalOrder()
    {
        QTemporaryDir dir;
        const QString clip = makeClip(dir.filePath("Episode 2.mkv"));
        QVERIFY(QFile::copy(clip, dir.filePath("Episode 10.mkv")));
        QVERIFY(QFile::copy(clip, dir.filePath("Episode 1.mkv")));
        write(dir.filePath("notes.txt"), "not media");

        Player player(Player::Output::None);
        player.command({"set", "pause", "yes"});
        player.openPaths({clip});
        QTRY_COMPARE_WITH_TIMEOUT(player.playlistCount(), 3, 10000);
        QTRY_COMPARE(player.playlistPosition(), 1);

        QStringList names;
        for (const auto &entry : player.playlist())
            names.append(QFileInfo(entry.toMap().value("filename").toString()).fileName());
        QCOMPARE(names, (QStringList{"Episode 1.mkv", "Episode 2.mkv", "Episode 10.mkv"}));
    }

    void playerLoadsSubtitlesInAnyEncoding()
    {
        QTemporaryDir dir;
        const QString clip = makeClip(dir.filePath("드라마 01화.mkv"));
        write(dir.filePath("드라마 01화.smi"), encode(
            "<SAMI><HEAD><STYLE><!--\n.KRCC { Name:한국어; lang:ko-KR; }\n.ENCC { Name:English; lang:en-US; }\n--></STYLE></HEAD>"
            "<BODY><SYNC Start=0><P Class=KRCC>안녕하세요\n<SYNC Start=0><P Class=ENCC>Hello\n</BODY></SAMI>", "cp949"));
        write(dir.filePath("드라마 01화.ja.srt"), encode("1\n00:00:00,000 --> 00:00:01,500\nこんにちは\n", "cp932"));
        write(dir.filePath("unrelated.srt"), "1\n00:00:00,000 --> 00:00:01,000\nnope\n");

        Player player(Player::Output::None);
        player.set("slang", "ko,kor");
        player.command({"set", "pause", "yes"});
        player.openPaths({clip});

        QStringList titles;
        QString selected;
        QTRY_VERIFY_WITH_TIMEOUT([&] {
            titles.clear();
            for (const auto &track : player.tracks()) {
                const auto map = track.toMap();
                if (map.value("type") != "sub")
                    continue;
                titles.append(map.value("title").toString());
                if (map.value("selected").toBool())
                    selected = map.value("title").toString();
            }
            return titles.size() == 3;
        }(), 10000);
        QVERIFY(titles.contains("한국어"));
        QVERIFY(titles.contains("English"));
        QVERIFY(titles.contains("ja"));
        QCOMPARE(selected, QString("한국어"));
    }

    void playerSaysWhatWentWrong()
    {
        QTemporaryDir dir;
        write(dir.filePath("broken.mkv"), "this is not a video");
        Player player(Player::Output::None);
        player.openPaths({dir.filePath("broken.mkv")});
        QTRY_VERIFY_WITH_TIMEOUT(!player.error().isEmpty(), 10000);
        QVERIFY(player.error().contains("broken.mkv"));
        QVERIFY(!player.opening());
    }

private:
    static QByteArray encode(const QString &text, const QString &codepage)
    {
        iconv_t converter = iconv_open(codepage.toLatin1().constData(), "UTF-8");
        if (converter == iconv_t(-1))
            return {};
        QByteArray in = text.toUtf8(), out(in.size() * 4 + 16, 0);
        char *source = in.data(), *target = out.data();
        size_t left = in.size(), room = out.size();
        iconv(converter, &source, &left, &target, &room);
        iconv_close(converter);
        out.resize(out.size() - room);
        return out;
    }

    static void write(const QString &path, const QByteArray &contents)
    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(contents);
    }

    // A two-second test pattern with a tone, made with ffmpeg.
    static QString makeClip(const QString &path)
    {
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty())
            QTest::qSkip("ffmpeg isn't installed", __FILE__, __LINE__);
        QProcess ffmpeg;
        ffmpeg.start("ffmpeg", {"-hide_banner", "-loglevel", "error", "-y",
                                "-f", "lavfi", "-i", "testsrc2=size=320x240:rate=25",
                                "-f", "lavfi", "-i", "sine=frequency=440",
                                "-t", "2", "-c:v", "libx264", "-preset", "ultrafast", "-c:a", "aac", path});
        ffmpeg.waitForFinished(30000);
        return path;
    }
};

QTEST_MAIN(OmaplayerTests)
#include "omaplayer_tests.moc"
