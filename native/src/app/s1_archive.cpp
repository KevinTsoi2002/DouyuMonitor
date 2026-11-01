#include "app/s1_archive.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace {
bool uniqueRecords(const QVariantList &records, const QString &field, QString *error)
{
    QSet<QString> ids;
    for (const QVariant &item : records) {
        const QVariantMap record = item.toMap();
        const QString id = record.value(field).toString();
        if (id.isEmpty() || ids.contains(id)) {
            if (error) *error = QStringLiteral("missing or duplicate %1").arg(field);
            return false;
        }
        ids.insert(id);
    }
    return true;
}
}

QVariantMap S1Archive::parse(const QByteArray &json, QString *error)
{
    if (error) error->clear();
    const QJsonDocument document = QJsonDocument::fromJson(json);
    if (!document.isObject()) {
        if (error) *error = QStringLiteral("invalid JSON object");
        return {};
    }
    const QVariantMap archive = document.object().toVariantMap();
    if (archive.value(QStringLiteral("version")).toInt() != 1
        || archive.value(QStringLiteral("season")).toString().isEmpty()
        || archive.value(QStringLiteral("capturedAt")).toString().isEmpty()
        || archive.value(QStringLiteral("sourceUrl")).toString()
            != QStringLiteral("https://cstg-zy.com/#/squad/s1")) {
        if (error) *error = QStringLiteral("invalid archive metadata");
        return {};
    }

    const QVariantList ranking = archive.value(QStringLiteral("ranking")).toMap()
                                     .value(QStringLiteral("participants")).toList();
    const QVariantList regular = archive.value(QStringLiteral("regularSeason")).toMap()
                                     .value(QStringLiteral("matches")).toList();
    const QVariantList finals = archive.value(QStringLiteral("finals")).toMap()
                                    .value(QStringLiteral("matches")).toList();
    const QVariantList football = archive.value(QStringLiteral("football")).toMap()
                                      .value(QStringLiteral("matches")).toList();
    const QVariantMap placement = archive.value(QStringLiteral("placement")).toMap();
    const QVariantList participants = placement.value(QStringLiteral("participants")).toList();
    const QVariantList sessions = placement.value(QStringLiteral("sessions")).toList();
    if (ranking.isEmpty() || regular.isEmpty() || finals.isEmpty() || football.isEmpty()
        || participants.isEmpty() || sessions.isEmpty()) {
        if (error) *error = QStringLiteral("missing archive section");
        return {};
    }
    if (!uniqueRecords(ranking, QStringLiteral("id"), error)
        || !uniqueRecords(regular, QStringLiteral("id"), error)
        || !uniqueRecords(finals, QStringLiteral("id"), error)
        || !uniqueRecords(football, QStringLiteral("id"), error)
        || !uniqueRecords(participants, QStringLiteral("id"), error)) {
        return {};
    }
    QSet<QString> rankingIds;
    for (const QVariant &entry : ranking) rankingIds.insert(entry.toMap().value("id").toString());
    for (const QVariant &entry : participants) {
        if (rankingIds.contains(entry.toMap().value("id").toString())) continue;
        if (error) *error = QStringLiteral("dangling placement participant reference");
        return {};
    }
    return archive;
}

QVariantMap S1Archive::bundled()
{
    QFile resource(QStringLiteral(":/archive/cstg_s1_archive.json"));
    if (!resource.open(QIODevice::ReadOnly)) return {};
    return parse(resource.readAll());
}
