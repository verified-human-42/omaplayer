#include "videorenderer.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QtGui/qguiapplication_platform.h>
#include <cmath>

#include <mpv/render_gl.h>

#include "player.h"
#include "timing.h"

namespace {
void *procAddress(void *, const char *name) {
    QOpenGLContext *context = QOpenGLContext::currentContext();
    return context ? reinterpret_cast<void *>(context->getProcAddress(name)) : nullptr;
}
}

VideoRenderer::VideoRenderer(QQuickWindow *window, Player *player)
    : QObject(window), m_window(window), m_player(player) {
    // mpv needs the compositor connection for VA-API's zero-copy path.
    if (auto *wayland = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>())
        m_waylandDisplay = wayland->display();

    // All of these run on the render thread with the window's GL context
    // current. The context is made as soon as Qt has one, before the first
    // frame, so a file given at launch can start its video output at once.
    connect(window, &QQuickWindow::sceneGraphInitialized, this, &VideoRenderer::createContext, Qt::DirectConnection);
    connect(window, &QQuickWindow::beforeRenderPassRecording, this, &VideoRenderer::render, Qt::DirectConnection);
    connect(window, &QQuickWindow::frameSwapped, this, [this] {
        if (m_context)
            mpv_render_context_report_swap(m_context);
    }, Qt::DirectConnection);
    connect(window, &QQuickWindow::sceneGraphInvalidated, this, &VideoRenderer::release, Qt::DirectConnection);
    connect(player, &Player::fileLoaded, this, [this] { m_awaitingFrame = true; });
}

bool VideoRenderer::createContext() {
    if (m_context)
        return true;
    if (m_failed || !m_player->handle())
        return false;
    // Qt makes its context current for the signals below, but not on every
    // platform for sceneGraphInitialized; the first frame catches that case.
    if (!QOpenGLContext::currentContext())
        return false;

    mpv_opengl_init_params gl{procAddress, nullptr};
    QList<mpv_render_param> params{
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl},
    };
    if (m_waylandDisplay)
        params.append({MPV_RENDER_PARAM_WL_DISPLAY, m_waylandDisplay});
    params.append({MPV_RENDER_PARAM_INVALID, nullptr});

    const int error = mpv_render_context_create(&m_context, m_player->handle(), params.data());
    if (error < 0) {
        m_failed = true;
        m_context = nullptr;
        const QString message = tr("The video output didn't start: %1").arg(QString::fromUtf8(mpv_error_string(error)));
        QMetaObject::invokeMethod(m_player, [player = m_player, message] { player->reportError(message); },
                                  Qt::QueuedConnection);
        return false;
    }
    // A new frame (or anything else mpv wants redrawn) schedules a frame of
    // the whole window; mpv calls this from its own threads.
    mpv_render_context_set_update_callback(m_context, [](void *window) {
        QMetaObject::invokeMethod(static_cast<QQuickWindow *>(window), "update", Qt::QueuedConnection);
    }, m_window);
    qCInfo(lcTiming, "render context at %lld", QDateTime::currentMSecsSinceEpoch());
    QMetaObject::invokeMethod(m_player, [player = m_player] { player->setVideoReady(); }, Qt::QueuedConnection);
    return true;
}

void VideoRenderer::render() {
    if (!createContext())
        return;

    const uint64_t flags = mpv_render_context_update(m_context);
    const qreal ratio = m_window->effectiveDevicePixelRatio();
    // Qt has already bound the pass's target: the window's framebuffer on
    // screen, or a texture when the window is being grabbed.
    GLint target = 0;
    QOpenGLContext::currentContext()->functions()->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &target);
    mpv_opengl_fbo fbo{static_cast<int>(target),
                       static_cast<int>(std::lround(m_window->width() * ratio)),
                       static_cast<int>(std::lround(m_window->height() * ratio)), 0};
    // OpenGL targets have their origin at the bottom.
    int flip = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    m_window->beginExternalCommands();
    mpv_render_context_render(m_context, params);
    m_window->endExternalCommands();

    if ((flags & MPV_RENDER_UPDATE_FRAME) && m_awaitingFrame.exchange(false))
        qCInfo(lcTiming, "first frame at %lld", QDateTime::currentMSecsSinceEpoch());
}

// mpv's render context must go while the GL context it was made with is still
// current, and before the mpv handle itself is destroyed.
void VideoRenderer::release() {
    if (m_context) {
        mpv_render_context_free(m_context);
        m_context = nullptr;
    }
}
