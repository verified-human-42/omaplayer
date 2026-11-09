#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QObject>
#include <QStringList>
#include <QVariantMap>

class Player;

// OmaPlayer on the session bus as an MPRIS media player
// (org.mpris.MediaPlayer2.omaplayer), so media keys, headphone buttons,
// playerctl, and the Omarchy bar's media widget see and control it.
class Mpris : public QObject {
    Q_OBJECT

public:
    // service: the bus name to claim; a second window takes an instance
    // name (…omaplayer.instance<pid>), as the spec asks.
    explicit Mpris(Player *player, QObject *parent = nullptr, const QString &service = {});
    ~Mpris() override;

    bool registered() const { return !m_service.isEmpty(); }
    QString service() const { return m_service; }
    Player *player() const { return m_player; }

    // Sends PropertiesChanged for these properties of an interface.
    void announce(const QString &interface, const QStringList &properties);

signals:
    void raiseRequested();
    void quitRequested();

private:
    Player *m_player;
    QString m_service;
    QObject *m_root = nullptr;
    QObject *m_control = nullptr;
};

class MprisRoot : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit)
    Q_PROPERTY(bool CanRaise READ canRaise)
    Q_PROPERTY(bool HasTrackList READ hasTrackList)
    Q_PROPERTY(QString Identity READ identity)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes)

public:
    explicit MprisRoot(Mpris *mpris);

    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return QStringLiteral("OmaPlayer"); }
    QString desktopEntry() const { return QStringLiteral("omaplayer"); }
    QStringList supportedUriSchemes() const;
    QStringList supportedMimeTypes() const;

public slots:
    void Raise();
    void Quit();

private:
    Mpris *m_mpris;
};

class MprisPlayer : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QString LoopStatus READ loopStatus WRITE setLoopStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(bool Shuffle READ shuffle WRITE setShuffle)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qlonglong Position READ position)
    Q_PROPERTY(double MinimumRate READ minimumRate)
    Q_PROPERTY(double MaximumRate READ maximumRate)
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPause)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl)

public:
    explicit MprisPlayer(Mpris *mpris);

    QString playbackStatus() const;
    QString loopStatus() const;
    void setLoopStatus(const QString &status);
    double rate() const;
    void setRate(double rate);
    bool shuffle() const { return false; }
    void setShuffle(bool on);
    QVariantMap metadata() const;
    double volume() const;
    void setVolume(double volume);
    qlonglong position() const;
    double minimumRate() const { return 0.25; }
    double maximumRate() const { return 4.0; }
    bool canGoNext() const;
    bool canGoPrevious() const;
    bool canPlay() const;
    bool canPause() const;
    bool canSeek() const;
    bool canControl() const { return true; }

public slots:
    void Next();
    void Previous();
    void Pause();
    void PlayPause();
    void Stop();
    void Play();
    void Seek(qlonglong offset);
    void SetPosition(const QDBusObjectPath &track, qlonglong position);
    void OpenUri(const QString &uri);

signals:
    void Seeked(qlonglong position);

private:
    Mpris *m_mpris;
    Player *player() const;
};
