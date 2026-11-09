#include <QClipboard>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>
#include <QKeySequence>
#include <QProcess>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>
#include <clocale>
#include <functional>
#include <memory>

#include <iconv.h>

#include "actions.h"
#include "desktop.h"
#include "mpris.h"
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
            // M belongs to Mute; giving it to Shuffle takes it from Mute.
            QCOMPARE(settings.bindKey("shuffle", "M"), QString("mute"));
            QCOMPARE(actions.keys("shuffle"), QStringList{"M"});
            QVERIFY(actions.keys("mute").isEmpty());
            settings.bindKey("open-file", "Ctrl+;");
            QCOMPARE(settings.customBindings(), 3);
        }
        Settings reopened(&player, &actions, file);
        QCOMPARE(actions.keys("shuffle"), QStringList{"M"});
        QCOMPARE(actions.keys("open-file"), (QStringList{"Ctrl+O", "Ctrl+;"}));
        QCOMPARE(actions.shortcut("shuffle"), QString("M"));

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

    // PotPlayer's features, one by one

    void newActionsHaveTheirKeys()
    {
        Player player(Player::Output::None);
        Actions actions(&player);
        const QHash<QString, QString> expected = {
            {"loop-start", "["}, {"loop-end", "]"}, {"loop-clear", "\\"}, {"go-to-time", "Ctrl+G"},
            {"chapter-previous", "Ctrl+PgUp"}, {"chapter-next", "Ctrl+PgDown"},
            {"screenshot", "Ctrl+E"}, {"copy-frame", "Ctrl+C"},
            {"brightness-down", "W"}, {"brightness-up", "E"}, {"contrast-down", "R"}, {"contrast-up", "T"},
            {"saturation-down", "Y"}, {"saturation-up", "U"}, {"hue-down", "I"}, {"hue-up", "O"}, {"picture-reset", "Q"},
            {"audio-earlier", "<"}, {"audio-later", ">"}, {"fill", "Ctrl+Return"}, {"fullscreen", "Alt+Return"},
            {"seek-start", "Backspace"}, {"seek-middle", "Ctrl+Backspace"}, {"seek-near-end", "Shift+Backspace"},
            {"seek-back-minute", "Shift+Left"}, {"seek-forward-5-minutes", "Ctrl+Alt+Right"}, {"go-to-time", "G"},
            {"sub-line-previous", "Home"}, {"sub-line-next", "End"}, {"sub-line-replay", "Ctrl+Home"},
            {"next-subtitle", "L"}, {"next-audio", "A"}, {"sub-sync-reset", "/"}, {"mirror", "Ctrl+Z"},
            {"reopen-last", "Ctrl+Y"}, {"stop", "F4"}, {"quit", "Ctrl+Q"}, {"normalize", "Shift+N"},
            {"sub-up", "Alt+Up"}, {"sub-down", "Alt+Down"}, {"sub-bigger", "Alt+PgUp"}, {"sub-smaller", "Alt+PgDown"},
        };
        for (auto it = expected.begin(); it != expected.end(); ++it)
            QVERIFY2(actions.keys(it.key()).contains(it.value()), qPrintable(it.key()));
        // PotPlayer's Q resets the picture; quitting is Ctrl+Q.
        QVERIFY(!actions.keys("quit").contains("Q"));
        // Menu-only actions exist too, without keys.
        for (const auto &id : {"aspect", "fill", "rotate", "next-second-subtitle", "audio-sync-reset", "repeat"})
            QVERIFY2(!actions.text(id).isEmpty(), id);
        // The start screen keeps to everyday keys.
        int everyday = 0;
        for (const auto &entry : actions.list())
            everyday += entry.toMap().value("everyday").toBool() && !entry.toMap().value("keys").toStringList().isEmpty();
        QVERIFY(everyday <= 32);
    }

    void seekStepsFollowSettings()
    {
        QTemporaryDir dir;
        Player player(Player::Output::None);
        Actions actions(&player);
        Settings settings(&player, &actions, dir.filePath("settings.ini"));
        settings.applyAll();
        QCOMPARE(actions.text("seek-forward"), QString("Forward 5 Seconds"));
        settings.set("playback/seekStep", "10");
        settings.set("playback/seekLongStep", "60");
        QCOMPARE(actions.text("seek-forward"), QString("Forward 10 Seconds"));
        QCOMPARE(actions.text("seek-back-long"), QString("Back 60 Seconds"));

        const QString clip = makeClip(dir.filePath("clip.mkv"), 20);
        QVERIFY(loadPaused(player, clip));
        actions.trigger("seek-forward");
        QTRY_VERIFY_WITH_TIMEOUT(qAbs(player.position() - 10) < 1.5, 5000);
    }

    void newSettingsReachMpv()
    {
        QTemporaryDir dir;
        Player player(Player::Output::None);
        Actions actions(&player);
        Settings settings(&player, &actions, dir.filePath("settings.ini"));
        settings.applyAll();
        auto expect = [&](const char *property, const QString &value) {
            QTRY_COMPARE_WITH_TIMEOUT(player.get(property).toString(), value, 3000);
        };
        // Defaults.
        expect("keep-open", "true");
        expect("hr-seek", "default");
        expect("deinterlace", "false");
        expect("sub-pos", "100");
        expect("sub-font", "sans-serif");
        expect("ytdl-format", "bestvideo[height<=?1080][vcodec!^=av01]+bestaudio/bestvideo[height<=?1080]+bestaudio/best[height<=?1080]/best");

        settings.set("playback/atEnd", "clear");
        settings.set("playback/exactSeek", true);
        settings.set("video/deinterlace", true);
        settings.set("video/streamQuality", "720");
        settings.set("audio/surround", "receiver");
        settings.set("subtitles/lift", 0.1);
        settings.set("subtitles/background", true);
        settings.set("subtitles/restyle", true);
        settings.set("subtitles/font", "Noto Sans CJK KR");
        expect("keep-open", "false");
        expect("hr-seek", "true");
        expect("deinterlace", "auto");
        expect("ytdl-format", "bestvideo[height<=?720][vcodec!^=av01]+bestaudio/bestvideo[height<=?720]+bestaudio/best[height<=?720]/best");
        expect("audio-spdif", "ac3,eac3,dts,dts-hd,truehd");
        settings.set("audio/surround", "stereo");
        expect("audio-channels", "stereo");
        expect("audio-spdif", "");
        expect("sub-pos", "90");
        expect("sub-border-style", "background-box");
        expect("sub-ass-override", "force");
        expect("sub-font", "Noto Sans CJK KR");

        settings.set("audio/device", "pipewire/nowhere");
        expect("audio-device", "pipewire/nowhere");
        settings.set("audio/device", "auto");
        expect("audio-device", "auto");
    }

    void oldSettingsCarryOver()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath("settings.ini");
        {
            QSettings old(file, QSettings::IniFormat);
            old.setValue("playback/keepOpen", false);
        }
        Player player(Player::Output::None);
        Actions actions(&player);
        Settings settings(&player, &actions, file);
        QCOMPARE(settings.value("playback/atEnd").toString(), QString("clear"));
        QVERIFY(!settings.store().contains("playback/keepOpen"));
    }

    void resetAllRestoresEverything()
    {
        QTemporaryDir dir;
        Player player(Player::Output::None);
        Actions actions(&player);
        Settings settings(&player, &actions, dir.filePath("settings.ini"));
        settings.beginSession();
        settings.set("video/smooth", true);
        settings.set("subtitles/second", "en");
        settings.bindKey("shuffle", "Ctrl+;");
        QVERIFY(settings.changedKeys().contains("keys/shuffle"));
        settings.resetAll();
        QCOMPARE(settings.value("video/smooth").toBool(), false);
        QCOMPARE(settings.value("subtitles/second").toString(), QString());
        QCOMPARE(settings.customBindings(), 0);
        QVERIFY(settings.changedKeys().isEmpty());

        // Undo all brings shortcuts back too.
        settings.bindKey("shuffle", "Ctrl+;");
        settings.beginSession();
        settings.resetAll();
        QCOMPARE(settings.changedKeys(), QStringList{"keys/shuffle"});
        settings.undoAll();
        QCOMPARE(actions.keys("shuffle"), QStringList{"Ctrl+;"});
        QVERIFY(settings.changedKeys().isEmpty());
    }

    void screenshotsGoToOmarchysFolder()
    {
        qputenv("OMARCHY_SCREENSHOT_DIR", "/tmp/omarchy-shots");
        QCOMPARE(Settings::defaultScreenshotFolder(), QString("/tmp/omarchy-shots"));
        qunsetenv("OMARCHY_SCREENSHOT_DIR");
        QVERIFY(!Settings::defaultScreenshotFolder().isEmpty());
    }

    void repeatCyclesOffOneAll()
    {
        Player player(Player::Output::None);
        QSignalSpy osd(&player, &Player::osd);
        QCOMPARE(player.repeatMode(), QString("off"));
        player.cycleRepeat();
        QTRY_COMPARE(player.repeatMode(), QString("file"));
        player.cycleRepeat();
        QTRY_COMPARE(player.repeatMode(), QString("playlist"));
        QCOMPARE(player.get("loop-file").toString(), QString("false"));
        player.cycleRepeat();
        QTRY_COMPARE(player.repeatMode(), QString("off"));
        QTRY_VERIFY(osd.count() >= 3);
        QCOMPARE(osd.last().first().toString(), QString("repeat"));
    }

    void abRepeatMarksAndClears()
    {
        QTemporaryDir dir;
        const QString clip = makeClip(dir.filePath("clip.mkv"), 20);
        Player player(Player::Output::None);
        QVERIFY(loadPaused(player, clip));

        // B before A loops from the start.
        player.seekTo(4, true);
        QTRY_VERIFY(qAbs(player.position() - 4) < 0.2);
        player.setLoopPoint("b");
        QTRY_VERIFY(qAbs(player.get("ab-loop-b").toDouble() - 4) < 0.2);
        QCOMPARE(player.get("ab-loop-a").toDouble(), 0.0);

        player.seekTo(2, true);
        QTRY_VERIFY(qAbs(player.position() - 2) < 0.2);
        player.setLoopPoint("a");
        QTRY_VERIFY(qAbs(player.get("ab-loop-a").toDouble() - 2) < 0.2);
        QVERIFY(qAbs(player.get("ab-loop-b").toDouble() - 4) < 0.2);

        // A new start past the end drops the old end.
        player.seekTo(8, true);
        QTRY_VERIFY(qAbs(player.position() - 8) < 0.2);
        player.setLoopPoint("a");
        QTRY_VERIFY(qAbs(player.get("ab-loop-a").toDouble() - 8) < 0.2);
        QCOMPARE(player.get("ab-loop-b").toString(), QString("no"));

        player.setLoopPoint("clear");
        QTRY_COMPARE(player.get("ab-loop-a").toString(), QString("no"));
    }

    void screenshotsSaveAndCopy()
    {
        QTemporaryDir dir;
        const QString clip = makeClip(dir.filePath("Holiday.mkv"));
        Player player(Player::Output::None);
        player.setScreenshotOptions(dir.filePath("shots"), false, "png");
        QSignalSpy osd(&player, &Player::osd);
        QVERIFY(loadPaused(player, clip));
        QTRY_VERIFY(player.videoSize().width() > 0);
        osd.clear();

        player.screenshot();
        QTRY_VERIFY_WITH_TIMEOUT(osd.count() > 0, 5000);
        QCOMPARE(osd.last().first().toString(), QString("screenshot"));
        QVERIFY(QFileInfo(player.lastScreenshot()).fileName().startsWith("Holiday 00.00.00"));
        QCOMPARE(QImage(player.lastScreenshot()).size(), QSize(320, 240));

        // A second one at the same moment doesn't overwrite the first.
        const QString first = player.lastScreenshot();
        osd.clear();
        player.screenshot();
        QTRY_VERIFY_WITH_TIMEOUT(osd.count() > 0, 5000);
        QVERIFY(player.lastScreenshot() != first);
        QVERIFY(QFileInfo::exists(first));

        osd.clear();
        player.screenshot(true);
        QTRY_VERIFY_WITH_TIMEOUT(osd.count() > 0, 5000);
        QCOMPARE(osd.last().first().toString(), QString("screenshot-copy"));
        QCOMPARE(QGuiApplication::clipboard()->image().size(), QSize(320, 240));
        QVERIFY(!QFileInfo::exists(player.lastScreenshot()));
    }

    // Q, as in the Windows players: colour controls off, and back again.
    void pictureAdjustmentsToggle()
    {
        QTemporaryDir dir;
        const QString clip = makeClip(dir.filePath("clip.mkv"));
        Player player(Player::Output::None);
        QSignalSpy osd(&player, &Player::osd);
        QVERIFY(loadPaused(player, clip));
        player.command({"add", "brightness", "10"}, "brightness");
        player.command({"add", "hue", "-4"}, "hue");
        player.command({"cycle-values", "video-rotate", "90", "0"}, "rotate");
        QTRY_COMPARE(player.get("video-rotate").toInt(), 90);
        QTRY_COMPARE(player.get("hue").toInt(), -4);

        player.resetPicture();
        QTRY_VERIFY_WITH_TIMEOUT(!osd.isEmpty() && osd.last().first().toString() == "picture-reset", 5000);
        QCOMPARE(player.get("brightness").toInt(), 0);
        QCOMPARE(player.get("hue").toInt(), 0);
        QCOMPARE(player.get("video-rotate").toInt(), 90); // only the colour controls

        player.resetPicture();
        QTRY_VERIFY_WITH_TIMEOUT(osd.last().first().toString() == "picture-back", 5000);
        QCOMPARE(player.get("brightness").toInt(), 10);
        QCOMPARE(player.get("hue").toInt(), -4);
    }

    void secondSubtitleShowsUpTop()
    {
        QTemporaryDir dir;
        const QString clip = makeClip(dir.filePath("Film.mkv"));
        write(dir.filePath("Film.ko.srt"), "1\n00:00:00,000 --> 00:00:01,500\n안녕\n");
        write(dir.filePath("Film.en.srt"), "1\n00:00:00,000 --> 00:00:01,500\nHello\n");
        Player player(Player::Output::None);
        player.set("slang", "ko,kor");
        player.setSecondLanguage({"en", "eng"});
        player.command({"set", "pause", "yes"});
        player.openPaths({clip});
        auto selected = [&](const char *property) {
            const QVariant id = player.get(property);
            for (const auto &track : player.tracks())
                if (track.toMap().value("type") == "sub" && track.toMap().value("id") == id)
                    return track.toMap().value("lang").toString();
            return QString();
        };
        QTRY_COMPARE_WITH_TIMEOUT(selected("sid"), QString("ko"), 10000);
        QTRY_COMPARE_WITH_TIMEOUT(selected("secondary-sid"), QString("en"), 5000);

        player.setSecondLanguage({});
        QTRY_COMPARE(player.get("secondary-sid").toString(), QString("false"));
    }

    void chaptersSayWhenThereAreNone()
    {
        QTemporaryDir dir;
        const QString clip = makeClip(dir.filePath("clip.mkv"));
        Player player(Player::Output::None);
        QSignalSpy osd(&player, &Player::osd);
        QVERIFY(loadPaused(player, clip));
        osd.clear();
        player.command({"add", "chapter", "1"}, "chapter");
        QTRY_VERIFY_WITH_TIMEOUT(!osd.isEmpty(), 5000);
        QCOMPARE(osd.last().first().toString(), QString("chapter-none"));
    }

    void audioSyncMoves()
    {
        QTemporaryDir dir;
        const QString clip = makeClip(dir.filePath("clip.mkv"));
        Player player(Player::Output::None);
        Actions actions(&player);
        QVERIFY(loadPaused(player, clip));
        actions.trigger("audio-later");
        actions.trigger("audio-later");
        QTRY_VERIFY(qAbs(player.get("audio-delay").toDouble() - 0.1) < 0.001);
        actions.trigger("audio-sync-reset");
        QTRY_COMPARE(player.get("audio-delay").toDouble(), 0.0);
        actions.trigger("sub-up");
        QTRY_COMPARE(player.get("sub-pos").toInt(), 99);
        actions.trigger("sub-bigger");
        QTRY_VERIFY(qAbs(player.get("sub-scale").toDouble() - 1.1) < 0.001);
    }

    // Every action in the table runs and reports, in both layouts' terms:
    // mpv commands say what they changed, the player's own say theirs, and
    // the window's reach the window.
    void everyActionRuns()
    {
        QTemporaryDir dir;
        const QString clip = makeClip(dir.filePath("clip.mkv"), 20);
        makeClip(dir.filePath("clip2.mkv"));
        Player player(Player::Output::None);
        player.setScreenshotOptions(dir.filePath("shots"), true, "png");
        Actions actions(&player);
        QVERIFY(loadPaused(player, clip));
        QTRY_VERIFY(player.playlistCount() == 2);
        player.seekTo(10, true);
        QTRY_VERIFY(qAbs(player.position() - 10) < 0.5);

        QSignalSpy osd(&player, &Player::osd);
        QSignalSpy window(&actions, &Actions::windowAction);
        const QStringList skipped{"quit", "open-file", "open-clipboard", "stop", "previous-file", "next-file", "play-pause"};
        for (const auto &spec : Actions::specs()) {
            if (skipped.contains(spec.id))
                continue;
            osd.clear();
            window.clear();
            actions.trigger(spec.id);
            if (spec.kind == Actions::Kind::Window) {
                QCOMPARE(window.count(), 1);
                QCOMPARE(window.first().first().toString(), spec.id);
                continue;
            }
            QVERIFY2(QTest::qWaitFor([&] { return !osd.isEmpty(); }, 5000), qPrintable(spec.id + " said nothing"));
            const QString kind = osd.last().first().toString().section(':', 0, 0);
            QStringList expected{spec.osd};
            if (spec.id == "repeat") expected = {"repeat"};
            if (spec.id.startsWith("loop-")) expected = {"ab-loop"};
            if (spec.id == "screenshot") expected = {"screenshot"};
            if (spec.id == "copy-frame") expected = {"screenshot-copy"};
            if (spec.id == "picture-reset") expected = {"picture-reset"};
            if (spec.osd == "chapter" || spec.osd == "subtitle-line") expected.append(spec.osd + "-none");
            QVERIFY2(expected.contains(kind), qPrintable(QString("%1 said %2").arg(spec.id, kind)));
        }
        // Then the ones that leave this file or stop it.
        for (const auto &id : {"next-file", "previous-file", "play-pause", "stop"}) {
            osd.clear();
            actions.trigger(id);
            if (QString(id) != "stop")
                QVERIFY2(QTest::qWaitFor([&] { return !osd.isEmpty(); }, 5000), id);
        }
        QTRY_VERIFY(player.idle());
    }

    void anEmptyClipboardLeavesPlaybackAlone()
    {
        QTemporaryDir dir;
        const QString clip = makeClip(dir.filePath("clip.mkv"));
        Player player(Player::Output::None);
        QVERIFY(loadPaused(player, clip));
        for (const auto &contents : {QString(), QString("just some words"), QString("/no/such/file.mkv")}) {
            QGuiApplication::clipboard()->setText(contents);
            QVERIFY(!player.openClipboard());
        }
        QGuiApplication::clipboard()->setImage(QImage(8, 8, QImage::Format_RGB32));
        QVERIFY(!player.openClipboard());
        player.open({});
        QTest::qWait(500);
        QVERIFY(!player.idle());
        QCOMPARE(player.path(), clip);

        // A folder or a page that isn't a stream: nothing happens, or the
        // video comes back where it was.
        QGuiApplication::clipboard()->setText(dir.path());
        QVERIFY(!player.openClipboard());
        player.seekTo(1, true);
        QTRY_VERIFY(qAbs(player.position() - 1) < 0.2);
        QGuiApplication::clipboard()->setText("http://127.0.0.1:9/nothing-here.mkv");
        QVERIFY(player.openClipboard());
        QTRY_VERIFY_WITH_TIMEOUT(!player.error().isEmpty(), 20000);
        QTRY_COMPARE_WITH_TIMEOUT(player.path(), clip, 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!player.idle() && qAbs(player.position() - 1) < 0.5, 10000);

        // A real path does open.
        const QString other = makeClip(dir.filePath("other.mkv"));
        QGuiApplication::clipboard()->setText(other);
        QVERIFY(player.openClipboard());
        QTRY_COMPARE_WITH_TIMEOUT(player.path(), other, 10000);
    }

    // Media keys and the bar reach the player over MPRIS.
    void mprisControlsPlayback()
    {
        if (!QDBusConnection::sessionBus().isConnected())
            QSKIP("no session bus");
        QTemporaryDir dir;
        const QString clip = makeClip(dir.filePath("Song.mkv"), 10);
        Player player(Player::Output::None);
        const QString service = QString("org.mpris.MediaPlayer2.omaplayertest%1").arg(QCoreApplication::applicationPid());
        Mpris mpris(&player, nullptr, service);
        QVERIFY(mpris.registered());
        QVERIFY(loadPaused(player, clip));

        // The calls come from another thread on a connection of its own, as a
        // real client's would: on this thread they'd wait on themselves.
        auto bus = [&](const std::function<QVariant(QDBusConnection &)> &work) {
            QVariant result;
            static int serial = 0;
            const QString name = QString("omaplayer-tests-%1").arg(++serial);
            std::unique_ptr<QThread> thread(QThread::create([&] {
                QDBusConnection connection = QDBusConnection::connectToBus(QDBusConnection::SessionBus, name);
                result = work(connection);
            }));
            thread->start();
            QTest::qWaitFor([&] { return thread->isFinished(); }, 10000);
            thread->wait();
            QDBusConnection::disconnectFromBus(name);
            return result;
        };
        auto get = [&](const QString &interface, const QString &property) {
            return bus([&](QDBusConnection &connection) {
                QDBusInterface properties(service, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", connection);
                const QDBusReply<QDBusVariant> reply = properties.call("Get", interface, property);
                return reply.isValid() ? reply.value().variant() : QVariant();
            });
        };
        auto call = [&](const QString &method, const QVariantList &args = {}) {
            bus([&](QDBusConnection &connection) {
                QDBusInterface control(service, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", connection);
                control.callWithArgumentList(QDBus::Block, method, args);
                return QVariant();
            });
        };
        auto set = [&](const QString &property, const QVariant &value) {
            bus([&](QDBusConnection &connection) {
                QDBusInterface properties(service, "/org/mpris/MediaPlayer2", "org.freedesktop.DBus.Properties", connection);
                properties.call("Set", "org.mpris.MediaPlayer2.Player", property, QVariant::fromValue(QDBusVariant(value)));
                return QVariant();
            });
        };
        const QString root = "org.mpris.MediaPlayer2", control = "org.mpris.MediaPlayer2.Player";
        QCOMPARE(get(root, "Identity").toString(), QString("OmaPlayer"));
        QCOMPARE(get(root, "DesktopEntry").toString(), QString("omaplayer"));
        call("Pause");
        QTRY_VERIFY(player.paused());
        QCOMPARE(get(control, "PlaybackStatus").toString(), QString("Paused"));
        call("PlayPause");
        QTRY_VERIFY(!player.paused());
        QCOMPARE(get(control, "PlaybackStatus").toString(), QString("Playing"));
        call("Pause");
        QTRY_VERIFY(player.paused());

        call("Seek", {qlonglong(3000000)});
        QTRY_VERIFY(player.position() > 2.5);

        set("Volume", 0.4);
        QTRY_COMPARE(player.volume(), 40.0);
        set("LoopStatus", QString("Track"));
        QTRY_COMPARE(player.repeatMode(), QString("file"));

        const QVariantMap metadata = qdbus_cast<QVariantMap>(get(control, "Metadata"));
        QVERIFY(metadata.value("xesam:url").toString().endsWith("Song.mkv"));
        QVERIFY(metadata.value("mpris:length").toLongLong() > 9000000);

        call("Stop");
        QTRY_VERIFY(player.idle());
        QCOMPARE(get(control, "PlaybackStatus").toString(), QString("Stopped"));
    }

    // Picture in picture: a third of the monitor, the video's shape, in the
    // bottom-right corner clear of the bar.
    void pictureInPictureFitsTheCorner()
    {
        const QJsonObject monitor{{"x", 2560}, {"y", 0}, {"width", 3840}, {"height", 2160}, {"scale", 1.5},
                                  {"reserved", QJsonArray{0, 24, 0, 0}}};
        const QRect wide = Desktop::cornerFor(monitor, 16.0 / 9);
        QCOMPARE(wide.size(), QSize(853, 480));
        QCOMPARE(wide.right() + 1, 2560 + 2560 - 20);
        QCOMPARE(wide.bottom() + 1, 1440 - 20);

        // Tall video is held to 40% of the height.
        const QRect tall = Desktop::cornerFor(monitor, 9.0 / 16);
        QCOMPARE(tall.height(), 576);
        QCOMPARE(tall.width(), 324);

        // A bar along the bottom pushes it up; nonsense aspects fall back.
        const QJsonObject bottomBar{{"x", 0}, {"y", 0}, {"width", 1920}, {"height", 1080}, {"scale", 1.0},
                                    {"reserved", QJsonArray{0, 0, 0, 40}}};
        QCOMPARE(Desktop::cornerFor(bottomBar, 0).bottom() + 1, 1080 - 40 - 20);
        QCOMPARE(Desktop::cornerFor(bottomBar, 0).size(), QSize(640, 360));

        // Outside Hyprland it does nothing.
        qunsetenv("HYPRLAND_INSTANCE_SIGNATURE");
        Desktop desktop;
        QVERIFY(!desktop.available());
        desktop.setPictureInPicture(true);
        QVERIFY(!desktop.pictureInPicture());
    }

    void theLastFileEndingIsReported()
    {
        QTemporaryDir dir;
        const QString clip = makeClip(dir.filePath("clip.mkv"));
        Player player(Player::Output::None);
        player.set("keep-open", false);
        QSignalSpy finished(&player, &Player::finished);
        player.openPaths({clip});
        QTRY_VERIFY_WITH_TIMEOUT(finished.count() == 1, 15000);
        QVERIFY(player.idle());

        // Stopping isn't finishing.
        player.openPaths({clip});
        QTRY_VERIFY_WITH_TIMEOUT(!player.idle(), 10000);
        player.stop();
        QTRY_VERIFY(player.idle());
        QTest::qWait(300);
        QCOMPARE(finished.count(), 1);
    }

private:
    // Opens a clip paused and waits until it has fully loaded.
    static bool loadPaused(Player &player, const QString &clip)
    {
        QSignalSpy loaded(&player, &Player::fileLoaded);
        player.command({"set", "pause", "yes"});
        player.openPaths({clip});
        return QTest::qWaitFor([&] { return loaded.count() > 0; }, 10000);
    }

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
    static QString makeClip(const QString &path, int seconds = 2)
    {
        if (QStandardPaths::findExecutable("ffmpeg").isEmpty())
            QTest::qSkip("ffmpeg isn't installed", __FILE__, __LINE__);
        QProcess ffmpeg;
        ffmpeg.start("ffmpeg", {"-hide_banner", "-loglevel", "error", "-y",
                                "-f", "lavfi", "-i", "testsrc2=size=320x240:rate=25",
                                "-f", "lavfi", "-i", "sine=frequency=440",
                                "-t", QString::number(seconds), "-c:v", "libx264", "-preset", "ultrafast", "-c:a", "aac", path});
        ffmpeg.waitForFinished(30000);
        return path;
    }
};

QTEST_MAIN(OmaplayerTests)
#include "omaplayer_tests.moc"
