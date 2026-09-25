#include <QtTest/QtTest>

#include "workspace/guild_roster.h"

class GuildRosterTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsBundledRosterWithoutRoleLabels();
    void stripsRoleSuffixOnlyForSearch();
    void rejectsInvalidRoomIdsFromResource();
    void rejectsMalformedRootAndUnsupportedVersion();
    void rejectsInvalidMemberShapesAndIds();
    void rejectsMalformedRoomIdsButAcceptsEmptyRoomId();
};

void GuildRosterTest::loadsBundledRosterWithoutRoleLabels()
{
    const QVector<GuildMember> members = GuildRoster::bundled();
    QCOMPARE(members.size(), 49);
    for (const GuildMember &member : members) {
        QVERIFY(!member.id.isEmpty());
        QVERIFY(!member.anchorName.isEmpty());
        QVERIFY(!member.anchorName.contains(QStringLiteral("-团长")));
        QVERIFY(!member.anchorName.contains(QStringLiteral("-队长")));
        QVERIFY(!member.anchorName.contains(QStringLiteral("-队员")));
        QVERIFY(!member.anchorName.endsWith(QStringLiteral("-OB")));
    }
}

void GuildRosterTest::stripsRoleSuffixOnlyForSearch()
{
    QCOMPARE(GuildRoster::searchName(QStringLiteral("主播阿郎-团长")),
             QStringLiteral("主播阿郎"));
    QCOMPARE(GuildRoster::searchName(QStringLiteral("Zy梓洋-OB")),
             QStringLiteral("Zy梓洋"));
    QCOMPARE(GuildRoster::searchName(QStringLiteral("寅子")),
             QStringLiteral("寅子"));
}

void GuildRosterTest::rejectsInvalidRoomIdsFromResource()
{
    const GuildMember *yinzi = GuildRoster::findByName(QStringLiteral("寅子"));
    QVERIFY(yinzi != nullptr);
    QCOMPARE(yinzi->roomId, QStringLiteral("71415"));

    const GuildMember *xiaoCousin = GuildRoster::findByName(QStringLiteral("尐表哥"));
    QVERIFY(xiaoCousin != nullptr);
    QCOMPARE(xiaoCousin->roomId, QStringLiteral("217331"));

    const GuildMember *eleven = GuildRoster::findByName(QStringLiteral("十一or"));
    QVERIFY(eleven != nullptr);
    QCOMPARE(eleven->roomId, QStringLiteral("12858969"));

    const GuildMember *misty = GuildRoster::findByName(QStringLiteral("雾蒙蒙y"));
    QVERIFY(misty != nullptr);
    QCOMPARE(misty->roomId, QStringLiteral("12874029"));

    const GuildMember *xiaoliu = GuildRoster::findByName(QStringLiteral("小六HQ"));
    QVERIFY(xiaoliu != nullptr);
    QCOMPARE(xiaoliu->roomId, QStringLiteral("12900462"));
}

void GuildRosterTest::rejectsMalformedRootAndUnsupportedVersion()
{
    QVERIFY(GuildRoster::parse(QByteArrayLiteral("{")).isEmpty());
    QVERIFY(GuildRoster::parse(QByteArrayLiteral("[]")).isEmpty());
    QVERIFY(GuildRoster::parse(
                QByteArrayLiteral(R"({"version":2,"members":[]})"))
                .isEmpty());
    QVERIFY(GuildRoster::parse(
                QByteArrayLiteral(R"({"version":1})"))
                .isEmpty());
    QVERIFY(GuildRoster::parse(
                QByteArrayLiteral(R"({"version":1,"members":{}})"))
                .isEmpty());
}

void GuildRosterTest::rejectsInvalidMemberShapesAndIds()
{
    const QVector<GuildMember> members = GuildRoster::parse(
        QByteArrayLiteral(R"({
            "version": 1,
            "members": [
                {},
                { "id": "", "name": "空 ID" },
                { "id": "   ", "name": "空白 ID" },
                { "id": "hamster-001", "name": "有效成员" },
                { "id": "hamster-001", "name": "重复 ID" },
                { "id": "hamster-002", "name": "   " },
                { "id": "hamster-003", "name": "另一位有效成员", "roomId": "12345" }
            ]
        })"));

    QCOMPARE(members.size(), 2);
    QCOMPARE(members.at(0).id, QStringLiteral("hamster-001"));
    QCOMPARE(members.at(1).id, QStringLiteral("hamster-003"));
}

void GuildRosterTest::rejectsMalformedRoomIdsButAcceptsEmptyRoomId()
{
    const QVector<GuildMember> members = GuildRoster::parse(
        QByteArrayLiteral(R"({
            "version": 1,
            "members": [
                { "id": "hamster-001", "name": "空房间号", "roomId": "" },
                { "id": "hamster-002", "name": "字母房间号", "roomId": "abc" },
                { "id": "hamster-003", "name": "负数房间号", "roomId": "-1" },
                { "id": "hamster-004", "name": "超长房间号", "roomId": "123456789012345678901" }
            ]
        })"));

    QCOMPARE(members.size(), 1);
    QCOMPARE(members.first().id, QStringLiteral("hamster-001"));
    QVERIFY(members.first().roomId.isEmpty());
}

QTEST_GUILESS_MAIN(GuildRosterTest)

#include "guild_roster_test.moc"
