#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtTest/QtTest>

#include "app/maozi_rank_client.h"

namespace {

QByteArray snapshotBody()
{
    QJsonObject highGrade{
        {QStringLiteral("letter"), QStringLiteral("S")},
        {QStringLiteral("min"), 15},
        {QStringLiteral("color"), QStringLiteral("#ffc93c")},
    };
    QJsonObject lowGrade{
        {QStringLiteral("letter"), QStringLiteral("A")},
        {QStringLiteral("min"), 10},
        {QStringLiteral("color"), QStringLiteral("#a78bfa")},
    };
    QJsonObject criteriaA{{QStringLiteral("id"), QStringLiteral("c0")},
                          {QStringLiteral("name"), QStringLiteral("力量")}};
    QJsonObject criteriaB{{QStringLiteral("id"), QStringLiteral("c1")},
                          {QStringLiteral("name"), QStringLiteral("财力")}};
    QJsonObject config{
        {QStringLiteral("grades"), QJsonArray{highGrade, lowGrade}},
        {QStringLiteral("criteria"), QJsonArray{criteriaA, criteriaB}},
    };
    QJsonObject firstHost{
        {QStringLiteral("id"), QStringLiteral("host-1")},
        {QStringLiteral("name"), QStringLiteral("寅子")},
        {QStringLiteral("guild"), QStringLiteral("仓鼠特工")},
        {QStringLiteral("douyu_id"), QStringLiteral("71415")},
        {QStringLiteral("note"), QStringLiteral("猴王")},
        {QStringLiteral("poster"), QStringLiteral("cloud://env-bucket/posters/host-1.webp")},
        {QStringLiteral("team"), 0},
    };
    QJsonObject secondHost{
        {QStringLiteral("id"), QStringLiteral("host-2")},
        {QStringLiteral("name"), QStringLiteral("雾萌萌y")},
        {QStringLiteral("guild"), QStringLiteral("仓鼠特工")},
        {QStringLiteral("douyu_id"), QStringLiteral("84452")},
        {QStringLiteral("note"), QString()},
        {QStringLiteral("poster"), QString()},
        {QStringLiteral("team"), 1},
    };
    QJsonObject leaderHost{
        {QStringLiteral("id"), QStringLiteral("host-leader")},
        {QStringLiteral("name"), QStringLiteral("主播阿郎")},
        {QStringLiteral("guild"), QStringLiteral("仓鼠特工")},
        {QStringLiteral("douyu_id"), QStringLiteral("320155")},
        {QStringLiteral("poster"), QString()},
        {QStringLiteral("team"), QJsonValue::Null},
    };
    QJsonObject captainHost{
        {QStringLiteral("id"), QStringLiteral("host-captain")},
        {QStringLiteral("name"), QStringLiteral("尐表哥")},
        {QStringLiteral("guild"), QStringLiteral("仓鼠特工")},
        {QStringLiteral("douyu_id"), QStringLiteral("217331")},
        {QStringLiteral("poster"), QString()},
        {QStringLiteral("team"), QJsonValue::Null},
    };
    QJsonObject firstDims{
        {QStringLiteral("c0"), QJsonObject{{QStringLiteral("sum"), 20}, {QStringLiteral("cnt"), 1}}},
        {QStringLiteral("c1"), QJsonObject{{QStringLiteral("sum"), 20}, {QStringLiteral("cnt"), 1}}},
    };
    QJsonObject secondDims{
        {QStringLiteral("c0"), QJsonObject{{QStringLiteral("sum"), 12}, {QStringLiteral("cnt"), 1}}},
        {QStringLiteral("c1"), QJsonObject{{QStringLiteral("sum"), 10}, {QStringLiteral("cnt"), 1}}},
    };
    QJsonObject firstAgg{{QStringLiteral("host_id"), QStringLiteral("host-1")},
                         {QStringLiteral("dims"), firstDims},
                         {QStringLiteral("voters"), 12}};
    QJsonObject secondAgg{{QStringLiteral("host_id"), QStringLiteral("host-2")},
                          {QStringLiteral("dims"), secondDims},
                          {QStringLiteral("voters"), 8}};
    QJsonObject leaderAgg{{QStringLiteral("host_id"), QStringLiteral("host-leader")},
                          {QStringLiteral("dims"), QJsonObject{}},
                          {QStringLiteral("voters"), 3}};
    QJsonObject captainAgg{{QStringLiteral("host_id"), QStringLiteral("host-captain")},
                           {QStringLiteral("dims"), QJsonObject{}},
                           {QStringLiteral("voters"), 4}};
    QJsonObject agg{
        {QStringLiteral("hosts"), QJsonArray{secondAgg, firstAgg, leaderAgg, captainAgg}},
        {QStringLiteral("updated"), QStringLiteral("2026-09-29T20:30:00+08:00")},
        {QStringLiteral("voters"), 20},
    };
    QJsonObject liveFirst{{QStringLiteral("room_id"), QStringLiteral("71415")},
                          {QStringLiteral("status"), QStringLiteral("live")}};
    QJsonObject liveSecond{{QStringLiteral("room_id"), QStringLiteral("84452")},
                           {QStringLiteral("status"), QStringLiteral("offline")}};
    QJsonObject placementDay{
        {QStringLiteral("ok"), true},
        {QStringLiteral("day"), QStringLiteral("2026-09-28")},
        {QStringLiteral("events"),
         QJsonArray{
             QJsonObject{{QStringLiteral("slot"), 1},
                         {QStringLiteral("name"), QStringLiteral("测试")},
                         {QStringLiteral("rank_mode"), QStringLiteral("sum")},
                         {QStringLiteral("dir"), QStringLiteral("asc")}},
         }},
        {QStringLiteral("stages"),
         QJsonArray{
             QJsonObject{{QStringLiteral("slot"), 1},
                         {QStringLiteral("stage"), 1},
                         {QStringLiteral("metric"), QStringLiteral("time")},
                         {QStringLiteral("dir"), QStringLiteral("asc")}},
         }},
        {QStringLiteral("results"),
         QJsonArray{
             QJsonObject{{QStringLiteral("slot"), 1},
                         {QStringLiteral("stage"), 1},
                         {QStringLiteral("value"), 10},
                         {QStringLiteral("host_id"), QStringLiteral("host-1")},
                         {QStringLiteral("invalid"), false}},
             QJsonObject{{QStringLiteral("slot"), 1},
                         {QStringLiteral("stage"), 1},
                         {QStringLiteral("value"), 20},
                         {QStringLiteral("host_id"), QStringLiteral("host-2")},
                         {QStringLiteral("invalid"), false}},
             QJsonObject{{QStringLiteral("slot"), 1},
                         {QStringLiteral("stage"), 1},
                         {QStringLiteral("value"), 5},
                         {QStringLiteral("host_id"), QStringLiteral("host-captain")},
                         {QStringLiteral("invalid"), false}},
             QJsonObject{{QStringLiteral("slot"), 1},
                         {QStringLiteral("stage"), 1},
                         {QStringLiteral("value"), 1},
                         {QStringLiteral("host_id"), QStringLiteral("host-leader")},
                         {QStringLiteral("invalid"), false}},
         }},
    };
    QJsonObject placement{
        {QStringLiteral("2026-09-28"), placementDay},
    };
    QJsonObject playValue{
        {QStringLiteral("uid"), QStringLiteral("100001")},
        {QStringLiteral("nickname"), QStringLiteral("寅子")},
        {QStringLiteral("points"), 6.4},
        {QStringLiteral("bombed"), 1},
        {QStringLiteral("updated_at"), QStringLiteral("2026-09-30T12:26:01+08:00")},
    };
    QJsonObject result{
        {QStringLiteral("v"), QStringLiteral("snapshot-1")},
        {QStringLiteral("hosts"), QJsonArray{firstHost, secondHost, leaderHost, captainHost}},
        {QStringLiteral("agg"), agg},
        {QStringLiteral("live"), QJsonArray{liveFirst, liveSecond}},
        {QStringLiteral("config"), config},
        {QStringLiteral("placement"), placement},
        {QStringLiteral("playvalue"), QJsonArray{playValue}},
    };
    return QJsonDocument(result).toJson(QJsonDocument::Compact);
}

class CloudBaseFixture final : public QObject {
    Q_OBJECT

public:
    explicit CloudBaseFixture(QObject *parent = nullptr)
        : QObject(parent)
    {
        connect(&server_, &QTcpServer::newConnection, this, [this] {
            auto *socket = server_.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
                pendingRequests_.insert(socket, pendingRequests_.value(socket) + socket->readAll());
                if (responded_.value(socket, false)) return;
                responded_.insert(socket, true);
                QTimer::singleShot(delayMs, socket, [this, socket] { respond(socket); });
            });
        });
    }

    bool listen()
    {
        return server_.listen(QHostAddress::LocalHost);
    }

    QUrl authUrl() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/auth/v1/signin/anonymously")
                        .arg(server_.serverPort()));
    }

    QUrl snapshotUrl() const
    {
        return QUrl(QStringLiteral("http://127.0.0.1:%1/v1/rdb/rest/rpc/fn_rank_snapshot")
                        .arg(server_.serverPort()));
    }

    int statusCode = 200;
    QByteArray authBody = QByteArrayLiteral(
        R"({"token_type":"Bearer","access_token":"test-access-token","refresh_token":"test-refresh-token","expires_in":7200,"scope":"anonymous","sub":"anonymous-user"})");
    QByteArray snapshot = snapshotBody();
    QByteArray versionBody = QByteArrayLiteral(R"({"v":"snapshot-1"})");
    int delayMs = 0;
    int snapshotRequests = 0;
    int versionRequests = 0;
    QList<QByteArray> authorizationHeaders;

private:
    void respond(QTcpSocket *socket)
    {
        const QByteArray request = pendingRequests_.take(socket);
        const int authorizationIndex = request.toLower().indexOf(QByteArrayLiteral("authorization:"));
        if (authorizationIndex >= 0) {
            const int start = authorizationIndex + QByteArrayLiteral("authorization:").size();
            const int end = request.indexOf("\r\n", start);
            authorizationHeaders.append(request.mid(start, end - start).trimmed());
        }

        const bool loginRequest = request.startsWith(QByteArrayLiteral("POST /auth/v1/signin/anonymously"));
        const bool versionRequest = request.startsWith(QByteArrayLiteral("POST /v1/rdb/rest/rpc/fn_rank_check"));
        if (!loginRequest && versionRequest) ++versionRequests;
        if (!loginRequest && !versionRequest) ++snapshotRequests;
        const QByteArray body = loginRequest ? authBody : (versionRequest ? versionBody : snapshot);
        const QByteArray reason = statusCode == 200 ? QByteArrayLiteral("OK")
                                                    : QByteArrayLiteral("Error");
        const QByteArray response = QByteArrayLiteral("HTTP/1.1 ") + QByteArray::number(statusCode)
            + ' ' + reason + QByteArrayLiteral("\r\nContent-Type: application/json\r\nContent-Length: ")
            + QByteArray::number(body.size()) + QByteArrayLiteral("\r\nConnection: close\r\n\r\n")
            + body;
        socket->write(response);
        socket->disconnectFromHost();
    }

    QTcpServer server_;
    QHash<QTcpSocket *, QByteArray> pendingRequests_;
    QHash<QTcpSocket *, bool> responded_;
};

} // namespace

class MaoziRankClientTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsAndSortsLeaderboardData();
    void exposesRolesPlacementPlayValueAndLookupIndexes();
    void exposesPlacementScoreFormula();
    void skipsSnapshotWhenVersionIsUnchanged();
    void reportsHttpErrors();
    void validatesCurrentTeamIdentifiers();
    void completesTimeoutOnceAndRejectsMalformedTeams();
    void fillsOnlyMissingCaptainTeams();
    void editsVersionedLocalMappingAndReclassifiesCachedSnapshot();
};

void MaoziRankClientTest::editsVersionedLocalMappingAndReclassifiesCachedSnapshot()
{
    CloudBaseFixture fixture;
    QVERIFY(fixture.listen());
    MaoziRankClient client(fixture.authUrl(), fixture.snapshotUrl(), 1000);
    client.refresh();
    QTRY_VERIFY(!client.loading());
    const QByteArray mapping = R"({"version":1,"event":"fixture","members":[{"roomId":"217331","name":"尐表哥","role":"member","team":null},{"roomId":"71415","name":"寅子","role":"captain","team":3}]})";
    QString error;
    QVERIFY(QMetaObject::invokeMethod(&client, "setEventMapping", Q_RETURN_ARG(QString, error),
                                     Q_ARG(QByteArray, mapping)));
    QVERIFY(error.isEmpty());
    QCOMPARE(client.entryForRoomId("217331").value("role").toString(), QStringLiteral("member"));
    QCOMPARE(client.entryForRoomId("71415").value("role").toString(), QStringLiteral("captain"));
    // The remote snapshot's explicit team wins over a local fallback.
    QCOMPARE(client.entryForRoomId("71415").value("teamId").toString(), QStringLiteral("0"));
    const auto unchanged = client.entries();
    const QByteArray invalid = R"({"version":2,"event":"fixture","members":[]})";
    QVERIFY(QMetaObject::invokeMethod(&client, "setEventMapping", Q_RETURN_ARG(QString, error),
                                     Q_ARG(QByteArray, invalid)));
    QVERIFY(!error.isEmpty());
    QCOMPARE(client.entries(), unchanged);
    const QByteArray alias = R"({"version":1,"event":"fixture","members":[{"roomId":"999","name":"尐表哥-队长","role":"leader","team":null}]})";
    QVERIFY(client.setEventMapping(alias).isEmpty());
    QCOMPARE(client.entryForRoomId("217331").value("role").toString(), QStringLiteral("leader"));
    for (const auto &bad : {QByteArray(R"({"version":1,"event":"fixture","members":[{"roomId":"1","name":"A","role":"admin"}]})"),
                           QByteArray(R"({"version":1,"event":"fixture","members":[{"roomId":"1","name":"A","role":"captain","team":0.5}]})"),
                           QByteArray(R"({"version":1,"event":"fixture","members":[{"roomId":"1","name":"A","role":"member"},{"roomId":"1","name":"B","role":"member"}]})")})
        QVERIFY(!client.setEventMapping(bad).isEmpty());
}

void MaoziRankClientTest::fillsOnlyMissingCaptainTeams()
{
    CloudBaseFixture fixture;
    QVERIFY(fixture.listen());
    const QStringList rooms{"217331", "7204164", "80432", "6151194",
                            "731252", "11222", "2140934", "2632018"};
    const QList<int> teamIds{3, 3, 1, 1, 2, 2, 0, 0};
    const QStringList teamNames{QStringLiteral("蓝队"), QStringLiteral("蓝队"),
                                QStringLiteral("黑队"), QStringLiteral("黑队"),
                                QStringLiteral("紫队"), QStringLiteral("紫队"),
                                QStringLiteral("红队"), QStringLiteral("红队")};
    QJsonArray hosts;
    for (int i = 0; i < rooms.size(); ++i) {
        QJsonObject host{{"id", rooms.at(i)}, {"name", rooms.at(i)},
                         {"douyu_id", rooms.at(i)}};
        if (i % 2 == 0) host.insert("team", QJsonValue::Null);
        hosts.append(host);
    }
    hosts.append(QJsonObject{{"id", "leader"}, {"name", "leader"}, {"douyu_id", "320155"}});
    hosts.append(QJsonObject{{"id", "member"}, {"name", "member"}, {"douyu_id", "71415"}});
    fixture.snapshot = QJsonDocument(QJsonObject{{"hosts", hosts}}).toJson();
    MaoziRankClient client(fixture.authUrl(), fixture.snapshotUrl(), 1000);
    client.refresh();
    QTRY_VERIFY(!client.loading());
    for (int i = 0; i < rooms.size(); ++i) {
        const auto entry = client.entryForRoomId(rooms.at(i));
        QCOMPARE(entry.value("role").toString(), QStringLiteral("captain"));
        QCOMPARE(entry.value("teamId").toString(), QString::number(teamIds.at(i)));
        QCOMPARE(entry.value("teamName").toString(), teamNames.at(i));
        QVERIFY(entry.value("teamValid").toBool());
    }
    for (const auto &room : {QStringLiteral("320155"), QStringLiteral("71415")})
        QVERIFY(client.entryForRoomId(room).value("teamId").toString().isEmpty());

    auto moved = hosts[0].toObject();
    moved.insert("team", 0);
    hosts[0] = moved;
    auto malformed = hosts[1].toObject();
    malformed.insert("team", QStringLiteral("3"));
    hosts[1] = malformed;
    fixture.snapshot = QJsonDocument(QJsonObject{{"hosts", hosts}}).toJson();
    client.refresh();
    QTRY_VERIFY(!client.loading());
    QCOMPARE(client.entryForRoomId("217331").value("teamId").toString(), QStringLiteral("0"));
    QCOMPARE(client.entryForRoomId("217331").value("teamName").toString(), QStringLiteral("红队"));
    QVERIFY(!client.entryForRoomId("7204164").value("teamValid").toBool());
    QVERIFY(client.entryForRoomId("7204164").value("teamId").toString().isEmpty());
}

void MaoziRankClientTest::validatesCurrentTeamIdentifiers()
{
    CloudBaseFixture fixture;
    QVERIFY(fixture.listen());
    QJsonObject root = QJsonDocument::fromJson(snapshotBody()).object();
    QJsonArray hosts;
    const QStringList expected{QStringLiteral("红队"), QStringLiteral("黑队"),
                               QStringLiteral("紫队"), QStringLiteral("蓝队")};
    for (int index = 0; index < 4; ++index) {
        hosts.append(QJsonObject{{QStringLiteral("id"), QString::number(index)},
                                 {QStringLiteral("name"), QString::number(index)},
                                 {QStringLiteral("team"), index}});
    }
    hosts.append(QJsonObject{{QStringLiteral("id"), QStringLiteral("unknown")},
                             {QStringLiteral("name"), QStringLiteral("unknown")},
                             {QStringLiteral("team"), 7}});
    root.insert(QStringLiteral("hosts"), hosts);
    fixture.snapshot = QJsonDocument(root).toJson();
    MaoziRankClient client(fixture.authUrl(), fixture.snapshotUrl(), 1000);
    QSignalSpy finished(&client, SIGNAL(snapshotRefreshFinished(bool)));
    client.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(!client.loading(), 3000);
    for (int index = 0; index < 4; ++index) {
        const auto entry = client.entryForName(QString::number(index));
        QCOMPARE(entry.value(QStringLiteral("teamName")).toString(), expected.at(index));
        QCOMPARE(entry.value(QStringLiteral("teamId")).toString(), QString::number(index));
        QVERIFY(entry.value(QStringLiteral("teamValid")).toBool());
    }
    QVERIFY(!client.entryForName(QStringLiteral("unknown"))
                 .value(QStringLiteral("teamValid")).toBool());
    QCOMPARE(finished.size(), 1);
    QCOMPARE(finished.at(0).at(0).toBool(), true);
    fixture.statusCode = 500;
    client.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(finished.size(), 2, 3000);
    QCOMPARE(finished.at(1).at(0).toBool(), false);
}

void MaoziRankClientTest::completesTimeoutOnceAndRejectsMalformedTeams()
{
    CloudBaseFixture fixture;
    QVERIFY(fixture.listen());
    auto root = QJsonDocument::fromJson(snapshotBody()).object();
    root.insert("hosts", QJsonArray{
        QJsonObject{{"id", "null"}, {"name", "null"}, {"team", QJsonValue::Null}},
        QJsonObject{{"id", "missing"}, {"name", "missing"}},
        QJsonObject{{"id", "string"}, {"name", "string"}, {"team", "0"}},
        QJsonObject{{"id", "fraction"}, {"name", "fraction"}, {"team", 0.5}},
        QJsonObject{{"id", "boolean"}, {"name", "boolean"}, {"team", true}}});
    fixture.snapshot = QJsonDocument(root).toJson();
    MaoziRankClient client(fixture.authUrl(), fixture.snapshotUrl(), 100);
    QSignalSpy finished(&client, &MaoziRankClient::snapshotRefreshFinished);
    client.refresh();
    QTRY_COMPARE(finished.size(), 1);
    for (const auto &name : {QStringLiteral("null"), QStringLiteral("missing")}) {
        const auto entry = client.entryForName(name);
        QVERIFY(entry.value("teamValid").toBool());
        QVERIFY(entry.value("teamId").toString().isEmpty());
    }
    for (const auto &name : {QStringLiteral("string"), QStringLiteral("fraction"),
                             QStringLiteral("boolean")})
        QVERIFY(!client.entryForName(name).value("teamValid").toBool());
    client.checkForChanges();
    QTRY_VERIFY(!client.syncPending());
    QCOMPARE(finished.size(), 1);
    fixture.delayMs = 300;
    client.refresh();
    QTRY_COMPARE(finished.size(), 2);
    QCOMPARE(finished.last().first().toBool(), false);
    QTest::qWait(400);
    QCOMPARE(finished.size(), 2);
}

void MaoziRankClientTest::loadsAndSortsLeaderboardData()
{
    CloudBaseFixture fixture;
    QVERIFY(fixture.listen());
    MaoziRankClient client(fixture.authUrl(), fixture.snapshotUrl(), 1000);
    QSignalSpy readySpy(&client, &MaoziRankClient::entriesChanged);

    client.refresh();

    QTRY_COMPARE_WITH_TIMEOUT(client.loading(), false, 3000);
    QCOMPARE(client.errorMessage(), QString());
    QCOMPARE(client.entries().size(), 4);
    const QVariantMap first = client.entryForName(QStringLiteral("寅子"));
    QCOMPARE(first.value(QStringLiteral("name")).toString(), QStringLiteral("寅子"));
    QCOMPARE(first.value(QStringLiteral("rank")).toInt(), 1);
    QCOMPARE(first.value(QStringLiteral("score")).toDouble(), 20.0);
    QCOMPARE(first.value(QStringLiteral("grade")).toString(), QStringLiteral("S"));
    QCOMPARE(first.value(QStringLiteral("live")).toBool(), true);
    QCOMPARE(first.value(QStringLiteral("teamName")).toString(), QStringLiteral("红队"));
    QCOMPARE(first.value(QStringLiteral("posterUrl")).toString(),
             QStringLiteral("https://6479-dy656750-d6g192t6k4a51aa36-1309340272.tcb.qcloud.la/posters/host-1.webp?imageMogr2/thumbnail/64x64/format/jpg"));
    QCOMPARE(client.totalVoters(), 20);
    QVERIFY(readySpy.count() >= 1);
    QCOMPARE(fixture.authorizationHeaders.size(), 1);
    QCOMPARE(fixture.authorizationHeaders.constFirst(),
             QByteArrayLiteral("Bearer test-access-token"));
}

void MaoziRankClientTest::exposesRolesPlacementPlayValueAndLookupIndexes()
{
    CloudBaseFixture fixture;
    QVERIFY(fixture.listen());
    MaoziRankClient client(fixture.authUrl(), fixture.snapshotUrl(), 1000);

    client.refresh();

    QTRY_COMPARE_WITH_TIMEOUT(client.loading(), false, 3000);
    QCOMPARE(client.errorMessage(), QString());

    const QVariantMap leader = client.entryForRoomId(QStringLiteral("320155"));
    QCOMPARE(leader.value(QStringLiteral("role")).toString(), QStringLiteral("leader"));
    const QVariantMap captain = client.entryForRoomId(QStringLiteral("217331"));
    QCOMPARE(captain.value(QStringLiteral("role")).toString(), QStringLiteral("captain"));
    QCOMPARE(captain.value(QStringLiteral("teamName")).toString(), QStringLiteral("蓝队"));
    QCOMPARE(captain.value(QStringLiteral("teamId")).toString(), QStringLiteral("3"));
    QVERIFY(captain.value(QStringLiteral("teamValid")).toBool());
    const QVariantMap member = client.entryForName(QStringLiteral("寅子"));
    QCOMPARE(member.value(QStringLiteral("role")).toString(), QStringLiteral("member"));
    QCOMPARE(member.value(QStringLiteral("playValue")).toDouble(), 6.4);
    QCOMPARE(member.value(QStringLiteral("playValueBombed")).toInt(), 1);
    QVERIFY(qAbs(member.value(QStringLiteral("placementAverage")).toDouble()
                 - 70.8295291826) < 0.0001);
    QCOMPARE(member.value(QStringLiteral("placementScoredSessions")).toInt(), 1);
    QVERIFY(!client.entryForName(QStringLiteral("雾蒙蒙y")).isEmpty());
    QCOMPARE(client.placementEntries().size(), 4);
    QCOMPARE(client.placementColumns().size(), 1);
    const QVariantMap column = client.placementColumns().value(
        QStringLiteral("2026-09-28:1")).toMap();
    QCOMPARE(column.value(QStringLiteral("label")).toString(), QStringLiteral("28午"));
    QCOMPARE(column.value(QStringLiteral("title")).toString(), QStringLiteral("测试"));
    const QVariantMap placementFirst = client.placementEntries().constFirst().toMap();
    QCOMPARE(placementFirst.value(QStringLiteral("placementRank")).toInt(), 1);
    QCOMPARE(placementFirst.value(QStringLiteral("name")).toString(),
             QStringLiteral("主播阿郎"));
    QVERIFY(placementFirst.value(QStringLiteral("placementSessions")).toMap()
                .contains(QStringLiteral("2026-09-28:1")));
    QCOMPARE(client.playValueEntries().size(), 1);
    QCOMPARE(client.playValueEntries().constFirst().toMap()
                 .value(QStringLiteral("points")).toDouble(),
             6.4);
}

void MaoziRankClientTest::exposesPlacementScoreFormula()
{
    QCOMPARE(MaoziRankClient::placementScore(3, 1), 94.0);
    QVERIFY(qAbs(MaoziRankClient::placementScore(3, 2) - 77.6746) < 0.01);
    QCOMPARE(MaoziRankClient::placementScore(3, 3), 46.0);
    QCOMPARE(MaoziRankClient::placementScore(1, 1), 94.0);
}

void MaoziRankClientTest::skipsSnapshotWhenVersionIsUnchanged()
{
    CloudBaseFixture fixture;
    QVERIFY(fixture.listen());
    MaoziRankClient client(fixture.authUrl(), fixture.snapshotUrl(), 1000);

    client.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(client.loading(), false, 3000);
    QCOMPARE(fixture.snapshotRequests, 1);

    client.checkForChanges();
    QTRY_COMPARE_WITH_TIMEOUT(client.syncPending(), false, 3000);
    QCOMPARE(fixture.versionRequests, 1);
    QCOMPARE(fixture.snapshotRequests, 1);
}

void MaoziRankClientTest::reportsHttpErrors()
{
    CloudBaseFixture fixture;
    QVERIFY(fixture.listen());
    MaoziRankClient client(fixture.authUrl(), fixture.snapshotUrl(), 1000);

    client.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(client.loading(), false, 3000);
    QCOMPARE(client.entries().size(), 4);
    QVERIFY(!client.entryForName(QStringLiteral("寅子")).isEmpty());

    fixture.statusCode = 500;
    client.checkForChanges();

    QTRY_COMPARE_WITH_TIMEOUT(client.syncPending(), false, 3000);
    QCOMPARE(client.lastSyncError(), QStringLiteral("数据可能已过期"));
    QCOMPARE(client.entries().size(), 4);
    QVERIFY(!client.entryForName(QStringLiteral("寅子")).isEmpty());
}

QTEST_GUILESS_MAIN(MaoziRankClientTest)

#include "maozi_rank_client_test.moc"
