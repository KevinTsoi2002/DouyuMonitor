#include <QtTest/QtTest>

#include "workspace/guild_roster.h"

class GuildRosterTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsBundledRosterWithoutRoleLabels();
    void stripsRoleSuffixOnlyForSearch();
    void rejectsInvalidRoomIdsFromResource();
};

void GuildRosterTest::loadsBundledRosterWithoutRoleLabels()
{
    const QVector<GuildMember> members = GuildRoster::bundled();
    QCOMPARE(members.size(), 47);
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
}

QTEST_GUILESS_MAIN(GuildRosterTest)

#include "guild_roster_test.moc"
