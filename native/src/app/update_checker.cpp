#include "app/update_checker.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QTimer>

#include <utility>

namespace {

QString stateMessage(const QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::TimeoutError) return QStringLiteral("检查更新超时");
    return QStringLiteral("检查更新失败，请稍后重试");
}

} // namespace

UpdateChecker::UpdateChecker(QString currentVersion, QUrl endpoint, int timeoutMs, QObject *parent)
    : QObject(parent)
    , manager_(new QNetworkAccessManager(this))
    , timeoutTimer_(new QTimer(this))
    , currentVersion_(normalizeVersionTag(std::move(currentVersion)))
    , endpoint_(endpoint.isValid()
                   ? std::move(endpoint)
                   : QUrl(QStringLiteral(
                         "https://api.github.com/repos/KevinTsoi2002/DouyuMonitor/releases/latest")))
    , timeoutMs_(qMax(timeoutMs, 1))
{
    timeoutTimer_->setSingleShot(true);
    connect(timeoutTimer_, &QTimer::timeout, this, [this] {
        if (reply_ == nullptr) return;
        reply_->abort();
        finishError(QStringLiteral("检查更新超时"));
    });
}

QString UpdateChecker::normalizeVersionTag(const QString &tag)
{
    QString value = tag.trimmed();
    if (value.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) value.remove(0, 1);
    const QStringList parts = value.split(QLatin1Char('.'));
    if (parts.size() != 3) return {};
    for (const QString &part : parts) {
        if (part.isEmpty()) return {};
        for (const QChar c : part) {
            if (!c.isDigit()) return {};
        }
    }
    return value;
}

int UpdateChecker::compareVersions(const QString &left, const QString &right)
{
    const QString normalizedLeft = normalizeVersionTag(left);
    const QString normalizedRight = normalizeVersionTag(right);
    if (normalizedLeft.isEmpty() || normalizedRight.isEmpty()) return 0;
    const QStringList leftParts = normalizedLeft.split(QLatin1Char('.'));
    const QStringList rightParts = normalizedRight.split(QLatin1Char('.'));
    for (int i = 0; i < 3; ++i) {
        const qulonglong leftNumber = leftParts.at(i).toULongLong();
        const qulonglong rightNumber = rightParts.at(i).toULongLong();
        if (leftNumber < rightNumber) return -1;
        if (leftNumber > rightNumber) return 1;
    }
    return 0;
}

void UpdateChecker::check()
{
    if (reply_ != nullptr) {
        QNetworkReply *staleReply = reply_;
        reply_ = nullptr;
        timeoutTimer_->stop();
        staleReply->abort();
        staleReply->deleteLater();
    }
    latestVersion_.clear();
    releaseUrl_ = {};
    errorMessage_.clear();
    emit resultChanged();
    setState(State::Checking);

    QNetworkRequest request(endpoint_);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("DouyuMonitor/%1").arg(currentVersion_));
    request.setRawHeader("Accept", "application/vnd.github+json");
    reply_ = manager_->get(request);
    QNetworkReply *requestReply = reply_;
    connect(requestReply, &QNetworkReply::finished, this, [this, requestReply] {
        if (reply_ != requestReply) {
            // The replacement path already scheduled this stale reply for deletion.
            return;
        }
        reply_ = nullptr;
        timeoutTimer_->stop();
        handleReply(requestReply);
        requestReply->deleteLater();
    });
    timeoutTimer_->start(timeoutMs_);
}

void UpdateChecker::setState(State state)
{
    if (state_ == state) return;
    state_ = state;
    emit stateChanged();
}

void UpdateChecker::finishError(const QString &message)
{
    if (reply_ != nullptr) {
        reply_->deleteLater();
        reply_ = nullptr;
    }
    timeoutTimer_->stop();
    errorMessage_ = message;
    emit resultChanged();
    setState(State::Error);
}

void UpdateChecker::handleReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        finishError(stateMessage(reply));
        return;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status < 200 || status >= 300) {
        finishError(QStringLiteral("检查更新失败，请稍后重试"));
        return;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        finishError(QStringLiteral("更新信息格式无效"));
        return;
    }
    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("draft")).toBool() || object.value(QStringLiteral("prerelease")).toBool()) {
        finishError(QStringLiteral("未找到正式版本"));
        return;
    }
    const QString latest = normalizeVersionTag(object.value(QStringLiteral("tag_name")).toString());
    const QUrl url(object.value(QStringLiteral("html_url")).toString());
    if (latest.isEmpty() || !url.isValid() || url.scheme() != QStringLiteral("https")) {
        finishError(QStringLiteral("更新信息格式无效"));
        return;
    }
    latestVersion_ = latest;
    releaseUrl_ = url;
    emit resultChanged();
    setState(compareVersions(latest, currentVersion_) > 0 ? State::UpdateAvailable : State::UpToDate);
}
