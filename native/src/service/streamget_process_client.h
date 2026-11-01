#pragma once

#include "service/stream_service_protocol.h"

#include <QHash>
#include <QDeadlineTimer>
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QStringList>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class QTimer;

class StreamgetProcessClient final : public QObject {
    Q_OBJECT

public:
    static constexpr int kMaxQueuedRequests = 16;
    static constexpr qsizetype kMaxJsonLineBytes = kMaxServiceJsonLineBytes;
    static constexpr qsizetype kMaxOutputBufferBytes = 2 * 1024 * 1024;
    static constexpr qsizetype kReadChunkBytes = 64 * 1024;

    explicit StreamgetProcessClient(QString program,
                                    QStringList arguments = {},
                                    QObject *parent = nullptr);
    ~StreamgetProcessClient() override;

    quint64 ping(int timeoutMs = 5000);
    quint64 resolve(const QString &roomId,
                    StreamQuality quality = StreamQuality::Auto,
                    int timeoutMs = 10000,
                    int qualityRate = -1);
    quint64 search(const QString &query, int timeoutMs = 10000);
    quint64 status(const QString &roomId, int timeoutMs = 10000);

    bool cancel(quint64 requestId);
    void shutdown(int timeoutMs = 1000);

    bool isRunning() const noexcept;
    int activeRequestCount() const noexcept;
    int queuedRequestCount() const noexcept;

signals:
    void childStarted();
    void childStopped();
    void responseReceived(ServiceResponse response);
    void requestFailed(quint64 requestId, QString errorCode);

private slots:
    void onProcessStarted();
    void onProcessReadyRead();
    void onProcessReadyReadStandardError();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    struct PendingRequest {
        ServiceRequest request;
        QTimer *deadline = nullptr;
        QDeadlineTimer deadlineAt;
    };

    quint64 enqueue(ServiceRequest request, int timeoutMs);
    void ensureProcessStarted();
    void pumpQueue();
    void writeRequest(const ServiceRequest &request);
    void writeCancel(quint64 targetRequestId);
    void failRequest(quint64 requestId, const QString &errorCode);
    void failAll(const QString &errorCode);
    void handleProtocolViolation();
    void onRequestTimeout(quint64 requestId);
    void expireQueuedRequests();
    void scheduleQueuedDeadlineTimer();
    void drainStandardError();
#ifdef Q_OS_WIN
    void attachProcessToJob();
    void terminateJob();
#endif

    QString program_;
    QStringList arguments_;
    QProcess *process_ = nullptr;
    QByteArray outputBuffer_;
    QQueue<ServiceRequest> queuedRequests_;
    QHash<quint64, QDeadlineTimer> queuedDeadlines_;
    QHash<quint64, PendingRequest> activeRequests_;
    QTimer *queuedDeadlineTimer_ = nullptr;
    quint64 nextRequestId_ = 1;
    bool shuttingDown_ = false;
    bool protocolFailed_ = false;
    int consecutivePlaybackRequests_ = 0;
#ifdef Q_OS_WIN
    HANDLE job_ = nullptr;
#endif
};
