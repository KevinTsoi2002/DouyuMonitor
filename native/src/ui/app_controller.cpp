#include "ui/app_controller.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QWindow>

#include <algorithm>
#include <memory>

#include "app/windows_notification_service.h"
#include "danmaku/danmaku_socket.h"
#include "danmaku/danmaku_timer_scheduler.h"
#include "service/streamget_process_client.h"
#include "ui/monitoring_model.h"
#include "ui/mpv_quick_item.h"
#include "ui/room_list_model.h"
#include "ui/workspace_model.h"
#include "workspace/multi_room_coordinator.h"

namespace {

QString generatedId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString normalizedSearchQuery(const QString &raw)
{
    const QString value = raw.trimmed();
    static const QRegularExpression roomIdPattern(QStringLiteral(R"(^[0-9]{1,20}$)"));
    if (roomIdPattern.match(value).hasMatch()) return value;

    const QUrl url(value);
    if (url.isValid() && !url.host().isEmpty()) {
        const QString host = url.host().toLower();
        if (host == QStringLiteral("douyu.com") || host.endsWith(QStringLiteral(".douyu.com"))) {
            const QStringList segments = url.path().split('/', Qt::SkipEmptyParts);
            if (!segments.isEmpty() && roomIdPattern.match(segments.first()).hasMatch()) {
                return segments.first();
            }
        }
    }
    return value;
}

} // namespace

AppController::AppController(QString serviceProgram,
                             QSettings *settings,
                             SystemNotificationSink *notificationSink,
                             DanmakuClientFactory danmakuFactory,
                             QObject *parent)
    : QObject(parent)
    , settings_(settings)
    , workspaceStore_(settings)
    , notificationPolicy_([] { return QDateTime::currentMSecsSinceEpoch(); })
{
    if (!danmakuFactory) {
        danmakuFactory = [](const QString &roomId, QObject *parent) {
            auto *scheduler = new QtDanmakuTimerScheduler(parent);
            auto socket = std::make_unique<QtDanmakuSocket>(parent);
            return std::unique_ptr<DanmakuClient>(
                new DouyuDanmakuClient(roomId, std::move(socket), scheduler, parent));
        };
    }
    service_ = std::make_unique<StreamgetProcessClient>(std::move(serviceProgram), QStringList{}, this);
    coordinator_ = std::make_unique<MultiRoomCoordinator>(service_.get(), this);
    notificationService_ = std::make_unique<WindowsNotificationService>(settings, notificationSink, this);
    rooms_ = std::make_unique<RoomListModel>(this);
    workspace_ = std::make_unique<WorkspaceModel>(this, this);
    monitoring_ = std::make_unique<MonitoringModel>(this);
    danmaku_ = std::make_unique<DanmakuController>(std::move(danmakuFactory), this);

    connect(service_.get(), &StreamgetProcessClient::responseReceived,
            this, &AppController::onServiceResponse);
    connect(service_.get(), &StreamgetProcessClient::requestFailed,
            this, &AppController::onServiceRequestFailed);

    connect(coordinator_.get(), &MultiRoomCoordinator::roomSnapshotsChanged,
            this, &AppController::onSnapshotsChanged);
    connect(coordinator_.get(), &MultiRoomCoordinator::failed, this,
            [this](const QString &, const QString &errorCode) {
                workspace_->setLastMessage(fixedPlaybackMessage(errorCode),
                                            QStringLiteral("error"),
                                            0);
            });
    connect(coordinator_.get(), &MultiRoomCoordinator::roomStatusRefreshed,
            this, &AppController::onRoomStatusRefreshed);
    connect(danmaku_.get(), &DanmakuController::settingsChanged, this, [this] {
        snapshot_.danmaku = danmaku_->configuration();
        synchronizeDanmaku();
        refreshPresentation();
        persistWorkspace();
    });
    connect(danmaku_.get(), &DanmakuController::roomStateChanged,
            this, [this](const QString &) { refreshPresentation(); });
    restoreWorkspace();
    const auto notificationStatus = notificationService_->preferences().enabled
        ? MonitoringModel::NotificationStatus::Enabled
        : MonitoringModel::NotificationStatus::Disabled;
    monitoring_->setStatus(notificationStatus, MonitoringModel::RecoveryStatus::Healthy);
}

AppController::~AppController()
{
    shutdown();
}

void AppController::shutdown()
{
    if (shuttingDown_) return;
    shuttingDown_ = true;

    const quint64 pendingSearch = searchRequestId_;
    searchRequestId_ = 0;
    if (pendingSearch != 0 && service_ != nullptr) service_->cancel(pendingSearch);
    persistWorkspace();
    if (danmaku_ != nullptr) danmaku_->stopAll();
    coordinator_.reset();
    if (service_ != nullptr) {
        service_->shutdown();
        service_.reset();
    }
    notificationService_.reset();
}

RoomListModel *AppController::rooms() noexcept
{
    return rooms_.get();
}

WorkspaceModel *AppController::workspace() noexcept
{
    return workspace_.get();
}

MonitoringModel *AppController::monitoring() noexcept
{
    return monitoring_.get();
}

DanmakuController *AppController::danmaku() noexcept
{
    return danmaku_.get();
}

QVariantList AppController::libraryRooms() const
{
    QVector<const NativeRoomRecord *> records;
    records.reserve(snapshot_.library.size());
    for (const NativeRoomRecord &record : snapshot_.library) records.push_back(&record);

    std::sort(records.begin(), records.end(), [](const NativeRoomRecord *left,
                                                 const NativeRoomRecord *right) {
        if (left->lastOpenedAtMs != right->lastOpenedAtMs) {
            return left->lastOpenedAtMs > right->lastOpenedAtMs;
        }
        return left->roomId < right->roomId;
    });

    QVariantList projection;
    projection.reserve(records.size());
    for (const NativeRoomRecord *record : records) {
        const RoomMetadata &metadata = record->metadata;
        projection.push_back(QVariantMap{
            {QStringLiteral("roomId"), record->roomId},
            {QStringLiteral("anchorName"), metadata.anchorName},
            {QStringLiteral("title"), metadata.title},
            {QStringLiteral("category"), metadata.category},
            {QStringLiteral("viewerLabel"), metadata.viewerLabel},
            {QStringLiteral("avatarUrl"), metadata.avatarUrl},
            {QStringLiteral("favorite"), record->favorite},
            {QStringLiteral("lastOpenedAtMs"), record->lastOpenedAtMs},
            {QStringLiteral("active"), snapshot_.activeRoomIds.contains(record->roomId)},
        });
    }
    return projection;
}

QVariantMap AppController::notificationPreferences() const
{
    const NotificationPreferences preferences = notificationService_->preferences();
    return {
        {QStringLiteral("enabled"), preferences.enabled},
        {QStringLiteral("roomOnline"), preferences.roomOnline},
        {QStringLiteral("roomOffline"), preferences.roomOffline},
        {QStringLiteral("playbackFailed"), preferences.playbackFailed},
        {QStringLiteral("playbackRecovered"), preferences.playbackRecovered},
    };
}

QVariantList AppController::searchResults() const
{
    return searchResults_;
}

QString AppController::searchStatus() const
{
    return searchStatus_;
}

QString AppController::searchError() const
{
    return searchError_;
}

QString AppController::fixedPlaybackMessage(const QString &) const
{
    return QStringLiteral("播放地址不可用");
}

void AppController::setMainWindow(QWindow *window)
{
    mainWindow_ = window;
}

QString AppController::addRoom(const QString &roomId)
{
    const NativeRoomRecord *record = libraryRecord(roomId);
    const RoomCommandResult result = coordinator_->addRoomDetailed(
        roomId,
        record != nullptr ? record->requestedQuality : StreamQuality::Auto,
        record != nullptr ? record->metadata : RoomMetadata{},
        record != nullptr && record->favorite,
        record != nullptr ? record->volume : 100);
    if (result == RoomCommandResult::Accepted) {
        touchHistory(roomId);
        persistWorkspace();
    }
    return commandMessage(result);
}

void AppController::searchRooms(const QString &query)
{
    const quint64 previousRequest = searchRequestId_;
    searchRequestId_ = 0;
    if (previousRequest != 0 && service_ != nullptr) service_->cancel(previousRequest);

    searchCandidates_.clear();
    searchResults_.clear();
    searchError_.clear();
    searchStatus_ = QStringLiteral("idle");
    emit searchResultsChanged();
    emit searchStateChanged();

    const QString normalized = normalizedSearchQuery(query);
    if (normalized.isEmpty()) {
        searchStatus_ = QStringLiteral("error");
        searchError_ = QStringLiteral("请输入直播间号、斗鱼链接或主播名字");
        emit searchStateChanged();
        return;
    }
    if (normalized.size() > 200 || service_ == nullptr) {
        searchStatus_ = QStringLiteral("error");
        searchError_ = normalized.size() > 200
            ? QStringLiteral("搜索内容过长")
            : QStringLiteral("搜索服务不可用");
        emit searchStateChanged();
        return;
    }

    searchStatus_ = QStringLiteral("searching");
    emit searchStateChanged();
    searchRequestId_ = service_->search(normalized);
}

QString AppController::addRoomCandidate(const QString &roomId)
{
    const auto candidate = searchCandidates_.constFind(roomId);
    if (candidate == searchCandidates_.cend()) return addRoom(roomId);

    const NativeRoomRecord *record = libraryRecord(roomId);
    const RoomCommandResult result = coordinator_->addRoomDetailed(
        roomId,
        record != nullptr ? record->requestedQuality : StreamQuality::Auto,
        candidate.value(),
        record != nullptr && record->favorite,
        record != nullptr ? record->volume : 100);
    if (result == RoomCommandResult::Accepted) {
        touchHistory(roomId);
        persistWorkspace();
    }
    return commandMessage(result);
}

QString AppController::removeRoom(const QString &roomId)
{
    const RoomCommandResult result = coordinator_->removeRoomDetailed(roomId);
    if (result == RoomCommandResult::Accepted) persistWorkspace();
    return commandMessage(result);
}

void AppController::requestRemoveRoom(const QString &roomId)
{
    const QString requestedRoomId = roomId.trimmed();
    if (requestedRoomId.isEmpty()) return;

    QTimer::singleShot(0, this, [this, requestedRoomId] {
        removeRoom(requestedRoomId);
    });
}

QString AppController::setPrimaryRoom(const QString &roomId)
{
    const RoomCommandResult result = coordinator_->setPrimaryRoomDetailed(roomId);
    if (result == RoomCommandResult::Accepted) persistWorkspace();
    return commandMessage(result);
}

QString AppController::setAudioRoom(const QString &roomId)
{
    if (coordinator_->sessionForRoom(roomId) == nullptr) {
        return commandMessage(RoomCommandResult::RoomNotFound);
    }
    if (coordinator_->audioRoomId() == roomId) return {};
    coordinator_->setAudioFocus(roomId);
    persistWorkspace();
    return {};
}

bool AppController::setAudioMode(const QString &mode)
{
    if (coordinator_ == nullptr || !coordinator_->setAudioMode(mode)) return false;
    snapshot_.audioMode = coordinator_->audioMode();
    refreshPresentation();
    persistWorkspace();
    return true;
}

bool AppController::setGlobalMuted(bool muted)
{
    if (coordinator_ == nullptr || !coordinator_->setGlobalMuted(muted)) return false;
    snapshot_.globalMuted = coordinator_->globalMuted();
    refreshPresentation();
    persistWorkspace();
    return true;
}

QString AppController::setQuality(const QString &roomId, int quality)
{
    if (!isValidQuality(quality)) return commandMessage(RoomCommandResult::InvalidRoomId);
    const RoomCommandResult result = coordinator_->setRequestedQuality(
        roomId, static_cast<StreamQuality>(quality));
    if (result == RoomCommandResult::Accepted) persistWorkspace();
    return commandMessage(result);
}

QString AppController::setFavorite(const QString &roomId, bool favorite)
{
    if (coordinator_->sessionForRoom(roomId) == nullptr) {
        return commandMessage(RoomCommandResult::RoomNotFound);
    }
    if (!coordinator_->setFavorite(roomId, favorite)) return {};
    persistWorkspace();
    return {};
}

QString AppController::moveRoom(const QString &roomId, int delta)
{
    const RoomCommandResult result = coordinator_->moveRoomDetailed(roomId, delta);
    if (result == RoomCommandResult::Accepted) persistWorkspace();
    return commandMessage(result);
}

QString AppController::retryPlayback(const QString &roomId)
{
    const QString message = commandMessage(coordinator_->retryRoomDetailed(roomId));
    if (!message.isEmpty()) {
        workspace_->setLastMessage(message, QStringLiteral("error"), 0);
    } else {
        workspace_->setLastMessage(QStringLiteral("正在检查播放源"),
                                   QStringLiteral("info"),
                                   1800);
    }
    return message;
}

QString AppController::setVolume(const QString &roomId, int volume)
{
    if (volume < 0 || volume > 100) return commandMessage(RoomCommandResult::InvalidRoomId);
    const RoomCommandResult result = coordinator_->setVolume(roomId, volume);
    if (result == RoomCommandResult::Accepted) persistWorkspace();
    return commandMessage(result);
}

void AppController::toggleDanmaku(const QString &roomId)
{
    NativeRoomRecord *record = libraryRecord(roomId);
    if (record == nullptr || coordinator_->sessionForRoom(roomId) == nullptr) return;
    record->danmakuEnabled = !record->danmakuEnabled;
    synchronizeDanmaku();
    refreshPresentation();
    persistWorkspace();
}

QString AppController::createGroup(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (!isValidName(trimmed)) return QStringLiteral("请输入 1 到 30 个字符的分组名称");
    snapshot_.groups.push_back({generatedId(), trimmed, {}});
    refreshPresentation();
    persistWorkspace();
    return snapshot_.groups.back().id;
}

QString AppController::renameGroup(const QString &groupId, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (!isValidName(trimmed)) return QStringLiteral("请输入 1 到 30 个字符的分组名称");
    for (NativeRoomGroup &group : snapshot_.groups) {
        if (group.id != groupId) continue;
        if (group.name == trimmed) return {};
        group.name = trimmed;
        refreshPresentation();
        persistWorkspace();
        return {};
    }
    return commandMessage(RoomCommandResult::RoomNotFound);
}

QString AppController::deleteGroup(const QString &groupId)
{
    const auto it = std::find_if(snapshot_.groups.cbegin(), snapshot_.groups.cend(),
                                 [&groupId](const NativeRoomGroup &group) {
                                     return group.id == groupId;
                                 });
    if (it == snapshot_.groups.cend()) return commandMessage(RoomCommandResult::RoomNotFound);
    snapshot_.groups.erase(it);
    if (snapshot_.activeGroupId == groupId) snapshot_.activeGroupId.clear();
    for (NativeWorkspacePreset &preset : snapshot_.presets) {
        if (preset.activeGroupId == groupId) preset.activeGroupId.clear();
    }
    refreshPresentation();
    persistWorkspace();
    return {};
}

QString AppController::assignRoomToGroup(const QString &roomId, const QString &groupId)
{
    if (libraryRecord(roomId) == nullptr) return commandMessage(RoomCommandResult::RoomNotFound);
    auto target = std::find_if(snapshot_.groups.begin(), snapshot_.groups.end(),
                               [&groupId](const NativeRoomGroup &group) {
                                   return group.id == groupId;
                               });
    if (target == snapshot_.groups.end()) return commandMessage(RoomCommandResult::RoomNotFound);
    const bool wasActive = snapshot_.activeGroupId == groupId;
    for (NativeRoomGroup &group : snapshot_.groups) group.roomIds.removeAll(roomId);
    target->roomIds.push_back(roomId);
    if (wasActive) {
        setActiveGroup(groupId);
    } else {
        refreshPresentation();
        persistWorkspace();
    }
    return {};
}

QString AppController::removeRoomFromGroup(const QString &groupId, const QString &roomId)
{
    auto target = std::find_if(snapshot_.groups.begin(), snapshot_.groups.end(),
                               [&groupId](const NativeRoomGroup &group) {
                                   return group.id == groupId;
                               });
    if (target == snapshot_.groups.end()) return commandMessage(RoomCommandResult::RoomNotFound);
    const int index = target->roomIds.indexOf(roomId);
    if (index < 0) return {};
    const bool wasActive = snapshot_.activeGroupId == groupId;
    target->roomIds.removeAt(index);
    if (wasActive) {
        setActiveGroup(groupId);
    } else {
        refreshPresentation();
        persistWorkspace();
    }
    return {};
}

QString AppController::moveRoomInGroup(const QString &groupId,
                                       const QString &roomId,
                                       int delta)
{
    if (delta == 0) return {};
    auto target = std::find_if(snapshot_.groups.begin(), snapshot_.groups.end(),
                               [&groupId](const NativeRoomGroup &group) {
                                   return group.id == groupId;
                               });
    if (target == snapshot_.groups.end()) return commandMessage(RoomCommandResult::RoomNotFound);
    const int from = target->roomIds.indexOf(roomId);
    const int to = from + delta;
    if (from < 0 || to < 0 || to >= target->roomIds.size()) return {};
    const bool wasActive = snapshot_.activeGroupId == groupId;
    target->roomIds.move(from, to);
    if (wasActive) {
        setActiveGroup(groupId);
    } else {
        refreshPresentation();
        persistWorkspace();
    }
    return {};
}

void AppController::setActiveGroup(const QString &groupId)
{
    const NativeRoomGroup *group = nullptr;
    if (!groupId.isEmpty()) {
        const auto it = std::find_if(snapshot_.groups.cbegin(), snapshot_.groups.cend(),
                                     [&groupId](const NativeRoomGroup &candidate) {
                                         return candidate.id == groupId;
                                     });
        if (it == snapshot_.groups.cend()) return;
        group = &*it;
    }

    QVector<CoordinatorRoomSpec> targetRooms;
    if (group != nullptr) {
        targetRooms.reserve(group->roomIds.size());
        for (const QString &roomId : group->roomIds) {
            const NativeRoomRecord *record = libraryRecord(roomId);
            if (record == nullptr) continue;
            CoordinatorRoomSpec spec;
            spec.roomId = record->roomId;
            spec.requestedQuality = record->requestedQuality;
            spec.metadata = record->metadata;
            spec.volume = record->volume;
            spec.favorite = record->favorite;
            targetRooms.push_back(std::move(spec));
        }
    }

    const QStringList targetRoomIds = [&targetRooms] {
        QStringList ids;
        ids.reserve(targetRooms.size());
        for (const CoordinatorRoomSpec &spec : targetRooms) ids.push_back(spec.roomId);
        return ids;
    }();
    if (snapshot_.activeGroupId == groupId && coordinator_->roomIds() == targetRoomIds) return;

    if (coordinator_->replaceRooms(targetRooms) != RoomCommandResult::Accepted) return;
    snapshot_.activeGroupId = groupId;
    if (!targetRoomIds.isEmpty()) coordinator_->setAudioFocus(targetRoomIds.first());
    refreshPresentation();
    persistWorkspace();
}

bool AppController::setLayout(const QString &layoutId)
{
    if (coordinator_ == nullptr || !coordinator_->setLayout(layoutId)) return false;
    snapshot_.layoutId = coordinator_->layoutMode();
    refreshPresentation();
    persistWorkspace();
    return true;
}

bool AppController::setPrimaryRoomRatio(double ratio)
{
    if (coordinator_ == nullptr || !coordinator_->setPrimaryRoomRatio(ratio)) return false;
    snapshot_.primaryRoomRatio = coordinator_->primaryRoomRatio();
    refreshPresentation();
    persistWorkspace();
    return true;
}

bool AppController::setSidebarVisible(bool visible)
{
    if (workspace_ == nullptr || workspace_->sidebarVisible() == visible) return false;
    workspace_->setSidebarVisible(visible);
    snapshot_.sidebarVisible = visible;
    persistWorkspace();
    return true;
}

QString AppController::saveWorkspacePreset(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (!isValidName(trimmed)) return {};
    NativeWorkspacePreset preset;
    preset.id = generatedId();
    preset.name = trimmed;
    preset.layoutId = coordinator_->layoutMode();
    preset.activeGroupId = snapshot_.activeGroupId;
    preset.primaryRoomId = coordinator_->primaryRoomId();
    preset.audioRoomId = coordinator_->audioRoomId();
    preset.roomIds = coordinator_->roomIds();
    preset.sidebarVisible = workspace_->sidebarVisible();
    preset.primaryRoomRatio = coordinator_->primaryRoomRatio();
    preset.audioMode = coordinator_->audioMode();
    preset.globalMuted = coordinator_->globalMuted();
    preset.danmaku = snapshot_.danmaku;
    snapshot_.presets.push_back(std::move(preset));
    refreshPresentation();
    persistWorkspace();
    return snapshot_.presets.back().id;
}

QString AppController::applyWorkspacePreset(const QString &presetId)
{
    const auto it = std::find_if(snapshot_.presets.cbegin(), snapshot_.presets.cend(),
                                 [&presetId](const NativeWorkspacePreset &preset) {
                                     return preset.id == presetId;
                                 });
    if (it == snapshot_.presets.cend()) return commandMessage(RoomCommandResult::RoomNotFound);

    const NativeWorkspacePreset preset = *it;
    QVector<CoordinatorRoomSpec> targetRooms;
    targetRooms.reserve(preset.roomIds.size());
    for (const QString &roomId : preset.roomIds) {
        const NativeRoomRecord *record = libraryRecord(roomId);
        NativeRoomRecord fallback;
        if (record == nullptr) {
            fallback.roomId = roomId;
            fallback.metadata.roomId = roomId;
            fallback.metadata.anchorName = roomId;
            fallback.requestedQuality = StreamQuality::Auto;
            fallback.volume = 100;
            fallback.danmakuEnabled = true;
            snapshot_.library.push_back(fallback);
            record = &snapshot_.library.back();
        }
        targetRooms.push_back({record->roomId,
                               record->requestedQuality,
                               record->metadata,
                               record->volume,
                               record->favorite});
    }
    if (coordinator_->replaceRooms(targetRooms) != RoomCommandResult::Accepted) {
        return commandMessage(RoomCommandResult::Unavailable);
    }
    coordinator_->setLayout(preset.layoutId);
    coordinator_->setPrimaryRoomRatio(preset.primaryRoomRatio);
    coordinator_->setAudioMode(preset.audioMode);
    coordinator_->setGlobalMuted(preset.globalMuted);
    if (!preset.primaryRoomId.isEmpty()) coordinator_->setPrimaryRoomDetailed(preset.primaryRoomId);
    if (!preset.audioRoomId.isEmpty()) coordinator_->setAudioFocus(preset.audioRoomId);
    snapshot_.activeGroupId = preset.activeGroupId;
    snapshot_.layoutId = coordinator_->layoutMode();
    snapshot_.primaryRoomRatio = coordinator_->primaryRoomRatio();
    snapshot_.audioMode = coordinator_->audioMode();
    snapshot_.globalMuted = coordinator_->globalMuted();
    snapshot_.sidebarVisible = preset.sidebarVisible;
    workspace_->setSidebarVisible(preset.sidebarVisible);
    snapshot_.danmaku = preset.danmaku;
    danmaku_->setConfiguration(snapshot_.danmaku);
    synchronizeDanmaku();
    refreshPresentation();
    persistWorkspace();
    return {};
}

QString AppController::setNotificationsEnabled(bool enabled)
{
    NotificationPreferences preferences = notificationService_->preferences();
    return setNotificationPreferences(enabled, preferences.roomOnline, preferences.roomOffline,
                                      preferences.playbackFailed, preferences.playbackRecovered);
}

QString AppController::setNotificationPreferences(bool enabled,
                                                  bool roomOnline,
                                                  bool roomOffline,
                                                  bool playbackFailed,
                                                  bool playbackRecovered)
{
    const NotificationPreferences previous = notificationService_->preferences();
    const NotificationPreferences preferences{
        enabled, roomOnline, roomOffline, playbackFailed, playbackRecovered,
    };
    if (previous.enabled == preferences.enabled && previous.roomOnline == preferences.roomOnline
        && previous.roomOffline == preferences.roomOffline
        && previous.playbackFailed == preferences.playbackFailed
        && previous.playbackRecovered == preferences.playbackRecovered) {
        return {};
    }
    if (!notificationService_->setPreferences(preferences)) {
        return QStringLiteral("通知设置保存失败");
    }

    monitoring_->setStatus(enabled ? MonitoringModel::NotificationStatus::Enabled
                                   : MonitoringModel::NotificationStatus::Disabled,
                           MonitoringModel::RecoveryStatus::Healthy);
    emit notificationPreferencesChanged();
    return {};
}

void AppController::refreshRoom(const QString &roomId)
{
    if (coordinator_->sessionForRoom(roomId) == nullptr) {
        workspace_->setLastMessage(commandMessage(RoomCommandResult::RoomNotFound));
        return;
    }
    coordinator_->refreshRoomStatusNow(roomId);
    workspace_->setLastMessage(QStringLiteral("正在刷新房间数据"),
                               QStringLiteral("info"),
                               1800);
}

void AppController::attachPlayer(const QString &roomId, MpvQuickItem *item)
{
    if (coordinator_ == nullptr) return;
    coordinator_->attachPlayer(roomId, item);
}

void AppController::detachPlayer(const QString &roomId, MpvQuickItem *item)
{
    if (coordinator_ == nullptr) return;
    coordinator_->detachPlayer(roomId, item);
}

void AppController::minimizeWindow()
{
    if (!mainWindow_.isNull()) mainWindow_->showMinimized();
}

void AppController::toggleMaximizedWindow()
{
    if (mainWindow_.isNull()) return;
    if (mainWindow_->visibility() == QWindow::Maximized) {
        mainWindow_->showNormal();
    } else {
        mainWindow_->showMaximized();
    }
}

void AppController::toggleFullScreen()
{
    if (mainWindow_.isNull()) return;
    if (mainWindow_->visibility() == QWindow::FullScreen) {
        exitFullScreen();
        return;
    }
    preFullScreenVisibility_ = mainWindow_->visibility();
    hadPreFullScreenVisibility_ = true;
    mainWindow_->showFullScreen();
}

void AppController::exitFullScreen()
{
    if (mainWindow_.isNull() || mainWindow_->visibility() != QWindow::FullScreen) return;
    if (hadPreFullScreenVisibility_
        && preFullScreenVisibility_ == QWindow::Maximized) {
        mainWindow_->showMaximized();
    } else {
        mainWindow_->showNormal();
    }
    hadPreFullScreenVisibility_ = false;
}

void AppController::closeWindow()
{
    shutdown();
    if (!mainWindow_.isNull()) mainWindow_->close();
}

#ifdef DOUYU_TESTING
int AppController::attachedPlayerCountForTest() const
{
    if (coordinator_ == nullptr) return 0;

    int count = 0;
    for (const QString &roomId : coordinator_->roomIds()) {
        if (coordinator_->playerForRoom(roomId) != nullptr) ++count;
    }
    return count;
}

bool AppController::serviceProcessRunningForTest() const
{
    return service_ != nullptr && service_->isRunning();
}

int AppController::activeDanmakuSessionCountForTest() const
{
    return danmaku_ != nullptr ? danmaku_->activeSessionCountForTest() : 0;
}
#endif

void AppController::restoreWorkspace()
{
    restoring_ = true;
    snapshot_ = workspaceStore_.load();
    danmaku_->setConfiguration(snapshot_.danmaku);
    const NativeRoomGroup *activeGroup = nullptr;
    if (!snapshot_.activeGroupId.isEmpty()) {
        const auto groupIt = std::find_if(snapshot_.groups.cbegin(), snapshot_.groups.cend(),
                                          [this](const NativeRoomGroup &group) {
                                              return group.id == snapshot_.activeGroupId;
                                          });
        if (groupIt != snapshot_.groups.cend()) {
            activeGroup = &*groupIt;
        } else {
            snapshot_.activeGroupId.clear();
        }
    }

    const QStringList roomsToRestore = activeGroup != nullptr ? activeGroup->roomIds
                                                                : snapshot_.activeRoomIds;
    QVector<CoordinatorRoomSpec> targetRooms;
    targetRooms.reserve(roomsToRestore.size());
    for (const QString &roomId : roomsToRestore) {
        const NativeRoomRecord *storedRecord = libraryRecord(roomId);
        if (storedRecord == nullptr) continue;
        const NativeRoomRecord record = *storedRecord;
        targetRooms.push_back({record.roomId,
                               record.requestedQuality,
                               record.metadata,
                               record.volume,
                               record.favorite});
    }
    coordinator_->replaceRooms(targetRooms);
    coordinator_->setLayout(snapshot_.layoutId);
    coordinator_->setPrimaryRoomRatio(snapshot_.primaryRoomRatio);
    coordinator_->setAudioMode(snapshot_.audioMode);
    coordinator_->setGlobalMuted(snapshot_.globalMuted);
    workspace_->setSidebarVisible(snapshot_.sidebarVisible);
    if (activeGroup != nullptr) {
        snapshot_.primaryRoomId = targetRooms.isEmpty() ? QString() : targetRooms.first().roomId;
        snapshot_.audioRoomId = snapshot_.primaryRoomId;
    }
    if (!snapshot_.primaryRoomId.isEmpty()) {
        coordinator_->setPrimaryRoomDetailed(snapshot_.primaryRoomId);
    }
    if (!snapshot_.audioRoomId.isEmpty()) coordinator_->setAudioFocus(snapshot_.audioRoomId);
    restoring_ = false;
    synchronizeDanmaku();
    refreshPresentation();
    notificationPolicy_.resetBaseline();
}

void AppController::onServiceResponse(const ServiceResponse &response)
{
    if (searchRequestId_ == 0 || response.requestId != searchRequestId_) return;
    searchRequestId_ = 0;

    if (!response.ok || !response.search) {
        searchStatus_ = QStringLiteral("error");
        searchError_ = QStringLiteral("搜索失败，请稍后重试");
        emit searchStateChanged();
        return;
    }

    searchResults_.clear();
    searchCandidates_.clear();
    for (const RoomSearchResult &result : response.results) {
        RoomMetadata metadata;
        metadata.roomId = result.roomId;
        metadata.anchorName = result.anchorName;
        metadata.title = result.title;
        metadata.category = result.category;
        metadata.viewerLabel = result.viewerLabel;
        metadata.avatarUrl = result.avatarUrl;
        searchCandidates_.insert(result.roomId, metadata);
        searchResults_.push_back(QVariantMap{
            {QStringLiteral("roomId"), result.roomId},
            {QStringLiteral("anchorName"), result.anchorName},
            {QStringLiteral("title"), result.title},
            {QStringLiteral("category"), result.category},
            {QStringLiteral("online"), result.online},
            {QStringLiteral("viewerLabel"), result.viewerLabel},
            {QStringLiteral("avatarUrl"), result.avatarUrl},
        });
    }

    searchStatus_ = searchResults_.isEmpty() ? QStringLiteral("empty") : QStringLiteral("success");
    searchError_.clear();
    emit searchResultsChanged();
    emit searchStateChanged();
}

void AppController::onServiceRequestFailed(quint64 requestId, const QString &)
{
    if (searchRequestId_ == 0 || requestId != searchRequestId_) return;
    searchRequestId_ = 0;
    searchStatus_ = QStringLiteral("error");
    searchError_ = QStringLiteral("搜索失败，请稍后重试");
    emit searchStateChanged();
}

void AppController::onRoomStatusRefreshed(const QString &roomId, bool online)
{
    const RoomLiveStatus nextStatus = online ? RoomLiveStatus::Online : RoomLiveStatus::Offline;
    const RoomLiveStatus previousStatus = lastLiveStatuses_.value(roomId, RoomLiveStatus::Unknown);
    if (previousStatus != RoomLiveStatus::Unknown && previousStatus != nextStatus) {
        workspace_->setLastMessage(online ? QStringLiteral("房间已开播")
                                          : QStringLiteral("房间已下播"),
                                   QStringLiteral("success"),
                                   4200);
    } else {
        workspace_->setLastMessage(QStringLiteral("房间数据已更新"),
                                   QStringLiteral("success"),
                                   3200);
    }
}

void AppController::persistWorkspace()
{
    if (!restoring_) workspaceStore_.save(snapshot_);
}

void AppController::onSnapshotsChanged(const RoomSnapshots &snapshots)
{
    snapshot_.activeRoomIds = coordinator_->roomIds();
    snapshot_.primaryRoomId = coordinator_->primaryRoomId();
    snapshot_.audioRoomId = coordinator_->audioRoomId();
    snapshot_.audioMode = coordinator_->audioMode();
    snapshot_.globalMuted = coordinator_->globalMuted();
    for (const RoomSnapshot &room : snapshots) {
        lastLiveStatuses_.insert(room.roomId, room.liveStatus);
        NativeRoomRecord *record = libraryRecord(room.roomId);
        if (record == nullptr) {
            snapshot_.library.push_back({room.roomId, room.metadata, room.requestedQuality,
                                         room.favorite, 0, room.volume, true});
            record = &snapshot_.library.back();
        }
        record->requestedQuality = room.requestedQuality;
        record->favorite = room.favorite;
        record->volume = room.volume;
        if (!room.metadata.anchorName.isEmpty() || !room.metadata.title.isEmpty()
            || !room.metadata.category.isEmpty() || !room.metadata.viewerLabel.isEmpty()
            || !room.metadata.avatarUrl.isEmpty()) {
            record->metadata = room.metadata;
        }
        record->metadata.roomId = room.roomId;
    }
    const QSet<QString> activeIds = QSet<QString>(snapshot_.activeRoomIds.cbegin(),
                                                   snapshot_.activeRoomIds.cend());
    for (auto it = lastLiveStatuses_.begin(); it != lastLiveStatuses_.end();) {
        if (activeIds.contains(it.key())) ++it;
        else it = lastLiveStatuses_.erase(it);
    }
    synchronizeDanmaku();
    emit libraryRoomsChanged();
    refreshPresentation();

    int online = 0;
    int offline = 0;
    int errors = 0;
    for (const RoomSnapshot &room : snapshots) {
        if (room.liveStatus == RoomLiveStatus::Online) ++online;
        if (room.liveStatus == RoomLiveStatus::Offline) ++offline;
        if (room.state == RoomSession::State::Error
            || room.playbackHealth == RoomPlaybackHealth::Error) {
            ++errors;
        }
    }
    monitoring_->setCounts(online, offline, errors);
    const auto notificationStatus = notificationService_->preferences().enabled
        ? MonitoringModel::NotificationStatus::Enabled
        : MonitoringModel::NotificationStatus::Disabled;
    monitoring_->setStatus(notificationStatus, MonitoringModel::RecoveryStatus::Healthy);
    if (!restoring_) {
        for (const NotificationEvent &event : notificationPolicy_.update(snapshots)) {
            notificationService_->deliver(event);
        }
    }
}

void AppController::refreshPresentation()
{
    const RoomSnapshots snapshots = coordinator_->roomSnapshots();
    rooms_->applySnapshots(snapshots);
    QHash<QString, RoomPresentationSettings> settings;
    for (const RoomSnapshot &room : snapshots) {
        const NativeRoomRecord *record = libraryRecord(room.roomId);
        const QVariantMap status = danmaku_->statusForRoom(room.roomId);
        const QVariantMap stats = danmaku_->statsForRoom(room.roomId);
        RoomPresentationSettings presentation;
        presentation.volume = record != nullptr ? record->volume : room.volume;
        presentation.danmakuEnabled = record != nullptr && record->danmakuEnabled;
        presentation.groupId = groupForRoom(room.roomId);
        presentation.danmakuState = status.value(QStringLiteral("state")).toString();
        presentation.danmakuErrorCode = status.value(QStringLiteral("errorCode")).toString();
        presentation.danmakuRecentRate = stats.value(QStringLiteral("recentRate")).toDouble();
        presentation.danmakuPeakRate = stats.value(QStringLiteral("peakRate")).toDouble();
        presentation.danmakuFiltered = stats.value(QStringLiteral("filtered")).toInt();
        presentation.danmakuDuplicates = stats.value(QStringLiteral("duplicates")).toInt();
        presentation.danmakuRateLimited = stats.value(QStringLiteral("rateLimited")).toInt();
        presentation.danmakuQueueOverflow = stats.value(QStringLiteral("queueOverflow")).toInt();
        presentation.danmakuUpstreamDropped =
            stats.value(QStringLiteral("upstreamDropped")).toInt();
        settings.insert(room.roomId, presentation);
    }
    rooms_->applyPresentationSettings(settings);
    workspace_->setCoordinatorState(coordinator_->layoutId(), coordinator_->primaryRoomId(),
                                    coordinator_->audioRoomId());
    workspace_->setAudioPolicy(coordinator_->audioMode(), coordinator_->globalMuted());
    workspace_->setLayoutPresentation(coordinator_->layoutMode(),
                                      coordinator_->primaryRoomRatio());
    workspace_->setWorkspaceData(snapshot_.groups, snapshot_.presets, snapshot_.activeGroupId);
}

void AppController::synchronizeDanmaku()
{
    if (danmaku_ == nullptr || coordinator_ == nullptr) return;
    const RoomSnapshots snapshots = coordinator_->roomSnapshots();
    QVector<DanmakuRoomEligibility> eligibility;
    eligibility.reserve(snapshots.size());
    for (const RoomSnapshot &room : snapshots) {
        const NativeRoomRecord *record = libraryRecord(room.roomId);
        DanmakuRoomEligibility state;
        state.roomId = room.roomId;
        state.active = snapshot_.activeRoomIds.contains(room.roomId);
        state.roomEnabled = record != nullptr && record->danmakuEnabled;
        state.globalEnabled = snapshot_.danmaku.globalEnabled;
        state.live = room.liveStatus == RoomLiveStatus::Online;
        state.governance = DanmakuGovernance::resolvedGovernance(
            snapshot_.danmaku.governance,
            snapshot_.danmaku.roomOverrides.value(room.roomId));
        eligibility.push_back(std::move(state));
    }
    danmaku_->synchronize(eligibility);
}

void AppController::touchHistory(const QString &roomId)
{
    NativeRoomRecord *record = libraryRecord(roomId);
    if (record == nullptr) return;

    record->lastOpenedAtMs = QDateTime::currentMSecsSinceEpoch();
    emit libraryRoomsChanged();
}

NativeRoomRecord *AppController::libraryRecord(const QString &roomId)
{
    for (NativeRoomRecord &record : snapshot_.library) {
        if (record.roomId == roomId) return &record;
    }
    return nullptr;
}

const NativeRoomRecord *AppController::libraryRecord(const QString &roomId) const
{
    for (const NativeRoomRecord &record : snapshot_.library) {
        if (record.roomId == roomId) return &record;
    }
    return nullptr;
}

QString AppController::groupForRoom(const QString &roomId) const
{
    for (const NativeRoomGroup &group : snapshot_.groups) {
        if (group.roomIds.contains(roomId)) return group.id;
    }
    return {};
}

QString AppController::commandMessage(RoomCommandResult result) const
{
    return workspace_->commandMessage(result);
}

bool AppController::isValidQuality(int quality) noexcept
{
    return quality >= static_cast<int>(StreamQuality::Auto)
        && quality <= static_cast<int>(StreamQuality::Standard);
}

bool AppController::isValidName(const QString &name) noexcept
{
    return !name.trimmed().isEmpty() && name.trimmed().size() <= 30;
}
