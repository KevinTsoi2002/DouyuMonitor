#include <QtTest/QtTest>

#include "workspace/notification_policy.h"
#include "workspace/room_workspace_types.h"

namespace {

RoomSnapshot room(const QString &roomId,
                  RoomLiveStatus liveStatus,
                  RoomPlaybackHealth playbackHealth,
                  const QString &anchorName = QStringLiteral("主播"))
{
    RoomSnapshot snapshot;
    snapshot.roomId = roomId;
    snapshot.metadata.roomId = roomId;
    snapshot.metadata.anchorName = anchorName;
    snapshot.metadata.title = QStringLiteral("测试房间");
    snapshot.liveStatus = liveStatus;
    snapshot.playbackHealth = playbackHealth;
    return snapshot;
}

RoomSnapshot offlineRoom(const QString &roomId)
{
    return room(roomId, RoomLiveStatus::Offline, RoomPlaybackHealth::Pending);
}

RoomSnapshot onlineRoom(const QString &roomId)
{
    return room(roomId, RoomLiveStatus::Online, RoomPlaybackHealth::Pending);
}

RoomSnapshot playingRoom(const QString &roomId)
{
    return room(roomId, RoomLiveStatus::Online, RoomPlaybackHealth::Playing);
}

RoomSnapshot failedRoom(const QString &roomId)
{
    return room(roomId, RoomLiveStatus::Online, RoomPlaybackHealth::Error);
}

} // namespace

class NotificationPolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void usesFirstSnapshotAsBaseline();
    void deduplicatesAndLimitsEvents();
    void doesNotTreatOfflineAsPlaybackFailure();
};

void NotificationPolicyTest::usesFirstSnapshotAsBaseline()
{
    NotificationPolicy policy([] { return qint64{1'000}; });
    QCOMPARE(policy.update({offlineRoom(QStringLiteral("63136"))}).size(), 0);
    QCOMPARE(policy.update({onlineRoom(QStringLiteral("63136"))}).at(0).type,
             NotificationEventType::RoomOnline);
}

void NotificationPolicyTest::deduplicatesAndLimitsEvents()
{
    qint64 now = 1'000;
    NotificationPolicy policy([&now] { return now; });
    policy.update({playingRoom(QStringLiteral("63136"))});
    QCOMPARE(policy.update({failedRoom(QStringLiteral("63136"))}).size(), 1);
    now += 1'000;
    QCOMPARE(policy.update({playingRoom(QStringLiteral("63136"))}).size(), 1);
    QCOMPARE(policy.update({failedRoom(QStringLiteral("63136"))}).size(), 0);

    policy.resetBaseline();
    policy.update({offlineRoom(QStringLiteral("1")), offlineRoom(QStringLiteral("2")),
                   offlineRoom(QStringLiteral("3")), offlineRoom(QStringLiteral("4")),
                   offlineRoom(QStringLiteral("5")), offlineRoom(QStringLiteral("6")),
                   offlineRoom(QStringLiteral("7"))});
    const QVector<NotificationEvent> events = policy.update(
        {onlineRoom(QStringLiteral("1")), onlineRoom(QStringLiteral("2")),
         onlineRoom(QStringLiteral("3")), onlineRoom(QStringLiteral("4")),
         onlineRoom(QStringLiteral("5")), onlineRoom(QStringLiteral("6")),
         onlineRoom(QStringLiteral("7"))});
    QCOMPARE(events.size(), 6);
}

void NotificationPolicyTest::doesNotTreatOfflineAsPlaybackFailure()
{
    NotificationPolicy policy([] { return qint64{1'000}; });
    policy.update({playingRoom(QStringLiteral("63136"))});
    const QVector<NotificationEvent> events =
        policy.update({offlineRoom(QStringLiteral("63136"))});
    QCOMPARE(events.size(), 1);
    QCOMPARE(events.front().type, NotificationEventType::RoomOffline);
}

QTEST_GUILESS_MAIN(NotificationPolicyTest)

#include "notification_policy_test.moc"
