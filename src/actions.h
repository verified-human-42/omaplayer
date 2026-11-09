#pragma once

#include <QHash>
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
        // Listed on the start screen's cheat sheet; the rest are in the menu
        // and the full shortcut sheet.
        bool everyday = true;
    };

    explicit Actions(Player *player, QObject *parent = nullptr);

    static const QList<Spec> &specs();
    static QStringList keymaps() { return {"omaplayer", "mpv"}; }
    static QStringList keysFor(const Spec &spec, const QString &keymap);

    QVariantList list() const;
    QVariantList groups() const;
    QString keymap() const { return m_keymap; }
    void setKeymap(const QString &keymap);
    // Seconds the arrow keys seek, alone and with Ctrl.
    void setSeekSteps(int step, int longStep);

    // Keys the user chose for an action, replacing the layout's.
    void setOverrides(const QHash<QString, QStringList> &overrides);
    QStringList keys(const QString &id) const;
    QString idForKey(const QString &key) const;
    static QString keyLabel(const QString &key);
    // A key press as a portable sequence ("Ctrl+L"), or empty for a lone modifier.
    Q_INVOKABLE static QString sequence(int key, int modifiers, const QString &text);

    Q_INVOKABLE void trigger(const QString &id);
    Q_INVOKABLE QString shortcut(const QString &id) const;
    Q_INVOKABLE QString text(const QString &id) const;

signals:
    void changed();
    void windowAction(const QString &id);

private:
    Player *m_player;
    QString m_keymap = "omaplayer";
    QHash<QString, QStringList> m_overrides;
    int m_seekStep = 5;
    int m_seekLongStep = 30;

    QString textFor(const Spec &spec) const;
    QStringList commandFor(const Spec &spec) const;
};
