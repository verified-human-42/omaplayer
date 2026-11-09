// omaplayer — an Omarchy media player that runs on mpv.

#include <QDateTime>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>
#include <clocale>

#include "actions.h"
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

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    Theme theme;
    Player player;
    qCInfo(lcTiming, "mpv ready at %lld", QDateTime::currentMSecsSinceEpoch());
    Actions actions(&player);
    PortalFilePicker picker;

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
    player.set("volume", settings.store().value("state/volume", 100).toDouble());
    player.set("mute", settings.store().value("state/mute", false).toBool());
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
    engine.load(QUrl("qrc:/Main.qml"));

    qCInfo(lcTiming, "qml loaded at %lld", QDateTime::currentMSecsSinceEpoch());
    auto *window = engine.rootObjects().isEmpty() ? nullptr : qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
        return 1;
    new VideoRenderer(window, &player);
    QObject::connect(window, &QQuickWindow::frameSwapped, window, [] {
        qCInfo(lcTiming, "window shown at %lld", QDateTime::currentMSecsSinceEpoch());
    }, Qt::ConnectionType(Qt::DirectConnection | Qt::SingleShotConnection));

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

    // Files and URLs from the command line, or from the launcher's %U.
    QStringList items = app.arguments().mid(1);
    items.removeIf([](const QString &item) { return item.startsWith('-') && item != "-"; });
    player.openPaths(items);

    return app.exec();
}
