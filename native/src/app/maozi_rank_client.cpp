#include "app/maozi_rank_client.h"

#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPair>
#include <QSet>
#include <QTimer>
#include <QUuid>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {

double resultValue(const QJsonValue &value);

const QString kCloudBaseHost =
    QStringLiteral("https://dy656750-d6g192t6k4a51aa36.api.tcloudbasegateway.com");
const QString kPosterCdn =
    QStringLiteral("https://6479-dy656750-d6g192t6k4a51aa36-1309340272.tcb.qcloud.la");
const QSet<QString> kLeaderRoomIds{
    QStringLiteral("320155"),
};
const QSet<QString> kCaptainRoomIds{
    QStringLiteral("731252"),
    QStringLiteral("7204164"),
    QStringLiteral("80432"),
    QStringLiteral("6151194"),
    QStringLiteral("217331"),
    QStringLiteral("2632018"),
    QStringLiteral("11222"),
    QStringLiteral("2140934"),
};
const QHash<QString, QString> kKnownNameAliases{
    {QStringLiteral("雾蒙蒙y"), QStringLiteral("雾萌萌y")},
};
const QStringList kPlacementDays{
    QStringLiteral("2026-09-28"),
    QStringLiteral("2026-09-29"),
    QStringLiteral("2026-09-30"),
};

const QStringList kTeamNames{
    QStringLiteral("红队"),
    QStringLiteral("蓝队"),
    QStringLiteral("黄队"),
    QStringLiteral("黑队"),
};

double numberValue(const QJsonValue &value, double fallback = 0.0)
{
    return value.isDouble() ? value.toDouble() : fallback;
}

QString statusMessage(const QNetworkReply *reply)
{
    if (reply != nullptr && reply->error() == QNetworkReply::TimeoutError) {
        return QStringLiteral("野榜请求超时");
    }
    return QStringLiteral("野榜数据加载失败");
}

QString normalizedName(const QString &value)
{
    return value.trimmed().toCaseFolded();
}

bool isMemberRole(const QString &role)
{
    return role == QStringLiteral("member");
}

struct PlacementStage {
    int slot = 0;
    int stage = 0;
    QString metric;
    QString direction;
    QString name;
};

struct PlacementSession {
    QString key;
    QString label;
    QString title;
    QString rankMode;
    QString direction;
    int slot = 0;
    QVector<PlacementStage> stages;
    QHash<QString, double> rawByHost;
    QHash<QString, bool> presentByHost;
    QHash<QString, double> scoreByHost;
};

double resultValue(const QJsonValue &value)
{
    if (value.isDouble()) return value.toDouble();
    if (value.isString()) {
        bool ok = false;
        const double parsed = value.toString().toDouble(&ok);
        return ok ? parsed : std::numeric_limits<double>::quiet_NaN();
    }
    return std::numeric_limits<double>::quiet_NaN();
}

QVector<int> competitionRanks(const QVector<double> &sortedValues)
{
    QVector<int> ranks(sortedValues.size(), 0);
    int index = 0;
    int rank = 1;
    while (index < sortedValues.size()) {
        int end = index + 1;
        while (end < sortedValues.size() && sortedValues.at(end) == sortedValues.at(index)) {
            ++end;
        }
        for (int position = index; position < end; ++position) ranks[position] = rank;
        rank += end - index;
        index = end;
    }
    return ranks;
}

QVariantMap placementColumns(const QVector<PlacementSession> &sessions)
{
    QVariantMap columns;
    for (const PlacementSession &session : sessions) {
        columns.insert(session.key, QVariantMap{
            {QStringLiteral("key"), session.key},
            {QStringLiteral("label"), session.label},
            {QStringLiteral("title"), session.title},
        });
    }
    return columns;
}

QHash<QString, int> competitionRanksForResults(const QVector<QJsonObject> &results,
                                               const QString &direction)
{
    QVector<QJsonObject> sorted = results;
    const bool descending = direction == QStringLiteral("desc");
    std::sort(sorted.begin(), sorted.end(), [descending](const QJsonObject &left,
                                                        const QJsonObject &right) {
        const double leftValue = left.value(QStringLiteral("value")).toDouble();
        const double rightValue = right.value(QStringLiteral("value")).toDouble();
        if (leftValue != rightValue) return descending ? leftValue > rightValue
                                                       : leftValue < rightValue;
        return left.value(QStringLiteral("host_id")).toString()
            < right.value(QStringLiteral("host_id")).toString();
    });

    QHash<QString, int> ranks;
    int position = 0;
    int rank = 1;
    while (position < sorted.size()) {
        int end = position + 1;
        while (end < sorted.size()
               && sorted.at(end).value(QStringLiteral("value")).toDouble()
                      == sorted.at(position).value(QStringLiteral("value")).toDouble()) {
            ++end;
        }
        for (int index = position; index < end; ++index) {
            ranks.insert(sorted.at(index).value(QStringLiteral("host_id")).toString(), rank);
        }
        rank += end - position;
        position = end;
    }
    return ranks;
}

QString placementSessionLabel(const QString &date, int slot)
{
    const int day = date.section(QLatin1Char('-'), 2, 2).toInt();
    return QStringLiteral("%1%2").arg(day).arg(slot == 2 ? QStringLiteral("晚")
                                                        : QStringLiteral("午"));
}

} // namespace

MaoziRankClient::MaoziRankClient(QUrl authEndpoint, QUrl snapshotEndpoint, int timeoutMs, QObject *parent)
    : QObject(parent)
    , manager_(new QNetworkAccessManager(this))
    , timeoutTimer_(new QTimer(this))
    , authEndpoint_(authEndpoint.isValid()
                        ? std::move(authEndpoint)
                        : QUrl(kCloudBaseHost + QStringLiteral("/auth/v1/signin/anonymously")))
    , snapshotEndpoint_(snapshotEndpoint.isValid()
                            ? std::move(snapshotEndpoint)
                            : QUrl(kCloudBaseHost + QStringLiteral("/v1/rdb/rest/rpc/fn_rank_snapshot")))
    , timeoutMs_(qMax(timeoutMs, 1))
{
    const QUrl defaultVersionUrl(
        kCloudBaseHost + QStringLiteral("/v1/rdb/rest/rpc/fn_rank_check"));
    if (snapshotEndpoint_.isValid()
        && snapshotEndpoint_.path().endsWith(QStringLiteral("/fn_rank_snapshot"))) {
        QString versionPath = snapshotEndpoint_.path();
        versionPath.replace(QStringLiteral("/fn_rank_snapshot"),
                            QStringLiteral("/fn_rank_check"));
        QUrl derived = snapshotEndpoint_;
        derived.setPath(versionPath);
        versionEndpoint_ = derived;
    } else {
        versionEndpoint_ = defaultVersionUrl;
    }
    timeoutTimer_->setSingleShot(true);
    connect(timeoutTimer_, &QTimer::timeout, this, [this] {
        if (reply_ == nullptr) return;
        reply_->abort();
        finishError(QStringLiteral("野榜请求超时"), true);
    });
}

QString MaoziRankClient::statusText() const
{
    if (loading_) return QStringLiteral("正在加载野榜数据...");
    if (!errorMessage_.isEmpty()) return errorMessage_;
    if (entries_.isEmpty()) return QStringLiteral("暂无野榜数据");
    return QStringLiteral("共 %1 位主播 · %2 人参与")
        .arg(entries_.size())
        .arg(totalVoters_);
}

void MaoziRankClient::refresh()
{
    if (reply_ != nullptr) {
        QNetworkReply *stale = reply_;
        reply_ = nullptr;
        timeoutTimer_->stop();
        stale->abort();
        stale->deleteLater();
    }
    accessToken_.clear();
    errorMessage_.clear();
    lastSyncError_.clear();
    emit entriesChanged();
    emit syncStateChanged();
    setLoading(true);
    pendingAuthAction_ = PendingAuthAction::Snapshot;
    requestAnonymousSession(PendingAuthAction::Snapshot);
}

void MaoziRankClient::checkForChanges()
{
    if (reply_ != nullptr || syncPending_) return;
    setSyncPending(true);
    if (!accessToken_.isEmpty()) {
        pendingAuthAction_ = PendingAuthAction::VersionCheck;
        requestVersionCheck(accessToken_);
        return;
    }
    pendingAuthAction_ = PendingAuthAction::VersionCheck;
    requestAnonymousSession(PendingAuthAction::VersionCheck);
}

QVariantMap MaoziRankClient::entryForRoomId(const QString &roomId) const
{
    const int index = entryIndexByRoomId_.value(roomId.trimmed(), -1);
    return index >= 0 && index < entries_.size() ? entries_.at(index).toMap() : QVariantMap{};
}

QVariantMap MaoziRankClient::entryForName(const QString &name) const
{
    const QString key = normalizedName(canonicalLookupName(name));
    const int index = entryIndexByName_.value(key, -1);
    return index >= 0 && index < entries_.size() ? entries_.at(index).toMap() : QVariantMap{};
}

double MaoziRankClient::placementScore(int competitorCount, int rank)
{
    if (rank <= 0) return 0.0;
    if (competitorCount <= 1) return 94.0;
    const double normalized =
        static_cast<double>(competitorCount - rank) / (competitorCount - 1);
    return 46.0 + 48.0 * std::pow(normalized, 0.6);
}

void MaoziRankClient::requestAnonymousSession(PendingAuthAction action)
{
    pendingAuthAction_ = action;
    QUrl endpoint = authEndpoint_;
    if (!endpoint.hasQuery() || !endpoint.query().contains(QStringLiteral("client_id="))) {
        QUrlQuery query(endpoint);
        query.addQueryItem(QStringLiteral("client_id"),
                           QStringLiteral("dy656750-d6g192t6k4a51aa36"));
        endpoint.setQuery(query);
    }

    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("X-Device-Id", QUuid::createUuid().toString(QUuid::WithoutBraces).toUtf8());
    request.setRawHeader("X-SDK-Version", "@cloudbase/js-sdk/3.10.1");
    reply_ = manager_->post(request, QByteArrayLiteral("{}"));
    QNetworkReply *requestReply = reply_;
    connect(requestReply, &QNetworkReply::finished, this, [this, requestReply] {
        if (reply_ != requestReply) return;
        reply_ = nullptr;
        timeoutTimer_->stop();
        handleAuthReply(requestReply);
        requestReply->deleteLater();
    });
    timeoutTimer_->start(timeoutMs_);
}

void MaoziRankClient::requestSnapshot(const QString &accessToken)
{
    QNetworkRequest request(snapshotEndpoint_);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());
    request.setRawHeader("X-SDK-Version", "@cloudbase/js-sdk/3.10.1");
    QJsonObject body{
        {QStringLiteral("p_dev"), false},
        {QStringLiteral("p_token"), QJsonValue::Null},
        {QStringLiteral("p_days"), QJsonArray::fromStringList(kPlacementDays)},
    };
    reply_ = manager_->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QNetworkReply *requestReply = reply_;
    connect(requestReply, &QNetworkReply::finished, this, [this, requestReply] {
        if (reply_ != requestReply) return;
        reply_ = nullptr;
        timeoutTimer_->stop();
        handleSnapshotReply(requestReply);
        requestReply->deleteLater();
    });
    timeoutTimer_->start(timeoutMs_);
}

void MaoziRankClient::requestVersionCheck(const QString &accessToken)
{
    QNetworkRequest request(versionEndpoint_);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());
    request.setRawHeader("X-SDK-Version", "@cloudbase/js-sdk/3.10.1");
    reply_ = manager_->post(request, QByteArrayLiteral("{\"p_dev\":false}"));
    QNetworkReply *requestReply = reply_;
    connect(requestReply, &QNetworkReply::finished, this, [this, requestReply] {
        if (reply_ != requestReply) return;
        reply_ = nullptr;
        timeoutTimer_->stop();
        handleVersionReply(requestReply);
        requestReply->deleteLater();
    });
    timeoutTimer_->start(timeoutMs_);
}

void MaoziRankClient::handleAuthReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        finishError(statusMessage(reply), pendingAuthAction_ == PendingAuthAction::VersionCheck);
        return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status < 200 || status >= 300) {
        finishError(QStringLiteral("野榜会话建立失败"),
                    pendingAuthAction_ == PendingAuthAction::VersionCheck);
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        finishError(QStringLiteral("野榜会话数据无效"),
                    pendingAuthAction_ == PendingAuthAction::VersionCheck);
        return;
    }
    const QString token = document.object().value(QStringLiteral("access_token")).toString();
    if (token.isEmpty()) {
        finishError(QStringLiteral("野榜会话数据无效"),
                    pendingAuthAction_ == PendingAuthAction::VersionCheck);
        return;
    }
    accessToken_ = token;
    if (pendingAuthAction_ == PendingAuthAction::VersionCheck) {
        requestVersionCheck(accessToken_);
    } else {
        requestSnapshot(accessToken_);
    }
}

void MaoziRankClient::handleSnapshotReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        finishError(statusMessage(reply), true);
        return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status < 200 || status >= 300) {
        finishError(QStringLiteral("野榜数据加载失败"), true);
        return;
    }
    parseSnapshot(reply->readAll());
}

void MaoziRankClient::handleVersionReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        finishError(QStringLiteral("数据可能已过期"), true);
        return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status < 200 || status >= 300) {
        finishError(QStringLiteral("数据可能已过期"), true);
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        finishError(QStringLiteral("数据可能已过期"), true);
        return;
    }
    QString version = document.object().value(QStringLiteral("v")).toString();
    if (version.isEmpty() && document.object().value(QStringLiteral("v")).isDouble()) {
        version = QString::number(document.object().value(QStringLiteral("v")).toDouble(), 'g', 16);
    }
    setSyncPending(false);
    if (version == snapshotVersion_ && !version.isEmpty()) {
        lastSyncError_.clear();
        emit syncStateChanged();
        return;
    }
    setLoading(true);
    setSyncPending(true);
    requestSnapshot(accessToken_);
}

void MaoziRankClient::parseSnapshot(const QByteArray &payload)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        finishError(QStringLiteral("野榜数据格式无效"), true);
        return;
    }
    const QJsonObject root = document.object();
    const QJsonArray hosts = root.value(QStringLiteral("hosts")).toArray();
    if (hosts.isEmpty()) {
        finishError(QStringLiteral("野榜数据为空"), true);
        return;
    }

    QHash<QString, QJsonObject> aggregates;
    const QJsonObject agg = root.value(QStringLiteral("agg")).toObject();
    for (const QJsonValue &value : agg.value(QStringLiteral("hosts")).toArray()) {
        const QJsonObject item = value.toObject();
        aggregates.insert(item.value(QStringLiteral("host_id")).toString(), item);
    }

    QHash<QString, bool> liveStates;
    for (const QJsonValue &value : root.value(QStringLiteral("live")).toArray()) {
        const QJsonObject item = value.toObject();
        liveStates.insert(item.value(QStringLiteral("room_id")).toString(),
                          item.value(QStringLiteral("status")).toString() == QStringLiteral("live"));
    }

    QVector<QJsonObject> grades;
    QVector<QJsonObject> criteria;
    const QJsonObject config = root.value(QStringLiteral("config")).toObject();
    for (const QJsonValue &value : config.value(QStringLiteral("grades")).toArray()) {
        grades.push_back(value.toObject());
    }
    std::sort(grades.begin(), grades.end(), [](const QJsonObject &left, const QJsonObject &right) {
        return numberValue(left.value(QStringLiteral("min")), -1.0)
            > numberValue(right.value(QStringLiteral("min")), -1.0);
    });
    for (const QJsonValue &value : config.value(QStringLiteral("criteria")).toArray()) {
        criteria.push_back(value.toObject());
    }

    QHash<QString, QJsonObject> playValuesByName;
    for (const QJsonValue &value : root.value(QStringLiteral("playvalue")).toArray()) {
        const QJsonObject item = value.toObject();
        const QString name = normalizedName(item.value(QStringLiteral("nickname")).toString());
        if (!name.isEmpty()) playValuesByName.insert(name, item);
    }

    QVariantList entries;
    QHash<QString, int> entryIndexByHostId;
    QHash<QString, QString> roleByHostId;
    for (const QJsonValue &value : hosts) {
        const QJsonObject host = value.toObject();
        const QString hostId = host.value(QStringLiteral("id")).toString();
        const QJsonObject stats = aggregates.value(hostId);
        const QJsonObject dims = stats.value(QStringLiteral("dims")).toObject();

        double weightedScore = 0.0;
        double totalWeight = 0.0;
        QVariantList dimensionList;
        for (const QJsonObject &criterion : criteria) {
            const QString id = criterion.value(QStringLiteral("id")).toString();
            const QJsonObject dim = dims.value(id).toObject();
            const double count = numberValue(dim.value(QStringLiteral("cnt")));
            const double sum = numberValue(dim.value(QStringLiteral("sum")));
            const double average = count > 0.0 ? sum / count : -1.0;
            dimensionList.push_back(QVariantMap{
                {QStringLiteral("name"), criterion.value(QStringLiteral("name")).toString()},
                {QStringLiteral("average"), average},
                {QStringLiteral("count"), static_cast<int>(count)},
            });
            if (average >= 0.0) {
                weightedScore += average * count;
                totalWeight += count;
            }
        }
        const double score = totalWeight > 0.0 ? weightedScore / totalWeight : -1.0;

        QString grade;
        QString gradeColor;
        for (const QJsonObject &gradeEntry : grades) {
            if (score >= numberValue(gradeEntry.value(QStringLiteral("min")))) {
                grade = gradeEntry.value(QStringLiteral("letter")).toString();
                gradeColor = gradeEntry.value(QStringLiteral("color")).toString();
                break;
            }
        }

        const QString roomId = host.value(QStringLiteral("douyu_id")).toString();
        const QString role = roleForRoomId(roomId);
        const QJsonObject playValue =
            playValuesByName.value(normalizedName(host.value(QStringLiteral("name")).toString()));
        entryIndexByHostId.insert(hostId, entries.size());
        roleByHostId.insert(hostId, role);
        entries.push_back(QVariantMap{
            {QStringLiteral("id"), hostId},
            {QStringLiteral("name"), host.value(QStringLiteral("name")).toString()},
            {QStringLiteral("guild"), host.value(QStringLiteral("guild")).toString()},
            {QStringLiteral("roomId"), roomId},
            {QStringLiteral("note"), host.value(QStringLiteral("note")).toString()},
            {QStringLiteral("posterUrl"), posterUrl(host.value(QStringLiteral("poster")).toString())},
            {QStringLiteral("teamName"), teamName(host.value(QStringLiteral("team")))},
            {QStringLiteral("score"), score},
            {QStringLiteral("grade"), grade},
            {QStringLiteral("gradeColor"), gradeColor},
            {QStringLiteral("voters"), stats.value(QStringLiteral("voters")).toInt()},
            {QStringLiteral("live"), liveStates.value(roomId, false)},
            {QStringLiteral("dimensions"), dimensionList},
            {QStringLiteral("role"), role},
            {QStringLiteral("playValue"),
             playValue.isEmpty() ? QVariant{}
                                 : QVariant(playValue.value(QStringLiteral("points")).toDouble())},
            {QStringLiteral("playValueBombed"),
             playValue.isEmpty() ? 0 : playValue.value(QStringLiteral("bombed")).toInt()},
            {QStringLiteral("playValueUpdatedAt"),
             playValue.value(QStringLiteral("updated_at")).toString()},
            {QStringLiteral("placementAverage"), -1.0},
            {QStringLiteral("placementScoredSessions"), 0},
        });
    }

    const QJsonObject placementRoot = root.value(QStringLiteral("placement")).toObject();
    QVector<PlacementSession> placementSessions;
    QHash<QString, double> placementTotals;
    QHash<QString, int> placementCounts;
    QHash<QString, QHash<QString, double>> placementSessionScores;
    for (auto dayIt = placementRoot.constBegin(); dayIt != placementRoot.constEnd(); ++dayIt) {
        const QString dayKey = dayIt.key();
        const QJsonObject day = dayIt.value().toObject();
        QHash<int, QJsonObject> eventBySlot;
        for (const QJsonValue &value : day.value(QStringLiteral("events")).toArray()) {
            const QJsonObject event = value.toObject();
            eventBySlot.insert(event.value(QStringLiteral("slot")).toInt(), event);
        }

        QHash<int, QVector<PlacementStage>> stagesBySlot;
        for (const QJsonValue &value : day.value(QStringLiteral("stages")).toArray()) {
            const QJsonObject object = value.toObject();
            PlacementStage stage;
            stage.slot = object.value(QStringLiteral("slot")).toInt();
            stage.stage = object.value(QStringLiteral("stage")).toInt();
            stage.metric = object.value(QStringLiteral("metric")).toString(QStringLiteral("time"));
            stage.direction = object.value(QStringLiteral("dir")).toString(QStringLiteral("asc"));
            stage.name = object.value(QStringLiteral("name")).toString();
            stagesBySlot[stage.slot].push_back(stage);
        }

        QHash<int, QVector<QJsonObject>> resultsBySlotAndStage;
        QHash<int, QSet<QString>> seenBySlot;
        for (const QJsonValue &value : day.value(QStringLiteral("results")).toArray()) {
            const QJsonObject object = value.toObject();
            const int slot = object.value(QStringLiteral("slot")).toInt();
            const int stage = object.value(QStringLiteral("stage")).toInt();
            const QString hostId = object.value(QStringLiteral("host_id")).toString();
            if (!entryIndexByHostId.contains(hostId)) continue;
            seenBySlot[slot].insert(hostId);
            if (object.value(QStringLiteral("invalid")).toBool()) continue;
            const double numeric = resultValue(object.value(QStringLiteral("value")));
            if (std::isnan(numeric)) continue;
            QJsonObject result = object;
            result.insert(QStringLiteral("value"), numeric);
            resultsBySlotAndStage[slot * 100 + stage].push_back(result);
        }

        for (auto slotIt = stagesBySlot.constBegin(); slotIt != stagesBySlot.constEnd(); ++slotIt) {
            const int slot = slotIt.key();
            const QVector<PlacementStage> &stages = slotIt.value();
            if (stages.isEmpty()) continue;
            const QJsonObject event = eventBySlot.value(slot);

            PlacementSession session;
            session.key = dayKey + QLatin1Char(':') + QString::number(slot);
            session.label = placementSessionLabel(dayKey, slot);
            session.title = event.value(QStringLiteral("name")).toString();
            session.rankMode = event.value(QStringLiteral("rank_mode")).toString(
                QStringLiteral("sum"));
            session.direction = event.value(QStringLiteral("dir")).toString(
                QStringLiteral("asc"));
            session.slot = slot;
            session.stages = stages;

            const bool paperMode = session.rankMode == QStringLiteral("sum")
                && std::all_of(stages.cbegin(), stages.cend(), [](const PlacementStage &stage) {
                       return stage.metric == QStringLiteral("score");
                   });
            if (session.rankMode == QStringLiteral("rank")) {
                QHash<int, QHash<QString, int>> stageRanks;
                for (const PlacementStage &stage : stages) {
                    const QVector<QJsonObject> results =
                        resultsBySlotAndStage.value(slot * 100 + stage.stage);
                    stageRanks.insert(stage.stage,
                                      competitionRanksForResults(results, stage.direction));
                }
                for (const QString &hostId : seenBySlot.value(slot)) {
                    double rankSum = 0.0;
                    bool hasScore = false;
                    for (const PlacementStage &stage : stages) {
                        const int rank = stageRanks.value(stage.stage).value(hostId, 0);
                        if (rank > 0) {
                            rankSum += rank;
                            hasScore = true;
                        }
                    }
                    if (!hasScore) continue;
                    session.rawByHost.insert(hostId, rankSum);
                    session.presentByHost.insert(hostId, true);
                }
            } else if (paperMode) {
                for (const PlacementStage &stage : stages) {
                    for (const QJsonObject &result :
                         resultsBySlotAndStage.value(slot * 100 + stage.stage)) {
                        const QString hostId =
                            result.value(QStringLiteral("host_id")).toString();
                        session.rawByHost[hostId] +=
                            result.value(QStringLiteral("value")).toDouble();
                        session.presentByHost.insert(hostId, true);
                    }
                }
            } else {
                for (const PlacementStage &stage : stages) {
                    for (const QJsonObject &result :
                         resultsBySlotAndStage.value(slot * 100 + stage.stage)) {
                        const QString hostId =
                            result.value(QStringLiteral("host_id")).toString();
                        session.rawByHost.insert(hostId,
                                                 result.value(QStringLiteral("value")).toDouble());
                        session.presentByHost.insert(hostId, true);
                    }
                }
            }

            if (!session.rawByHost.isEmpty()) {
                QVector<QPair<double, QString>> ranking;
                ranking.reserve(session.rawByHost.size());
                for (auto it = session.rawByHost.constBegin();
                     it != session.rawByHost.constEnd(); ++it) {
                    ranking.push_back({it.value(), it.key()});
                }
                const bool descending =
                    session.rankMode == QStringLiteral("rank")
                        ? false
                        : (paperMode || (stages.size() == 1
                                         && stages.constFirst().direction
                                                == QStringLiteral("desc")));
                std::sort(ranking.begin(), ranking.end(), [descending](const auto &left,
                                                                        const auto &right) {
                    if (left.first != right.first) {
                        return descending ? left.first > right.first
                                          : left.first < right.first;
                    }
                    return left.second < right.second;
                });
                QVector<double> values;
                values.reserve(ranking.size());
                for (const auto &item : ranking) values.push_back(item.first);
                const QVector<int> ranks = competitionRanks(values);
                for (int index = 0; index < ranking.size(); ++index) {
                    const QString hostId = ranking.at(index).second;
                    const double score = placementScore(ranking.size(), ranks.at(index));
                    session.scoreByHost.insert(hostId, score);
                    placementTotals[hostId] += score;
                    placementCounts[hostId] += 1;
                    placementSessionScores[hostId].insert(session.key, score);
                }
                for (const QString &hostId : seenBySlot.value(slot)) {
                    if (session.scoreByHost.contains(hostId)) continue;
                    session.scoreByHost.insert(hostId, 40.0);
                    placementTotals[hostId] += 40.0;
                    placementCounts[hostId] += 1;
                    placementSessionScores[hostId].insert(session.key, 40.0);
                }
            }
            placementSessions.push_back(session);
        }
    }

    std::sort(placementSessions.begin(), placementSessions.end(),
              [](const PlacementSession &left, const PlacementSession &right) {
                  if (left.key != right.key) return left.key < right.key;
                  return left.slot < right.slot;
              });

    QVariantList placementEntries;
    for (int index = 0; index < entries.size(); ++index) {
        QVariantMap entry = entries.at(index).toMap();
        const QString hostId = entry.value(QStringLiteral("id")).toString();
        const int count = placementCounts.value(hostId, 0);
        if (count > 0) {
            QVariantMap sessionScores;
            const QHash<QString, double> scores = placementSessionScores.value(hostId);
            for (auto scoreIt = scores.constBegin(); scoreIt != scores.constEnd(); ++scoreIt) {
                sessionScores.insert(scoreIt.key(), scoreIt.value());
            }
            entry.insert(QStringLiteral("placementAverage"),
                         placementTotals.value(hostId) / count);
            entry.insert(QStringLiteral("placementScoredSessions"), count);
            entry.insert(QStringLiteral("placementRank"), 0);
            entry.insert(QStringLiteral("placementSessions"), sessionScores);
            placementEntries.push_back(entry);
        }
        entries[index] = entry;
    }

    std::sort(placementEntries.begin(), placementEntries.end(),
              [](const QVariant &left, const QVariant &right) {
                  const QVariantMap leftMap = left.toMap();
                  const QVariantMap rightMap = right.toMap();
                  const double leftAverage =
                      leftMap.value(QStringLiteral("placementAverage")).toDouble();
                  const double rightAverage =
                      rightMap.value(QStringLiteral("placementAverage")).toDouble();
                  if (leftAverage != rightAverage) return leftAverage > rightAverage;
                  return leftMap.value(QStringLiteral("name")).toString()
                      < rightMap.value(QStringLiteral("name")).toString();
              });
    for (int index = 0; index < placementEntries.size(); ++index) {
        QVariantMap entry = placementEntries.at(index).toMap();
        entry.insert(QStringLiteral("placementRank"), index + 1);
        placementEntries[index] = entry;
    }

    QVariantList playValueEntries;
    QHash<QString, QVariantMap> entryByNormalizedName;
    for (const QVariant &value : entries) {
        const QVariantMap entry = value.toMap();
        const QString name = entry.value(QStringLiteral("name")).toString();
        entryByNormalizedName.insert(normalizedName(name), entry);
    }
    for (const QJsonValue &value : root.value(QStringLiteral("playvalue")).toArray()) {
        const QJsonObject item = value.toObject();
        const QString nickname = item.value(QStringLiteral("nickname")).toString();
        const QVariantMap host = entryByNormalizedName.value(normalizedName(nickname));
        playValueEntries.push_back(QVariantMap{
            {QStringLiteral("name"), nickname},
            {QStringLiteral("points"), item.value(QStringLiteral("points")).toDouble()},
            {QStringLiteral("bombed"), item.value(QStringLiteral("bombed")).toInt()},
            {QStringLiteral("updatedAt"), item.value(QStringLiteral("updated_at")).toString()},
            {QStringLiteral("roomId"), host.value(QStringLiteral("roomId"))},
            {QStringLiteral("teamName"), host.value(QStringLiteral("teamName"))},
            {QStringLiteral("role"), host.value(QStringLiteral("role"))},
            {QStringLiteral("posterUrl"), host.value(QStringLiteral("posterUrl"))},
            {QStringLiteral("live"), host.value(QStringLiteral("live"), false)},
            {QStringLiteral("rank"), 0},
        });
    }
    std::sort(playValueEntries.begin(), playValueEntries.end(),
              [](const QVariant &left, const QVariant &right) {
                  const QVariantMap leftMap = left.toMap();
                  const QVariantMap rightMap = right.toMap();
                  const double leftPoints = leftMap.value(QStringLiteral("points")).toDouble();
                  const double rightPoints = rightMap.value(QStringLiteral("points")).toDouble();
                  if (leftPoints != rightPoints) return leftPoints > rightPoints;
                  return leftMap.value(QStringLiteral("name")).toString()
                      < rightMap.value(QStringLiteral("name")).toString();
              });
    for (int index = 0; index < playValueEntries.size(); ++index) {
        QVariantMap entry = playValueEntries.at(index).toMap();
        entry.insert(QStringLiteral("rank"), index + 1);
        playValueEntries[index] = entry;
    }

    std::sort(entries.begin(), entries.end(), [](const QVariant &left, const QVariant &right) {
        const QVariantMap leftMap = left.toMap();
        const QVariantMap rightMap = right.toMap();
        const double leftScore = leftMap.value(QStringLiteral("score")).toDouble();
        const double rightScore = rightMap.value(QStringLiteral("score")).toDouble();
        if (leftScore != rightScore) return leftScore > rightScore;
        return leftMap.value(QStringLiteral("name")).toString()
            < rightMap.value(QStringLiteral("name")).toString();
    });
    for (int index = 0; index < entries.size(); ++index) {
        QVariantMap entry = entries.at(index).toMap();
        entry.insert(QStringLiteral("rank"), index + 1);
        entries[index] = entry;
    }

    applyEntries(entries,
                 placementEntries,
                 ::placementColumns(placementSessions),
                 playValueEntries,
                 agg.value(QStringLiteral("voters")).toInt(),
                 agg.value(QStringLiteral("updated")).toString(),
                 root.value(QStringLiteral("v")).toString());
}

void MaoziRankClient::applyEntries(QVariantList entries,
                                   QVariantList placementEntries,
                                   QVariantMap placementColumns,
                                   QVariantList playValueEntries,
                                   int totalVoters,
                                   const QString &updatedAt,
                                   const QString &version)
{
    entries_ = std::move(entries);
    placementEntries_ = std::move(placementEntries);
    placementColumns_ = std::move(placementColumns);
    playValueEntries_ = std::move(playValueEntries);
    totalVoters_ = totalVoters;
    updatedAt_ = updatedAt;
    snapshotVersion_ = version;
    entryIndexByRoomId_.clear();
    entryIndexByName_.clear();
    for (int index = 0; index < entries_.size(); ++index) {
        const QVariantMap entry = entries_.at(index).toMap();
        const QString roomId = entry.value(QStringLiteral("roomId")).toString().trimmed();
        const QString nameKey = normalizedName(
            entry.value(QStringLiteral("name")).toString());
        if (!roomId.isEmpty()) entryIndexByRoomId_.insert(roomId, index);
        if (!nameKey.isEmpty()) entryIndexByName_.insert(nameKey, index);
    }
    errorMessage_.clear();
    lastSyncError_.clear();
    setSyncPending(false);
    setLoading(false);
    emit entriesChanged();
    emit syncStateChanged();
}

void MaoziRankClient::finishError(const QString &message, bool preserveEntries)
{
    timeoutTimer_->stop();
    if (preserveEntries && !entries_.isEmpty()) {
        lastSyncError_ = QStringLiteral("数据可能已过期");
    } else {
        errorMessage_ = message;
        lastSyncError_.clear();
        entries_.clear();
        placementEntries_.clear();
        placementColumns_.clear();
        playValueEntries_.clear();
        entryIndexByRoomId_.clear();
        entryIndexByName_.clear();
        totalVoters_ = 0;
        updatedAt_.clear();
        snapshotVersion_.clear();
    }
    setSyncPending(false);
    setLoading(false);
    emit entriesChanged();
    emit syncStateChanged();
}

void MaoziRankClient::setLoading(bool loading)
{
    if (loading_ == loading) return;
    loading_ = loading;
    emit loadingChanged();
}

void MaoziRankClient::setSyncPending(bool pending)
{
    if (syncPending_ == pending) return;
    syncPending_ = pending;
    emit syncStateChanged();
}

QString MaoziRankClient::posterUrl(const QString &cloudPath)
{
    if (cloudPath.isEmpty()) return {};
    QString directUrl;
    if (cloudPath.startsWith(QStringLiteral("cloud://"))) {
        const int slash = cloudPath.indexOf(QLatin1Char('/'), 8);
        directUrl = slash >= 0 ? kPosterCdn + cloudPath.mid(slash) : QString();
    } else if (cloudPath.startsWith(QStringLiteral("http"))) {
        directUrl = cloudPath;
    } else if (cloudPath.startsWith(QStringLiteral("posters/"))) {
        directUrl = kPosterCdn + QLatin1Char('/') + cloudPath;
    } else {
        directUrl = kPosterCdn + QStringLiteral("/posters/") + cloudPath;
    }
    if (directUrl.endsWith(QStringLiteral(".webp"), Qt::CaseInsensitive)) {
        return directUrl
            + QStringLiteral("?imageMogr2/thumbnail/64x64/format/jpg");
    }
    return directUrl;
}

QString MaoziRankClient::teamName(const QJsonValue &team)
{
    if (team.isNull() || team.isUndefined()) return QStringLiteral("未分队");
    const int index = team.toInt(-1);
    return index >= 0 && index < kTeamNames.size() ? kTeamNames.at(index)
                                                   : QStringLiteral("未分队");
}

QString MaoziRankClient::roleForRoomId(const QString &roomId)
{
    if (kLeaderRoomIds.contains(roomId)) return QStringLiteral("leader");
    if (kCaptainRoomIds.contains(roomId)) return QStringLiteral("captain");
    return QStringLiteral("member");
}

QString MaoziRankClient::canonicalLookupName(const QString &name)
{
    const QString trimmed = name.trimmed();
    return kKnownNameAliases.value(trimmed, trimmed);
}
