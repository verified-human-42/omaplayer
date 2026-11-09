#include "desktop.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QProcess>
#include <QRect>
#include <cmath>

Desktop::Desktop(QObject *parent, const QString &hyprctl)
    : QObject(parent), m_hyprctl(hyprctl), m_available(!qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE")) {}

QByteArray Desktop::run(const QStringList &arguments) const {
    QProcess process;
    process.start(m_hyprctl, arguments);
    if (!process.waitForFinished(2000))
        return {};
    return process.readAllStandardOutput();
}

// Hyprland set up in Lua takes the first form; older setups the classic one.
void Desktop::dispatch(const QString &lua, const QStringList &classic) const {
    const QByteArray reply = run({"dispatch", lua});
    if (reply.isEmpty() || reply.contains("error"))
        run(QStringList{"dispatch"} + classic);
}

QJsonObject Desktop::client() const {
    const qint64 pid = QCoreApplication::applicationPid();
    for (const auto &value : QJsonDocument::fromJson(run({"clients", "-j"})).array())
        if (value.toObject().value("pid").toInteger() == pid)
            return value.toObject();
    return {};
}

QJsonObject Desktop::monitor(int id) const {
    for (const auto &value : QJsonDocument::fromJson(run({"monitors", "-j"})).array())
        if (value.toObject().value("id").toInt() == id)
            return value.toObject();
    return {};
}

// A third of the monitor's width, as tall as the picture needs (but no more
// than 40% of the height), in the bottom-right corner clear of the bar and
// any other reserved edge.
QRect Desktop::cornerFor(const QJsonObject &monitor, double aspect) {
    const double scale = monitor.value("scale").toDouble(1);
    const int x = monitor.value("x").toInt(), y = monitor.value("y").toInt();
    const int width = int(std::lround(monitor.value("width").toInt() / scale));
    const int height = int(std::lround(monitor.value("height").toInt() / scale));
    const QJsonArray reserved = monitor.value("reserved").toArray(); // left, top, right, bottom
    const int right = reserved.size() == 4 ? reserved[2].toInt() : 0;
    const int bottom = reserved.size() == 4 ? reserved[3].toInt() : 0;
    if (!(aspect > 0.1 && aspect < 10))
        aspect = 16.0 / 9.0;
    int w = int(std::lround(width / 3.0));
    int h = int(std::lround(w / aspect));
    if (h > height * 0.4) {
        h = int(std::lround(height * 0.4));
        w = int(std::lround(h * aspect));
    }
    const int margin = 20;
    return QRect(x + width - right - margin - w, y + height - bottom - margin - h, w, h);
}

void Desktop::setPictureInPicture(bool on, double aspect) {
    if (!m_available || on == m_pictureInPicture)
        return;
    const QJsonObject self = client();
    const QString address = self.value("address").toString();
    if (address.isEmpty())
        return;
    const QString window = "address:" + address;
    if (on) {
        const QRect corner = cornerFor(monitor(self.value("monitor").toInt()), aspect);
        if (!self.value("floating").toBool())
            dispatch(QString("hl.dsp.window.float({ window = \"%1\", action = \"on\" })").arg(window), {"setfloating", window});
        dispatch(QString("hl.dsp.window.resize({ window = \"%1\", x = %2, y = %3 })").arg(window).arg(corner.width()).arg(corner.height()),
                 {"resizewindowpixel", QString("exact %1 %2,%3").arg(corner.width()).arg(corner.height()).arg(window)});
        dispatch(QString("hl.dsp.window.move({ window = \"%1\", x = %2, y = %3 })").arg(window).arg(corner.x()).arg(corner.y()),
                 {"movewindowpixel", QString("exact %1 %2,%3").arg(corner.x()).arg(corner.y()).arg(window)});
        if (!self.value("pinned").toBool())
            dispatch(QString("hl.dsp.window.pin({ window = \"%1\" })").arg(window), {"pin", window});
        dispatch(QString("hl.dsp.window.alter_zorder({ window = \"%1\", mode = \"top\" })").arg(window), {"alterzorder", "top," + window});
        // Omarchy's own popped windows wear this tag (Super+O): rounded, and
        // re-centered when moved to another monitor.
        dispatch(QString("hl.dsp.window.tag({ window = \"%1\", tag = \"+pop\" })").arg(window), {"tagwindow", "+pop", window});
    } else {
        if (self.value("pinned").toBool())
            dispatch(QString("hl.dsp.window.pin({ window = \"%1\" })").arg(window), {"pin", window});
        dispatch(QString("hl.dsp.window.float({ window = \"%1\", action = \"off\" })").arg(window), {"settiled", window});
        dispatch(QString("hl.dsp.window.tag({ window = \"%1\", tag = \"-pop\" })").arg(window), {"tagwindow", "-pop", window});
    }
    m_pictureInPicture = on;
    emit pictureInPictureChanged();
}

void Desktop::setAwake(bool awake) {
    if (!m_available || awake == m_awake)
        return;
    m_awake = awake;
    const QString tag = QString(awake ? "+" : "-") + "noidle";
    const QString self = QString("pid:%1").arg(QCoreApplication::applicationPid());
    dispatch(QString("hl.dsp.window.tag({ tag = \"%1\", window = \"%2\" })").arg(tag, self), {"tagwindow", tag, self});
}
