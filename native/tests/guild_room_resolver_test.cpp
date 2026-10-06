#include <QtTest/QtTest>

#include <QDateTime>

#include "workspace/guild_room_resolver.h"

namespace {

class FakeSearchTransport final : public SearchTransport {
public:
    quint64 search(const QString &query) override
    {
        ++requestCount;
        lastQuery = query;
        lastRequestId = nextRequestId++;
        return lastRequestId;
    }

    quint64 status(const QString &roomId) override
    {
        ++statusRequestCount;
        lastStatusRoomId = roomId;
        lastStatusRequestId = nextRequestId++;
        return lastStatusRequestId;
    }

    void cancel(quint64 requestId) override
    {
        cancelledRequestId = requestId;
    }

    int requestCount = 0;
    int statusRequestCount = 0;
    QString lastQuery;
    QString lastStatusRoomId;
    quint64 lastRequestId = 0;
    quint64 lastStatusRequestId = 0;
    quint64 cancelledRequestId = 0;

private:
    quint64 nextRequestId = 1;
};

} // namespace

class GuildRoomResolverTest final : public QObject {
    Q_OBJECT

private slots:
    void resolvesExactAnchorMatchOnly();
    void marksAmbiguousExactMatchesUnconfirmed();
    void backsOffAfterFailureInsteadOfBursting();
    void acceptsManualRoomIdAsVerified();
    void skipsBundledAndCachedRoomIds();
    void exposesFreshMetadataFromSearchResults();
    void prioritizesVerifiedRoomMetadataBeforeResolution();
    void deduplicatesRepeatedMetadataRefreshes();
    void refreshesBundledRoomMetadataWithoutResolvingAgain();
    void cachesFreshLiveStateAfterMetadataRefresh();
    void skipsMetadataRefreshWhenCacheAlreadyComplete();
    void reusesCachedLiveStateWhenFresh();
    void refreshesLiveStatusWithoutRefetchingIdentity();
    void preservesCachedAvatarWhenSearchStatusIsUnknown();
    void resumesCancelledMetadataTaskAfterPause();
};

void GuildRoomResolverTest::resumesCancelledMetadataTaskAfterPause()
{
    FakeSearchTransport transport;
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({{"a", "A", "A", "100"}, {"b", "B", "B", "200"}});
    resolver.start();
    QTRY_COMPARE(transport.requestCount, 1);
    const quint64 cancelledId = transport.lastRequestId;
    const QString cancelledQuery = transport.lastQuery;
    resolver.stop();
    QCOMPARE(transport.cancelledRequestId, cancelledId);
    QTest::qWait(50);
    QCOMPARE(transport.requestCount, 1);
    resolver.start();
    QTRY_COMPARE(transport.requestCount, 2);
    QCOMPARE(transport.lastQuery, cancelledQuery);
    ServiceResponse late;
    late.requestId = cancelledId;
    late.ok = true;
    late.search = true;
    QVERIFY(!resolver.handleResponse(late));
    late.requestId = transport.lastRequestId;
    QVERIFY(resolver.handleResponse(late));
    QTRY_COMPARE(transport.requestCount, 3);
    QVERIFY(transport.lastQuery != cancelledQuery);
}

void GuildRoomResolverTest::resolvesExactAnchorMatchOnly()
{
    FakeSearchTransport transport;
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({{QStringLiteral("hamster-002"),
                         QStringLiteral("主播阿飞"),
                         QStringLiteral("主播阿飞"),
                         QString()}});
    resolver.setCache({});
    resolver.start();

    QCOMPARE(transport.requestCount, 1);
    ServiceResponse response;
    response.requestId = transport.lastRequestId;
    response.ok = true;
    response.search = true;
    response.results = {
        {QStringLiteral("63136"), QStringLiteral("仓鼠特工阿飞"), QStringLiteral("标题")},
        {QStringLiteral("63137"), QStringLiteral("主播阿飞"), QStringLiteral("标题")},
    };

    QVERIFY(resolver.handleResponse(response));
    QCOMPARE(resolver.roomIdFor(QStringLiteral("hamster-002")),
             QStringLiteral("63137"));
    QCOMPARE(resolver.statusFor(QStringLiteral("hamster-002")),
             QStringLiteral("resolved"));
}

void GuildRoomResolverTest::marksAmbiguousExactMatchesUnconfirmed()
{
    FakeSearchTransport transport;
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({{QStringLiteral("hamster-002"),
                         QStringLiteral("主播阿飞"),
                         QStringLiteral("主播阿飞"),
                         QString()}});
    resolver.setCache({});
    resolver.start();

    ServiceResponse response;
    response.requestId = transport.lastRequestId;
    response.ok = true;
    response.search = true;
    response.results = {
        {QStringLiteral("63137"), QStringLiteral("主播阿飞"), QStringLiteral("标题")},
        {QStringLiteral("63138"), QStringLiteral("主播阿飞"), QStringLiteral("标题")},
    };

    QVERIFY(resolver.handleResponse(response));
    QVERIFY(resolver.roomIdFor(QStringLiteral("hamster-002")).isEmpty());
    QCOMPARE(resolver.statusFor(QStringLiteral("hamster-002")),
             QStringLiteral("unconfirmed"));
}

void GuildRoomResolverTest::backsOffAfterFailureInsteadOfBursting()
{
    FakeSearchTransport transport;
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({{QStringLiteral("hamster-002"),
                         QStringLiteral("主播阿飞"),
                         QStringLiteral("主播阿飞"),
                         QString()}});
    resolver.setCache({});
    resolver.start();

    QVERIFY(resolver.handleFailure(transport.lastRequestId,
                                   QStringLiteral("service-unavailable")));
    QCOMPARE(transport.requestCount, 1);
    QCOMPARE(resolver.statusFor(QStringLiteral("hamster-002")),
             QStringLiteral("retrying"));
    QVERIFY(resolver.nextRetryDelayMsForTest() >= 3000);
}

void GuildRoomResolverTest::acceptsManualRoomIdAsVerified()
{
    FakeSearchTransport transport;
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({{QStringLiteral("hamster-002"),
                         QStringLiteral("主播阿飞"),
                         QStringLiteral("主播阿飞"),
                         QString()}});

    QCOMPARE(resolver.setManualRoomId(QStringLiteral("hamster-002"),
                                      QStringLiteral("84452")),
             QString());
    QCOMPARE(resolver.roomIdFor(QStringLiteral("hamster-002")),
             QStringLiteral("84452"));
    QCOMPARE(resolver.statusFor(QStringLiteral("hamster-002")),
             QStringLiteral("resolved"));
    QCOMPARE(resolver.cache().size(), 1);
}

void GuildRoomResolverTest::exposesFreshMetadataFromSearchResults()
{
    FakeSearchTransport transport;
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({{QStringLiteral("hamster-002"),
                         QStringLiteral("主播阿飞"),
                         QStringLiteral("主播阿飞"),
                         QString()}});
    resolver.setCache({});
    resolver.start();

    ServiceResponse response;
    response.requestId = transport.lastRequestId;
    response.ok = true;
    response.search = true;
    RoomSearchResult result;
    result.roomId = QStringLiteral("84452");
    result.anchorName = QStringLiteral("主播阿飞");
    result.online = true;
    result.avatarUrl = QUrl(QStringLiteral("https://example.invalid/avatar.jpg"));
    response.results = {result};

    QVERIFY(resolver.handleResponse(response));
    QCOMPARE(resolver.roomIdFor(QStringLiteral("hamster-002")),
             QStringLiteral("84452"));
    QCOMPARE(resolver.avatarUrlFor(QStringLiteral("hamster-002")),
             QStringLiteral("https://example.invalid/avatar.jpg"));
    QCOMPARE(resolver.liveStateFor(QStringLiteral("hamster-002")),
             QStringLiteral("online"));
    QCOMPARE(resolver.cache().size(), 1);
    QCOMPARE(resolver.cache().first().liveState, QStringLiteral("online"));
    QVERIFY(resolver.cache().first().liveCheckedAtMs > 0);
}

void GuildRoomResolverTest::prioritizesVerifiedRoomMetadataBeforeResolution()
{
    FakeSearchTransport transport;
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({
        {QStringLiteral("hamster-001"),
         QStringLiteral("寅子"),
         QStringLiteral("寅子"),
         QStringLiteral("71415")},
        {QStringLiteral("hamster-002"),
         QStringLiteral("主播阿飞"),
         QStringLiteral("主播阿飞"),
         QString()},
    });
    resolver.setCache({});
    resolver.start();

    QCOMPARE(transport.requestCount, 1);
    QCOMPARE(transport.lastQuery, QStringLiteral("71415"));

    ServiceResponse response;
    response.requestId = transport.lastRequestId;
    response.ok = true;
    response.search = true;
    response.results = {{
        QStringLiteral("71415"),
        QStringLiteral("寅子"),
        QStringLiteral("寅子的直播间"),
    }};
    response.results.first().online = true;
    QVERIFY(resolver.handleResponse(response));

    QTest::qWait(1300);
    QCOMPARE(transport.requestCount, 2);
    QCOMPARE(transport.lastQuery, QStringLiteral("主播阿飞"));
}

void GuildRoomResolverTest::deduplicatesRepeatedMetadataRefreshes()
{
    FakeSearchTransport transport;
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({{QStringLiteral("hamster-001"),
                         QStringLiteral("寅子"),
                         QStringLiteral("寅子"),
                         QStringLiteral("71415")}});
    resolver.setCache({});
    resolver.start();

    QCOMPARE(transport.requestCount, 1);
    resolver.refreshMetadata();
    resolver.refreshMetadata();
    QCOMPARE(transport.requestCount, 1);

    ServiceResponse response;
    response.requestId = transport.lastRequestId;
    response.ok = true;
    response.search = true;
    response.results = {{
        QStringLiteral("71415"),
        QStringLiteral("寅子"),
        QStringLiteral("寅子的直播间"),
    }};
    QVERIFY(resolver.handleResponse(response));
    QCOMPARE(transport.requestCount, 1);
}
void GuildRoomResolverTest::refreshesBundledRoomMetadataWithoutResolvingAgain()
{
    FakeSearchTransport transport;
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({{QStringLiteral("hamster-001"),
                         QStringLiteral("寅子"),
                         QStringLiteral("寅子"),
                         QStringLiteral("71415")}});
    resolver.setCache({});
    resolver.start();

    QCOMPARE(transport.requestCount, 1);
    QCOMPARE(transport.lastQuery, QStringLiteral("71415"));

    ServiceResponse response;
    response.requestId = transport.lastRequestId;
    response.ok = true;
    response.search = true;
    RoomSearchResult result;
    result.roomId = QStringLiteral("71415");
    result.anchorName = QStringLiteral("寅子");
    result.online = false;
    result.avatarUrl = QUrl(QStringLiteral("https://example.invalid/bundled.jpg"));
    response.results = {result};

    QVERIFY(resolver.handleResponse(response));
    QCOMPARE(resolver.statusFor(QStringLiteral("hamster-001")),
             QStringLiteral("resolved"));
    QCOMPARE(resolver.avatarUrlFor(QStringLiteral("hamster-001")),
             QStringLiteral("https://example.invalid/bundled.jpg"));
    QCOMPARE(resolver.liveStateFor(QStringLiteral("hamster-001")),
             QStringLiteral("offline"));
    QCOMPARE(resolver.cache().size(), 1);
    QVERIFY(resolver.cache().first().metadataCheckedAtMs > 0);
    QVERIFY(resolver.cache().first().liveCheckedAtMs > 0);
}

void GuildRoomResolverTest::cachesFreshLiveStateAfterMetadataRefresh()
{
    FakeSearchTransport transport;
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({{
        QStringLiteral("hamster-002"),
        QStringLiteral("主播阿飞"),
        QStringLiteral("主播阿飞"),
        QString(),
    }});
    resolver.setCache({{
        QStringLiteral("hamster-002"),
        QStringLiteral("84452"),
        QStringLiteral("主播阿飞"),
        QUrl(),
        0,
        1,
    }});
    resolver.start();

    QCOMPARE(transport.requestCount, 1);
    QCOMPARE(transport.statusRequestCount, 0);
    QCOMPARE(transport.lastQuery, QStringLiteral("84452"));

    ServiceResponse response;
    response.requestId = transport.lastRequestId;
    response.ok = true;
    response.search = true;
    RoomSearchResult result;
    result.roomId = QStringLiteral("84452");
    result.anchorName = QStringLiteral("主播阿飞");
    result.online = false;
    response.results = {result};
    QVERIFY(resolver.handleResponse(response));
    QCOMPARE(resolver.avatarUrlFor(QStringLiteral("hamster-002")), QString());
    QCOMPARE(resolver.liveStateFor(QStringLiteral("hamster-002")),
             QStringLiteral("offline"));

    resolver.refreshMetadata();
    QTest::qWait(1300);
    QCOMPARE(transport.requestCount, 1);
    QCOMPARE(transport.statusRequestCount, 0);
}

void GuildRoomResolverTest::skipsMetadataRefreshWhenCacheAlreadyComplete()
{
    FakeSearchTransport transport;
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({{
        QStringLiteral("hamster-002"),
        QStringLiteral("主播阿飞"),
        QStringLiteral("主播阿飞"),
        QString(),
    }});
    resolver.setCache({{
        QStringLiteral("hamster-002"),
        QStringLiteral("84452"),
        QStringLiteral("主播阿飞"),
        QUrl(),
        1,
        1,
    }});
    resolver.start();

    QCOMPARE(transport.requestCount, 0);
    QCOMPARE(transport.statusRequestCount, 1);
    QCOMPARE(resolver.avatarUrlFor(QStringLiteral("hamster-002")),
             QString());

    resolver.refreshMetadata();
    QTest::qWait(1300);
    QCOMPARE(transport.statusRequestCount, 1);
    QCOMPARE(transport.lastStatusRoomId, QStringLiteral("84452"));
}

void GuildRoomResolverTest::reusesCachedLiveStateWhenFresh()
{
    FakeSearchTransport transport;
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({{
        QStringLiteral("hamster-002"),
        QStringLiteral("主播阿飞"),
        QStringLiteral("主播阿飞"),
        QString(),
    }});
    resolver.setCache({{
        QStringLiteral("hamster-002"),
        QStringLiteral("84452"),
        QStringLiteral("主播阿飞"),
        QUrl(),
        1,
        1,
        QStringLiteral("online"),
        QDateTime::currentMSecsSinceEpoch(),
    }});
    resolver.start();

    QCOMPARE(transport.requestCount, 0);
    QCOMPARE(transport.statusRequestCount, 0);
    QCOMPARE(resolver.liveStateFor(QStringLiteral("hamster-002")),
             QStringLiteral("online"));
    QCOMPARE(resolver.cache().size(), 1);
    QCOMPARE(resolver.cache().first().liveState, QStringLiteral("online"));
    QVERIFY(resolver.cache().first().liveCheckedAtMs > 0);
}

void GuildRoomResolverTest::skipsBundledAndCachedRoomIds()
{
    FakeSearchTransport transport;
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({
        {QStringLiteral("hamster-001"),
         QStringLiteral("寅子"),
         QStringLiteral("寅子"),
         QStringLiteral("71415")},
        {QStringLiteral("hamster-002"),
         QStringLiteral("主播阿飞"),
         QStringLiteral("主播阿飞"),
         QString()},
        {QStringLiteral("hamster-003"),
         QStringLiteral("午夜抹抹茶"),
         QStringLiteral("午夜抹抹茶"),
         QString()},
    });
    resolver.setCache({{
        QStringLiteral("hamster-002"),
        QStringLiteral("84452"),
        QStringLiteral("主播阿飞"),
        QUrl(),
        0,
        1,
    }});
    resolver.start();

    QCOMPARE(transport.requestCount, 1);
    QCOMPARE(transport.lastQuery, QStringLiteral("71415"));
    QCOMPARE(resolver.roomIdFor(QStringLiteral("hamster-001")),
             QStringLiteral("71415"));
    QCOMPARE(resolver.roomIdFor(QStringLiteral("hamster-002")),
             QStringLiteral("84452"));

    ServiceResponse metadataResponse;
    metadataResponse.requestId = transport.lastRequestId;
    metadataResponse.ok = true;
    metadataResponse.search = true;
    metadataResponse.results = {{
        QStringLiteral("71415"),
        QStringLiteral("寅子"),
        QStringLiteral("寅子的直播间"),
    }};
    QVERIFY(resolver.handleResponse(metadataResponse));

    QTest::qWait(1300);
    QCOMPARE(transport.requestCount, 2);
    QCOMPARE(transport.lastQuery, QStringLiteral("84452"));

    ServiceResponse cachedMetadataResponse;
    cachedMetadataResponse.requestId = transport.lastRequestId;
    cachedMetadataResponse.ok = true;
    cachedMetadataResponse.search = true;
    cachedMetadataResponse.results = {{
        QStringLiteral("84452"),
        QStringLiteral("主播阿飞"),
        QStringLiteral("主播阿飞的直播间"),
    }};
    QVERIFY(resolver.handleResponse(cachedMetadataResponse));

    QTest::qWait(1300);
    QCOMPARE(transport.requestCount, 3);
    QCOMPARE(transport.lastQuery, QStringLiteral("午夜抹抹茶"));

    ServiceResponse response;
    response.requestId = transport.lastRequestId;
    response.ok = true;
    response.search = true;
    response.results = {{
        QStringLiteral("63138"),
        QStringLiteral("午夜抹抹茶"),
        QStringLiteral("午夜抹抹茶的直播间"),
    }};
    QVERIFY(resolver.handleResponse(response));
}

void GuildRoomResolverTest::refreshesLiveStatusWithoutRefetchingIdentity()
{
    FakeSearchTransport transport;
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({{"member", "Anchor", {}, "123"}});
    GuildRoomCacheEntry entry;
    entry.memberId = "member";
    entry.roomId = "123";
    entry.metadataCheckedAtMs = entry.verifiedAtMs = entry.liveCheckedAtMs = QDateTime::currentMSecsSinceEpoch();
    entry.liveState = "online";
    resolver.setCache({entry});
    resolver.start();
    QCOMPARE(transport.requestCount, 0);
    resolver.refreshMetadata();
    QCOMPARE(transport.statusRequestCount, 0);
    // The visible navigation's periodic refresh must bypass the reopen TTL.
    QVERIFY(QMetaObject::invokeMethod(&resolver, "refreshLiveStatus"));
    QTRY_COMPARE(transport.statusRequestCount, 1);
    QVERIFY(QMetaObject::invokeMethod(&resolver, "refreshLiveStatus"));
    QCOMPARE(transport.statusRequestCount, 1);
    QCOMPARE(transport.requestCount, 0);
    QVERIFY(resolver.handleFailure(transport.lastStatusRequestId, "unavailable"));
    QCOMPARE(resolver.liveStateFor("member"), QStringLiteral("unknown"));
    QCOMPARE(resolver.roomIdFor("member"), QStringLiteral("123"));
}

void GuildRoomResolverTest::preservesCachedAvatarWhenSearchStatusIsUnknown()
{
    FakeSearchTransport transport;
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({{"member", "Anchor", {}, "123"}});
    GuildRoomCacheEntry cached;
    cached.memberId = "member";
    cached.roomId = "123";
    cached.verifiedAtMs = cached.metadataCheckedAtMs = 1;
    cached.avatarUrl = QUrl("https://example.invalid/avatar.png");
    resolver.setCache({cached});
    resolver.start();
    QTRY_COMPARE(transport.statusRequestCount, 1);
    ServiceResponse status;
    status.requestId = transport.lastStatusRequestId;
    status.ok = true;
    status.status = true;
    status.isLive = true;
    QVERIFY(resolver.handleResponse(status));
    resolver.refreshMetadata(true);
    QTRY_COMPARE(transport.requestCount, 1);
    ServiceResponse response;
    response.requestId = transport.lastRequestId;
    response.ok = true;
    response.search = true;
    RoomSearchResult hint;
    hint.roomId = "123";
    hint.anchorName = "Anchor";
    hint.statusKnown = false;
    response.results = {hint};
    QVERIFY(resolver.handleResponse(response));
    QCOMPARE(resolver.avatarUrlFor("member"), cached.avatarUrl.toString());
    QCOMPARE(resolver.cache().first().avatarUrl, cached.avatarUrl);
    QCOMPARE(resolver.liveStateFor("member"), QStringLiteral("unknown"));
}

QTEST_GUILESS_MAIN(GuildRoomResolverTest)

#include "guild_room_resolver_test.moc"
