#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

struct GuildMember {
    QString id;
    QString anchorName;
    QString searchName;
    QString roomId;

    bool operator==(const GuildMember &) const = default;
};

class GuildRoster final {
public:
    static QVector<GuildMember> bundled();
    static QVector<GuildMember> parse(const QByteArray &json);
    // Returned pointers refer to immutable process-lifetime static storage.
    static const GuildMember *findByName(const QString &anchorName);
    static const GuildMember *findById(const QString &memberId);

    // Query-name normalization and canonical identity normalization currently
    // share the same rules, but are kept separate to avoid binding query-only
    // changes to persisted or displayed member identities.
    static QString searchName(const QString &rawName);
    static QString normalizedName(const QString &rawName);

private:
    static QString normalizedKey(const QString &rawName);
};
