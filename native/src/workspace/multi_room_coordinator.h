#pragma once

#include <QHash>
#include <QObject>
#include <QStringList>
#include <QVector>

#include "service/stream_service_protocol.h"
#include "workspace/room_workspace_types.h"

class PlayerSurface;
class StreamgetProcessClient;
class QWidget;

class MultiRoomCoordinator final : public QObject {
    Q_OBJECT

public:
    static constexpr int kMaxRooms = 9;

    explicit MultiRoomCoordinator(StreamgetProcessClient *client,
                                  QWidget *surfaceParent,
                                  QObject *parent = nullptr);
    ~MultiRoomCoordinator() override;

    bool addRoom(const QString &roomId,
                 StreamQuality userQuality = StreamQuality::Auto);
    bool removeRoom(const QString &roomId);
    bool setPrimaryRoom(const QString &roomId);
    RoomCommandResult addRoomDetailed(const QString &roomId,
                                      StreamQuality requestedQuality = StreamQuality::Auto);
    RoomCommandResult removeRoomDetailed(const QString &roomId);
    RoomCommandResult setPrimaryRoomDetailed(const QString &roomId);
    RoomCommandResult setRequestedQuality(const QString &roomId,
                                          StreamQuality requestedQuality);

    int roomCount() const noexcept;
    QString primaryRoomId() const;
    QStringList roomIds() const;
    QString layoutId() const;
    RoomSnapshots roomSnapshots() const;
    StreamQuality userQuality(const QString &roomId) const noexcept;
    StreamQuality effectiveQuality(const QString &roomId) const noexcept;
    RoomSession *sessionForRoom(const QString &roomId) const noexcept;
    PlayerSurface *surfaceForRoom(const QString &roomId) const noexcept;

signals:
    void roomAdded(QString roomId);
    void roomRemoved(QString roomId);
    void layoutChanged(QString layoutId);
    void roomStateChanged(QString roomId);
    void qualityChanged(QString roomId, StreamQuality effectiveQuality);
    void failed(QString roomId, QString errorCode);
    void roomSnapshotsChanged(RoomSnapshots snapshots);

private:
    void recomputeQuality();
    void publishSnapshots();
    void connectSession(RoomSession *session);
    static bool isValidRoomId(const QString &roomId);

    StreamgetProcessClient *client_ = nullptr;
    QWidget *surfaceParent_ = nullptr;
    QVector<QString> order_;
    QHash<QString, RoomSession *> sessions_;
    QString primaryRoomId_;
    QString layoutId_ = QStringLiteral("single");
};
