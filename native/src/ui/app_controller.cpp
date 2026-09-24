#include "ui/app_controller.h"

#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QRegularExpression>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QWindow>

#include <algorithm>
#include <memory>

#include "app/windows_notification_service.h"
#include "app/update_checker.h"
#include "app/windows_tray_service.h"
#include "danmaku/danmaku_socket.h"
#include "danmaku/danmaku_timer_scheduler.h"
#include "service/streamget_process_client.h"
#include "ui/monitoring_model.h"
#include "ui/mpv_quick_item.h"
#include "ui/room_list_model.h"
#include "ui/workspace_model.h"
#include "workspace/favorite_monitor.h"
#include "workspace/guild_roster.h"
#include "workspace/multi_room_coordinator.h"

#ifndef DOUYU_APP_VERSION
#define DOUYU_APP_VERSION "dev"
#endif

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
    favoriteMonitor_ = std::make_unique<FavoriteMonitor>(
        [this](const QString &roomId) {
            return service_ != nullptr ? service_->search(roomId) : 0;
        },
        [this](quint64 requestId) {
            if (service_ != nullptr) service_->cancel(requestId);
        },
        RoomRefreshTiming{},
        this);
    notificationService_ = std::make_unique<WindowsNotificationService>(settings, notificationSink, this);
    trayService_ = std::make_unique<WindowsTrayService>(this);
    rooms_ = std::make_unique<RoomListModel>(this);
    workspace_ = std::make_unique<WorkspaceModel>(this, this);
    monitoring_ = std::make_unique<MonitoringModel>(this);
    danmaku_ = std::make_unique<DanmakuController>(std::move(danmakuFactory), this);
    updateChecker_ = std::make_unique<UpdateChecker>(QStringLiteral(DOUYU_APP_VERSION), QUrl{}, 8000, this);
    connect(updateChecker_.get(), &UpdateChecker::stateChanged, this, &AppController::updateStateChanged);
    connect(updateChecker_.get(), &UpdateChecker::resultChanged, this, &AppController::updateStateChanged);

    connect(service_.get(), &StreamgetProcessClient::responseReceived,
            this, &AppController::onServiceResponse);
    connect(service_.get(), &StreamgetProcessClient::requestFailed,
            this, &AppController::onServiceRequestFailed);
    connect(favoriteMonitor_.get(), &FavoriteMonitor::eventsReady,
            this, [this](const QVector<NotificationEvent> &events) {
                if (restoring_) return;
                for (const NotificationEvent &event : events) notificationService_->deliver(event);
            });
    connect(favoriteMonitor_.get(), &FavoriteMonitor::roomUpdated,
            this, &AppController::onFavoriteRoomUpdated);

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
    if (settings_ != nullptr) {
        const QString behavior = settings_->value(QStringLiteral("window/closeBehavior"),
                                                  QStringLiteral("ask")).toString();
        if (behavior == QStringLiteral("quit") || behavior == QStringLiteral("background")) {
            closeBehavior_ = behavior;
        }
    }
    const auto notificationStatus = notificationService_->preferences().enabled
        ? MonitoringModel::NotificationStatus::Enabled
        : MonitoringModel::NotificationStatus::Disabled;
    monitoring_->setStatus(notificationStatus, MonitoringModel::RecoveryStatus::Healthy);
    synchronizeFavoriteMonitor();

    connect(trayService_.get(), &WindowsTrayService::showRequested,
            this, &AppController::restoreFromBackground);
    connect(trayService_.get(), &WindowsTrayService::quitRequested,
            this, &AppController::requestQuit);
}

AppController::~AppController()
{
    shutdown();
}

void AppController::shutdown()
{
    if (shuttingDown_) return;
    shuttingDown_ = true;
    if (trayService_ != nullptr) trayService_->stop();

    const quint64 pendingSearch = searchRequestId_;
    searchRequestId_ = 0;
    if (pendingSearch != 0 && service_ != nullptr) service_->cancel(pendingSearch);
    persistWorkspace();
    if (danmaku_ != nullptr) danmaku_->stopAll();
    coordinator_.reset();
    if (favoriteMonitor_ != nullptr) favoriteMonitor_->stop();
    favoriteMonitor_.reset();
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
        if (left->favorite != right->favorite) return left->favorite > right->favorite;
        if (left->favorite) {
            if (left->favoriteSortOrder != right->favoriteSortOrder) {
                return left->favoriteSortOrder < right->favoriteSortOrder;
            }
            if (left->favoriteAddedAtMs != right->favoriteAddedAtMs) {
                return left->favoriteAddedAtMs < right->favoriteAddedAtMs;
            }
        }
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
            {QStringLiteral("favoriteAddedAtMs"), record->favoriteAddedAtMs},
            {QStringLiteral("favoriteSortOrder"), record->favoriteSortOrder},
            {QStringLiteral("lastOpenedAtMs"), record->lastOpenedAtMs},
            {QStringLiteral("active"), snapshot_.activeRoomIds.contains(record->roomId)},
            {QStringLiteral("online"), favoriteLiveStatuses_.value(record->roomId,
                                                                      RoomLiveStatus::Unknown)
                                             == RoomLiveStatus::Online},
        });
    }
    return projection;
}

QVariantList AppController::guildRoster() const
{
    QVariantList projection;
    const QVector<GuildMember> members = GuildRoster::bundled();
    projection.reserve(members.size());
    for (const GuildMember &member : members) {
        projection.push_back(QVariantMap{
            {QStringLiteral("id"), member.id},
            {QStringLiteral("anchorName"), member.anchorName},
            {QStringLiteral("roomId"), member.roomId},
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
        {QStringLiteral("favoriteTitleChanged"), preferences.favoriteTitleChanged},
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

QString AppController::updateState() const
{
    if (updateChecker_ == nullptr) return QStringLiteral("idle");
    switch (updateChecker_->state()) {
    case UpdateChecker::State::Checking: return QStringLiteral("checking");
    case UpdateChecker::State::UpToDate: return QStringLiteral("upToDate");
    case UpdateChecker::State::UpdateAvailable: return QStringLiteral("updateAvailable");
    case UpdateChecker::State::Error: return QStringLiteral("error");
    case UpdateChecker::State::Idle: return QStringLiteral("idle");
    }
    return QStringLiteral("idle");
}

QString AppController::updateMessage() const
{
    if (updateChecker_ == nullptr) return {};
    switch (updateChecker_->state()) {
    case UpdateChecker::State::Checking: return QStringLiteral("正在检查更新...");
    case UpdateChecker::State::UpToDate:
        return QStringLiteral("已是最新版本（%1）").arg(updateChecker_->currentVersion());
    case UpdateChecker::State::UpdateAvailable:
        return QStringLiteral("发现新版本 %1").arg(updateChecker_->latestVersion());
    case UpdateChecker::State::Error: return updateChecker_->errorMessage();
    case UpdateChecker::State::Idle: return {};
    }
    return {};
}

QString AppController::currentVersion() const
{
    return updateChecker_ != nullptr ? updateChecker_->currentVersion() : QString();
}

QString AppController::latestVersion() const
{
    return updateChecker_ != nullptr ? updateChecker_->latestVersion() : QString();
}

QUrl AppController::updateReleaseUrl() const
{
    return updateChecker_ != nullptr ? updateChecker_->releaseUrl() : QUrl();
}

void AppController::checkForUpdates()
{
    if (updateChecker_ != nullptr) updateChecker_->check();
}

bool AppController::openLatestRelease()
{
    const QUrl url = updateReleaseUrl();
    return url.isValid() && QDesktopServices::openUrl(url);
}

bool AppController::backgroundHosted() const noexcept
{
    return backgroundHosted_;
}

bool AppController::windowMinimized() const noexcept
{
    return windowMinimized_;
}

QString AppController::closeBehavior() const
{
    return closeBehavior_;
}

bool AppController::quitRequested() const noexcept
{
    return quitRequested_;
}

QString AppController::fixedPlaybackMessage(const QString &) const
{
    return QStringLiteral("播放地址不可用");
}

void AppController::setMainWindow(QWindow *window)
{
    mainWindow_ = window;
    if (window == nullptr) return;

    // The QML engine may destroy the window before AppController during test
    // teardown or application shutdown. Clear both references at that boundary
    // so the tray service never retains a dangling native window pointer.
    connect(window, &QObject::destroyed, this, [this] {
        mainWindow_.clear();
        if (trayService_ != nullptr) trayService_->stop();
    });
    if (trayService_ != nullptr) trayService_->start(window);
}

QString AppController::addRoom(const QString &roomId)
{
    if (coordinator_ != nullptr && coordinator_->roomCount() >= 9
        && coordinator_->layoutMode() != QStringLiteral("primary-two")) {
        return QStringLiteral("当前布局最多支持 9 个房间");
    }
    const NativeRoomRecord *record = libraryRecord(roomId);
    const RoomCommandResult result = coordinator_->addRoomDetailed(
        roomId,
        record != nullptr ? record->requestedQuality : StreamQuality::Auto,
        record != nullptr ? record->requestedQualityRate : -1,
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
    if (coordinator_ != nullptr && coordinator_->roomCount() >= 9
        && coordinator_->layoutMode() != QStringLiteral("primary-two")) {
        return QStringLiteral("当前布局最多支持 9 个房间");
    }

    const NativeRoomRecord *record = libraryRecord(roomId);
    const RoomCommandResult result = coordinator_->addRoomDetailed(
        roomId,
        record != nullptr ? record->requestedQuality : StreamQuality::Auto,
        record != nullptr ? record->requestedQualityRate : -1,
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
    const bool wasDualPrimary = coordinator_ != nullptr
        && coordinator_->layoutMode() == QStringLiteral("primary-two");
    const RoomCommandResult result = coordinator_->removeRoomDetailed(roomId);
    if (result == RoomCommandResult::Accepted) {
        if (wasDualPrimary
            && coordinator_->roomCount() < 4) {
            coordinator_->setLayout(QStringLiteral("auto"));
            snapshot_.layoutId = QStringLiteral("auto");
            workspace_->setLastMessage(QStringLiteral("双主布局至少需要 4 路直播间"),
                                       QStringLiteral("info"), 3200);
        }
        persistWorkspace();
    }
    return commandMessage(result);
}

QString AppController::removeHistoryRoom(const QString &roomId)
{
    const QString requestedRoomId = roomId.trimmed();
    if (requestedRoomId.isEmpty()) return commandMessage(RoomCommandResult::RoomNotFound);
    if (snapshot_.activeRoomIds.contains(requestedRoomId)) {
        return QStringLiteral("正在播放的房间无法删除历史记录");
    }

    NativeRoomRecord *record = libraryRecord(requestedRoomId);
    if (record == nullptr || record->lastOpenedAtMs <= 0) {
        return commandMessage(RoomCommandResult::RoomNotFound);
    }

    record->lastOpenedAtMs = 0;

    emit libraryRoomsChanged();
    persistWorkspace();
    return {};
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

QString AppController::setSecondaryPrimaryRoom(const QString &roomId)
{
    const RoomCommandResult result = coordinator_->setSecondaryPrimaryRoomDetailed(roomId);
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

bool AppController::setRoomMuted(const QString &roomId, bool muted)
{
    if (coordinator_ == nullptr || !coordinator_->setRoomMuted(roomId, muted)) return false;
    refreshPresentation();
    persistWorkspace();
    return true;
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

QString AppController::setQuality(const QString &roomId, int quality, int qualityRate)
{
    if (!isValidQuality(quality) || qualityRate < -1 || qualityRate > 255) {
        return commandMessage(RoomCommandResult::InvalidRoomId);
    }
    const RoomCommandResult result = coordinator_->setRequestedQuality(
        roomId, static_cast<StreamQuality>(quality), qualityRate);
    if (result == RoomCommandResult::Accepted) persistWorkspace();
    return commandMessage(result);
}

QString AppController::setFavorite(const QString &roomId, bool favorite)
{
    if (coordinator_->sessionForRoom(roomId) == nullptr) {
        return commandMessage(RoomCommandResult::RoomNotFound);
    }
    if (!coordinator_->setFavorite(roomId, favorite)) return {};
    if (NativeRoomRecord *record = libraryRecord(roomId)) {
        record->favorite = favorite;
        if (favorite) {
            const qint64 now = QDateTime::currentMSecsSinceEpoch();
            record->favoriteAddedAtMs = now;
            record->favoriteSortOrder = now;
        } else {
            record->favoriteAddedAtMs = 0;
            record->favoriteSortOrder = 0;
        }
        emit libraryRoomsChanged();
    }
    synchronizeFavoriteMonitor();
    persistWorkspace();
    return {};
}

QString AppController::moveFavoriteRoom(const QString &roomId, int targetIndex)
{
    NativeRoomRecord *record = libraryRecord(roomId);
    if (record == nullptr || !record->favorite) return commandMessage(RoomCommandResult::RoomNotFound);
    QVector<NativeRoomRecord *> favorites;
    for (NativeRoomRecord &candidate : snapshot_.library) {
        if (candidate.favorite) favorites.push_back(&candidate);
    }
    std::sort(favorites.begin(), favorites.end(), [](const NativeRoomRecord *left,
                                                     const NativeRoomRecord *right) {
        if (left->favoriteSortOrder != right->favoriteSortOrder) {
            return left->favoriteSortOrder < right->favoriteSortOrder;
        }
        return left->roomId < right->roomId;
    });
    const int clampedIndex = qBound(0, targetIndex, favorites.size() - 1);
    const int currentIndex = favorites.indexOf(record);
    if (currentIndex < 0 || currentIndex == clampedIndex) return {};
    favorites.removeAt(currentIndex);
    favorites.insert(clampedIndex, record);
    for (int index = 0; index < favorites.size(); ++index) favorites[index]->favoriteSortOrder = index + 1;
    emit libraryRoomsChanged();
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
    if (record == nullptr || coordinator_->sessionForRoom(roomId) == nullptr) {
        qWarning().noquote() << "danmaku toggle ignored room=" << roomId;
        return;
    }
    qInfo().noquote() << "danmaku toggle room=" << roomId
                      << "enabled=" << (!record->danmakuEnabled);
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
    if (target->roomIds.contains(roomId)) return {};
    if (target->roomIds.size() >= 9) return QStringLiteral("分组最多包含 9 个房间");
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

QString AppController::createTeam(const QString &name)
{
    const QString trimmed = name.trimmed();
    if (!isValidName(trimmed)) return QStringLiteral("请输入 1 到 30 个字符的队伍名称");
    if (snapshot_.teams.size() >= 20) return QStringLiteral("最多创建 20 个队伍");
    snapshot_.teams.push_back({generatedId(), trimmed, {}});
    refreshPresentation();
    persistWorkspace();
    return snapshot_.teams.back().id;
}

QString AppController::renameTeam(const QString &teamId, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (!isValidName(trimmed)) return QStringLiteral("请输入 1 到 30 个字符的队伍名称");
    for (NativeTeam &team : snapshot_.teams) {
        if (team.id != teamId) continue;
        if (team.name == trimmed) return {};
        team.name = trimmed;
        refreshPresentation();
        persistWorkspace();
        return {};
    }
    return QStringLiteral("未找到该队伍");
}

QString AppController::deleteTeam(const QString &teamId)
{
    const auto it = std::find_if(snapshot_.teams.cbegin(), snapshot_.teams.cend(),
                                 [&teamId](const NativeTeam &team) {
                                     return team.id == teamId;
                                 });
    if (it == snapshot_.teams.cend()) return QStringLiteral("未找到该队伍");
    snapshot_.teams.erase(it);
    refreshPresentation();
    persistWorkspace();
    return {};
}

QString AppController::moveTeam(const QString &teamId, int delta)
{
    if (delta == 0) return {};
    const auto it = std::find_if(snapshot_.teams.cbegin(), snapshot_.teams.cend(),
                                 [&teamId](const NativeTeam &team) {
                                     return team.id == teamId;
                                 });
    if (it == snapshot_.teams.cend()) return QStringLiteral("未找到该队伍");
    const int from = static_cast<int>(std::distance(snapshot_.teams.cbegin(), it));
    const int to = qBound(0, from + delta, snapshot_.teams.size() - 1);
    if (from == to) return {};
    snapshot_.teams.move(from, to);
    refreshPresentation();
    persistWorkspace();
    return {};
}

QString AppController::assignGuildMemberToTeam(const QString &memberId, const QString &teamId)
{
    const GuildMember *member = GuildRoster::findById(memberId);
    if (member == nullptr) return QStringLiteral("未找到该公会主播");
    const auto target = std::find_if(snapshot_.teams.begin(), snapshot_.teams.end(),
                                     [&teamId](const NativeTeam &team) {
                                         return team.id == teamId;
                                     });
    if (target == snapshot_.teams.end()) return QStringLiteral("未找到该队伍");

    for (NativeTeam &team : snapshot_.teams) {
        if (team.id == teamId) continue;
        team.memberIds.removeAll(member->id);
    }
    if (!target->memberIds.contains(member->id)) target->memberIds.push_back(member->id);
    refreshPresentation();
    persistWorkspace();
    return {};
}

QString AppController::removeGuildMemberFromTeam(const QString &teamId, const QString &memberId)
{
    const auto target = std::find_if(snapshot_.teams.begin(), snapshot_.teams.end(),
                                     [&teamId](const NativeTeam &team) {
                                         return team.id == teamId;
                                     });
    if (target == snapshot_.teams.end()) return QStringLiteral("未找到该队伍");
    const GuildMember *member = GuildRoster::findById(memberId);
    if (member == nullptr) return QStringLiteral("未找到该公会主播");
    if (target->memberIds.removeAll(member->id) == 0) return {};
    refreshPresentation();
    persistWorkspace();
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
            spec.requestedQualityRate = record->requestedQualityRate;
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
    if (layoutId.trimmed().compare(QStringLiteral("primary-two"), Qt::CaseInsensitive) == 0
        && coordinator_ != nullptr && coordinator_->roomCount() < 4) {
        workspace_->setLastMessage(QStringLiteral("双主布局至少需要 4 路直播间"),
                                   QStringLiteral("error"), 3200);
        return false;
    }
    if (coordinator_ == nullptr || !coordinator_->setLayout(layoutId)) return false;

    const bool leftDualPrimaryAtCapacity = coordinator_->layoutMode() != QStringLiteral("primary-two")
        && coordinator_->roomCount() == MultiRoomCoordinator::kMaxRooms;
    if (leftDualPrimaryAtCapacity) {
        const QStringList roomIds = coordinator_->roomIds();
        if (!roomIds.isEmpty()) coordinator_->removeRoomDetailed(roomIds.constLast());
    }
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

QString AppController::setNavigationVisible(bool visible)
{
    if (workspace_ == nullptr) return QStringLiteral("当前操作不可用");
    if (snapshot_.navigationVisible == visible && workspace_->navigationVisible() == visible) return {};
    snapshot_.navigationVisible = visible;
    workspace_->setNavigationVisible(visible);
    persistWorkspace();
    return {};
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
    preset.secondaryPrimaryRoomId = coordinator_->secondaryPrimaryRoomId();
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
            fallback.requestedQualityRate = -1;
            fallback.volume = 100;
            fallback.danmakuEnabled = true;
            snapshot_.library.push_back(fallback);
            record = &snapshot_.library.back();
        }
        targetRooms.push_back({record->roomId,
                               record->requestedQuality,
                               record->requestedQualityRate,
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
    if (!preset.secondaryPrimaryRoomId.isEmpty()) {
        coordinator_->setSecondaryPrimaryRoomDetailed(preset.secondaryPrimaryRoomId);
    }
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

QString AppController::deleteWorkspacePreset(const QString &presetId)
{
    const auto it = std::find_if(snapshot_.presets.begin(), snapshot_.presets.end(),
                                 [&presetId](const NativeWorkspacePreset &preset) {
                                     return preset.id == presetId;
                                 });
    if (it == snapshot_.presets.end()) return commandMessage(RoomCommandResult::RoomNotFound);

    snapshot_.presets.erase(it);
    refreshPresentation();
    persistWorkspace();
    return {};
}

QString AppController::setNotificationsEnabled(bool enabled)
{
    NotificationPreferences preferences = notificationService_->preferences();
    return setNotificationPreferences(enabled, preferences.roomOnline, preferences.roomOffline,
                                      preferences.playbackFailed, preferences.playbackRecovered,
                                      preferences.favoriteTitleChanged);
}

QString AppController::setNotificationPreferences(bool enabled,
                                                  bool roomOnline,
                                                   bool roomOffline,
                                                   bool playbackFailed,
                                                   bool playbackRecovered,
                                                   bool favoriteTitleChanged)
{
    const NotificationPreferences previous = notificationService_->preferences();
    const NotificationPreferences preferences{
        enabled, roomOnline, roomOffline, playbackFailed, playbackRecovered, favoriteTitleChanged,
    };
    if (previous.enabled == preferences.enabled && previous.roomOnline == preferences.roomOnline
        && previous.roomOffline == preferences.roomOffline
        && previous.playbackFailed == preferences.playbackFailed
        && previous.playbackRecovered == preferences.playbackRecovered
        && previous.favoriteTitleChanged == preferences.favoriteTitleChanged) {
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
    if (shuttingDown_ || backgroundHosted_) return;
    if (windowMinimized_) return;
    if (!windowMinimized_) {
        windowMinimized_ = true;
        emit windowMinimizedChanged();
    }
    if (coordinator_ != nullptr) coordinator_->suspendRendering();
    if (danmaku_ != nullptr) danmaku_->setPresentationSuspended(true);
    if (!mainWindow_.isNull()) mainWindow_->showMinimized();
}

void AppController::minimizeToBackground()
{
    if (shuttingDown_) return;
    if (coordinator_ != nullptr) coordinator_->suspendRendering();
    if (danmaku_ != nullptr) danmaku_->setPresentationSuspended(true);
    if (!backgroundHosted_) {
        backgroundHosted_ = true;
        emit backgroundHostedChanged();
    }
    if (windowMinimized_) {
        windowMinimized_ = false;
        emit windowMinimizedChanged();
    }
    if (!mainWindow_.isNull()) mainWindow_->hide();
}

void AppController::closeToTray()
{
    minimizeToBackground();
}

void AppController::restoreFromBackground()
{
    if (shuttingDown_) return;
    if (backgroundHosted_) {
        backgroundHosted_ = false;
        emit backgroundHostedChanged();
    }
    if (windowMinimized_) {
        windowMinimized_ = false;
        emit windowMinimizedChanged();
    }
    if (coordinator_ != nullptr) coordinator_->resumeRendering();
    if (danmaku_ != nullptr) danmaku_->setPresentationSuspended(false);
    if (mainWindow_.isNull()) return;
    mainWindow_->show();
    mainWindow_->raise();
    mainWindow_->requestActivate();
}

void AppController::restoreFromMinimized()
{
    if (shuttingDown_ || !windowMinimized_) return;
    windowMinimized_ = false;
    emit windowMinimizedChanged();
    if (coordinator_ != nullptr) coordinator_->resumeRendering();
    if (danmaku_ != nullptr) danmaku_->setPresentationSuspended(false);
    if (mainWindow_.isNull()) return;
    mainWindow_->showNormal();
    mainWindow_->raise();
    mainWindow_->requestActivate();
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
    requestClose();
}

void AppController::requestClose()
{
    if (closeBehavior_ == QStringLiteral("quit")) {
        requestQuit();
    } else if (closeBehavior_ == QStringLiteral("background")) {
        closeToTray();
    }
}

bool AppController::setCloseBehavior(const QString &behavior, bool persist)
{
    if (behavior != QStringLiteral("ask") && behavior != QStringLiteral("quit")
        && behavior != QStringLiteral("background")) return false;
    if (!persist && settings_ != nullptr) {
        settings_->remove(QStringLiteral("window/closeBehavior"));
    }
    if (closeBehavior_ == behavior) {
        if (persist && settings_ != nullptr) settings_->setValue(QStringLiteral("window/closeBehavior"), behavior);
        return true;
    }
    closeBehavior_ = behavior;
    if (persist && settings_ != nullptr) settings_->setValue(QStringLiteral("window/closeBehavior"), behavior);
    emit closeBehaviorChanged();
    return true;
}

void AppController::clearCloseBehavior()
{
    setCloseBehavior(QStringLiteral("ask"));
    if (settings_ != nullptr) settings_->remove(QStringLiteral("window/closeBehavior"));
}

void AppController::requestQuit()
{
    if (quitRequested_) return;
    quitRequested_ = true;
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
                               record.requestedQualityRate,
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
    workspace_->setNavigationVisible(snapshot_.navigationVisible);
    if (activeGroup != nullptr) {
        snapshot_.primaryRoomId = targetRooms.isEmpty() ? QString() : targetRooms.first().roomId;
        snapshot_.audioRoomId = snapshot_.primaryRoomId;
    }
    if (!snapshot_.primaryRoomId.isEmpty()) {
        coordinator_->setPrimaryRoomDetailed(snapshot_.primaryRoomId);
    }
    if (!snapshot_.secondaryPrimaryRoomId.isEmpty()) {
        coordinator_->setSecondaryPrimaryRoomDetailed(snapshot_.secondaryPrimaryRoomId);
    }
    if (!snapshot_.audioRoomId.isEmpty()) coordinator_->setAudioFocus(snapshot_.audioRoomId);
    restoring_ = false;
    synchronizeDanmaku();
    refreshPresentation();
    notificationPolicy_.resetBaseline();
}

void AppController::onServiceResponse(const ServiceResponse &response)
{
    if (favoriteMonitor_ != nullptr) favoriteMonitor_->onSearchResponse(response);
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
    if (favoriteMonitor_ != nullptr) favoriteMonitor_->onRequestFailed(requestId);
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
    }
}

void AppController::synchronizeFavoriteMonitor()
{
    if (favoriteMonitor_ == nullptr) return;
    QVector<FavoriteRoomSpec> rooms;
    for (const NativeRoomRecord &record : snapshot_.library) {
        if (!record.favorite) continue;
        rooms.push_back({record.roomId,
                         record.metadata,
                         favoriteLiveStatuses_.value(record.roomId, RoomLiveStatus::Unknown)});
    }
    favoriteMonitor_->synchronize(rooms);
}

void AppController::onFavoriteRoomUpdated(const QString &roomId,
                                          const RoomMetadata &metadata,
                                          RoomLiveStatus liveStatus)
{
    NativeRoomRecord *record = libraryRecord(roomId);
    if (record == nullptr || !record->favorite) return;
    record->metadata = metadata;
    record->metadata.roomId = roomId;
    favoriteLiveStatuses_.insert(roomId, liveStatus);
    emit libraryRoomsChanged();
    if (!restoring_) persistWorkspace();
}

void AppController::persistWorkspace()
{
    if (!restoring_) workspaceStore_.save(snapshot_);
}

void AppController::onSnapshotsChanged(const RoomSnapshots &snapshots)
{
    snapshot_.activeRoomIds = coordinator_->roomIds();
    snapshot_.primaryRoomId = coordinator_->primaryRoomId();
    snapshot_.secondaryPrimaryRoomId = coordinator_->secondaryPrimaryRoomId();
    snapshot_.layoutId = coordinator_->layoutId();
    snapshot_.audioRoomId = coordinator_->audioRoomId();
    snapshot_.audioMode = coordinator_->audioMode();
    snapshot_.globalMuted = coordinator_->globalMuted();
    for (const RoomSnapshot &room : snapshots) {
        lastLiveStatuses_.insert(room.roomId, room.liveStatus);
        NativeRoomRecord *record = libraryRecord(room.roomId);
        if (record == nullptr) {
            snapshot_.library.push_back({room.roomId, room.metadata, room.requestedQuality,
                                         room.requestedQualityRate, room.favorite, 0,
                                         room.volume, true});
            record = &snapshot_.library.back();
        }
        record->requestedQuality = room.requestedQuality;
        record->requestedQualityRate = room.requestedQualityRate;
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
        presentation.groupIds = groupIdsForRoom(room.roomId);
        presentation.groupId = presentation.groupIds.value(0);
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
                                    coordinator_->secondaryPrimaryRoomId(), coordinator_->audioRoomId());
    workspace_->setAudioPolicy(coordinator_->audioMode(), coordinator_->globalMuted());
    workspace_->setLayoutPresentation(coordinator_->layoutMode(),
                                      coordinator_->primaryRoomRatio());
    workspace_->setWorkspaceData(snapshot_.teams, snapshot_.groups, snapshot_.presets,
                                 snapshot_.activeGroupId);
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

QStringList AppController::groupIdsForRoom(const QString &roomId) const
{
    QStringList result;
    for (const NativeRoomGroup &group : snapshot_.groups) {
        if (group.roomIds.contains(roomId)) result.push_back(group.id);
    }
    return result;
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
