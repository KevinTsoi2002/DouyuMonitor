#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QHash>
#include <QWindow>
#include <QUrl>

#include <memory>

#include "ui/monitoring_model.h"
#include "ui/mpv_quick_item.h"
#include "ui/room_list_model.h"
#include "ui/workspace_model.h"
#include "danmaku/danmaku_controller.h"
#include "workspace/native_workspace_store.h"
#include "workspace/notification_policy.h"

class MultiRoomCoordinator;
class FavoriteMonitor;
class StreamgetProcessClient;
class SystemNotificationSink;
class WindowsNotificationService;
class WindowsTrayService;
class QSettings;
class UpdateChecker;

class AppController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(RoomListModel *rooms READ rooms CONSTANT)
    Q_PROPERTY(WorkspaceModel *workspace READ workspace CONSTANT)
    Q_PROPERTY(MonitoringModel *monitoring READ monitoring CONSTANT)
    Q_PROPERTY(DanmakuController *danmaku READ danmaku CONSTANT)
    Q_PROPERTY(QVariantList libraryRooms READ libraryRooms NOTIFY libraryRoomsChanged)
    Q_PROPERTY(QVariantMap notificationPreferences READ notificationPreferences
               NOTIFY notificationPreferencesChanged)
    Q_PROPERTY(bool backgroundHosted READ backgroundHosted NOTIFY backgroundHostedChanged)
    Q_PROPERTY(bool windowMinimized READ windowMinimized NOTIFY windowMinimizedChanged)
    Q_PROPERTY(QString closeBehavior READ closeBehavior NOTIFY closeBehaviorChanged)
    Q_PROPERTY(bool quitRequested READ quitRequested CONSTANT)
    Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchResultsChanged)
    Q_PROPERTY(QString searchStatus READ searchStatus NOTIFY searchStateChanged)
    Q_PROPERTY(QString searchError READ searchError NOTIFY searchStateChanged)
    Q_PROPERTY(QString updateState READ updateState NOTIFY updateStateChanged)
    Q_PROPERTY(QString updateMessage READ updateMessage NOTIFY updateStateChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY updateStateChanged)
    Q_PROPERTY(QUrl updateReleaseUrl READ updateReleaseUrl NOTIFY updateStateChanged)

public:
    explicit AppController(QString serviceProgram,
                           QSettings *settings,
                           SystemNotificationSink *notificationSink = nullptr,
                           DanmakuClientFactory danmakuFactory = {},
                           QObject *parent = nullptr);
    ~AppController() override;

    RoomListModel *rooms() noexcept;
    WorkspaceModel *workspace() noexcept;
    MonitoringModel *monitoring() noexcept;
    DanmakuController *danmaku() noexcept;
    QVariantList libraryRooms() const;
    QVariantMap notificationPreferences() const;
    QVariantList searchResults() const;
    QString searchStatus() const;
    QString searchError() const;
    QString updateState() const;
    QString updateMessage() const;
    QString currentVersion() const;
    QString latestVersion() const;
    QUrl updateReleaseUrl() const;
    bool backgroundHosted() const noexcept;
    bool windowMinimized() const noexcept;
    QString closeBehavior() const;
    bool quitRequested() const noexcept;
    QString fixedPlaybackMessage(const QString &errorCode) const;
    void setMainWindow(QWindow *window);

    Q_INVOKABLE QString addRoom(const QString &roomId);
    Q_INVOKABLE void searchRooms(const QString &query);
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE bool openLatestRelease();
    Q_INVOKABLE QString addRoomCandidate(const QString &roomId);
    Q_INVOKABLE QString removeRoom(const QString &roomId);
    Q_INVOKABLE void requestRemoveRoom(const QString &roomId);
    Q_INVOKABLE QString removeHistoryRoom(const QString &roomId);
    Q_INVOKABLE QString setPrimaryRoom(const QString &roomId);
    Q_INVOKABLE QString setSecondaryPrimaryRoom(const QString &roomId);
    Q_INVOKABLE QString setAudioRoom(const QString &roomId);
    Q_INVOKABLE bool setRoomMuted(const QString &roomId, bool muted);
    Q_INVOKABLE bool setAudioMode(const QString &mode);
    Q_INVOKABLE bool setGlobalMuted(bool muted);
    Q_INVOKABLE QString setQuality(const QString &roomId, int quality, int qualityRate = -1);
    Q_INVOKABLE QString setFavorite(const QString &roomId, bool favorite);
    Q_INVOKABLE QString moveFavoriteRoom(const QString &roomId, int targetIndex);
    Q_INVOKABLE QString moveRoom(const QString &roomId, int delta);
    Q_INVOKABLE QString retryPlayback(const QString &roomId);
    Q_INVOKABLE QString setVolume(const QString &roomId, int volume);
    Q_INVOKABLE void toggleDanmaku(const QString &roomId);
    Q_INVOKABLE QString createGroup(const QString &name);
    Q_INVOKABLE QString renameGroup(const QString &groupId, const QString &name);
    Q_INVOKABLE QString deleteGroup(const QString &groupId);
    Q_INVOKABLE QString assignRoomToGroup(const QString &roomId, const QString &groupId);
    Q_INVOKABLE QString removeRoomFromGroup(const QString &groupId, const QString &roomId);
    Q_INVOKABLE QString moveRoomInGroup(const QString &groupId,
                                        const QString &roomId,
                                        int delta);
    Q_INVOKABLE void setActiveGroup(const QString &groupId);
    Q_INVOKABLE bool setLayout(const QString &layoutId);
    Q_INVOKABLE bool setPrimaryRoomRatio(double ratio);
    Q_INVOKABLE bool setSidebarVisible(bool visible);
    Q_INVOKABLE QString saveWorkspacePreset(const QString &name);
    Q_INVOKABLE QString applyWorkspacePreset(const QString &presetId);
    Q_INVOKABLE QString deleteWorkspacePreset(const QString &presetId);
    Q_INVOKABLE QString setNotificationsEnabled(bool enabled);
    Q_INVOKABLE QString setNotificationPreferences(bool enabled,
                                                   bool roomOnline,
                                                   bool roomOffline,
                                                   bool playbackFailed,
                                                   bool playbackRecovered,
                                                   bool favoriteTitleChanged = true);
    Q_INVOKABLE void refreshRoom(const QString &roomId);
    Q_INVOKABLE void attachPlayer(const QString &roomId, MpvQuickItem *item);
    Q_INVOKABLE void detachPlayer(const QString &roomId, MpvQuickItem *item);
    Q_INVOKABLE void minimizeWindow();
    Q_INVOKABLE void restoreFromMinimized();
    Q_INVOKABLE void toggleMaximizedWindow();
    Q_INVOKABLE void toggleFullScreen();
    Q_INVOKABLE void exitFullScreen();
    Q_INVOKABLE void closeWindow();
    Q_INVOKABLE void requestClose();
    Q_INVOKABLE bool setCloseBehavior(const QString &behavior, bool persist = true);
    Q_INVOKABLE void clearCloseBehavior();
    Q_INVOKABLE void minimizeToBackground();
    Q_INVOKABLE void closeToTray();
    Q_INVOKABLE void restoreFromBackground();
    Q_INVOKABLE void requestQuit();
    Q_INVOKABLE void shutdown();

#ifdef DOUYU_TESTING
    int attachedPlayerCountForTest() const;
    bool serviceProcessRunningForTest() const;
    int activeDanmakuSessionCountForTest() const;
#endif

signals:
    void libraryRoomsChanged();
    void notificationPreferencesChanged();
    void searchResultsChanged();
    void searchStateChanged();
    void updateStateChanged();
    void backgroundHostedChanged();
    void windowMinimizedChanged();
    void closeBehaviorChanged();

private:
    void restoreWorkspace();
    void persistWorkspace();
    void onSnapshotsChanged(const RoomSnapshots &snapshots);
    void onServiceResponse(const ServiceResponse &response);
    void onServiceRequestFailed(quint64 requestId, const QString &errorCode);
    void onRoomStatusRefreshed(const QString &roomId, bool online);
    void synchronizeFavoriteMonitor();
    void onFavoriteRoomUpdated(const QString &roomId,
                               const RoomMetadata &metadata,
                               RoomLiveStatus liveStatus);
    void refreshPresentation();
    void synchronizeDanmaku();
    void touchHistory(const QString &roomId);
    NativeRoomRecord *libraryRecord(const QString &roomId);
    const NativeRoomRecord *libraryRecord(const QString &roomId) const;
    QStringList groupIdsForRoom(const QString &roomId) const;
    QString commandMessage(RoomCommandResult result) const;
    static bool isValidQuality(int quality) noexcept;
    static bool isValidName(const QString &name) noexcept;

    QSettings *settings_ = nullptr;
    std::unique_ptr<StreamgetProcessClient> service_;
    std::unique_ptr<MultiRoomCoordinator> coordinator_;
    std::unique_ptr<FavoriteMonitor> favoriteMonitor_;
    NativeWorkspaceStore workspaceStore_;
    NotificationPolicy notificationPolicy_;
    std::unique_ptr<WindowsNotificationService> notificationService_;
    std::unique_ptr<WindowsTrayService> trayService_;
    std::unique_ptr<RoomListModel> rooms_;
    std::unique_ptr<WorkspaceModel> workspace_;
    std::unique_ptr<MonitoringModel> monitoring_;
    std::unique_ptr<DanmakuController> danmaku_;
    NativeWorkspaceSnapshot snapshot_;
    QPointer<QWindow> mainWindow_;
    QWindow::Visibility preFullScreenVisibility_ = QWindow::Windowed;
    bool hadPreFullScreenVisibility_ = false;
    bool restoring_ = false;
    bool shuttingDown_ = false;
    bool backgroundHosted_ = false;
    bool windowMinimized_ = false;
    QString closeBehavior_ = QStringLiteral("ask");
    bool quitRequested_ = false;
    QVariantList searchResults_;
    QHash<QString, RoomMetadata> searchCandidates_;
    quint64 searchRequestId_ = 0;
    QString searchStatus_ = QStringLiteral("idle");
    QString searchError_;
    std::unique_ptr<UpdateChecker> updateChecker_;
    QHash<QString, RoomLiveStatus> lastLiveStatuses_;
    QHash<QString, RoomLiveStatus> favoriteLiveStatuses_;
};
