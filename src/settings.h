#pragma once

#include <QHash>
#include <QObject>
#include <QSettings>
#include <QStringList>
#include <QVariantMap>
#include <memory>

class Actions;
class Player;

// The preferences behind the settings page. Each one is stored as soon as it
// changes and applied to mpv (or the keymap) at once. What changed since the
// page was opened is tracked, so it can be undone one setting at a time or all
// together, the way OmaControl does it.
class Settings : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap values READ values NOTIFY changed)
    Q_PROPERTY(QStringList changedKeys READ changedKeys NOTIFY changed)
    Q_PROPERTY(int customBindings READ customBindings NOTIFY changed)
    // Whether videos open in OmaPlayer from the file manager and browser.
    Q_PROPERTY(bool defaultPlayer READ defaultPlayer NOTIFY defaultPlayerChanged)
    Q_PROPERTY(QString defaultPlayerError READ defaultPlayerError NOTIFY defaultPlayerChanged)

public:
    struct Spec {
        QString key;
        QVariant fallback;
        QStringList choices = {};
        double minimum = 0;
        double maximum = 0;
    };

    // file is for tests; the app stores in ~/.config/omaplayer/omaplayer.conf.
    Settings(Player *player, Actions *actions, const QString &file = {}, QObject *parent = nullptr);

    static const QList<Spec> &specs();

    QVariantMap values() const { return m_values; }
    QStringList changedKeys() const;
    QSettings &store() { return *m_store; }
    Q_INVOKABLE QString fileName() const { return m_store->fileName(); }

    Q_INVOKABLE QVariant value(const QString &key) const { return m_values.value(key); }
    Q_INVOKABLE void set(const QString &key, const QVariant &value);
    Q_INVOKABLE void undo(const QString &key);
    Q_INVOKABLE void undoAll();
    // Starts a new round of changes: what's set now is what Undo goes back to.
    Q_INVOKABLE void beginSession();

    void applyAll();

    // Omarchy's language, which subtitles follow until languages are set.
    void setSystemLanguage(const QString &language);
    static QString languageCodes(const QString &language);
    static QString legacyCodepage(const QString &language);

    // Key bindings. A key already bound elsewhere moves to this action; the
    // id of the action it came from is returned so the page can say so.
    Q_INVOKABLE QString bindKey(const QString &id, const QString &key);
    Q_INVOKABLE void unbindKey(const QString &id, const QString &key);
    Q_INVOKABLE void resetKeys(const QString &id);
    Q_INVOKABLE void resetAllKeys();
    // Every setting and key back to how it came, undoable until the page closes.
    Q_INVOKABLE void resetAll();

    Q_INVOKABLE static QString defaultScreenshotFolder();
    static QStringList mediaTypes();
    bool defaultPlayer() const { return m_defaultPlayer; }
    QString defaultPlayerError() const { return m_defaultPlayerError; }
    Q_INVOKABLE void checkDefaultPlayer();
    Q_INVOKABLE void makeDefaultPlayer();
    int customBindings() const { return m_bindings.size(); }

signals:
    void changed();
    void defaultPlayerChanged();

private:
    static const Spec *find(const QString &key);
    static QVariant normalize(const Spec &spec, const QVariant &value);
    void apply(const QString &key);
    void storeBindings();

    std::unique_ptr<QSettings> m_store;
    Player *m_player;
    Actions *m_actions;
    QVariantMap m_values;
    QVariantMap m_session;
    QHash<QString, QStringList> m_bindings;
    QString m_language = "en";
    bool m_defaultPlayer = false;
    QString m_defaultPlayerError;
    QHash<QString, QStringList> m_sessionBindings;
};
