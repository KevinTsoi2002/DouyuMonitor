#pragma once

#include "service/stream_service_protocol.h"
#include "workspace/guild_roster.h"
#include "workspace/native_workspace_types.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

class QTimer;

class SearchTransport : public QObject {
    Q_OBJECT

public:
    explicit SearchTransport(QObject *parent = nullptr) : QObject(parent) {}
    ~SearchTransport() override = default;

    virtual quint64 search(const QString &query) = 0;
    virtual quint64 status(const QString &roomId) = 0;
    virtual void cancel(quint64 requestId) = 0;

signals:
    void searchCompleted(ServiceResponse response);
    void searchFailed(quint64 requestId, QString errorCode);
};

class GuildRoomResolver final : public QObject {
    Q_OBJECT

public:
    explicit GuildRoomResolver(SearchTransport *transport, QObject *parent = nullptr);
    ~GuildRoomResolver() override;

    void setRoster(QVector<GuildMember> roster);
    void setCache(QVector<GuildRoomCacheEntry> cache);
    QVector<GuildRoomCacheEntry> cache() const;
    void start();
    void stop();
    void refreshMetadata(bool forceFullMetadata = false);

    QString roomIdFor(const QString &memberId) const;
    QString avatarUrlFor(const QString &memberId) const;
    QString liveStateFor(const QString &memberId) const;
    QString statusFor(const QString &memberId) const;
    QString setManualRoomId(const QString &memberId, const QString &roomId);
    bool handleResponse(const ServiceResponse &response);
    bool handleFailure(quint64 requestId, const QString &errorCode);
    int nextRetryDelayMsForTest() const noexcept;

signals:
    void memberChanged(QString memberId);
    void cacheChanged();

private:
    struct MetadataRefreshTask {
        QString memberId;
        bool statusOnly = false;
    };

    static bool isValidRoomId(const QString &roomId);
    void scheduleNextRequest(int delayMs);
    void sendNextRequest();
    void completeWithoutRoom(const QString &memberId, const QString &status);
    void scheduleRetry(const QString &memberId);
    void removeQueuedMember(const QString &memberId);
    void enqueueMetadataRefresh(const QString &memberId, bool forceFullMetadata = false);
    void refreshMemberMetadata(const QString &memberId, bool forceFullMetadata = false);
    void applyMetadata(const QString &memberId, const RoomSearchResult &result);
    void applyLiveStatus(const QString &memberId, const ServiceResponse &response);
    void updateCacheLiveState(const QString &memberId, const QString &liveState);
    bool storeCacheEntry(const QString &memberId,
                         const QString &roomId,
                         const QUrl &avatarUrl = {},
                         bool metadataChecked = false);
    bool isExactMatch(const GuildMember &member, const RoomSearchResult &result) const;

    SearchTransport *transport_ = nullptr;
    QTimer *scheduleTimer_ = nullptr;
    QHash<QString, GuildMember> members_;
    QHash<QString, QString> roomIds_;
    QHash<QString, QString> statuses_;
    QHash<QString, QString> avatarUrls_;
    QHash<QString, QString> liveStates_;
    QHash<QString, int> failures_;
    QVector<GuildRoomCacheEntry> cache_;
    QSet<QString> cachedMemberIds_;
    QVector<QString> queue_;
    QSet<QString> queuedMembers_;
    QVector<MetadataRefreshTask> metadataQueue_;
    QSet<QString> queuedMetadataMembers_;
    QString activeMemberId_;
    bool activeRequestIsMetadataRefresh_ = false;
    bool activeRequestStatusOnly_ = false;
    quint64 activeRequestId_ = 0;
    int nextRetryDelayMs_ = 3000;
    bool started_ = false;
};

