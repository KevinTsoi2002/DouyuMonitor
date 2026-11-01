#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlProperty>
#include <QQuickWindow>
#include <QQuickItem>
#include <QPointF>
#include <QSize>
#include <QDir>
#include <QImage>
#include <QFont>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QPointer>
#include <QJSValue>
#include <QtTest/QtTest>

#include "ui/mpv_quick_item.h"
#include "app/s1_archive.h"
#include "ui/room_list_model.h"
#include "ui/workspace_model.h"
#include "test_font_support.h"

#include <memory>

namespace {

class FakeMonitoringModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int onlineCount READ onlineCount CONSTANT)
    Q_PROPERTY(int offlineCount READ offlineCount CONSTANT)
    Q_PROPERTY(int errorCount READ errorCount CONSTANT)
    Q_PROPERTY(QString notificationStatus READ notificationStatus WRITE setNotificationStatus
               NOTIFY notificationStatusChanged)

public:
    int onlineCount() const { return 0; }
    int offlineCount() const { return 0; }
    int errorCount() const { return 0; }
    QString notificationStatus() const { return status_; }
    void setNotificationStatus(const QString &status)
    {
        if (status_ == status) return;
        status_ = status;
        emit notificationStatusChanged();
    }

signals:
    void notificationStatusChanged();

private:
    QString status_ = QStringLiteral("disabled");
};

class FakeSearchController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList searchResults READ searchResults CONSTANT)
    Q_PROPERTY(QString searchStatus READ searchStatus CONSTANT)
    Q_PROPERTY(QString searchError READ searchError CONSTANT)

public:
    QVariantList searchResults() const
    {
        return {QVariantMap{
            {QStringLiteral("roomId"), QStringLiteral("63136")},
            {QStringLiteral("anchorName"), QStringLiteral("主播")},
            {QStringLiteral("title"), QStringLiteral("直播标题")},
            {QStringLiteral("category"), QStringLiteral("游戏")},
            {QStringLiteral("viewerLabel"), QStringLiteral("1.2万")},
            {QStringLiteral("avatarUrl"), QUrl()},
            {QStringLiteral("online"), true},
            {QStringLiteral("statusKnown"), statusKnown},
        }};
    }

    QString searchStatus() const { return QStringLiteral("success"); }
    QString searchError() const { return {}; }
    Q_INVOKABLE void searchRooms(const QString &query)
    {
        ++searchCount;
        lastSearch = query;
    }
    Q_INVOKABLE QString addRoomCandidate(const QString &roomId)
    {
        addedRoomId = roomId;
        return {};
    }

    QString addedRoomId;
    QString lastSearch;
    int searchCount = 0;
    bool statusKnown = true;
};

class FakeHeaderController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(WorkspaceModel *workspace READ workspace CONSTANT)
    Q_PROPERTY(RoomListModel *rooms READ rooms CONSTANT)
    Q_PROPERTY(bool workspaceUnsaved READ workspaceUnsaved NOTIFY workspaceSaveStateChanged)

public:
    FakeHeaderController()
        : workspaceModel(nullptr, this)
    {
        workspaceModel.setAudioPolicy(QStringLiteral("single"), false);
    }

    WorkspaceModel *workspace() noexcept { return &workspaceModel; }
    RoomListModel *rooms() noexcept { return &roomModel; }
    bool workspaceUnsaved() const { return unsaved; }
    Q_INVOKABLE void retryWorkspaceSave() { ++saveRetries; unsaved = false; emit workspaceSaveStateChanged(); }
    bool unsaved = false;
    int saveRetries = 0;

    void setRoomCount(int count)
    {
        RoomSnapshots snapshots;
        snapshots.reserve(count);
        for (int index = 0; index < count; ++index) {
            RoomSnapshot snapshot;
            snapshot.roomId = QStringLiteral("room-%1").arg(index + 1);
            snapshots.append(snapshot);
        }
        roomModel.applySnapshots(snapshots);
    }

    Q_INVOKABLE bool setGlobalMuted(bool muted)
    {
        lastMuted = muted;
        workspaceModel.setAudioPolicy(workspaceModel.audioMode(), muted);
        return true;
    }

    Q_INVOKABLE bool setAudioMode(const QString &mode)
    {
        lastAudioMode = mode;
        workspaceModel.setAudioPolicy(mode, workspaceModel.globalMuted());
        return true;
    }

    Q_INVOKABLE bool toggleFullScreen()
    {
        fullScreenToggled = true;
        return true;
    }

    Q_INVOKABLE bool setLayout(const QString &layoutId)
    {
        lastLayout = layoutId;
        return true;
    }

    Q_INVOKABLE bool openExternalUrl(const QString &url)
    {
        externalUrl = url;
        return true;
    }

    WorkspaceModel workspaceModel;
    RoomListModel roomModel;
    bool lastMuted = false;
    QString lastAudioMode;
    QString lastLayout;
    bool fullScreenToggled = false;
    QString externalUrl;
signals:
    void workspaceSaveStateChanged();
};

class FakeRoomDanmakuController final : public QObject {
    Q_OBJECT

public:
    explicit FakeRoomDanmakuController(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    Q_INVOKABLE void retry(const QString &roomId)
    {
        retryRoom = roomId;
        ++retryCount;
    }

    QString retryRoom;
    int retryCount = 0;
};

class FakeRoomController final : public QObject {
    Q_OBJECT

    Q_PROPERTY(QObject *workspace READ workspaceObject CONSTANT)

public:
    FakeRoomController() : workspaceModel(nullptr, this), danmaku(this) {}

    Q_PROPERTY(QObject *danmaku READ danmakuObject CONSTANT)

    QObject *workspaceObject() noexcept { return &workspaceModel; }
    QObject *danmakuObject() noexcept { return &danmaku; }

    Q_INVOKABLE void attachPlayer(const QString &roomId, MpvQuickItem *player)
    {
        Q_UNUSED(player);
        attachedRooms.push_back(roomId);
    }

    Q_INVOKABLE void detachPlayer(const QString &roomId, MpvQuickItem *player)
    {
        Q_UNUSED(player);
        detachedRooms.push_back(roomId);
    }

    Q_INVOKABLE QString setVolume(const QString &roomId, int volume)
    {
        lastVolumeRoom = roomId;
        lastVolume = volume;
        return {};
    }

    Q_INVOKABLE QString setQuality(const QString &roomId, int quality, int qualityRate = -1)
    {
        lastQualityRoom = roomId;
        lastQuality = quality;
        lastQualityRate = qualityRate;
        return {};
    }

    Q_INVOKABLE void refreshRoom(const QString &roomId)
    {
        refreshedRoom = roomId;
    }

    Q_INVOKABLE QString moveRoom(const QString &roomId, int delta)
    {
        movedRoom = roomId;
        movedDelta = delta;
        return {};
    }

    Q_INVOKABLE QString removeRoom(const QString &roomId)
    {
        removedRoom = roomId;
        return {};
    }

    Q_INVOKABLE void requestRemoveRoom(const QString &roomId)
    {
        removedRoom = roomId;
    }

    Q_INVOKABLE void setFavorite(const QString &roomId, bool favorite)
    {
        favoriteRoom = roomId;
        favoriteValue = favorite;
    }

    Q_INVOKABLE QString setAudioRoom(const QString &roomId)
    {
        audioRoom = roomId;
        return audioError;
    }

    Q_INVOKABLE void setPrimaryRoom(const QString &roomId)
    {
        primaryRoom = roomId;
        ++setPrimaryRoomCount;
    }

    QString lastVolumeRoom;
    int lastVolume = -1;
    QString lastQualityRoom;
    int lastQuality = -1;
    int lastQualityRate = -2;
    QString refreshedRoom;
    QString movedRoom;
    int movedDelta = 0;
    QString removedRoom;
    QString favoriteRoom;
    bool favoriteValue = false;
    QString audioRoom;
    QString audioError;
    WorkspaceModel workspaceModel;
    QString primaryRoom;
    int setPrimaryRoomCount = 0;
    FakeRoomDanmakuController danmaku;
    QStringList attachedRooms;
    QStringList detachedRooms;
};

class FakePresetController final : public QObject {
    Q_OBJECT

public:
    Q_INVOKABLE QString saveWorkspacePreset(const QString &name) { savedName = name; ++saveCount; return saveResult; }
    QString saveResult;
    QString savedName;
    int saveCount = 0;
    Q_INVOKABLE QString applyWorkspacePreset(const QString &presetId)
    {
        appliedPresetId = presetId;
        return {};
    }
    Q_INVOKABLE QString deleteWorkspacePreset(const QString &presetId)
    {
        deletedPresetId = presetId;
        return {};
    }

    QString appliedPresetId;
    QString deletedPresetId;
};

class FakeGuildNavigationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList guildRoster READ guildRoster NOTIFY guildRosterChanged)
    Q_PROPERTY(QVariantList teams READ teams CONSTANT)

public:
    QVariantList guildRoster() const { return roster; }
    QVariantList teams() const { return teamItems; }

    Q_INVOKABLE QString setGuildMemberRoomId(const QString &memberId, const QString &roomId)
    {
        lastConfirmedMemberId = memberId;
        lastConfirmedRoomId = roomId;
        return {};
    }

    Q_INVOKABLE QString addGuildMemberRoom(const QString &memberId)
    {
        lastAddedMemberId = memberId;
        return {};
    }
    Q_INVOKABLE void refreshGuildLiveStatus() { ++liveRefreshCount; }

    QVariantList roster;
    QVariantList teamItems;
    QString lastConfirmedMemberId;
    QString lastConfirmedRoomId;
    QString lastAddedMemberId;
    int liveRefreshCount = 0;

signals:
    void guildRosterChanged();
};

class FakeGuildHoverController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList guildRoster READ guildRoster CONSTANT)
    Q_PROPERTY(QVariantList teams READ teams CONSTANT)

public:
    bool missingMetrics = false;
    QVariantList guildRoster() const
    {
        return {
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("hamster-001")},
                {QStringLiteral("anchorName"), QStringLiteral("寅子")},
                {QStringLiteral("pinyinKey"), QStringLiteral("Y")},
                {QStringLiteral("roomId"), QStringLiteral("71415")},
                {QStringLiteral("status"), QStringLiteral("resolved")},
                {QStringLiteral("avatarUrl"), QString()},
                {QStringLiteral("liveState"), QStringLiteral("online")},
                {QStringLiteral("active"), false},
                {QStringLiteral("role"), QStringLiteral("member")},
                {QStringLiteral("rankMatched"), true},
                {QStringLiteral("score"), 15.36},
                {QStringLiteral("radarDimensions"), QVariantList{
                     QVariantMap{{QStringLiteral("name"), QStringLiteral("力量")},
                                 {QStringLiteral("average"), 18.0},
                                 {QStringLiteral("count"), 100}},
                     QVariantMap{{QStringLiteral("name"), QStringLiteral("财力")},
                                 {QStringLiteral("average"), 12.0},
                                 {QStringLiteral("count"), 100}},
                 }},
                {QStringLiteral("placementAverage"), missingMetrics ? QVariant() : QVariant(88.5)},
                {QStringLiteral("placementScoredSessions"), 3},
                {QStringLiteral("playValue"), missingMetrics ? QVariant() : QVariant(6.4)},
                {QStringLiteral("playValueBombed"), 1},
                {QStringLiteral("playValueUpdatedAt"),
                 QStringLiteral("2026-09-30T12:26:01+08:00")},
            },
        };
    }

    QVariantList teams() const
    {
        return {
            QVariantMap{{QStringLiteral("id"), QStringLiteral("team-a")},
                        {QStringLiteral("name"), QStringLiteral("一队")},
                        {QStringLiteral("memberIds"), QStringList{
                             QStringLiteral("hamster-001")}}},
        };
    }
};

class FakeTeamManagerController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList guildRoster READ guildRoster CONSTANT)
    Q_PROPERTY(QVariantMap maoziTeamImport READ maoziTeamImport NOTIFY maoziTeamImportChanged)

public:
    FakeTeamManagerController()
        : workspaceModel(nullptr, this)
    {
    }

    QVariantList guildRoster() const { return {}; }
    QVariantMap maoziTeamImport() const { return importState; }
    void setImportState(QVariantMap state) {
        importState = std::move(state);
        emit maoziTeamImportChanged();
    }
    Q_INVOKABLE void previewMaoziTeamImport() {
        ++previewCount;
        setImportState({{"state", "loading"}, {"canConfirm", false}});
    }
    Q_INVOKABLE QString confirmMaoziTeamImport() {
        ++confirmCount;
        setImportState({{"state", "idle"}});
        return {};
    }
    Q_INVOKABLE void cancelMaoziTeamImport() {
        ++cancelCount;
        setImportState({{"state", "idle"}});
    }

    void addTeam(const QString &id, const QString &name)
    {
        teams_.push_back({id, name, {}});
        workspaceModel.setWorkspaceData(teams_, {}, {}, {});
    }

    Q_INVOKABLE QString deleteTeam(const QString &teamId)
    {
        deletedTeamId = teamId;
        for (auto it = teams_.begin(); it != teams_.end(); ++it) {
            if (it->id != teamId) continue;
            teams_.erase(it);
            workspaceModel.setWorkspaceData(teams_, {}, {}, {});
            return {};
        }
        return QStringLiteral("未找到该队伍");
    }

    WorkspaceModel workspaceModel;
    QString deletedTeamId;
    int previewCount = 0, confirmCount = 0, cancelCount = 0;
    QVariantMap importState{{"state", "idle"}};

signals:
    void maoziTeamImportChanged();

private:
    QVector<NativeTeam> teams_;
};

class FakeNotificationPreferencesController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap notificationPreferences READ notificationPreferences
                   NOTIFY notificationPreferencesChanged)

public:
    QVariantMap notificationPreferences() const { return preferences_; }

    Q_INVOKABLE void setNotificationPreferences(bool enabled,
                                                bool roomOnline,
                                                bool roomOffline,
                                                bool playbackFailed,
                                                bool playbackRecovered,
                                                bool favoriteTitleChanged)
    {
        ++saveCount;
        preferences_ = {{QStringLiteral("enabled"), enabled},
                        {QStringLiteral("roomOnline"), roomOnline},
                        {QStringLiteral("roomOffline"), roomOffline},
                        {QStringLiteral("playbackFailed"), playbackFailed},
                        {QStringLiteral("playbackRecovered"), playbackRecovered},
                        {QStringLiteral("favoriteTitleChanged"), favoriteTitleChanged}};
        emit notificationPreferencesChanged();
    }

    int saveCount = 0;

signals:
    void notificationPreferencesChanged();

private:
    QVariantMap preferences_{{QStringLiteral("enabled"), true},
                            {QStringLiteral("roomOnline"), true},
                            {QStringLiteral("roomOffline"), true},
                            {QStringLiteral("playbackFailed"), true},
                            {QStringLiteral("playbackRecovered"), true},
                            {QStringLiteral("favoriteTitleChanged"), true}};
};

class FakeUpdateController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString updateState READ updateState NOTIFY updateStateChanged)
    Q_PROPERTY(QString updateMessage READ updateMessage NOTIFY updateStateChanged)
    Q_PROPERTY(QUrl updateReleaseUrl READ updateReleaseUrl NOTIFY updateStateChanged)
    Q_PROPERTY(QString closeBehavior READ closeBehavior NOTIFY closeBehaviorChanged)
    Q_PROPERTY(bool closeBehaviorUnsaved READ closeBehaviorUnsaved
               NOTIFY closeBehaviorSaveStateChanged)
    Q_PROPERTY(QString closeBehaviorError READ closeBehaviorError
               NOTIFY closeBehaviorSaveStateChanged)
    Q_PROPERTY(QString eventMappingJson READ eventMappingJson CONSTANT)
    Q_PROPERTY(QVariantList guildRoster READ guildRoster CONSTANT)

public:
    QString updateState() const { return updateState_; }
    QString updateMessage() const { return updateMessage_; }
    QUrl updateReleaseUrl() const { return updateReleaseUrl_; }
    QString closeBehavior() const { return closeBehavior_; }
    bool closeBehaviorUnsaved() const { return closeBehaviorUnsaved_; }
    QString closeBehaviorError() const { return closeBehaviorError_; }
    QString eventMappingJson() const {
        QJsonArray members;
        for (int index = 0; index < memberCount; ++index) {
            members.append(QJsonObject{
                {QStringLiteral("roomId"), QString::number(123 + index)},
                {QStringLiteral("name"), index == 0 ? QStringLiteral("Anchor")
                                                    : QStringLiteral("Anchor %1").arg(index)},
                {QStringLiteral("role"), QStringLiteral("member")},
                {QStringLiteral("team"), QJsonValue::Null}});
        }
        return QString::fromUtf8(QJsonDocument(QJsonObject{
            {QStringLiteral("version"), 1}, {QStringLiteral("event"), QStringLiteral("fixture")},
            {QStringLiteral("members"), members}}).toJson(QJsonDocument::Compact));
    }
    Q_INVOKABLE void setCloseBehavior(const QString &value, bool remember) {
        closeBehavior_ = value;
        rememberedChoice = remember;
        emit closeBehaviorChanged();
    }
    Q_INVOKABLE void clearCloseBehavior() {
        closeBehavior_ = QStringLiteral("ask");
        ++clearCount;
        emit closeBehaviorChanged();
    }
    Q_INVOKABLE bool retryCloseBehaviorSave() {
        ++closeRetryCount;
        closeBehaviorUnsaved_ = false;
        closeBehaviorError_.clear();
        emit closeBehaviorSaveStateChanged();
        return true;
    }
    Q_INVOKABLE void closeToTray() { ++closeToTrayCount; }
    Q_INVOKABLE void requestQuit() { ++requestQuitCount; }
    Q_INVOKABLE void failCloseBehaviorSave() {
        closeBehaviorUnsaved_ = true;
        closeBehaviorError_ = QStringLiteral("关闭行为保存失败，请重试");
        emit closeBehaviorSaveStateChanged();
    }
    int memberCount = 1;
    int clearCount = 0;
    bool rememberedChoice = false;
    QVariantList guildRoster() const { return roster; }
    QVariantList roster;
    Q_INVOKABLE QString saveEventMapping(const QString &mapping) { savedMapping = mapping; return {}; }
    QString savedMapping;

    Q_INVOKABLE void checkForUpdates() { ++checkCount; }
    Q_INVOKABLE bool openLatestRelease()
    {
        ++openCount;
        return true;
    }

    void setUpdate(QString state, QString message, QUrl releaseUrl = {})
    {
        updateState_ = std::move(state);
        updateMessage_ = std::move(message);
        updateReleaseUrl_ = std::move(releaseUrl);
        emit updateStateChanged();
    }

    int checkCount = 0;
    int openCount = 0;

signals:
    void closeBehaviorChanged();
    void closeBehaviorSaveStateChanged();
    void updateStateChanged();

private:
    QString closeBehavior_ = QStringLiteral("ask");
    bool closeBehaviorUnsaved_ = false;
    QString closeBehaviorError_;
public:
    int closeRetryCount = 0;
    int closeToTrayCount = 0;
    int requestQuitCount = 0;
    QString updateState_ = QStringLiteral("idle");
    QString updateMessage_;
    QUrl updateReleaseUrl_;
};

class FakeDanmakuSettingsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap governanceSettings READ governanceSettings NOTIFY settingsChanged)
    Q_PROPERTY(QVariantMap displaySettings READ displaySettings CONSTANT)
    Q_PROPERTY(bool globalEnabled READ globalEnabled CONSTANT)

public:
    explicit FakeDanmakuSettingsController(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    QVariantMap governanceSettings() const { return globalSettings_; }
    QVariantMap displaySettings() const
    {
        return {{QStringLiteral("durationSeconds"), 8},
                {QStringLiteral("fontSize"), 24},
                {QStringLiteral("opacity"), 0.9},
                {QStringLiteral("region"), QStringLiteral("full")},
                {QStringLiteral("density"), QStringLiteral("normal")},
                {QStringLiteral("fontFamily"), QStringLiteral("microsoft-yahei")},
                {QStringLiteral("rendering"), QStringLiteral("native")}};
    }
    bool globalEnabled() const { return true; }

    Q_INVOKABLE QVariantMap governanceSettingsForRoom(const QString &roomId) const
    {
        QVariantMap result = globalSettings_;
        const QVariantMap override = roomOverrides_.value(roomId);
        for (auto it = override.cbegin(); it != override.cend(); ++it) result.insert(it.key(), it.value());
        return result;
    }

    Q_INVOKABLE void setGovernanceSetting(const QString &roomId,
                                          const QString &key,
                                          const QVariant &value)
    {
        (roomId.isEmpty() ? globalSettings_ : roomOverrides_[roomId]).insert(key, value);
        emit settingsChanged();
    }

    Q_INVOKABLE void clearRoomGovernanceOverride(const QString &roomId)
    {
        roomOverrides_.remove(roomId);
        emit settingsChanged();
    }

    Q_INVOKABLE QVariantMap statsForRoom(const QString &roomId) const
    {
        return stats_.value(roomId,
                            QVariantMap{{QStringLiteral("level"), QStringLiteral("normal")},
                                        {QStringLiteral("recentRate"), 0},
                                        {QStringLiteral("peakRate"), 0},
                                        {QStringLiteral("filtered"), 0},
                                        {QStringLiteral("duplicates"), 0},
                                        {QStringLiteral("rateLimited"), 0},
                                        {QStringLiteral("queueOverflow"), 0},
                                        {QStringLiteral("upstreamDropped"), 0}});
    }

    Q_INVOKABLE void clearStats(const QString &roomId)
    {
        stats_.remove(roomId);
        emit statisticsChanged(roomId);
    }

    void setStats(const QString &roomId, QVariantMap stats)
    {
        QVariantMap normalized{{QStringLiteral("level"), QStringLiteral("normal")},
                               {QStringLiteral("recentRate"), 0},
                               {QStringLiteral("peakRate"), 0},
                               {QStringLiteral("filtered"), 0},
                               {QStringLiteral("duplicates"), 0},
                               {QStringLiteral("rateLimited"), 0},
                               {QStringLiteral("queueOverflow"), 0},
                               {QStringLiteral("upstreamDropped"), 0}};
        for (auto it = stats.cbegin(); it != stats.cend(); ++it) normalized.insert(it.key(), it.value());
        stats_.insert(roomId, std::move(normalized));
        emit statisticsChanged(roomId);
    }

signals:
    void settingsChanged();
    void statisticsChanged(const QString &roomId);

private:
    QVariantMap globalSettings_{{QStringLiteral("enabled"), true},
                                {QStringLiteral("keywordBlacklist"), QStringList{}},
                                {QStringLiteral("duplicateWindowSeconds"), 3},
                                {QStringLiteral("peakProtectionEnabled"), true}};
    QHash<QString, QVariantMap> roomOverrides_;
    QHash<QString, QVariantMap> stats_;
};

class FakeDanmakuAppController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *danmaku READ danmaku CONSTANT)

public:
    FakeDanmakuAppController() : danmaku_(this) {}
    QObject *danmaku() noexcept { return &danmaku_; }
    FakeDanmakuSettingsController danmaku_;
};

class FakeMaoziRankClient final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QVariantList placementEntries READ placementEntries NOTIFY entriesChanged)
    Q_PROPERTY(QVariantMap placementColumns READ placementColumns NOTIFY entriesChanged)
    Q_PROPERTY(QVariantList playValueEntries READ playValueEntries NOTIFY entriesChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY entriesChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY entriesChanged)

public:
    QVariantList entries() const
    {
        if (hasEntriesOverride) return entriesOverride;
        return {
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("host-1")},
                {QStringLiteral("rank"), 1},
                {QStringLiteral("name"), QStringLiteral("寅子")},
                {QStringLiteral("roomId"), QStringLiteral("71415")},
                {QStringLiteral("note"), QStringLiteral("猴王")},
                {QStringLiteral("teamName"), QStringLiteral("红队")},
                {QStringLiteral("grade"), QStringLiteral("S")},
                {QStringLiteral("gradeColor"), QStringLiteral("#ffc93c")},
                {QStringLiteral("score"), 18.5},
                {QStringLiteral("voters"), 120},
                {QStringLiteral("live"), true},
                {QStringLiteral("posterUrl"), QString()},
            },
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("host-2")},
                {QStringLiteral("rank"), 2},
                {QStringLiteral("name"), QStringLiteral("主播阿飞")},
                {QStringLiteral("roomId"), QStringLiteral("84452")},
                {QStringLiteral("note"), QString()},
                {QStringLiteral("teamName"), QStringLiteral("蓝队")},
                {QStringLiteral("grade"), QStringLiteral("A")},
                {QStringLiteral("gradeColor"), QStringLiteral("#a78bfa")},
                {QStringLiteral("score"), 12.0},
                {QStringLiteral("voters"), 80},
                {QStringLiteral("live"), false},
                {QStringLiteral("posterUrl"), QString()},
            },
        };
    }

    bool loading() const { return loading_; }
    QString statusText() const { return QStringLiteral("共 2 位主播 · 200 人参与"); }
    QVariantList placementEntries() const
    {
        return {
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("host-1")},
                {QStringLiteral("name"), QStringLiteral("寅子")},
                {QStringLiteral("roomId"), QStringLiteral("71415")},
                {QStringLiteral("posterUrl"), QString()},
                {QStringLiteral("placementRank"), 1},
                {QStringLiteral("placementAverage"), 88.5},
                {QStringLiteral("placementScoredSessions"), 3},
                {QStringLiteral("placementSessions"),
                 QVariantMap{{QStringLiteral("2026-09-28:1"), 94.0},
                             {QStringLiteral("2026-09-28:2"), 88.4},
                             {QStringLiteral("2026-09-29:1"), 83.1}}},
            },
        };
    }
    QVariantMap placementColumns() const
    {
        return {
            {QStringLiteral("2026-09-28:1"),
             QVariantMap{{QStringLiteral("key"), QStringLiteral("2026-09-28:1")},
                         {QStringLiteral("label"), QStringLiteral("28午")}}},
            {QStringLiteral("2026-09-28:2"),
             QVariantMap{{QStringLiteral("key"), QStringLiteral("2026-09-28:2")},
                         {QStringLiteral("label"), QStringLiteral("28晚")}}},
            {QStringLiteral("2026-09-29:1"),
             QVariantMap{{QStringLiteral("key"), QStringLiteral("2026-09-29:1")},
                         {QStringLiteral("label"), QStringLiteral("29午")}}},
        };
    }
    QVariantList playValueEntries() const
    {
        return {
            QVariantMap{
                {QStringLiteral("name"), QStringLiteral("尐表哥")},
                {QStringLiteral("points"), 10600.0},
                {QStringLiteral("roomId"), QStringLiteral("217331")},
                {QStringLiteral("teamName"), QStringLiteral("未分队")},
                {QStringLiteral("role"), QStringLiteral("captain")},
                {QStringLiteral("posterUrl"), QString()},
                {QStringLiteral("live"), true},
                {QStringLiteral("rank"), 1},
            },
        };
    }

    Q_INVOKABLE void refresh()
    {
        ++refreshCount;
        emit entriesChanged();
    }

    int refreshCount = 0;
    bool hasEntriesOverride = false;
    QVariantList entriesOverride;
    void setEntries(QVariantList entries)
    {
        hasEntriesOverride = true;
        entriesOverride = std::move(entries);
        emit entriesChanged();
    }

signals:
    void entriesChanged();

private:
    bool loading_ = false;
};

class FakeMaoziController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(FakeMaoziRankClient *maoziRank READ maoziRank CONSTANT)
    Q_PROPERTY(QVariantMap s1Archive READ s1Archive CONSTANT)
    Q_PROPERTY(QVariantList guildRoster READ guildRoster NOTIFY guildRosterChanged)

public:
    FakeMaoziRankClient *maoziRank() noexcept { return &rank; }
    QVariantList guildRoster() const { return roster; }
    void setRoster(QVariantList value) { roster = std::move(value); emit guildRosterChanged(); }
    QVariantMap s1Archive() const
    {
        if (useBundledArchive) return S1Archive::bundled();
        const QVariantMap regular{{QStringLiteral("id"), QStringLiteral("match-a")},
                                  {QStringLiteral("title"), QStringLiteral("常规测试赛")},
                                  {QStringLiteral("detail"), QStringLiteral("逐场详情")}};
        const QVariantMap final{{QStringLiteral("id"), QStringLiteral("final-a")},
                                {QStringLiteral("title"), QStringLiteral("决赛测试赛")},
                                {QStringLiteral("detail"), QStringLiteral("决赛详情")}};
        const QVariantMap football{{QStringLiteral("id"), QStringLiteral("g0")},
                                   {QStringLiteral("summary"), QStringLiteral("G1 测试赛")},
                                   {QStringLiteral("detail"), QVariantMap{{QStringLiteral("sections"),
                                       QVariantList{QVariantMap{{QStringLiteral("text"),
                                           QStringLiteral("进球事件")}}}}}}};
        return {
            {QStringLiteral("version"), 1},
            {QStringLiteral("capturedAt"), QStringLiteral("2026-10-27")},
            {QStringLiteral("ranking"), QVariantMap{{QStringLiteral("participants"), rank.entries()}}},
            {QStringLiteral("regularSeason"), QVariantMap{{QStringLiteral("matches"), QVariantList{regular}}}},
            {QStringLiteral("finals"), QVariantMap{{QStringLiteral("matches"), QVariantList{final}}}},
            {QStringLiteral("football"), QVariantMap{{QStringLiteral("matches"), QVariantList{football}}}},
            {QStringLiteral("placement"), QVariantMap{{QStringLiteral("participants"), rank.placementEntries()},
                                                       {QStringLiteral("sessions"), QVariantList{}}}},
        };
    }
    Q_INVOKABLE bool openExternalUrl(const QString &url)
    {
        lastExternalUrl = url;
        return true;
    }

    FakeMaoziRankClient rank;
    bool useBundledArchive = false;
    QString lastExternalUrl;
    QVariantList roster;
signals:
    void guildRosterChanged();
};

void registerQmlTypes()
{
    static const int registered = qmlRegisterType<MpvQuickItem>("DouyuNative", 1, 0, "MpvQuickItem");
    Q_UNUSED(registered);
}

QQuickWindow *loadWindow(QQmlApplicationEngine &engine)
{
    registerQmlTypes();
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return nullptr;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (window == nullptr) return nullptr;

    window->resize(QSize(1280, 720));
    window->show();
    return window;
}

void click(QObject *object)
{
    QVERIFY(object != nullptr);
    QVERIFY(QMetaObject::invokeMethod(object, "clicked"));
}

void pointerClick(QQuickWindow *window, QQuickItem *item)
{
    QVERIFY(item != nullptr);
    QVERIFY(item->isVisible());
    QVERIFY(item->isEnabled());
    window->requestActivate();
    QTRY_VERIFY(window->isActive());
    // Settle the popup/section layout before converting the target to scene coordinates.
    QTest::qWait(50);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
                      item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint());
}

QRectF sceneBounds(QQuickItem *item)
{
    return {item->mapToScene(QPointF()), item->size()};
}

QVariantMap roomTileProperties(const QString &danmakuState)
{
    return {
        {QStringLiteral("roomId"), QStringLiteral("63136")},
        {QStringLiteral("anchorName"), QStringLiteral("主播")},
        {QStringLiteral("title"), QStringLiteral("标题")},
        {QStringLiteral("category"), QStringLiteral("游戏")},
        {QStringLiteral("viewerLabel"), QStringLiteral("1.2万")},
        {QStringLiteral("avatarUrl"), QUrl(QStringLiteral("https://example.com/avatar.jpg"))},
        {QStringLiteral("liveState"), QStringLiteral("online")},
        {QStringLiteral("playbackState"), QStringLiteral("playing")},
        {QStringLiteral("primary"), true},
        {QStringLiteral("favorite"), false},
        {QStringLiteral("audioFocused"), false},
        {QStringLiteral("requestedQuality"), QStringLiteral("auto")},
        {QStringLiteral("requestedQualityRate"), -1},
        {QStringLiteral("effectiveQuality"), QStringLiteral("auto")},
        {QStringLiteral("availableQualities"), QVariantList{
            QVariantMap{{QStringLiteral("id"), QStringLiteral("auto")},
                        {QStringLiteral("label"), QStringLiteral("自动")},
                        {QStringLiteral("rate"), 4}},
            QVariantMap{{QStringLiteral("id"), QStringLiteral("rate-3")},
                        {QStringLiteral("label"), QStringLiteral("高清")},
                        {QStringLiteral("rate"), 3}},
        }},
        {QStringLiteral("muted"), true},
        {QStringLiteral("volume"), 100},
        {QStringLiteral("danmakuEnabled"), true},
        {QStringLiteral("danmakuState"), danmakuState},
        {QStringLiteral("danmakuErrorCode"), QStringLiteral("NONE")},
        {QStringLiteral("renderEnabled"), true},
        {QStringLiteral("index"), 0},
    };
}

QList<QQuickItem *> roomTiles(QQuickItem *surface)
{
    QList<QQuickItem *> tiles;
    for (QQuickItem *child : surface->childItems()) {
        if (child->property("roomId").isValid()) tiles.push_back(child);
    }
    return tiles;
}

QQuickItem *quickItemByObjectName(QQuickItem *root, const QString &objectName)
{
    if (!root) return nullptr;
    if (root->objectName() == objectName) return root;
    for (QQuickItem *child : root->childItems()) {
        if (QQuickItem *match = quickItemByObjectName(child, objectName)) return match;
    }
    return nullptr;
}

QList<QQuickItem *> quickItemsByObjectName(QQuickItem *root, const QString &objectName)
{
    QList<QQuickItem *> items;
    if (root->objectName() == objectName) items.append(root);
    for (QQuickItem *child : root->childItems()) items.append(quickItemsByObjectName(child, objectName));
    return items;
}

QVariant qmlData(QObject *object, const char *property)
{
    const QVariant value = object->property(property);
    return value.metaType() == QMetaType::fromType<QJSValue>() ? value.value<QJSValue>().toVariant() : value;
}

QQuickItem *archiveDetailRoot(QQuickItem *child)
{
    for (auto *item = child; item != nullptr; item = item->parentItem()) {
        if (item->property("record").isValid() && item->property("matchRecord").isValid()) return item;
    }
    return nullptr;
}

} // namespace

class QmlInteractionTest final : public QObject {
    Q_OBJECT

private slots:
    void keepsBottomRowRoomMenuInsideWindowAndClosesOutside();
    void preservesPresetNameUntilSaveSucceeds();
    void keepsKeyboardRoomControlsVisibleAndHiddenBarsInactive();
    void watchToolsOverlayPreservesWorkspaceGeometry();
    void navigatesWatchToolsInlineWithPointerAndKeyboard();
    void roomDrawerHasOneAddEntry();
    void watchSurfacesReplaceEachOtherAndLayoutSupportsKeyboard();
    void watchToolsShortcutProtectsUnnamedEditableControls();
    void immersiveToolsCloseBeforeFullscreenAndRestoreFocus();
    void themeChangesKeepWorkspaceInstances();
    void countsConnectedDanmakuFromRoomStates();
    void initTestCase();
    void opensAddRoomDialogAndRejectsInvalidRoomId();
    void opensMonitoringDanmakuAndWorkspaceSurfaces();
    void reusesSurfacesAcrossRepeatedViewSwitches();
    void rendersUnavailableNotificationStatus();
    void closesTransientSurfacesFromExplicitActions();
    void togglesSidebarAndHandlesRetainedShortcuts();
    void handlesLegacyShortcutParity();
    void rendersOnlineStatusForOnlineToken();
    void showsDanmakuDisplayGovernanceAndStatsTabs();
    void readsSelectedRoomGovernanceOverride();
    void refreshesSelectedRoomStatsFromRoomSignal();
    void showsDanmakuStatusOnRoomTile();
    void reportsAudioFocusCommandErrorsFromRoomActions();
    void rendersRoomAvatarFromModelRole();
    void placesSidebarToggleBeforeBrandAndOpensHistory();
    void placesNavigationEntryAfterSidebarToggleAndBeforeBrand();
    void makesRoomListAndNavigationPanelsMutuallyExclusive();
    void closesToastFromQml();
    void autoDismissesToastBySeverity();
    void usesFramelessWindowWithTitleBarInteractions();
    void keepsTaskbarMinimizeCapabilityForFramelessWindow();
    void updatesMaximizeIconWithWindowState();
    void doubleClicksOnlyRoomPictureForFullscreen();
    void doesNotExposeGroupManagementControls();
    void rendersAndAddsSearchCandidate();
    void exposesSupportedLayoutOptions();
    void alignsLayoutMenuSelectionIndicator();
    void enablesDualPrimaryLayoutAfterFourthRoomIsAdded();
    void laysOutFiveAutomaticRoomsInThreeAndTwoRows();
    void laysOutPrimaryRoomsAcrossFullHeight();
    void exposesGlobalAudioControls();
    void distinguishesWorkspaceAndLayoutActions();
    void usesGroupedHeaderControls();
    void groupsSoundControlsAndExposesFullscreen();
    void positionsSoundPopoverLikeDanmakuPanel();
    void truncatesLongRoomTitleBeforeActions();
    void keepsSidebarMetadataClearOfActionsForLongTitles();
    void keepsCurrentRoomHoverClearOfActions();
    void keepsHistoryHoverClearOfActions();
    void keepsFavoritesHoverClearOfActions();
    void exposesRoomVolumeAndRefreshControls();
    void hidesInactiveRoomControlsWithRetainedFocus();
    void hidesRoomControlsAfterMenuCloses();
    void keepsControlsVisibleDuringQualityPopupInteraction();
    void hidesInitiallyVisibleRoomControlsWithoutPointerEntry();
    void switchesRoomQualityByStreamRate();
    void keepsQualityLabelGeometryWhileHoveringSelector();
    void hidesRoomQualitySelectorIndicator();
    void rendersQualityPopupOptions();
    void rendersAvailableQualitiesFromModelRole();
    void defersRoomRemovalUntilAfterQmlHandlerReturns();
    void rebindsPlayerWhenRoomIdentityChanges();
    void rendersFallbackMetadataAndUnknownStatus();
    void exposesRoomOrderingControls();
    void disablesOrderingAtListBoundaries();
    void keepsSidebarActionsWithinRoomRows();
    void groupsHeaderActionsWithStableTooltips();
    void doesNotExposeRoomDragAndDropSurface();
    void deletesWorkspacePresetFromPanel();
    void defersWorkspacePresetApplyUntilPopupHandlerReturns();
    void refreshesRoomSidebarAfterPresetLikeModelUpdate();
    void keepsTransientDialogSurfacesDark();
    void keepsInputAndMenuControlsOnDarkTheme();
    void exposesFavoriteTitleNotificationPreference();
    void preservesDialogEscapeCancelAndFocusSemantics();
    void keepsTeamManagerReachableAtShortViewport();
    void checksForUpdatesFromSettingsPage();
    void managesTeamsOnlyFromSettings();
    void deletesTeamFromManagerRow();
    void previewsAndConfirmsTeamImport();
    void listsGuildNavigationWithoutTeamGroups();
    void filtersGuildNavigationByGenderStatusAndSearch();
    void clearsFilteredGuildSelectionButKeepsMatchingLiveUpdates();
    void quickAddsResolvedGuildMember();
    void submitsManualGuildRoomId();
    void rendersGuildMemberAvatarAndLiveState();
    void ordersGuildNavigationByPublicDirectory();
    void checksRankVersionOnlyWhileGuildNavigationIsVisible();
    void hoverCardOmitsDetailsButton();
    void highlightsWholeGuildRowWithoutNameStrip();
    void compactsGuildHoverCardWithoutRankData();
    void opensMaoziRankPageFromHeader();
    void filtersAndRefreshesMaoziRankPage();
    void browsesCompleteS1Archive();
    void updatesRankPortraitsWithoutChangingArchiveOrGuessingNames();
    void retriesFailedRankPortraitsAndResetsForNewSources();
    void preservesMissingRadarValuesAndValidZero();
    void keepsAllStructuredArchiveDetailsAndReturnContext();
    void keepsVideoControlsDarkAndQualityKeyboardUsableAcrossThemes();
    void showsCstgTerminalName();
    void editsEventMappingFromSettings();
    void laysOutSettingsAcrossWidths();
    void scrollsLongSettingsRosterWithoutClipping();
    void preservesSettingsCloseBehaviorActions();
    void showsCloseBehaviorPersistenceRetry();
    void eventMappingPopupsUseDarkTheme();
    void updatesGuildStatusWithoutRecreatingRows();
    void keepsGuildAddHoverStableWithoutDataPopup();
    void showsHoverMetricsInKeyboardMemberDetails_data();
    void showsHoverMetricsInKeyboardMemberDetails();
    void rendersUnknownSearchStatus();
    void retriesUnsavedWorkspaceFromPersistentWarning();
};

void QmlInteractionTest::initTestCase()
{
    QVERIFY2(DouyuTestSupport::prepareTestFonts(),
             "No Simplified Chinese font is available for QML tests (check QT_QPA_FONTDIR)");
}

void QmlInteractionTest::listsGuildNavigationWithoutTeamGroups()
{
    FakeGuildNavigationController controller;
    controller.roster = {
        QVariantMap{{"id", "one"}, {"anchorName", "主播一"}, {"roomId", "101"}, {"role", "member"},
                    {"avatarUrl", "qrc:/missing/guild-avatar.png"},
                    {"fallbackAvatarUrl", "qrc:/qml/assets/brand/icon-navigation.png"},
                    {"portraitIdentityVerified", true}},
        QVariantMap{{"id", "two"}, {"anchorName", "主播二"}, {"roomId", "102"}, {"role", "leader"}},
    };
    controller.teamItems = {QVariantMap{{"id", "team"}, {"name", "旧队伍"}, {"memberIds", QStringList{"two", "one"}}}};
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl("qrc:/qml/components/GuildNavigationPanel.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({{"width", 880}, {"height", 564},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))}}));
    QVERIFY(panel);
    auto *rows = panel->findChild<QObject *>("guildVisibleRows");
    QVERIFY(rows);
    QCOMPARE(rows->property("count").toInt(), 2);
    QVERIFY(!panel->findChild<QObject *>("guildTeamTitle"));
    QVERIFY(!panel->findChild<QObject *>("guildRoleTitle"));
    QVERIFY(!panel->findChild<QObject *>("guildTeamFilter"));
    for (int i=0; i<2; ++i) {
        QQuickItem *row = nullptr;
        QVERIFY(QMetaObject::invokeMethod(rows, "itemAt", Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, i)));
        QVERIFY(row);
        auto *member = row->findChild<QObject *>("guildMemberRow");
        QVERIFY(member);
        QCOMPARE(member->property("memberId").toString(), i == 0 ? QString("one") : QString("two"));
        if (i == 0) {
            auto *avatar = member->findChild<QObject *>("guildMemberAvatarImage");
            QVERIFY(avatar);
            QTRY_COMPARE(avatar->property("source").toString(),
                         QStringLiteral("qrc:/qml/assets/brand/icon-navigation.png"));
            QTRY_COMPARE(avatar->property("status").toInt(), 1);
    QSignalSpy avatarSourceChanges(avatar, SIGNAL(sourceChanged(const QUrl &)));
            QVERIFY(avatarSourceChanges.isValid());
            QVariantMap updatedMember = controller.roster.at(0).toMap();
            updatedMember.insert(QStringLiteral("liveState"), QStringLiteral("online"));
            controller.roster[0] = updatedMember;
            emit controller.guildRosterChanged();
            QTRY_COMPARE(avatar->property("source").toString(),
                         QStringLiteral("qrc:/qml/assets/brand/icon-navigation.png"));
            QCOMPARE(avatarSourceChanges.count(), 0);
        }
    }
}

void QmlInteractionTest::filtersGuildNavigationByGenderStatusAndSearch()
{
    FakeGuildNavigationController controller;
    controller.roster = {
        QVariantMap{{"id", "m"}, {"anchorName", "男主播"}, {"roomId", "101"}, {"gender", "m"}, {"liveState", "online"}},
        QVariantMap{{"id", "f"}, {"anchorName", "女主播"}, {"roomId", "102"}, {"gender", "f"}, {"liveState", "offline"}, {"active", true}},
        QVariantMap{{"id", "old"}, {"anchorName", "旧成员"}, {"gender", "f"}, {"directoryListed", false}}
    };
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl("qrc:/qml/components/GuildNavigationPanel.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({{"width", 880}, {"height", 564},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))}}));
    QVERIFY(panel);
    auto *search = panel->findChild<QObject *>("guildNavigationSearch");
    QVERIFY(search);
    auto *rows = panel->findChild<QObject *>("guildVisibleRows");
    QVERIFY(rows);
    QQuickItem *male = nullptr;
    QQuickItem *female = nullptr;
    QVERIFY(QMetaObject::invokeMethod(rows, "itemAt", Q_RETURN_ARG(QQuickItem *, male), Q_ARG(int, 0)));
    QVERIFY(QMetaObject::invokeMethod(rows, "itemAt", Q_RETURN_ARG(QQuickItem *, female), Q_ARG(int, 1)));
    QVERIFY(male && female);
    QPointer<QQuickItem> originalMale(male), originalFemale(female);
    QCOMPARE(panel->property("visibleMemberCount").toInt(), 2);
    QVERIFY(panel->setProperty("genderFilter", "f"));
    QTRY_COMPARE(panel->property("visibleMemberCount").toInt(), 1);
    QVERIFY(!originalMale.isNull() && !originalFemale.isNull());
    QVERIFY(!male->isVisible() && !male->isEnabled());
    QVERIFY(female->isVisible() && female->isEnabled());
    panel->setProperty("statusFilter", "online");
    QTRY_COMPARE(panel->property("visibleMemberCount").toInt(), 0);
    panel->setProperty("statusFilter", "active");
    QTRY_COMPARE(panel->property("visibleMemberCount").toInt(), 1);
    search->setProperty("text", "101");
    QTRY_COMPARE(panel->property("visibleMemberCount").toInt(), 0);
    search->setProperty("text", "102");
    QTRY_COMPARE(panel->property("visibleMemberCount").toInt(), 1);
    panel->setProperty("genderFilter", "all");
    panel->setProperty("statusFilter", "all");
    search->setProperty("text", "");
    QTRY_COMPARE(panel->property("visibleMemberCount").toInt(), 2);
    for (int cycle = 0; cycle < 4; ++cycle) {
        for (const char *filter : {"m", "f", "all"}) {
            panel->setProperty("genderFilter", filter);
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QVERIFY(!originalMale.isNull() && !originalFemale.isNull());
            QCOMPARE(rows->property("count").toInt(), 2);
            QQuickItem *current = nullptr;
            QVERIFY(QMetaObject::invokeMethod(rows, "itemAt", Q_RETURN_ARG(QQuickItem *, current), Q_ARG(int, 0)));
            QCOMPARE(current, originalMale.data());
            QVERIFY(QMetaObject::invokeMethod(rows, "itemAt", Q_RETURN_ARG(QQuickItem *, current), Q_ARG(int, 1)));
            QCOMPARE(current, originalFemale.data());
        }
    }
}

void QmlInteractionTest::clearsFilteredGuildSelectionButKeepsMatchingLiveUpdates()
{
    FakeGuildNavigationController controller;
    controller.roster = {
        QVariantMap{{"id", "m"}, {"anchorName", "男主播"}, {"roomId", "101"}, {"gender", "m"}, {"liveState", "online"}},
        QVariantMap{{"id", "f"}, {"anchorName", "女主播"}, {"roomId", "102"}, {"gender", "f"}, {"liveState", "offline"}}};
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    QQmlComponent component(&engine, QUrl("qrc:/qml/components/GuildNavigationPanel.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({{"width", 880}, {"height", 564},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))}}));
    QVERIFY(panel);
    auto *details = panel->findChild<QQuickItem *>("guildMemberDetails");
    auto *search = panel->findChild<QObject *>("guildNavigationSearch");
    QVERIFY(details && search);
    QVERIFY(panel->setProperty("selectedMemberId", "m"));
    QTRY_VERIFY(details->isVisible());
    auto member = controller.roster[0].toMap();
    member["liveState"] = "offline";
    controller.roster[0] = member;
    emit controller.guildRosterChanged();
    QCOMPARE(panel->property("selectedMemberId").toString(), QStringLiteral("m"));
    QVERIFY(details->isVisible());
    QVERIFY(panel->setProperty("genderFilter", "f"));
    QTRY_COMPARE(panel->property("selectedMemberId").toString(), QString());
    QTRY_VERIFY(!details->isVisible());
    QCOMPARE(panel->property("visibleMemberCount").toInt(), 1);
    QVERIFY(panel->setProperty("selectedMemberId", "f"));
    QVERIFY(search->setProperty("text", "101"));
    QTRY_COMPARE(panel->property("selectedMemberId").toString(), QString());
    QVERIFY(search->setProperty("text", ""));
    QVERIFY(panel->setProperty("selectedMemberId", "f"));
    QVERIFY(panel->setProperty("statusFilter", "online"));
    QTRY_COMPARE(panel->property("selectedMemberId").toString(), QString());
    QCOMPARE(warnings.count(), 0);
}

void QmlInteractionTest::ordersGuildNavigationByPublicDirectory()
{
    FakeGuildNavigationController controller;
    controller.roster = {
        QVariantMap{{"id", "third"}, {"anchorName", "A团长"}, {"pinyinKey", "A"}, {"role", "leader"}, {"directoryOrder", 2}},
        QVariantMap{{"id", "first"}, {"anchorName", "Z队员"}, {"pinyinKey", "Z"}, {"role", "member"}, {"directoryOrder", 0}},
        QVariantMap{{"id", "second"}, {"anchorName", "M队长"}, {"pinyinKey", "M"}, {"role", "captain"}, {"directoryOrder", 1}}
    };
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl("qrc:/qml/components/GuildNavigationPanel.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({{"width", 880}, {"height", 564},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))}}));
    QVERIFY(panel);
    auto *rows = panel->findChild<QObject *>("guildVisibleRows");
    QVERIFY(rows);
    QCOMPARE(rows->property("count").toInt(), 3);
    const QStringList expected{"first", "second", "third"};
    for (int i=0; i<expected.size(); ++i) {
        QQuickItem *row = nullptr;
        QVERIFY(QMetaObject::invokeMethod(rows, "itemAt", Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, i)));
        QVERIFY(row);
        QCOMPARE(row->findChild<QObject *>("guildMemberRow")->property("memberId").toString(), expected[i]);
    }
}

void QmlInteractionTest::checksRankVersionOnlyWhileGuildNavigationIsVisible()
{
    FakeGuildNavigationController controller;
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qml/components/GuildNavigationPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("visible"), true},
    }));
    QVERIFY2(panel != nullptr, qPrintable(component.errorString()));

    QObject *timer = panel->findChild<QObject *>(QStringLiteral("rankSyncTimer"));
    QVERIFY(timer != nullptr);
    QVERIFY(timer->property("running").toBool());
    QCOMPARE(timer->property("interval").toInt(), 60000);
    QVERIFY(QMetaObject::invokeMethod(panel.get(), "syncLiveStatus"));
    QCOMPARE(controller.liveRefreshCount, 1);

    panel->setProperty("visible", false);
    QTRY_VERIFY(!timer->property("running").toBool());
    QVERIFY(QMetaObject::invokeMethod(panel.get(), "syncLiveStatus"));
    QCOMPARE(controller.liveRefreshCount, 1);
}

void QmlInteractionTest::updatesGuildStatusWithoutRecreatingRows()
{
    FakeGuildNavigationController controller;
    for (int index = 0; index < 58; ++index) {
        controller.roster.append(QVariantMap{
            {"id", QString::number(index)}, {"anchorName", QStringLiteral("Anchor %1").arg(index)},
            {"roomId", QString::number(1000 + index)}, {"pinyinKey", "A"},
            {"status", "resolved"}, {"role", "member"}, {"liveState", "unknown"},
            {"avatarUrl", ""}, {"active", false}});
    }
    QQmlApplicationEngine engine;
    QQuickWindow window;
    window.resize(284, 720);
    window.show();
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/GuildNavigationPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {"parent", QVariant::fromValue(window.contentItem())}, {"width", 284}, {"height", 720},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))}}));
    QVERIFY(panel != nullptr);
    auto *rows = panel->findChild<QObject *>("guildVisibleRows");
    QVERIFY(rows != nullptr);
    QQuickItem *first = nullptr;
    QVERIFY(QMetaObject::invokeMethod(rows, "itemAt", Q_RETURN_ARG(QQuickItem *, first), Q_ARG(int, 2)));
    QVERIFY(first != nullptr);
    QPointer<QQuickItem> original(first);
    QElapsedTimer elapsed;
    elapsed.start();
    for (int index = 0; index < 58; ++index) {
        auto member = controller.roster[index].toMap();
        member.insert("liveState", "online");
        controller.roster[index] = member;
        emit controller.guildRosterChanged();
    }
    qInfo() << "58 navigation status updates (ms):" << elapsed.elapsed();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    QVERIFY2(!original.isNull(), "A live-status refresh destroyed the existing navigation row");
    QQuickItem *updated = nullptr;
    QVERIFY(QMetaObject::invokeMethod(rows, "itemAt", Q_RETURN_ARG(QQuickItem *, updated), Q_ARG(int, 2)));
    QCOMPARE(updated, original.data());
    QCOMPARE(updated->findChild<QObject *>("guildMemberLiveStateLabel")->property("text").toString(),
             QStringLiteral("直播中"));
    panel->setProperty("visible", false);
    QVERIFY(!panel->property("visible").toBool());
}

void QmlInteractionTest::keepsGuildAddHoverStableWithoutDataPopup()
{
    FakeGuildNavigationController controller;
    controller.roster = {QVariantMap{{"id", "fixture"}, {"anchorName", "测试主播"}, {"roomId", "101"}, {"status", "resolved"}}};
    QQmlApplicationEngine engine;
    auto *window = loadWindow(engine);
    QVERIFY(window);
    window->resize(1280, 800);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    click(window->findChild<QObject *>("hamsterNavigationButton"));
    auto *panel = window->findChild<QObject *>("guildNavigationPanel");
    QVERIFY(panel);
    panel->setProperty("controller", QVariant::fromValue(static_cast<QObject *>(&controller)));
    QQuickItem *add = nullptr;
    QTRY_VERIFY((add = quickItemByObjectName(qobject_cast<QQuickItem *>(panel), "guildQuickAddButton")) != nullptr);
    QTRY_VERIFY(add->isEnabled());
    // Wait for the opened page's layout; a newly created delegate initially uses its implicit width.
    QTRY_COMPARE(panel->property("width").toReal(), qreal(window->width() - 80));
    QTRY_COMPARE(add->mapToScene(add->boundingRect().center()).x(), qreal(window->width() - 68));
    QSignalSpy hovered(add, SIGNAL(hoveredChanged()));
    const QPoint point = add->mapToScene(QPointF(add->width()/2, add->height()/2)).toPoint();
    QTest::mouseMove(window, point);
    QTRY_VERIFY(add->property("hovered").toBool());
    hovered.clear();
    const QRectF original(add->mapToScene(QPointF()), add->size());
    for (int i=0; i<12; ++i) {
        QTest::qWait(100);
        QVERIFY(add->property("hovered").toBool());
        QCOMPARE(QRectF(add->mapToScene(QPointF()), add->size()), original);
        QVERIFY(!window->findChild<QObject *>("guildRankHoverCard"));
    }
    QCOMPARE(hovered.count(), 0);
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
    QCOMPARE(controller.lastAddedMemberId, QString("fixture"));
    click(window->findChild<QObject *>("sidebarToggleButton"));
    QTRY_VERIFY(!panel->property("visible").toBool());
}

void QmlInteractionTest::showsHoverMetricsInKeyboardMemberDetails_data()
{
    QTest::addColumn<bool>("missingMetrics");
    QTest::newRow("recorded") << false;
    QTest::newRow("missing") << true;
}

void QmlInteractionTest::showsHoverMetricsInKeyboardMemberDetails()
{
    QFETCH(bool, missingMetrics);
    FakeGuildHoverController controller;
    controller.missingMetrics = missingMetrics;
    QQmlApplicationEngine engine;
    QQuickWindow window;
    window.resize(QSize(1200, 764));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/GuildNavigationPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(window.contentItem())},
        {QStringLiteral("width"), 1200}, {QStringLiteral("height"), 764},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
    }));
    QVERIFY2(panel != nullptr, qPrintable(component.errorString()));
    auto *rows = panel->findChild<QObject *>(QStringLiteral("guildVisibleRows"));
    QVERIFY(rows != nullptr);
    QQuickItem *nameButton = nullptr;
    const auto findNameButton = [&]() {
        for (int index = 0; index < rows->property("count").toInt(); ++index) {
            QQuickItem *row = nullptr;
            if (!QMetaObject::invokeMethod(rows, "itemAt", Q_RETURN_ARG(QQuickItem *, row),
                                          Q_ARG(int, index)) || row == nullptr) continue;
            auto *candidate = row->findChild<QQuickItem *>(QStringLiteral("guildMemberName"));
            if (candidate && candidate->isVisible()) { nameButton = candidate; return true; }
        }
        return false;
    };
    QTRY_VERIFY(findNameButton());
    nameButton->forceActiveFocus(Qt::TabFocusReason);
    QTest::keyClick(&window, Qt::Key_Space);
    QTRY_COMPARE(panel->property("selectedMemberId").toString(), QStringLiteral("hamster-001"));
    auto *placement = panel->findChild<QQuickItem *>(QStringLiteral("guildSelectedPlacementSummary"));
    auto *playValue = panel->findChild<QQuickItem *>(QStringLiteral("guildSelectedPlayValueSummary"));
    QVERIFY(placement && playValue);
    QTRY_VERIFY(placement->isVisible() && playValue->isVisible());
    QCOMPARE(placement->property("text").toString(),
             missingMetrics ? QStringLiteral("定级赛总评 —") : QStringLiteral("定级赛总评 88.50"));
    QCOMPARE(playValue->property("text").toString(),
             missingMetrics ? QStringLiteral("游乐值 —") : QStringLiteral("游乐值 6.4"));
}

void QmlInteractionTest::hoverCardOmitsDetailsButton()
{
    QQmlApplicationEngine engine;
    QQmlComponent component(
        &engine, QUrl(QStringLiteral("qrc:/qml/components/GuildRankHoverCard.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> card(component.create());
    QVERIFY2(card != nullptr, qPrintable(component.errorString()));
    QVERIFY(card->findChild<QObject *>(QStringLiteral("guildRankDetailsButton")) == nullptr);
}

void QmlInteractionTest::highlightsWholeGuildRowWithoutNameStrip()
{
    FakeGuildHoverController controller;
    QQmlApplicationEngine engine;
    QQuickWindow window;
    window.resize(1200, 764);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.requestActivate();
    QTRY_VERIFY(window.isActive());
    QQmlComponent component(&engine, QUrl("qrc:/qml/components/GuildNavigationPanel.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {"parent", QVariant::fromValue(window.contentItem())},
        {"width", 1200}, {"height", 764},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))},
    }));
    QVERIFY2(panel != nullptr, qPrintable(component.errorString()));
    QQuickItem *member = nullptr;
    const auto findMember = [&]() {
        const auto visit = [&](auto &&self, QQuickItem *item) -> QQuickItem * {
            if (item->objectName() == "guildMemberRow" && item->isVisible()
                && item->property("memberId").toString() == "hamster-001") return item;
            for (auto *child : item->childItems())
                if (auto *match = self(self, child)) return match;
            return nullptr;
        };
        return visit(visit, qobject_cast<QQuickItem *>(panel.get()));
    };
    QTRY_VERIFY((member = findMember()) != nullptr);
    auto *background = member->parentItem()->findChild<QQuickItem *>("guildMemberRowBackground");
    auto *name = member->findChild<QQuickItem *>("guildMemberName");
    auto *nameBackground = member->findChild<QQuickItem *>("guildMemberNameBackground");
    auto *avatar = member->findChild<QQuickItem *>("guildMemberAvatar");
    QVERIFY(background && name && nameBackground && avatar);
    QSignalSpy frames(&window, &QQuickWindow::frameSwapped);
    window.update();
    QTRY_VERIFY(!frames.isEmpty());
    QTest::mouseMove(&window, QPoint(1199, 763));
    QTRY_VERIFY(!member->property("hovered").toBool());
    const QColor normal = background->property("color").value<QColor>();
    QTest::mouseMove(&window, avatar->mapToScene(QPointF(20, 20)).toPoint());
    QTRY_VERIFY(member->property("hovered").toBool());
    QTRY_VERIFY(background->property("color").value<QColor>() != normal);
    QVERIFY(background->width() >= member->width());
    QVERIFY(background->height() >= member->height());
    QTest::mouseMove(&window, name->mapToScene(QPointF(name->width() / 2, name->height() / 2)).toPoint());
    QTRY_VERIFY(name->property("hovered").toBool());
    QCOMPARE(nameBackground->property("color").value<QColor>().alpha(), 0);
    QTest::qWait(600);
    for (QObject *child : name->findChildren<QObject *>()) {
        if (QByteArray(child->metaObject()->className()).contains("ToolTip"))
            QVERIFY(!child->property("visible").toBool());
    }
    QTest::mouseMove(&window, QPoint(1199, 763));
    QTRY_COMPARE(background->property("color").value<QColor>(), normal);
}

void QmlInteractionTest::compactsGuildHoverCardWithoutRankData()
{
    QQmlApplicationEngine engine;
    QQuickWindow window;
    window.resize(640, 480);
    window.show();
    QQmlComponent component(&engine, QUrl("qrc:/qml/components/GuildRankHoverCard.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QVariantMap member{{"anchorName", QStringLiteral("测试主播")}, {"rankMatched", false}};
    std::unique_ptr<QObject> card(component.createWithInitialProperties({
        {"parent", QVariant::fromValue(window.contentItem())}, {"member", member},
    }));
    QVERIFY2(card != nullptr, qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(card.get(), "open"));
    QTRY_VERIFY(card->property("visible").toBool());
    QTRY_VERIFY(card->property("height").toReal() > 60);
    QVERIFY2(card->property("height").toReal() <= 120,
             "Missing rank metrics must not leave the radar chart's empty vertical space");
    const qreal compactHeight = card->property("height").toReal();
    member.insert("rankMatched", true);
    card->setProperty("member", member);
    QTRY_VERIFY(card->property("height").toReal() > compactHeight + 100);
}

void QmlInteractionTest::quickAddsResolvedGuildMember()
{
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qml/components/GuildMemberRow.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> row(component.createWithInitialProperties({
        {QStringLiteral("width"), 260},
        {QStringLiteral("member"), QVariantMap{
             {QStringLiteral("id"), QStringLiteral("hamster-001")},
             {QStringLiteral("anchorName"), QStringLiteral("寅子")},
             {QStringLiteral("roomId"), QStringLiteral("71415")},
             {QStringLiteral("status"), QStringLiteral("resolved")},
             {QStringLiteral("active"), false}}},
        {QStringLiteral("canAdd"), true},
    }));
    QVERIFY2(row != nullptr, qPrintable(component.errorString()));

    QSignalSpy added(row.get(), SIGNAL(addRequested(QString)));
    QObject *addButton = row->findChild<QObject *>(QStringLiteral("guildQuickAddButton"));
    QVERIFY(addButton != nullptr);
    QVERIFY(addButton->property("enabled").toBool());
    click(addButton);
    QCOMPARE(added.count(), 1);
    QCOMPARE(added.at(0).at(0).toString(), QStringLiteral("hamster-001"));

    FakeGuildNavigationController panelController;
    panelController.roster = QVariantList{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("hamster-001")},
                    {QStringLiteral("anchorName"), QStringLiteral("寅子")},
                    {QStringLiteral("roomId"), QStringLiteral("71415")},
                    {QStringLiteral("status"), QStringLiteral("resolved")},
                    {QStringLiteral("active"), false}},
    };
    QQmlApplicationEngine panelEngine;
    QQmlComponent panelComponent(
        &panelEngine, QUrl(QStringLiteral("qrc:/qml/components/GuildNavigationPanel.qml")));
    QVERIFY2(panelComponent.isReady(), qPrintable(panelComponent.errorString()));
    QQuickWindow panelHost;
    panelHost.resize(QSize(284, 720));
    panelHost.show();
    std::unique_ptr<QObject> panel(panelComponent.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(panelHost.contentItem())},
        {QStringLiteral("width"), 284},
        {QStringLiteral("height"), 720},
        {QStringLiteral("controller"),
         QVariant::fromValue(static_cast<QObject *>(&panelController))},
    }));
    QVERIFY(panel != nullptr);
    QCOMPARE(panel->property("visibleMemberCount").toInt(), 1);
    QVERIFY(QMetaObject::invokeMethod(panel.get(), "quickAdd",
                                      Q_ARG(QVariant, QStringLiteral("hamster-001"))));
    QCOMPARE(panelController.lastAddedMemberId, QStringLiteral("hamster-001"));
}
void QmlInteractionTest::submitsManualGuildRoomId()
{
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qml/components/GuildMemberRow.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> row(component.createWithInitialProperties({
        {QStringLiteral("member"), QVariantMap{
             {QStringLiteral("id"), QStringLiteral("hamster-001")},
             {QStringLiteral("anchorName"), QStringLiteral("寅子")},
             {QStringLiteral("roomId"), QString()},
             {QStringLiteral("status"), QStringLiteral("unresolved")},
             {QStringLiteral("active"), false}}},
    }));
    QVERIFY2(row != nullptr, qPrintable(component.errorString()));

    QSignalSpy submitted(row.get(), SIGNAL(roomIdSubmitted(QString, QString)));
    QObject *roomLabel = row->findChild<QObject *>(QStringLiteral("guildMemberRoomLabel"));
    QVERIFY(roomLabel != nullptr);
    click(roomLabel);

    QObject *input = row->findChild<QObject *>(QStringLiteral("guildMemberRoomInput"));
    QVERIFY(input != nullptr);
    QTRY_VERIFY(input->property("visible").toBool());
    input->setProperty("text", QStringLiteral("71415"));
    QVERIFY(QMetaObject::invokeMethod(input, "accepted"));
    QCOMPARE(submitted.count(), 1);
    QCOMPARE(submitted.at(0).at(0).toString(), QStringLiteral("hamster-001"));
    QCOMPARE(submitted.at(0).at(1).toString(), QStringLiteral("71415"));
    click(roomLabel);
    QTRY_VERIFY(input->property("visible").toBool());
    input->setProperty("text", QStringLiteral("99999"));
    auto resolvedMember = row->property("member").toMap();
    resolvedMember["roomId"] = QStringLiteral("71415");
    row->setProperty("member", resolvedMember);
    QTRY_VERIFY(!input->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(input, "accepted"));
    QCOMPARE(submitted.count(), 1);
}

void QmlInteractionTest::keepsTransientDialogSurfacesDark()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(640, 720));
    hostWindow.show();

    QQmlComponent notificationComponent(&engine,
                                        QUrl(QStringLiteral("qrc:/qml/dialogs/NotificationSettingsDialog.qml")));
    QVERIFY2(notificationComponent.isReady(), qPrintable(notificationComponent.errorString()));
    std::unique_ptr<QObject> notification(notificationComponent.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
    }));
    QVERIFY(notification != nullptr);
    QObject *notificationFooter = notification->findChild<QObject *>(QStringLiteral("notificationDialogFooter"));
    QVERIFY(notificationFooter != nullptr);
    QCOMPARE(notificationFooter->property("color").value<QColor>(), QColor(QStringLiteral("#161c1b")));
    QObject *notificationLabel = notification->findChild<QObject *>(QStringLiteral("notificationEnabledLabel"));
    QVERIFY(notificationLabel != nullptr);
    QCOMPARE(notificationLabel->property("color").value<QColor>(), QColor(QStringLiteral("#9aa8a3")));

    QQmlComponent addRoomComponent(&engine,
                                   QUrl(QStringLiteral("qrc:/qml/dialogs/AddRoomDialog.qml")));
    QVERIFY2(addRoomComponent.isReady(), qPrintable(addRoomComponent.errorString()));
    std::unique_ptr<QObject> addRoom(addRoomComponent.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
    }));
    QVERIFY(addRoom != nullptr);
    QObject *addRoomFooter = addRoom->findChild<QObject *>(QStringLiteral("addRoomDialogFooter"));
    QVERIFY(addRoomFooter != nullptr);
    QCOMPARE(addRoomFooter->property("color").value<QColor>(), QColor(QStringLiteral("#161c1b")));
}

void QmlInteractionTest::retriesUnsavedWorkspaceFromPersistentWarning()
{
    registerQmlTypes();
    FakeHeaderController controller;
    controller.unsaved = true;
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    std::unique_ptr<QObject> window(component.createWithInitialProperties({
        {"appController", QVariant::fromValue(static_cast<QObject *>(&controller))}}));
    QVERIFY2(window != nullptr, qPrintable(component.errorString()));
    auto *warning = window->findChild<QQuickItem *>("workspaceUnsavedWarning");
    QVERIFY(warning != nullptr);
    QVERIFY(warning->isVisible());
    click(window->findChild<QObject *>("retryWorkspaceSaveButton"));
    QCOMPARE(controller.saveRetries, 1);
    QTRY_VERIFY(!warning->isVisible());
}

void QmlInteractionTest::rendersUnknownSearchStatus()
{
    FakeSearchController controller;
    controller.statusKnown = false;
    QQmlApplicationEngine engine;
    QQuickWindow window;
    window.resize(640, 720);
    window.show();
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/dialogs/AddRoomDialog.qml")));
    std::unique_ptr<QObject> dialog(component.createWithInitialProperties({
        {"parent", QVariant::fromValue(window.contentItem())},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))}}));
    QVERIFY2(dialog != nullptr, qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(dialog.get(), "open"));
    auto *results = dialog->findChild<QQuickItem *>("searchResultList");
    QVERIFY(results != nullptr);
    QTRY_COMPARE(results->property("count").toInt(), 1);
    QQuickItem *row = nullptr;
    QVERIFY(QMetaObject::invokeMethod(results, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, 0)));
    QVERIFY(row != nullptr);
    bool foundUnknown = false;
    for (auto *text : row->findChildren<QObject *>())
        if (text->property("text").toString().contains(QStringLiteral("状态未知"))) foundUnknown = true;
    QVERIFY(foundUnknown);
}

void QmlInteractionTest::editsEventMappingFromSettings()
{
    FakeUpdateController controller;
    QQmlApplicationEngine engine;
    QQuickWindow hostWindow;
    hostWindow.resize(640, 720);
    hostWindow.show();
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/pages/SettingsPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> page(component.createWithInitialProperties({
        {"parent", QVariant::fromValue(hostWindow.contentItem())},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))}, {"width", 640}, {"height", 720}}));
    QVERIFY(page != nullptr);
    auto *rows = page->findChild<QObject *>("eventMappingRows");
    QVERIFY(rows != nullptr);
    QTRY_COMPARE(rows->property("count").toInt(), 1);
    QQuickItem *row = nullptr;
    QVERIFY(QMetaObject::invokeMethod(rows, "itemAt", Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, 0)));
    QVERIFY(row != nullptr);
    auto *role = row->findChild<QObject *>("eventMappingRole_0");
    auto *team = row->findChild<QObject *>("eventMappingTeam_0");
    QVERIFY(role != nullptr);
    QVERIFY(team != nullptr);
    role->setProperty("currentIndex", 1);
    QVERIFY(QMetaObject::invokeMethod(role, "activated", Q_ARG(int, 1)));
    team->setProperty("currentIndex", 4);
    QVERIFY(QMetaObject::invokeMethod(team, "activated", Q_ARG(int, 4)));
    click(page->findChild<QObject *>("saveEventMappingButton"));
    const auto member = QJsonDocument::fromJson(controller.savedMapping.toUtf8()).object()
                            .value("members").toArray().first().toObject();
    QCOMPARE(member.value("role").toString(), QStringLiteral("captain"));
    QCOMPARE(member.value("team").toInt(), 3);
    QTest::qWait(100);
    const auto directory = QCoreApplication::applicationDirPath() + "/verification";
    QVERIFY(QDir().mkpath(directory));
    QVERIFY(hostWindow.grabWindow().save(directory + "/event-mapping-settings.png"));
}

void QmlInteractionTest::laysOutSettingsAcrossWidths()
{
    FakeUpdateController controller;
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    QQuickWindow window;
    window.resize(1600, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/pages/SettingsPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> page(component.createWithInitialProperties({
        {"parent", QVariant::fromValue(window.contentItem())},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))},
        {"width", 1600}, {"height", 900}}));
    QVERIFY2(page != nullptr, qPrintable(component.errorString()));
    auto *general = page->findChild<QQuickItem *>("closeBehaviorSection");
    auto *team = page->findChild<QQuickItem *>("teamManagementSection");
    auto *activity = page->findChild<QQuickItem *>("eventMappingSection");
    auto *updates = page->findChild<QQuickItem *>("updateSection");
    QVERIFY(general && team && activity && updates);
    QTRY_VERIFY(activity->x() > general->x() + general->width());
    QVERIFY(team->y() > general->y());
    QVERIFY(updates->y() > team->y());
    QVERIFY(activity->width() > general->width());
    QVERIFY(activity->x() + activity->width() <= 1600 - 24);
    const auto directory = QCoreApplication::applicationDirPath() + "/verification";
    QVERIFY(QDir().mkpath(directory));
    QVERIFY(window.grabWindow().save(directory + "/settings-wide-1600.png"));

    auto *pageItem = qobject_cast<QQuickItem *>(page.get());
    QVERIFY(pageItem != nullptr);
    pageItem->setWidth(640);
    window.resize(640, 720);
    pageItem->setHeight(720);
    QTRY_VERIFY(activity->y() > updates->y());
    QVERIFY(qAbs(activity->x() - general->x()) < 2);
    QVERIFY(activity->x() + activity->width() <= 640 - 16);
    auto *field = page->findChild<QQuickItem *>("eventMappingName");
    QVERIFY(field != nullptr);
    QVERIFY(field->width() <= activity->width());
    QVERIFY(window.grabWindow().save(directory + "/settings-narrow-640.png"));
    QCOMPARE(warnings.count(), 0);
}

void QmlInteractionTest::scrollsLongSettingsRosterWithoutClipping()
{
    FakeUpdateController controller;
    controller.memberCount = 26;
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    QQuickWindow window;
    window.resize(920, 480);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/pages/SettingsPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> page(component.createWithInitialProperties({
        {"parent", QVariant::fromValue(window.contentItem())},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))},
        {"width", 920}, {"height", 480}}));
    QVERIFY2(page != nullptr, qPrintable(component.errorString()));
    auto *rows = page->findChild<QObject *>("eventMappingRows");
    auto *scroller = page->findChild<QQuickItem *>("settingsScrollArea");
    QVERIFY(rows && scroller);
    QTRY_COMPARE(rows->property("count").toInt(), 26);
    QVERIFY(scroller->property("contentHeight").toReal() > scroller->height());
    QQuickItem *first = nullptr;
    QQuickItem *last = nullptr;
    QVERIFY(QMetaObject::invokeMethod(rows, "itemAt", Q_RETURN_ARG(QQuickItem *, first), Q_ARG(int, 0)));
    QVERIFY(QMetaObject::invokeMethod(rows, "itemAt", Q_RETURN_ARG(QQuickItem *, last), Q_ARG(int, 25)));
    QVERIFY(first && last);
    auto *role = last->findChild<QQuickItem *>("eventMappingRole_25");
    auto *team = last->findChild<QQuickItem *>("eventMappingTeam_25");
    QVERIFY(role && team);
    const auto roleStart = role->mapToScene(QPointF(0, 0));
    const auto teamStart = team->mapToScene(QPointF(0, 0));
    QVERIFY(qAbs(roleStart.y() - teamStart.y()) < 2);
    QVERIFY(roleStart.x() + role->width() <= teamStart.x());
    QVERIFY(teamStart.x() + team->width() <= 920 - 16);
    scroller->setProperty("contentY", scroller->property("contentHeight").toReal() - scroller->height());
    QTRY_VERIFY(last->mapToScene(QPointF(0, last->height())).y() < 480);
    auto *save = page->findChild<QQuickItem *>("saveEventMappingButton");
    QVERIFY(save != nullptr);
    QVERIFY(save->mapToScene(QPointF(0, save->height())).y() <= 480);
    const auto directory = QCoreApplication::applicationDirPath() + "/verification";
    QVERIFY(QDir().mkpath(directory));
    QVERIFY(window.grabWindow().save(directory + "/settings-long-list-920.png"));
    QCOMPARE(warnings.count(), 0);
}

void QmlInteractionTest::preservesSettingsCloseBehaviorActions()
{
    FakeUpdateController controller;
    QQmlApplicationEngine engine;
    QQuickWindow hostWindow;
    hostWindow.resize(800, 600);
    hostWindow.show();
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/pages/SettingsPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> page(component.createWithInitialProperties({
        {"parent", QVariant::fromValue(hostWindow.contentItem())},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))},
        {"width", 800}, {"height", 600}}));
    QVERIFY2(page != nullptr, qPrintable(component.errorString()));
    auto *options = page->findChild<QObject *>("closeBehaviorOptions");
    QVERIFY(options != nullptr);
    QQuickItem *background = nullptr;
    QQuickItem *quit = nullptr;
    QQuickItem *ask = nullptr;
    QVERIFY(QMetaObject::invokeMethod(options, "itemAt", Q_RETURN_ARG(QQuickItem *, ask), Q_ARG(int, 0)));
    QVERIFY(QMetaObject::invokeMethod(options, "itemAt", Q_RETURN_ARG(QQuickItem *, background), Q_ARG(int, 1)));
    QVERIFY(QMetaObject::invokeMethod(options, "itemAt", Q_RETURN_ARG(QQuickItem *, quit), Q_ARG(int, 2)));
    QVERIFY(ask && background && quit);
    click(background);
    QCOMPARE(controller.closeBehavior(), QStringLiteral("background"));
    QVERIFY(controller.rememberedChoice);
    click(quit);
    QCOMPARE(controller.closeBehavior(), QStringLiteral("quit"));
    click(ask);
    QCOMPARE(controller.closeBehavior(), QStringLiteral("ask"));
    QCOMPARE(controller.clearCount, 1);
}

void QmlInteractionTest::showsCloseBehaviorPersistenceRetry()
{
    FakeUpdateController controller;
    QQmlApplicationEngine engine;
    QQuickWindow hostWindow;
    hostWindow.resize(800, 600);
    hostWindow.show();
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/pages/SettingsPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> page(component.createWithInitialProperties({
        {"parent", QVariant::fromValue(hostWindow.contentItem())},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))},
        {"width", 800}, {"height", 600}}));
    QVERIFY2(page != nullptr, qPrintable(component.errorString()));
    auto *status = page->findChild<QObject *>("closeBehaviorSaveStatus");
    auto *retry = page->findChild<QObject *>("retryCloseBehaviorSaveButton");
    QVERIFY(status != nullptr);
    QVERIFY(retry != nullptr);
    QVERIFY(!status->property("visible").toBool());
    controller.failCloseBehaviorSave();
    QTRY_VERIFY(status->property("visible").toBool());
    QCOMPARE(status->property("text").toString(), QStringLiteral("关闭行为保存失败，请重试"));
    QVERIFY(QMetaObject::invokeMethod(retry, "click"));
    QTRY_VERIFY(!status->property("visible").toBool());
    QCOMPARE(controller.closeRetryCount, 1);
}

void QmlInteractionTest::eventMappingPopupsUseDarkTheme()
{
    FakeUpdateController controller;
    controller.roster = {QVariantMap{{"roomId", "456"}, {"anchorName", "Popup Anchor"}}};
    QQmlApplicationEngine engine;
    QQuickWindow window;
    window.resize(900, 900);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/pages/SettingsPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> page(component.createWithInitialProperties({
        {"parent", QVariant::fromValue(window.contentItem())},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))},
        {"width", 900}, {"height", 900}}));
    QVERIFY(page != nullptr);
    auto *rows = page->findChild<QObject *>("eventMappingRows");
    QVERIFY(rows != nullptr);
    QQuickItem *row = nullptr;
    QVERIFY(QMetaObject::invokeMethod(rows, "itemAt", Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, 0)));
    QVERIFY(row != nullptr);
    const QList<QObject *> selectors{
        page->findChild<QObject *>("eventMappingMemberPicker"),
        row->findChild<QObject *>("eventMappingRole_0"),
        row->findChild<QObject *>("eventMappingTeam_0")};
    for (auto *selector : selectors) {
        QVERIFY(selector != nullptr);
        auto *popup = selector->property("popup").value<QObject *>();
        QVERIFY(popup != nullptr);
        QVERIFY(QMetaObject::invokeMethod(popup, "open"));
        QTRY_VERIFY(popup->property("visible").toBool());
        auto *background = popup->property("background").value<QQuickItem *>();
        QVERIFY(background != nullptr);
        QCOMPARE(background->property("color").value<QColor>(), QColor("#0e1312"));
        const auto directory = QCoreApplication::applicationDirPath() + "/verification";
        QVERIFY(QDir().mkpath(directory));
        QTest::qWait(200);
        const QImage capture = window.grabWindow();
        QVERIFY(!capture.isNull());
        const QPointF sample = background->mapToScene(QPointF(background->width() - 8, 8));
        const QPoint pixel = (sample * window.devicePixelRatio()).toPoint();
        QVERIFY(capture.rect().contains(pixel));
        const auto color = capture.pixelColor(pixel);
        QVERIFY2(color == QColor("#1e2624") || color == QColor("#0e1312")
                 || color == QColor("#25312e") || color == QColor("#1a342c"),
                 qPrintable(color.name()));
        QVERIFY(capture.save(directory + "/" + selector->objectName() + "-dark-popup.png"));
        QVERIFY(QMetaObject::invokeMethod(popup, "close"));
    }
}

void QmlInteractionTest::checksForUpdatesFromSettingsPage()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(640, 720));
    hostWindow.show();

    FakeUpdateController controller;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/pages/SettingsPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> page(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
    }));
    QVERIFY(page != nullptr);

    QObject *checkButton = page->findChild<QObject *>(QStringLiteral("checkUpdateButton"));
    QVERIFY(checkButton != nullptr);
    QVERIFY(checkButton->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(checkButton, "clicked"));
    QCOMPARE(controller.checkCount, 1);

    controller.setUpdate(QStringLiteral("checking"), QStringLiteral("正在检查更新..."));
    QTRY_VERIFY_WITH_TIMEOUT(!checkButton->property("enabled").toBool(), 1000);
    QObject *openButton = page->findChild<QObject *>(QStringLiteral("openReleaseButton"));
    QVERIFY(openButton != nullptr);
    QVERIFY(!openButton->property("visible").toBool());

    controller.setUpdate(QStringLiteral("updateAvailable"), QStringLiteral("发现新版本 0.2.4"),
                         QUrl(QStringLiteral("https://github.com/KevinTsoi2002/DouyuMonitor/releases/tag/v0.2.4")));
    QTRY_VERIFY_WITH_TIMEOUT(openButton->property("visible").toBool(), 1000);
    QVERIFY(QMetaObject::invokeMethod(openButton, "clicked"));
    QCOMPARE(controller.openCount, 1);
}

void QmlInteractionTest::managesTeamsOnlyFromSettings()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);

    QObject *settingsButton = window->findChild<QObject *>(QStringLiteral("settingsButton"));
    QVERIFY(settingsButton != nullptr);
    click(settingsButton);
    QObject *manageTeams = window->findChild<QObject *>(QStringLiteral("manageTeamsButton"));
    QVERIFY(manageTeams != nullptr);
    click(manageTeams);

    QObject *dialog = window->findChild<QObject *>(QStringLiteral("teamManagerDialog"));
    QVERIFY(dialog != nullptr);
    QTRY_VERIFY(dialog->property("visible").toBool());
    QVERIFY(dialog->findChild<QObject *>(QStringLiteral("createTeamButton")) != nullptr);
    QVERIFY(dialog->findChild<QObject *>(QStringLiteral("teamMemberAssignmentList")) != nullptr);
}

void QmlInteractionTest::deletesTeamFromManagerRow()
{
    registerQmlTypes();
    FakeTeamManagerController controller;
    controller.addTeam(QStringLiteral("team-a"), QStringLiteral("一队"));
    controller.addTeam(QStringLiteral("team-b"), QStringLiteral("二队"));

    QQmlApplicationEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qml/dialogs/TeamManagerDialog.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(640, 720));
    hostWindow.show();
    std::unique_ptr<QObject> dialog(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("controller"),
         QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("workspaceModel"),
         QVariant::fromValue(static_cast<QObject *>(&controller.workspaceModel))},
    }));
    QVERIFY2(dialog != nullptr, qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(dialog.get(), "open"));
    QTRY_VERIFY(dialog->property("visible").toBool());

    QObject *teamList = dialog->findChild<QObject *>(QStringLiteral("teamList"));
    QVERIFY(teamList != nullptr);
    QTRY_COMPARE(teamList->property("count").toInt(), 2);
    QQuickItem *secondRow = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(QMetaObject::invokeMethod(teamList,
                                                       "itemAtIndex",
                                                       Q_RETURN_ARG(QQuickItem *, secondRow),
                                                       Q_ARG(int, 1))
                                 && secondRow != nullptr,
                             1000);
    QObject *deleteButton =
        secondRow->findChild<QObject *>(QStringLiteral("deleteTeamRowButton"));
    QVERIFY(deleteButton != nullptr);
    click(deleteButton);

    QCOMPARE(controller.deletedTeamId, QStringLiteral("team-b"));
    QTRY_COMPARE(teamList->property("count").toInt(), 1);
}

void QmlInteractionTest::previewsAndConfirmsTeamImport()
{
    registerQmlTypes();
    const auto previousFont = QGuiApplication::font();
    QGuiApplication::setFont(QFont(QStringLiteral("Microsoft YaHei UI"), 10));
    FakeTeamManagerController controller;
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl("qrc:/qml/dialogs/TeamManagerDialog.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(640, 720);
    window.show();
    std::unique_ptr<QObject> dialog(component.createWithInitialProperties({
        {"parent", QVariant::fromValue(window.contentItem())},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))},
        {"workspaceModel", QVariant::fromValue(static_cast<QObject *>(&controller.workspaceModel))}}));
    QVERIFY(dialog != nullptr);
    QVERIFY(QMetaObject::invokeMethod(dialog.get(), "open"));
    auto *button = dialog->findChild<QObject *>("importMaoziTeamsButton");
    QVERIFY(button != nullptr);
    click(button);
    QCOMPARE(controller.previewCount, 1);
    QTRY_VERIFY(!button->property("enabled").toBool());
    auto *confirm = dialog->findChild<QObject *>("confirmMaoziTeamImportButton");
    auto *cancel = dialog->findChild<QObject *>("cancelMaoziTeamImportButton");
    QVERIFY(confirm != nullptr && cancel != nullptr);
    QVERIFY(!confirm->property("enabled").toBool());
    click(cancel);
    QCOMPARE(controller.cancelCount, 1);
    controller.setImportState({{"state", "preview"}, {"canConfirm", true},
        {"createdTeams", 1}, {"changedMembers", 1}, {"unchangedMembers", 0},
        {"teams", QVariantList{QVariantMap{{"name", QStringLiteral("红队")},
          {"sourceCount", 1}, {"matchedCount", 1}, {"retainedCount", 0},
          {"members", QStringList{QStringLiteral("测试主播")}}}}}});
    QTRY_VERIFY(confirm->property("enabled").toBool());
    auto *list = dialog->findChild<QObject *>("maoziTeamImportPreviewList");
    QVERIFY(list != nullptr);
    QTRY_COMPARE(list->property("count").toInt(), 1);
    QVariantList previewTeams;
    for (const auto &name : {QStringLiteral("红队"), QStringLiteral("黑队"),
                             QStringLiteral("紫队"), QStringLiteral("蓝队")}) {
        QStringList members;
        for (int i = 0; i < 11; ++i)
            members.append(QStringLiteral("测试主播%1名字较长").arg(i + 1));
        previewTeams.append(QVariantMap{{"name", name}, {"sourceCount", 11},
            {"matchedCount", 11}, {"retainedCount", 1}, {"members", members},
            {"retainedMembers", QStringList{QStringLiteral("手动保留主播")}}});
    }
    controller.setImportState({{"state", "preview"}, {"canConfirm", true},
        {"createdTeams", 4}, {"changedMembers", 44}, {"unchangedMembers", 0},
        {"teams", previewTeams},
        {"conflictNames", QStringList{QStringLiteral("身份冲突主播")}},
        {"unmatchedNames", QStringList{QStringLiteral("未匹配主播")}}});
    QTRY_COMPARE(list->property("count").toInt(), 4);
    QTest::qWait(150);
    const QString verificationDir = QCoreApplication::applicationDirPath()
        + QStringLiteral("/../../verification/team-import");
    QVERIFY(QDir().mkpath(verificationDir));
    QVERIFY(window.grabWindow().save(verificationDir + "/preview-minimum.png"));
    window.resize(1280, 900);
    QTest::qWait(150);
    QVERIFY(window.grabWindow().save(verificationDir + "/preview-desktop.png"));
    click(confirm);
    QCOMPARE(controller.confirmCount, 1);
    controller.setImportState({{"state", "error"}, {"canConfirm", false},
                               {"error", QStringLiteral("刷新失败，请重试")}});
    QTRY_VERIFY(!confirm->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(dialog.get(), "close"));
    QTRY_COMPARE(controller.cancelCount, 2);
    QGuiApplication::setFont(previousFont);
}
void QmlInteractionTest::keepsInputAndMenuControlsOnDarkTheme()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(1280, 720));
    hostWindow.show();

    QQmlComponent addRoomComponent(&engine,
                                   QUrl(QStringLiteral("qrc:/qml/dialogs/AddRoomDialog.qml")));
    QVERIFY2(addRoomComponent.isReady(), qPrintable(addRoomComponent.errorString()));
    std::unique_ptr<QObject> addRoom(addRoomComponent.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
    }));
    QVERIFY(addRoom != nullptr);
    QObject *roomInput = addRoom->findChild<QObject *>(QStringLiteral("roomSearchInput"));
    QVERIFY(roomInput != nullptr);
    QCOMPARE(roomInput->property("color").value<QColor>(), QColor(QStringLiteral("#e9f0ee")));

    QQmlApplicationEngine mainEngine;
    QQuickWindow *window = loadWindow(mainEngine);
    QVERIFY(window != nullptr);
    auto *layoutButton = window->findChild<QObject *>(QStringLiteral("layoutMenuButton"));
    QVERIFY(layoutButton != nullptr);
    click(window->findChild<QObject *>(QStringLiteral("watchToolsButton")));
    QVERIFY(QMetaObject::invokeMethod(layoutButton, "clicked"));
    QObject *layoutMenu = window->findChild<QObject *>(QStringLiteral("layoutMenu"));
    QVERIFY(layoutMenu != nullptr);
    QTRY_VERIFY(layoutMenu->property("visible").toBool());
    QObject *menuBackground = layoutMenu->findChild<QObject *>(QStringLiteral("layoutMenuBackground"));
    QVERIFY(menuBackground != nullptr);
    QCOMPARE(menuBackground->property("color").value<QColor>(), QColor(QStringLiteral("#1e2624")));
    QObject *menuItemLabel = layoutMenu->findChild<QObject *>(QStringLiteral("layoutMenuItemLabel"));
    QVERIFY(menuItemLabel != nullptr);
    QCOMPARE(menuItemLabel->property("color").value<QColor>(), QColor(QStringLiteral("#e9f0ee")));

    QObject *dualPrimary = layoutMenu->findChild<QObject *>(QStringLiteral("dualPrimaryLayoutOption"));
    QVERIFY(dualPrimary != nullptr);
    QObject *dualPrimaryBackground =
        dualPrimary->findChild<QObject *>(QStringLiteral("dualPrimaryLayoutOptionBackground"));
    QVERIFY(dualPrimaryBackground != nullptr);
    dualPrimary->setProperty("highlighted", true);
    QCOMPARE(dualPrimaryBackground->property("color").value<QColor>(),
             QColor(QStringLiteral("#0e1312")));

    QObject *notification = nullptr;
    QQmlComponent notificationComponent(&engine,
                                        QUrl(QStringLiteral("qrc:/qml/dialogs/NotificationSettingsDialog.qml")));
    QVERIFY2(notificationComponent.isReady(), qPrintable(notificationComponent.errorString()));
    std::unique_ptr<QObject> notificationObject(notificationComponent.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
    }));
    notification = notificationObject.get();
    QVERIFY(notification != nullptr);
    QObject *checkBoxLabel = notification->findChild<QObject *>(QStringLiteral("notificationEnabledLabel"));
    QVERIFY(checkBoxLabel != nullptr);
    QVERIFY(checkBoxLabel->property("leftPadding").toInt() >= 12);

    QObject *closeIcon = window->findChild<QObject *>(QStringLiteral("windowCloseIcon"));
    QObject *minimizeIcon = window->findChild<QObject *>(QStringLiteral("minimizeIcon"));
    QObject *maximizeIcon = window->findChild<QObject *>(QStringLiteral("maximizeIcon"));
    QObject *fullscreenButton = window->findChild<QObject *>(QStringLiteral("fullscreenButton"));
    QVERIFY(closeIcon != nullptr);
    QVERIFY(minimizeIcon != nullptr);
    QVERIFY(maximizeIcon != nullptr);
    QVERIFY(fullscreenButton != nullptr);
    const QUrl fullscreenSource = QQmlProperty::read(
        fullscreenButton, "icon.source", qmlContext(fullscreenButton)).toUrl();
    QVERIFY(fullscreenSource.isValid());
    QVERIFY(fullscreenSource.toString().endsWith(QStringLiteral("window-fullscreen.svg")));
    auto *fullscreenContent = qvariant_cast<QQuickItem *>(fullscreenButton->property("contentItem"));
    QVERIFY(fullscreenContent != nullptr);
    QTRY_VERIFY(fullscreenContent->width() > 0 && fullscreenContent->height() > 0);
    for (QObject *icon : {closeIcon, minimizeIcon, maximizeIcon}) {
        QVERIFY(icon->property("source").toUrl().isValid());
    }
}

void QmlInteractionTest::exposesFavoriteTitleNotificationPreference()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qml/dialogs/NotificationSettingsDialog.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> dialog(component.create());
    QVERIFY(dialog != nullptr);
    QVERIFY(dialog->findChild<QObject *>(QStringLiteral("notificationFavoriteTitleCheckBox")) != nullptr);
}

void QmlInteractionTest::preservesDialogEscapeCancelAndFocusSemantics()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow window;
    window.resize(QSize(960, 720));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    auto *triggerItem = new QQuickItem(window.contentItem());
    triggerItem->setObjectName(QStringLiteral("dialogFocusReturnTrigger"));
    triggerItem->setSize(QSizeF(120, 32));

    const auto focusTrigger = [&]() {
        triggerItem->forceActiveFocus(Qt::TabFocusReason);
        QTRY_VERIFY(window.activeFocusItem() == triggerItem);
    };
    const auto verifyFocusReturned = [&]() {
        QTRY_VERIFY(window.activeFocusItem() == triggerItem);
    };

    FakeSearchController searchController;
    QQmlComponent addRoomComponent(
        &engine, QUrl(QStringLiteral("qrc:/qml/dialogs/AddRoomDialog.qml")));
    QVERIFY2(addRoomComponent.isReady(), qPrintable(addRoomComponent.errorString()));
    std::unique_ptr<QObject> addRoom(addRoomComponent.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(window.contentItem())},
        {QStringLiteral("controller"),
         QVariant::fromValue(static_cast<QObject *>(&searchController))},
    }));
    QVERIFY2(addRoom != nullptr, qPrintable(addRoomComponent.errorString()));
    focusTrigger();
    QVERIFY(QMetaObject::invokeMethod(addRoom.get(), "open"));
    QTRY_VERIFY(addRoom->property("visible").toBool());
    auto *roomInput = addRoom->findChild<QQuickItem *>(QStringLiteral("roomSearchInput"));
    QVERIFY(roomInput != nullptr);
    QTRY_VERIFY(roomInput->property("activeFocus").toBool());
    roomInput->setProperty("text", QStringLiteral("63136"));
    QTest::keyClick(&window, Qt::Key_Escape);
    QTRY_VERIFY(!addRoom->property("visible").toBool());
    QCOMPARE(searchController.searchCount, 0);
    QVERIFY(searchController.addedRoomId.isEmpty());
    verifyFocusReturned();

    FakeNotificationPreferencesController notificationController;
    QQmlComponent notificationComponent(
        &engine, QUrl(QStringLiteral("qrc:/qml/dialogs/NotificationSettingsDialog.qml")));
    QVERIFY2(notificationComponent.isReady(), qPrintable(notificationComponent.errorString()));
    std::unique_ptr<QObject> notification(notificationComponent.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(window.contentItem())},
        {QStringLiteral("controller"),
         QVariant::fromValue(static_cast<QObject *>(&notificationController))},
    }));
    QVERIFY2(notification != nullptr, qPrintable(notificationComponent.errorString()));
    focusTrigger();
    QVERIFY(QMetaObject::invokeMethod(notification.get(), "open"));
    QTRY_VERIFY(notification->property("visible").toBool());
    auto *notificationsEnabled =
        notification->findChild<QQuickItem *>(QStringLiteral("notificationEnabledCheckBox"));
    QVERIFY(notificationsEnabled != nullptr);
    QTRY_VERIFY(notificationsEnabled->property("activeFocus").toBool());
    const QPoint notificationCheckCenter =
        notificationsEnabled->mapToScene(QPointF(notificationsEnabled->width() / 2,
                                                  notificationsEnabled->height() / 2))
            .toPoint();
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, notificationCheckCenter);
    QTRY_COMPARE(notificationController.saveCount, 1);
    QCOMPARE(notificationController.notificationPreferences()
                 .value(QStringLiteral("enabled")).toBool(),
             false);
    QTest::keyClick(&window, Qt::Key_Escape);
    QTRY_VERIFY(!notification->property("visible").toBool());
    QCOMPARE(notificationController.saveCount, 1);
    QCOMPARE(notificationController.notificationPreferences()
                 .value(QStringLiteral("enabled")).toBool(),
             false);
    verifyFocusReturned();

    FakeUpdateController closeController;
    QQmlComponent closeBehaviorComponent(
        &engine, QUrl(QStringLiteral("qrc:/qml/dialogs/CloseBehaviorDialog.qml")));
    QVERIFY2(closeBehaviorComponent.isReady(),
             qPrintable(closeBehaviorComponent.errorString()));
    std::unique_ptr<QObject> closeBehavior(
        closeBehaviorComponent.createWithInitialProperties({
            {QStringLiteral("parent"), QVariant::fromValue(window.contentItem())},
            {QStringLiteral("controller"),
             QVariant::fromValue(static_cast<QObject *>(&closeController))},
        }));
    QVERIFY2(closeBehavior != nullptr, qPrintable(closeBehaviorComponent.errorString()));
    auto *cancelClose = closeBehavior->findChild<QQuickItem *>(
        QStringLiteral("cancelCloseButton"));
    QVERIFY(cancelClose != nullptr);
    focusTrigger();
    QVERIFY(QMetaObject::invokeMethod(closeBehavior.get(), "open"));
    QTRY_VERIFY(closeBehavior->property("visible").toBool());
    QTRY_VERIFY(cancelClose->property("activeFocus").toBool());
    QTest::keyClick(&window, Qt::Key_Escape);
    QTRY_VERIFY(!closeBehavior->property("visible").toBool());
    QCOMPARE(closeController.closeToTrayCount, 0);
    QCOMPARE(closeController.requestQuitCount, 0);
    QCOMPARE(closeController.closeBehavior(), QStringLiteral("ask"));
    verifyFocusReturned();

    focusTrigger();
    QVERIFY(QMetaObject::invokeMethod(closeBehavior.get(), "open"));
    QTRY_VERIFY(closeBehavior->property("visible").toBool());
    click(cancelClose);
    QTRY_VERIFY(!closeBehavior->property("visible").toBool());
    QCOMPARE(closeController.closeToTrayCount, 0);
    QCOMPARE(closeController.requestQuitCount, 0);
    verifyFocusReturned();

    focusTrigger();
    QVERIFY(QMetaObject::invokeMethod(closeBehavior.get(), "open"));
    QTRY_VERIFY(closeBehavior->property("visible").toBool());
    auto *backgroundAction =
        closeBehavior->findChild<QQuickItem *>(QStringLiteral("enterBackgroundButton"));
    QVERIFY(backgroundAction != nullptr);
    click(backgroundAction);
    QCOMPARE(closeController.closeToTrayCount, 1);
    QCOMPARE(closeController.requestQuitCount, 0);

    focusTrigger();
    QVERIFY(QMetaObject::invokeMethod(closeBehavior.get(), "open"));
    QTRY_VERIFY(closeBehavior->property("visible").toBool());
    auto *quitAction =
        closeBehavior->findChild<QQuickItem *>(QStringLiteral("quitApplicationButton"));
    QVERIFY(quitAction != nullptr);
    click(quitAction);
    QCOMPARE(closeController.closeToTrayCount, 1);
    QCOMPARE(closeController.requestQuitCount, 1);

    FakeTeamManagerController teamController;
    teamController.addTeam(QStringLiteral("team-a"), QStringLiteral("保留队伍"));
    QQmlComponent teamManagerComponent(
        &engine, QUrl(QStringLiteral("qrc:/qml/dialogs/TeamManagerDialog.qml")));
    QVERIFY2(teamManagerComponent.isReady(), qPrintable(teamManagerComponent.errorString()));
    std::unique_ptr<QObject> teamManager(teamManagerComponent.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(window.contentItem())},
        {QStringLiteral("controller"),
         QVariant::fromValue(static_cast<QObject *>(&teamController))},
        {QStringLiteral("workspaceModel"),
         QVariant::fromValue(static_cast<QObject *>(&teamController.workspaceModel))},
    }));
    QVERIFY2(teamManager != nullptr, qPrintable(teamManagerComponent.errorString()));
    focusTrigger();
    QVERIFY(QMetaObject::invokeMethod(teamManager.get(), "open"));
    QTRY_VERIFY(teamManager->property("visible").toBool());
    auto *teamNameInput =
        teamManager->findChild<QQuickItem *>(QStringLiteral("teamNameInput"));
    auto *teamImport =
        teamManager->findChild<QQuickItem *>(QStringLiteral("importMaoziTeamsButton"));
    QVERIFY(teamNameInput != nullptr);
    QVERIFY(teamImport != nullptr);
    QTRY_VERIFY(teamNameInput->property("activeFocus").toBool());
    click(teamImport);
    QCOMPARE(teamController.previewCount, 1);
    QTRY_VERIFY(teamManager->property("importing").toBool());
    QTest::keyClick(&window, Qt::Key_Escape);
    QTRY_VERIFY(!teamManager->property("visible").toBool());
    QCOMPARE(teamController.cancelCount, 1);
    QCOMPARE(teamController.confirmCount, 0);
    QCOMPARE(teamController.workspaceModel.teams().size(), 1);
    verifyFocusReturned();
}

void QmlInteractionTest::keepsTeamManagerReachableAtShortViewport()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow window;
    window.resize(QSize(960, 600));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.requestActivate();
    QTRY_VERIFY(window.isActive());
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QQmlComponent component(
        &engine, QUrl(QStringLiteral("qrc:/qml/dialogs/TeamManagerDialog.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> dialog(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(window.contentItem())},
    }));
    QVERIFY2(dialog != nullptr, qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(dialog.get(), "open"));
    QTRY_VERIFY(dialog->property("visible").toBool());

    const qreal dialogY = dialog->property("y").toReal();
    const qreal dialogHeight = dialog->property("height").toReal();
    QVERIFY(dialogY >= 0);
    QVERIFY(dialogY + dialogHeight <= window.height());

    auto *scroll = dialog->findChild<QQuickItem *>(
        QStringLiteral("teamManagerGeneralScroll"));
    QVERIFY(scroll != nullptr);
    QVERIFY(scroll->height() > 0);
    if (scroll->property("contentHeight").toReal() > scroll->height()) {
        scroll->setProperty("contentY", scroll->property("contentHeight").toReal()
                                           - scroll->height());
    }
    auto *status = dialog->findChild<QQuickItem *>(
        QStringLiteral("teamManagerStatus"));
    QVERIFY(status != nullptr);
    QTRY_VERIFY(status->mapToScene(QPointF(0, status->height())).y()
                <= window.height());
}

void QmlInteractionTest::watchToolsOverlayPreservesWorkspaceGeometry()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    auto *grid = window->findChild<QQuickItem *>(QStringLiteral("workspaceGrid"));
    auto *rail = window->findChild<QQuickItem *>(QStringLiteral("appNavigationRail"));
    QObject *tools = window->findChild<QObject *>(QStringLiteral("watchToolsPanel"));
    QVERIFY(grid && rail && tools);
    QCOMPARE(rail->width(), qreal(80));
    const QRectF before(grid->position(), grid->size());
    click(window->findChild<QObject *>(QStringLiteral("watchToolsButton")));
    QTRY_VERIFY(tools->property("visible").toBool());
    QCOMPARE(QRectF(grid->position(), grid->size()), before);
    QVERIFY(QMetaObject::invokeMethod(tools, "close"));
    QTRY_VERIFY(!tools->property("visible").toBool());
    QCOMPARE(window->findChild<QQuickItem *>(QStringLiteral("workspaceGrid")), grid);
    QCOMPARE(QRectF(grid->position(), grid->size()), before);
}

void QmlInteractionTest::navigatesWatchToolsInlineWithPointerAndKeyboard()
{
    FakeRoomController controller;
    QQmlApplicationEngine engine;
    auto *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    window->setProperty("showPreviewRooms", true);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    auto *grid = window->findChild<QQuickItem *>("workspaceGrid");
    auto *tools = window->findChild<QObject *>("watchToolsPanel");
    auto *toolsButton = window->findChild<QQuickItem *>("watchToolsButton");
    QVERIFY(grid && tools && toolsButton);
    auto *overview = tools->findChild<QQuickItem *>("watchToolsOverview");
    auto *sound = tools->findChild<QQuickItem *>("soundMasterPopover");
    auto *layout = tools->findChild<QQuickItem *>("layoutMenu");
    auto *back = tools->findChild<QQuickItem *>("watchToolsBackButton");
    QVERIFY(grid && tools && toolsButton && overview && sound && layout && back);
    auto *surface = grid->findChild<QQuickItem *>("layoutSurface");
    QVERIFY(surface != nullptr);
    QTRY_VERIFY(!roomTiles(surface).isEmpty());
    const auto tiles = roomTiles(surface);
    auto *firstTile = tiles.constFirst();
    firstTile->setProperty("controller", QVariant::fromValue(static_cast<QObject *>(&controller)));
    firstTile->setProperty("renderEnabled", true);
    QTRY_COMPARE(controller.attachedRooms.size(), 1);
    QPointer<MpvQuickItem> player = firstTile->findChild<MpvQuickItem *>();
    QVERIFY(player != nullptr);
    const QRectF gridGeometry = sceneBounds(grid);

    pointerClick(window, toolsButton);
    QTRY_VERIFY(tools->property("visible").toBool());
    QCOMPARE(tools->property("currentSection").toString(), QStringLiteral("overview"));
    QVERIFY(overview->isVisible() && !sound->isVisible() && !layout->isVisible());
    QVERIFY(!back->isVisible());
    QVERIFY(tools->findChild<QObject *>("watchAddRoomButton") == nullptr);

    pointerClick(window, tools->findChild<QQuickItem *>("soundMasterButton"));
    QTRY_COMPARE(tools->property("currentSection").toString(), QStringLiteral("sound"));
    QVERIFY(sound->isVisible() && !overview->isVisible() && !layout->isVisible());
    auto *content = qobject_cast<QQuickItem *>(tools->property("contentItem").value<QObject *>());
    QVERIFY(content != nullptr);
    QTRY_VERIFY(sceneBounds(content).contains(sceneBounds(sound)));
    pointerClick(window, back);
    QTRY_VERIFY(overview->isVisible() && !sound->isVisible());

    pointerClick(window, tools->findChild<QQuickItem *>("layoutMenuButton"));
    QTRY_COMPARE(tools->property("currentSection").toString(), QStringLiteral("layout"));
    QVERIFY(layout->isVisible() && !overview->isVisible() && !sound->isVisible());
    QTRY_VERIFY(sceneBounds(content).contains(sceneBounds(layout)));
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(overview->isVisible() && !layout->isVisible());
    QVERIFY(tools->property("visible").toBool());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(!tools->property("visible").toBool());
    QTRY_VERIFY(toolsButton->hasActiveFocus());

    pointerClick(window, toolsButton);
    pointerClick(window, tools->findChild<QQuickItem *>("soundMasterButton"));
    QTRY_VERIFY(sound->isVisible());
    QTest::mouseMove(window, back->mapToScene(QPointF(back->width() / 2, back->height() / 2)).toPoint());
    auto *backTip = back->findChild<QObject *>("consoleToolTip");
    QVERIFY(backTip != nullptr);
    QTest::qWait(650);
    QVERIFY(!backTip->property("visible").toBool());
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, QPoint(100, 100));
    QTRY_VERIFY(!tools->property("visible").toBool());
    QTRY_VERIFY(toolsButton->hasActiveFocus());
    QCOMPARE(window->findChild<QQuickItem *>("workspaceGrid"), grid);
    QCOMPARE(sceneBounds(grid), gridGeometry);
    QCOMPARE(roomTiles(surface), tiles);
    QVERIFY(!player.isNull());
    QCOMPARE(firstTile->findChild<MpvQuickItem *>(), player.data());
    QCOMPARE(controller.attachedRooms.size(), 1);
    QVERIFY(controller.detachedRooms.isEmpty());
    firstTile->setProperty("controller", QVariant::fromValue(static_cast<QObject *>(nullptr)));
    firstTile->setProperty("renderEnabled", false);
}

void QmlInteractionTest::watchSurfacesReplaceEachOtherAndLayoutSupportsKeyboard()
{
    FakeHeaderController controller;
    QQmlApplicationEngine engine;
    auto *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    auto *tools = window->findChild<QObject *>("watchToolsPanel");
    auto *drawer = window->findChild<QObject *>("roomToolsDrawer");
    auto *headerButton = window->findChild<QQuickItem *>("watchToolsButton");
    QVERIFY(tools && drawer && headerButton);
    for (const auto &name : {"danmakuSettingsPanel", "monitoringDrawer", "workspacePresetsPanel"}) {
        auto *panel = window->findChild<QObject *>(name);
        QVERIFY(panel != nullptr);
        QVERIFY(QMetaObject::invokeMethod(panel, "open"));
        QTRY_VERIFY(panel->property("visible").toBool());
        window->requestActivate();
        QTRY_VERIFY(window->isActive());
        QTest::keyClick(window, Qt::Key_S, Qt::ControlModifier | Qt::ShiftModifier);
        QTRY_VERIFY(drawer->property("visible").toBool());
        QVERIFY(!panel->property("visible").toBool());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!drawer->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(panel, "open"));
        QTRY_VERIFY(panel->property("visible").toBool());
        pointerClick(window, headerButton);
        QTRY_VERIFY(tools->property("visible").toBool());
        QVERIFY(!panel->property("visible").toBool());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!tools->property("visible").toBool());
    }
    tools->setProperty("controller", QVariant::fromValue(static_cast<QObject *>(&controller)));
    pointerClick(window, headerButton);
    pointerClick(window, tools->findChild<QQuickItem *>("layoutMenuButton"));
    auto *primary = tools->findChild<QQuickItem *>("primaryLayoutOption");
    QVERIFY(primary != nullptr);
    for (int i = 0; i < 5 && !primary->hasActiveFocus(); ++i)
        QTest::keyClick(window, Qt::Key_Tab);
    QTRY_VERIFY(primary->hasActiveFocus());
    QVERIFY(primary->property("visualFocus").toBool());
    auto *background = primary->property("background").value<QObject *>();
    QVERIFY(background != nullptr);
    QCOMPARE(QQmlProperty::read(background, "border.width").toInt(), 2);
    QTest::keyClick(window, Qt::Key_Space);
    QCOMPARE(controller.lastLayout, QStringLiteral("primary"));
    QVERIFY(primary->property("checked").toBool());
    QTest::keyClick(window, Qt::Key_Space);
    QVERIFY(primary->property("checked").toBool());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_COMPARE(tools->property("currentSection").toString(), QStringLiteral("overview"));
}

void QmlInteractionTest::roomDrawerHasOneAddEntry()
{
    QQmlApplicationEngine engine;
    auto *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    pointerClick(window, window->findChild<QQuickItem *>("watchToolsButton"));
    pointerClick(window, window->findChild<QQuickItem *>("sidebarToggleButton"));
    auto *drawer = window->findChild<QObject *>("roomToolsDrawer");
    auto *sidebar = window->findChild<QQuickItem *>("roomSidebar");
    QVERIFY(drawer && sidebar);
    QTRY_VERIFY(drawer->property("visible").toBool());
    auto *add = sidebar->findChild<QQuickItem *>("quickAddButton");
    auto *close = sidebar->findChild<QQuickItem *>("closeRoomToolsButton");
    QVERIFY(add && close);
    QCOMPARE(add->property("text").toString(), QStringLiteral("添加房间"));
    QVERIFY(!sceneBounds(add).intersects(sceneBounds(close)));
    int addEntries = 0;
    for (auto *item : sidebar->findChildren<QQuickItem *>()) {
        const QString label = QQmlProperty::read(item, "Accessible.name", qmlContext(item)).toString();
        if (label.contains(QStringLiteral("添加房间"))) ++addEntries;
        QVERIFY2(item->property("placeholderText").toString() != QStringLiteral("输入房间号"),
                 "Room drawer still exposes the duplicate inline add-room input");
    }
    QCOMPARE(addEntries, 1);
    pointerClick(window, add);
    auto *dialog = window->findChild<QObject *>("addRoomDialog");
    QVERIFY(dialog != nullptr);
    QTRY_VERIFY(dialog->property("visible").toBool());
    QTRY_VERIFY(!drawer->property("visible").toBool());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(!dialog->property("visible").toBool());
}

void QmlInteractionTest::watchToolsShortcutProtectsUnnamedEditableControls()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QQmlComponent inputComponent(&engine);
    inputComponent.setData("import QtQuick; TextInput { width: 180; height: 32 }", QUrl());
    std::unique_ptr<QObject> input(inputComponent.create());
    auto *inputItem = qobject_cast<QQuickItem *>(input.get());
    QVERIFY(inputItem != nullptr);
    inputItem->setParentItem(window->contentItem());
    inputItem->forceActiveFocus();
    QTRY_VERIFY(window->property("editableFocus").toBool());
    QTest::keyClick(window, Qt::Key_T);
    QCOMPARE(input->property("text").toString(), QStringLiteral("t"));
    QObject *tools = window->findChild<QObject *>(QStringLiteral("watchToolsPanel"));
    QVERIFY(tools != nullptr);
    QVERIFY(!tools->property("visible").toBool());
}

void QmlInteractionTest::immersiveToolsCloseBeforeFullscreenAndRestoreFocus()
{
    QQmlApplicationEngine engine;
    auto *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    auto *grid = window->findChild<QQuickItem *>("workspaceGrid");
    auto *header = window->findChild<QQuickItem *>("appHeader");
    auto *rail = window->findChild<QQuickItem *>("appNavigationRail");
    auto *edge = window->findChild<QQuickItem *>("immersiveEdgeButton");
    auto *tools = window->findChild<QObject *>("watchToolsPanel");
    QVERIFY(grid && header && rail && edge && tools);
    window->showFullScreen();
    QTRY_COMPARE(window->visibility(), QWindow::FullScreen);
    window->requestActivate();
    QTRY_VERIFY(window->isActive());
    QVERIFY(!header->isVisible());
    QVERIFY(!rail->isVisible());
    QTRY_COMPARE(grid->position(), QPointF(0, 0));
    QTRY_COMPARE(grid->size(), QSizeF(window->size()));
    edge->forceActiveFocus(Qt::TabFocusReason);
    QTest::keyClick(window, Qt::Key_T);
    QTRY_VERIFY(tools->property("visible").toBool());
    click(tools->findChild<QObject *>("soundMasterButton"));
    auto *sound = tools->findChild<QObject *>("soundMasterPopover");
    QVERIFY(sound != nullptr);
    QTRY_VERIFY(sound->property("visible").toBool());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(!sound->property("visible").toBool());
    QVERIFY(tools->property("visible").toBool());
    QCOMPARE(window->visibility(), QWindow::FullScreen);
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_VERIFY(!tools->property("visible").toBool());
    QCOMPARE(window->visibility(), QWindow::FullScreen);
    QTRY_VERIFY(edge->hasActiveFocus());
    auto *tooltip = edge->findChild<QObject *>("consoleToolTip");
    QVERIFY(tooltip != nullptr);
    QVERIFY(tooltip->setProperty("visible", true));
    QTRY_VERIFY(tooltip->property("visible").toBool());
    QVERIFY(!window->property("popupOpen").toBool());
    QTest::keyClick(window, Qt::Key_Escape);
    QTRY_COMPARE(window->visibility(), QWindow::Windowed);
    QCOMPARE(window->findChild<QQuickItem *>("workspaceGrid"), grid);
    auto *toolsButton = header->findChild<QQuickItem *>("watchToolsButton");
    QVERIFY(toolsButton != nullptr);
    QTRY_VERIFY(toolsButton->hasActiveFocus());
}

void QmlInteractionTest::themeChangesKeepWorkspaceInstances()
{
    QQmlApplicationEngine engine;
    auto *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    auto *grid = window->findChild<QQuickItem *>("workspaceGrid");
    auto *brand = window->findChild<QObject *>("brandIcon");
    QVERIFY(grid && brand);
    const QRectF geometry(grid->position(), grid->size());
    click(window->findChild<QObject *>("themeToggleButton"));
    QTRY_COMPARE(window->color(), QColor("#f0f3ed"));
    QTRY_VERIFY(brand->property("source").toUrl().toString().endsWith("wordmark-day.png"));
    QCOMPARE(window->findChild<QQuickItem *>("workspaceGrid"), grid);
    QCOMPARE(QRectF(grid->position(), grid->size()), geometry);
    click(window->findChild<QObject *>("themeToggleButton"));
    QTRY_COMPARE(window->color(), QColor("#0e1312"));
}

void QmlInteractionTest::countsConnectedDanmakuFromRoomStates()
{
    FakeHeaderController controller;
    const auto snapshot = [](const QString &roomId) {
        RoomSnapshot room;
        room.roomId = roomId;
        room.metadata.roomId = roomId;
        room.metadata.anchorName = roomId;
        room.renderEnabled = false;
        return room;
    };
    const RoomSnapshot first = snapshot(QStringLiteral("room-a"));
    const RoomSnapshot second = snapshot(QStringLiteral("room-b"));
    const RoomSnapshot replacement = snapshot(QStringLiteral("room-c"));
    controller.roomModel.applySnapshots({first, second});
    QQmlApplicationEngine engine;
    auto *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    window->setProperty("appController", QVariant::fromValue(static_cast<QObject *>(&controller)));
    QVERIFY(QTest::qWaitForWindowExposed(window));
    auto *grid = window->findChild<QQuickItem *>(QStringLiteral("workspaceGrid"));
    auto *status = window->findChild<QObject *>(QStringLiteral("workspaceStatusBar"));
    QVERIFY(grid && status);
    grid->setProperty("controller", QVariant::fromValue(static_cast<QObject *>(nullptr)));
    auto *surface = grid->findChild<QQuickItem *>(QStringLiteral("layoutSurface"));
    QVERIFY(surface != nullptr);
    QTRY_COMPARE(roomTiles(surface).size(), 2);
    const auto tileFor = [&](const QString &roomId) -> QQuickItem * {
        for (auto *tile : roomTiles(surface)) {
            if (tile->property("roomId").toString() == roomId) return tile;
        }
        return nullptr;
    };
    QPointer<QQuickItem> firstTile = tileFor(first.roomId);
    QPointer<QQuickItem> secondTile = tileFor(second.roomId);
    QVERIFY(firstTile && secondTile);
    QTRY_COMPARE(status->property("danmakuCount").toInt(), 0);

    RoomPresentationSettings firstSettings;
    RoomPresentationSettings secondSettings;
    firstSettings.danmakuEnabled = true;
    secondSettings.danmakuEnabled = true;
    firstSettings.danmakuState = QStringLiteral("connected");
    controller.roomModel.applyPresentationSettings({{first.roomId, firstSettings}, {second.roomId, secondSettings}});
    QTRY_COMPARE(status->property("danmakuCount").toInt(), 1);
    QTRY_COMPARE(grid->property("connectedDanmakuCount").toInt(), 1);
    QCOMPARE(tileFor(first.roomId), firstTile.data());
    QCOMPARE(tileFor(second.roomId), secondTile.data());

    secondSettings.danmakuState = QStringLiteral("connected");
    controller.roomModel.applyPresentationSettings({{first.roomId, firstSettings}, {second.roomId, secondSettings}});
    QTRY_COMPARE(status->property("danmakuCount").toInt(), 2);
    QCOMPARE(tileFor(first.roomId), firstTile.data());
    QCOMPARE(tileFor(second.roomId), secondTile.data());

    firstSettings.danmakuState = QStringLiteral("reconnecting");
    controller.roomModel.applyPresentationSettings({{first.roomId, firstSettings}, {second.roomId, secondSettings}});
    QTRY_COMPARE(status->property("danmakuCount").toInt(), 1);
    QCOMPARE(tileFor(first.roomId), firstTile.data());
    QCOMPARE(tileFor(second.roomId), secondTile.data());

    controller.roomModel.applySnapshots({first});
    QTRY_COMPARE(roomTiles(surface).size(), 1);
    QTRY_COMPARE(status->property("danmakuCount").toInt(), 0);
    QVERIFY(!firstTile.isNull());
    QCOMPARE(tileFor(first.roomId), firstTile.data());

    controller.roomModel.applySnapshots({first, second});
    controller.roomModel.applyPresentationSettings({{first.roomId, firstSettings}, {second.roomId, secondSettings}});
    QTRY_COMPARE(status->property("danmakuCount").toInt(), 1);
    QSignalSpy countChanged(&controller.roomModel, &RoomListModel::roomCountChanged);
    controller.roomModel.applySnapshots({first, replacement});
    QCOMPARE(countChanged.count(), 0);
    QTRY_VERIFY(tileFor(replacement.roomId) != nullptr && tileFor(second.roomId) == nullptr);
    QTRY_COMPARE(status->property("danmakuCount").toInt(), 0);
    QVERIFY(!firstTile.isNull());
    QCOMPARE(tileFor(first.roomId), firstTile.data());

    RoomPresentationSettings replacementSettings;
    replacementSettings.danmakuEnabled = true;
    replacementSettings.danmakuState = QStringLiteral("connected");
    controller.roomModel.applyPresentationSettings({{first.roomId, firstSettings}, {replacement.roomId, replacementSettings}});
    QTRY_COMPARE(status->property("danmakuCount").toInt(), 1);
    controller.roomModel.applySnapshots({});
    QTRY_COMPARE(roomTiles(surface).size(), 0);
    QTRY_COMPARE(status->property("danmakuCount").toInt(), 0);
    for (auto *label : status->findChildren<QQuickItem *>()) {
        QVERIFY(label->property("text").toString() != QStringLiteral("静默检查"));
        QVERIFY(label->property("text").toString() != QStringLiteral("检查中"));
    }
}

void QmlInteractionTest::opensAddRoomDialogAndRejectsInvalidRoomId()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(window->isVisible());

    auto *emptyNavigation = window->findChild<QQuickItem *>("emptyNavigationButton");
    auto *emptyAddRoom = window->findChild<QQuickItem *>("emptyAddRoomButton");
    QVERIFY(emptyNavigation && emptyAddRoom);
    QVERIFY(emptyNavigation->isVisible() && emptyAddRoom->isVisible());
    click(emptyNavigation);
    QTRY_COMPARE(window->property("currentView").toString(), QStringLiteral("navigation"));
    click(window->findChild<QObject *>("watchWorkspaceButton"));
    click(emptyAddRoom);
    auto *emptyDialog = window->findChild<QObject *>("addRoomDialog");
    QVERIFY(emptyDialog != nullptr);
    QTRY_VERIFY(emptyDialog->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(emptyDialog, "close"));

    click(window->findChild<QObject *>(QStringLiteral("watchToolsButton")));
    click(window->findChild<QObject *>(QStringLiteral("sidebarToggleButton")));
    click(window->findChild<QObject *>(QStringLiteral("quickAddButton")));
    QObject *dialog = window->findChild<QObject *>(QStringLiteral("addRoomDialog"));
    QVERIFY(dialog != nullptr);
    QTRY_VERIFY(dialog->property("visible").toBool());
    QVERIFY(!dialog->property("canSubmit").toBool());
}

void QmlInteractionTest::opensMonitoringDanmakuAndWorkspaceSurfaces()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(window->isVisible());

    click(window->findChild<QObject *>(QStringLiteral("monitoringButton")));
    QObject *monitoring = window->findChild<QObject *>(QStringLiteral("monitoringDrawer"));
    QVERIFY(monitoring != nullptr);
    QTRY_VERIFY(monitoring->property("visible").toBool());

    click(window->findChild<QObject *>(QStringLiteral("danmakuButton")));
    QObject *danmaku = window->findChild<QObject *>(QStringLiteral("danmakuSettingsPanel"));
    QVERIFY(danmaku != nullptr);
    QTRY_VERIFY(danmaku->property("visible").toBool());

    click(window->findChild<QObject *>(QStringLiteral("workspaceButton")));
    QObject *workspace = window->findChild<QObject *>(QStringLiteral("workspacePresetsPanel"));
    QVERIFY(workspace != nullptr);
    QTRY_VERIFY(workspace->property("visible").toBool());
}

void QmlInteractionTest::reusesSurfacesAcrossRepeatedViewSwitches()
{
    FakeMaoziController controller;
    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("appController"),
         QVariant::fromValue(static_cast<QObject *>(&controller))},
    });
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    const QStringList names = {QStringLiteral("settingsPage"),
                               QStringLiteral("maoziRankPage"),
                               QStringLiteral("danmakuSettingsPanel"),
                               QStringLiteral("monitoringDrawer"),
                               QStringLiteral("workspacePresetsPanel")};
    QMap<QString, QPointer<QObject>> instances;
    for (const QString &name : names) {
        QObject *surface = window->findChild<QObject *>(name);
        QVERIFY2(surface != nullptr, qPrintable(name));
        instances.insert(name, surface);
    }
    const int initialRefreshCount = controller.rank.refreshCount;

    for (int iteration = 1; iteration <= 3; ++iteration) {
        window->setProperty("currentView", QStringLiteral("settings"));
        QTRY_VERIFY(!instances.value(QStringLiteral("settingsPage")).isNull()
                    && instances.value(QStringLiteral("settingsPage"))->property("visible").toBool());
        click(window->findChild<QObject *>(QStringLiteral("settingsBackButton")));
        QTRY_COMPARE(window->property("currentView").toString(), QStringLiteral("monitoring"));

        click(window->findChild<QObject *>(QStringLiteral("maoziRankButton")));
        QTRY_VERIFY(!instances.value(QStringLiteral("maoziRankPage")).isNull()
                    && instances.value(QStringLiteral("maoziRankPage"))->property("visible").toBool());
        QCOMPARE(controller.rank.refreshCount, initialRefreshCount + iteration);
        click(window->findChild<QObject *>(QStringLiteral("maoziBackButton")));
        QTRY_COMPARE(window->property("currentView").toString(), QStringLiteral("monitoring"));

        for (const auto &entry : {qMakePair("danmakuButton", "danmakuSettingsPanel"),
                                  qMakePair("monitoringButton", "monitoringDrawer"),
                                  qMakePair("workspaceButton", "workspacePresetsPanel")}) {
            click(window->findChild<QObject *>(QString::fromLatin1(entry.first)));
            QPointer<QObject> panel = instances.value(QString::fromLatin1(entry.second));
            QVERIFY2(!panel.isNull(), entry.second);
            QTRY_VERIFY(!panel.isNull() && panel->property("visible").toBool());
            QVERIFY(QMetaObject::invokeMethod(panel, "close"));
            QTRY_VERIFY(!panel.isNull() && !panel->property("visible").toBool());
        }
        QCOMPARE(controller.rank.refreshCount, initialRefreshCount + iteration);
        for (const QString &name : names) {
            QVERIFY2(!instances.value(name).isNull(), qPrintable(name));
            QCOMPARE(window->findChildren<QObject *>(name).size(), 1);
            QCOMPARE(window->findChild<QObject *>(name), instances.value(name).data());
        }
    }
}

void QmlInteractionTest::rendersUnavailableNotificationStatus()
{
    registerQmlTypes();
    FakeMonitoringModel model;
    model.setNotificationStatus(QStringLiteral("unavailable"));
    QQmlEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qml/panels/MonitoringStatusPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {QStringLiteral("monitoringModel"), QVariant::fromValue(static_cast<QObject *>(&model))},
    }));
    QVERIFY2(panel != nullptr, qPrintable(component.errorString()));
    const auto *label = panel->findChild<QObject *>(QStringLiteral("notificationStatusLabel"));
    QVERIFY(label != nullptr);
    QCOMPARE(label->property("text").toString(), QStringLiteral("Windows 通知后端不可用"));

    QQmlComponent dialogComponent(&engine,
                                  QUrl(QStringLiteral("qrc:/qml/dialogs/NotificationSettingsDialog.qml")));
    QVERIFY2(dialogComponent.isReady(), qPrintable(dialogComponent.errorString()));
    std::unique_ptr<QObject> dialog(dialogComponent.createWithInitialProperties({
        {QStringLiteral("monitoringModel"), QVariant::fromValue(static_cast<QObject *>(&model))},
    }));
    QVERIFY2(dialog != nullptr, qPrintable(dialogComponent.errorString()));
    const auto *description =
        dialog->findChild<QObject *>(QStringLiteral("notificationStatusDescription"));
    QVERIFY(description != nullptr);
    QCOMPARE(description->property("text").toString(), QStringLiteral("Windows 通知后端不可用"));
}

void QmlInteractionTest::closesTransientSurfacesFromExplicitActions()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(window->isVisible());

    click(window->findChild<QObject *>(QStringLiteral("monitoringButton")));
    QObject *monitoring = window->findChild<QObject *>(QStringLiteral("monitoringDrawer"));
    QVERIFY(monitoring != nullptr);
    QTRY_VERIFY(monitoring->property("visible").toBool());
    QObject *closeMonitoring = monitoring->findChild<QObject *>(
        QStringLiteral("closeMonitoringStatusButton"));
    QVERIFY(closeMonitoring != nullptr);
    click(closeMonitoring);
    QTRY_VERIFY(!monitoring->property("visible").toBool());

    click(window->findChild<QObject *>(QStringLiteral("danmakuButton")));
    QObject *danmaku = window->findChild<QObject *>(QStringLiteral("danmakuSettingsPanel"));
    QVERIFY(danmaku != nullptr);
    QTRY_VERIFY(danmaku->property("visible").toBool());
    QObject *closeDanmaku = danmaku->findChild<QObject *>(
        QStringLiteral("closeDanmakuSettingsButton"));
    QVERIFY(closeDanmaku != nullptr);
    click(closeDanmaku);
    QTRY_VERIFY(!danmaku->property("visible").toBool());

    click(window->findChild<QObject *>(QStringLiteral("workspaceButton")));
    QObject *workspace = window->findChild<QObject *>(QStringLiteral("workspacePresetsPanel"));
    QVERIFY(workspace != nullptr);
    QTRY_VERIFY(workspace->property("visible").toBool());
    QObject *closeWorkspace = workspace->findChild<QObject *>(
        QStringLiteral("closeWorkspacePresetsButton"));
    QVERIFY(closeWorkspace != nullptr);
    click(closeWorkspace);
    QTRY_VERIFY(!workspace->property("visible").toBool());

    click(window->findChild<QObject *>(QStringLiteral("watchToolsButton")));
    click(window->findChild<QObject *>(QStringLiteral("soundMasterButton")));
    QObject *soundMaster = window->findChild<QObject *>(QStringLiteral("soundMasterPopover"));
    QVERIFY(soundMaster != nullptr);
    QTRY_VERIFY(soundMaster->property("visible").toBool());
    QObject *closeSoundMaster = window->findChild<QObject *>(
        QStringLiteral("watchToolsBackButton"));
    QVERIFY(closeSoundMaster != nullptr);
    click(closeSoundMaster);
    QTRY_VERIFY(!soundMaster->property("visible").toBool());

    click(window->findChild<QObject *>(QStringLiteral("quickAddButton")));
    QObject *addRoom = window->findChild<QObject *>(QStringLiteral("addRoomDialog"));
    QVERIFY(addRoom != nullptr);
    QTRY_VERIFY(addRoom->property("visible").toBool());
    QObject *closeAddRoom = addRoom->findChild<QObject *>(
        QStringLiteral("closeAddRoomDialogButton"));
    QVERIFY(closeAddRoom != nullptr);
    click(closeAddRoom);
    QTRY_VERIFY(!addRoom->property("visible").toBool());

    click(window->findChild<QObject *>(QStringLiteral("monitoringButton")));
    QObject *notificationSettings = window->findChild<QObject *>(
        QStringLiteral("notificationSettingsDialog"));
    QVERIFY(notificationSettings != nullptr);
    click(window->findChild<QObject *>(QStringLiteral("openNotificationSettingsButton")));
    QTRY_VERIFY(notificationSettings->property("visible").toBool());
    QObject *closeNotification = notificationSettings->findChild<QObject *>(
        QStringLiteral("closeNotificationSettingsButton"));
    QVERIFY(closeNotification != nullptr);
    click(closeNotification);
    QTRY_VERIFY(!notificationSettings->property("visible").toBool());
}

void QmlInteractionTest::togglesSidebarAndHandlesRetainedShortcuts()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(window->isVisible());

    QObject *sidebar = window->findChild<QObject *>(QStringLiteral("roomSidebar"));
    QVERIFY(sidebar != nullptr);
    QVERIFY(!sidebar->property("visible").toBool());
    click(window->findChild<QObject *>(QStringLiteral("sidebarToggleButton")));
    QTRY_VERIFY(sidebar->property("visible").toBool());
    click(window->findChild<QObject *>(QStringLiteral("closeRoomToolsButton")));
    QTRY_VERIFY(!sidebar->property("visible").toBool());

    QTest::keyClick(window, Qt::Key_A, Qt::ControlModifier | Qt::ShiftModifier);
    QObject *dialog = window->findChild<QObject *>(QStringLiteral("addRoomDialog"));
    QVERIFY(dialog != nullptr);
    QTRY_VERIFY(dialog->property("visible").toBool());
    QTest::keyClick(window, Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
    QObject *monitoringWhileEditing = window->findChild<QObject *>(QStringLiteral("monitoringDrawer"));
    QVERIFY(monitoringWhileEditing != nullptr);
    QVERIFY(!monitoringWhileEditing->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
    QTRY_VERIFY(!dialog->property("visible").toBool());

    QTest::keyClick(window, Qt::Key_M, Qt::ControlModifier | Qt::ShiftModifier);
    QObject *monitoring = window->findChild<QObject *>(QStringLiteral("monitoringDrawer"));
    QVERIFY(monitoring != nullptr);
    QTRY_VERIFY(monitoring->property("visible").toBool());

    QTest::keyClick(window, Qt::Key_R, Qt::ControlModifier | Qt::ShiftModifier);
    QVERIFY(window->property("refreshRequested").toBool());
}

void QmlInteractionTest::handlesLegacyShortcutParity()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(window->isVisible());

    auto press = [window](Qt::Key key) {
        QTest::keyClick(window, key, Qt::ControlModifier | Qt::ShiftModifier);
    };

    press(Qt::Key_A);
    QObject *addDialog = window->findChild<QObject *>(QStringLiteral("addRoomDialog"));
    QVERIFY(addDialog != nullptr);
    QTRY_VERIFY(addDialog->property("visible").toBool());
    addDialog->setProperty("visible", false);

    press(Qt::Key_W);
    QObject *workspace = window->findChild<QObject *>(QStringLiteral("workspacePresetsPanel"));
    QVERIFY(workspace != nullptr);
    QTRY_VERIFY(workspace->property("visible").toBool());
    press(Qt::Key_W);
    QTRY_VERIFY(!workspace->property("visible").toBool());

    press(Qt::Key_M);
    QObject *monitoring = window->findChild<QObject *>(QStringLiteral("monitoringDrawer"));
    QVERIFY(monitoring != nullptr);
    QTRY_VERIFY(monitoring->property("visible").toBool());
    press(Qt::Key_M);
    QTRY_VERIFY(!monitoring->property("visible").toBool());

    press(Qt::Key_D);
    QObject *danmaku = window->findChild<QObject *>(QStringLiteral("danmakuSettingsPanel"));
    QVERIFY(danmaku != nullptr);
    QTRY_VERIFY(danmaku->property("visible").toBool());
    press(Qt::Key_D);
    QTRY_VERIFY(!danmaku->property("visible").toBool());

    QObject *sidebar = window->findChild<QObject *>(QStringLiteral("roomSidebar"));
    QVERIFY(sidebar != nullptr);
    QVERIFY(!sidebar->property("visible").toBool());
    press(Qt::Key_S);
    QTRY_VERIFY(sidebar->property("visible").toBool());
    press(Qt::Key_S);
    QTRY_VERIFY(!sidebar->property("visible").toBool());

    press(Qt::Key_R);
    QVERIFY(window->property("refreshRequested").toBool());
}

void QmlInteractionTest::rendersOnlineStatusForOnlineToken()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomTile.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    const QVariantMap properties = roomTileProperties(QStringLiteral("idle"));
    std::unique_ptr<QObject> tile(component.createWithInitialProperties(properties));
    QVERIFY2(tile != nullptr, qPrintable(component.errorString()));

    bool hasLiveLabel = false;
    for (QObject *object : tile->findChildren<QObject *>()) {
        if (object->property("text").toString() == QStringLiteral("直播中")) {
            hasLiveLabel = true;
            break;
        }
    }
    QVERIFY(hasLiveLabel);
}

void QmlInteractionTest::showsDanmakuDisplayGovernanceAndStatsTabs()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);

    click(window->findChild<QObject *>(QStringLiteral("danmakuButton")));
    QObject *panel = window->findChild<QObject *>(QStringLiteral("danmakuSettingsPanel"));
    QVERIFY(panel != nullptr);
    QTRY_VERIFY(panel->property("visible").toBool());

    const struct {
        QString tab;
        QString section;
    } cases[] = {
        {QStringLiteral("danmakuDisplayTab"), QStringLiteral("danmakuDisplaySection")},
        {QStringLiteral("danmakuGovernanceTab"), QStringLiteral("danmakuGovernanceSection")},
        {QStringLiteral("danmakuStatsTab"), QStringLiteral("danmakuStatsSection")},
    };
    for (const auto &testCase : cases) {
        click(window->findChild<QObject *>(testCase.tab));
        QObject *section = window->findChild<QObject *>(testCase.section);
        QVERIFY2(section != nullptr, qPrintable(testCase.section));
        QTRY_VERIFY(section->property("visible").toBool());
    }

    click(window->findChild<QObject *>(QStringLiteral("danmakuDisplayTab")));
    QObject *resetDisplay = window->findChild<QObject *>(QStringLiteral("resetDanmakuDisplaySettings"));
    QVERIFY(resetDisplay != nullptr);
    QVERIFY(resetDisplay->property("visible").toBool());
}

void QmlInteractionTest::readsSelectedRoomGovernanceOverride()
{
    FakeDanmakuAppController controller;
    controller.danmaku_.setGovernanceSetting(QStringLiteral("room-a"),
                                              QStringLiteral("duplicateWindowSeconds"), 7);
    controller.danmaku_.setGovernanceSetting(QStringLiteral("room-b"),
                                              QStringLiteral("duplicateWindowSeconds"), 9);

    QQmlEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qml/panels/DanmakuSettingsPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const QVariantList rooms = {
        QVariantMap{{QStringLiteral("roomId"), QStringLiteral("room-a")},
                    {QStringLiteral("anchorName"), QStringLiteral("A")},
                    {QStringLiteral("danmakuEnabled"), true}},
        QVariantMap{{QStringLiteral("roomId"), QStringLiteral("room-b")},
                    {QStringLiteral("anchorName"), QStringLiteral("B")},
                    {QStringLiteral("danmakuEnabled"), true}},
    };
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("roomModel"), rooms},
        {QStringLiteral("visible"), true},
    }));
    QVERIFY2(panel != nullptr, qPrintable(component.errorString()));

    panel->setProperty("governanceScope", QStringLiteral("room"));
    auto *roomPicker = panel->findChild<QObject *>(QStringLiteral("danmakuGovernanceRoom"));
    QVERIFY(roomPicker != nullptr);
    roomPicker->setProperty("currentIndex", 0);
    QTRY_COMPARE(panel->property("governanceSettings").toMap().value(
                     QStringLiteral("duplicateWindowSeconds")), QVariant(7));
    roomPicker->setProperty("currentIndex", 1);
    QTRY_COMPARE(panel->property("governanceSettings").toMap().value(
                     QStringLiteral("duplicateWindowSeconds")), QVariant(9));

    roomPicker->setProperty("currentIndex", 0);
    QVERIFY(QMetaObject::invokeMethod(
        panel.get(), "applyGovernanceSetting", Q_ARG(QVariant, QStringLiteral("enabled")),
        Q_ARG(QVariant, QVariant(false))));
    QCOMPARE(controller.danmaku_.governanceSettingsForRoom(QStringLiteral("room-a"))
                 .value(QStringLiteral("enabled")), QVariant(false));
    QCOMPARE(controller.danmaku_.governanceSettingsForRoom(QStringLiteral("room-b"))
                 .value(QStringLiteral("enabled")), QVariant(true));
    controller.danmaku_.clearRoomGovernanceOverride(QStringLiteral("room-a"));
    QTRY_COMPARE(panel->property("governanceSettings").toMap().value(
                     QStringLiteral("enabled")), QVariant(true));
}

void QmlInteractionTest::refreshesSelectedRoomStatsFromRoomSignal()
{
    FakeDanmakuAppController controller;
    QQmlEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qml/panels/DanmakuSettingsPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const QVariantList rooms = {QVariantMap{{QStringLiteral("roomId"), QStringLiteral("room-a")},
                                            {QStringLiteral("anchorName"), QStringLiteral("A")},
                                            {QStringLiteral("danmakuEnabled"), true}}};
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("roomModel"), rooms},
        {QStringLiteral("visible"), true},
    }));
    QVERIFY2(panel != nullptr, qPrintable(component.errorString()));
    panel->setProperty("currentTab", QStringLiteral("stats"));
    auto *statsSection = panel->findChild<QObject *>(QStringLiteral("danmakuStatsSection"));
    QVERIFY(statsSection != nullptr);
    QCOMPARE(statsSection->property("stats").toMap().value(QStringLiteral("filtered")), QVariant(0));

    controller.danmaku_.setStats(QStringLiteral("room-a"),
                                  {{QStringLiteral("level"), QStringLiteral("normal")},
                                   {QStringLiteral("filtered"), 3},
                                   {QStringLiteral("duplicates"), 1}});
    QTRY_COMPARE(statsSection->property("stats").toMap().value(QStringLiteral("filtered")),
                 QVariant(3));
}

void QmlInteractionTest::showsDanmakuStatusOnRoomTile()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomTile.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    FakeRoomController controller;

    std::unique_ptr<QObject> connected(
        component.createWithInitialProperties(roomTileProperties(QStringLiteral("connected"))));
    QVERIFY2(connected != nullptr, qPrintable(component.errorString()));
    QObject *indicator = connected->findChild<QObject *>(QStringLiteral("danmakuConnectedIndicator"));
    QVERIFY(indicator != nullptr);
    QVERIFY(indicator->property("visible").toBool());

    QVariantMap blockedProperties = roomTileProperties(QStringLiteral("platform-blocked"));
    blockedProperties.insert(QStringLiteral("controller"),
                             QVariant::fromValue(static_cast<QObject *>(&controller)));
    std::unique_ptr<QObject> blocked(component.createWithInitialProperties(blockedProperties));
    QVERIFY2(blocked != nullptr, qPrintable(component.errorString()));
    QObject *retry = blocked->findChild<QObject *>(QStringLiteral("danmakuRetryAction"));
    QVERIFY(retry != nullptr);
    QVERIFY(retry->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(retry, "clicked"));
    QCOMPARE(controller.danmaku.retryRoom, QStringLiteral("63136"));
    QCOMPARE(controller.danmaku.retryCount, 1);
    QCOMPARE(controller.setPrimaryRoomCount, 0);

    QObject *primary = blocked->findChild<QObject *>(QStringLiteral("primaryRoomAction"));
    QVERIFY(primary != nullptr);
    QVERIFY(QMetaObject::invokeMethod(primary, "clicked"));
    QCOMPARE(controller.primaryRoom, QStringLiteral("63136"));
    QCOMPARE(controller.setPrimaryRoomCount, 1);

    std::unique_ptr<QObject> waiting(
        component.createWithInitialProperties(roomTileProperties(QStringLiteral("waiting"))));
    QVERIFY2(waiting != nullptr, qPrintable(component.errorString()));
    QObject *waitingLabel = waiting->findChild<QObject *>(QStringLiteral("danmakuWaitingLabel"));
    QVERIFY(waitingLabel != nullptr);
    QVERIFY(waitingLabel->property("visible").toBool());
}

void QmlInteractionTest::reportsAudioFocusCommandErrorsFromRoomActions()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent tileComponent(&engine,
                                QUrl(QStringLiteral("qrc:/qml/components/RoomTile.qml")));
    QVERIFY2(tileComponent.isReady(), qPrintable(tileComponent.errorString()));

    FakeRoomController tileController;
    tileController.audioError = QStringLiteral("音频操作失败");
    QVariantMap tileProperties = roomTileProperties(QStringLiteral("connected"));
    tileProperties[QStringLiteral("controller")] =
        QVariant::fromValue(static_cast<QObject *>(&tileController));
    tileProperties[QStringLiteral("audioFocused")] = true;
    std::unique_ptr<QObject> tile(tileComponent.createWithInitialProperties(tileProperties));
    QVERIFY2(tile != nullptr, qPrintable(tileComponent.errorString()));
    auto *tileAudio = tile->findChild<QObject *>(QStringLiteral("roomAudioButton"));
    QVERIFY(tileAudio != nullptr);
    QVERIFY(QMetaObject::invokeMethod(tileAudio, "clicked"));
    QCOMPARE(tileController.audioRoom, QString());
    QCOMPARE(tileController.workspaceModel.lastMessage(), QStringLiteral("音频操作失败"));
    QCOMPARE(tileController.workspaceModel.lastMessageLevel(), QStringLiteral("error"));

    WorkspaceModel workspace(nullptr);
    RoomListModel roomModel;
    RoomSnapshot snapshot;
    snapshot.roomId = QStringLiteral("63136");
    snapshot.metadata.roomId = snapshot.roomId;
    snapshot.metadata.anchorName = QStringLiteral("主播");
    snapshot.liveStatus = RoomLiveStatus::Online;
    snapshot.audioFocused = true;
    roomModel.applySnapshots({snapshot});

    FakeRoomController sidebarController;
    sidebarController.audioError = QStringLiteral("侧边栏音频失败");
    QQmlComponent sidebarComponent(&engine,
                                   QUrl(QStringLiteral("qrc:/qml/components/RoomSidebar.qml")));
    QVERIFY2(sidebarComponent.isReady(), qPrintable(sidebarComponent.errorString()));
    QQuickWindow sidebarWindow;
    sidebarWindow.resize(268, 640);
    sidebarWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&sidebarWindow));
    std::unique_ptr<QObject> sidebar(sidebarComponent.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(sidebarWindow.contentItem())},
        {QStringLiteral("width"), 268},
        {QStringLiteral("height"), 640},
        {QStringLiteral("controller"),
         QVariant::fromValue(static_cast<QObject *>(&sidebarController))},
        {QStringLiteral("workspaceModel"),
         QVariant::fromValue(static_cast<QObject *>(&workspace))},
        {QStringLiteral("roomModel"), QVariant::fromValue(static_cast<QObject *>(&roomModel))},
    }));
    QVERIFY2(sidebar != nullptr, qPrintable(sidebarComponent.errorString()));
    auto *roomList = sidebar->findChild<QObject *>(QStringLiteral("roomList"));
    QVERIFY(roomList != nullptr);
    QTRY_COMPARE(roomList->property("count").toInt(), 1);
    QQuickItem *row = nullptr;
    QTRY_VERIFY(QMetaObject::invokeMethod(roomList, "itemAtIndex",
                                          Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, 0))
                && row != nullptr);
    auto *sidebarAudio = row->findChild<QObject *>(QStringLiteral("roomSidebarAudioButton"));
    QVERIFY(sidebarAudio != nullptr);
    QVERIFY(QMetaObject::invokeMethod(sidebarAudio, "clicked"));
    QCOMPARE(sidebarController.audioRoom, QString());
    QCOMPARE(workspace.lastMessage(), QStringLiteral("侧边栏音频失败"));
    QCOMPARE(workspace.lastMessageLevel(), QStringLiteral("error"));
}



void QmlInteractionTest::rendersRoomAvatarFromModelRole()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomTile.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    std::unique_ptr<QObject> tile(component.createWithInitialProperties(roomTileProperties(
        QStringLiteral("connected"))));
    QVERIFY2(tile != nullptr, qPrintable(component.errorString()));
    QObject *avatar = tile->findChild<QObject *>(QStringLiteral("roomAvatarImage"));
    QVERIFY(avatar != nullptr);
    QCOMPARE(avatar->property("source").toUrl(),
             QUrl(QStringLiteral("https://example.com/avatar.jpg")));
}

void QmlInteractionTest::placesSidebarToggleBeforeBrandAndOpensHistory()
{
    QQmlApplicationEngine engine;
    auto *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    click(window->findChild<QObject *>("watchToolsButton"));
    auto *tools = window->findChild<QObject *>("watchToolsPanel");
    QVERIFY(tools && tools->property("visible").toBool());
    click(window->findChild<QObject *>("sidebarToggleButton"));
    auto *drawer = window->findChild<QObject *>("roomToolsDrawer");
    QTRY_VERIFY(drawer->property("visible").toBool());
    QVERIFY(!tools->property("visible").toBool());
    click(window->findChild<QObject *>("historyTab"));
    auto *library = window->findChild<QObject *>("roomLibraryView");
    QVERIFY(library != nullptr);
    QTRY_VERIFY(library->property("visible").toBool());
}

void QmlInteractionTest::placesNavigationEntryAfterSidebarToggleAndBeforeBrand()
{
    QQmlApplicationEngine engine;
    auto *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    auto *rail = window->findChild<QQuickItem *>("appNavigationRail");
    auto *navigation = window->findChild<QQuickItem *>("hamsterNavigationButton");
    auto *ranking = window->findChild<QQuickItem *>("maoziRankButton");
    auto *watch = window->findChild<QQuickItem *>("watchWorkspaceButton");
    QVERIFY(rail && navigation && ranking && watch);
    QCOMPARE(rail->width(), qreal(80));
    QCOMPARE(navigation->property("text").toString(), QStringLiteral("主播导航"));
    QVERIFY(navigation->mapToScene(QPointF()).y() < ranking->mapToScene(QPointF()).y());
    QVERIFY(ranking->mapToScene(QPointF()).y() < watch->mapToScene(QPointF()).y());
    click(navigation);
    QCOMPARE(window->property("currentView").toString(), QStringLiteral("navigation"));
    click(watch);
    QCOMPARE(window->property("currentView").toString(), QStringLiteral("monitoring"));
}

void QmlInteractionTest::makesRoomListAndNavigationPanelsMutuallyExclusive()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);

    QObject *roomSidebar = window->findChild<QObject *>(QStringLiteral("roomSidebar"));
    QObject *navigationPanel =
        window->findChild<QObject *>(QStringLiteral("guildNavigationPanel"));
    QVERIFY(roomSidebar != nullptr);
    QVERIFY(navigationPanel != nullptr);

    click(window->findChild<QObject *>(QStringLiteral("hamsterNavigationButton")));
    QTRY_VERIFY(navigationPanel->property("visible").toBool());
    QVERIFY(!roomSidebar->property("visible").toBool());
    QObject *navigationTitle =
        navigationPanel->findChild<QObject *>(QStringLiteral("guildNavigationTitle"));
    QVERIFY(navigationTitle != nullptr);
    QCOMPARE(navigationTitle->property("text").toString(), QStringLiteral("CSTG 主播导航"));

    click(window->findChild<QObject *>(QStringLiteral("sidebarToggleButton")));
    QTRY_VERIFY(roomSidebar->property("visible").toBool());
    QVERIFY(!navigationPanel->property("visible").toBool());
}

void QmlInteractionTest::usesFramelessWindowWithTitleBarInteractions()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(window->isVisible());

    QVERIFY(window->flags().testFlag(Qt::FramelessWindowHint));
    QVERIFY(window->findChild<QObject *>(QStringLiteral("titleBarDragArea")) != nullptr);
}

void QmlInteractionTest::keepsTaskbarMinimizeCapabilityForFramelessWindow()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);

    QVERIFY(window->flags().testFlag(Qt::FramelessWindowHint));
    QVERIFY2(window->flags().testFlag(Qt::WindowMinimizeButtonHint),
             "frameless window must retain the native minimize capability used by the taskbar");
}

void QmlInteractionTest::doubleClicksOnlyRoomPictureForFullscreen()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl("qrc:/qml/components/RoomTile.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(800, 600);
    window.show();
    auto properties = roomTileProperties("idle");
    properties["parent"] = QVariant::fromValue(window.contentItem());
    properties["width"] = 640;
    properties["height"] = 400;
    properties["avatarUrl"] = QUrl();
    std::unique_ptr<QObject> tile(component.createWithInitialProperties(properties));
    QVERIFY2(tile != nullptr, qPrintable(component.errorString()));
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    QSignalSpy fullscreen(tile.get(), SIGNAL(fullScreenToggleRequested()));
    QVERIFY(fullscreen.isValid());
    const QPoint pictureCenter(320, 200);
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, pictureCenter);
    QCOMPARE(fullscreen.count(), 0);
    QTest::mouseDClick(&window, Qt::RightButton, Qt::NoModifier, pictureCenter);
    QCOMPARE(fullscreen.count(), 0);
    QTest::mouseDClick(&window, Qt::LeftButton, Qt::NoModifier, pictureCenter);
    QCOMPARE(fullscreen.count(), 1);

    for (const QString &name : {QStringLiteral("roomVolumeSlider"),
                                QStringLiteral("roomQualitySelector"),
                                QStringLiteral("roomTopActions")}) {
        QVERIFY(QMetaObject::invokeMethod(tile.get(), "revealControls"));
        QQuickItem *control = tile->findChild<QQuickItem *>(name);
        QVERIFY(control != nullptr);
        const QPoint position = control->mapToScene(
            QPointF(control->width() / 2, control->height() / 2)).toPoint();
        QTest::mouseDClick(&window, Qt::LeftButton, Qt::NoModifier, position);
        QCOMPARE(fullscreen.count(), 1);
        QObject *popup = tile->findChild<QObject *>(QStringLiteral("roomQualityPopup"));
        QVERIFY(popup != nullptr);
        QVERIFY(QMetaObject::invokeMethod(popup, "close"));
        tile->setProperty("menuOpen", false);
    }
    tile->setProperty("menuOpen", true);
    QTest::mouseDClick(&window, Qt::LeftButton, Qt::NoModifier, pictureCenter);
    QCOMPARE(fullscreen.count(), 1);
    tile->setProperty("menuOpen", false);
    QObject *popup = tile->findChild<QObject *>(QStringLiteral("roomQualityPopup"));
    QVERIFY(popup != nullptr);
    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("visible").toBool());
    QTest::mouseDClick(&window, Qt::LeftButton, Qt::NoModifier, pictureCenter);
    QCOMPARE(fullscreen.count(), 1);
}

void QmlInteractionTest::updatesMaximizeIconWithWindowState()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QObject *button = window->findChild<QObject *>(QStringLiteral("maximizeButton"));
    QObject *icon = window->findChild<QObject *>(QStringLiteral("maximizeIcon"));
    QVERIFY(button != nullptr);
    QVERIFY(icon != nullptr);
    const QSizeF buttonSize(button->property("width").toReal(),
                            button->property("height").toReal());
    const QSizeF iconSize(icon->property("width").toReal(),
                          icon->property("height").toReal());

    for (int cycle = 0; cycle < 3; ++cycle) {
        window->showMaximized();
        QTRY_COMPARE(window->visibility(), QWindow::Maximized);
        QTRY_VERIFY(icon->property("source").toUrl().path().endsWith(
            QStringLiteral("window-restore.svg")));
        QCOMPARE(button->property("accessibilityLabel").toString(), QStringLiteral("还原窗口"));
        QCOMPARE(QQmlProperty::read(button, "Accessible.name", qmlContext(button)).toString(), QStringLiteral("还原窗口"));
        QCOMPARE(QQmlProperty::read(button, "ToolTip.text", qmlContext(button)).toString(), QStringLiteral("还原窗口"));
        QTRY_COMPARE(icon->property("status").toInt(), 1);
        QCOMPARE(QSizeF(button->property("width").toReal(),
                        button->property("height").toReal()), buttonSize);
        QCOMPARE(QSizeF(icon->property("width").toReal(),
                        icon->property("height").toReal()), iconSize);
        if (cycle == 0 && qEnvironmentVariableIsSet("DOUYU_WINDOW_CONTROLS_CAPTURE_DIR")) {
            QVERIFY(QTest::qWaitForWindowExposed(window));
            const QImage capture = window->grabWindow();
            QVERIFY(!capture.isNull());
            const QString directory = qEnvironmentVariable("DOUYU_WINDOW_CONTROLS_CAPTURE_DIR");
            QVERIFY(QDir().mkpath(directory));
            QVERIFY(capture.copy(capture.width() - 150, 0, 150, 50).save(
                QDir(directory).filePath(QStringLiteral("maximized.png"))));
        }

        window->showNormal();
        QTRY_COMPARE(window->visibility(), QWindow::Windowed);
        QTRY_VERIFY(icon->property("source").toUrl().path().endsWith(
            QStringLiteral("window-maximize.svg")));
        QCOMPARE(button->property("accessibilityLabel").toString(), QStringLiteral("最大化窗口"));
        QCOMPARE(QQmlProperty::read(button, "Accessible.name", qmlContext(button)).toString(), QStringLiteral("最大化窗口"));
        QCOMPARE(QQmlProperty::read(button, "ToolTip.text", qmlContext(button)).toString(), QStringLiteral("最大化窗口"));
        QTRY_COMPARE(icon->property("status").toInt(), 1);
        QCOMPARE(QSizeF(button->property("width").toReal(),
                        button->property("height").toReal()), buttonSize);
        QCOMPARE(QSizeF(icon->property("width").toReal(),
                        icon->property("height").toReal()), iconSize);
        if (cycle == 0 && qEnvironmentVariableIsSet("DOUYU_WINDOW_CONTROLS_CAPTURE_DIR")) {
            QVERIFY(QTest::qWaitForWindowExposed(window));
            const QImage capture = window->grabWindow();
            QVERIFY(!capture.isNull());
            QVERIFY(capture.copy(capture.width() - 150, 0, 150, 50).save(
                QDir(qEnvironmentVariable("DOUYU_WINDOW_CONTROLS_CAPTURE_DIR"))
                    .filePath(QStringLiteral("normal.png"))));
        }
    }
}

void QmlInteractionTest::doesNotExposeGroupManagementControls()
{
    registerQmlTypes();
    WorkspaceModel workspace(nullptr);
    workspace.setWorkspaceData(
        {},
        {{QStringLiteral("group-a"), QStringLiteral("赛事"), {QStringLiteral("63136")} },
         {QStringLiteral("group-b"), QStringLiteral("关注"), {QStringLiteral("63137")} }},
        {},
        QStringLiteral("group-a"));
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomSidebar.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> sidebar(component.createWithInitialProperties({
        {QStringLiteral("workspaceModel"),
         QVariant::fromValue(static_cast<QObject *>(&workspace))},
        {QStringLiteral("roomModel"), QVariantList{}},
    }));
    QVERIFY2(sidebar != nullptr, qPrintable(component.errorString()));

    QVERIFY(sidebar->findChild<QObject *>(QStringLiteral("groupTabs")) == nullptr);
    QVERIFY(sidebar->findChild<QObject *>(QStringLiteral("groupOverflowButton")) == nullptr);
    QVERIFY(sidebar->findChild<QObject *>(QStringLiteral("groupManagementButton")) == nullptr);
}

void QmlInteractionTest::rendersAndAddsSearchCandidate()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(640, 480));
    hostWindow.show();
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/dialogs/AddRoomDialog.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    FakeSearchController controller;
    std::unique_ptr<QObject> dialog(component.createWithInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
    }));
    QVERIFY2(dialog != nullptr, qPrintable(component.errorString()));
    dialog->setProperty("visible", true);
    QTRY_VERIFY(dialog->property("visible").toBool());

    QObject *results = dialog->findChild<QObject *>(QStringLiteral("searchResultList"));
    QVERIFY(results != nullptr);
    QTRY_COMPARE(results->property("count").toInt(), 1);
    QQuickItem *resultRowItem = nullptr;
    QVERIFY(QMetaObject::invokeMethod(results,
                                      "itemAtIndex",
                                      Q_RETURN_ARG(QQuickItem *, resultRowItem),
                                      Q_ARG(int, 0)));
    QObject *resultRow = resultRowItem;
    QVERIFY(resultRow != nullptr);
    QObject *addButton = resultRow->findChild<QObject *>(QStringLiteral("addSearchResultButton"));
    QVERIFY(addButton != nullptr);
    click(addButton);
    QCOMPARE(controller.addedRoomId, QStringLiteral("63136"));
}

void QmlInteractionTest::exposesSupportedLayoutOptions()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(window->isVisible());

    QObject *layoutButton = window->findChild<QObject *>(QStringLiteral("layoutMenuButton"));
    QVERIFY(layoutButton != nullptr);
    click(window->findChild<QObject *>(QStringLiteral("watchToolsButton")));
    click(layoutButton);
    QObject *layoutMenu = window->findChild<QObject *>(QStringLiteral("layoutMenu"));
    QVERIFY(layoutMenu != nullptr);
    QTRY_VERIFY(layoutMenu->property("visible").toBool());

    QVERIFY(window->findChild<QObject *>(QStringLiteral("autoLayoutOption")) != nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("primaryLayoutOption")) != nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("dualPrimaryLayoutOption")) != nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("singleLayoutOption")) == nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("grid2LayoutOption")) == nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("grid3LayoutOption")) == nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("grid3x3LayoutOption")) == nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("primaryTwoLayoutOption")) == nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("splitHorizontalLayoutOption")) == nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("splitVerticalLayoutOption")) == nullptr);
}

void QmlInteractionTest::alignsLayoutMenuSelectionIndicator()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/WatchToolsPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    FakeHeaderController controller;
    QQuickWindow window;
    window.resize(QSize(800, 600));
    window.show();
    std::unique_ptr<QObject> header(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(window.contentItem())},
        {QStringLiteral("width"), 360},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
    }));
    QVERIFY2(header != nullptr, qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(header.get(), "open"));
    QTRY_VERIFY(header->property("visible").toBool());
    click(header->findChild<QObject *>(QStringLiteral("layoutMenuButton")));
    auto *menu = header->findChild<QObject *>(QStringLiteral("layoutMenu"));
    QVERIFY(menu != nullptr);
    QTRY_VERIFY(menu->property("visible").toBool());
    for (const auto &name : {"autoLayoutOption", "primaryLayoutOption"}) {
        auto *option = header->findChild<QObject *>(QString::fromLatin1(name));
        QVERIFY(option != nullptr);
        auto *indicator = qobject_cast<QQuickItem *>(option->property("indicator").value<QObject *>());
        auto *content = qobject_cast<QQuickItem *>(option->property("contentItem").value<QObject *>());
        QVERIFY(indicator != nullptr);
        QVERIFY(content != nullptr);
        QVERIFY(indicator->width() > 0);
        QVERIFY(indicator->x() >= 0);
        QVERIFY2(indicator->x() + indicator->width() + 4 <= content->x(),
                 qPrintable(QStringLiteral("indicator x=%1 width=%2 content x=%3")
                                .arg(indicator->x()).arg(indicator->width()).arg(content->x())));
        QCOMPARE(indicator->property("visible").toBool(), option->property("checked").toBool());
    }
}

void QmlInteractionTest::hidesRoomQualitySelectorIndicator()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomTile.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    FakeRoomController controller;
    QVariantMap properties = roomTileProperties(QStringLiteral("connected"));
    properties[QStringLiteral("controller")] = QVariant::fromValue(static_cast<QObject *>(&controller));
    std::unique_ptr<QObject> tile(component.createWithInitialProperties(properties));
    QVERIFY2(tile != nullptr, qPrintable(component.errorString()));
    auto *quality = tile->findChild<QObject *>(QStringLiteral("roomQualitySelector"));
    QVERIFY(quality != nullptr);
    auto *indicator = qobject_cast<QQuickItem *>(quality->property("indicator").value<QObject *>());
    QVERIFY(indicator == nullptr || !indicator->isVisible());
    QVERIFY(quality->property("enabled").toBool());
    QCOMPARE(quality->property("count").toInt(), 2);
}

void QmlInteractionTest::showsCstgTerminalName()
{
    QQmlApplicationEngine engine;
    auto *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QCOMPARE(window->title(), QStringLiteral("CSTG特勤终端"));
    auto *brand = window->findChild<QQuickItem *>("brandIcon");
    QVERIFY(brand != nullptr);
    auto *appIcon = window->findChild<QQuickItem *>("appIcon");
    QVERIFY(appIcon != nullptr);
    QCOMPARE(appIcon->size(), QSizeF(26, 26));
    QTRY_COMPARE(appIcon->property("status").toInt(), 1);
    QVERIFY(appIcon->property("source").toUrl().toString().endsWith("icon-app.png"));
    QVERIFY(sceneBounds(appIcon).right() + 8 <= sceneBounds(brand).left());
    QVERIFY(!sceneBounds(appIcon).intersects(sceneBounds(brand)));
    QCOMPARE(brand->size(), QSizeF(210, 24));
    QTRY_COMPARE(brand->property("status").toInt(), 1);
    QVERIFY(brand->property("source").toUrl().toString().endsWith("wordmark-night.png"));
    QCOMPARE(QQmlProperty::read(brand, "Accessible.name", qmlContext(brand)).toString(), QStringLiteral("CSTG特勤终端"));
}

void QmlInteractionTest::enablesDualPrimaryLayoutAfterFourthRoomIsAdded()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/WatchToolsPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    FakeHeaderController controller;
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(800, 600));
    hostWindow.show();
    std::unique_ptr<QObject> header(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("width"), 360},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
    }));
    QVERIFY2(header != nullptr, qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(header.get(), "open"));
    QTRY_VERIFY(header->property("visible").toBool());

    QObject *layoutButton = header->findChild<QObject *>(QStringLiteral("layoutMenuButton"));
    QObject *dualPrimary = header->findChild<QObject *>(QStringLiteral("dualPrimaryLayoutOption"));
    QVERIFY(layoutButton != nullptr);
    QVERIFY(dualPrimary != nullptr);
    QVERIFY(!dualPrimary->property("enabled").toBool());

    controller.setRoomCount(4);
    QTRY_VERIFY(dualPrimary->property("enabled").toBool());

    click(layoutButton);
    click(dualPrimary);
    QCOMPARE(controller.lastLayout, QStringLiteral("primary-two"));
}

void QmlInteractionTest::closesToastFromQml()
{
    WorkspaceModel workspace(nullptr);
    workspace.setLastMessage(QStringLiteral("播放地址不可用"));

    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/ToastViewport.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    std::unique_ptr<QObject> toast(component.createWithInitialProperties({
        {QStringLiteral("workspaceModel"),
         QVariant::fromValue(static_cast<QObject *>(&workspace))},
    }));
    QVERIFY2(toast != nullptr, qPrintable(component.errorString()));

    QObject *dismiss = toast->findChild<QObject *>(QStringLiteral("toastDismissButton"));
    QVERIFY(dismiss != nullptr);
    QVERIFY(toast->property("message").toString() == QStringLiteral("播放地址不可用"));
    QVERIFY(QMetaObject::invokeMethod(dismiss, "clicked"));
    QTRY_COMPARE(workspace.lastMessage(), QString());
    QTRY_COMPARE(toast->property("message").toString(), QString());
}

void QmlInteractionTest::autoDismissesToastBySeverity()
{
    WorkspaceModel workspace(nullptr);
    workspace.setLastMessage(QStringLiteral("房间数据已刷新"), QStringLiteral("success"), 40);

    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/ToastViewport.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> toast(component.createWithInitialProperties({
        {QStringLiteral("workspaceModel"), QVariant::fromValue(static_cast<QObject *>(&workspace))},
    }));
    QVERIFY2(toast != nullptr, qPrintable(component.errorString()));
    QCOMPARE(toast->property("level").toString(), QStringLiteral("success"));
    QObject *surface = toast->findChild<QObject *>(QStringLiteral("toastSurface"));
    QVERIFY(surface != nullptr);
    QVERIFY(!surface->property("visible").toBool());
    QTRY_COMPARE_WITH_TIMEOUT(toast->property("message").toString(), QString(), 500);
}

void QmlInteractionTest::exposesGlobalAudioControls()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/WatchToolsPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    FakeHeaderController controller;
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(800, 600));
    hostWindow.show();
    std::unique_ptr<QObject> header(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("width"), 360},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
    }));
    QVERIFY2(header != nullptr, qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(header.get(), "open"));
    QTRY_VERIFY(header->property("visible").toBool());

    QObject *mute = header->findChild<QObject *>(QStringLiteral("globalMuteButton"));
    QObject *single = header->findChild<QObject *>(QStringLiteral("audioSingleButton"));
    QObject *multi = header->findChild<QObject *>(QStringLiteral("audioMultiButton"));
    QVERIFY(mute != nullptr);
    QVERIFY(single != nullptr);
    QVERIFY(multi != nullptr);
    QVERIFY(single->property("checked").toBool());
    QVERIFY(!multi->property("checked").toBool());

    click(mute);
    QCOMPARE(controller.lastMuted, true);
    QTRY_VERIFY(mute->property("checked").toBool());

    click(multi);
    QCOMPARE(controller.lastAudioMode, QStringLiteral("multi"));
    QTRY_VERIFY(multi->property("checked").toBool());
    QVERIFY(!single->property("checked").toBool());
}

void QmlInteractionTest::distinguishesWorkspaceAndLayoutActions()
{
    QQmlApplicationEngine engine;
    auto *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    auto *workspace = window->findChild<QObject *>("workspaceButton");
    auto *layout = window->findChild<QObject *>("layoutMenuButton");
    QVERIFY(workspace && layout);
    QVERIFY(QQmlProperty::read(workspace, "icon.source", qmlContext(workspace)).toUrl()
            != QQmlProperty::read(layout, "icon.source", qmlContext(layout)).toUrl());
    QVERIFY(!window->findChild<QObject *>("nativeQtWorkspaceSubtitle"));
}

void QmlInteractionTest::usesGroupedHeaderControls()
{
    QQmlApplicationEngine engine;
    auto *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    auto *header = window->findChild<QQuickItem *>("appHeader");
    QVERIFY(header != nullptr);
    QCOMPARE(header->height(), qreal(36));
    for (const char *name : {"watchToolsButton", "themeToggleButton", "minimizeButton", "maximizeButton", "closeButton"}) {
        auto *control = header->findChild<QQuickItem *>(QString::fromLatin1(name));
        QVERIFY2(control != nullptr, name);
        QCOMPARE(control->height(), qreal(28));
        auto *content = qvariant_cast<QQuickItem *>(control->property("contentItem"));
        QVERIFY2(content != nullptr, name);
        QTRY_VERIFY_WITH_TIMEOUT(content->height() >= 16, 1000);
    }
    for (const char *name : {"minimizeIcon", "maximizeIcon", "windowCloseIcon"}) {
        auto *icon = header->findChild<QQuickItem *>(QString::fromLatin1(name));
        QVERIFY2(icon != nullptr, name);
        QTRY_VERIFY_WITH_TIMEOUT(icon->width() >= 15 && icon->height() >= 15, 1000);
    }
    click(header->findChild<QObject *>("watchToolsButton"));
    auto *tools = window->findChild<QObject *>("watchToolsPanel");
    QTRY_VERIFY(tools->property("visible").toBool());
    for (const char *name : {"sidebarToggleButton", "danmakuButton", "monitoringButton", "workspaceButton", "layoutMenuButton", "soundMasterButton", "fullscreenButton"})
        QVERIFY2(tools->findChild<QObject *>(QString::fromLatin1(name)) != nullptr, name);
}

void QmlInteractionTest::groupsSoundControlsAndExposesFullscreen()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl("qrc:/qml/components/WatchToolsPanel.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    FakeHeaderController controller;
    QQuickWindow window;
    window.resize(800, 600);
    window.show();
    std::unique_ptr<QObject> tools(component.createWithInitialProperties({
        {"parent", QVariant::fromValue(window.contentItem())},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))}}));
    QVERIFY2(tools != nullptr, qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(tools.get(), "open"));
    click(tools->findChild<QObject *>("soundMasterButton"));
    auto *sound = tools->findChild<QObject *>("soundMasterPopover");
    QVERIFY(sound != nullptr);
    QTRY_VERIFY(sound->property("visible").toBool());
    QCOMPARE(tools->property("currentSection").toString(), QStringLiteral("sound"));
    auto *back = tools->findChild<QQuickItem *>("watchToolsBackButton");
    QVERIFY(back != nullptr);
    QTRY_VERIFY(back->hasActiveFocus());
    for (const char *name : {"globalMuteButton", "audioSingleButton", "audioMultiButton"})
        QVERIFY(tools->findChild<QObject *>(QString::fromLatin1(name)) != nullptr);
    click(back);
    QTRY_VERIFY(!sound->property("visible").toBool());
    click(tools->findChild<QObject *>("fullscreenButton"));
    QVERIFY(controller.fullScreenToggled);
}

void QmlInteractionTest::positionsSoundPopoverLikeDanmakuPanel()
{
    QQmlApplicationEngine engine;
    auto *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    click(window->findChild<QObject *>("watchToolsButton"));
    click(window->findChild<QObject *>("soundMasterButton"));
    auto *popup = window->findChild<QObject *>("soundMasterPopover");
    QVERIFY(popup != nullptr);
    QTRY_VERIFY(popup->property("visible").toBool());
    auto *section = qobject_cast<QQuickItem *>(popup);
    QVERIFY(section != nullptr);
    QTRY_VERIFY(QRectF(QPointF(), window->size()).contains(sceneBounds(section)));
    click(window->findChild<QObject *>("watchToolsBackButton"));
    QTRY_VERIFY(!popup->property("visible").toBool());
}

void QmlInteractionTest::truncatesLongRoomTitleBeforeActions()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomTile.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QVariantMap properties = roomTileProperties(QStringLiteral("idle"));
    properties[QStringLiteral("title")] = QString(180, QLatin1Char('长'));
    std::unique_ptr<QObject> tile(component.createWithInitialProperties(properties));
    QVERIFY2(tile != nullptr, qPrintable(component.errorString()));
    QObject *title = tile->findChild<QObject *>(QStringLiteral("roomTitleText"));
    QObject *actions = tile->findChild<QObject *>(QStringLiteral("roomActionBar"));
    QVERIFY(title != nullptr);
    QVERIFY(actions != nullptr);
    QCOMPARE(title->property("elide").toInt(), static_cast<int>(Qt::ElideRight));
    QVERIFY(title->property("width").toDouble() < tile->property("width").toDouble());
}

void QmlInteractionTest::keepsSidebarMetadataClearOfActionsForLongTitles()
{
    registerQmlTypes();
    WorkspaceModel workspace(nullptr);
    RoomListModel roomModel;
    RoomSnapshot snapshot;
    snapshot.roomId = QStringLiteral("63136");
    snapshot.metadata.roomId = snapshot.roomId;
    snapshot.metadata.anchorName = QStringLiteral("主播");
    snapshot.metadata.title = QString(180, QLatin1Char('长'));
    snapshot.liveStatus = RoomLiveStatus::Online;
    roomModel.applySnapshots({snapshot});

    FakeRoomController controller;
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomSidebar.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(360, 640));
    hostWindow.show();
    std::unique_ptr<QObject> sidebar(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("width"), 360},
        {QStringLiteral("height"), 640},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("workspaceModel"), QVariant::fromValue(static_cast<QObject *>(&workspace))},
        {QStringLiteral("roomModel"), QVariant::fromValue(static_cast<QObject *>(&roomModel))},
    }));
    QVERIFY2(sidebar != nullptr, qPrintable(component.errorString()));

    QObject *list = sidebar->findChild<QObject *>(QStringLiteral("roomList"));
    QVERIFY(list != nullptr);
    QTRY_COMPARE(list->property("count").toInt(), 1);
    QQuickItem *row = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(QMetaObject::invokeMethod(list, "itemAtIndex",
                                                       Q_RETURN_ARG(QQuickItem *, row),
                                                       Q_ARG(int, 0))
                                 && row != nullptr,
                             1000);

    auto *title = qobject_cast<QQuickItem *>(
        row->findChild<QObject *>(QStringLiteral("sidebarRoomTitle")));
    auto *actionBar = qobject_cast<QQuickItem *>(
        row->findChild<QObject *>(QStringLiteral("sidebarRoomActionBar")));
    QVERIFY(title != nullptr);
    QVERIFY(actionBar != nullptr);
    QVERIFY(title->mapToItem(row, QPointF(title->width(), 0)).x() <= actionBar->x());
}

void QmlInteractionTest::keepsCurrentRoomHoverClearOfActions()
{
    registerQmlTypes();
    WorkspaceModel workspace(nullptr);
    FakeRoomController controller;
    RoomListModel roomModel;
    RoomSnapshot snapshot;
    snapshot.roomId = QStringLiteral("63136");
    snapshot.metadata.roomId = snapshot.roomId;
    snapshot.metadata.anchorName = QStringLiteral("当前主播");
    snapshot.metadata.title = QStringLiteral("当前房间");
    snapshot.liveStatus = RoomLiveStatus::Online;
    snapshot.playbackHealth = RoomPlaybackHealth::Playing;
    snapshot.isPrimary = false;
    roomModel.applySnapshots({snapshot});

    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomSidebar.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(268, 640);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    std::unique_ptr<QObject> sidebar(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(window.contentItem())},
        {QStringLiteral("width"), 268},
        {QStringLiteral("height"), 640},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("workspaceModel"), QVariant::fromValue(static_cast<QObject *>(&workspace))},
        {QStringLiteral("roomModel"), QVariant::fromValue(static_cast<QObject *>(&roomModel))},
    }));
    QVERIFY2(sidebar != nullptr, qPrintable(component.errorString()));

    auto *list = sidebar->findChild<QQuickItem *>(QStringLiteral("roomList"));
    QVERIFY(list != nullptr);
    QTRY_COMPARE(list->property("count").toInt(), 1);
    QQuickItem *row = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(QMetaObject::invokeMethod(list,
                                                       "itemAtIndex",
                                                       Q_RETURN_ARG(QQuickItem *, row),
                                                       Q_ARG(int, 0))
                                 && row != nullptr,
                             1000);

    auto *hover = row->findChild<QQuickItem *>(QStringLiteral("roomSidebarHoverArea"));
    auto *actions = row->findChild<QQuickItem *>(QStringLiteral("sidebarRoomActionBar"));
    auto *remove = row->findChild<QQuickItem *>(QStringLiteral("roomRemoveButton"));
    auto *surface = row->findChild<QQuickItem *>(QStringLiteral("roomSidebarSelectionSurface"));
    QVERIFY(hover != nullptr);
    QVERIFY(actions != nullptr);
    QVERIFY(remove != nullptr);
    QVERIFY(surface != nullptr);
    QVERIFY(hover->x() + hover->width() <= actions->x());

    const QColor idleColor = surface->property("color").value<QColor>();
    const QPoint hoverCenter =
        hover->mapToScene(QPointF(hover->width() / 2, hover->height() / 2)).toPoint();
    QTest::mouseMove(&window, hoverCenter);
    QTRY_VERIFY(hover->property("containsMouse").toBool());
    QTRY_COMPARE(surface->property("color").value<QColor>(), QColor(QStringLiteral("#1e2624")));
    QVERIFY(surface->property("color").value<QColor>() != idleColor);

    const QPoint removeCenter =
        remove->mapToScene(QPointF(remove->width() / 2, remove->height() / 2)).toPoint();
    QTest::mouseMove(&window, removeCenter);
    QTRY_VERIFY(remove->property("hovered").toBool());
    QVERIFY(!hover->property("containsMouse").toBool());
    QTest::qWait(220);
    QTRY_COMPARE(surface->property("color").value<QColor>(), QColor(QStringLiteral("#1e2624")));
    QTest::mouseMove(&window, QPoint(2, 2));
    QTRY_COMPARE(surface->property("color").value<QColor>(), idleColor);
}

void QmlInteractionTest::keepsHistoryHoverClearOfActions()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomLibraryView.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QQuickWindow hostWindow;
    hostWindow.resize(QSize(360, 240));
    hostWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&hostWindow));

    FakeRoomController controller;
    const QVariantList rooms{
        QVariantMap{
            {QStringLiteral("roomId"), QStringLiteral("63136")},
            {QStringLiteral("anchorName"), QStringLiteral("主播")},
            {QStringLiteral("title"), QStringLiteral("历史房间")},
            {QStringLiteral("avatarUrl"), QUrl(QStringLiteral("qrc:/missing/history-avatar.png"))},
            {QStringLiteral("active"), false},
            {QStringLiteral("favorite"), false},
            {QStringLiteral("lastOpenedAtMs"), 1},
        },
    };
    std::unique_ptr<QObject> library(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("width"), 360},
        {QStringLiteral("height"), 240},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("libraryRooms"), rooms},
    }));
    QVERIFY2(library != nullptr, qPrintable(component.errorString()));

    auto *list = library->findChild<QObject *>(QStringLiteral("historyList"));
    QVERIFY(list != nullptr);
    QTRY_COMPARE(list->property("count").toInt(), 1);

    QQuickItem *row = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(QMetaObject::invokeMethod(list, "itemAtIndex",
                                                       Q_RETURN_ARG(QQuickItem *, row),
                                                       Q_ARG(int, 0))
                                 && row != nullptr,
                             1000);
    auto *hover = row->findChild<QQuickItem *>(QStringLiteral("historyHoverArea"));
    auto *actions = row->findChild<QQuickItem *>(QStringLiteral("libraryRoomActionBar"));
    auto *remove = row->findChild<QQuickItem *>(QStringLiteral("removeHistoryButton"));
    auto *avatarImage = row->findChild<QObject *>(QStringLiteral("roomHistoryAvatarImage"));
    auto *avatarFallback = row->findChild<QObject *>(QStringLiteral("roomHistoryAvatarFallback"));
    auto *surface = row->childItems().isEmpty() ? nullptr : row->childItems().first();
    QVERIFY(hover != nullptr);
    QVERIFY(actions != nullptr);
    QVERIFY(remove != nullptr);
    QVERIFY(avatarImage != nullptr);
    QVERIFY(avatarFallback != nullptr);
    QVERIFY(surface != nullptr);
    QTRY_COMPARE(avatarImage->property("status").toInt(), 3);
    QVERIFY(!avatarImage->property("visible").toBool());
    QVERIFY(avatarFallback->property("visible").toBool());
    QVERIFY(hover->x() + hover->width() <= actions->x());

    const QColor idleColor = surface->property("color").value<QColor>();
    QTest::mouseMove(&hostWindow, hover->mapToScene(
        QPointF(hover->width() / 2, hover->height() / 2)).toPoint());
    QTRY_COMPARE(surface->property("color").value<QColor>(), QColor(QStringLiteral("#1e2624")));

    const QPoint removeCenter =
        remove->mapToScene(QPointF(remove->width() / 2, remove->height() / 2)).toPoint();
    QTest::mouseMove(&hostWindow, removeCenter);
    QTRY_VERIFY(remove->property("hovered").toBool());
    QVERIFY(!hover->property("containsMouse").toBool());
    QTest::qWait(220);
    QTRY_COMPARE(surface->property("color").value<QColor>(), QColor(QStringLiteral("#1e2624")));
    QTest::mouseMove(&hostWindow, QPoint(2, 2));
    QTRY_COMPARE(surface->property("color").value<QColor>(), idleColor);
}

void QmlInteractionTest::keepsFavoritesHoverClearOfActions()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomLibraryView.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QQuickWindow hostWindow;
    hostWindow.resize(QSize(360, 240));
    hostWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&hostWindow));

    FakeRoomController controller;
    const QVariantList rooms{
        QVariantMap{
            {QStringLiteral("roomId"), QStringLiteral("63136")},
            {QStringLiteral("anchorName"), QStringLiteral("收藏主播")},
            {QStringLiteral("title"), QStringLiteral("收藏房间")},
            {QStringLiteral("avatarUrl"), QUrl()},
            {QStringLiteral("active"), false},
            {QStringLiteral("favorite"), true},
            {QStringLiteral("lastOpenedAtMs"), 0},
        },
    };
    std::unique_ptr<QObject> library(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("width"), 360},
        {QStringLiteral("height"), 240},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("libraryRooms"), rooms},
        {QStringLiteral("favoritesOnly"), true},
    }));
    QVERIFY2(library != nullptr, qPrintable(component.errorString()));

    auto *list = library->findChild<QQuickItem *>(QStringLiteral("historyList"));
    QVERIFY(list != nullptr);
    QTRY_COMPARE(list->property("count").toInt(), 1);
    QQuickItem *row = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(QMetaObject::invokeMethod(list,
                                                       "itemAtIndex",
                                                       Q_RETURN_ARG(QQuickItem *, row),
                                                       Q_ARG(int, 0))
                                 && row != nullptr,
                             1000);

    auto *hover = row->findChild<QQuickItem *>(QStringLiteral("historyHoverArea"));
    auto *actions = row->findChild<QQuickItem *>(QStringLiteral("libraryRoomActionBar"));
    auto *open = row->findChild<QQuickItem *>(QStringLiteral("openHistoryRoomButton"));
    auto *remove = row->findChild<QQuickItem *>(QStringLiteral("removeHistoryButton"));
    auto *surface = row->childItems().isEmpty() ? nullptr : row->childItems().first();
    QVERIFY(hover != nullptr);
    QVERIFY(actions != nullptr);
    QVERIFY(open != nullptr);
    QVERIFY(remove != nullptr);
    QVERIFY(surface != nullptr);
    QVERIFY(!remove->property("visible").toBool());
    QVERIFY(hover->x() + hover->width() <= actions->x());

    const QColor idleColor = surface->property("color").value<QColor>();
    QTest::mouseMove(&hostWindow, hover->mapToScene(
        QPointF(hover->width() / 2, hover->height() / 2)).toPoint());
    QTRY_COMPARE(surface->property("color").value<QColor>(), QColor(QStringLiteral("#1e2624")));

    const QPoint openCenter =
        open->mapToScene(QPointF(open->width() / 2, open->height() / 2)).toPoint();
    QTest::mouseMove(&hostWindow, openCenter);
    QTRY_VERIFY(open->property("hovered").toBool());
    QVERIFY(!hover->property("containsMouse").toBool());
    QTest::qWait(220);
    QTRY_COMPARE(surface->property("color").value<QColor>(), QColor(QStringLiteral("#1e2624")));
    QTest::mouseMove(&hostWindow, QPoint(2, 2));
    QTRY_COMPARE(surface->property("color").value<QColor>(), idleColor);
}

void QmlInteractionTest::exposesRoomVolumeAndRefreshControls()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomTile.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    FakeRoomController controller;
    QQuickWindow hostWindow;
    hostWindow.resize(800, 600);
    hostWindow.show();
    QVERIFY(QTest::qWaitForWindowExposed(&hostWindow));
    QVariantMap properties = roomTileProperties(QStringLiteral("connected"));
    properties[QStringLiteral("controller")] = QVariant::fromValue(static_cast<QObject *>(&controller));
    properties[QStringLiteral("parent")] = QVariant::fromValue(hostWindow.contentItem());
    std::unique_ptr<QObject> tile(component.createWithInitialProperties(properties));
    QVERIFY2(tile != nullptr, qPrintable(component.errorString()));

    QObject *slider = tile->findChild<QObject *>(QStringLiteral("roomVolumeSlider"));
    QObject *refresh = tile->findChild<QObject *>(QStringLiteral("refreshRoomAction"));
    QObject *topBar = tile->findChild<QObject *>(QStringLiteral("roomTopBar"));
    QObject *topActions = tile->findChild<QObject *>(QStringLiteral("roomTopActions"));
    QObject *cardSurface = tile->findChild<QObject *>(QStringLiteral("roomCardSurface"));
    QVERIFY(slider != nullptr);
    QVERIFY(refresh != nullptr);
    QVERIFY(topBar != nullptr);
    QVERIFY(topActions != nullptr);
    QVERIFY(cardSurface != nullptr);
    QVERIFY(topActions->property("width").toDouble() >= 32.0);
    QCOMPARE(qobject_cast<QQuickItem *>(topBar)->parentItem(),
             qobject_cast<QQuickItem *>(tile.get()));
    QVERIFY(topBar->property("z").toDouble() > cardSurface->property("z").toDouble());
    QVERIFY(QMetaObject::invokeMethod(topActions, "clicked"));
    QTRY_VERIFY(tile->property("menuOpen").toBool());
    QObject *menu = tile->findChild<QObject *>(QStringLiteral("roomControlMenu"));
    QVERIFY(menu != nullptr);
    QVERIFY(qobject_cast<QQuickItem *>(menu)->parentItem()
            != qobject_cast<QQuickItem *>(tile.get()));
    QVERIFY(menu->property("visible").toBool());
    const auto *menuSceneItem = qobject_cast<QQuickItem *>(menu);
    QVERIFY(QRectF(QPointF(), hostWindow.size()).contains(
        QRectF(menuSceneItem->mapToScene(QPointF()), menuSceneItem->size())));
    QVERIFY(tile->property("z").toDouble() > 0.0);
    const auto *menuItem = qobject_cast<QQuickItem *>(menu);
    for (QQuickItem *action : menuItem->findChildren<QQuickItem *>()) {
        if (!action->isVisible() || !action->objectName().endsWith(QStringLiteral("Action"))) continue;
        const QRectF actionRect(action->mapToItem(menuItem, QPointF()), action->size());
        QVERIFY2(QRectF(QPointF(), menuItem->size()).contains(actionRect), qPrintable(action->objectName()));
    }
    tile->setProperty("menuOpen", false);
    QCOMPARE(slider->objectName(), QStringLiteral("roomVolumeSlider"));
    QObject *sliderHandle = tile->findChild<QObject *>(QStringLiteral("roomVolumeSliderHandle"));
    QVERIFY(sliderHandle != nullptr);
    QVERIFY(sliderHandle->property("implicitWidth").toDouble() <= 14.0);
    QVERIFY(sliderHandle->property("implicitHeight").toDouble() <= 14.0);

    slider->setProperty("value", 0.35);
    QVERIFY(QMetaObject::invokeMethod(slider, "moved"));
    QCOMPARE(controller.lastVolumeRoom, QStringLiteral("63136"));
    QCOMPARE(controller.lastVolume, 35);

    QVERIFY(QMetaObject::invokeMethod(refresh, "clicked"));
    QCOMPARE(controller.refreshedRoom, QStringLiteral("63136"));
}

void QmlInteractionTest::keepsKeyboardRoomControlsVisibleAndHiddenBarsInactive()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl("qrc:/qml/components/RoomTile.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(800, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    auto properties = roomTileProperties("connected");
    properties["parent"] = QVariant::fromValue(window.contentItem());
    properties["width"] = 640;
    properties["height"] = 400;
    properties["avatarUrl"] = QUrl();
    std::unique_ptr<QObject> tile(component.createWithInitialProperties(properties));
    QVERIFY2(tile != nullptr, qPrintable(component.errorString()));
    auto *timer = tile->findChild<QObject *>("roomControlsTimer");
    auto *button = tile->findChild<QQuickItem *>("roomTopActions");
    QVERIFY(timer && button);
    button->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(button->hasActiveFocus());
    QVERIFY(QMetaObject::invokeMethod(timer, "triggered"));
    QVERIFY(tile->property("controlsVisible").toBool());
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(320, 200));
    QTest::mouseMove(&window, QPoint(760, 560));
    QVERIFY(QMetaObject::invokeMethod(timer, "triggered"));
    QVERIFY(!tile->property("controlsVisible").toBool());
    for (const char *name : {"roomTopBar", "roomBottomBar"}) {
        auto *bar = tile->findChild<QQuickItem *>(QString::fromLatin1(name));
        QVERIFY(bar != nullptr);
        QVERIFY(!bar->isEnabled());
    }
}

void QmlInteractionTest::keepsBottomRowRoomMenuInsideWindowAndClosesOutside()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl("qrc:/qml/components/RoomTile.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(640, 480);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.requestActivate();
    QTRY_VERIFY(window.isActive());
    auto properties = roomTileProperties("idle");
    properties["parent"] = QVariant::fromValue(window.contentItem());
    properties["x"] = 320;
    properties["y"] = 320;
    properties["width"] = 320;
    properties["height"] = 160;
    properties["avatarUrl"] = QUrl();
    std::unique_ptr<QObject> tile(component.createWithInitialProperties(properties));
    QVERIFY2(tile != nullptr, qPrintable(component.errorString()));
    click(tile->findChild<QObject *>("roomTopActions"));
    QTRY_VERIFY(tile->property("menuOpen").toBool());
    auto *menu = tile->findChild<QQuickItem *>("roomControlMenu");
    QVERIFY(menu != nullptr);
    const QRectF bounds(menu->mapToScene(QPointF()), menu->size());
    QVERIFY2(QRectF(QPointF(), window.size()).contains(bounds), qPrintable(QStringLiteral("menu %1,%2 %3x%4").arg(bounds.x()).arg(bounds.y()).arg(bounds.width()).arg(bounds.height())));
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, QPoint(20, 100));
    QTRY_VERIFY(!tile->property("menuOpen").toBool());
}

void QmlInteractionTest::preservesPresetNameUntilSaveSucceeds()
{
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl("qrc:/qml/panels/WorkspacePresetsPanel.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    FakePresetController controller;
    QQuickWindow window;
    window.resize(480, 600);
    window.show();
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {"parent", QVariant::fromValue(window.contentItem())},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))}}));
    QVERIFY2(panel != nullptr, qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(panel.get(), "open"));
    auto *input = panel->findChild<QObject *>("presetNameInput");
    auto *save = panel->findChild<QObject *>("saveWorkspacePresetButton");
    QVERIFY(input && save);
    input->setProperty("text", QStringLiteral("晚间观看"));
    click(save);
    QCOMPARE(controller.saveCount, 1);
    QCOMPARE(controller.savedName, QStringLiteral("晚间观看"));
    QCOMPARE(input->property("text").toString(), QStringLiteral("晚间观看"));
    controller.saveResult = QStringLiteral("preset-created");
    click(save);
    QCOMPARE(controller.saveCount, 2);
    QCOMPARE(input->property("text").toString(), QString());
}

void QmlInteractionTest::hidesInactiveRoomControlsWithRetainedFocus()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl("qrc:/qml/components/RoomTile.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(640, 480);
    window.show();
    auto properties = roomTileProperties("connected");
    properties["parent"] = QVariant::fromValue(window.contentItem());
    properties["width"] = 320;
    properties["height"] = 180;
    properties["avatarUrl"] = QUrl();
    std::unique_ptr<QObject> tile(component.createWithInitialProperties(properties));
    QVERIFY2(tile != nullptr, qPrintable(component.errorString()));
    auto *timer = tile->findChild<QObject *>("roomControlsTimer");
    QVERIFY(timer);
    QCOMPARE(timer->property("interval").toInt(), 2200);
    timer->setProperty("interval", 100);
    auto *item = qobject_cast<QQuickItem *>(tile.get());
    item->forceActiveFocus(Qt::MouseFocusReason);
    QTRY_VERIFY(item->hasActiveFocus());
    QVERIFY(QMetaObject::invokeMethod(tile.get(), "revealControls"));
    QTest::mouseMove(&window, QPoint(500, 350));
    QTRY_VERIFY_WITH_TIMEOUT(!tile->property("controlsVisible").toBool(), 5000);
}

void QmlInteractionTest::hidesInitiallyVisibleRoomControlsWithoutPointerEntry()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl("qrc:/qml/components/RoomTile.qml"));
    QVERIFY(component.isReady());
    QQuickWindow window;
    window.resize(640, 480);
    window.show();
    QTest::mouseMove(&window, QPoint(620, 460));
    auto properties = roomTileProperties("connected");
    properties["parent"] = QVariant::fromValue(window.contentItem());
    properties["width"] = 320;
    properties["height"] = 180;
    properties["avatarUrl"] = QUrl();
    std::unique_ptr<QObject> tile(component.createWithInitialProperties(properties));
    QVERIFY(tile);
    auto *timer = tile->findChild<QObject *>("roomControlsTimer");
    QVERIFY(timer);
    QCOMPARE(timer->property("interval").toInt(), 2200);
    timer->setProperty("interval", 100);
    QTRY_VERIFY_WITH_TIMEOUT(!tile->property("controlsVisible").toBool(), 5000);
}

void QmlInteractionTest::hidesRoomControlsAfterMenuCloses()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl("qrc:/qml/components/RoomTile.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(640, 480);
    window.show();
    auto properties = roomTileProperties("connected");
    properties["parent"] = QVariant::fromValue(window.contentItem());
    properties["width"] = 320;
    properties["height"] = 180;
    properties["avatarUrl"] = QUrl();
    std::unique_ptr<QObject> tile(component.createWithInitialProperties(properties));
    QVERIFY2(tile != nullptr, qPrintable(component.errorString()));
    auto *timer = tile->findChild<QObject *>("roomControlsTimer");
    QVERIFY(timer);
    timer->setProperty("interval", 100);
    tile->setProperty("menuOpen", true);
    QVERIFY(QMetaObject::invokeMethod(tile.get(), "revealControls"));
    QTest::qWait(250);
    QVERIFY(tile->property("controlsVisible").toBool());
    tile->setProperty("menuOpen", false);
    QTRY_VERIFY_WITH_TIMEOUT(!tile->property("controlsVisible").toBool(), 5000);
}

void QmlInteractionTest::keepsControlsVisibleDuringQualityPopupInteraction()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl("qrc:/qml/components/RoomTile.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(640, 480);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.requestActivate();
    QTRY_VERIFY(window.isActive());
    auto properties = roomTileProperties("connected");
    properties["parent"] = QVariant::fromValue(window.contentItem());
    properties["width"] = 640;
    properties["height"] = 360;
    properties["avatarUrl"] = QUrl();
    std::unique_ptr<QObject> tile(component.createWithInitialProperties(properties));
    QVERIFY(tile);
    auto *timer = tile->findChild<QObject *>("roomControlsTimer");
    QVERIFY(timer);
    timer->setProperty("interval", 100);
    auto *quality = tile->findChild<QObject *>("roomQualitySelector");
    QVERIFY(quality);
    QObject *popup = quality->property("popup").value<QObject *>();
    QVERIFY(popup);
    QCOMPARE(popup, tile->findChild<QObject *>("roomQualityPopup"));
    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("visible").toBool());
    QVERIFY(QMetaObject::invokeMethod(tile.get(), "revealControls"));
    QTest::qWait(250);
    const QString state = QStringLiteral("popup=%1 transient=%2 timer=%3 active=%4")
                              .arg(popup->property("visible").toBool())
                              .arg(tile->property("hasTransientControls").toBool())
                              .arg(timer->property("running").toBool())
                              .arg(window.isActive());
    QVERIFY2(tile->property("controlsVisible").toBool(), qPrintable(state));
    QVERIFY(QMetaObject::invokeMethod(popup, "close"));
    QTest::mouseMove(&window, QPoint(620, 460));
    QTRY_VERIFY_WITH_TIMEOUT(!tile->property("controlsVisible").toBool(), 5000);
}

void QmlInteractionTest::switchesRoomQualityByStreamRate()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomTile.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    FakeRoomController controller;
    QVariantMap properties = roomTileProperties(QStringLiteral("connected"));
    properties[QStringLiteral("requestedQualityRate")] = 3;
    properties[QStringLiteral("controller")] = QVariant::fromValue(static_cast<QObject *>(&controller));
    std::unique_ptr<QObject> tile(component.createWithInitialProperties(properties));
    QVERIFY2(tile != nullptr, qPrintable(component.errorString()));

    QObject *quality = tile->findChild<QObject *>(QStringLiteral("roomQualitySelector"));
    QVERIFY(quality != nullptr);
    QCOMPARE(quality->property("count").toInt(), 2);
    QCOMPARE(quality->property("currentIndex").toInt(), 1);
    QVERIFY(quality->property("width").toDouble() >= 82.0);
    QObject *qualityLabel = tile->findChild<QObject *>(QStringLiteral("roomQualityLabel"));
    QVERIFY(qualityLabel != nullptr);
    QCOMPARE(qualityLabel->property("text").toString(), QStringLiteral("高清"));
    QCOMPARE(qualityLabel->property("elide").toInt(), int(Qt::ElideNone));
    QVERIFY(QMetaObject::invokeMethod(quality, "activated", Q_ARG(int, 0)));
    QCOMPARE(controller.lastQualityRoom, QStringLiteral("63136"));
    QCOMPARE(controller.lastQualityRate, 4);
    QCOMPARE(controller.lastQuality, 0);
}

void QmlInteractionTest::keepsQualityLabelGeometryWhileHoveringSelector()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomTile.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QQuickWindow hostWindow;
    hostWindow.resize(QSize(640, 480));
    hostWindow.show();

    FakeRoomController controller;
    QVariantMap properties = roomTileProperties(QStringLiteral("connected"));
    properties[QStringLiteral("controller")] = QVariant::fromValue(static_cast<QObject *>(&controller));
    properties[QStringLiteral("parent")] = QVariant::fromValue(hostWindow.contentItem());
    properties[QStringLiteral("width")] = 640;
    properties[QStringLiteral("height")] = 360;
    std::unique_ptr<QObject> tile(component.createWithInitialProperties(properties));
    QVERIFY2(tile != nullptr, qPrintable(component.errorString()));

    auto *quality = qobject_cast<QQuickItem *>(
        tile->findChild<QObject *>(QStringLiteral("roomQualitySelector")));
    auto *qualityLabel = qobject_cast<QQuickItem *>(
        tile->findChild<QObject *>(QStringLiteral("roomQualityLabel")));
    QVERIFY(quality != nullptr);
    QVERIFY(qualityLabel != nullptr);

    const auto labelCenter = [qualityLabel]() {
        return qualityLabel->mapToScene(QPointF(qualityLabel->width() / 2.0,
                                                qualityLabel->height() / 2.0));
    };
    const QPointF initialCenter = labelCenter();

    const QPoint bottomCenter = quality->mapToScene(
        QPointF(quality->width() / 2.0, quality->height() - 1.0)).toPoint();
    QTest::mouseMove(&hostWindow, bottomCenter);
    QTest::qWait(50);

    const QPointF hoveredCenter = labelCenter();
    QVERIFY(qAbs(hoveredCenter.x() - initialCenter.x()) <= 0.1);
    QVERIFY(qAbs(hoveredCenter.y() - initialCenter.y()) <= 0.1);
}

void QmlInteractionTest::rendersQualityPopupOptions()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomTile.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QQuickWindow hostWindow;
    hostWindow.resize(QSize(640, 480));
    hostWindow.show();

    FakeRoomController controller;
    QVariantMap properties = roomTileProperties(QStringLiteral("connected"));
    properties[QStringLiteral("controller")] = QVariant::fromValue(static_cast<QObject *>(&controller));
    properties[QStringLiteral("parent")] = QVariant::fromValue(hostWindow.contentItem());
    properties[QStringLiteral("width")] = 640;
    properties[QStringLiteral("height")] = 360;
    std::unique_ptr<QObject> tile(component.createWithInitialProperties(properties));
    QVERIFY2(tile != nullptr, qPrintable(component.errorString()));

    QObject *quality = tile->findChild<QObject *>(QStringLiteral("roomQualitySelector"));
    QVERIFY(quality != nullptr);
    QObject *popup = quality->property("popup").value<QObject *>();
    QVERIFY(popup != nullptr);
    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY_WITH_TIMEOUT(popup->property("visible").toBool(), 1000);
    auto *popupContent = qobject_cast<QQuickItem *>(popup->property("contentItem").value<QObject *>());
    QVERIFY(popupContent != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(popupContent->property("count").toInt(), 2, 1000);

    QQuickItem *automaticOption = nullptr;
    QVERIFY(QMetaObject::invokeMethod(popupContent,
                                      "itemAtIndex",
                                      Q_RETURN_ARG(QQuickItem *, automaticOption),
                                      Q_ARG(int, 0)));
    QQuickItem *highDefinitionOption = nullptr;
    QVERIFY(QMetaObject::invokeMethod(popupContent,
                                      "itemAtIndex",
                                      Q_RETURN_ARG(QQuickItem *, highDefinitionOption),
                                      Q_ARG(int, 1)));
    QVERIFY(automaticOption != nullptr);
    QVERIFY(highDefinitionOption != nullptr);

    QQuickItem *automaticLabel =
        quickItemByObjectName(automaticOption, QStringLiteral("roomQualityOptionLabel"));
    QQuickItem *highDefinitionLabel =
        quickItemByObjectName(highDefinitionOption, QStringLiteral("roomQualityOptionLabel"));
    QVERIFY(automaticLabel != nullptr);
    QVERIFY(highDefinitionLabel != nullptr);
    QCOMPARE(automaticLabel->property("text").toString(), QStringLiteral("自动"));
    QCOMPARE(highDefinitionLabel->property("text").toString(), QStringLiteral("高清"));

    const auto labelCenter = [](QQuickItem *label) {
        return label->mapToScene(QPointF(label->width() / 2.0, label->height() / 2.0));
    };
    const QPointF initialCenter = labelCenter(automaticLabel);
    const qreal initialWidth = automaticLabel->width();
    const QPoint bottomCenter = automaticOption->mapToScene(
        QPointF(automaticOption->width() / 2.0, automaticOption->height() - 1.0)).toPoint();
    QTest::mouseMove(&hostWindow, bottomCenter);
    QTest::qWait(50);

    const QPointF hoveredCenter = labelCenter(automaticLabel);
    QVERIFY(qAbs(hoveredCenter.x() - initialCenter.x()) <= 0.1);
    QVERIFY(qAbs(hoveredCenter.y() - initialCenter.y()) <= 0.1);
    QVERIFY(qAbs(automaticLabel->width() - initialWidth) <= 0.1);
}

void QmlInteractionTest::rendersAvailableQualitiesFromModelRole()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/WorkspaceGrid.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QQuickWindow hostWindow;
    hostWindow.resize(QSize(1280, 720));
    hostWindow.show();

    const QVariantList rooms{
        QVariantMap{
            {QStringLiteral("roomId"), QStringLiteral("63136")},
            {QStringLiteral("anchorName"), QStringLiteral("主播")},
            {QStringLiteral("title"), QStringLiteral("标题")},
            {QStringLiteral("category"), QStringLiteral("游戏")},
            {QStringLiteral("viewerLabel"), QStringLiteral("1.2万")},
            {QStringLiteral("avatarUrl"), QUrl()},
            {QStringLiteral("liveState"), QStringLiteral("online")},
            {QStringLiteral("playbackState"), QStringLiteral("playing")},
            {QStringLiteral("primary"), true},
            {QStringLiteral("favorite"), false},
            {QStringLiteral("audioFocused"), false},
            {QStringLiteral("requestedQuality"), QStringLiteral("high")},
            {QStringLiteral("requestedQualityRate"), 3},
            {QStringLiteral("effectiveQuality"), QStringLiteral("high")},
            {QStringLiteral("availableQualities"), QVariantList{
                 QVariantMap{{QStringLiteral("id"), QStringLiteral("auto")},
                             {QStringLiteral("label"), QStringLiteral("自动")},
                             {QStringLiteral("rate"), 4}},
                 QVariantMap{{QStringLiteral("id"), QStringLiteral("rate-3")},
                             {QStringLiteral("label"), QStringLiteral("高清")},
                             {QStringLiteral("rate"), 3}},
             }},
            {QStringLiteral("muted"), false},
            {QStringLiteral("volume"), 100},
            {QStringLiteral("danmakuEnabled"), false},
            {QStringLiteral("danmakuState"), QStringLiteral("idle")},
            {QStringLiteral("danmakuErrorCode"), QStringLiteral("NONE")},
            {QStringLiteral("renderEnabled"), true},
        },
    };

    std::unique_ptr<QObject> grid(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("width"), 1280},
        {QStringLiteral("height"), 720},
        {QStringLiteral("roomModel"), rooms},
        {QStringLiteral("layoutMode"), QStringLiteral("auto")},
        {QStringLiteral("primaryRoomId"), QStringLiteral("63136")},
    }));
    QVERIFY2(grid != nullptr, qPrintable(component.errorString()));

    auto *surface = grid->findChild<QQuickItem *>(QStringLiteral("layoutSurface"));
    QVERIFY(surface != nullptr);
    const QList<QQuickItem *> tiles = roomTiles(surface);
    QCOMPARE(tiles.size(), 1);

    QObject *quality = tiles.constFirst()->findChild<QObject *>(QStringLiteral("roomQualitySelector"));
    QVERIFY(quality != nullptr);
    QCOMPARE(quality->property("count").toInt(), 2);
    QCOMPARE(quality->property("currentIndex").toInt(), 1);
}
void QmlInteractionTest::defersRoomRemovalUntilAfterQmlHandlerReturns()
{
    registerQmlTypes();
    WorkspaceModel workspace(nullptr);
    FakeRoomController controller;
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomSidebar.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    RoomListModel roomModel;
    RoomSnapshot snapshot;
    snapshot.roomId = QStringLiteral("63136");
    snapshot.metadata.roomId = snapshot.roomId;
    roomModel.applySnapshots({snapshot});

    std::unique_ptr<QObject> sidebar(component.createWithInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("workspaceModel"), QVariant::fromValue(static_cast<QObject *>(&workspace))},
        {QStringLiteral("roomModel"), QVariant::fromValue(static_cast<QObject *>(&roomModel))},
    }));
    QVERIFY2(sidebar != nullptr, qPrintable(component.errorString()));

    QVERIFY(QMetaObject::invokeMethod(sidebar.get(),
                                      "requestRoomRemoval",
                                      Q_ARG(QVariant, QStringLiteral("63136"))));
    QCOMPARE(controller.removedRoom, QStringLiteral("63136"));
}

void QmlInteractionTest::rebindsPlayerWhenRoomIdentityChanges()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomTile.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    FakeRoomController controller;
    QVariantMap properties = roomTileProperties(QStringLiteral("connected"));
    properties[QStringLiteral("controller")] = QVariant::fromValue(static_cast<QObject *>(&controller));
    std::unique_ptr<QObject> tile(component.createWithInitialProperties(properties));
    QVERIFY2(tile != nullptr, qPrintable(component.errorString()));
    const QStringList initialAttachment{QStringLiteral("63136")};
    QTRY_COMPARE(controller.attachedRooms, initialAttachment);

    tile->setProperty("roomId", QStringLiteral("63137"));
    const QStringList expectedDetach{QStringLiteral("63136")};
    const QStringList expectedAttachments{QStringLiteral("63136"), QStringLiteral("63137")};
    QTRY_COMPARE(controller.detachedRooms, expectedDetach);
    QTRY_COMPARE(controller.attachedRooms, expectedAttachments);
}

void QmlInteractionTest::rendersFallbackMetadataAndUnknownStatus()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomTile.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QVariantMap properties = roomTileProperties(QStringLiteral("idle"));
    properties[QStringLiteral("anchorName")] = QString();
    properties[QStringLiteral("title")] = QString();
    properties[QStringLiteral("category")] = QString();
    properties[QStringLiteral("viewerLabel")] = QString();
    properties[QStringLiteral("liveState")] = QStringLiteral("unknown");
    properties[QStringLiteral("playbackState")] = QStringLiteral("idle");
    std::unique_ptr<QObject> tile(component.createWithInitialProperties(properties));
    QVERIFY2(tile != nullptr, qPrintable(component.errorString()));

    QObject *anchor = tile->findChild<QObject *>(QStringLiteral("roomAnchorName"));
    QObject *title = tile->findChild<QObject *>(QStringLiteral("roomTitle"));
    QObject *status = tile->findChild<QObject *>(QStringLiteral("roomStatusLabel"));
    QObject *viewer = tile->findChild<QObject *>(QStringLiteral("roomViewerLabel"));
    QVERIFY(anchor != nullptr);
    QVERIFY(title != nullptr);
    QVERIFY(viewer != nullptr);
    QCOMPARE(viewer->property("text").toString(), QStringLiteral("热度 --"));
    QVERIFY(status != nullptr);
    QCOMPARE(anchor->property("text").toString(), QStringLiteral("63136"));
    QCOMPARE(title->property("text").toString(), QStringLiteral("斗鱼直播间"));
    QCOMPARE(status->property("text").toString(), QStringLiteral("检查中"));
}

void QmlInteractionTest::exposesRoomOrderingControls()
{
    registerQmlTypes();
    WorkspaceModel workspace(nullptr);
    FakeRoomController controller;
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomSidebar.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QQuickWindow hostWindow;
    hostWindow.resize(QSize(360, 640));
    hostWindow.show();
    RoomListModel roomModel;
    RoomSnapshot snapshot;
    snapshot.roomId = QStringLiteral("63136");
    snapshot.metadata.roomId = snapshot.roomId;
    snapshot.metadata.anchorName = QStringLiteral("主播");
    snapshot.metadata.title = QStringLiteral("标题");
    snapshot.liveStatus = RoomLiveStatus::Online;
    snapshot.playbackHealth = RoomPlaybackHealth::Playing;
    roomModel.applySnapshots({snapshot});
    std::unique_ptr<QObject> sidebar(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("width"), 360},
        {QStringLiteral("height"), 640},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("workspaceModel"), QVariant::fromValue(static_cast<QObject *>(&workspace))},
        {QStringLiteral("roomModel"), QVariant::fromValue(static_cast<QObject *>(&roomModel))},
    }));
    QVERIFY2(sidebar != nullptr, qPrintable(component.errorString()));

    QObject *list = sidebar->findChild<QObject *>(QStringLiteral("roomList"));
    QVERIFY(list != nullptr);
    QTRY_COMPARE(list->property("count").toInt(), 1);
    QQuickItem *rowItem = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(QMetaObject::invokeMethod(list,
                                                       "itemAtIndex",
                                                       Q_RETURN_ARG(QQuickItem *, rowItem),
                                                       Q_ARG(int, 0))
                                 && rowItem != nullptr,
                             1000);
    QObject *up = rowItem->findChild<QObject *>(QStringLiteral("moveRoomUpButton"));
    QObject *down = rowItem->findChild<QObject *>(QStringLiteral("moveRoomDownButton"));
    QVERIFY(up != nullptr);
    QVERIFY(down != nullptr);
    QVERIFY(QMetaObject::invokeMethod(up, "clicked"));
    QCOMPARE(controller.movedRoom, QStringLiteral("63136"));
    QCOMPARE(controller.movedDelta, -1);
    QVERIFY(QMetaObject::invokeMethod(down, "clicked"));
    QCOMPARE(controller.movedDelta, 1);
}

void QmlInteractionTest::disablesOrderingAtListBoundaries()
{
    registerQmlTypes();
    WorkspaceModel workspace(nullptr);
    RoomListModel roomModel;
    RoomSnapshot first;
    first.roomId = QStringLiteral("63136");
    first.metadata.roomId = first.roomId;
    first.metadata.anchorName = QStringLiteral("主播 1");
    first.liveStatus = RoomLiveStatus::Online;
    RoomSnapshot second = first;
    second.roomId = QStringLiteral("63137");
    second.metadata.roomId = second.roomId;
    second.metadata.anchorName = QStringLiteral("主播 2");
    roomModel.applySnapshots({first, second});

    FakeRoomController controller;
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomSidebar.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(360, 640));
    hostWindow.show();
    std::unique_ptr<QObject> sidebar(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("width"), 360},
        {QStringLiteral("height"), 640},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("workspaceModel"), QVariant::fromValue(static_cast<QObject *>(&workspace))},
        {QStringLiteral("roomModel"), QVariant::fromValue(static_cast<QObject *>(&roomModel))},
    }));
    QVERIFY2(sidebar != nullptr, qPrintable(component.errorString()));

    QObject *list = sidebar->findChild<QObject *>(QStringLiteral("roomList"));
    QVERIFY(list != nullptr);
    QTRY_COMPARE(list->property("count").toInt(), 2);
    QQuickItem *firstRow = nullptr;
    QQuickItem *lastRow = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(QMetaObject::invokeMethod(list, "itemAtIndex",
                                                       Q_RETURN_ARG(QQuickItem *, firstRow),
                                                       Q_ARG(int, 0))
                                 && firstRow != nullptr,
                             1000);
    QTRY_VERIFY_WITH_TIMEOUT(QMetaObject::invokeMethod(list, "itemAtIndex",
                                                       Q_RETURN_ARG(QQuickItem *, lastRow),
                                                       Q_ARG(int, 1))
                                 && lastRow != nullptr,
                             1000);
    QObject *firstUp = firstRow->findChild<QObject *>(QStringLiteral("moveRoomUpButton"));
    QObject *firstDown = firstRow->findChild<QObject *>(QStringLiteral("moveRoomDownButton"));
    QObject *lastUp = lastRow->findChild<QObject *>(QStringLiteral("moveRoomUpButton"));
    QObject *lastDown = lastRow->findChild<QObject *>(QStringLiteral("moveRoomDownButton"));
    QVERIFY(firstUp != nullptr);
    QVERIFY(firstDown != nullptr);
    QVERIFY(lastUp != nullptr);
    QVERIFY(lastDown != nullptr);
    QVERIFY(!firstUp->property("enabled").toBool());
    QVERIFY(firstDown->property("enabled").toBool());
    QVERIFY(lastUp->property("enabled").toBool());
    QVERIFY(!lastDown->property("enabled").toBool());
}

void QmlInteractionTest::keepsSidebarActionsWithinRoomRows()
{
    registerQmlTypes();
    WorkspaceModel workspace(nullptr);
    FakeRoomController controller;
    RoomListModel roomModel;
    RoomSnapshot snapshot;
    snapshot.roomId = QStringLiteral("63136");
    snapshot.metadata.roomId = snapshot.roomId;
    snapshot.metadata.anchorName = QStringLiteral("长").repeated(120);
    snapshot.metadata.title = QStringLiteral("标").repeated(120);
    snapshot.metadata.category = QStringLiteral("游戏");
    snapshot.liveStatus = RoomLiveStatus::Online;
    snapshot.favorite = false;
    roomModel.applySnapshots({snapshot});

    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomSidebar.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(268, 640);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    std::unique_ptr<QObject> sidebar(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(window.contentItem())},
        {QStringLiteral("width"), 268}, {QStringLiteral("height"), 640},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("workspaceModel"), QVariant::fromValue(static_cast<QObject *>(&workspace))},
        {QStringLiteral("roomModel"), QVariant::fromValue(static_cast<QObject *>(&roomModel))},
    }));
    QVERIFY2(sidebar != nullptr, qPrintable(component.errorString()));
    auto *list = sidebar->findChild<QQuickItem *>(QStringLiteral("roomList"));
    QVERIFY(list != nullptr);
    QTRY_COMPARE(list->property("count").toInt(), 1);
    QQuickItem *row = nullptr;
    QTRY_VERIFY(QMetaObject::invokeMethod(list, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, 0))
                && row != nullptr);
    auto *title = row->findChild<QQuickItem *>(QStringLiteral("sidebarRoomAnchorName"));
    auto *actions = row->findChild<QQuickItem *>(QStringLiteral("sidebarRoomActionBar"));
    auto *favorite = row->findChild<QQuickItem *>(QStringLiteral("roomFavoriteIcon"));
    QVERIFY(title && actions && favorite);
    QVERIFY(title->property("elide").toInt() == static_cast<int>(Qt::ElideRight));
    QVERIFY(title->mapToScene(QPointF(title->width(), 0)).x()
            <= actions->mapToScene(QPointF(0, 0)).x());
    QVERIFY(actions->height() >= 27);
    QVERIFY(actions->width() >= 5 * 24);
    QVERIFY(row->property("height").toReal() >= 60.0);
    QTRY_VERIFY(favorite->property("width").toInt() >= 14);
    QVERIFY(favorite->property("width").toInt() <= 16);
    const auto *surface = row->findChild<QObject *>(QStringLiteral("roomSidebarSelectionSurface"));
    QVERIFY(surface != nullptr);
    QVERIFY(surface->property("visible").toBool());
    const QVariant borderWidth = QQmlProperty::read(surface, "border.width");
    QVERIFY(borderWidth.isValid());
    QVERIFY(borderWidth.toReal() <= 1);
    auto *favoriteButton = row->findChild<QQuickItem *>(QStringLiteral("roomFavoriteButton"));
    QVERIFY(favoriteButton != nullptr);
    const QPoint favoriteCenter = favoriteButton->mapToScene(
        QPointF(favoriteButton->width() / 2, favoriteButton->height() / 2)).toPoint();
    QTest::mouseMove(&window, favoriteCenter);
    QTRY_VERIFY(favoriteButton->property("hovered").toBool());
    auto *tooltip = favoriteButton->findChild<QObject *>(QStringLiteral("consoleToolTip"));
    QVERIFY(tooltip != nullptr);
    QTest::qWait(650);
    QVERIFY(!tooltip->property("visible").toBool());
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, favoriteCenter);
    QCOMPARE(controller.favoriteRoom, QStringLiteral("63136"));
    QVERIFY(controller.favoriteValue);
    QCOMPARE(QQmlProperty::read(favoriteButton, "ToolTip.text", qmlContext(favoriteButton)).toString(),
             QStringLiteral("收藏"));
    QCOMPARE(warnings.count(), 0);
}

void QmlInteractionTest::groupsHeaderActionsWithStableTooltips()
{
    QQmlApplicationEngine engine;
    auto *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));
    window->requestActivate();
    QTRY_VERIFY(window->isActive());
    auto *header = window->findChild<QQuickItem *>("appHeader");
    auto *drag = header->findChild<QQuickItem *>("titleBarDragArea");
    auto *tools = header->findChild<QQuickItem *>("watchToolsButton");
    auto *controls = header->findChild<QQuickItem *>("windowControls");
    QVERIFY(header && drag && tools && controls);
    for (const int width : {960, 1280, 1600}) {
        window->resize(width, 800);
        QTRY_VERIFY(drag->width() > 0);
        QVERIFY(drag->mapToScene(QPointF(drag->width(), 0)).x() <= tools->mapToScene(QPointF()).x());
        QTRY_VERIFY(controls->mapToScene(QPointF(controls->width(), 0)).x() <= window->width());
    }
    for (const char *name : {"watchToolsButton", "themeToggleButton", "minimizeButton", "maximizeButton", "closeButton"}) {
        auto *button = header->findChild<QQuickItem *>(QString::fromLatin1(name));
        QVERIFY(button != nullptr);
        QVERIFY(!QQmlProperty::read(button, "ToolTip.text", qmlContext(button)).toString().isEmpty());
        QTest::mouseMove(window, button->mapToScene(button->boundingRect().center()).toPoint());
        QTRY_VERIFY(button->property("hovered").toBool());
        QTest::qWait(650);
        if (auto *tip = button->findChild<QObject *>("consoleToolTip"))
            QVERIFY(!tip->property("visible").toBool());
        QVERIFY(!QQmlProperty::read(button, "ToolTip.visible", qmlContext(button)).toBool());
    }
}

void QmlInteractionTest::doesNotExposeRoomDragAndDropSurface()
{
    registerQmlTypes();
    WorkspaceModel workspace(nullptr);
    RoomListModel roomModel;
    RoomSnapshot snapshot;
    snapshot.roomId = QStringLiteral("63136");
    snapshot.metadata.roomId = snapshot.roomId;
    snapshot.metadata.anchorName = QStringLiteral("主播");
    snapshot.liveStatus = RoomLiveStatus::Online;
    roomModel.applySnapshots({snapshot});
    FakeRoomController controller;
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomSidebar.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(360, 640));
    hostWindow.show();
    std::unique_ptr<QObject> sidebar(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("width"), 360},
        {QStringLiteral("height"), 640},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("workspaceModel"), QVariant::fromValue(static_cast<QObject *>(&workspace))},
        {QStringLiteral("roomModel"), QVariant::fromValue(static_cast<QObject *>(&roomModel))},
    }));
    QVERIFY2(sidebar != nullptr, qPrintable(component.errorString()));
    QObject *list = sidebar->findChild<QObject *>(QStringLiteral("roomList"));
    QVERIFY(list != nullptr);
    QTRY_COMPARE(list->property("count").toInt(), 1);
    QQuickItem *row = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(QMetaObject::invokeMethod(list, "itemAtIndex",
                                                       Q_RETURN_ARG(QQuickItem *, row),
                                                       Q_ARG(int, 0))
                                 && row != nullptr,
                             1000);
    QVERIFY(row->findChild<QObject *>(QStringLiteral("roomDragHandle")) == nullptr);
    QVERIFY(row->findChild<QObject *>(QStringLiteral("roomDropArea")) == nullptr);
}

void QmlInteractionTest::deletesWorkspacePresetFromPanel()
{
    registerQmlTypes();
    WorkspaceModel workspace(nullptr);
    NativeWorkspacePreset preset;
    preset.id = QStringLiteral("preset-1");
    preset.name = QStringLiteral("默认布局");
    workspace.setWorkspaceData({}, {}, {preset}, {});
    FakePresetController controller;
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/panels/WorkspacePresetsPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(480, 480));
    hostWindow.show();
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("workspaceModel"), QVariant::fromValue(static_cast<QObject *>(&workspace))},
    }));
    QVERIFY2(panel != nullptr, qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(panel.get(), "open"));
    QTRY_VERIFY(panel->property("visible").toBool());
    QObject *presetList = panel->findChild<QObject *>(QStringLiteral("workspacePresetList"));
    QVERIFY(presetList != nullptr);
    QTRY_COMPARE(presetList->property("count").toInt(), 1);
    QQuickItem *presetRow = nullptr;
    QTRY_VERIFY(QMetaObject::invokeMethod(presetList, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, presetRow), Q_ARG(int, 0)) && presetRow != nullptr);
    QObject *deleteButton = presetRow->findChild<QObject *>(QStringLiteral("deleteWorkspacePresetButton"));
    QVERIFY(deleteButton != nullptr);
    QVERIFY(QMetaObject::invokeMethod(deleteButton, "clicked"));
    QCOMPARE(controller.deletedPresetId, QStringLiteral("preset-1"));
}

void QmlInteractionTest::defersWorkspacePresetApplyUntilPopupHandlerReturns()
{
    registerQmlTypes();
    WorkspaceModel workspace(nullptr);
    NativeWorkspacePreset preset;
    preset.id = QStringLiteral("preset-1");
    preset.name = QStringLiteral("默认布局");
    workspace.setWorkspaceData({}, {}, {preset}, {});
    FakePresetController controller;
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/panels/WorkspacePresetsPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(480, 480));
    hostWindow.show();
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("workspaceModel"), QVariant::fromValue(static_cast<QObject *>(&workspace))},
    }));
    QVERIFY2(panel != nullptr, qPrintable(component.errorString()));
    QVERIFY(QMetaObject::invokeMethod(panel.get(), "open"));
    QTRY_VERIFY(panel->property("visible").toBool());
    QObject *presetList = panel->findChild<QObject *>(QStringLiteral("workspacePresetList"));
    QVERIFY(presetList != nullptr);
    QTRY_COMPARE(presetList->property("count").toInt(), 1);
    QQuickItem *presetRow = nullptr;
    QTRY_VERIFY(QMetaObject::invokeMethod(presetList, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, presetRow), Q_ARG(int, 0)) && presetRow != nullptr);
    QObject *applyButton = presetRow->findChild<QObject *>(QStringLiteral("applyWorkspacePresetButton"));
    QVERIFY(applyButton != nullptr);
    QVERIFY(QMetaObject::invokeMethod(applyButton, "clicked"));
    QCOMPARE(controller.appliedPresetId, QString());
    QTRY_COMPARE(controller.appliedPresetId, QStringLiteral("preset-1"));
}


void QmlInteractionTest::refreshesRoomSidebarAfterPresetLikeModelUpdate()
{
    registerQmlTypes();
    WorkspaceModel workspace(nullptr);
    RoomListModel roomModel;
    RoomSnapshot first;
    first.roomId = QStringLiteral("63136");
    first.metadata.roomId = first.roomId;
    first.metadata.anchorName = QStringLiteral("主播 1");
    first.liveStatus = RoomLiveStatus::Online;
    roomModel.applySnapshots({first});

    FakeRoomController controller;
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomSidebar.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(360, 640));
    hostWindow.show();
    std::unique_ptr<QObject> sidebar(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("width"), 360},
        {QStringLiteral("height"), 640},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("workspaceModel"), QVariant::fromValue(static_cast<QObject *>(&workspace))},
        {QStringLiteral("roomModel"), QVariant::fromValue(static_cast<QObject *>(&roomModel))},
    }));
    QVERIFY2(sidebar != nullptr, qPrintable(component.errorString()));
    QObject *list = sidebar->findChild<QObject *>(QStringLiteral("roomList"));
    QVERIFY(list != nullptr);
    QTRY_COMPARE(list->property("count").toInt(), 1);

    QVector<RoomSnapshot> fiveRooms;
    for (int i = 0; i < 5; ++i) {
        RoomSnapshot room = first;
        room.roomId = QStringLiteral("6313%1").arg(i + 6);
        room.metadata.roomId = room.roomId;
        room.metadata.anchorName = QStringLiteral("主播 %1").arg(i + 1);
        fiveRooms.push_back(room);
    }
    roomModel.applySnapshots(fiveRooms);
    QTRY_COMPARE(list->property("count").toInt(), 5);

    fiveRooms.removeFirst();
    roomModel.applySnapshots(fiveRooms);
    QTRY_COMPARE(list->property("count").toInt(), 4);
    QTRY_VERIFY(([&]() {
        QQuickItem *firstRow = nullptr;
        if (!QMetaObject::invokeMethod(list, "itemAtIndex",
                                       Q_RETURN_ARG(QQuickItem *, firstRow), Q_ARG(int, 0))) {
            return false;
        }
        return firstRow != nullptr
               && firstRow->property("roomId").toString() == QStringLiteral("63137");
    })());
}

void QmlInteractionTest::laysOutFiveAutomaticRoomsInThreeAndTwoRows()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qml/components/WorkspaceGrid.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QQuickWindow hostWindow;
    hostWindow.resize(QSize(1280, 720));
    hostWindow.show();

    QVariantList rooms;
    for (int i = 0; i < 5; ++i) {
        QVariantMap room = roomTileProperties(QStringLiteral("idle"));
        room[QStringLiteral("roomId")] = QStringLiteral("6313%1").arg(i + 6);
        rooms.push_back(room);
    }
    std::unique_ptr<QObject> grid(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("width"), 1280},
        {QStringLiteral("height"), 720},
        {QStringLiteral("roomModel"), rooms},
        {QStringLiteral("layoutMode"), QStringLiteral("auto")},
    }));
    QVERIFY2(grid != nullptr, qPrintable(component.errorString()));

    auto *surface = grid->findChild<QQuickItem *>(QStringLiteral("layoutSurface"));
    QVERIFY(surface != nullptr);
    const auto tiles = roomTiles(surface);
    QCOMPARE(tiles.size(), 5);
    QCOMPARE(tiles.at(0)->y(), 0.0);
    QCOMPARE(tiles.at(1)->y(), 0.0);
    QCOMPARE(tiles.at(2)->y(), 0.0);
    QVERIFY(tiles.at(3)->y() > tiles.at(0)->y());
    QCOMPARE(tiles.at(3)->y(), tiles.at(4)->y());
    QVERIFY(tiles.at(3)->width() > tiles.at(0)->width());
    QVERIFY(qAbs((tiles.at(3)->x() + tiles.at(3)->width()) - tiles.at(4)->x()) < 0.1);
    QVERIFY(tiles.at(4)->x() > tiles.at(3)->x());
    QVERIFY(tiles.at(4)->x() + tiles.at(4)->width() <= surface->width() + 0.1);
}

void QmlInteractionTest::laysOutPrimaryRoomsAcrossFullHeight()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qml/components/WorkspaceGrid.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    QQuickWindow hostWindow;
    hostWindow.resize(QSize(1280, 720));
    hostWindow.show();

    for (const int count : {1, 2, 3, 4, 5, 6, 7, 8, 9, 12, 16, 24}) {
        QVariantList rooms;
        for (int i = 0; i < count; ++i) {
            QVariantMap room = roomTileProperties(QStringLiteral("idle"));
            room[QStringLiteral("roomId")] = QStringLiteral("6313%1").arg(i + 6);
            room[QStringLiteral("anchorName")] = QStringLiteral("主播 %1").arg(i + 1);
            rooms.push_back(room);
        }

        std::unique_ptr<QObject> grid(component.createWithInitialProperties({
            {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
            {QStringLiteral("width"), 1280},
            {QStringLiteral("height"), 720},
            {QStringLiteral("roomModel"), rooms},
            {QStringLiteral("layoutMode"), QStringLiteral("primary")},
            {QStringLiteral("primaryRoomId"), QStringLiteral("63136")},
        }));
        QVERIFY2(grid != nullptr, qPrintable(component.errorString()));

        auto *surface = grid->findChild<QQuickItem *>(QStringLiteral("layoutSurface"));
        QVERIFY(surface != nullptr);
        const auto tiles = roomTiles(surface);
        QCOMPARE(tiles.size(), count);
        QQuickItem *primary = nullptr;
        for (QQuickItem *tile : tiles) {
            if (tile->property("roomId").toString() == QStringLiteral("63136")) {
                primary = tile;
                break;
            }
        }
        QVERIFY(primary != nullptr);
        QCOMPARE(primary->y(), 0.0);
        QVERIFY(qAbs(primary->height() - surface->height()) < 0.1);

        if (count == 1) {
            QVERIFY(qAbs(primary->width() - surface->width()) < 0.1);
            continue;
        }

        if (count <= 4) {
            QCOMPARE(primary->x(), 0.0);
            QVERIFY(tiles.at(1)->x() >= primary->x() + primary->width() - 0.1);
            QCOMPARE(tiles.at(1)->x(), tiles.at(count - 1)->x());
            if (count > 2) QVERIFY(tiles.at(count - 1)->y() > tiles.at(1)->y());
        } else if (count <= 8) {
            const qreal sideWidth = surface->width() / 3.35;
            QVERIFY(primary->x() > 0.0);
            QVERIFY(primary->x() + primary->width() < surface->width());
            QVERIFY(qAbs(primary->x() - sideWidth) < 0.1);
            QVERIFY(qAbs(primary->width() - (surface->width() - sideWidth * 2.0)) < 0.1);
            const int leftCount = std::floor((count - 1) / 2.0);
            const int rightCount = count - 1 - leftCount;
            for (QQuickItem *tile : tiles) {
                if (tile == primary) continue;
                QVERIFY(tile->y() >= 0.0);
                if (tile->x() < primary->x()) {
                    QVERIFY(qAbs(tile->x()) < 0.1);
                } else {
                    QVERIFY(qAbs((tile->x() + tile->width()) - surface->width()) < 0.1);
                }
            }
            QVERIFY(leftCount > 0);
            QVERIFY(rightCount > 0);
        } else {
            QCOMPARE(primary->x(), 0.0);
            QVERIFY(qAbs(primary->width() - surface->width() * 0.44) < 0.1);
            for (QQuickItem *tile : tiles) {
                if (tile == primary) continue;
                QVERIFY(tile->x() >= primary->width() - 0.1);
                QVERIFY(tile->x() + tile->width() <= surface->width() + 0.1);
            }
        }
    }
}

void QmlInteractionTest::rendersGuildMemberAvatarAndLiveState()
{
    FakeGuildNavigationController controller;
    controller.roster = QVariantList{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("hamster-online")},
                    {QStringLiteral("anchorName"), QStringLiteral("在线主播")},
                    {QStringLiteral("pinyinKey"), QStringLiteral("Z")},
                    {QStringLiteral("roomId"), QStringLiteral("71415")},
                    {QStringLiteral("status"), QStringLiteral("resolved")},
                    {QStringLiteral("avatarUrl"), QStringLiteral("https://example.invalid/online.jpg")},
                    {QStringLiteral("liveState"), QStringLiteral("online")},
                    {QStringLiteral("active"), false}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("hamster-offline")},
                    {QStringLiteral("anchorName"), QStringLiteral("离线主播")},
                    {QStringLiteral("pinyinKey"), QStringLiteral("L")},
                    {QStringLiteral("roomId"), QStringLiteral("84452")},
                    {QStringLiteral("status"), QStringLiteral("resolved")},
                    {QStringLiteral("avatarUrl"), QString()},
                    {QStringLiteral("liveState"), QStringLiteral("offline")},
                    {QStringLiteral("active"), false}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("hamster-unknown")},
                    {QStringLiteral("anchorName"), QStringLiteral("检查主播")},
                    {QStringLiteral("pinyinKey"), QStringLiteral("J")},
                    {QStringLiteral("roomId"), QString()},
                    {QStringLiteral("status"), QStringLiteral("resolving")},
                    {QStringLiteral("liveState"), QStringLiteral("unknown")},
                    {QStringLiteral("active"), false}},
    };
    controller.teamItems = QVariantList{
        QVariantMap{{QStringLiteral("id"), QStringLiteral("team-a")},
                    {QStringLiteral("name"), QStringLiteral("一队")},
                    {QStringLiteral("memberIds"), QStringList{QStringLiteral("hamster-online"),
                                                              QStringLiteral("hamster-offline"),
                                                              QStringLiteral("hamster-unknown")}}},
    };

    QQmlApplicationEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qml/components/GuildNavigationPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(284, 720));
    hostWindow.show();
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("width"), 284},
        {QStringLiteral("height"), 720},
        {QStringLiteral("controller"),
         QVariant::fromValue(static_cast<QObject *>(&controller))},
    }));
    QVERIFY2(panel != nullptr, qPrintable(component.errorString()));

    QObject *visibleRows = panel->findChild<QObject *>(QStringLiteral("guildVisibleRows"));
    QVERIFY(visibleRows != nullptr);
    QTRY_COMPARE(visibleRows->property("count").toInt(), 3);

    const QList<QString> expectedStates{QStringLiteral("直播中"), QStringLiteral("未开播"),
                                      QStringLiteral("检查中")};
    const QList<QString> expectedAvatars{QStringLiteral("https://example.invalid/online.jpg"), QString(), QString()};
    for (int index = 0; index < expectedStates.size(); ++index) {
        QQuickItem *row = nullptr;
        QVERIFY(QMetaObject::invokeMethod(visibleRows, "itemAt",
                                          Q_RETURN_ARG(QQuickItem *, row),
                                          Q_ARG(int, index)));
        QVERIFY(row != nullptr);
        QObject *liveLabel =
            row->findChild<QObject *>(QStringLiteral("guildMemberLiveStateLabel"));
        QObject *avatarImage =
            row->findChild<QObject *>(QStringLiteral("guildMemberAvatarImage"));
        QVERIFY(liveLabel != nullptr);
        QVERIFY(avatarImage != nullptr);
        QCOMPARE(avatarImage->property("sourceSize").toSize(), QSize(64, 64));
        QVERIFY(avatarImage->property("asynchronous").toBool());
        QTRY_COMPARE(liveLabel->property("text").toString(), expectedStates.at(index));
        QTRY_COMPARE(avatarImage->property("source").toString(), expectedAvatars.at(index));
    }
}

void QmlInteractionTest::opensMaoziRankPageFromHeader()
{
    registerQmlTypes();
    FakeMaoziController controller;
    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("appController"),
         QVariant::fromValue(static_cast<QObject *>(&controller))},
    });
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    window->resize(QSize(1280, 720));
    window->show();

    QObject *rankButton = window->findChild<QObject *>(QStringLiteral("maoziRankButton"));
    QVERIFY(rankButton != nullptr);
    click(rankButton);

    QObject *page = window->findChild<QObject *>(QStringLiteral("maoziRankPage"));
    QVERIFY(page != nullptr);
    QTRY_VERIFY(page->property("visible").toBool());
    QObject *list = page->findChild<QObject *>(QStringLiteral("maoziRankList"));
    QVERIFY(list != nullptr);
    QTRY_COMPARE(list->property("count").toInt(), 2);
}

void QmlInteractionTest::filtersAndRefreshesMaoziRankPage()
{
    registerQmlTypes();
    FakeMaoziController controller;
    QQmlApplicationEngine engine;
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(1100, 720));
    hostWindow.show();

    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/pages/MaoziRankPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> page(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("width"), 1100},
        {QStringLiteral("height"), 720},
        {QStringLiteral("controller"),
         QVariant::fromValue(static_cast<QObject *>(&controller))},
    }));
    QVERIFY2(page != nullptr, qPrintable(component.errorString()));

    QObject *list = page->findChild<QObject *>(QStringLiteral("maoziRankList"));
    QVERIFY(list != nullptr);
    QTRY_COMPARE(list->property("count").toInt(), 2);

    QObject *placementTab = page->findChild<QObject *>(QStringLiteral("maoziPlacementTab"));
    QObject *playValueTab = page->findChild<QObject *>(QStringLiteral("maoziPlayValueTab"));
    QObject *placementHeader =
        page->findChild<QObject *>(QStringLiteral("maoziPlacementHeaderBar"));
    QObject *placementList =
        page->findChild<QObject *>(QStringLiteral("maoziPlacementList"));
    QObject *playValueList =
        page->findChild<QObject *>(QStringLiteral("maoziPlayValueList"));
    QVERIFY(placementTab != nullptr);
    const QStringList tabNames{QStringLiteral("maoziRankingTab"),
                               QStringLiteral("maoziRegularSeasonTab"),
                               QStringLiteral("maoziFinalsTab"),
                               QStringLiteral("maoziFootballTab"),
                               QStringLiteral("maoziPlacementTab")};
    for (const QString &name : tabNames) {
        QVERIFY2(page->findChild<QObject *>(name) != nullptr, qPrintable(name));
    }
    click(page->findChild<QObject *>(QStringLiteral("maoziRegularSeasonTab")));
    QObject *regularList = page->findChild<QObject *>(QStringLiteral("maoziRegularSeasonList"));
    QVERIFY(regularList != nullptr);
    QTRY_COMPARE(regularList->property("count").toInt(), 1);
    click(page->findChild<QObject *>(QStringLiteral("maoziFootballTab")));
    QObject *footballList = page->findChild<QObject *>(QStringLiteral("maoziFootballList"));
    QVERIFY(footballList != nullptr);
    QTRY_COMPARE(footballList->property("count").toInt(), 1);
    QVERIFY(playValueTab == nullptr);
    QVERIFY(placementHeader != nullptr);
    QVERIFY(placementList != nullptr);
    QVERIFY(playValueList == nullptr);

    click(placementTab);
    QTRY_VERIFY(placementHeader->property("visible").toBool());
    QTRY_VERIFY(placementList->property("visible").toBool());
    QTRY_COMPARE(placementList->property("count").toInt(), 1);

    QObject *search = page->findChild<QObject *>(QStringLiteral("maoziSearchField"));
    QVERIFY(search != nullptr);
    search->setProperty("text", QStringLiteral("阿飞"));
    QTRY_COMPARE(list->property("count").toInt(), 1);

    QObject *refresh = page->findChild<QObject *>(QStringLiteral("maoziRefreshButton"));
    QVERIFY(refresh != nullptr);
    QVERIFY(!refresh->property("enabled").toBool());
    QCOMPARE(controller.rank.refreshCount, 0);

    QObject *openSite = page->findChild<QObject *>(QStringLiteral("maoziOpenSiteButton"));
    QVERIFY(openSite != nullptr);
    click(openSite);
    QCOMPARE(controller.lastExternalUrl,
             QStringLiteral("https://cstg-zy.com/#/squad/s1"));
}

void QmlInteractionTest::updatesRankPortraitsWithoutChangingArchiveOrGuessingNames()
{
    FakeMaoziController controller;
    controller.useBundledArchive = true;
    controller.rank.setEntries({});
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    QQmlComponent component(&engine, QUrl("qrc:/qml/pages/MaoziRankPage.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(960, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    std::unique_ptr<QObject> page(component.createWithInitialProperties({
        {"parent", QVariant::fromValue(window.contentItem())}, {"width", 960}, {"height", 600},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))}}));
    QVERIFY(page);
    const QVariantMap archived = controller.s1Archive().value("ranking").toMap().value("participants").toList().first().toMap();
    const auto first = [&] { return qmlData(page.get(), "visibleEntries").toList().first().toMap(); };
    QCOMPARE(first().value("posterUrl").toString(), QString());
    const QString portrait = QStringLiteral("qrc:/qml/assets/brand/icon-app.png");
    controller.rank.setEntries({QVariantMap{{"id", archived.value("id")}, {"roomId", archived.value("roomId")},
        {"name", "更新名字"}, {"score", 99}, {"rank", 99}, {"posterUrl", portrait}}});
    QTRY_COMPARE(first().value("posterUrl").toString(), portrait);
    QCOMPARE(first().value("score"), archived.value("score"));
    QCOMPARE(first().value("name"), archived.value("name"));
    QCOMPARE(first().value("rank").toInt(), 1);
    QVERIFY(page->setProperty("selectedRankIndex", 0));
    auto *image = page->findChild<QObject *>("maoziRankDetailAvatar");
    QVERIFY(image);
    QTRY_COMPARE(image->property("status").toInt(), 1);
    QCOMPARE(image->property("source").toUrl(), QUrl(portrait));
    controller.rank.setEntries({});
    const QString refreshed = QStringLiteral("qrc:/qml/assets/brand/icon-ranking.png");
    controller.setRoster({QVariantMap{{"rankHostId", archived.value("id")}, {"roomId", archived.value("roomId")}, {"avatarUrl", refreshed}}});
    QTRY_COMPARE(image->property("source").toUrl(), QUrl(refreshed));
    QTRY_COMPARE(image->property("status").toInt(), 1);
    QCOMPARE(first().value("score"), archived.value("score"));
    controller.setRoster({});
    controller.rank.setEntries({QVariantMap{{"id", "other"}, {"roomId", "999"}, {"name", archived.value("name")}, {"posterUrl", portrait}}});
    QTRY_COMPARE(first().value("posterUrl").toString(), QString());
    QCOMPARE(first().value("score"), archived.value("score"));
    controller.rank.setEntries({QVariantMap{{"id", "other"}, {"roomId", archived.value("roomId")}, {"posterUrl", portrait}}});
    QTRY_COMPARE(first().value("posterUrl").toString(), portrait);
    controller.rank.setEntries({});
    controller.setRoster({QVariantMap{{"roomId", "new-room"}, {"portraitRoomIds", QStringList{archived.value("roomId").toString()}}, {"avatarUrl", refreshed}}});
    QTRY_COMPARE(first().value("posterUrl").toString(), refreshed);
    controller.setRoster({QVariantMap{{"roomId", "unrelated-manual-room"}, {"rankHostId", archived.value("id")},
        {"portraitRoomIds", QStringList{archived.value("roomId").toString()}}, {"avatarUrl", refreshed},
        {"portraitIdentityVerified", false}}});
    QTRY_COMPARE(first().value("posterUrl").toString(), QString());
    QCOMPARE(first().value("score"), archived.value("score"));
    QCOMPARE(warnings.count(), 0);
}

void QmlInteractionTest::retriesFailedRankPortraitsAndResetsForNewSources()
{
    FakeMaoziController controller;
    controller.useBundledArchive = true;
    const auto archived = controller.s1Archive().value("ranking").toMap().value("participants").toList().first().toMap();
    const QString missing = QStringLiteral("qrc:/missing-online-portrait.png");
    const QString resolverMissing = QStringLiteral("qrc:/missing-resolver-portrait.png");
    const QString fallback = QStringLiteral("qrc:/qml/assets/brand/icon-app.png");
    controller.rank.setEntries({QVariantMap{{"id", archived.value("id")}, {"roomId", archived.value("roomId")}, {"posterUrl", missing}}});
    controller.setRoster({QVariantMap{{"rankHostId", archived.value("id")}, {"roomId", archived.value("roomId")},
        {"avatarUrl", resolverMissing}, {"fallbackAvatarUrl", fallback}}});
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    QQmlComponent component(&engine, QUrl("qrc:/qml/pages/MaoziRankPage.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(960, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    std::unique_ptr<QObject> page(component.createWithInitialProperties({
        {"parent", QVariant::fromValue(window.contentItem())}, {"width", 960}, {"height", 600},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))}}));
    QVERIFY(page);
    QVERIFY(page->setProperty("selectedRankIndex", 0));
    auto *detail = page->findChild<QObject *>("maoziRankDetailAvatar");
    QVERIFY(detail);
    QTRY_COMPARE(detail->property("status").toInt(), 1);
    QCOMPARE(detail->property("source").toUrl(), QUrl(fallback));
    QCOMPARE(detail->property("candidateIndex").toInt(), 2);
    QSignalSpy sourceChanges(detail, SIGNAL(sourceChanged(const QUrl &)));
    QVERIFY(sourceChanges.isValid());
    controller.setRoster({QVariantMap{{"rankHostId", archived.value("id")}, {"roomId", archived.value("roomId")},
        {"avatarUrl", resolverMissing}, {"fallbackAvatarUrl", fallback}, {"liveState", "online"}}});
    QTest::qWait(150);
    QCOMPARE(detail->property("candidateIndex").toInt(), 2);
    QCOMPARE(sourceChanges.count(), 0);
    const auto entries = qmlData(page.get(), "visibleEntries").toList();
    QCOMPARE(entries.first().toMap().value("score"), archived.value("score"));

    const QString refreshed = QStringLiteral("qrc:/qml/assets/brand/icon-ranking.png");
    controller.rank.setEntries({QVariantMap{{"id", archived.value("id")}, {"roomId", archived.value("roomId")}, {"posterUrl", refreshed}}});
    QTRY_COMPARE(detail->property("source").toUrl(), QUrl(refreshed));
    QTRY_COMPARE(detail->property("status").toInt(), 1);
    QCOMPARE(detail->property("candidateIndex").toInt(), 0);

    controller.setRoster({});
    controller.rank.setEntries({QVariantMap{{"id", archived.value("id")}, {"posterUrl", missing}}});
    QTRY_COMPARE(detail->property("status").toInt(), 3);
    QCOMPARE(detail->property("candidateIndex").toInt(), 0);
    QTest::qWait(150);
    QCOMPARE(detail->property("status").toInt(), 3);
    for (const auto &event : warnings) {
        for (const auto &warning : event.first().value<QList<QQmlError>>()) {
            QVERIFY2(warning.description().contains("missing-online-portrait.png")
                         || warning.description().contains("missing-resolver-portrait.png"),
                     qPrintable(warning.toString()));
        }
    }
}

void QmlInteractionTest::preservesMissingRadarValuesAndValidZero()
{
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    QQmlComponent component(&engine, QUrl("qrc:/qml/components/RadarChart.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    const QVariantList dimensions{QVariantMap{{"name", "A"}, {"value", "0"}},
        QVariantMap{{"name", "B"}, {"average", 14}}, QVariantMap{{"name", "C"}, {"average", -1}},
        QVariantMap{{"name", "D"}, {"value", QVariant()}}, QVariantMap{{"name", "E"}, {"value", ""}}};
    std::unique_ptr<QObject> radar(component.createWithInitialProperties({{"dimensions", dimensions}, {"width", 340}, {"height", 300}}));
    QVERIFY(radar);
    QCOMPARE(radar->property("validValueCount").toInt(), 2);
    const auto normalized = qmlData(radar.get(), "normalizedDimensions").toList();
    QCOMPARE(normalized.first().toMap().value("value").toDouble(), 0.0);
    for (int i = 2; i < normalized.size(); ++i) QVERIFY(normalized[i].toMap().value("value").isNull());
    QVERIFY(!radar->property("completeData").toBool());
    const auto archive = S1Archive::bundled().value("ranking").toMap().value("participants").toList();
    QVERIFY(radar->setProperty("dimensions", archive.first().toMap().value("dimensions")));
    QTRY_COMPARE(radar->property("validValueCount").toInt(), 9);
    QVERIFY(radar->property("completeData").toBool());
    QCOMPARE(warnings.count(), 0);
}

void QmlInteractionTest::keepsAllStructuredArchiveDetailsAndReturnContext()
{
    FakeMaoziController controller;
    controller.useBundledArchive = true;
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    QQmlComponent component(&engine, QUrl("qrc:/qml/pages/MaoziRankPage.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    QQuickWindow window;
    window.resize(960, 600);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    std::unique_ptr<QObject> page(component.createWithInitialProperties({
        {"parent", QVariant::fromValue(window.contentItem())}, {"width", 960}, {"height", 600},
        {"controller", QVariant::fromValue(static_cast<QObject *>(&controller))}}));
    QVERIFY(page);
    const struct { const char *tab; const char *panel; const char *archive; bool football; } cases[] = {
        {"maoziRegularSeasonTab", "regularSeasonPanel", "regularSeason", false},
        {"maoziFinalsTab", "finalsPanel", "finals", false},
        {"maoziFootballTab", "footballPanel", "football", true}};
    int visited = 0;
    for (const auto &entry : cases) {
        click(page->findChild<QObject *>(QString::fromLatin1(entry.tab)));
        auto *panel = page->findChild<QQuickItem *>(QString::fromLatin1(entry.panel));
        QVERIFY(panel);
        const auto matches = controller.s1Archive().value(QString::fromLatin1(entry.archive)).toMap().value("matches").toList();
        QCOMPARE(qmlData(panel, "visibleMatches").toList().size(), matches.size());
        for (int i = 0; i < matches.size(); ++i) {
            QVERIFY(panel->setProperty("pageIndex", i / 10));
            QVERIFY(panel->setProperty("selectedIndex", i));
            QQuickItem *anchor = nullptr;
            QTRY_VERIFY((anchor = quickItemByObjectName(panel, entry.football ? QStringLiteral("footballScoreText") : QStringLiteral("seasonMatchOutcome"))) != nullptr);
            auto *detail = archiveDetailRoot(anchor);
            QVERIFY(detail);
            const QVariantMap source = matches[i].toMap();
            QTRY_COMPARE(qmlData(detail, "matchRecord").toMap().value("id"), source.value("id"));
            const QVariantMap record = qmlData(detail, "record").toMap();
            const QString raw = record.value("raw").toString();
            QVERIFY(!raw.isEmpty());
            if (!entry.football) {
                QCOMPARE(raw, source.value("detail").toString());
                if (i == 0) {
                    const auto standings = record.value("standings").toList();
                    QCOMPARE(standings.size(), 4);
                    QCOMPARE(standings.first().toMap().value("value").toString(),
                             QString::fromLatin1(entry.archive) == "finals" ? QStringLiteral("26分") : QStringLiteral("6分"));
                    QCOMPARE(record.value("rounds").toList().size(), QString::fromLatin1(entry.archive) == "finals" ? 9 : 6);
                }
            } else {
                QVERIFY(raw.contains(source.value("summary").toString()));
                for (const auto &section : source.value("detail").toMap().value("sections").toList())
                    QVERIFY(raw.contains(section.toMap().value("text").toString()));
                if (i == 0) {
                    QCOMPARE(record.value("score").toString(), QStringLiteral("5 : 1"));
                    QCOMPARE(record.value("events").toList().size(), 31);
                    QCOMPARE(record.value("votes").toList().size(), 18);
                    QCOMPARE(quickItemsByObjectName(panel, QStringLiteral("footballLineupPlayer")).size(), 8);
                }
            }
            click(panel->findChild<QObject *>("seasonArchiveBack"));
            QTRY_COMPARE(panel->property("selectedIndex").toInt(), -1);
            QCOMPARE(panel->property("pageIndex").toInt(), i / 10);
            ++visited;
        }
    }
    QCOMPARE(visited, 45);
    click(page->findChild<QObject *>("maoziRegularSeasonTab"));
    auto *regular = page->findChild<QQuickItem *>("regularSeasonPanel");
    auto *search = page->findChild<QObject *>("maoziSearchField");
    QVERIFY(regular && search);
    QVERIFY(search->setProperty("text", QStringLiteral("拳击")));
    QTRY_VERIFY(qmlData(regular, "visibleMatches").toList().size() > 0);
    QVERIFY(regular->setProperty("selectedIndex", 0));
    click(regular->findChild<QObject *>("seasonArchiveBack"));
    QTRY_COMPARE(regular->property("selectedIndex").toInt(), -1);
    QCOMPARE(search->property("text").toString(), QStringLiteral("拳击"));
    QCOMPARE(page->property("activeTab").toString(), QStringLiteral("regularSeason"));
    QCOMPARE(warnings.count(), 0);
}

void QmlInteractionTest::keepsVideoControlsDarkAndQualityKeyboardUsableAcrossThemes()
{
    registerQmlTypes();
    FakeRoomController controller;
    QQmlApplicationEngine engine;
    auto *window = loadWindow(engine);
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QVERIFY(window->setProperty("reducedMotion", true));
    QQmlComponent component(&engine, QUrl("qrc:/qml/components/RoomTile.qml"));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    auto properties = roomTileProperties(QStringLiteral("connected"));
    properties["controller"] = QVariant::fromValue(static_cast<QObject *>(&controller));
    properties["parent"] = QVariant::fromValue(window->contentItem());
    properties["width"] = 640;
    properties["height"] = 420;
    properties["z"] = 1000;
    properties["avatarUrl"] = QUrl();
    properties["renderEnabled"] = false;
    std::unique_ptr<QObject> tile(component.createWithInitialProperties(properties));
    QVERIFY(tile);
    auto *quality = tile->findChild<QQuickItem *>("roomQualitySelector");
    auto *label = tile->findChild<QQuickItem *>("roomQualityLabel");
    auto *timer = tile->findChild<QObject *>("roomControlsTimer");
    QVERIFY(quality && label && timer);
    QVERIFY(tile->setProperty("controlsVisible", true));
    QVERIFY(QMetaObject::invokeMethod(timer, "stop"));
    const auto dark = [](const QColor &color) { return color.isValid() && 0.2126 * color.redF() + 0.7152 * color.greenF() + 0.0722 * color.blueF() < 0.3; };
    for (const QString &mode : {QStringLiteral("day"), QStringLiteral("night"), QStringLiteral("day")}) {
        click(window->findChild<QObject *>("themeToggleButton"));
        QTRY_COMPARE(window->color(), QColor(mode == "day" ? "#f0f3ed" : "#0e1312"));
        for (const char *name : {"primaryRoomAction", "roomAudioButton", "danmakuRetryAction", "roomQualitySelector"}) {
            auto *control = tile->findChild<QObject *>(QString::fromLatin1(name));
            QVERIFY(control);
            auto *background = control->property("background").value<QObject *>();
            QVERIFY(background);
            QTRY_VERIFY(dark(background->property("color").value<QColor>()));
        }
        QCOMPARE(label->property("color").value<QColor>(), QColor("#edf3f0"));
    }
    QTest::qWait(100);
    const QPointF center = label->mapToItem(quality, QPointF(label->width() / 2, label->height() / 2));
    QVERIFY(qAbs(center.x() - quality->width() / 2) <= 0.5);
    auto *popup = quality->property("popup").value<QObject *>();
    QVERIFY(popup);
    window->requestActivate();
    QTRY_VERIFY(window->isActive());
    quality->forceActiveFocus(Qt::TabFocusReason);
    QTest::keyClick(window, Qt::Key_Space);
    QTRY_VERIFY(popup->property("opened").toBool());
    auto *list = tile->findChild<QQuickItem *>("roomQualityPopupList");
    QVERIFY(list);
    QQuickItem *row = nullptr;
    QTRY_VERIFY((row = list->property("currentItem").value<QQuickItem *>()) != nullptr);
    auto *option = quickItemByObjectName(row, QStringLiteral("roomQualityOptionLabel"));
    QVERIFY(option);
    QVERIFY(row->property("highlighted").toBool());
    QVERIFY(qAbs(option->mapToItem(row, QPointF(option->width() / 2, option->height() / 2)).x() - row->width() / 2) <= 0.5);
    QTest::keyClick(window, Qt::Key_Down);
    QTRY_COMPARE(quality->property("highlightedIndex").toInt(), 1);
    QTest::keyClick(window, Qt::Key_Return);
    QTRY_VERIFY(!popup->property("opened").toBool());
    QCOMPARE(controller.lastQualityRate, 3);
    QCOMPARE(quality->property("currentIndex").toInt(), 1);
    auto *action = tile->findChild<QQuickItem *>("roomTopActions");
    QVERIFY(action);
    QTest::mouseMove(window, action->mapToScene(action->boundingRect().center()).toPoint());
    QTRY_VERIFY(action->property("hovered").toBool());
    auto *actionBackground = action->property("background").value<QObject *>();
    QVERIFY(actionBackground);
    QTRY_VERIFY(dark(actionBackground->property("color").value<QColor>()));
    QCOMPARE(action->property("contentColor").value<QColor>(), QColor("#edf3f0"));
}

void QmlInteractionTest::browsesCompleteS1Archive()
{
    FakeMaoziController controller;
    controller.useBundledArchive = true;
    QQmlApplicationEngine engine;
    QQuickWindow window;
    window.resize(QSize(960, 600));
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    window.requestActivate();
    QTRY_VERIFY(window.isActive());
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/pages/MaoziRankPage.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> page(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(window.contentItem())},
        {QStringLiteral("width"), 960},
        {QStringLiteral("height"), 600},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
    }));
    QVERIFY2(page != nullptr, qPrintable(component.errorString()));
    QObject *ranking = page->findChild<QObject *>(QStringLiteral("maoziRankList"));
    QVERIFY(ranking != nullptr);
    QTRY_COMPARE(ranking->property("count").toInt(), 46);
    auto *rankingItem = qobject_cast<QQuickItem *>(ranking);
    QVERIFY(rankingItem != nullptr);
    QQuickItem *rankingRow = nullptr;
    QVERIFY(QMetaObject::invokeMethod(ranking, "itemAtIndex",
                                    Q_RETURN_ARG(QQuickItem *, rankingRow), Q_ARG(int, 0)));
    QVERIFY(rankingRow != nullptr);
    auto *firstScore = rankingRow->findChild<QObject *>(QStringLiteral("maoziCellScore"));
    QVERIFY(firstScore != nullptr);
    QTRY_COMPARE(firstScore->property("text").toString(), QStringLiteral("15.5"));
    QTRY_VERIFY(rankingItem->mapToScene(QPointF(0, 0)).y() > 100);
    QCoreApplication::processEvents();
    QQuickItem *currentRankingRow = nullptr;
    QVERIFY(QMetaObject::invokeMethod(ranking, "itemAtIndex",
                                    Q_RETURN_ARG(QQuickItem *, currentRankingRow), Q_ARG(int, 0)));
    QVERIFY(currentRankingRow != nullptr);
    const QPoint currentRankingPoint = currentRankingRow
        ->mapToScene(currentRankingRow->boundingRect().center()).toPoint();
    QTest::mouseMove(&window, QPoint(window.width() - 1, window.height() - 1));
    QTest::qWait(50);
    QTest::mouseMove(&window, currentRankingPoint);
    QTRY_VERIFY(currentRankingRow->property("rowHovered").toBool());
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, currentRankingPoint);
    QTRY_COMPARE(page->property("selectedRankIndex").toInt(), 0);
    QObject *rankingDetail = page->findChild<QObject *>(QStringLiteral("maoziRankParticipantDetail"));
    QVERIFY(rankingDetail != nullptr);
    QTRY_VERIFY(rankingDetail->property("visible").toBool());
    QVERIFY(rankingDetail->property("text").toString().contains(QStringLiteral("力量")));
    click(page->findChild<QObject *>(QStringLiteral("maoziRankDetailBack")));
    QVERIFY(currentRankingRow->property("activeFocusOnTab").toBool());
    auto *focusableRankingRow = qobject_cast<QQuickItem *>(currentRankingRow);
    QVERIFY(focusableRankingRow != nullptr);
    focusableRankingRow->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(focusableRankingRow->hasActiveFocus());
    QTest::keyClick(&window, Qt::Key_Return);
    QTRY_COMPARE(page->property("selectedRankIndex").toInt(), 0);
    QTRY_VERIFY(rankingDetail->property("visible").toBool());
    click(page->findChild<QObject *>(QStringLiteral("maoziRankDetailBack")));
    QObject *archiveSearch = page->findChild<QObject *>(QStringLiteral("maoziSearchField"));
    QVERIFY(archiveSearch != nullptr);
    archiveSearch->setProperty("text", QStringLiteral("苏袜酱"));
    QTRY_COMPARE(ranking->property("count").toInt(), 1);
    QQuickItem *filteredRow = nullptr;
    QTRY_VERIFY(QMetaObject::invokeMethod(ranking, "itemAtIndex",
                                    Q_RETURN_ARG(QQuickItem *, filteredRow), Q_ARG(int, 0))
                && filteredRow != nullptr
                && filteredRow->findChild<QObject *>(QStringLiteral("maoziCellHost")) != nullptr);
    QVERIFY(filteredRow != nullptr);
    QTRY_VERIFY(filteredRow->mapToScene(filteredRow->boundingRect().center()).y() > 100);
    const QPoint filteredRankPoint =
        filteredRow->mapToScene(filteredRow->boundingRect().center()).toPoint();
    QTest::mouseMove(&window, QPoint(window.width() - 1, window.height() - 1));
    QTest::qWait(50);
    QTest::mouseMove(&window, filteredRankPoint);
    QTRY_VERIFY(filteredRow->property("rowHovered").toBool());
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, filteredRankPoint);
    QTRY_COMPARE(page->property("selectedRankIndex").toInt(), 0);
    QTRY_VERIFY(rankingDetail->property("visible").toBool());
    QVERIFY(rankingDetail->property("text").toString().startsWith(QStringLiteral("苏袜酱")));
    click(page->findChild<QObject *>(QStringLiteral("maoziRankDetailBack")));
    archiveSearch->setProperty("text", QString());
    QObject *liveFilter = page->findChild<QObject *>(QStringLiteral("maoziLiveFilter"));
    QVERIFY(liveFilter != nullptr);
    QVERIFY(!liveFilter->property("visible").toBool());
    const struct { const char *tab; const char *list; int count; } cases[] = {
        {"maoziRegularSeasonTab", "maoziRegularSeasonList", 10},
        {"maoziFinalsTab", "maoziFinalsList", 4},
        {"maoziFootballTab", "maoziFootballList", 10},
        {"maoziPlacementTab", "maoziPlacementList", 36},
    };
    for (const auto &entry : cases) {
        QObject *tab = page->findChild<QObject *>(QString::fromLatin1(entry.tab));
        QObject *list = page->findChild<QObject *>(QString::fromLatin1(entry.list));
        QVERIFY(tab != nullptr);
        QVERIFY(list != nullptr);
        click(tab);
        QTRY_COMPARE(list->property("count").toInt(), entry.count);
    }
    click(page->findChild<QObject *>(QStringLiteral("maoziFootballTab")));
    QObject *footballPanel = page->findChild<QObject *>(QStringLiteral("footballPanel"));
    QVERIFY(footballPanel != nullptr);
    QObject *assistsTab = footballPanel->findChild<QObject *>(QStringLiteral("seasonArchiveAssistsTab"));
    QObject *leaderList = footballPanel->findChild<QObject *>(QStringLiteral("seasonArchiveLeaderList"));
    QVERIFY(assistsTab != nullptr);
    QVERIFY(leaderList != nullptr);
    click(footballPanel->findChild<QObject *>(QStringLiteral("seasonArchiveLeadersTab")));
    click(assistsTab);
    QTRY_COMPARE(leaderList->property("count").toInt(), 4);
    click(page->findChild<QObject *>(QStringLiteral("maoziPlacementTab")));
    QQuickItem *placementActions = nullptr;
    QObject *scores = nullptr;
    for (auto *item : page->findChildren<QQuickItem *>()) {
        if (item->property("text").toString() == QStringLiteral("得分统计")) {
            scores = item;
            placementActions = item->parentItem();
            break;
        }
    }
    QVERIFY(placementActions != nullptr);
    const QRectF placementBounds = placementActions->mapRectToItem(
        window.contentItem(), placementActions->boundingRect());
    QVERIFY2(placementBounds.bottom() <= window.height(),
             qPrintable(QStringLiteral("placement actions bottom %1 > %2")
                            .arg(placementBounds.bottom()).arg(window.height())));
    QObject *summary = page->findChild<QObject *>(QStringLiteral("maoziPlacementSummary"));
    QVERIFY(scores != nullptr);
    QVERIFY(summary != nullptr);
    click(scores);
    QTRY_VERIFY(summary->property("visible").toBool());
    auto *summaryItem = qobject_cast<QQuickItem *>(summary);
    QVERIFY(summaryItem != nullptr);
    QVERIFY(summaryItem->mapRectToItem(window.contentItem(), summaryItem->boundingRect())
                .bottom() <= window.height());
    QObject *overview = nullptr;
    for (auto *item : page->findChildren<QQuickItem *>()) {
        if (item->property("text").toString() == QStringLiteral("总览")
            && item->parentItem() == placementActions) {
            overview = item;
            break;
        }
    }
    click(overview);
    QObject *placementList = page->findChild<QObject *>(QStringLiteral("maoziPlacementList"));
    QVERIFY(placementList != nullptr);
    QTRY_VERIFY(placementList->property("visible").toBool());
    const QVariantMap placementIdentity = qmlData(page.get(), "visiblePlacementEntries").toList().first().toMap();
    QVERIFY(!placementIdentity.value("roomId").toString().isEmpty());
    archiveSearch->setProperty("text", placementIdentity.value("roomId"));
    QTRY_COMPARE(placementList->property("count").toInt(), 1);
    const QVariantMap matchedPlacement = qmlData(page.get(), "visiblePlacementEntries").toList().first().toMap();
    QCOMPARE(matchedPlacement.value("id"), placementIdentity.value("id"));
    QCOMPARE(matchedPlacement.value("placementAverage"), placementIdentity.value("placementAverage"));
    archiveSearch->setProperty("text", QString());
    QTRY_COMPARE(placementList->property("count").toInt(), 36);
    auto *placementListItem = qobject_cast<QQuickItem *>(placementList);
    QVERIFY(placementListItem != nullptr);
    auto *placementHeader = page->findChild<QQuickItem *>(
        QStringLiteral("maoziPlacementHeaderBar"));
    QVERIFY(placementHeader != nullptr);
    QTRY_VERIFY(placementListItem->mapToScene(QPointF(0, 0)).y()
                >= placementHeader->mapToScene(QPointF(0, placementHeader->height())).y());
    QQuickItem *placementRow = nullptr;
    QVERIFY(QMetaObject::invokeMethod(placementList, "itemAtIndex",
                                    Q_RETURN_ARG(QQuickItem *, placementRow), Q_ARG(int, 0)));
    QVERIFY(placementRow != nullptr);
    QVERIFY(placementRow->property("activeFocusOnTab").toBool());
    placementRow->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(placementRow->hasActiveFocus());
    QTest::keyClick(&window, Qt::Key_Space);
    QTRY_COMPARE(page->property("selectedPlacementIndex").toInt(), 0);
    QObject *placementDetail = page->findChild<QObject *>(
        QStringLiteral("maoziPlacementParticipantDetail"));
    QVERIFY(placementDetail != nullptr);
    QTRY_VERIFY(placementDetail->property("visible").toBool());
    click(page->findChild<QObject *>(QStringLiteral("maoziPlacementDetailBack")));
    const QPoint placementPoint = placementRow->mapToScene(
        placementRow->boundingRect().center()).toPoint();
    QTest::mouseMove(&window, QPoint(window.width() - 1, window.height() - 1));
    QTest::qWait(50);
    QTest::mouseMove(&window, placementPoint);
    QTRY_VERIFY(placementRow->property("rowHovered").toBool());
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, placementPoint);
    QTRY_COMPARE(page->property("selectedPlacementIndex").toInt(), 0);
    QTRY_VERIFY(placementDetail->property("visible").toBool());
    QVERIFY(placementDetail->property("text").toString().contains(QStringLiteral("夏季雨i")));
    click(page->findChild<QObject *>(QStringLiteral("maoziPlacementDetailBack")));
    archiveSearch->setProperty("text", QStringLiteral("夏季雨i"));
    QTRY_COMPARE(placementList->property("count").toInt(), 1);
    QQuickItem *filteredPlacement = nullptr;
    QTRY_VERIFY(QMetaObject::invokeMethod(placementList, "itemAtIndex",
                                    Q_RETURN_ARG(QQuickItem *, filteredPlacement), Q_ARG(int, 0))
                && filteredPlacement != nullptr
                && filteredPlacement->mapToScene(filteredPlacement->boundingRect().center()).y() > 100);
    const QPoint filteredPlacementPoint = filteredPlacement
        ->mapToScene(filteredPlacement->boundingRect().center()).toPoint();
    QTest::mouseMove(&window, QPoint(window.width() - 1, window.height() - 1));
    QTest::qWait(50);
    QTest::mouseMove(&window, filteredPlacementPoint);
    QTRY_VERIFY(filteredPlacement->property("rowHovered").toBool());
    QTest::mouseClick(&window, Qt::LeftButton, Qt::NoModifier, filteredPlacementPoint);
    QTRY_COMPARE(page->property("selectedPlacementIndex").toInt(), 0);
    QTRY_VERIFY(placementDetail->property("visible").toBool());
    QVERIFY(placementDetail->property("text").toString().startsWith(QStringLiteral("夏季雨i")));
    click(page->findChild<QObject *>(QStringLiteral("maoziPlacementDetailBack")));
    archiveSearch->setProperty("text", QString());
    click(page->findChild<QObject *>(QStringLiteral("maoziRegularSeasonTab")));
    QObject *dateFilter = page->findChild<QObject *>(QStringLiteral("seasonArchiveDateFilter"));
    QVERIFY(dateFilter != nullptr);
    QVERIFY(dateFilter->property("count").toInt() > 1);
    dateFilter->setProperty("currentIndex", 1);
    QObject *regularPanel = page->findChild<QObject *>(QStringLiteral("regularSeasonPanel"));
    QVERIFY(regularPanel != nullptr);
    QVERIFY(regularPanel->property("visibleMatches").toList().size() < 31);
    dateFilter->setProperty("currentIndex", 0);
    QObject *next = page->findChild<QObject *>(QStringLiteral("seasonArchiveNextPage"));
    QVERIFY(next != nullptr);
    click(next);
    QTRY_COMPARE(regularPanel->property("pageIndex").toInt(), 1);
    QObject *regularList = page->findChild<QObject *>(QStringLiteral("maoziRegularSeasonList"));
    QVERIFY(regularList != nullptr);
    QQuickItem *matchRow = nullptr;
    QTRY_VERIFY(QMetaObject::invokeMethod(regularList, "itemAtIndex",
                                    Q_RETURN_ARG(QQuickItem *, matchRow), Q_ARG(int, 0)));
    QVERIFY(matchRow != nullptr);
    QVERIFY(matchRow->property("activeFocusOnTab").toBool());
    auto *focusableMatchRow = qobject_cast<QQuickItem *>(matchRow);
    QVERIFY(focusableMatchRow != nullptr);
    focusableMatchRow->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(focusableMatchRow->hasActiveFocus());
    QTest::keyClick(&window, Qt::Key_Return);
    QTRY_COMPARE(regularPanel->property("selectedIndex").toInt(), 10);
    QObject *detail = regularPanel->findChild<QObject *>(QStringLiteral("seasonArchiveDetail"));
    QVERIFY(detail != nullptr);
    QTRY_VERIFY(detail->property("visible").toBool());
    QVERIFY(detail->property("text").toString().length() > 30);
    click(regularPanel->findChild<QObject *>(QStringLiteral("seasonArchiveBack")));
    QTRY_COMPARE(regularPanel->property("selectedIndex").toInt(), -1);
    focusableMatchRow->forceActiveFocus(Qt::TabFocusReason);
    QTRY_VERIFY(focusableMatchRow->hasActiveFocus());
    QTest::keyClick(&window, Qt::Key_Space);
    QTRY_COMPARE(regularPanel->property("selectedIndex").toInt(), 10);
    QTRY_VERIFY(detail->property("visible").toBool());
    click(regularPanel->findChild<QObject *>(QStringLiteral("seasonArchiveBack")));
    QTRY_COMPARE(regularPanel->property("selectedIndex").toInt(), -1);
    QObject *refresh = page->findChild<QObject *>(QStringLiteral("maoziRefreshButton"));
    QVERIFY(refresh != nullptr);
    QVERIFY(!refresh->property("enabled").toBool());
    page->setProperty("width", 1280);
    page->setProperty("height", 720);
    window.resize(QSize(1280, 720));
    click(page->findChild<QObject *>(QStringLiteral("maoziPlacementTab")));
    click(overview);
    QTRY_VERIFY(placementListItem->mapToScene(QPointF(0, 0)).y() > 100);
    const QRectF wideActions = placementActions->mapRectToItem(
        window.contentItem(), placementActions->boundingRect());
    QVERIFY(wideActions.bottom() <= window.height());
}

QTEST_MAIN(QmlInteractionTest)

#include "qml_interaction_test.moc"
