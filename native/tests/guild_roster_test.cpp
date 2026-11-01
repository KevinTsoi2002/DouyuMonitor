#include <QtTest/QtTest>

#include "workspace/guild_roster.h"

class GuildRosterTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsBundledRosterWithoutRoleLabels();
    void sharesCachedBundledRosterWithoutReparsing();
    void exposesStablePinyinKeysForEveryMember();
    void includesNewMembersWithVerifiedRoomIds();
    void stripsRoleSuffixOnlyForSearch();
    void rejectsInvalidRoomIdsFromResource();
    void rejectsMalformedRootAndUnsupportedVersion();
    void rejectsInvalidMemberShapesAndIds();
    void rejectsMalformedRoomIdsButAcceptsEmptyRoomId();
    void keepsHistoricalIdentitiesAndPublicDirectoryMetadata();
    void suppliesVerifiedPortraitsForHistoricalRankingMembers();
};

void GuildRosterTest::loadsBundledRosterWithoutRoleLabels()
{
    const QVector<GuildMember> members = GuildRoster::bundled();
    QCOMPARE(members.size(), 75);
    QVERIFY(GuildRoster::findById(QStringLiteral("hamster-046")) == nullptr);
    QVERIFY(GuildRoster::findByName(QStringLiteral("白小帅子")) != nullptr);
    for (const GuildMember &member : members) {
        QVERIFY(!member.id.isEmpty());
        QVERIFY(!member.anchorName.isEmpty());
        QVERIFY(!member.anchorName.contains(QStringLiteral("-团长")));
        QVERIFY(!member.anchorName.contains(QStringLiteral("-队长")));
        QVERIFY(!member.anchorName.contains(QStringLiteral("-队员")));
        QVERIFY(!member.anchorName.endsWith(QStringLiteral("-OB")));
    }
}

void GuildRosterTest::sharesCachedBundledRosterWithoutReparsing()
{
    const auto first = GuildRoster::bundled();
    const auto second = GuildRoster::bundled();
    QVERIFY(!first.isEmpty());
    QCOMPARE(first.constData(), second.constData());
    auto changed = first;
    changed.first().anchorName = QStringLiteral("fixture");
    QCOMPARE(GuildRoster::bundled().first().anchorName, first.first().anchorName);
}

void GuildRosterTest::exposesStablePinyinKeysForEveryMember()
{
    const QVector<GuildMember> members = GuildRoster::bundled();
    QCOMPARE(members.size(), 75);

    for (const GuildMember &member : members) {
        QVERIFY2(!member.pinyinKey.isEmpty(), qPrintable(member.anchorName));
        QVERIFY2(member.pinyinKey.size() == 1, qPrintable(member.anchorName));
        const QChar key = member.pinyinKey.at(0);
        QVERIFY2((key >= QLatin1Char('A') && key <= QLatin1Char('Z'))
                     || key == QLatin1Char('#'),
                 qPrintable(QStringLiteral("%1 -> %2")
                                .arg(member.anchorName, member.pinyinKey)));
    }

    QCOMPARE(GuildRoster::findByName(QStringLiteral("寅子"))->pinyinKey,
             QStringLiteral("Y"));
    QCOMPARE(GuildRoster::findByName(QStringLiteral("主播阿飞"))->pinyinKey,
             QStringLiteral("Z"));
    QCOMPARE(GuildRoster::findByName(QStringLiteral("阿愈Ayu"))->pinyinKey,
             QStringLiteral("A"));
    QCOMPARE(GuildRoster::findByName(QStringLiteral("bulaQoQ"))->pinyinKey,
             QStringLiteral("B"));
}

void GuildRosterTest::includesNewMembersWithVerifiedRoomIds()
{
    const GuildMember *youyou = GuildRoster::findByName(QStringLiteral("优优or"));
    QVERIFY(youyou != nullptr);
    QCOMPARE(youyou->id, QStringLiteral("hamster-059"));
    QCOMPARE(youyou->roomId, QStringLiteral("12906996"));
    QCOMPARE(youyou->pinyinKey, QStringLiteral("Y"));
    QVERIFY(GuildRoster::findByName(QStringLiteral("优优77989")) == nullptr);

    const GuildMember *yueya = GuildRoster::findByName(QStringLiteral("小月牙ing"));
    QVERIFY(yueya != nullptr);
    QCOMPARE(yueya->id, QStringLiteral("hamster-060"));
    QCOMPARE(yueya->roomId, QStringLiteral("12106059"));
    QCOMPARE(yueya->pinyinKey, QStringLiteral("X"));

    const GuildMember *mya = GuildRoster::findById(QStringLiteral("hamster-009"));
    QVERIFY(mya != nullptr);
    QCOMPARE(mya->roomId, QStringLiteral("77111"));
    const GuildMember *keke = GuildRoster::findById(QStringLiteral("hamster-017"));
    QVERIFY(keke != nullptr);
    QCOMPARE(keke->roomId, QStringLiteral("12767534"));
    QCOMPARE(keke->portraitRoomIds, QStringList{QStringLiteral("55588")});
    QCOMPARE(GuildRoster::findById(QStringLiteral("hamster-018"))->roomId,
             QStringLiteral("12485490"));
    QCOMPARE(GuildRoster::findById(QStringLiteral("hamster-019"))->roomId,
             QStringLiteral("12866270"));
    QCOMPARE(GuildRoster::findById(QStringLiteral("hamster-020"))->roomId,
             QStringLiteral("12832995"));
    const QHash<QString, QString> additionalS1Rooms{
        {QStringLiteral("hamster-015"), QStringLiteral("6151194")},
        {QStringLiteral("hamster-021"), QStringLiteral("9652630")},
        {QStringLiteral("hamster-022"), QStringLiteral("7183329")},
        {QStringLiteral("hamster-023"), QStringLiteral("11136575")},
        {QStringLiteral("hamster-024"), QStringLiteral("12887525")},
        {QStringLiteral("hamster-025"), QStringLiteral("12861862")},
        {QStringLiteral("hamster-026"), QStringLiteral("12806304")},
        {QStringLiteral("hamster-027"), QStringLiteral("12866727")},
        {QStringLiteral("hamster-028"), QStringLiteral("12767614")},
        {QStringLiteral("hamster-029"), QStringLiteral("12867831")},
        {QStringLiteral("hamster-030"), QStringLiteral("12887453")},
        {QStringLiteral("hamster-031"), QStringLiteral("12646246")},
        {QStringLiteral("hamster-032"), QStringLiteral("12897312")},
        {QStringLiteral("hamster-033"), QStringLiteral("12891352")},
        {QStringLiteral("hamster-034"), QStringLiteral("12651462")},
        {QStringLiteral("hamster-035"), QStringLiteral("12897304")},
        {QStringLiteral("hamster-036"), QStringLiteral("12738439")},
        {QStringLiteral("hamster-037"), QStringLiteral("12812023")},
        {QStringLiteral("hamster-038"), QStringLiteral("12467917")},
        {QStringLiteral("hamster-039"), QStringLiteral("9997")},
        {QStringLiteral("hamster-040"), QStringLiteral("12897307")},
        {QStringLiteral("hamster-050"), QStringLiteral("11222")},
        {QStringLiteral("hamster-051"), QStringLiteral("12235739")},
        {QStringLiteral("hamster-052"), QStringLiteral("12641916")},
        {QStringLiteral("hamster-053"), QStringLiteral("12831589")},
        {QStringLiteral("hamster-054"), QStringLiteral("9669")},
        {QStringLiteral("hamster-055"), QStringLiteral("6632")},
        {QStringLiteral("hamster-056"), QStringLiteral("77788")},
        {QStringLiteral("hamster-057"), QStringLiteral("22411")},
        {QStringLiteral("hamster-058"), QStringLiteral("12570508")},
    };
    for (auto it = additionalS1Rooms.cbegin(); it != additionalS1Rooms.cend(); ++it) {
        const GuildMember *member = GuildRoster::findById(it.key());
        QVERIFY2(member != nullptr, qPrintable(it.key()));
        QCOMPARE(member->roomId, it.value());
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

void GuildRosterTest::suppliesVerifiedPortraitsForHistoricalRankingMembers()
{
    const QHash<QString, QString> expected{
        {QStringLiteral("hamster-039"), QStringLiteral("9997")},
        {QStringLiteral("hamster-049"), QStringLiteral("12900462")},
        {QStringLiteral("hamster-050"), QStringLiteral("11222")},
        {QStringLiteral("hamster-051"), QStringLiteral("12235739")},
        {QStringLiteral("hamster-052"), QStringLiteral("12641916")},
        {QStringLiteral("hamster-053"), QStringLiteral("12831589")},
        {QStringLiteral("hamster-054"), QStringLiteral("9669")},
        {QStringLiteral("hamster-055"), QStringLiteral("6632")},
        {QStringLiteral("hamster-056"), QStringLiteral("77788")},
        {QStringLiteral("hamster-057"), QStringLiteral("22411")},
        {QStringLiteral("hamster-058"), QStringLiteral("12570508")},
        {QStringLiteral("hamster-060"), QStringLiteral("12106059")},
    };
    for (auto it = expected.cbegin(); it != expected.cend(); ++it) {
        const GuildMember *member = GuildRoster::findById(it.key());
        QVERIFY2(member != nullptr, qPrintable(it.key()));
        QCOMPARE(member->roomId, it.value());
        QVERIFY2(member->avatarUrl.startsWith(QStringLiteral("https://apic.douyucdn.cn/")),
                 qPrintable(it.key()));
    }
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

    const GuildMember *guiHai = GuildRoster::findByName(QStringLiteral("归海念一刀"));
    QVERIFY(guiHai != nullptr);
    QCOMPARE(guiHai->roomId, QStringLiteral("2632018"));
    QVERIFY(GuildRoster::findByName(QStringLiteral("阿海")) == nullptr);

    const GuildMember *blindHorse = GuildRoster::findByName(QStringLiteral("你个瞎马"));
    QVERIFY(blindHorse != nullptr);
    QCOMPARE(blindHorse->roomId, QStringLiteral("7204164"));
    QVERIFY(GuildRoster::findByName(QStringLiteral("踏马")) == nullptr);

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
                { "id": "hamster-001", "name": "有效成员", "pinyinKey": "Y" },
                { "id": "hamster-001", "name": "重复 ID", "pinyinKey": "C" },
                { "id": "hamster-002", "name": "   ", "pinyinKey": "K" },
                { "id": "hamster-003", "name": "另一位有效成员", "roomId": "12345", "pinyinKey": "L" }
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
                { "id": "hamster-001", "name": "空房间号", "roomId": "", "pinyinKey": "K" },
                { "id": "hamster-002", "name": "字母房间号", "roomId": "abc", "pinyinKey": "Z" },
                { "id": "hamster-003", "name": "负数房间号", "roomId": "-1", "pinyinKey": "F" },
                { "id": "hamster-004", "name": "超长房间号", "roomId": "123456789012345678901", "pinyinKey": "C" }
            ]
        })"));

    QCOMPARE(members.size(), 1);
    QCOMPARE(members.first().id, QStringLiteral("hamster-001"));
    QVERIFY(members.first().roomId.isEmpty());
}

void GuildRosterTest::keepsHistoricalIdentitiesAndPublicDirectoryMetadata()
{
    int listed = 0, male = 0, female = 0;
    QSet<int> order;
    for (const auto &member : GuildRoster::bundled()) {
        if (!member.directoryListed) continue;
        ++listed;
        male += member.gender == QStringLiteral("m");
        female += member.gender == QStringLiteral("f");
        QVERIFY(!member.directoryName.isEmpty());
        QVERIFY(member.avatarUrl.startsWith(QStringLiteral("https://apic.douyucdn.cn/")));
        QVERIFY(member.directoryOrder >= 0 && member.directoryOrder < 62);
        QVERIFY(!order.contains(member.directoryOrder));
        order.insert(member.directoryOrder);
    }
    QCOMPARE(listed, 62);
    QCOMPARE(male, 25);
    QCOMPARE(female, 37);
    const auto *historical = GuildRoster::findById(QStringLiteral("hamster-060"));
    QVERIFY(historical != nullptr);
    QVERIFY(!historical->directoryListed);
    QCOMPARE(historical->roomId, QStringLiteral("12106059"));
    const auto *renamed = GuildRoster::findByName(QStringLiteral("雾蒙蒙y"));
    QVERIFY(renamed != nullptr);
    QCOMPARE(renamed->directoryName, QStringLiteral("雾萌萌y"));
    QCOMPARE(renamed->roomId, QStringLiteral("12874029"));
    const auto *newMember = GuildRoster::findByName(QStringLiteral("白小帅子"));
    QVERIFY(newMember != nullptr);
    QCOMPARE(newMember->id, QStringLiteral("guild-51417"));
    QVERIFY(GuildRoster::findById(QStringLiteral("hamster-046")) == nullptr);
    QCOMPARE(GuildRoster::findById(QStringLiteral("hamster-001"))->directoryOrder, 0);
}

QTEST_GUILESS_MAIN(GuildRosterTest)

#include "guild_roster_test.moc"
