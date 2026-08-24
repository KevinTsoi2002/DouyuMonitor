#include "media/player_surface.h"

#include <QFileInfo>
#include <QMetaObject>
#include <QOpenGLContext>
#include <QTimer>

#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>

namespace {

constexpr uint64_t kLoadMediaRequest = 1;
constexpr uint64_t kStopMediaRequest = 2;

void *getProcAddress(void *, const char *name)
{
    return QOpenGLContext::currentContext()
        ? QOpenGLContext::currentContext()->getProcAddress(QByteArray(name))
        : nullptr;
}

} // namespace

PlayerSurface::PlayerSurface(QWidget *parent)
    : QOpenGLWidget(parent)
{
    eventTimer_ = new QTimer(this);
    eventTimer_->setInterval(20);
    connect(eventTimer_, &QTimer::timeout, this, &PlayerSurface::pollMpvEvents);

    mpv_ = mpv_create();
    if (mpv_ == nullptr) {
        return;
    }

    mpvInitialized_ = mpv_initialize(mpv_) >= 0;
    if (!mpvInitialized_) {
        mpv_terminate_destroy(mpv_);
        mpv_ = nullptr;
        return;
    }

    eventTimer_->start();
}

PlayerSurface::~PlayerSurface()
{
    if (eventTimer_ != nullptr) {
        eventTimer_->stop();
    }

    if (renderContext_ != nullptr) {
        makeCurrent();
        mpv_render_context_free(renderContext_);
        renderContext_ = nullptr;
        doneCurrent();
    }

    if (mpv_ != nullptr) {
        mpv_terminate_destroy(mpv_);
        mpv_ = nullptr;
    }
}

bool PlayerSurface::isMpvInitialized() const noexcept
{
    return mpvInitialized_;
}

bool PlayerSurface::isRenderContextReady() const noexcept
{
    return renderContext_ != nullptr;
}

bool PlayerSurface::loadLocalMedia(const QString &path)
{
    const auto source = MediaSource::fromDescriptor(path);
    if (!source.has_value()) {
        playbackState_ = PlaybackState::Error;
        mediaError_ = QStringLiteral("local media file does not exist");
        return false;
    }
    return loadSource(*source);
}

bool PlayerSurface::loadSource(const MediaSource &source)
{
    if (!mpvInitialized_ || mpv_ == nullptr) {
        playbackState_ = PlaybackState::Error;
        mediaError_ = QStringLiteral("libmpv is not initialized");
        return false;
    }

    QByteArray encodedSource;
    if (source.kind() == MediaSource::Kind::LocalFile) {
        const QFileInfo fileInfo(source.localPath());
        if (!fileInfo.isFile()) {
            playbackState_ = PlaybackState::Error;
            mediaError_ = QStringLiteral("local media file does not exist");
            return false;
        }
        encodedSource = fileInfo.absoluteFilePath().toUtf8();
    } else if (source.kind() == MediaSource::Kind::RemoteStream) {
        encodedSource = source.remoteUrl().toEncoded();
    } else {
        playbackState_ = PlaybackState::Error;
        mediaError_ = QStringLiteral("unsupported media source");
        return false;
    }

    mediaLoaded_ = false;
    videoConfigured_ = false;
    firstFrameRendered_ = false;
    playbackState_ = PlaybackState::Loading;
    mediaError_.clear();

    const char *args[] = {
        "loadfile",
        encodedSource.constData(),
        "replace",
        nullptr,
    };
    const int result = mpv_command_async(mpv_, kLoadMediaRequest, args);
    if (result < 0) {
        playbackState_ = PlaybackState::Error;
        mediaError_ = QString::fromUtf8(mpv_error_string(result));
        return false;
    }

    return true;
}

bool PlayerSurface::stop()
{
    if (!mpvInitialized_ || mpv_ == nullptr) {
        playbackState_ = PlaybackState::Error;
        mediaError_ = QStringLiteral("libmpv is not initialized");
        return false;
    }

    const char *args[] = {
        "stop",
        nullptr,
    };
    const int result = mpv_command_async(mpv_, kStopMediaRequest, args);
    if (result < 0) {
        playbackState_ = PlaybackState::Error;
        mediaError_ = QStringLiteral("failed to stop media");
        return false;
    }

    resetMediaState(PlaybackState::Ended);
    return true;
}

void PlayerSurface::release()
{
    if (mpv_ == nullptr || !mpvInitialized_) {
        resetMediaState(PlaybackState::Idle);
        return;
    }

    stop();
    resetMediaState(PlaybackState::Idle);
}

bool PlayerSurface::isMediaLoaded() const noexcept
{
    return mediaLoaded_;
}

bool PlayerSurface::isFirstFrameRendered() const noexcept
{
    return firstFrameRendered_;
}

bool PlayerSurface::setPaused(bool paused)
{
    if (!mpvInitialized_ || mpv_ == nullptr) {
        playbackState_ = PlaybackState::Error;
        mediaError_ = QStringLiteral("libmpv is not initialized");
        return false;
    }

    const int result = mpv_set_property_string(mpv_, "pause", paused ? "yes" : "no");
    if (result < 0) {
        playbackState_ = PlaybackState::Error;
        mediaError_ = QString::fromUtf8(mpv_error_string(result));
        return false;
    }

    if (paused) {
        playbackState_ = PlaybackState::Paused;
    } else if (firstFrameRendered_) {
        playbackState_ = PlaybackState::Playing;
    } else if (mediaLoaded_) {
        playbackState_ = PlaybackState::Loading;
    }

    return true;
}

bool PlayerSurface::isPaused() const noexcept
{
    if (!mpvInitialized_ || mpv_ == nullptr) {
        return false;
    }

    int paused = 0;
    if (mpv_get_property(mpv_, "pause", MPV_FORMAT_FLAG, &paused) < 0) {
        return false;
    }

    return paused != 0;
}

PlayerSurface::PlaybackState PlayerSurface::playbackState() const noexcept
{
    return playbackState_;
}

QString PlayerSurface::mediaError() const
{
    return mediaError_;
}

void PlayerSurface::resetMediaState(PlaybackState state)
{
    mediaLoaded_ = false;
    videoConfigured_ = false;
    firstFrameRendered_ = false;
    playbackState_ = state;
    mediaError_.clear();
    update();
}

void PlayerSurface::initializeGL()
{
    if (!mpvInitialized_ || mpv_ == nullptr || renderContext_ != nullptr) {
        return;
    }

    mpv_opengl_init_params glInitParams{
        .get_proc_address = getProcAddress,
        .get_proc_address_ctx = nullptr,
    };
    const char *apiType = MPV_RENDER_API_TYPE_OPENGL;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(apiType)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &glInitParams},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };

    if (mpv_render_context_create(&renderContext_, mpv_, params) < 0) {
        renderContext_ = nullptr;
        return;
    }

    mpv_render_context_set_update_callback(renderContext_, &PlayerSurface::onMpvUpdate, this);
}

void PlayerSurface::paintGL()
{
    if (renderContext_ == nullptr) {
        return;
    }

    int framebuffer = defaultFramebufferObject();
    int flipY = 1;
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &framebuffer},
        {MPV_RENDER_PARAM_FLIP_Y, &flipY},
        {MPV_RENDER_PARAM_INVALID, nullptr},
    };
    const int result = mpv_render_context_render(renderContext_, params);
    if (result >= 0 && mediaLoaded_ && videoConfigured_) {
        firstFrameRendered_ = true;
        if (playbackState_ == PlaybackState::Loading) {
            playbackState_ = PlaybackState::Playing;
        }
    }
}

void PlayerSurface::resizeGL(int width, int height)
{
    if (renderContext_ == nullptr) {
        return;
    }

    mpv_render_context_set_update_callback(renderContext_, &PlayerSurface::onMpvUpdate, this);
    Q_UNUSED(width);
    Q_UNUSED(height);
}

void PlayerSurface::onMpvUpdate(void *ctx)
{
    auto *surface = static_cast<PlayerSurface *>(ctx);
    if (surface == nullptr) {
        return;
    }

    QMetaObject::invokeMethod(surface, &PlayerSurface::requestFrame, Qt::QueuedConnection);
}

void PlayerSurface::requestFrame()
{
    update();
}

void PlayerSurface::pollMpvEvents()
{
    if (mpv_ == nullptr) {
        return;
    }

    while (true) {
        mpv_event *event = mpv_wait_event(mpv_, 0);
        if (event == nullptr || event->event_id == MPV_EVENT_NONE) {
            return;
        }

        handleMpvEvent(event);
    }
}

void PlayerSurface::handleMpvEvent(const mpv_event *event)
{
    if (event == nullptr) {
        return;
    }

    switch (event->event_id) {
    case MPV_EVENT_COMMAND_REPLY:
        if (event->reply_userdata == kLoadMediaRequest && event->error < 0) {
            playbackState_ = PlaybackState::Error;
            mediaError_ = QString::fromUtf8(mpv_error_string(event->error));
        } else if (event->reply_userdata == kStopMediaRequest && event->error < 0
                   && playbackState_ != PlaybackState::Idle) {
            playbackState_ = PlaybackState::Error;
            mediaError_ = QStringLiteral("failed to stop media");
        }
        break;
    case MPV_EVENT_START_FILE:
        playbackState_ = PlaybackState::Loading;
        break;
    case MPV_EVENT_FILE_LOADED:
        mediaLoaded_ = true;
        update();
        break;
    case MPV_EVENT_VIDEO_RECONFIG:
        videoConfigured_ = true;
        update();
        break;
    case MPV_EVENT_END_FILE: {
        if (playbackState_ == PlaybackState::Idle && !mediaLoaded_) {
            break;
        }
        const auto *endFile = static_cast<const mpv_event_end_file *>(event->data);
        if (endFile != nullptr && endFile->error < 0) {
            playbackState_ = PlaybackState::Error;
            mediaError_ = QString::fromUtf8(mpv_error_string(endFile->error));
        } else {
            playbackState_ = PlaybackState::Ended;
        }
        break;
    }
    default:
        break;
    }
}
