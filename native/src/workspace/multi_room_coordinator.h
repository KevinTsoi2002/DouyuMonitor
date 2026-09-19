#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVector>

#include "service/stream_service_protocol.h"
#include "workspace/room_status_scheduler.h"
#include "workspace/room_workspace_types.h"

class MpvQuickItem;
class StreamgetProcessClient;

struct CoordinatorRoomSpec {
    QString roomId;
    StreamQuality requestedQuality = StreamQuality::Auto;
    int requestedQualityRate = -1;
    RoomMetadata metadata;
    int volume = 100;
    bool favorite = false;
};

class MultiRoomCoordinator final : public QObject {
    Q_OBJECT

public:
    static constexpr int kMaxRooms = 10;

    explicit MultiRoomCoordinator(StreamgetProcessClient *client,
                                  QObject *parent = nullptr);
    MultiRoomCoordinator(StreamgetProcessClient *client,
                         RoomRefreshTiming timing,
                         QObject *parent = nullptr);
    ~MultiRoomCoordinator() override;

    bool addRoom(const QString &roomId,
                 StreamQuality userQuality = StreamQuality::Auto);
    bool removeRoom(const QString &roomId);
    bool setPrimaryRoom(const QString &roomId);
    bool setSecondaryPrimaryRoom(const QString &roomId);
    bool setAudioFocus(const QString &roomId);
    bool setRoomMuted(const QString &roomId, bool muted);
    bool setFavorite(const QString &roomId, bool favorite);
    QString audioRoomId() const;
    QString audioMode() const;
    bool globalMuted() const noexcept;
    bool setAudioMode(const QString &mode);
    bool setGlobalMuted(bool muted);
    RoomCommandResult addRoomDetailed(const QString &roomId,
                                      StreamQuality requestedQuality = StreamQuality::Auto,
                                      int requestedQualityRate = -1,
                                      RoomMetadata metadata = {},
                                      bool favorite = false,
                                      int volume = 100);
    RoomCommandResult removeRoomDetailed(const QString &roomId);
    RoomCommandResult setPrimaryRoomDetailed(const QString &roomId);
    RoomCommandResult setSecondaryPrimaryRoomDetailed(const QString &roomId);
    RoomCommandResult setRequestedQuality(const QString &roomId,
                                          StreamQuality requestedQuality,
                                          int requestedQualityRate = -1);
    RoomCommandResult moveRoomDetailed(const QString &roomId, int delta);
    RoomCommandResult retryRoomDetailed(const QString &roomId);
    RoomCommandResult setVolume(const QString &roomId, int volume);
    RoomCommandResult replaceRooms(const QVector<CoordinatorRoomSpec> &rooms);

    int roomCount() const noexcept;
    QString primaryRoomId() const;
    QString secondaryPrimaryRoomId() const;
    QStringList roomIds() const;
    QString layoutId() const;
    QString layoutMode() const;
    double primaryRoomRatio() const noexcept;
    Q_INVOKABLE bool setLayout(const QString &layoutId);
    Q_INVOKABLE bool setPrimaryRoomRatio(double ratio);
    RoomSnapshots roomSnapshots() const;
    StreamQuality userQuality(const QString &roomId) const noexcept;
    StreamQuality effectiveQuality(const QString &roomId) const noexcept;
    RoomSession *sessionForRoom(const QString &roomId) const noexcept;
    bool attachPlayer(const QString &roomId, MpvQuickItem *player);
    void detachPlayer(const QString &roomId, MpvQuickItem *player);
    MpvQuickItem *playerForRoom(const QString &roomId) const noexcept;
    void refreshRoomStatusNow(const QString &roomId);
    void suspendRendering();
    void resumeRendering();

signals:
    void roomAdded(QString roomId);
    void roomRemoved(QString roomId);
    void layoutChanged(QString layoutId);
    void roomStateChanged(QString roomId);
    void qualityChanged(QString roomId, StreamQuality effectiveQuality);
    void qualityRateChanged(QString roomId, int effectiveQualityRate);
    void roomStatusRefreshed(QString roomId, bool online);
    void failed(QString roomId, QString errorCode);
    void roomSnapshotsChanged(RoomSnapshots snapshots);

private:
    void recomputeQuality();
    void applyAudioFocus();
    void publishSnapshots();
    void connectSession(RoomSession *session);
    void onResponse(ServiceResponse response);
    void onRequestFailed(quint64 requestId, QString errorCode);
    static bool isValidRoomId(const QString &roomId);

    StreamgetProcessClient *client_ = nullptr;
    QVector<QString> order_;
    QHash<QString, RoomSession *> sessions_;
    QString primaryRoomId_;
    QString secondaryPrimaryRoomId_;
    QString audioRoomId_;
    QString audioMode_ = QStringLiteral("single");
    bool globalMuted_ = false;
    QSet<QString> mutedRooms_;
    QString layoutId_ = QStringLiteral("auto");
    QString layoutMode_ = QStringLiteral("auto");
    double primaryRoomRatio_ = 0.6;
    RoomStatusScheduler scheduler_;
};
