#include "app/main_window.h"

#include <QCoreApplication>
#include <QDockWidget>
#include <QDir>
#include <QGridLayout>
#include <QSignalBlocker>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QWidget>

#include "media/media_source.h"
#include "media/player_surface.h"
#include "app/room_management_dock.h"
#include "service/streamget_process_client.h"
#include "workspace/multi_room_coordinator.h"

MainWindow::MainWindow(QWidget *parent)
    : MainWindow(QDir(QCoreApplication::applicationDirPath())
                     .filePath(QStringLiteral("streamget_service.exe")), parent)
{
}

MainWindow::MainWindow(const QString &serviceProgram, QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Douyu Monitor"));

    gridHost_ = new QWidget(this);
    gridLayout_ = new QGridLayout(gridHost_);
    gridLayout_->setContentsMargins(0, 0, 0, 0);
    gridLayout_->setSpacing(2);
    setCentralWidget(gridHost_);

    compatibilitySurface_ = new PlayerSurface(gridHost_);
    compatibilitySurface_->setMinimumSize(160, 90);
    gridLayout_->addWidget(compatibilitySurface_, 0, 0);

    streamClient_ = new StreamgetProcessClient(serviceProgram, {}, this);
    coordinator_ = new MultiRoomCoordinator(streamClient_, gridHost_, this);

    roomDockHost_ = new QDockWidget(QStringLiteral("Room Management"), this);
    roomDockHost_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    roomManagementDock_ = new RoomManagementDock(roomDockHost_);
    roomDockHost_->setWidget(roomManagementDock_);
    addDockWidget(Qt::RightDockWidgetArea, roomDockHost_);

    connect(coordinator_, &MultiRoomCoordinator::roomSnapshotsChanged,
            this, [this](const RoomSnapshots &) { synchronizeWorkspace(); });

    const auto showResult = [this](RoomCommandResult result) {
        if (roomManagementDock_ != nullptr) roomManagementDock_->setCommandResult(result);
    };
    connect(roomManagementDock_, &RoomManagementDock::addRequested, this,
            [this, showResult](const QString &roomId) {
                showResult(coordinator_->addRoomDetailed(roomId));
            });
    connect(roomManagementDock_, &RoomManagementDock::removeRequested, this,
            [this, showResult](const QString &roomId) {
                showResult(coordinator_->removeRoomDetailed(roomId));
            });
    connect(roomManagementDock_, &RoomManagementDock::primaryRequested, this,
            [this, showResult](const QString &roomId) {
                showResult(coordinator_->setPrimaryRoomDetailed(roomId));
            });
    connect(roomManagementDock_, &RoomManagementDock::requestedQualityChanged, this,
            [this, showResult](const QString &roomId, StreamQuality quality) {
                showResult(coordinator_->setRequestedQuality(roomId, quality));
            });
    synchronizeWorkspace();

    resize(1280, 720);

    auto *toolbar = addToolBar(QStringLiteral("Playback"));
    toolbar->setMovable(false);
    toolbar->setFloatable(false);

    pauseButton_ = new QToolButton(toolbar);
    pauseButton_->setCheckable(true);
    pauseButton_->setAutoRaise(true);
    pauseButton_->setToolTip(QStringLiteral("Pause or resume playback"));
    toolbar->addWidget(pauseButton_);
    updatePauseButtonIcon();

    stopButton_ = new QToolButton(toolbar);
    stopButton_->setObjectName(QStringLiteral("stopButton"));
    stopButton_->setAutoRaise(true);
    stopButton_->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    stopButton_->setToolTip(QStringLiteral("Stop playback"));
    toolbar->addWidget(stopButton_);

    connect(pauseButton_, &QToolButton::toggled, this, [this](bool paused) {
        auto *surface = playerSurface();
        if (surface == nullptr || !surface->setPaused(paused)) {
            synchronizePauseButton(!paused);
            return;
        }
        synchronizePauseButton(paused);
    });

    connect(stopButton_, &QToolButton::clicked, this, [this] {
        auto *surface = playerSurface();
        if (surface == nullptr || !surface->stop()) {
            return;
        }

        surface->setPaused(false);
        synchronizePauseButton(false);
    });
}

MainWindow::~MainWindow()
{
    if (coordinator_ != nullptr) {
        delete coordinator_;
        coordinator_ = nullptr;
    }
    if (streamClient_ != nullptr) {
        streamClient_->shutdown(250);
    }
}

PlayerSurface *MainWindow::playerSurface() const noexcept
{
    if (coordinator_ != nullptr && coordinator_->roomCount() > 0) {
        return coordinator_->surfaceForRoom(coordinator_->primaryRoomId());
    }
    return compatibilitySurface_;
}

bool MainWindow::loadLocalMedia(const QString &path)
{
    const auto source = MediaSource::fromDescriptor(path);
    auto *surface = playerSurface();
    if (!source.has_value() || surface == nullptr) {
        return false;
    }

    if (!surface->loadLocalMedia(source->localPath())) {
        return false;
    }

    if (!surface->setPaused(false)) {
        return false;
    }

    synchronizePauseButton(false);
    return true;
}

QToolButton *MainWindow::pauseButton() const noexcept
{
    return pauseButton_;
}

bool MainWindow::addRoom(const QString &roomId, StreamQuality userQuality)
{
    if (coordinator_ == nullptr) return false;
    return coordinator_->addRoom(roomId, userQuality);
}

bool MainWindow::removeRoom(const QString &roomId)
{
    if (coordinator_ == nullptr) return false;
    return coordinator_->removeRoom(roomId);
}

bool MainWindow::setPrimaryRoom(const QString &roomId)
{
    if (coordinator_ == nullptr) return false;
    return coordinator_->setPrimaryRoom(roomId);
}

int MainWindow::roomCount() const noexcept
{
    return coordinator_ != nullptr ? coordinator_->roomCount() : 0;
}

QString MainWindow::layoutId() const
{
    return coordinator_ != nullptr ? coordinator_->layoutId() : QStringLiteral("single");
}

QStringList MainWindow::roomIds() const
{
    return coordinator_ != nullptr ? coordinator_->roomIds() : QStringList();
}

PlayerSurface *MainWindow::surfaceForRoom(const QString &roomId) const noexcept
{
    return coordinator_ != nullptr ? coordinator_->surfaceForRoom(roomId) : nullptr;
}

RoomManagementDock *MainWindow::roomManagementDock() const noexcept
{
    return roomManagementDock_;
}

void MainWindow::synchronizeWorkspace()
{
    rebuildGrid();
    if (coordinator_ != nullptr && roomManagementDock_ != nullptr) {
        roomManagementDock_->setRooms(coordinator_->roomSnapshots());
    }
}

void MainWindow::rebuildGrid()
{
    if (gridLayout_ == nullptr) return;

    while (QLayoutItem *item = gridLayout_->takeAt(0)) {
        if (item->widget() != nullptr) item->widget()->setParent(gridHost_);
        delete item;
    }

    if (coordinator_ == nullptr || coordinator_->roomCount() == 0) {
        compatibilitySurface_->show();
        gridLayout_->addWidget(compatibilitySurface_, 0, 0);
        return;
    }

    const QStringList ids = coordinator_->roomIds();
    const int columns = ids.size() <= 1 ? 1 : ids.size() <= 4 ? 2 : 3;
    for (int index = 0; index < ids.size(); ++index) {
        auto *surface = coordinator_->surfaceForRoom(ids.at(index));
        if (surface == nullptr) continue;
        surface->setMinimumSize(120, 68);
        gridLayout_->addWidget(surface, index / columns, index % columns);
    }
    compatibilitySurface_->hide();
}

void MainWindow::updatePauseButtonIcon()
{
    if (pauseButton_ == nullptr) {
        return;
    }

    const auto standardPixmap = pauseButton_->isChecked()
        ? QStyle::SP_MediaPause
        : QStyle::SP_MediaPlay;
    pauseButton_->setIcon(style()->standardIcon(standardPixmap));
}

void MainWindow::synchronizePauseButton(bool paused)
{
    if (pauseButton_ == nullptr) {
        return;
    }

    const QSignalBlocker blocker(pauseButton_);
    pauseButton_->setChecked(paused);
    updatePauseButtonIcon();
}
