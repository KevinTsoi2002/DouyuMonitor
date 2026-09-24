#include <QtTest/QtTest>

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

    void cancel(quint64 requestId) override
    {
        cancelledRequestId = requestId;
    }

    int requestCount = 0;
    QString lastQuery;
    quint64 lastRequestId = 0;
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
};

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
        1,
    }});
    resolver.start();

    QCOMPARE(transport.requestCount, 1);
    QCOMPARE(transport.lastQuery, QStringLiteral("午夜抹抹茶"));
    QCOMPARE(resolver.roomIdFor(QStringLiteral("hamster-001")),
             QStringLiteral("71415"));
    QCOMPARE(resolver.roomIdFor(QStringLiteral("hamster-002")),
             QStringLiteral("84452"));
}

QTEST_GUILESS_MAIN(GuildRoomResolverTest)

#include "guild_room_resolver_test.moc"
