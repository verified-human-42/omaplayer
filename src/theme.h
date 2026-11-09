#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QTimer>

// The current Omarchy theme's palette, followed live, plus Hyprland's window
// rounding. Panels and menus use the theme as it is, light or dark. The
// "stage" colors are for controls sitting on or beside the video: always a
// dark surface taken from the palette, so they read over any picture.
class Theme : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool dark READ dark NOTIFY changed)
    Q_PROPERTY(QString accent READ accent NOTIFY changed)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY changed)
    Q_PROPERTY(QString background READ background NOTIFY changed)
    Q_PROPERTY(QString surface READ surface NOTIFY changed)
    Q_PROPERTY(QString foreground READ foreground NOTIFY changed)
    Q_PROPERTY(QString muted READ muted NOTIFY changed)
    Q_PROPERTY(QString selection READ selection NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString stage READ stage NOTIFY changed)
    Q_PROPERTY(QString stageForeground READ stageForeground NOTIFY changed)
    Q_PROPERTY(QString stageMuted READ stageMuted NOTIFY changed)
    // The palette's own colours, for values that each want a colour of
    // their own (the Tab info panel). Light variants on the dark stage.
    Q_PROPERTY(QString green READ green NOTIFY changed)
    Q_PROPERTY(QString blue READ blue NOTIFY changed)
    Q_PROPERTY(QString cyan READ cyan NOTIFY changed)
    Q_PROPERTY(QString magenta READ magenta NOTIFY changed)
    Q_PROPERTY(QString orange READ orange NOTIFY changed)
    Q_PROPERTY(int radius READ radius NOTIFY changed)

public:
    explicit Theme(const QString &currentDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    bool dark() const { return m_colors.value("mode") != "light"; }
    QString accent() const { return color("accent"); }
    QString accentForeground() const;
    QString background() const { return color("background"); }
    QString surface() const { return color("lighter_background"); }
    QString foreground() const { return color("foreground"); }
    QString muted() const { return color("dark_foreground"); }
    QString selection() const { return color("selection"); }
    QString error() const { return color("red"); }
    QString stage() const { return dark() ? color("background") : color("bright_foreground"); }
    QString stageForeground() const { return dark() ? color("foreground") : color("background"); }
    QString stageMuted() const { return color("dark_foreground"); }
    int radius() const { return m_radius; }

    QString color(const QString &key) const;
    QString green() const { return bright("green"); }
    QString blue() const { return bright("blue"); }
    QString cyan() const { return bright("cyan"); }
    QString magenta() const { return bright("magenta"); }
    QString orange() const { return color("orange"); }
    // The bright_ variant where the palette has one, so it reads on the stage.
    QString bright(const QString &key) const {
        const QString value = color("bright_" + key);
        return value.isEmpty() ? color(key) : value;
    }

    // Every `key = "value"` pair in a colors.toml, values as written.
    static QHash<QString, QString> readColors(const QString &path);

signals:
    void changed();

private:
    void reload();
    void refreshRounding();

    QString m_directory;
    QHash<QString, QString> m_colors;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 0;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll, m_roundingTimeout;
};
