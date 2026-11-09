#pragma once

#include <QHash>
#include <QObject>
#include <QSize>
#include <QVariant>

struct mpv_handle;
struct mpv_event;

// The one libmpv instance behind the window. Playback state mpv reports is
// mirrored into properties QML binds to; actions go to mpv asynchronously so
// the interface never waits on the player core.
class Player : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool idle READ idle NOTIFY idleChanged)
    // A file has been asked for and hasn't started (or failed) yet.
    Q_PROPERTY(bool opening READ opening NOTIFY openingChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY pausedChanged)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged)
    Q_PROPERTY(QString path READ path NOTIFY pathChanged)
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool seekable READ seekable NOTIFY seekableChanged)
    Q_PROPERTY(double volume READ volume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted NOTIFY mutedChanged)
    Q_PROPERTY(double speed READ speed NOTIFY speedChanged)
    Q_PROPERTY(double subDelay READ subDelay NOTIFY subDelayChanged)
    Q_PROPERTY(int playlistPosition READ playlistPosition NOTIFY playlistChanged)
    Q_PROPERTY(int playlistCount READ playlistCount NOTIFY playlistChanged)
    Q_PROPERTY(QVariantList playlist READ playlist NOTIFY playlistChanged)
    Q_PROPERTY(bool repeat READ repeat NOTIFY repeatChanged)
    // "off", "file" (this one over and over), or "playlist".
    Q_PROPERTY(QString repeatMode READ repeatMode NOTIFY repeatChanged)
    Q_PROPERTY(QSize videoSize READ videoSize NOTIFY videoSizeChanged)
    Q_PROPERTY(QVariantList tracks READ tracks NOTIFY tracksChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    // Video renders into the window; None decodes without any output, for
    // tests and anything else that has no screen.
    enum class Output { Video, None };

    explicit Player(Output output = Output::Video, QObject *parent = nullptr);
    ~Player() override;

    mpv_handle *handle() const { return m_mpv; }

    bool idle() const { return m_idle; }
    bool paused() const { return m_paused; }
    bool opening() const { return m_opening; }
    QString title() const { return m_title; }
    QString path() const { return m_path; }
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    bool seekable() const { return m_seekable; }
    double volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    double speed() const { return m_speed; }
    double subDelay() const { return m_subDelay; }
    int playlistPosition() const { return m_playlistPosition; }
    int playlistCount() const { return m_playlistCount; }
    QVariantList playlist() const { return m_playlist; }
    bool repeat() const { return m_loopFile || m_loopPlaylist; }
    QString repeatMode() const { return m_loopFile ? "file" : m_loopPlaylist ? "playlist" : "off"; }
    QSize videoSize() const { return m_videoSize; }
    QVariantList tracks() const { return m_tracks; }
    QString error() const { return m_error; }

    // Paths, file URLs, or stream URLs. The first replaces what's playing and
    // the rest queue behind it; a single local file brings its folder along
    // as the playlist.
    Q_INVOKABLE void open(const QVariantList &items);
    Q_INVOKABLE void openPaths(const QStringList &items);
    // Adds to the end of the playlist, starting playback only if idle.
    Q_INVOKABLE void append(const QVariantList &items);
    Q_INVOKABLE bool openClipboard();

    // Runs an mpv command. When osd names what the command changes, osd() is
    // emitted once mpv has carried it out, so the message shows the result.
    Q_INVOKABLE void command(const QVariantList &args, const QString &osd = {});
    Q_INVOKABLE void set(const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant get(const QString &name) const;

    Q_INVOKABLE void playIndex(int index);
    Q_INVOKABLE void removeIndex(int index);
    Q_INVOKABLE void togglePause();
    // Off, then this file over and over, then the whole playlist.
    Q_INVOKABLE void cycleRepeat();
    // A-B repeat: "a" and "b" mark the ends at the current moment, "clear"
    // ends the loop. Marking B first loops from the start.
    Q_INVOKABLE void setLoopPoint(const QString &which);
    // Saves the current frame to the screenshot folder, or copies it.
    Q_INVOKABLE void screenshot(bool toClipboard = false);
    Q_INVOKABLE QString lastScreenshot() const { return m_lastScreenshot; }
    // Brightness, contrast, saturation, hue, and gamma off; again, back to
    // what they were.
    Q_INVOKABLE void resetPicture();
    // The subtitle on screen now, to the clipboard: for looking up a word.
    Q_INVOKABLE void copySubtitle();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seekTo(double seconds, bool exact);
    // Fractions of the window the video keeps clear of, for docked controls.
    Q_INVOKABLE void setVideoMargins(double left, double top, double right, double bottom);
    Q_INVOKABLE void clearError();

    void reportError(const QString &message);
    // The window's render context exists; files asked for before it now load.
    void setVideoReady();
    // Opening a single local file queues the rest of its folder around it.
    void setFolderPlaylist(bool enabled) { m_folderPlaylist = enabled; }

    // Subtitle files beside a video are found and decoded here, not by mpv,
    // so any encoding reads right. mode: "fuzzy", "exact", or "no"; encoding:
    // a codepage to force, or empty to tell it from the text.
    void setSubtitleMode(const QString &mode) { m_subtitleMode = mode; }
    void setSubtitleEncoding(const QString &encoding) { m_subtitleEncoding = encoding; }
    void setLanguage(const QString &language) { m_language = language; }
    // A second subtitle in these language codes shows at the top, for
    // watching in two languages at once. Empty turns it off.
    void setSecondLanguage(const QStringList &codes);
    // folder: where screenshots go; subtitles: whether they're in the picture;
    // format: png, jpg, or webp.
    void setScreenshotOptions(const QString &folder, bool subtitles, const QString &format);
    // Adds subtitle files to what's playing; the first is shown.
    Q_INVOKABLE void addSubtitles(const QStringList &paths, bool select = true);
    QStringList folderOf(const QString &path) const;

signals:
    void idleChanged();
    void openingChanged();
    void pausedChanged();
    void titleChanged();
    void pathChanged();
    void positionChanged();
    void durationChanged();
    void seekableChanged();
    void volumeChanged();
    void mutedChanged();
    void speedChanged();
    void subDelayChanged();
    void playlistChanged();
    void repeatChanged();
    void videoSizeChanged();
    void tracksChanged();
    void errorChanged();
    void fileLoaded();
    // Playback picked up again after a seek (or at a file's start).
    void seeked();
    // The last file played to its end and nothing more is queued.
    void finished();
    void osd(const QString &kind);

private:
    void processEvents();
    void handleEvent(const mpv_event &event);
    void handleProperty(quint64 id, const QVariant &value);
    void refresh(const QString &kind);
    void rememberPosition();
    void sendCommand(const QList<QByteArray> &args, quint64 userdata = 0);
    void load(const QStringList &paths);
    void setOpening(bool opening);
    void loadStreamSupport();
    void loadSubtitles();
    void chooseSecondSubtitle();
    void screenshotInSoftware(const QString &path, const QString &kind);
    void finishScreenshot(const QString &kind, bool ok);
    static QStringList normalize(const QVariantList &items);

    mpv_handle *m_mpv = nullptr;
    quint64 m_nextRequest = 1;
    QHash<quint64, QString> m_pendingOsd;
    int m_stoppedAt = -1;
    bool m_videoReady = false;
    bool m_folderPlaylist = true;
    bool m_streamSupport = false;
    QString m_subtitleMode = "fuzzy";
    QString m_subtitleEncoding;
    QString m_language = "en";
    QStringList m_waiting;
    double m_margins[4] = {0, 0, 0, 0};

    bool m_idle = true;
    bool m_paused = false;
    bool m_opening = false;
    QString m_title;
    QString m_path;
    double m_position = 0;
    double m_duration = 0;
    bool m_seekable = false;
    double m_volume = 100;
    bool m_muted = false;
    double m_speed = 1;
    double m_subDelay = 0;
    int m_playlistPosition = -1;
    int m_playlistCount = 0;
    QVariantList m_playlist;
    bool m_loopFile = false;
    bool m_loopPlaylist = false;
    bool m_endedAtEof = false;
    QStringList m_secondLanguage;
    bool m_secondChosen = false;
    QString m_screenshotFolder;
    bool m_screenshotSubtitles = true;
    QString m_screenshotFormat = "png";
    QString m_lastScreenshot;
    QVariantMap m_savedPicture;
    // What to go back to when something opened from the clipboard won't play.
    QString m_fallbackPath;
    double m_fallbackPosition = 0;
    bool m_restoring = false;
    QSize m_videoSize;
    QVariantList m_tracks;
    QString m_error;
};
