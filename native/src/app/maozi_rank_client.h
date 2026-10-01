#pragma once

#include <QObject>
#include <QHash>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonValue>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

class MaoziRankClient final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QVariantList placementEntries READ placementEntries NOTIFY entriesChanged)
    Q_PROPERTY(QVariantMap placementColumns READ placementColumns NOTIFY entriesChanged)
    Q_PROPERTY(QVariantList playValueEntries READ playValueEntries NOTIFY entriesChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY entriesChanged)
    Q_PROPERTY(int totalVoters READ totalVoters NOTIFY entriesChanged)
    Q_PROPERTY(QString updatedAt READ updatedAt NOTIFY entriesChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY entriesChanged)
    Q_PROPERTY(bool syncPending READ syncPending NOTIFY syncStateChanged)
    Q_PROPERTY(QString lastSyncError READ lastSyncError NOTIFY syncStateChanged)

public:
    explicit MaoziRankClient(QUrl authEndpoint = {},
                             QUrl snapshotEndpoint = {},
                             int timeoutMs = 15000,
                             QObject *parent = nullptr);

    QVariantList entries() const { return entries_; }
    QVariantList placementEntries() const { return placementEntries_; }
    QVariantMap placementColumns() const { return placementColumns_; }
    QVariantList playValueEntries() const { return playValueEntries_; }
    bool loading() const noexcept { return loading_; }
    QString errorMessage() const { return errorMessage_; }
    int totalVoters() const noexcept { return totalVoters_; }
    QString updatedAt() const { return updatedAt_; }
    QString statusText() const;
    bool syncPending() const noexcept { return syncPending_; }
    QString snapshotVersion() const { return snapshotVersion_; }
    QString lastSyncError() const { return lastSyncError_; }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void checkForChanges();
    Q_INVOKABLE QVariantMap entryForRoomId(const QString &roomId) const;
    Q_INVOKABLE QVariantMap entryForName(const QString &name) const;

    static double placementScore(int competitorCount, int rank);

signals:
    void entriesChanged();
    void loadingChanged();
    void syncStateChanged();

private:
    enum class PendingAuthAction {
        None,
        Snapshot,
        VersionCheck,
    };

    void requestAnonymousSession(PendingAuthAction action);
    void requestSnapshot(const QString &accessToken);
    void requestVersionCheck(const QString &accessToken);
    void finishError(const QString &message, bool preserveEntries = false);
    void handleAuthReply(QNetworkReply *reply);
    void handleSnapshotReply(QNetworkReply *reply);
    void handleVersionReply(QNetworkReply *reply);
    void parseSnapshot(const QByteArray &payload);
    void applyEntries(QVariantList entries,
                      QVariantList placementEntries,
                      QVariantMap placementColumns,
                      QVariantList playValueEntries,
                      int totalVoters,
                      const QString &updatedAt,
                      const QString &version);
    void setLoading(bool loading);
    void setSyncPending(bool pending);
    static QString posterUrl(const QString &cloudPath);
    static QString teamName(const QJsonValue &team);
    static QString roleForRoomId(const QString &roomId);
    static QString canonicalLookupName(const QString &name);

    QNetworkAccessManager *manager_ = nullptr;
    QTimer *timeoutTimer_ = nullptr;
    QNetworkReply *reply_ = nullptr;
    QUrl authEndpoint_;
    QUrl snapshotEndpoint_;
    QUrl versionEndpoint_;
    int timeoutMs_ = 15000;
    PendingAuthAction pendingAuthAction_ = PendingAuthAction::None;
    QString accessToken_;
    QVariantList entries_;
    QVariantList placementEntries_;
    QVariantMap placementColumns_;
    QVariantList playValueEntries_;
    QHash<QString, int> entryIndexByRoomId_;
    QHash<QString, int> entryIndexByName_;
    bool loading_ = false;
    bool syncPending_ = false;
    QString errorMessage_;
    QString lastSyncError_;
    int totalVoters_ = 0;
    QString updatedAt_;
    QString snapshotVersion_;
};
