#pragma once

#include <QMainWindow>
#include <QString>
#include <QStringList>

#include "service/stream_service_protocol.h"

class MultiRoomCoordinator;
class PlayerSurface;
class QDockWidget;
class QGridLayout;
class RoomManagementDock;
class StreamgetProcessClient;
class QToolButton;
class QWidget;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    explicit MainWindow(const QString &serviceProgram, QWidget *parent = nullptr);
    ~MainWindow() override;

    PlayerSurface *playerSurface() const noexcept;
    bool loadLocalMedia(const QString &path);
    QToolButton *pauseButton() const noexcept;
    bool addRoom(const QString &roomId, StreamQuality userQuality = StreamQuality::Auto);
    bool removeRoom(const QString &roomId);
    bool setPrimaryRoom(const QString &roomId);
    int roomCount() const noexcept;
    QString layoutId() const;
    QStringList roomIds() const;
    PlayerSurface *surfaceForRoom(const QString &roomId) const noexcept;
    RoomManagementDock *roomManagementDock() const noexcept;

private:
    void rebuildGrid();
    void synchronizeWorkspace();
    void updatePauseButtonIcon();
    void synchronizePauseButton(bool paused);

    QWidget *gridHost_ = nullptr;
    QGridLayout *gridLayout_ = nullptr;
    PlayerSurface *compatibilitySurface_ = nullptr;
    StreamgetProcessClient *streamClient_ = nullptr;
    MultiRoomCoordinator *coordinator_ = nullptr;
    QDockWidget *roomDockHost_ = nullptr;
    RoomManagementDock *roomManagementDock_ = nullptr;
    QToolButton *pauseButton_ = nullptr;
    QToolButton *stopButton_ = nullptr;
};
