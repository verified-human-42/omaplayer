#include "player.h"

#include <QClipboard>
#include <QCollator>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QLoggingCategory>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <utility>

#include <mpv/client.h>

#include "subtitles.h"
#include "timing.h"

namespace {
enum Observed : quint64 {
    Idle = 1,
    Pause,
    Title,
    Path,
    TimePos,
    Duration,
    Seekable,
    Volume,
    Mute,
    Speed,
    SubDelay,
    PlaylistPos,
    PlaylistCount,
    Width,
    Height,
    Tracks,
    Playlist,
    Repeat,
    RepeatFile,
};

QVariant fromNode(const mpv_node &node) {
    switch (node.format) {
    case MPV_FORMAT_STRING:
        return QString::fromUtf8(node.u.string);
    case MPV_FORMAT_FLAG:
        return bool(node.u.flag);
    case MPV_FORMAT_INT64:
        return qlonglong(node.u.int64);
    case MPV_FORMAT_DOUBLE:
        return node.u.double_;
    case MPV_FORMAT_NODE_ARRAY: {
        QVariantList list;
        for (int i = 0; i < node.u.list->num; ++i)
            list.append(fromNode(node.u.list->values[i]));
        return list;
    }
    case MPV_FORMAT_NODE_MAP: {
        QVariantMap map;
        for (int i = 0; i < node.u.list->num; ++i)
            map.insert(QString::fromUtf8(node.u.list->keys[i]), fromNode(node.u.list->values[i]));
        return map;
    }
    default:
        return {};
    }
}

QVariant fromProperty(const mpv_event_property &property) {
    switch (property.format) {
    case MPV_FORMAT_FLAG:
        return bool(*static_cast<int *>(property.data));
    case MPV_FORMAT_DOUBLE:
        return *static_cast<double *>(property.data);
    case MPV_FORMAT_INT64:
        return qlonglong(*static_cast<int64_t *>(property.data));
    case MPV_FORMAT_STRING:
        return QString::fromUtf8(*static_cast<char **>(property.data));
    case MPV_FORMAT_NODE:
        return fromNode(*static_cast<mpv_node *>(property.data));
    default:
        return {};
    }
}

// mpv takes every value as text in the C locale, which QByteArray::number
// writes regardless of the user's language.
QByteArray toMpv(const QVariant &value) {
    switch (value.typeId()) {
    case QMetaType::Bool:
        return value.toBool() ? "yes" : "no";
    case QMetaType::Double:
    case QMetaType::Float:
        return QByteArray::number(value.toDouble(), 'g', 12);
    case QMetaType::Int:
    case QMetaType::LongLong:
        return QByteArray::number(value.toLongLong());
    default:
        return value.toString().toUtf8();
    }
}

// A loop option, read as text ("inf", "no") or as a node (true, false, 3).
bool looping(const QVariant &value) {
    const QString text = value.toString();
    return !(text.isEmpty() || text == "no" || text == "false" || text == "0");
}

// Commands that leave the current file, after which its position is saved.
bool leavesFile(const QByteArray &name) {
    return name == "loadfile" || name == "playlist-next" || name == "playlist-prev" || name == "playlist-play-index";
}
}

Player::Player(Output output, QObject *parent) : QObject(parent), m_videoReady(output == Output::None) {
    m_mpv = mpv_create();
    if (!m_mpv) {
        m_error = tr("mpv could not be started.");
        return;
    }

    auto option = [this](const char *name, const QByteArray &value) {
        if (mpv_set_option_string(m_mpv, name, value.constData()) < 0)
            qWarning("mpv rejected option %s=%s", name, value.constData());
    };

    if (output == Output::Video) {
        option("vo", "libmpv");
        option("hwdec", "auto-safe");
    } else {
        option("vo", "null");
        option("ao", "null");
    }

    // Stay open between files and at the end of the last one, with the last
    // frame up, instead of shutting the core down.
    option("idle", "yes");
    option("keep-open", "yes");

    // The window draws every control and message itself, and every key goes
    // through the app's own keymap.
    option("osc", "no");
    // mpv's built-in Lua scripts each start a thread at launch and hold up the
    // first file until they're ready. None is needed here except the yt-dlp
    // hook for stream URLs, and that one loads after launch instead.
    for (const char *script : {"load-stats-overlay", "load-console", "load-auto-profiles", "load-select",
                               "load-positioning", "load-commands", "load-context-menu"})
        option(script, "no");
    option("ytdl", "no");
    option("osd-level", "0");
    option("osd-bar", "no");
    option("input-default-bindings", "no");
    option("input-vo-keyboard", "no");
    option("audio-client-name", "omaplayer");
    option("volume-max", "100");

    // Subtitle files are found and decoded to UTF-8 here (loadSubtitles), so
    // mpv neither looks for them nor guesses their encoding. The folder
    // playlist is built here too rather than by mpv's autocreate-playlist,
    // which starts the file, swaps in the folder, and starts it again.
    option("sub-auto", "no");
    option("sub-codepage", "utf-8");
    // Subtitles stay on the picture, never in the strip the docked bar covers.
    option("sub-use-margins", "no");

    // Pick up where a file was left. Volume stays global, not per file.
    const QString state = QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation);
    option("watch-later-dir", QDir(state).filePath("omaplayer/watch_later").toUtf8());
    option("watch-later-options", "start,aid,sid,secondary-sid,sub-delay,audio-delay");
    option("resume-playback", "yes");

    // For development: OMAPLAYER_MPV_OPTIONS="vf=format=rgb24;hwdec=no" sets
    // mpv options directly, after everything above.
    for (const auto &pair : qEnvironmentVariable("OMAPLAYER_MPV_OPTIONS").split(';', Qt::SkipEmptyParts))
        option(pair.section('=', 0, 0).toUtf8().constData(), pair.section('=', 1).toUtf8());

    mpv_observe_property(m_mpv, Idle, "idle-active", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, Pause, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, Title, "media-title", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, Path, "path", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, TimePos, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, Duration, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, Seekable, "seekable", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, Volume, "volume", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, Mute, "mute", MPV_FORMAT_FLAG);
    mpv_observe_property(m_mpv, Speed, "speed", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, SubDelay, "sub-delay", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_mpv, PlaylistPos, "playlist-pos", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, PlaylistCount, "playlist-count", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, Width, "dwidth", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, Height, "dheight", MPV_FORMAT_INT64);
    mpv_observe_property(m_mpv, Tracks, "track-list", MPV_FORMAT_NODE);
    mpv_observe_property(m_mpv, Playlist, "playlist", MPV_FORMAT_NODE);
    mpv_observe_property(m_mpv, Repeat, "loop-playlist", MPV_FORMAT_STRING);
    mpv_observe_property(m_mpv, RepeatFile, "loop-file", MPV_FORMAT_STRING);

    // OMAPLAYER_MPV_LOG=v (or debug, trace) shows mpv's own log, timestamped.
    const QByteArray logLevel = qgetenv("OMAPLAYER_MPV_LOG");
    mpv_request_log_messages(m_mpv, logLevel.isEmpty() ? "error" : logLevel.constData());

    // mpv calls this from its own threads; events are drained on ours.
    mpv_set_wakeup_callback(m_mpv, [](void *context) {
        auto *player = static_cast<Player *>(context);
        QMetaObject::invokeMethod(player, [player] { player->processEvents(); }, Qt::QueuedConnection);
    }, this);

    if (mpv_initialize(m_mpv) < 0)
        m_error = tr("mpv could not be started.");
    else
        QTimer::singleShot(1500, this, &Player::loadStreamSupport);
}

void Player::loadStreamSupport() {
    if (m_streamSupport || !m_mpv)
        return;
    m_streamSupport = true;
    sendCommand({"load-script", "@ytdl_hook.lua"});
}

Player::~Player() {
    if (!m_mpv)
        return;
    // Synchronous on the way out, so the position is on disk before mpv goes.
    if (!m_idle) {
        const bool finished = m_duration > 0 && m_position > m_duration - 5;
        const char *save[] = {"write-watch-later-config", nullptr};
        const char *forget[] = {"delete-watch-later-config", nullptr};
        mpv_command(m_mpv, finished ? forget : save);
    }
    mpv_set_wakeup_callback(m_mpv, nullptr, nullptr);
    mpv_terminate_destroy(m_mpv);
}

void Player::processEvents() {
    while (m_mpv) {
        mpv_event *event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE)
            break;
        handleEvent(*event);
    }
}

void Player::handleEvent(const mpv_event &event) {
    switch (event.event_id) {
    case MPV_EVENT_PROPERTY_CHANGE:
        handleProperty(event.reply_userdata, fromProperty(*static_cast<mpv_event_property *>(event.data)));
        break;
    case MPV_EVENT_START_FILE: {
        qCInfo(lcTiming, "start file at %lld", QDateTime::currentMSecsSinceEpoch());
        m_endedAtEof = false;
        m_secondChosen = false;
        const QString path = get("path").toString();
        if (!path.isEmpty() && path != m_path) {
            m_path = path;
            emit pathChanged();
        }
        break;
    }
    case MPV_EVENT_FILE_LOADED:
        qCInfo(lcTiming, "file loaded at %lld", QDateTime::currentMSecsSinceEpoch());
        setOpening(false);
        // Back on the file that was playing, the error stays up to say why.
        if (!m_restoring)
            clearError();
        m_restoring = false;
        m_fallbackPath.clear();
        loadSubtitles();
        // Read now: the corner shows them, and their change notices can
        // arrive after this event.
        refresh("position");
        refresh("playlist");
        refresh("playlist-count");
        emit fileLoaded();
        emit osd(m_position > 1 ? "resume" : "file");
        break;
    case MPV_EVENT_PLAYBACK_RESTART:
        refresh("position");
        emit seeked();
        break;
    case MPV_EVENT_END_FILE: {
        const auto *end = static_cast<mpv_event_end_file *>(event.data);
        if (end->reason == MPV_END_FILE_REASON_ERROR) {
            const QString name = QFileInfo(m_path).fileName().isEmpty() ? m_path : QFileInfo(m_path).fileName();
            QString reason;
            switch (end->error) {
            case MPV_ERROR_LOADING_FAILED: reason = tr("the file couldn't be read"); break;
            case MPV_ERROR_UNKNOWN_FORMAT: reason = tr("the format isn't supported"); break;
            case MPV_ERROR_NOTHING_TO_PLAY: reason = tr("it has no audio or video"); break;
            case MPV_ERROR_AO_INIT_FAILED: reason = tr("the audio output didn't start"); break;
            case MPV_ERROR_VO_INIT_FAILED: reason = tr("the video output didn't start"); break;
            default: reason = QString::fromUtf8(mpv_error_string(end->error)); break;
            }
            reportError(tr("Couldn't play %1: %2").arg(name, reason));
            setOpening(false);
            if (!m_fallbackPath.isEmpty()) {
                const QByteArray back = m_fallbackPath.toUtf8();
                const QByteArray start = "start=" + QByteArray::number(m_fallbackPosition, 'f', 3);
                m_fallbackPath.clear();
                m_restoring = true;
                sendCommand({"loadfile", back, "replace", "-1", start});
            }
        } else if (end->reason == MPV_END_FILE_REASON_EOF && !m_path.isEmpty()) {
            // Watched to the end, so next time it starts from the top.
            sendCommand({"delete-watch-later-config", m_path.toUtf8()});
            m_endedAtEof = true;
        }
        break;
    }
    case MPV_EVENT_COMMAND_REPLY: {
        const QString kind = m_pendingOsd.take(event.reply_userdata);
        if (kind.isEmpty())
            break;
        if (event.error < 0) {
            if (kind == "playlist")
                emit osd("playlist-edge");
            else if (kind == "chapter" || kind == "subtitle-line")
                emit osd(kind + "-none");
            else if (kind.startsWith("screenshot"))
                emit osd("screenshot-failed");
            break;
        }
        if (kind.startsWith("screenshot")) {
            finishScreenshot(kind, true);
            break;
        }
        refresh(kind.section(':', 0, 0));
        emit osd(kind);
        break;
    }
    case MPV_EVENT_LOG_MESSAGE: {
        const auto *message = static_cast<mpv_event_log_message *>(event.data);
        qWarning("%lld mpv %s: %s", QDateTime::currentMSecsSinceEpoch() % 100000, message->prefix,
                 QByteArray(message->text).trimmed().constData());
        break;
    }
    default:
        break;
    }
}

void Player::handleProperty(quint64 id, const QVariant &value) {
    auto update = [this](auto &field, const auto &next, void (Player::*signal)()) {
        if (field == next)
            return;
        field = next;
        emit (this->*signal)();
    };

    switch (id) {
    case Idle:
        update(m_idle, value.toBool(), &Player::idleChanged);
        if (m_idle && m_endedAtEof) {
            m_endedAtEof = false;
            emit finished();
        }
        break;
    case Pause: update(m_paused, value.toBool(), &Player::pausedChanged); break;
    case Title: update(m_title, value.toString(), &Player::titleChanged); break;
    case Path:
        if (!value.toString().isEmpty())
            update(m_path, value.toString(), &Player::pathChanged);
        break;
    case TimePos: update(m_position, value.toDouble(), &Player::positionChanged); break;
    case Duration: update(m_duration, value.toDouble(), &Player::durationChanged); break;
    case Seekable: update(m_seekable, value.toBool(), &Player::seekableChanged); break;
    case Volume: update(m_volume, value.toDouble(), &Player::volumeChanged); break;
    case Mute: update(m_muted, value.toBool(), &Player::mutedChanged); break;
    case Speed: update(m_speed, value.isValid() ? value.toDouble() : 1.0, &Player::speedChanged); break;
    case SubDelay: update(m_subDelay, value.toDouble(), &Player::subDelayChanged); break;
    case PlaylistPos: update(m_playlistPosition, value.isValid() ? value.toInt() : -1, &Player::playlistChanged); break;
    case PlaylistCount: update(m_playlistCount, value.toInt(), &Player::playlistChanged); break;
    case Width: update(m_videoSize, QSize(value.toInt(), m_videoSize.height()), &Player::videoSizeChanged); break;
    case Height: update(m_videoSize, QSize(m_videoSize.width(), value.toInt()), &Player::videoSizeChanged); break;
    case Tracks:
        update(m_tracks, value.toList(), &Player::tracksChanged);
        chooseSecondSubtitle();
        break;
    case Playlist: update(m_playlist, value.toList(), &Player::playlistChanged); break;
    case Repeat: update(m_loopPlaylist, looping(value), &Player::repeatChanged); break;
    case RepeatFile: update(m_loopFile, looping(value), &Player::repeatChanged); break;
    default: break;
    }
}

// Re-reads what an action changed right away, so its message never shows the
// value from before mpv's own change notification arrives.
void Player::refresh(const QString &kind) {
    static const QHash<QString, QPair<quint64, const char *>> sources = {
        {"pause", {Pause, "pause"}},
        {"position", {TimePos, "time-pos"}},
        {"frame", {TimePos, "time-pos"}},
        {"volume", {Volume, "volume"}},
        {"mute", {Mute, "mute"}},
        {"speed", {Speed, "speed"}},
        {"sub-delay", {SubDelay, "sub-delay"}},
        {"audio", {Tracks, "track-list"}},
        {"subtitle", {Tracks, "track-list"}},
        {"playlist", {PlaylistPos, "playlist-pos"}},
        {"playlist-count", {PlaylistCount, "playlist-count"}},
        {"repeat", {Repeat, "loop-playlist"}},
        {"repeat-file", {RepeatFile, "loop-file"}},
    };
    const auto source = sources.find(kind);
    if (source != sources.end())
        handleProperty(source->first, get(QString::fromLatin1(source->second)));
    if (kind == "repeat")
        refresh("repeat-file");
}

void Player::rememberPosition() {
    if (m_idle || m_path.isEmpty())
        return;
    const bool finished = m_duration > 0 && m_position > m_duration - 5;
    sendCommand({finished ? "delete-watch-later-config" : "write-watch-later-config"});
}

void Player::sendCommand(const QList<QByteArray> &args, quint64 userdata) {
    if (!m_mpv || args.isEmpty())
        return;
    QList<const char *> argv;
    for (const auto &arg : args)
        argv.append(arg.constData());
    argv.append(nullptr);
    mpv_command_async(m_mpv, userdata, argv.data());
}

QStringList Player::normalize(const QVariantList &items) {
    QStringList paths;
    for (const auto &item : items) {
        const QUrl url = item.typeId() == QMetaType::QUrl ? item.toUrl() : QUrl(item.toString());
        if (url.isLocalFile())
            paths.append(url.toLocalFile());
        else
            paths.append(item.toString());
    }
    return paths;
}

void Player::open(const QVariantList &items) {
    // Subtitle files dropped or opened while something plays join it rather
    // than replace it.
    QStringList media, subtitles;
    for (const auto &path : normalize(items))
        (Subtitles::isSubtitle(path) ? subtitles : media).append(path);
    if (media.isEmpty() && !subtitles.isEmpty() && !m_idle) {
        addSubtitles(subtitles);
        return;
    }
    if (!media.isEmpty())
        openPaths(media);
}

void Player::loadSubtitles() {
    if (m_path.isEmpty() || m_path.contains("://"))
        return;
    addSubtitles(Subtitles::besides(m_path, m_subtitleMode), false);
}

void Player::addSubtitles(const QStringList &paths, bool select) {
    const QString cache = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)).filePath("omaplayer/subtitles");
    QList<Subtitles::Track> tracks;
    for (const auto &path : paths)
        tracks += Subtitles::prepare(path, m_subtitleEncoding, m_language, cache);
    if (tracks.isEmpty())
        return;

    // Shown: the first in a preferred language; failing that, the first one,
    // unless mpv already shows a subtitle (one of the file's own, say).
    QStringList preferred;
    for (const auto &code : get("slang").toList())
        preferred.append(code.toString().toLower());
    auto wanted = [&](const QString &language) {
        const QString code = language.toLower();
        if (code.isEmpty())
            return false;
        for (const auto &want : preferred)
            if (want == code || want.startsWith(code + "-") || code.startsWith(want + "-") || code.section('-', 0, 0) == want)
                return true;
        return false;
    };
    qsizetype shown = -1;
    for (qsizetype i = 0; i < tracks.size() && shown < 0; ++i)
        if (wanted(tracks[i].language))
            shown = i;
    const QVariant current = get("sid");
    if (shown < 0 && (select || !current.isValid() || current.toString() == "no" || current.toBool() == false))
        shown = 0;

    for (qsizetype i = 0; i < tracks.size(); ++i)
        sendCommand({"sub-add", tracks[i].path.toUtf8(), i == shown ? "select" : "auto",
                     tracks[i].title.toUtf8(), tracks[i].language.toUtf8()});
    if (select)
        emit osd("subtitle");
}

void Player::append(const QVariantList &items) {
    if (m_idle && m_playlistCount == 0) {
        open(items);
        return;
    }
    for (const auto &path : normalize(items))
        sendCommand({"loadfile", QFileInfo(path).isRelative() && !path.contains("://") ? QFileInfo(path).absoluteFilePath().toUtf8() : path.toUtf8(), "append"});
}

void Player::openPaths(const QStringList &items) {
    QStringList paths;
    for (const auto &item : items) {
        const QString text = item.trimmed();
        if (text.isEmpty())
            continue;
        // Stream URLs pass straight to mpv (and yt-dlp); anything else is a
        // local path, made absolute so the playlist survives a cd.
        if (text.contains("://") && !text.startsWith("file://"))
            paths.append(text);
        else if (text.startsWith("file://"))
            paths.append(QUrl(text).toLocalFile());
        else
            paths.append(QFileInfo(text).absoluteFilePath());
    }
    load(paths);
}

// mpv can't start its video output before the window's render context
// exists, so files asked for at launch wait the few milliseconds for it.
void Player::load(const QStringList &paths) {
    if (paths.isEmpty())
        return;
    setOpening(true);
    if (!m_videoReady) {
        m_waiting = paths;
        return;
    }

    // A stream URL before the yt-dlp hook is up: load it, and give its script
    // a moment to register before the URL goes to mpv.
    const bool stream = std::any_of(paths.begin(), paths.end(), [](const QString &path) {
        return path.startsWith("http://") || path.startsWith("https://");
    });
    if (stream && !m_streamSupport) {
        loadStreamSupport();
        QTimer::singleShot(300, this, [this, paths] { load(paths); });
        return;
    }
    // Streams come with their captions in the preferred subtitle language,
    // the site's own translations included (YouTube's auto-translate).
    if (stream) {
        const QString language = get("slang").toList().value(0).toString().section('-', 0, 0);
        set("ytdl-raw-options", language.isEmpty() ? QString()
                                                  : QString("write-auto-subs=,sub-langs=%1").arg(language));
    }

    clearError();
    rememberPosition();
    m_stoppedAt = -1;
    for (int i = 0; i < paths.size(); ++i)
        sendCommand({"loadfile", paths[i].toUtf8(), i == 0 ? "replace" : "append"});
    set("pause", false);

    // The chosen file starts first; its neighbours go in around it, in the
    // folder's natural order, before and after.
    if (paths.size() == 1 && m_folderPlaylist) {
        const QStringList folder = folderOf(paths.first());
        const qsizetype at = folder.indexOf(paths.first());
        for (qsizetype i = 0; at > 0 && i < at; ++i)
            sendCommand({"loadfile", folder[i].toUtf8(), "insert-at", QByteArray::number(i)});
        for (qsizetype i = at + 1; at >= 0 && i < folder.size(); ++i)
            sendCommand({"loadfile", folder[i].toUtf8(), "append"});
    }
}

QStringList Player::folderOf(const QString &path) const {
    const QFileInfo file(path);
    if (path.contains("://") || !file.isFile())
        return {};

    // mpv's own lists of what counts as video and audio.
    static const QStringList extensions = [this] {
        QStringList list;
        for (const char *option : {"video-exts", "audio-exts"})
            for (const auto &extension : get(QString::fromLatin1(option)).toList())
                list.append(extension.toString().toLower());
        return list;
    }();

    QStringList entries;
    for (const auto &entry : file.dir().entryInfoList(QDir::Files | QDir::Readable))
        if (extensions.contains(entry.suffix().toLower()))
            entries.append(entry.absoluteFilePath());

    // Natural order, so "Episode 2" comes before "Episode 10".
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(entries.begin(), entries.end(), [&](const QString &a, const QString &b) { return collator.compare(a, b) < 0; });
    return entries.size() > 2000 ? QStringList{} : entries;
}

void Player::setVideoReady() {
    if (m_videoReady)
        return;
    m_videoReady = true;
    qCInfo(lcTiming, "video ready at %lld", QDateTime::currentMSecsSinceEpoch());
    load(std::exchange(m_waiting, {}));
}

bool Player::openClipboard() {
    // Only what can play: files (not folders), and URLs with a scheme mpv
    // streams from. A copied sentence or web address of nothing in
    // particular isn't a reason to stop what's playing.
    static const QStringList schemes{"http", "https", "file", "rtmp", "rtmps", "rtsp", "rtp", "udp", "ftp", "sftp", "smb", "ytdl"};
    const QStringList lines = QGuiApplication::clipboard()->text().split('\n', Qt::SkipEmptyParts);
    QStringList items;
    for (const auto &line : lines) {
        const QString text = line.trimmed();
        const QUrl url(text);
        const bool stream = url.isValid() && schemes.contains(url.scheme().toLower()) && !url.host().isEmpty();
        if (text.contains("://") ? stream || url.isLocalFile() : QFileInfo(text).isFile())
            items.append(text);
    }
    // Nothing to open (empty, plain text, an image) leaves playback alone.
    if (items.isEmpty())
        return false;
    // And if what's there won't play after all, what was playing comes back.
    if (!m_idle && !m_path.isEmpty()) {
        m_fallbackPath = m_path;
        m_fallbackPosition = m_position;
    }
    // Through open(), so a subtitle path joins what's playing.
    QVariantList list;
    for (const auto &item : items)
        list.append(item);
    open(list);
    return true;
}

void Player::command(const QVariantList &args, const QString &osd) {
    QList<QByteArray> list;
    for (const auto &arg : args)
        list.append(toMpv(arg));
    if (list.isEmpty())
        return;
    if (leavesFile(list.first()))
        rememberPosition();
    quint64 userdata = 0;
    if (!osd.isEmpty()) {
        userdata = m_nextRequest++;
        m_pendingOsd.insert(userdata, osd);
    }
    sendCommand(list, userdata);
}

void Player::set(const QString &name, const QVariant &value) {
    if (!m_mpv)
        return;
    const QByteArray text = toMpv(value);
    const char *data = text.constData();
    mpv_set_property_async(m_mpv, 0, name.toUtf8().constData(), MPV_FORMAT_STRING, &data);
}

QVariant Player::get(const QString &name) const {
    if (!m_mpv)
        return {};
    mpv_node node;
    if (mpv_get_property(m_mpv, name.toUtf8().constData(), MPV_FORMAT_NODE, &node) < 0)
        return {};
    const QVariant value = fromNode(node);
    mpv_free_node_contents(&node);
    return value;
}

void Player::playIndex(int index) {
    if (index < 0 || index >= m_playlistCount)
        return;
    rememberPosition();
    m_stoppedAt = -1;
    sendCommand({"playlist-play-index", QByteArray::number(index)});
    set("pause", false);
}

void Player::removeIndex(int index) {
    if (index >= 0 && index < m_playlistCount)
        sendCommand({"playlist-remove", QByteArray::number(index)});
}

void Player::togglePause() {
    // After Stop the playlist stays, and play starts the stopped file over.
    if (m_idle) {
        if (m_playlistCount > 0) {
            sendCommand({"playlist-play-index", QByteArray::number(qMax(0, m_stoppedAt))});
            set("pause", false);
            m_stoppedAt = -1;
        }
        return;
    }
    command({"cycle", "pause"}, "pause");
}

void Player::stop() {
    if (m_idle)
        return;
    m_stoppedAt = m_playlistPosition;
    rememberPosition();
    sendCommand({"stop", "keep-playlist"});
}

void Player::seekTo(double seconds, bool exact) {
    if (m_idle)
        return;
    sendCommand({"seek", QByteArray::number(qMax(0.0, seconds), 'f', 3),
                 exact ? "absolute+exact" : "absolute+keyframes"});
}

void Player::setVideoMargins(double left, double top, double right, double bottom) {
    static const char *names[] = {"video-margin-ratio-left", "video-margin-ratio-top",
                                  "video-margin-ratio-right", "video-margin-ratio-bottom"};
    const double values[] = {left, top, right, bottom};
    for (int i = 0; i < 4; ++i) {
        double value = qBound(0.0, values[i], 1.0);
        if (!m_mpv || qFuzzyCompare(1 + value, 1 + m_margins[i]))
            continue;
        m_margins[i] = value;
        mpv_set_property_async(m_mpv, 0, names[i], MPV_FORMAT_DOUBLE, &value);
    }
}

void Player::setOpening(bool opening) {
    if (opening == m_opening)
        return;
    m_opening = opening;
    emit openingChanged();
}

void Player::reportError(const QString &message) {
    if (m_error == message)
        return;
    m_error = message;
    emit errorChanged();
}

void Player::clearError() {
    reportError({});
}

void Player::cycleRepeat() {
    if (m_loopFile) {
        set("loop-file", "no");
        command({"set", "loop-playlist", "inf"}, "repeat");
    } else if (m_loopPlaylist) {
        command({"set", "loop-playlist", "no"}, "repeat");
    } else {
        command({"set", "loop-file", "inf"}, "repeat");
    }
}

void Player::setLoopPoint(const QString &which) {
    if (m_idle)
        return;
    if (which == "clear") {
        set("ab-loop-a", "no");
        command({"set", "ab-loop-b", "no"}, "ab-loop");
        return;
    }
    const double now = get("time-pos").toDouble();
    const QVariant a = get("ab-loop-a"), b = get("ab-loop-b");
    const bool hasA = a.typeId() == QMetaType::Double, hasB = b.typeId() == QMetaType::Double;
    if (which == "a") {
        // A new start after the end starts a new loop.
        if (hasB && b.toDouble() <= now)
            set("ab-loop-b", "no");
        command({"set", "ab-loop-a", now}, "ab-loop");
    } else {
        // An end at or before the start can't be; the message shows the
        // loop as it stands instead of nothing happening.
        if (hasA && a.toDouble() >= now) {
            emit osd("ab-loop");
            return;
        }
        if (!hasA)
            set("ab-loop-a", 0.0);
        command({"set", "ab-loop-b", now}, "ab-loop");
    }
}

void Player::setScreenshotOptions(const QString &folder, bool subtitles, const QString &format) {
    m_screenshotFolder = folder;
    m_screenshotSubtitles = subtitles;
    m_screenshotFormat = format;
}

void Player::screenshot(bool toClipboard) {
    if (m_idle || !m_mpv)
        return;
    QString path;
    if (toClipboard) {
        const QString cache = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
        QDir().mkpath(QDir(cache).filePath("omaplayer"));
        path = QDir(cache).filePath("omaplayer/frame.png");
    } else {
        QString folder = m_screenshotFolder;
        if (folder.startsWith("~/"))
            folder = QDir::home().filePath(folder.mid(2));
        if (!QDir().mkpath(folder)) {
            emit osd("screenshot-failed");
            return;
        }
        // "Movie 01.02.03.png", named after the file and the moment.
        QString name = m_path.contains("://") ? m_title : QFileInfo(m_path).completeBaseName();
        name.replace('/', '-');
        const int seconds = int(get("time-pos").toDouble());
        const QString stamp = QString("%1.%2.%3").arg(seconds / 3600, 2, 10, QChar('0'))
                                  .arg(seconds / 60 % 60, 2, 10, QChar('0')).arg(seconds % 60, 2, 10, QChar('0'));
        const QString base = QDir(folder).filePath(name + " " + stamp);
        path = base + "." + m_screenshotFormat;
        for (int n = 2; QFileInfo::exists(path); ++n)
            path = QString("%1 (%2).%3").arg(base).arg(n).arg(m_screenshotFormat);
    }
    m_lastScreenshot = path;
    const QString kind = toClipboard ? "screenshot-copy" : "screenshot";
    const QString hwdec = get("hwdec-current").toString();
    if (hwdec.isEmpty() || hwdec == "no")
        command({"screenshot-to-file", path, m_screenshotSubtitles ? "subtitles" : "video"}, kind);
    else
        screenshotInSoftware(path, kind);
}

// A hardware-decoded frame lives on the GPU, out of reach of mpv's own
// screenshot through libmpv. Rather than copy every frame back (slower for
// all playback), a second mpv that decodes in software opens the same moment,
// with the same subtitle, and takes the picture, on a thread of its own.
void Player::screenshotInSoftware(const QString &path, const QString &kind) {
    const QString source = m_path.contains("://") ? get("stream-open-filename").toString() : m_path;
    const double at = get("time-pos").toDouble();
    QString subtitleFile;
    QString subtitleId = "no";
    if (m_screenshotSubtitles && get("sub-visibility").toBool()) {
        const QVariant sid = get("sid");
        for (const auto &item : m_tracks) {
            const QVariantMap track = item.toMap();
            if (track.value("type") == "sub" && track.value("id") == sid) {
                if (track.value("external").toBool())
                    subtitleFile = track.value("external-filename").toString();
                else
                    subtitleId = track.value("id").toString();
            }
        }
    }
    QHash<QByteArray, QByteArray> style;
    for (const char *name : {"sub-delay", "sub-scale", "sub-pos", "sub-font", "sub-border-style", "sub-ass-override",
                             "video-aspect-override", "video-rotate"})
        style.insert(name, toMpv(get(name)));

    auto *worker = QThread::create([this, path, kind, source, at, subtitleFile, subtitleId, style] {
        mpv_handle *shot = mpv_create();
        bool ok = false;
        if (shot) {
            for (const auto &[name, value] : std::initializer_list<std::pair<const char *, QByteArray>>{
                     {"config", "no"}, {"vo", "null"}, {"ao", "null"}, {"hwdec", "no"}, {"pause", "yes"},
                     {"hr-seek", "yes"}, {"ytdl", "no"}, {"load-scripts", "no"}, {"sub-auto", "no"},
                     {"sub-codepage", "utf-8"}, {"resume-playback", "no"}, {"idle", "no"},
                     {"start", QByteArray::number(at, 'f', 3)}, {"sid", subtitleId.toUtf8()}})
                mpv_set_option_string(shot, name, value.constData());
            for (auto it = style.begin(); it != style.end(); ++it)
                mpv_set_option_string(shot, it.key().constData(), it.value().constData());
            if (mpv_initialize(shot) >= 0) {
                const QByteArray file = source.toUtf8();
                const char *load[] = {"loadfile", file.constData(), nullptr};
                mpv_command(shot, load);
                QElapsedTimer clock;
                clock.start();
                while (clock.elapsed() < 15000) {
                    const mpv_event *event = mpv_wait_event(shot, 1);
                    if (event->event_id == MPV_EVENT_FILE_LOADED && !subtitleFile.isEmpty()) {
                        const QByteArray subtitle = subtitleFile.toUtf8();
                        const char *add[] = {"sub-add", subtitle.constData(), "select", nullptr};
                        mpv_command(shot, add);
                    } else if (event->event_id == MPV_EVENT_PLAYBACK_RESTART) {
                        const QByteArray target = path.toUtf8();
                        const char *take[] = {"screenshot-to-file", target.constData(),
                                              subtitleId == "no" && subtitleFile.isEmpty() ? "video" : "subtitles", nullptr};
                        ok = mpv_command(shot, take) >= 0;
                        break;
                    } else if (event->event_id == MPV_EVENT_END_FILE || event->event_id == MPV_EVENT_SHUTDOWN) {
                        break;
                    }
                }
            }
            mpv_terminate_destroy(shot);
        }
        QMetaObject::invokeMethod(this, [this, kind, ok] { finishScreenshot(kind, ok); }, Qt::QueuedConnection);
    });
    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void Player::finishScreenshot(const QString &kind, bool ok) {
    if (ok && kind == "screenshot-copy") {
        // The frame went to a file first; the clipboard keeps its own copy.
        const QImage frame(m_lastScreenshot);
        QFile::remove(m_lastScreenshot);
        ok = !frame.isNull();
        if (ok)
            QGuiApplication::clipboard()->setImage(frame);
    }
    emit osd(ok ? kind : QString("screenshot-failed"));
}

void Player::resetPicture() {
    for (const char *name : {"brightness", "contrast", "saturation", "gamma", "panscan", "video-zoom",
                             "video-pan-x", "video-pan-y", "video-rotate"})
        set(name, 0);
    command({"set", "video-aspect-override", "no"}, "picture-reset");
}

void Player::setSecondLanguage(const QStringList &codes) {
    m_secondLanguage = codes;
    m_secondChosen = false;
    if (codes.isEmpty())
        set("secondary-sid", "no");
    else
        chooseSecondSubtitle();
}

// The first subtitle in the second language that isn't already showing goes
// up top. Tracks arrive in steps (the file's own, then the files beside it),
// so this runs on each change until one is found.
void Player::chooseSecondSubtitle() {
    if (m_secondLanguage.isEmpty() || m_secondChosen || m_idle)
        return;
    for (const auto &item : m_tracks) {
        const QVariantMap track = item.toMap();
        if (track.value("type").toString() != "sub" || track.value("selected").toBool())
            continue;
        const QString code = track.value("lang").toString().toLower();
        if (code.isEmpty())
            continue;
        for (const auto &want : m_secondLanguage)
            if (want == code || code.section('-', 0, 0) == want || want.section('-', 0, 0) == code) {
                m_secondChosen = true;
                set("secondary-sid", track.value("id").toLongLong());
                return;
            }
    }
}
