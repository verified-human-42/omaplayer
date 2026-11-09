#pragma once

#include <QJsonObject>
#include <QObject>
#include <QRect>
#include <QStringList>

// OmaPlayer's own window on the Hyprland desktop: picture in picture, and
// keeping the screen awake while a video plays. Both go through hyprctl, in
// Hyprland's Lua dispatchers with the classic ones as a fallback, and both
// do nothing outside Hyprland.
class Desktop : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool available READ available CONSTANT)
    Q_PROPERTY(bool pictureInPicture READ pictureInPicture NOTIFY pictureInPictureChanged)

public:
    // hyprctl: the command to run, for tests.
    explicit Desktop(QObject *parent = nullptr, const QString &hyprctl = QStringLiteral("hyprctl"));

    bool available() const { return m_available; }
    bool pictureInPicture() const { return m_pictureInPicture; }

    // Pops the window out to a pinned, floating corner of its monitor, sized
    // for a picture of this aspect (width / height), or puts it back.
    Q_INVOKABLE void setPictureInPicture(bool on, double aspect = 16.0 / 9.0);
    // Tags the window "noidle", which Omarchy's rules turn into an idle
    // inhibitor, or takes the tag off.
    void setAwake(bool awake);

    // Where a picture-in-picture window goes on a monitor: its logical size
    // and reserved edges (bar), in the order hyprctl reports them.
    static QRect cornerFor(const QJsonObject &monitor, double aspect);

signals:
    void pictureInPictureChanged();

private:
    QString m_hyprctl;
    bool m_available = false;
    bool m_pictureInPicture = false;
    bool m_awake = false;

    QJsonObject client() const;
    QJsonObject monitor(int id) const;
    QByteArray run(const QStringList &arguments) const;
    void dispatch(const QString &lua, const QStringList &classic) const;
};
