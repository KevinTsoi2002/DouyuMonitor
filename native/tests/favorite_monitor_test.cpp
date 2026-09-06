#include <QtTest/QtTest>

#include "workspace/favorite_monitor.h"

namespace {

FavoriteRoomSpec favorite(const QString &roomId,
                          RoomLiveStatus status,
                          const QString &title)
{
    FavoriteRoomSpec room;
    room.roomId = roomId;
    room.metadata.roomId = roomId;
    room.metadata.anchorName = QStringLiteral("主播");
    room.metadata.title = title;
    room.liveStatus = status;
    return room;
}

ServiceResponse result(quint64 requestId,
                       const QString &roomId,
                       bool online,
                       const QString &title)
{
    ServiceResponse response;
    response.requestId = requestId;
    response.ok = true;
    response.search = true;
    RoomSearchResult item;
    item.roomId = roomId;
    item.anchorName = QStringLiteral("主播");
    item.title = title;
    item.online = online;
    response.results.push_back(item);
    return response;
}

} // namespace

class FavoriteMonitorTest final : public QObject {
    Q_OBJECT

private slots:
    void emitsOnlineAndTitleChangesAfterBaseline();
    void removesUnfavoritedRoomAndCancelsRequest();
};

void FavoriteMonitorTest::emitsOnlineAndTitleChangesAfterBaseline()
{
    quint64 nextRequestId = 1;
    FavoriteMonitor monitor(
        [&nextRequestId](const QString &) { return nextRequestId++; },
        [](quint64) {},
        RoomRefreshTiming{60'000, 120'000, {}, 0});

    monitor.synchronize({favorite(QStringLiteral("63136"), RoomLiveStatus::Unknown,
                                   QStringLiteral("旧标题"))});
    QSignalSpy eventsSpy(&monitor, &FavoriteMonitor::eventsReady);
    monitor.onSearchResponse(result(1, QStringLiteral("63136"), false,
                                    QStringLiteral("旧标题")));
    QCOMPARE(eventsSpy.count(), 0);

    monitor.requestNow(QStringLiteral("63136"));
    monitor.onSearchResponse(result(2, QStringLiteral("63136"), true,
                                    QStringLiteral("新标题")));
    QVERIFY(eventsSpy.count() >= 1);
    const auto events = eventsSpy.takeLast().at(0).value<QVector<NotificationEvent>>();
    QVERIFY(!events.isEmpty());
    QCOMPARE(events.front().type, NotificationEventType::RoomOnline);

    monitor.synchronize({favorite(QStringLiteral("63136"), RoomLiveStatus::Online,
                                   QStringLiteral("新标题"))});
    monitor.requestNow(QStringLiteral("63136"));
    monitor.onSearchResponse(result(3, QStringLiteral("63136"), true,
                                    QStringLiteral("更新标题")));
    QVERIFY(eventsSpy.count() >= 1);
    const auto titleEvents = eventsSpy.takeLast().at(0).value<QVector<NotificationEvent>>();
    QVERIFY(!titleEvents.isEmpty());
    QCOMPARE(titleEvents.front().type, NotificationEventType::FavoriteTitleChanged);
}

void FavoriteMonitorTest::removesUnfavoritedRoomAndCancelsRequest()
{
    quint64 requestId = 41;
    quint64 cancelled = 0;
    FavoriteMonitor monitor(
        [&requestId](const QString &) { return requestId; },
        [&cancelled](quint64 id) { cancelled = id; },
        RoomRefreshTiming{60'000, 120'000, {}, 0});

    monitor.synchronize({favorite(QStringLiteral("63136"), RoomLiveStatus::Unknown,
                                   QStringLiteral("标题"))});
    monitor.synchronize({});
    QCOMPARE(cancelled, quint64(41));
}

QTEST_GUILESS_MAIN(FavoriteMonitorTest)

#include "favorite_monitor_test.moc"
