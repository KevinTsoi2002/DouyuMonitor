#pragma once

#include <QHash>
#include <QObject>
#include <QVector>

#include <functional>
#include <optional>

#include "workspace/room_workspace_types.h"

class QTimer;

struct RoomRefreshTiming {
    int onlineIntervalMs = 60'000;
    int offlineIntervalMs = 120'000;
    QVector<int> retryDelaysMs{30'000, 60'000, 120'000, 240'000};
    int jitterPercent = 10;
};

class RoomStatusScheduler final : public QObject {
    Q_OBJECT

public:
    using StartSearch = std::function<quint64(const QString &roomId)>;
    using CancelRequest = std::function<void(quint64 requestId)>;

    explicit RoomStatusScheduler(StartSearch startSearch,
                                 CancelRequest cancelRequest,
                                 RoomRefreshTiming timing = {},
                                 QObject *parent = nullptr);
    ~RoomStatusScheduler() override;

    void synchronize(const RoomSnapshots &rooms);
    void requestNow(const QString &roomId);
    std::optional<QString> roomForRequest(quint64 requestId) const;
    std::optional<QString> takeCompletedRequest(quint64 requestId, bool succeeded);
    void stop();

private:
    struct Entry {
        RoomLiveStatus liveStatus = RoomLiveStatus::Unknown;
        QTimer *timer = nullptr;
        quint64 requestId = 0;
        quint64 generation = 0;
        int failureCount = 0;
    };

    void scheduleBase(const QString &roomId, Entry &entry);
    void scheduleRetry(const QString &roomId, Entry &entry);
    void schedule(const QString &roomId, Entry &entry, int delayMs);
    void removeRoom(const QString &roomId);
    int baseIntervalMs(RoomLiveStatus status) const noexcept;
    int jitteredDelayMs(const QString &roomId, int delayMs) const noexcept;

    StartSearch startSearch_;
    CancelRequest cancelRequest_;
    RoomRefreshTiming timing_;
    QHash<QString, Entry> entries_;
    QHash<quint64, QString> requestRooms_;
};
