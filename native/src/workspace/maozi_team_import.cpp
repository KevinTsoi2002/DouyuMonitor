#include "workspace/maozi_team_import.h"

#include <QHash>
#include <QSet>
#include <QUuid>

namespace {
QString nameKey(const QString &name)
{
    auto key = GuildRoster::normalizedName(name).toCaseFolded();
    if (key == QStringLiteral("雾蒙蒙y")) key = QStringLiteral("雾萌萌y");
    return key;
}

QString teamKey(const QString &name)
{
    return name.trimmed().toCaseFolded();
}
}

MaoziTeamImportPlan MaoziTeamImport::build(const QVector<NativeTeam> &currentTeams,
                                         const QVector<GuildMember> &roster,
                                         const QVector<GuildRoomCacheEntry> &roomCache,
                                         const QVariantList &rankEntries)
{
    MaoziTeamImportPlan plan;
    plan.teams = currentTeams;
    QHash<QString, int> teamIndexes;
    for (int i = 0; i < currentTeams.size(); ++i) {
        const auto key = teamKey(currentTeams.at(i).name);
        if (key.isEmpty() || teamIndexes.contains(key)) {
            plan.error = QStringLiteral("请先处理本地重复的队伍名称");
            return plan;
        }
        teamIndexes.insert(key, i);
    }

    QHash<QString, QSet<QString>> names, rooms;
    QHash<QString, QString> memberNames;
    for (const auto &member : roster) {
        memberNames.insert(member.id, member.anchorName);
        names[nameKey(member.anchorName)].insert(member.id);
        if (!member.roomId.isEmpty()) rooms[member.roomId].insert(member.id);
    }
    for (const auto &entry : roomCache) {
        if (memberNames.contains(entry.memberId) && !entry.roomId.isEmpty()
            && entry.verifiedAtMs > 0)
            rooms[entry.roomId].insert(entry.memberId);
    }

    struct Match {
        QString name, memberId, team;
        bool conflict = false;
        QSet<QString> candidates;
    };
    QVector<Match> matches;
    QStringList sourceTeams;
    QHash<QString, QString> sourceNames;
    QHash<QString, int> sourceCounts;
    QHash<QString, int> hostCounts;
    QHash<QString, QVector<int>> memberMatches;
    for (const auto &value : rankEntries) {
        const auto id = value.toMap().value(QStringLiteral("id")).toString();
        if (!id.isEmpty()) ++hostCounts[id];
    }
    for (const auto &value : rankEntries) {
        const auto entry = value.toMap();
        if (!entry.value(QStringLiteral("teamValid")).toBool()) {
            plan.error = QStringLiteral("野榜队伍格式暂不支持");
            return plan;
        }
        if (entry.value(QStringLiteral("role")).toString() == QStringLiteral("leader")) continue;
        if (entry.value(QStringLiteral("teamId")).toString().isEmpty()) {
            plan.unassignedNames.append(entry.value(QStringLiteral("name")).toString());
            continue;
        }
        const auto teamName = entry.value(QStringLiteral("teamName")).toString().trimmed();
        const auto key = teamKey(teamName);
        if (key.isEmpty()) {
            plan.error = QStringLiteral("野榜队伍格式暂不支持");
            return plan;
        }
        if (!sourceNames.contains(key)) {
            sourceTeams.append(key);
            sourceNames.insert(key, teamName);
        }
        ++sourceCounts[key];
        const auto hostId = entry.value(QStringLiteral("id")).toString();
        const auto name = entry.value(QStringLiteral("name")).toString();
        const auto byName = names.value(nameKey(name));
        const auto byRoom = rooms.value(entry.value(QStringLiteral("roomId")).toString());
        const auto candidates = byName | byRoom;
        Match match{name, {}, key, candidates.size() > 1 || hostId.isEmpty(), candidates};
        if (candidates.size() == 1) {
            match.memberId = *candidates.cbegin();
            memberMatches[match.memberId].append(matches.size());
        }
        matches.append(match);
    }
    // A second pass prevents a later duplicate from changing an earlier member.
    QSet<QString> protectedMembers;
    int matchIndex = 0;
    for (const auto &value : rankEntries) {
        const auto entry = value.toMap();
        if (entry.value(QStringLiteral("role")).toString() == QStringLiteral("leader")
            || entry.value(QStringLiteral("teamId")).toString().isEmpty()) continue;
        auto &match = matches[matchIndex++];
        if (hostCounts.value(entry.value(QStringLiteral("id")).toString()) > 1
            || memberMatches.value(match.memberId).size() > 1) match.conflict = true;
        if (match.conflict) protectedMembers.unite(match.candidates);
    }
    for (auto &match : matches) {
        if (protectedMembers.contains(match.memberId)) match.conflict = true;
        if (match.conflict) plan.conflictNames.append(match.name);
        else if (match.memberId.isEmpty()) plan.unmatchedNames.append(match.name);
    }

    for (const auto &key : sourceTeams)
        if (!teamIndexes.contains(key)) ++plan.createdTeams;
    if (currentTeams.size() + plan.createdTeams > 20) {
        plan.error = QStringLiteral("导入后将超过 20 个队伍");
        return plan;
    }
    for (const auto &key : sourceTeams) {
        if (teamIndexes.contains(key)) continue;
        teamIndexes.insert(key, plan.teams.size());
        plan.teams.append({QUuid::createUuid().toString(QUuid::WithoutBraces),
                           sourceNames.value(key), {}});
    }
    QHash<QString, QStringList> imported;
    for (const auto &match : matches) {
        if (match.conflict || match.memberId.isEmpty()) continue;
        const auto targetIndex = teamIndexes.value(match.team);
        bool unchanged = plan.teams.at(targetIndex).memberIds.contains(match.memberId);
        for (int i = 0; i < plan.teams.size(); ++i) {
            if (i == targetIndex) continue;
            if (plan.teams[i].memberIds.removeAll(match.memberId) > 0) unchanged = false;
        }
        if (!plan.teams[targetIndex].memberIds.contains(match.memberId))
            plan.teams[targetIndex].memberIds.append(match.memberId);
        if (unchanged) ++plan.unchangedMembers;
        else ++plan.changedMembers;
        imported[match.team].append(match.memberId);
    }
    for (const auto &key : sourceTeams) {
        const auto &team = plan.teams.at(teamIndexes.value(key));
        QStringList displayNames, retainedNames;
        for (const auto &id : imported.value(key)) displayNames.append(memberNames.value(id));
        for (const auto &id : team.memberIds)
            if (!imported.value(key).contains(id)) retainedNames.append(memberNames.value(id));
        plan.previewTeams.append(QVariantMap{
            {QStringLiteral("name"), team.name},
            {QStringLiteral("sourceCount"), sourceCounts.value(key)},
            {QStringLiteral("matchedCount"), displayNames.size()},
            {QStringLiteral("retainedCount"), retainedNames.size()},
            {QStringLiteral("members"), displayNames},
            {QStringLiteral("retainedMembers"), retainedNames}});
    }
    return plan;
}
