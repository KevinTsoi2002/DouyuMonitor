#include "ui/mpv_quick_item.h"

#include <QFileInfo>
#include <QCoreApplication>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QQuickOpenGLUtils>
#include <QQuickWindow>
#include <QRunnable>

#include <mpv/client.h>
#include <mpv/render.h>
#include <mpv/render_gl.h>

namespace {

constexpr quint64 kStopMediaRequest = 2;

#ifdef DOUYU_TESTING
std::atomic_uint64_t g_teardownSequence = 0;
std::atomic_uint64_t g_lastRenderContextReleaseSequence = 0;
std::atomic_uint64_t g_lastCoreTeardownSequence = 0;
#endif

void *getProcAddress(void *, const char *name)
{
    QOpenGLContext *context = QOpenGLContext::currentContext();
    return context != nullptr ? context->getProcAddress(QByteArray(name)) : nullptr;
}

QString safeErrorLabelForCode(const QString &errorCode)
{
    if (errorCode == QStringLiteral("LOCAL_MEDIA_UNAVAILABLE")) {
        return QStringLiteral("本地媒体不可用");
    }
    if (errorCode == QStringLiteral("UNSUPPORTED_SOURCE")) {
        return QStringLiteral("不支持的媒体来源");
    }
    if (errorCode == QStringLiteral("PLAYER_UNAVAILABLE")) {
        return QStringLiteral("播放器不可用");
    }
    if (errorCode == QStringLiteral("LOAD_FAILED")) {
        return QStringLiteral("播放地址不可用");
    }
    if (errorCode == QStringLiteral("STOP_FAILED")) {
        return QStringLiteral("停止播放失败");
    }
    if (errorCode == QStringLiteral("PLAYBACK_FAILED")) {
        return QStringLiteral("播放失败");
    }
    return {};
}

} // namespace

struct MpvQuickItem::MpvRenderState : std::enable_shared_from_this<MpvRenderState> {
    explicit MpvRenderState(MpvQuickItem *item)
        : item_(item)
    {
    }

    void detachItem()
    {
        QMutexLocker locker(&itemMutex_);
        item_ = nullptr;
    }

    void requestFrame()
    {
        if (!acceptUpdates.load() || frameUpdateQueued.exchange(true)) return;

        QMutexLocker locker(&itemMutex_);
        if (!acceptUpdates.load() || item_ == nullptr) {
            frameUpdateQueued.store(false);
            return;
        }
        QMetaObject::invokeMethod(item_, &MpvQuickItem::requestFrame, Qt::QueuedConnection);
    }

    void requestEventDrain()
    {
        if (!acceptUpdates.load() || eventDrainQueued.exchange(true)) return;

        QMutexLocker locker(&itemMutex_);
        if (!acceptUpdates.load() || item_ == nullptr) {
            eventDrainQueued.store(false);
            return;
        }
        QMetaObject::invokeMethod(item_, &MpvQuickItem::pollMpvEvents, Qt::QueuedConnection);
    }

    void releaseRenderContextOnRenderThread()
    {
        mpv_render_context *context = renderContext.exchange(nullptr);
        if (context == nullptr) return;

        mpv_render_context_set_update_callback(context, nullptr, nullptr);
        mpv_render_context_free(context);
        renderContextReady.store(false);
#ifdef DOUYU_TESTING
        g_lastRenderContextReleaseSequence.store(++g_teardownSequence);
#endif
    }

    void destroyCoreOnGuiThread()
    {
        if (renderContext.load() != nullptr) return;
        mpv_handle *mpv = handle.exchange(nullptr);
        if (mpv == nullptr) return;

        mpv_set_wakeup_callback(mpv, nullptr, nullptr);
        wakeupCallbackRegistered.store(false);
#ifdef DOUYU_TESTING
        g_lastCoreTeardownSequence.store(++g_teardownSequence);
#endif
        mpv_terminate_destroy(mpv);
    }

    void queueCoreTeardownOnGuiThread()
    {
        if (coreTeardownQueued.exchange(true)) return;

        QCoreApplication *application = QCoreApplication::instance();
        if (application == nullptr) return;
        const std::shared_ptr<MpvRenderState> state = shared_from_this();
        QMetaObject::invokeMethod(application, [state] { state->destroyCoreOnGuiThread(); },
                                  Qt::QueuedConnection);
    }

    QMutex itemMutex_;
    MpvQuickItem *item_ = nullptr;
    std::atomic<mpv_handle *> handle = nullptr;
    std::atomic<mpv_render_context *> renderContext = nullptr;
    std::atomic_bool rendererAttached = false;
    std::atomic_bool teardownRequested = false;
    std::atomic_bool teardownJobScheduled = false;
    std::atomic_bool coreTeardownQueued = false;
    std::atomic_bool acceptUpdates = true;
    std::atomic_bool frameUpdateQueued = false;
    std::atomic_bool eventDrainQueued = false;
    std::atomic_bool wakeupCallbackRegistered = false;
    std::atomic_bool renderContextReady = false;
    std::atomic_bool mediaLoaded = false;
    std::atomic_bool videoConfigured = false;
    std::atomic_bool firstFrameRendered = false;
    std::atomic<PlaybackState> playbackState = PlaybackState::Idle;
};

class MpvQuickItem::MpvRenderer final : public QQuickFramebufferObject::Renderer {
public:
    explicit MpvRenderer(std::shared_ptr<MpvRenderState> state)
        : state_(std::move(state))
    {
    }

    ~MpvRenderer() override
    {
        if (!state_) return;
        state_->releaseRenderContextOnRenderThread();
        state_->rendererAttached.store(false);
        if (state_->teardownRequested.load()) {
            state_->queueCoreTeardownOnGuiThread();
        }
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        // Layout transitions can briefly report a zero-sized tile. Keep the
        // framebuffer valid so the next non-zero geometry can render again.
        const QSize safeSize(qMax(1, size.width()), qMax(1, size.height()));
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        return new QOpenGLFramebufferObject(safeSize, format);
    }

    void synchronize(QQuickFramebufferObject *item) override
    {
        Q_UNUSED(item);
        if (state_ != nullptr) state_->rendererAttached.store(true);
    }

    void render() override
    {
        if (state_ == nullptr || state_->teardownRequested.load()) return;
        initializeRenderContext();
        mpv_render_context *context = state_->renderContext.load();
        if (context == nullptr || framebufferObject() == nullptr) return;

        const QSize framebufferSize = framebufferObject()->size();
        mpv_opengl_fbo framebuffer{
            .fbo = static_cast<int>(framebufferObject()->handle()),
            .w = framebufferSize.width(),
            .h = framebufferSize.height(),
            .internal_format = 0,
        };
        int flipY = 1;
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_OPENGL_FBO, &framebuffer},
            {MPV_RENDER_PARAM_FLIP_Y, &flipY},
            {MPV_RENDER_PARAM_INVALID, nullptr},
        };
        const int result = mpv_render_context_render(context, params);
        QQuickOpenGLUtils::resetOpenGLState();
        if (result >= 0 && state_->mediaLoaded.load() && state_->videoConfigured.load()) {
            state_->firstFrameRendered.store(true);
            if (state_->playbackState.load() == PlaybackState::Loading) {
                state_->playbackState.store(PlaybackState::Playing);
            }
        }
    }

private:
    void initializeRenderContext()
    {
        if (state_ == nullptr || state_->renderContext.load() != nullptr
            || state_->teardownRequested.load()) {
            return;
        }
        mpv_handle *mpv = state_->handle.load();
        if (mpv == nullptr) return;

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
        mpv_render_context *context = nullptr;
        if (mpv_render_context_create(&context, mpv, params) < 0) {
            state_->playbackState.store(PlaybackState::Error);
            return;
        }

        state_->renderContext.store(context);
        mpv_render_context_set_update_callback(context, &MpvQuickItem::onMpvUpdate,
                                               state_.get());
        state_->renderContextReady.store(true);
    }

    std::shared_ptr<MpvRenderState> state_;
};

MpvQuickItem::MpvQuickItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
    , renderState_(std::make_shared<MpvRenderState>(this))
{
    setMirrorVertically(true);
    setTextureFollowsItemSize(true);
    mpv_ = mpv_create();
    if (mpv_ == nullptr) {
        errorCode_ = QStringLiteral("PLAYER_UNAVAILABLE");
        renderState_->playbackState.store(PlaybackState::Error);
        return;
    }
    renderState_->handle.store(mpv_);

    mpvInitialized_ = mpv_set_option_string(mpv_, "vo", "libmpv") >= 0
        && mpv_initialize(mpv_) >= 0;
    if (!mpvInitialized_) {
        mpv_terminate_destroy(mpv_);
        renderState_->handle.store(nullptr);
        mpv_ = nullptr;
        errorCode_ = QStringLiteral("PLAYER_UNAVAILABLE");
        renderState_->playbackState.store(PlaybackState::Error);
        return;
    }

    mpv_set_property_string(mpv_, "mute", "yes");
    mpv_set_wakeup_callback(mpv_, &MpvQuickItem::onMpvWakeup, renderState_.get());
    renderState_->wakeupCallbackRegistered.store(true);
}

MpvQuickItem::~MpvQuickItem()
{
    if (renderState_ != nullptr) {
        renderState_->acceptUpdates.store(false);
        renderState_->detachItem();
    }
    if (mpv_ != nullptr) {
        mpv_set_wakeup_callback(mpv_, nullptr, nullptr);
        renderState_->wakeupCallbackRegistered.store(false);
    }
    release();
    scheduleCoreTeardown();
    mpv_ = nullptr;
}

void MpvQuickItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickFramebufferObject::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.width() > 0.0 && newGeometry.height() > 0.0
        && renderState_ != nullptr && renderState_->acceptUpdates.load()) {
        // A transient zero-size FBO may have consumed the last mpv update.
        // Request a fresh scene-graph pass when the tile becomes visible again.
        update();
    }
}

QQuickFramebufferObject::Renderer *MpvQuickItem::createRenderer() const
{
    return new MpvRenderer(renderState_);
}

bool MpvQuickItem::isMpvInitialized() const noexcept
{
    return mpvInitialized_;
}

bool MpvQuickItem::isRenderContextReady() const noexcept
{
    return renderState_ != nullptr && renderState_->renderContextReady.load();
}

bool MpvQuickItem::loadLocalMedia(const QString &path)
{
    const auto source = MediaSource::fromDescriptor(path);
    if (!source.has_value()) {
        renderState_->playbackState.store(PlaybackState::Error);
        errorCode_ = QStringLiteral("LOCAL_MEDIA_UNAVAILABLE");
        return false;
    }
    return loadSource(*source);
}

bool MpvQuickItem::loadSource(const MediaSource &source)
{
    if (!mpvInitialized_ || mpv_ == nullptr) {
        renderState_->playbackState.store(PlaybackState::Error);
        errorCode_ = QStringLiteral("PLAYER_UNAVAILABLE");
        return false;
    }

    QByteArray encodedSource;
    if (source.kind() == MediaSource::Kind::LocalFile) {
        const QFileInfo fileInfo(source.localPath());
        if (!fileInfo.isFile()) {
            renderState_->playbackState.store(PlaybackState::Error);
            errorCode_ = QStringLiteral("LOCAL_MEDIA_UNAVAILABLE");
            return false;
        }
        encodedSource = fileInfo.absoluteFilePath().toUtf8();
    } else if (source.kind() == MediaSource::Kind::RemoteStream) {
        encodedSource = source.remoteUrl().toEncoded();
    } else {
        renderState_->playbackState.store(PlaybackState::Error);
        errorCode_ = QStringLiteral("UNSUPPORTED_SOURCE");
        return false;
    }

    renderState_->mediaLoaded.store(false);
    renderState_->videoConfigured.store(false);
    renderState_->firstFrameRendered.store(false);
    renderState_->playbackState.store(PlaybackState::Loading);
    errorCode_.clear();
    const quint64 loadRequestId = beginLoadRequest();
    const char *args[] = {"loadfile", encodedSource.constData(), "replace", nullptr};
    if (mpv_command_async(mpv_, loadRequestId, args) < 0) {
        pendingLoadRequestId_ = 0;
        renderState_->playbackState.store(PlaybackState::Error);
        errorCode_ = QStringLiteral("LOAD_FAILED");
        return false;
    }
    return true;
}

bool MpvQuickItem::stop()
{
    if (!mpvInitialized_ || mpv_ == nullptr) {
        renderState_->playbackState.store(PlaybackState::Error);
        errorCode_ = QStringLiteral("PLAYER_UNAVAILABLE");
        return false;
    }

    const char *args[] = {"stop", nullptr};
    if (mpv_command_async(mpv_, kStopMediaRequest, args) < 0) {
        renderState_->playbackState.store(PlaybackState::Error);
        errorCode_ = QStringLiteral("STOP_FAILED");
        return false;
    }
    resetMediaState(PlaybackState::Ended);
    return true;
}

void MpvQuickItem::release()
{
    if (mpv_ != nullptr && mpvInitialized_) {
        if (pendingLoadRequestId_ != 0) {
            mpv_abort_async_command(mpv_, pendingLoadRequestId_);
            pendingLoadRequestId_ = 0;
        }
        if (activePlaylistEntryId_ != 0) {
            retiredPlaylistEntryIds_.insert(activePlaylistEntryId_);
            activePlaylistEntryId_ = 0;
        }
        stop();
    }
    resetMediaState(PlaybackState::Idle);
}

bool MpvQuickItem::isMediaLoaded() const noexcept
{
    return renderState_ != nullptr && renderState_->mediaLoaded.load();
}

bool MpvQuickItem::isFirstFrameRendered() const noexcept
{
    return renderState_ != nullptr && renderState_->firstFrameRendered.load();
}

bool MpvQuickItem::setPaused(bool paused)
{
    if (!mpvInitialized_ || mpv_ == nullptr
        || mpv_set_property_string(mpv_, "pause", paused ? "yes" : "no") < 0) {
        renderState_->playbackState.store(PlaybackState::Error);
        errorCode_ = QStringLiteral("PLAYER_UNAVAILABLE");
        return false;
    }

    if (paused) {
        renderState_->playbackState.store(PlaybackState::Paused);
    } else if (renderState_->firstFrameRendered.load()) {
        renderState_->playbackState.store(PlaybackState::Playing);
    } else if (renderState_->mediaLoaded.load()) {
        renderState_->playbackState.store(PlaybackState::Loading);
    }
    return true;
}

bool MpvQuickItem::isPaused() const noexcept
{
    if (!mpvInitialized_ || mpv_ == nullptr) return false;
    int paused = 0;
    return mpv_get_property(mpv_, "pause", MPV_FORMAT_FLAG, &paused) >= 0 && paused != 0;
}

bool MpvQuickItem::setMuted(bool muted)
{
    if (!mpvInitialized_ || mpv_ == nullptr
        || mpv_set_property_string(mpv_, "mute", muted ? "yes" : "no") < 0) {
        renderState_->playbackState.store(PlaybackState::Error);
        errorCode_ = QStringLiteral("PLAYER_UNAVAILABLE");
        return false;
    }
    muted_ = muted;
    return true;
}

bool MpvQuickItem::isMuted() const noexcept
{
    if (!mpvInitialized_ || mpv_ == nullptr) return muted_;
    int muted = 0;
    return mpv_get_property(mpv_, "mute", MPV_FORMAT_FLAG, &muted) >= 0 ? muted != 0 : muted_;
}

bool MpvQuickItem::setVolume(int volume)
{
    if (volume < 0 || volume > 100 || !mpvInitialized_ || mpv_ == nullptr) return false;

    const QByteArray value = QByteArray::number(volume);
    if (mpv_set_property_string(mpv_, "volume", value.constData()) < 0) return false;
    volume_ = volume;
    return true;
}

int MpvQuickItem::volume() const noexcept
{
    return volume_;
}

MpvQuickItem::PlaybackState MpvQuickItem::playbackState() const noexcept
{
    return renderState_ != nullptr ? renderState_->playbackState.load() : PlaybackState::Error;
}

QString MpvQuickItem::safeErrorLabel() const
{
    return safeErrorLabelForCode(errorCode_);
}

void MpvQuickItem::scheduleCoreTeardown()
{
    const std::shared_ptr<MpvRenderState> state = renderState_;
    if (state == nullptr) return;

    state->teardownRequested.store(true);
    QQuickWindow *quickWindow = window();
    if (quickWindow != nullptr && state->rendererAttached.load()
        && quickWindow->isSceneGraphInitialized()) {
        class CoreTeardownJob final : public QRunnable {
        public:
            explicit CoreTeardownJob(std::shared_ptr<MpvRenderState> renderState)
                : renderState_(std::move(renderState))
            {
            }

            void run() override
            {
                renderState_->releaseRenderContextOnRenderThread();
                renderState_->queueCoreTeardownOnGuiThread();
                renderState_->teardownJobScheduled.store(false);
            }

        private:
            std::shared_ptr<MpvRenderState> renderState_;
        };

        if (!state->teardownJobScheduled.exchange(true)) {
            quickWindow->scheduleRenderJob(new CoreTeardownJob(state),
                                           QQuickWindow::AfterSynchronizingStage);
            quickWindow->releaseResources();
            quickWindow->update();
        }
        return;
    }

    if (!state->rendererAttached.load() && state->renderContext.load() == nullptr) {
        state->destroyCoreOnGuiThread();
    }
}

#ifdef DOUYU_TESTING
void MpvQuickItem::resetTeardownObservationForTest()
{
    g_teardownSequence.store(0);
    g_lastRenderContextReleaseSequence.store(0);
    g_lastCoreTeardownSequence.store(0);
}

bool MpvQuickItem::wasLastCoreTeardownAfterRenderContextReleaseForTest()
{
    return g_lastRenderContextReleaseSequence.load() != 0
        && g_lastCoreTeardownSequence.load() > g_lastRenderContextReleaseSequence.load();
}

bool MpvQuickItem::usesWakeupCallbackForTest() const noexcept
{
    return renderState_ != nullptr && renderState_->wakeupCallbackRegistered.load();
}

bool MpvQuickItem::usesTimerPollingForTest() const noexcept
{
    return false;
}

int MpvQuickItem::pendingEventDrainCountForTest() const noexcept
{
    return renderState_ != nullptr && renderState_->eventDrainQueued.load() ? 1 : 0;
}
#endif

void MpvQuickItem::onMpvWakeup(void *ctx)
{
    auto *state = static_cast<MpvRenderState *>(ctx);
    if (state != nullptr) state->requestEventDrain();
}

void MpvQuickItem::onMpvUpdate(void *ctx)
{
    auto *state = static_cast<MpvRenderState *>(ctx);
    if (state != nullptr) state->requestFrame();
}

void MpvQuickItem::requestFrame()
{
    if (renderState_ == nullptr) return;
    renderState_->frameUpdateQueued.store(false);
    if (renderState_->acceptUpdates.load()) update();
}

void MpvQuickItem::pollMpvEvents()
{
    const std::shared_ptr<MpvRenderState> state = renderState_;
    if (state == nullptr) return;
    if (mpv_ == nullptr) {
        state->eventDrainQueued.store(false);
        return;
    }
    while (true) {
        mpv_event *event = mpv_wait_event(mpv_, 0);
        if (event == nullptr || event->event_id == MPV_EVENT_NONE) break;
        handleMpvEvent(event);
    }

    state->eventDrainQueued.store(false);

    // Close the race where libmpv queued an event after the drain observed none.
    mpv_event *event = mpv_wait_event(mpv_, 0);
    if (event == nullptr || event->event_id == MPV_EVENT_NONE) return;
    handleMpvEvent(event);
    state->requestEventDrain();
}

void MpvQuickItem::handleMpvEvent(const mpv_event *event)
{
    if (event == nullptr) return;

    switch (event->event_id) {
    case MPV_EVENT_COMMAND_REPLY:
        if (event->reply_userdata == pendingLoadRequestId_) {
            pendingLoadRequestId_ = 0;
            if (event->error < 0 && renderState_->playbackState.load() != PlaybackState::Ended
                && renderState_->playbackState.load() != PlaybackState::Idle) {
                setAsyncPlaybackError(QStringLiteral("LOAD_FAILED"));
            }
        } else if (event->reply_userdata == kStopMediaRequest && event->error < 0
                   && renderState_->playbackState.load() != PlaybackState::Idle) {
            renderState_->playbackState.store(PlaybackState::Error);
            errorCode_ = QStringLiteral("STOP_FAILED");
        }
        break;
    case MPV_EVENT_START_FILE: {
        const auto *startFile = static_cast<const mpv_event_start_file *>(event->data);
        if (startFile == nullptr || renderState_->playbackState.load() == PlaybackState::Idle
            || retiredPlaylistEntryIds_.contains(startFile->playlist_entry_id)) {
            break;
        }
        activePlaylistEntryId_ = startFile->playlist_entry_id;
        renderState_->playbackState.store(PlaybackState::Loading);
        break;
    }
    case MPV_EVENT_FILE_LOADED:
        if (activePlaylistEntryId_ == 0 || renderState_->playbackState.load() == PlaybackState::Idle
            || renderState_->playbackState.load() == PlaybackState::Ended
            || renderState_->playbackState.load() == PlaybackState::Error) {
            break;
        }
        renderState_->mediaLoaded.store(true);
        update();
        break;
    case MPV_EVENT_VIDEO_RECONFIG:
        if (activePlaylistEntryId_ == 0 || renderState_->playbackState.load() == PlaybackState::Idle
            || renderState_->playbackState.load() == PlaybackState::Ended
            || renderState_->playbackState.load() == PlaybackState::Error) {
            break;
        }
        renderState_->videoConfigured.store(true);
        update();
        break;
    case MPV_EVENT_END_FILE: {
        if (renderState_->playbackState.load() == PlaybackState::Idle
            && !renderState_->mediaLoaded.load()) {
            break;
        }
        const auto *endFile = static_cast<const mpv_event_end_file *>(event->data);
        if (endFile == nullptr || retiredPlaylistEntryIds_.remove(endFile->playlist_entry_id)
            || endFile->playlist_entry_id != activePlaylistEntryId_) {
            break;
        }
        activePlaylistEntryId_ = 0;
        if (endFile->error < 0 && renderState_->playbackState.load() != PlaybackState::Ended) {
            setAsyncPlaybackError(QStringLiteral("PLAYBACK_FAILED"));
        } else {
            renderState_->playbackState.store(PlaybackState::Ended);
        }
        break;
    }
    default:
        break;
    }
}

quint64 MpvQuickItem::beginLoadRequest()
{
    if (pendingLoadRequestId_ != 0 && mpv_ != nullptr) {
        mpv_abort_async_command(mpv_, pendingLoadRequestId_);
    }
    if (activePlaylistEntryId_ != 0) {
        retiredPlaylistEntryIds_.insert(activePlaylistEntryId_);
        activePlaylistEntryId_ = 0;
    }
    pendingLoadRequestId_ = nextLoadRequestId_++;
    return pendingLoadRequestId_;
}

void MpvQuickItem::setAsyncPlaybackError(QString errorCode)
{
    const bool wasError = renderState_->playbackState.load() == PlaybackState::Error;
    renderState_->playbackState.store(PlaybackState::Error);
    errorCode_ = std::move(errorCode);
    if (!wasError) emit playbackFailed();
}

void MpvQuickItem::resetMediaState(PlaybackState state)
{
    renderState_->mediaLoaded.store(false);
    renderState_->videoConfigured.store(false);
    renderState_->firstFrameRendered.store(false);
    renderState_->playbackState.store(state);
    errorCode_.clear();
    update();
}
