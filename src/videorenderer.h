#pragma once

#include <QObject>
#include <atomic>

class Player;
class QQuickWindow;
struct mpv_render_context;
struct wl_display;

// Draws mpv's video straight into the window's own framebuffer, underneath
// the Qt Quick scene, on Qt's render thread. Nothing is copied through an
// intermediate texture, and the controls are painted over the frame.
class VideoRenderer : public QObject {
    Q_OBJECT

public:
    VideoRenderer(QQuickWindow *window, Player *player);

private:
    bool createContext();
    void render();
    void release();

    QQuickWindow *m_window;
    Player *m_player;
    mpv_render_context *m_context = nullptr;
    wl_display *m_waylandDisplay = nullptr;
    bool m_failed = false;
    std::atomic_bool m_awaitingFrame = false;
};
