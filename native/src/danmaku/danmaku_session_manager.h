#pragma once

#include "danmaku/danmaku_governance.h"
#include "danmaku/douyu_danmaku_client.h"

#include <QHash>
#include <QObject>
#include <QQueue>
#include <QSet>

#include <memory>
#include <optional>

struct DanmakuRoomEligibility {
    QString roomId;
    bool active = false;
    bool roomEnabled = false;
    bool globalEnabled = false;
    bool live = false;
    DanmakuGovernanceSettings governance;
};

class DanmakuSessionManager final : public QObject {
    Q_OBJECT

public:
    explicit DanmakuSessionManager(DanmakuClientFactory factory = {},
                                    QObject *parent = nullptr);

    void synchronize(const QVector<DanmakuRoomEligibility> &rooms);
    void retry(const QString &roomId);
    std::optional<DanmakuMessage> takeNextMessage(const QString &roomId);
    int pendingCount(const QString &roomId) const;
    int activeSessionCount() const;
    DanmakuConnectionStatus statusForRoom(const QString &roomId) const;
    DanmakuGovernanceStats statsForRoom(const QString &roomId) const;
    void clearStats(const QString &roomId);
    void stopAll();
    void clearRoom(const QString &roomId);

signals:
    void roomStateChanged(const QString &roomId);
    void messageAvailable(const QString &roomId);

private:
    struct Session {
        std::unique_ptr<DanmakuClient> client;
        DanmakuGovernanceSettings governance;
        DanmakuGovernanceRuntime runtime = DanmakuGovernance::createRuntime();
        QQueue<DanmakuMessage> queue;
        QSet<QString> seenIds;
        QList<QString> seenOrder;
        DanmakuConnectionStatus status;
    };

    static bool sameGovernance(const DanmakuGovernanceSettings &left,
                               const DanmakuGovernanceSettings &right);
    static bool isEligible(const DanmakuRoomEligibility &room);
    void attachSessionSignals(const QString &roomId, Session &session);
    void handleChat(const QString &roomId, const QMap<QString, QString> &rawChat);
    void removeSession(const QString &roomId);

    DanmakuClientFactory factory_;
    QHash<QString, std::shared_ptr<Session>> sessions_;
    QHash<QString, DanmakuConnectionStatus> statuses_;
};
