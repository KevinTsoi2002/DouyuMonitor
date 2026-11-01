#include "service/streamget_process_client.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QProcess>
#include <QTimer>
#include <algorithm>
#include <limits>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

constexpr int kMaxInFlight = 2;
constexpr int kMaxPlaybackBurst = 4;

bool responseMatchesRequest(const ServiceResponse &response, const ServiceRequest &request)
{
    if (response.operation.has_value() && *response.operation != request.operation) return false;
    if (!response.ok) return true;
    switch (request.operation) {
    case ServiceOperation::Ping:
        return response.pong;
    case ServiceOperation::Resolve:
        return !response.roomId.isEmpty();
    case ServiceOperation::Search:
        return response.search;
    case ServiceOperation::Status:
        return response.status;
    case ServiceOperation::Cancel:
        return response.cancelledRequestId != 0;
    case ServiceOperation::Shutdown:
        return response.shutdown;
    }
    return false;
}

} // namespace

StreamgetProcessClient::StreamgetProcessClient(QString program,
                                               QStringList arguments,
                                               QObject *parent)
    : QObject(parent)
    , program_(std::move(program))
    , arguments_(std::move(arguments))
    , process_(new QProcess(this))
    , queuedDeadlineTimer_(new QTimer(this))
{
    qRegisterMetaType<ServiceResponse>();
    process_->setProcessChannelMode(QProcess::SeparateChannels);
    connect(process_, &QProcess::started, this, &StreamgetProcessClient::onProcessStarted);
    connect(process_, &QProcess::readyReadStandardOutput,
            this, &StreamgetProcessClient::onProcessReadyRead);
    connect(process_, &QProcess::readyReadStandardError,
            this, &StreamgetProcessClient::onProcessReadyReadStandardError);
    connect(process_, &QProcess::errorOccurred,
            this, &StreamgetProcessClient::onProcessError);
    connect(process_, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, &StreamgetProcessClient::onProcessFinished);
    queuedDeadlineTimer_->setSingleShot(true);
    connect(queuedDeadlineTimer_, &QTimer::timeout, this, [this] {
        expireQueuedRequests();
        scheduleQueuedDeadlineTimer();
        pumpQueue();
    });
#ifdef Q_OS_WIN
    job_ = CreateJobObjectW(nullptr, nullptr);
    if (job_ != nullptr) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(job_, JobObjectExtendedLimitInformation,
                                     &limits, sizeof(limits))) {
            CloseHandle(job_);
            job_ = nullptr;
        }
    }
#endif
}

StreamgetProcessClient::~StreamgetProcessClient()
{
    shutdown(250);
#ifdef Q_OS_WIN
    if (job_ != nullptr) {
        CloseHandle(job_);
        job_ = nullptr;
    }
#endif
}

quint64 StreamgetProcessClient::ping(int timeoutMs)
{
    ServiceRequest request;
    request.operation = ServiceOperation::Ping;
    return enqueue(request, timeoutMs);
}

quint64 StreamgetProcessClient::resolve(const QString &roomId,
                                        StreamQuality quality,
                                        int timeoutMs,
                                        int qualityRate)
{
    ServiceRequest request;
    request.operation = ServiceOperation::Resolve;
    request.roomId = roomId;
    request.quality = quality;
    request.qualityRate = qualityRate;
    return enqueue(request, timeoutMs);
}

quint64 StreamgetProcessClient::search(const QString &query, int timeoutMs)
{
    ServiceRequest request;
    request.operation = ServiceOperation::Search;
    request.query = query;
    return enqueue(request, timeoutMs);
}

quint64 StreamgetProcessClient::status(const QString &roomId, int timeoutMs)
{
    ServiceRequest request;
    request.operation = ServiceOperation::Status;
    request.query = roomId;
    return enqueue(request, timeoutMs);
}

bool StreamgetProcessClient::cancel(quint64 requestId)
{
    if (requestId == 0) return false;

    for (auto it = queuedRequests_.begin(); it != queuedRequests_.end(); ++it) {
        if (it->requestId != requestId) continue;
        queuedRequests_.erase(it);
        queuedDeadlines_.remove(requestId);
        scheduleQueuedDeadlineTimer();
        emit requestFailed(requestId, QStringLiteral("CANCELLED"));
        return true;
    }

    auto active = activeRequests_.find(requestId);
    if (active == activeRequests_.end()) return false;
    if (active->deadline) {
        active->deadline->stop();
        active->deadline->deleteLater();
    }
    activeRequests_.erase(active);
    emit requestFailed(requestId, QStringLiteral("CANCELLED"));
    writeCancel(requestId);
    pumpQueue();
    return true;
}

void StreamgetProcessClient::shutdown(int timeoutMs)
{
    if (!process_ || process_->state() == QProcess::NotRunning) {
#ifdef Q_OS_WIN
        terminateJob();
#endif
        failAll(QStringLiteral("CANCELLED"));
        return;
    }

    shuttingDown_ = true;
    failAll(QStringLiteral("CANCELLED"));

    ServiceRequest request;
    request.requestId = nextRequestId_++;
    if (nextRequestId_ == 0) nextRequestId_ = 1;
    request.operation = ServiceOperation::Shutdown;
    if (process_->state() == QProcess::Running) {
        writeRequest(request);
        process_->closeWriteChannel();
    }

    const int boundedTimeout = qBound(0, timeoutMs, 1500);
    if (!process_->waitForFinished(boundedTimeout)) {
#ifdef Q_OS_WIN
        terminateJob();
#endif
        process_->kill();
        process_->waitForFinished(250);
    }
    drainStandardError();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    outputBuffer_.clear();
    shuttingDown_ = false;
}

bool StreamgetProcessClient::isRunning() const noexcept
{
    return process_ && process_->state() == QProcess::Running;
}

int StreamgetProcessClient::activeRequestCount() const noexcept
{
    return activeRequests_.size();
}

int StreamgetProcessClient::queuedRequestCount() const noexcept
{
    return queuedRequests_.size();
}

quint64 StreamgetProcessClient::enqueue(ServiceRequest request, int timeoutMs)
{
    if (shuttingDown_) return 0;
    expireQueuedRequests();
    if (queuedRequests_.size() >= kMaxQueuedRequests) return 0;

    request.requestId = nextRequestId_++;
    if (nextRequestId_ == 0) nextRequestId_ = 1;
    queuedRequests_.enqueue(request);
    queuedDeadlines_.insert(request.requestId,
                            QDeadlineTimer(qBound(1, timeoutMs, 60000)));
    scheduleQueuedDeadlineTimer();
    ensureProcessStarted();
    pumpQueue();
    return request.requestId;
}

void StreamgetProcessClient::ensureProcessStarted()
{
    if (process_->state() != QProcess::NotRunning || shuttingDown_) return;
    protocolFailed_ = false;
    outputBuffer_.clear();
    process_->start(program_, arguments_);
}

void StreamgetProcessClient::pumpQueue()
{
    if (process_->state() != QProcess::Running) return;

    expireQueuedRequests();

    while (activeRequests_.size() < kMaxInFlight && !queuedRequests_.isEmpty()) {
        const auto playback = std::find_if(
            queuedRequests_.cbegin(), queuedRequests_.cend(), [](const ServiceRequest &request) {
                return request.operation == ServiceOperation::Resolve;
            });
        const auto background = std::find_if(
            queuedRequests_.cbegin(), queuedRequests_.cend(), [](const ServiceRequest &request) {
                return request.operation != ServiceOperation::Resolve;
            });
        // Playback takes priority, but a long restore must not starve status/search work.
        const auto selected = playback != queuedRequests_.cend()
                && (consecutivePlaybackRequests_ < kMaxPlaybackBurst
                    || background == queuedRequests_.cend())
            ? playback : background;
        const qsizetype index = std::distance(queuedRequests_.cbegin(), selected);
        const ServiceRequest request = queuedRequests_.takeAt(index);
        if (request.operation == ServiceOperation::Resolve) ++consecutivePlaybackRequests_;
        else consecutivePlaybackRequests_ = 0;
        PendingRequest pending;
        pending.request = request;
        pending.deadlineAt = queuedDeadlines_.take(request.requestId);
        const qint64 remainingMs = pending.deadlineAt.remainingTime();
        if (remainingMs <= 0) {
            emit requestFailed(request.requestId, QStringLiteral("TIMEOUT"));
            continue;
        }
        pending.deadline = new QTimer(this);
        pending.deadline->setSingleShot(true);
        connect(pending.deadline, &QTimer::timeout, this,
                [this, requestId = request.requestId] { onRequestTimeout(requestId); });
        activeRequests_.insert(request.requestId, pending);
        writeRequest(request);
        pending.deadline->start(static_cast<int>(qMin<qint64>(remainingMs, 60000)));
    }
    scheduleQueuedDeadlineTimer();
}

void StreamgetProcessClient::expireQueuedRequests()
{
    for (auto it = queuedRequests_.begin(); it != queuedRequests_.end();) {
        const auto deadline = queuedDeadlines_.constFind(it->requestId);
        if (deadline == queuedDeadlines_.cend() || !deadline->hasExpired()) {
            ++it;
            continue;
        }
        const quint64 requestId = it->requestId;
        it = queuedRequests_.erase(it);
        queuedDeadlines_.remove(requestId);
        emit requestFailed(requestId, QStringLiteral("TIMEOUT"));
    }
}

void StreamgetProcessClient::scheduleQueuedDeadlineTimer()
{
    if (queuedDeadlineTimer_ == nullptr) return;
    if (queuedDeadlines_.isEmpty()) {
        queuedDeadlineTimer_->stop();
        return;
    }

    qint64 remainingMs = (std::numeric_limits<qint64>::max)();
    for (const auto &deadline : queuedDeadlines_) {
        remainingMs = qMin(remainingMs, deadline.remainingTime());
    }
    if (remainingMs <= 0) remainingMs = 1;
    queuedDeadlineTimer_->start(static_cast<int>(qMin<qint64>(remainingMs, 60000)));
}

void StreamgetProcessClient::writeRequest(const ServiceRequest &request)
{
    if (process_->state() != QProcess::Running) return;
    process_->write(encodeRequest(request));
    process_->write("\n");
}

void StreamgetProcessClient::writeCancel(quint64 targetRequestId)
{
    if (process_->state() != QProcess::Running) return;
    ServiceRequest request;
    request.requestId = nextRequestId_++;
    if (nextRequestId_ == 0) nextRequestId_ = 1;
    request.operation = ServiceOperation::Cancel;
    request.targetRequestId = targetRequestId;
    writeRequest(request);
}

void StreamgetProcessClient::failRequest(quint64 requestId, const QString &errorCode)
{
    auto it = activeRequests_.find(requestId);
    if (it == activeRequests_.end()) return;
    if (it->deadline) {
        it->deadline->stop();
        it->deadline->deleteLater();
    }
    activeRequests_.erase(it);
    emit requestFailed(requestId, errorCode);
}

void StreamgetProcessClient::failAll(const QString &errorCode)
{
    consecutivePlaybackRequests_ = 0;
    const QList<quint64> activeIds = activeRequests_.keys();
    for (const quint64 requestId : activeIds) failRequest(requestId, errorCode);
    while (!queuedRequests_.isEmpty()) {
        const quint64 requestId = queuedRequests_.dequeue().requestId;
        queuedDeadlines_.remove(requestId);
        emit requestFailed(requestId, errorCode);
    }
    if (queuedDeadlineTimer_) queuedDeadlineTimer_->stop();
}

void StreamgetProcessClient::handleProtocolViolation()
{
    protocolFailed_ = true;
    failAll(QStringLiteral("INVALID_RESPONSE"));
    if (process_->state() != QProcess::NotRunning) process_->kill();
}

void StreamgetProcessClient::onRequestTimeout(quint64 requestId)
{
    if (!activeRequests_.contains(requestId)) return;
    failRequest(requestId, QStringLiteral("TIMEOUT"));
    writeCancel(requestId);
    pumpQueue();
}

void StreamgetProcessClient::onProcessStarted()
{
#ifdef Q_OS_WIN
    attachProcessToJob();
#endif
    emit childStarted();
    pumpQueue();
}

#ifdef Q_OS_WIN
void StreamgetProcessClient::attachProcessToJob()
{
    if (job_ == nullptr || process_ == nullptr) return;
    const HANDLE processHandle = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE
                                                 | PROCESS_QUERY_LIMITED_INFORMATION,
                                             FALSE,
                                             static_cast<DWORD>(process_->processId()));
    if (processHandle == nullptr) return;
    AssignProcessToJobObject(job_, processHandle);
    CloseHandle(processHandle);
}

void StreamgetProcessClient::terminateJob()
{
    if (job_ != nullptr) TerminateJobObject(job_, 1);
}
#endif

void StreamgetProcessClient::onProcessReadyRead()
{
    const auto drainLines = [this]() -> bool {
        while (true) {
            const qsizetype newline = outputBuffer_.indexOf('\n');
            if (newline < 0) {
                if (outputBuffer_.size() > kMaxJsonLineBytes) {
                    handleProtocolViolation();
                    return false;
                }
                return true;
            }
            if (newline > kMaxJsonLineBytes) {
                handleProtocolViolation();
                return false;
            }
            const QByteArray line = outputBuffer_.left(newline).trimmed();
            outputBuffer_.remove(0, newline + 1);
            if (line.isEmpty()) continue;

            const auto response = decodeResponse(line);
            if (!response.has_value()) {
                handleProtocolViolation();
                return false;
            }
            auto active = activeRequests_.find(response->requestId);
            if (active == activeRequests_.end()) continue;
            if (!responseMatchesRequest(*response, active->request)) {
                failRequest(response->requestId, QStringLiteral("INVALID_RESPONSE"));
                pumpQueue();
                continue;
            }
            if (active->deadline) {
                active->deadline->stop();
                active->deadline->deleteLater();
            }
            activeRequests_.erase(active);
            emit responseReceived(*response);
            pumpQueue();
        }
    };

    while (process_->bytesAvailable() > 0) {
        const QByteArray chunk = process_->read(kReadChunkBytes);
        if (chunk.isEmpty()) break;
        outputBuffer_.append(chunk);
        if (outputBuffer_.size() > kMaxOutputBufferBytes) {
            handleProtocolViolation();
            return;
        }
        if (!drainLines()) return;
    }
    if (outputBuffer_.size() > kMaxOutputBufferBytes) {
        handleProtocolViolation();
        return;
    }
    if (outputBuffer_.indexOf('\n') < 0 && outputBuffer_.size() > kMaxJsonLineBytes) {
        handleProtocolViolation();
    }
}

void StreamgetProcessClient::onProcessReadyReadStandardError()
{
    drainStandardError();
}

void StreamgetProcessClient::drainStandardError()
{
    const QProcess::ProcessChannel previousChannel = process_->readChannel();
    process_->setReadChannel(QProcess::StandardError);
    char chunk[kReadChunkBytes];
    while (process_->bytesAvailable() > 0) {
        if (process_->read(chunk, sizeof(chunk)) <= 0) break;
    }
    process_->setReadChannel(previousChannel);
}

void StreamgetProcessClient::onProcessError(QProcess::ProcessError error)
{
    if (error == QProcess::FailedToStart || process_->state() == QProcess::NotRunning) {
        failAll(QStringLiteral("SERVICE_FAILED"));
    }
}

void StreamgetProcessClient::onProcessFinished(int, QProcess::ExitStatus)
{
    drainStandardError();
    if (!protocolFailed_) failAll(QStringLiteral("SERVICE_FAILED"));
    outputBuffer_.clear();
    emit childStopped();
}
