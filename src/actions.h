#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>

class Player;

// Every action in the app, in one table. The keymap, the right-click menu,
// the shortcut overlay, and tooltips all read it, so adding a row adds the
// feature everywhere at once.
class Actions : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList list READ list NOTIFY changed)
    Q_PROPERTY(QVariantList groups READ groups NOTIFY changed)
    Q_PROPERTY(QString keymap READ keymap WRITE setKeymap NOTIFY changed)

public:
    // How an action is carried out: an mpv command, a Player method, or by
    // the window itself (fullscreen, dialogs, overlays).
    enum class Kind { Mpv, Player, Window };

    struct Spec {
        QString id;
        QString group;
        const char *text;
        QStringList keys;
        QStringList mpvKeys;
        Kind kind;
        QStringList command;
        QString osd;
    };

    explicit Actions(Player *player, QObject *parent = nullptr);

    static const QList<Spec> &specs();
    static QStringList keymaps() { return {"omaplayer", "mpv"}; }
    static QStringList keysFor(const Spec &spec, const QString &keymap);

    QVariantList list() const;
    QVariantList groups() const;
    QString keymap() const { return m_keymap; }
    void setKeymap(const QString &keymap);

    Q_INVOKABLE void trigger(const QString &id);
    Q_INVOKABLE QString shortcut(const QString &id) const;
    Q_INVOKABLE QString text(const QString &id) const;

signals:
    void changed();
    void windowAction(const QString &id);

private:
    Player *m_player;
    QString m_keymap = "omaplayer";
};
