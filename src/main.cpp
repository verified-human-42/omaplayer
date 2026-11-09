// omaplayer — an Omarchy media player that runs on mpv.

#include <QDateTime>
#include <QGuiApplication>
#include <QDir>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <clocale>
#include <unistd.h>

#include "actions.h"
#include "desktop.h"
#include "mpris.h"
#include "player.h"
#include "portalfilepicker.h"
#include "settings.h"
#include "theme.h"
#include "timing.h"
#include "translator.h"
#include "videorenderer.h"

Q_LOGGING_CATEGORY(lcTiming, "omaplayer.timing", QtWarningMsg)

int main(int argc, char *argv[]) {
    qCInfo(lcTiming, "main at %lld", QDateTime::currentMSecsSinceEpoch());

    // libmpv draws with OpenGL, straight into the window, so Qt Quick has to
    // render the controls with OpenGL as well.
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

    QGuiApplication app(argc, argv);
    qCInfo(lcTiming, "app created at %lld", QDateTime::currentMSecsSinceEpoch());
    app.setApplicationName("omaplayer");
    app.setOrganizationName("omaplayer");
    app.setApplicationVersion(OMAPLAYER_VERSION);

    // Matches the window to omaplayer.desktop, so the compositor (Wayland
    // app_id) and the launcher pick up the installed icon.
    app.setDesktopFileName("omaplayer");
    app.setWindowIcon(QIcon::fromTheme("omaplayer"));

    // libmpv refuses to start unless numbers are read the C way, and Qt has
    // just set the locale from the environment.
    std::setlocale(LC_NUMERIC, "C");

    QQuickStyle::setStyle("Material");

    // Files and URLs from the command line, or from the launcher's %U.
    QStringList items = app.arguments().mid(1);
    items.removeIf([](const QString &item) { return item.startsWith('-') && item != "-"; });

    // Files opened while a window is up replace what it plays, the way
    // Windows players behave, unless Settings › Playback asks for a new
    // window each time. Asking takes
    // a moment only when one is running; otherwise this one takes the calls.
    const QString server = QString("omaplayer-%1").arg(getuid());
    QLocalSocket running;
    running.connectToServer(server);
    const bool another = running.waitForConnected(250);
    if (another && !items.isEmpty() && QSettings().value("playback/openIn", "replace").toString() != "window") {
        QStringList absolute;
        for (const auto &item : items)
            absolute.append(item.contains("://") || item == "-" ? item : QDir::current().absoluteFilePath(item));
        running.write(absolute.join('\n').toUtf8());
        running.waitForBytesWritten(1000);
        running.disconnectFromServer();
        return 0;
    }
    running.abort();

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    // For development: OMAPLAYER_THEME=DIR shows a theme without switching the
    // desktop's. DIR is laid out like ~/.local/state/omarchy/current: the
    // theme's folder (or a link to it) is DIR/theme.
    Theme theme(qEnvironmentVariable("OMAPLAYER_THEME"));
    Player player;
    qCInfo(lcTiming, "mpv ready at %lld", QDateTime::currentMSecsSinceEpoch());
    Actions actions(&player);
    PortalFilePicker picker;
    Desktop desktop;

    // Preferences go to mpv before the first file opens. Volume and mute
    // carry over between runs; positions are per file and kept by mpv.
    Settings settings(&player, &actions);
    // The language picked in settings, if any, wins over Omarchy's.
    auto followLanguage = [&] {
        const QString chosen = settings.value("interface/language").toString();
        translator.setOverride(chosen == "omarchy" ? QString() : chosen);
    };
    followLanguage();
    QObject::connect(&settings, &Settings::changed, &translator, followLanguage);
    settings.setSystemLanguage(translator.language());
    settings.applyAll();
    QObject::connect(&translator, &Translator::changed, &settings,
                     [&] { settings.setSystemLanguage(translator.language()); });
    if (settings.value("audio/rememberVolume").toBool()) {
        player.set("volume", settings.store().value("state/volume", 100).toDouble());
        player.set("mute", settings.store().value("state/mute", false).toBool());
    }
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&] {
        settings.store().setValue("state/volume", player.volume());
        settings.store().setValue("state/mute", player.muted());
    });

    QObject::connect(&picker, &FilePicker::openSelected, &player, [&player](const QList<QUrl> &urls, bool append) {
        QVariantList items;
        for (const auto &url : urls)
            items.append(url);
        if (append)
            player.append(items);
        else
            player.open(items);
    });
    QObject::connect(&picker, &FilePicker::failed, &player, &Player::reportError);

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    QObject::connect(&translator, &Translator::changed, &actions, &Actions::changed);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("player", &player);
    engine.rootContext()->setContextProperty("actions", &actions);
    engine.rootContext()->setContextProperty("picker", &picker);
    engine.rootContext()->setContextProperty("settings", &settings);
    engine.rootContext()->setContextProperty("desktop", &desktop);
    engine.load(QUrl("qrc:/Main.qml"));

    qCInfo(lcTiming, "qml loaded at %lld", QDateTime::currentMSecsSinceEpoch());
    auto *window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
        return 1;
    new VideoRenderer(window, &player);

    // Media keys, headphone buttons, playerctl, and the bar's media widget.
    Mpris mpris(&player);
    QObject::connect(&mpris, &Mpris::raiseRequested, window, [window] {
        window->show();
        window->raise();
        window->requestActivate();
    });
    QObject::connect(&mpris, &Mpris::quitRequested, window, &QWindow::close);
    QObject::connect(window, &QQuickWindow::frameSwapped, window, [] {
        qCInfo(lcTiming, "window shown at %lld", QDateTime::currentMSecsSinceEpoch());
    }, Qt::ConnectionType(Qt::DirectConnection | Qt::SingleShotConnection));

    // For development: OMAPLAYER_DO=id,id[:ms] runs actions from the table
    // after ms (default 2000), as if their keys were pressed.
    if (const QString run = qEnvironmentVariable("OMAPLAYER_DO"); !run.isEmpty()) {
        const int after = run.section(':', 1, 1).isEmpty() ? 2000 : run.section(':', 1, 1).toInt();
        QTimer::singleShot(after, &actions, [&actions, run] {
            for (const auto &id : run.section(':', 0, 0).split(',', Qt::SkipEmptyParts))
                actions.trigger(id);
        });
    }

    // For development: OMAPLAYER_GRAB=shot.png[:ms] saves what the window
    // shows, video included, after ms (default 3000), then quits.
    if (const QString grab = qEnvironmentVariable("OMAPLAYER_GRAB"); !grab.isEmpty()) {
        const QString path = grab.section(':', 0, 0);
        const int after = grab.section(':', 1, 1).isEmpty() ? 3000 : grab.section(':', 1, 1).toInt();
        QTimer::singleShot(after, window, [window, path] {
            window->grabWindow().save(path);
            QCoreApplication::quit();
        });
    }

    player.openPaths(items);

    // While a video plays, the window wears Omarchy's "noidle" tag, so the
    // screen stays on. (libmpv leaves keeping the screen awake to whoever
    // owns the window.)
    auto keepAwake = [&] {
        desktop.setAwake(settings.value("video/keepAwake").toBool() && !player.idle() && !player.paused()
                         && player.videoSize().width() > 0);
    };
    QObject::connect(&player, &Player::pausedChanged, &app, keepAwake);
    QObject::connect(&player, &Player::idleChanged, &app, keepAwake);
    QObject::connect(&player, &Player::videoSizeChanged, &app, keepAwake);
    QObject::connect(&settings, &Settings::changed, &app, keepAwake);

    // The first window listens for the files later launches hand over.
    QLocalServer handover;
    if (!another) {
        QLocalServer::removeServer(server);
        handover.listen(server);
        QObject::connect(&handover, &QLocalServer::newConnection, &handover, [&handover, &player, window] {
            while (QLocalSocket *socket = handover.nextPendingConnection()) {
                QObject::connect(socket, &QLocalSocket::disconnected, socket, [socket, &player, window] {
                    QVariantList opened;
                    for (const auto &line : QString::fromUtf8(socket->readAll()).split('\n', Qt::SkipEmptyParts))
                        opened.append(line);
                    if (!opened.isEmpty()) {
                        player.open(opened);
                        window->requestActivate();
                    }
                    socket->deleteLater();
                });
            }
        });
    }

    return app.exec();
}
