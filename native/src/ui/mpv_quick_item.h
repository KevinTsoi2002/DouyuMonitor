#pragma once

#include <QQuickFramebufferObject>
#include <QString>

#include <QSet>

#include <atomic>
#include <memory>

#include "media/media_source.h"

struct mpv_event;
struct mpv_handle;

class MpvQuickItem : public QQuickFramebufferObject {
    Q_OBJECT

public:
    enum class PlaybackState {
        Idle,
        Loading,
        Playing,
        Paused,
        Ended,
        Error,
    };
    Q_ENUM(PlaybackState)

    explicit MpvQuickItem(QQuickItem *parent = nullptr);
    ~MpvQuickItem() override;

    Renderer *createRenderer() const override;

    bool isMpvInitialized() const noexcept;
    bool isRenderContextReady() const noexcept;
    bool loadLocalMedia(const QString &path);
    bool loadSource(const MediaSource &source);
    bool stop();
    void release();
    bool isMediaLoaded() const noexcept;
    bool isFirstFrameRendered() const noexcept;
    bool setPaused(bool paused);
    bool isPaused() const noexcept;
    bool setMuted(bool muted);
    bool isMuted() const noexcept;
    bool setVolume(int volume);
    int volume() const noexcept;
    bool renderingSuspended() const noexcept;
    void suspendRendering();
    void resumeRendering();
    PlaybackState playbackState() const noexcept;
    QString safeErrorLabel() const;

#ifdef DOUYU_TESTING
    static void resetTeardownObservationForTest();
    static bool wasLastCoreTeardownAfterRenderContextReleaseForTest();
    bool usesWakeupCallbackForTest() const noexcept;
    bool usesTimerPollingForTest() const noexcept;
    int pendingEventDrainCountForTest() const noexcept;
#endif

signals:
    void renderContextReady();
    void playbackFailed();

protected:
    void geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry) override;

private:
    struct MpvRenderState;
    class MpvRenderer;
    friend class MpvRenderer;

    static void onMpvWakeup(void *ctx);
    static void onMpvUpdate(void *ctx);
    void requestFrame();
    void notifyRenderContextReady();
    void pollMpvEvents();
    void handleMpvEvent(const mpv_event *event);
    quint64 beginLoadRequest();
    void setAsyncPlaybackError(QString errorCode);
    void resetMediaState(PlaybackState state);
    void scheduleCoreTeardown();

    mpv_handle *mpv_ = nullptr;
    std::shared_ptr<MpvRenderState> renderState_;
    bool mpvInitialized_ = false;
    bool muted_ = true;
    int volume_ = 100;
    QString errorCode_;
    quint64 nextLoadRequestId_ = 3;
    quint64 pendingLoadRequestId_ = 0;
    qint64 activePlaylistEntryId_ = 0;
    QSet<qint64> retiredPlaylistEntryIds_;
};
