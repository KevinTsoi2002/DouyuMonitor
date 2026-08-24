#pragma once

#include <QOpenGLWidget>
#include <QString>

#include "media/media_source.h"

struct mpv_handle;
struct mpv_event;
struct mpv_render_context;
class QTimer;

class PlayerSurface final : public QOpenGLWidget {
public:
    enum class PlaybackState {
        Idle,
        Loading,
        Playing,
        Paused,
        Ended,
        Error,
    };

    explicit PlayerSurface(QWidget *parent = nullptr);
    ~PlayerSurface() override;

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
    PlaybackState playbackState() const noexcept;
    QString mediaError() const;

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

private:
    static void onMpvUpdate(void *ctx);
    void requestFrame();
    void pollMpvEvents();
    void handleMpvEvent(const mpv_event *event);
    void resetMediaState(PlaybackState state);

    mpv_handle *mpv_ = nullptr;
    mpv_render_context *renderContext_ = nullptr;
    QTimer *eventTimer_ = nullptr;
    bool mpvInitialized_ = false;
    bool mediaLoaded_ = false;
    bool videoConfigured_ = false;
    bool firstFrameRendered_ = false;
    PlaybackState playbackState_ = PlaybackState::Idle;
    QString mediaError_;
};
