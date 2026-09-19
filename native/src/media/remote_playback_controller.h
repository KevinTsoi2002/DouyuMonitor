#pragma once

#include "media/media_source.h"

#include <QObject>

class StreamgetProcessClient;

class RemotePlaybackController final : public QObject {
    Q_OBJECT

public:
    enum class State {
        Idle,
        Resolving,
        Ready,
        Error,
    };

    explicit RemotePlaybackController(StreamgetProcessClient *client,
                                      QObject *parent = nullptr);
    ~RemotePlaybackController() override;

    quint64 resolve(const QString &roomId, StreamQuality quality, int qualityRate = -1);
    void cancel();
    void stop();
    void release();

    State state() const noexcept;
    quint64 generation() const noexcept;

signals:
    void sourceReady(MediaSource source);
    void variantsReady(QVector<StreamVariant> variants,
                       QVector<StreamQualityOption> qualityOptions);
    void failed(QString errorCode);
    void stateChanged(State state);

private slots:
    void onResponse(ServiceResponse response);
    void onRequestFailed(quint64 requestId, QString errorCode);

private:
    void invalidate(State nextState);
    void setState(State state);
    void bumpGeneration();
    void failWithCode(const QString &errorCode);

    StreamgetProcessClient *client_ = nullptr;
    quint64 activeRequestId_ = 0;
    quint64 generation_ = 0;
    State state_ = State::Idle;
    QString roomId_;
};
