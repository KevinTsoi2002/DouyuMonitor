#include <QtTest/QtTest>
#include "workspace/maozi_team_import.h"

namespace {
QVector<GuildMember> roster()
{
    return {{QStringLiteral("mya"), QStringLiteral("美伢Mya-队员"), {}, QStringLiteral("12870978"), "M"},
            {QStringLiteral("mist"), QStringLiteral("雾蒙蒙y"), {}, {}, "W"},
            {QStringLiteral("captain"), QStringLiteral("尐表哥"), {}, QStringLiteral("217331"), "S"},
            {QStringLiteral("manual"), QStringLiteral("手动主播"), {}, {}, "S"},
            {QStringLiteral("leader"), QStringLiteral("主播阿郎"), {}, QStringLiteral("320155"), "Z"}};
}
QVariantMap host(const QString &id, const QString &name, const QString &room,
                 const QString &team = QStringLiteral("红队"))
{
    return {{QStringLiteral("id"), id}, {QStringLiteral("name"), name},
            {QStringLiteral("roomId"), room}, {QStringLiteral("teamName"), team},
            {QStringLiteral("teamId"), team}, {QStringLiteral("teamValid"), true},
            {QStringLiteral("role"), QStringLiteral("member")}};
}
}

class MaoziTeamImportTest final : public QObject {
    Q_OBJECT
private slots:
    void importsPreservesAndRepeats();
    void handlesIdentityConflicts();
    void rejectsInvalidTeamsAndCapacity();
    void excludesLeadersAndKeepsUnassigned();
    void matchesVerifiedCacheAndRejectsDuplicateHosts();
    void protectsAllCandidatesAcrossConflictingRows();
};

void MaoziTeamImportTest::importsPreservesAndRepeats()
{
    const QVector<NativeTeam> current{
        {"red", QStringLiteral("红队"), {"manual"}},
        {"old", QStringLiteral("自建队伍"), {"mya"}}};
    const QVariantList entries{host("h1", QStringLiteral("美伢Mya"), "77111"),
                               host("h2", QStringLiteral("雾萌萌y"), "12874029"),
                               host("h3", QStringLiteral("尐表哥"), "217331", QStringLiteral("蓝队"))};
    const auto result = MaoziTeamImport::build(current, roster(), {}, entries);
    QVERIFY(result.error.isEmpty());
    QCOMPARE(result.teams.size(), 3);
    QCOMPARE(result.teams.at(0).id, QStringLiteral("red"));
    QVERIFY(result.teams.at(0).memberIds.contains("manual"));
    QVERIFY(result.teams.at(0).memberIds.contains("mya"));
    QVERIFY(result.teams.at(0).memberIds.contains("mist"));
    QVERIFY(result.teams.at(1).memberIds.isEmpty());
    QCOMPARE(result.changedMembers, 3);
    QCOMPARE(result.createdTeams, 1);
    QCOMPARE(result.previewTeams.size(), 2);
    QCOMPARE(result.previewTeams.at(0).toMap().value("retainedCount").toInt(), 1);
    const auto repeated = MaoziTeamImport::build(result.teams, roster(), {}, entries);
    QCOMPARE(repeated.teams, result.teams);
    QCOMPARE(repeated.changedMembers, 0);
    QCOMPARE(repeated.createdTeams, 0);
    QCOMPARE(repeated.unchangedMembers, 3);
}

void MaoziTeamImportTest::handlesIdentityConflicts()
{
    const QVector<NativeTeam> current{{"old", QStringLiteral("手动"), {"mya", "captain"}}};
    auto result = MaoziTeamImport::build(current, roster(), {}, {
        host("h1", QStringLiteral("美伢Mya"), "217331"),
        host("unknown", QStringLiteral("不在名单"), "999")});
    QCOMPARE(result.conflictNames.size(), 1);
    QCOMPARE(result.unmatchedNames.size(), 1);
    QCOMPARE(result.teams.first().memberIds, current.first().memberIds);
    result = MaoziTeamImport::build(current, roster(), {}, {
        host("h1", QStringLiteral("尐表哥"), "217331"),
        host("h2", QStringLiteral("尐表哥"), "217331", QStringLiteral("蓝队"))});
    QCOMPARE(result.conflictNames.size(), 2);
    QCOMPARE(result.teams.first().memberIds, current.first().memberIds);
    auto members = roster();
    members.append({"duplicate", QStringLiteral("尐表哥"), {}, "111", "S"});
    result = MaoziTeamImport::build(current, members, {}, {
        host("h1", QStringLiteral("尐表哥"), "217331")});
    QCOMPARE(result.conflictNames.size(), 1);
}

void MaoziTeamImportTest::rejectsInvalidTeamsAndCapacity()
{
    auto invalid = host("h", QStringLiteral("尐表哥"), "217331");
    invalid.insert("teamValid", false);
    QVERIFY(!MaoziTeamImport::build({}, roster(), {}, {invalid}).error.isEmpty());
    QVector<NativeTeam> current;
    for (int i = 0; i < 20; ++i) current.append({QString::number(i), QString::number(i), {}});
    const auto full = MaoziTeamImport::build(current, roster(), {}, {
        host("h", QStringLiteral("尐表哥"), "217331")});
    QVERIFY(!full.error.isEmpty());
    QCOMPARE(full.teams, current);
    current = {{"one", QStringLiteral("红队"), {}}, {"two", QStringLiteral("红队"), {}}};
    QVERIFY(!MaoziTeamImport::build(current, roster(), {}, {
        host("h", QStringLiteral("尐表哥"), "217331")}).error.isEmpty());
}

void MaoziTeamImportTest::excludesLeadersAndKeepsUnassigned()
{
    auto leader = host("leader", QStringLiteral("主播阿郎"), "320155");
    leader.insert("role", "leader");
    auto unassigned = host("unassigned", QStringLiteral("美伢Mya"), "77111", QStringLiteral("未分队"));
    unassigned.insert("teamId", QString());
    const QVector<NativeTeam> current{{"old", QStringLiteral("手动"), {"mya"}}};
    const auto result = MaoziTeamImport::build(current, roster(), {}, {
        leader, unassigned, host("captain", QStringLiteral("尐表哥"), "217331")});
    QCOMPARE(result.changedMembers, 1);
    QVERIFY(result.unassignedNames.contains(QStringLiteral("美伢Mya")));
    QCOMPARE(result.teams.first().memberIds, current.first().memberIds);
    for (const auto &team : result.teams) QVERIFY(!team.memberIds.contains("leader"));
}

void MaoziTeamImportTest::matchesVerifiedCacheAndRejectsDuplicateHosts()
{
    const QVector<GuildRoomCacheEntry> cache{{"manual", "12345", {}, {}, 1, 1}};
    auto result = MaoziTeamImport::build({}, roster(), cache, {
        host("cached", QStringLiteral("改名主播"), "12345")});
    QCOMPARE(result.changedMembers, 1);
    QVERIFY(result.teams.first().memberIds.contains("manual"));
    result = MaoziTeamImport::build({}, roster(), {}, {
        host("duplicate", QStringLiteral("美伢Mya"), "77111"),
        host("duplicate", QStringLiteral("尐表哥"), "217331")});
    QCOMPARE(result.conflictNames.size(), 2);
    QCOMPARE(result.changedMembers, 0);
    auto unverified = cache;
    unverified[0].verifiedAtMs = 0;
    result = MaoziTeamImport::build({}, roster(), unverified, {
        host("cached", QStringLiteral("改名主播"), "12345")});
    QCOMPARE(result.changedMembers, 0);
    QCOMPARE(result.unmatchedNames.size(), 1);
    auto unassigned = host("duplicate", QStringLiteral("手动主播"), "12345");
    unassigned.insert("teamId", QString());
    result = MaoziTeamImport::build({}, roster(), {}, {
        host("duplicate", QStringLiteral("尐表哥"), "217331"), unassigned});
    QCOMPARE(result.changedMembers, 0);
    QCOMPARE(result.conflictNames.size(), 1);
}

void MaoziTeamImportTest::protectsAllCandidatesAcrossConflictingRows()
{
    const QVector<NativeTeam> current{{"old", QStringLiteral("手动"), {"mya", "captain"}}};
    const auto ambiguous = host("a", QStringLiteral("美伢Mya"), "217331");
    const auto valid = host("b", QStringLiteral("尐表哥"), "217331", QStringLiteral("蓝队"));
    for (const auto &entries : {QVariantList{ambiguous, valid}, QVariantList{valid, ambiguous}}) {
        const auto result = MaoziTeamImport::build(current, roster(), {}, entries);
        QCOMPARE(result.changedMembers, 0);
        QCOMPARE(result.conflictNames.size(), 2);
        QCOMPARE(result.teams.first().memberIds, current.first().memberIds);
    }
    const QVector<GuildRoomCacheEntry> aliases{{"mya", "217331", {}, {}, 1, 1}};
    const auto result = MaoziTeamImport::build(current, roster(), aliases, {valid});
    QCOMPARE(result.changedMembers, 0);
    QCOMPARE(result.teams.first().memberIds, current.first().memberIds);
}

QTEST_GUILESS_MAIN(MaoziTeamImportTest)
#include "maozi_team_import_test.moc"
