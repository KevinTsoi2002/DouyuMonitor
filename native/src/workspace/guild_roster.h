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
    static const GuildMember *findByName(const QString &anchorName);
    static const GuildMember *findById(const QString &memberId);
    static QString searchName(const QString &rawName);
    static QString normalizedName(const QString &rawName);

private:
    static QString normalizedKey(const QString &rawName);
};
