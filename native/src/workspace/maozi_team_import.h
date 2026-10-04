#pragma once

#include <QVariantList>
#include "workspace/guild_roster.h"
#include "workspace/native_workspace_types.h"

struct MaoziTeamImportPlan {
    QVector<NativeTeam> teams;
    QVariantList previewTeams;
    QStringList unmatchedNames;
    QStringList conflictNames;
    QStringList unassignedNames;
    QString error;
    int createdTeams = 0;
    int changedMembers = 0;
    int unchangedMembers = 0;
};

class MaoziTeamImport final {
public:
    static MaoziTeamImportPlan build(const QVector<NativeTeam> &currentTeams,
                                    const QVector<GuildMember> &roster,
                                    const QVector<GuildRoomCacheEntry> &roomCache,
                                    const QVariantList &rankEntries);
};
