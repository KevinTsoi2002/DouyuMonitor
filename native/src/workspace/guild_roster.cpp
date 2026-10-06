#include "workspace/guild_roster.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace {

const QString kRosterResourcePath =
    QStringLiteral(":/guild/app/resources/hamster_agent_roster.json");
const QRegularExpression kRoomIdPattern(QStringLiteral(R"(^[0-9]{1,20}$)"));
const QRegularExpression kRoleSuffix(
    QStringLiteral(R"(-(?:团长|队长|队员|OB)$)"),
    QRegularExpression::CaseInsensitiveOption);

bool isValidRoomId(const QString &roomId)
{
    return kRoomIdPattern.match(roomId).hasMatch();
}

} // namespace

QVector<GuildMember> GuildRoster::bundled()
{
    static const QVector<GuildMember> members = [] {
        QFile file(kRosterResourcePath);
        if (!file.open(QIODevice::ReadOnly)) return QVector<GuildMember>{};
        return parse(file.readAll());
    }();
    return members;
}

QVector<GuildMember> GuildRoster::parse(const QByteArray &json)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) return {};

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt(-1) != 1) return {};

    const QJsonValue membersValue = root.value(QStringLiteral("members"));
    if (!membersValue.isArray()) return {};

    QVector<GuildMember> members;
    QSet<QString> ids;
    const QJsonArray memberArray = membersValue.toArray();
    members.reserve(memberArray.size());
    for (const QJsonValue &value : memberArray) {
        if (!value.isObject()) continue;
        const QJsonObject object = value.toObject();
        const QString id = object.value(QStringLiteral("id")).toString().trimmed();
        const QString rawName = object.value(QStringLiteral("name")).toString().trimmed();
        const QString roomId = object.value(QStringLiteral("roomId")).toString().trimmed();
        const QString pinyinKey =
            object.value(QStringLiteral("pinyinKey")).toString().trimmed().toUpper();
        const bool validPinyinKey =
            pinyinKey.size() == 1
            && ((pinyinKey.at(0) >= QLatin1Char('A') && pinyinKey.at(0) <= QLatin1Char('Z'))
                || pinyinKey.at(0) == QLatin1Char('#'));
        if (id.isEmpty() || rawName.isEmpty() || ids.contains(id)
            || (!roomId.isEmpty() && !isValidRoomId(roomId))
            || !validPinyinKey) {
            continue;
        }

        const QString anchorName = normalizedName(rawName);
        if (anchorName.isEmpty()) continue;

        ids.insert(id);
        members.push_back({
            id,
            anchorName,
            searchName(rawName),
            roomId,
            pinyinKey,
        });
    }
    return members;
}

const GuildMember *GuildRoster::findByName(const QString &anchorName)
{
    static const QVector<GuildMember> members = bundled();
    const QString key = normalizedKey(anchorName);
    for (const GuildMember &member : members) {
        if (normalizedKey(member.anchorName) == key) return &member;
    }
    return nullptr;
}

const GuildMember *GuildRoster::findById(const QString &memberId)
{
    static const QVector<GuildMember> members = bundled();
    const QString normalizedId = memberId.trimmed();
    for (const GuildMember &member : members) {
        if (member.id == normalizedId) return &member;
    }
    return nullptr;
}

QString GuildRoster::searchName(const QString &rawName)
{
    return normalizedName(rawName);
}

QString GuildRoster::normalizedName(const QString &rawName)
{
    const QString trimmed = rawName.trimmed();
    QString normalized = trimmed;
    normalized.remove(kRoleSuffix);
    return normalized.trimmed();
}

QString GuildRoster::normalizedKey(const QString &rawName)
{
    return normalizedName(rawName).toCaseFolded();
}
