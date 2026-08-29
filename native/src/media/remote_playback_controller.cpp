#include "media/remote_playback_controller.h"

#include "service/streamget_process_client.h"

#include <QRegularExpression>

namespace {

const QRegularExpression kRoomIdPattern(QStringLiteral(R"(^[0-9]{1,20}$)"));

bool isKnownQuality(StreamQuality quality)
{
    switch (quality) {
    case StreamQuality::Auto:
    case StreamQuality::Original:
    case StreamQuality::Super:
    case StreamQuality::High:
    case StreamQuality::Standard:
        return true;
    }
    return false;
}

} // namespace

RemotePlaybackController::RemotePlaybackController(StreamgetProcessClient *client,
                                                   QObject *parent)
    : QObject(parent)
    , client_(client)
{
    qRegisterMetaType<MediaSource>();
    if (client_ == nullptr) return;

    connect(client_, &StreamgetProcessClient::responseReceived,
            this, &RemotePlaybackController::onResponse);
    connect(client_, &StreamgetProcessClient::requestFailed,
            this, &RemotePlaybackController::onRequestFailed);
}

RemotePlaybackController::~RemotePlaybackController()
{
    release();
}

quint64 RemotePlaybackController::resolve(const QString &roomId, StreamQuality quality)
{
    bumpGeneration();
    if (activeRequestId_ != 0 && client_ != nullptr) {
        const quint64 previousRequestId = activeRequestId_;
        activeRequestId_ = 0;
        client_->cancel(previousRequestId);
    } else {
        activeRequestId_ = 0;
    }

    roomId_.clear();
    if (client_ == nullptr || !kRoomIdPattern.match(roomId).hasMatch()
        || !isKnownQuality(quality)) {
        failWithCode(client_ == nullptr ? QStringLiteral("SERVICE_FAILED")
                                       : QStringLiteral("INVALID_INPUT"));
        return 0;
    }

    roomId_ = roomId;
    setState(State::Resolving);
    activeRequestId_ = client_->resolve(roomId, quality);
    if (activeRequestId_ == 0) {
        failWithCode(QStringLiteral("SERVICE_FAILED"));
    }
    return activeRequestId_;
}

void RemotePlaybackController::cancel()
{
    invalidate(State::Idle);
}

void RemotePlaybackController::stop()
{
    invalidate(State::Idle);
}

void RemotePlaybackController::release()
{
    invalidate(State::Idle);
}

RemotePlaybackController::State RemotePlaybackController::state() const noexcept
{
    return state_;
}

quint64 RemotePlaybackController::generation() const noexcept
{
    return generation_;
}

void RemotePlaybackController::onResponse(ServiceResponse response)
{
    if (state_ != State::Resolving || activeRequestId_ == 0
        || response.requestId != activeRequestId_) {
        return;
    }

    activeRequestId_ = 0;
    if (!response.ok) {
        failWithCode(response.errorCode.isEmpty() ? QStringLiteral("SERVICE_FAILED")
                                                   : response.errorCode);
        return;
    }
    if (response.roomId != roomId_) {
        failWithCode(QStringLiteral("INVALID_RESPONSE"));
        return;
    }
    if (!response.isLive) {
        failWithCode(QStringLiteral("ROOM_OFFLINE"));
        return;
    }
    if (response.variants.isEmpty()) {
        failWithCode(QStringLiteral("INVALID_RESPONSE"));
        return;
    }

    emit variantsReady(response.variants);

    const auto source = MediaSource::fromRemoteVariant(response.roomId, response.variants.first());
    if (!source.has_value()) {
        failWithCode(QStringLiteral("UNSAFE_STREAM_URL"));
        return;
    }

    setState(State::Ready);
    emit sourceReady(*source);
}

void RemotePlaybackController::onRequestFailed(quint64 requestId, QString errorCode)
{
    if (state_ != State::Resolving || activeRequestId_ == 0
        || requestId != activeRequestId_) {
        return;
    }

    activeRequestId_ = 0;
    failWithCode(errorCode.isEmpty() ? QStringLiteral("SERVICE_FAILED") : errorCode);
}

void RemotePlaybackController::invalidate(State nextState)
{
    bumpGeneration();
    const quint64 previousRequestId = activeRequestId_;
    activeRequestId_ = 0;
    if (previousRequestId != 0 && client_ != nullptr) {
        client_->cancel(previousRequestId);
    }
    setState(nextState);
}

void RemotePlaybackController::setState(State state)
{
    if (state_ == state) return;
    state_ = state;
    emit stateChanged(state_);
}

void RemotePlaybackController::bumpGeneration()
{
    ++generation_;
    if (generation_ == 0) ++generation_;
}

void RemotePlaybackController::failWithCode(const QString &errorCode)
{
    setState(State::Error);
    emit failed(errorCode);
}
