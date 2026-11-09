#include <QDir>
#include <QFile>
#include <QKeySequence>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>
#include <clocale>

#include "actions.h"
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
