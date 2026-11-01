#include "workspace/guild_roster.h"

#include <algorithm>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>

namespace {

const QString kRosterResourcePath =
    QStringLiteral(":/guild/app/resources/hamster_agent_roster.json");
const QString kPortraitResourcePath =
    QStringLiteral(":/guild/app/resources/guild_portraits.json");
const QRegularExpression kRoomIdPattern(QStringLiteral(R"(^[0-9]{1,20}$)"));
const QRegularExpression kRoleSuffix(
    QStringLiteral(R"(-(?:团长|队长|队员|OB)$)"),
    QRegularExpression::CaseInsensitiveOption);

bool isValidRoomId(const QString &roomId)
{
    return kRoomIdPattern.match(roomId).hasMatch();
}

QVector<GuildMember> addPublicDirectory(QVector<GuildMember> members, const QByteArray &json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json);
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != 1) return members;
    QSet<QString> listedIds;
    QSet<int> orders;
    for (const auto &value : root.value(QStringLiteral("members")).toArray()) {
        const auto entry = value.toObject();
        const QString id = entry.value(QStringLiteral("id")).toString().trimmed();
        const QString name = entry.value(QStringLiteral("name")).toString().trimmed();
        const QString roomId = entry.value(QStringLiteral("roomId")).toString();
        const QString gender = entry.value(QStringLiteral("gender")).toString();
        const int order = entry.value(QStringLiteral("order")).toInt(-1);
        if (id.isEmpty() || name.isEmpty() || !isValidRoomId(roomId)
            || (gender != QStringLiteral("m") && gender != QStringLiteral("f"))
            || order < 0 || listedIds.contains(id) || orders.contains(order)) continue;
        auto found = std::find_if(members.begin(), members.end(), [&id](const GuildMember &member) {
            return member.id == id;
        });
        if (found == members.end()) {
            // New public identities never reuse a retired hamster-* identifier.
            if (id != QStringLiteral("guild-") + roomId) continue;
            GuildMember member;
            member.id = id;
            member.anchorName = GuildRoster::normalizedName(name);
            member.searchName = GuildRoster::searchName(name);
            member.pinyinKey = QStringLiteral("#");
            members.push_back(member);
            found = members.end() - 1;
        }
        // Keep the historical canonical name and persisted ID; only current
        // directory presentation and default room lookup follow the public list.
        found->directoryName = name;
        if (!found->roomId.isEmpty() && found->roomId != roomId)
            found->portraitRoomIds.append(found->roomId);
        found->roomId = roomId;
        found->gender = gender;
        found->directoryOrder = order;
        found->directoryListed = true;
        const QUrl avatar(entry.value(QStringLiteral("avatarUrl")).toString());
        if (avatar.isValid() && avatar.scheme() == QStringLiteral("https")
            && avatar.host() == QStringLiteral("apic.douyucdn.cn")
            && avatar.userInfo().isEmpty() && !avatar.hasQuery()) {
            found->avatarUrl = avatar.toString();
        }
        listedIds.insert(id);
        orders.insert(order);
    }
    return members;
}

QVector<GuildMember> addVerifiedPortraits(QVector<GuildMember> members, const QByteArray &json)
{
    const QJsonDocument document = QJsonDocument::fromJson(json);
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("version")).toInt() != 1) return members;

    for (const auto &value : root.value(QStringLiteral("portraits")).toArray()) {
        const auto entry = value.toObject();
        const QString memberId = entry.value(QStringLiteral("memberId")).toString().trimmed();
        const QString roomId = entry.value(QStringLiteral("roomId")).toString().trimmed();
        const QUrl avatar(entry.value(QStringLiteral("avatarUrl")).toString());
        if (memberId.isEmpty() || !isValidRoomId(roomId)
            || !avatar.isValid() || avatar.scheme() != QStringLiteral("https")
            || avatar.host() != QStringLiteral("apic.douyucdn.cn")
            || !avatar.userInfo().isEmpty() || avatar.hasQuery()) {
            continue;
        }

        auto found = std::find_if(members.begin(), members.end(), [&memberId](const GuildMember &member) {
            return member.id == memberId;
        });
        if (found == members.end() || found->roomId != roomId || !found->avatarUrl.isEmpty()) continue;
        found->avatarUrl = avatar.toString();
    }
    return members;
}

} // namespace

QVector<GuildMember> GuildRoster::bundled()
{
    static const QVector<GuildMember> members = [] {
        QFile file(kRosterResourcePath);
        if (!file.open(QIODevice::ReadOnly)) return QVector<GuildMember>{};
        auto members = parse(file.readAll());
        QFile directory(QStringLiteral(":/guild/app/resources/guild_directory.json"));
        if (directory.open(QIODevice::ReadOnly))
            members = addPublicDirectory(std::move(members), directory.readAll());
        QFile portraits(kPortraitResourcePath);
        return portraits.open(QIODevice::ReadOnly)
            ? addVerifiedPortraits(std::move(members), portraits.readAll()) : members;
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
