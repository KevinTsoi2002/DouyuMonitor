#pragma once

#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class UpdateChecker final : public QObject {
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY resultChanged)
    Q_PROPERTY(QUrl releaseUrl READ releaseUrl NOTIFY resultChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY resultChanged)

public:
    enum class State { Idle, Checking, UpToDate, UpdateAvailable, Error };
    Q_ENUM(State)

    explicit UpdateChecker(QString currentVersion,
                           QUrl endpoint = QUrl(QStringLiteral(
                               "https://api.github.com/repos/KevinTsoi2002/DouyuMonitor/releases/latest")),
                           int timeoutMs = 8000,
                           QObject *parent = nullptr);

    State state() const noexcept { return state_; }
    QString currentVersion() const { return currentVersion_; }
    QString latestVersion() const { return latestVersion_; }
    QUrl releaseUrl() const { return releaseUrl_; }
    QString errorMessage() const { return errorMessage_; }

    Q_INVOKABLE void check();

    static QString normalizeVersionTag(const QString &tag);
    static int compareVersions(const QString &left, const QString &right);

signals:
    void stateChanged();
    void resultChanged();

private:
    void setState(State state);
    void finishError(const QString &message);
    void handleReply(QNetworkReply *reply);

    QNetworkAccessManager *manager_ = nullptr;
    QNetworkReply *reply_ = nullptr;
    QTimer *timeoutTimer_ = nullptr;
    QString currentVersion_;
    QUrl endpoint_;
    int timeoutMs_ = 8000;
    State state_ = State::Idle;
    QString latestVersion_;
    QUrl releaseUrl_;
    QString errorMessage_;
};
