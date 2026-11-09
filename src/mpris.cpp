#include "mpris.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QUrl>
#include <cmath>

#include "player.h"
#include "settings.h"

namespace {
const QString kPath = QStringLiteral("/org/mpris/MediaPlayer2");
const QString kRoot = QStringLiteral("org.mpris.MediaPlayer2");
const QString kPlayer = QStringLiteral("org.mpris.MediaPlayer2.Player");

qlonglong micro(double seconds) {
    return std::isfinite(seconds) ? qlonglong(seconds * 1e6) : 0;
}
}

Mpris::Mpris(Player *player, QObject *parent, const QString &service) : QObject(parent), m_player(player) {
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;
    auto *control = new MprisPlayer(this);
    m_root = new MprisRoot(this);
    m_control = control;
    if (!bus.registerObject(kPath, this))
        return;
    const QString base = service.isEmpty() ? QStringLiteral("org.mpris.MediaPlayer2.omaplayer") : service;
    for (const QString &name : {base, base + ".instance" + QString::number(QCoreApplication::applicationPid())})
        if (bus.registerService(name)) {
            m_service = name;
            break;
        }
    if (m_service.isEmpty()) {
        bus.unregisterObject(kPath);
        return;
    }

    // What changes, announced as it changes, so the bar never has to poll.
    // Position is the exception the spec makes: clients read it, and a seek
    // says so with Seeked.
    auto onPlayer = [this](auto signal, const QString &interface, const QStringList &properties) {
        connect(m_player, signal, this, [this, interface, properties] { announce(interface, properties); });
    };
    onPlayer(&Player::pausedChanged, kPlayer, {"PlaybackStatus", "CanPlay", "CanPause"});
    onPlayer(&Player::idleChanged, kPlayer, {"PlaybackStatus", "CanPlay", "CanPause", "CanSeek", "Metadata"});
    onPlayer(&Player::titleChanged, kPlayer, {"Metadata"});
    onPlayer(&Player::pathChanged, kPlayer, {"Metadata"});
    onPlayer(&Player::durationChanged, kPlayer, {"Metadata"});
    onPlayer(&Player::playlistChanged, kPlayer, {"Metadata", "CanGoNext", "CanGoPrevious"});
    onPlayer(&Player::volumeChanged, kPlayer, {"Volume"});
    onPlayer(&Player::mutedChanged, kPlayer, {"Volume"});
    onPlayer(&Player::speedChanged, kPlayer, {"Rate"});
    onPlayer(&Player::repeatChanged, kPlayer, {"LoopStatus", "CanGoNext", "CanGoPrevious"});
    onPlayer(&Player::seekableChanged, kPlayer, {"CanSeek"});
    connect(m_player, &Player::seeked, control, [this, control] { emit control->Seeked(micro(m_player->position())); });
}

Mpris::~Mpris() {
    if (m_service.isEmpty())
        return;
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterService(m_service);
    bus.unregisterObject(kPath);
}

void Mpris::announce(const QString &interface, const QStringList &properties) {
    if (m_service.isEmpty())
        return;
    QObject *adaptor = interface == kRoot ? m_root : m_control;
    QVariantMap changed;
    for (const auto &name : properties)
        changed.insert(name, adaptor->property(name.toLatin1().constData()));
    QDBusMessage signal = QDBusMessage::createSignal(kPath, "org.freedesktop.DBus.Properties", "PropertiesChanged");
    signal << interface << changed << QStringList();
    QDBusConnection::sessionBus().send(signal);
}

// org.mpris.MediaPlayer2

MprisRoot::MprisRoot(Mpris *mpris) : QDBusAbstractAdaptor(mpris), m_mpris(mpris) {}

QStringList MprisRoot::supportedUriSchemes() const {
    return {"file", "http", "https", "rtmp", "rtsp", "ftp", "smb"};
}

QStringList MprisRoot::supportedMimeTypes() const {
    return Settings::mediaTypes();
}

void MprisRoot::Raise() {
    emit m_mpris->raiseRequested();
}

void MprisRoot::Quit() {
    emit m_mpris->quitRequested();
}

// org.mpris.MediaPlayer2.Player

MprisPlayer::MprisPlayer(Mpris *mpris) : QDBusAbstractAdaptor(mpris), m_mpris(mpris) {}

Player *MprisPlayer::player() const {
    return m_mpris->player();
}

QString MprisPlayer::playbackStatus() const {
    if (player()->idle())
        return QStringLiteral("Stopped");
    return player()->paused() ? QStringLiteral("Paused") : QStringLiteral("Playing");
}

QString MprisPlayer::loopStatus() const {
    const QString mode = player()->repeatMode();
    return mode == "file" ? QStringLiteral("Track") : mode == "playlist" ? QStringLiteral("Playlist") : QStringLiteral("None");
}

void MprisPlayer::setLoopStatus(const QString &status) {
    player()->set("loop-file", status == "Track" ? "inf" : "no");
    player()->command({"set", "loop-playlist", status == "Playlist" ? "inf" : "no"}, "repeat");
}

double MprisPlayer::rate() const {
    return player()->speed();
}

void MprisPlayer::setRate(double rate) {
    if (rate > 0)
        player()->command({"set", "speed", qBound(minimumRate(), rate, maximumRate())}, "speed");
}

void MprisPlayer::setShuffle(bool on) {
    if (on)
        player()->command({"playlist-shuffle"}, "shuffle");
}

QVariantMap MprisPlayer::metadata() const {
    QVariantMap map;
    if (player()->idle())
        return map;
    const int index = qMax(0, player()->playlistPosition());
    map.insert("mpris:trackid", QVariant::fromValue(QDBusObjectPath(QString("/org/omaplayer/track/%1").arg(index))));
    if (player()->duration() > 0)
        map.insert("mpris:length", micro(player()->duration()));
    const QString path = player()->path();
    const bool stream = path.contains("://") && !path.startsWith("file://");
    map.insert("xesam:url", stream ? path : QUrl::fromLocalFile(path).toString());
    if (!player()->title().isEmpty())
        map.insert("xesam:title", player()->title());
    // Tags from the file (music, mostly), or the uploader of a stream.
    const QVariantMap tags = player()->get("metadata").toMap();
    for (auto it = tags.begin(); it != tags.end(); ++it) {
        const QString key = it.key().toLower();
        if (key == "artist" || key == "uploader")
            map.insert("xesam:artist", QStringList{it.value().toString()});
        else if (key == "album")
            map.insert("xesam:album", it.value().toString());
    }
    return map;
}

double MprisPlayer::volume() const {
    return player()->muted() ? 0.0 : player()->volume() / 100.0;
}

void MprisPlayer::setVolume(double volume) {
    player()->command({"set", "volume", qBound(0.0, volume, 1.5) * 100.0}, "volume");
}

qlonglong MprisPlayer::position() const {
    return micro(player()->get("time-pos").toDouble());
}

bool MprisPlayer::canGoNext() const {
    return !player()->idle() && (player()->playlistPosition() < player()->playlistCount() - 1 || player()->repeatMode() == "playlist");
}

bool MprisPlayer::canGoPrevious() const {
    return !player()->idle() && (player()->playlistPosition() > 0 || player()->repeatMode() == "playlist");
}

bool MprisPlayer::canPlay() const {
    return !player()->idle();
}

bool MprisPlayer::canPause() const {
    return !player()->idle();
}

bool MprisPlayer::canSeek() const {
    return !player()->idle() && player()->seekable();
}

void MprisPlayer::Next() {
    player()->command({"playlist-next"}, "playlist");
}

void MprisPlayer::Previous() {
    player()->command({"playlist-prev"}, "playlist");
}

void MprisPlayer::Pause() {
    if (!player()->idle() && !player()->paused())
        player()->togglePause();
}

void MprisPlayer::PlayPause() {
    if (!player()->idle())
        player()->togglePause();
}

void MprisPlayer::Stop() {
    player()->stop();
}

void MprisPlayer::Play() {
    if (!player()->idle() && player()->paused())
        player()->togglePause();
}

void MprisPlayer::Seek(qlonglong offset) {
    if (canSeek())
        player()->command({"seek", offset / 1e6}, "position");
}

void MprisPlayer::SetPosition(const QDBusObjectPath &track, qlonglong position) {
    const QString current = metadata().value("mpris:trackid").value<QDBusObjectPath>().path();
    if (!canSeek() || track.path() != current || position < 0 || position > micro(player()->duration()))
        return;
    player()->seekTo(position / 1e6, true);
}

void MprisPlayer::OpenUri(const QString &uri) {
    player()->open({uri});
}
