#include <QSettings>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtTest/QtTest>

#include "app/windows_notification_service.h"
#include "danmaku/douyu_danmaku_client.h"
#include "ui/app_controller.h"
#include "ui/room_list_model.h"
#include "ui/workspace_model.h"
#include "workspace/room_capacity.h"
#include "workspace/guild_room_resolver.h"

#ifndef FAKE_STREAMGET_SERVICE_PATH
#define FAKE_STREAMGET_SERVICE_PATH "fake_streamget_service"
#endif

namespace {

QString fakeServicePath()
{
    return QString::fromLocal8Bit(FAKE_STREAMGET_SERVICE_PATH);
}

class ImportRankServer final : public QObject {
public:
    ImportRankServer()
    {
        server.listen(QHostAddress::LocalHost);
        connect(&server, &QTcpServer::newConnection, this, [this] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, socket, [this, socket] {
                const auto request = socket->readAll();
                if (socket->property("responded").toBool()) return;
                socket->setProperty("responded", true);
                const bool auth = request.startsWith("POST /auth");
                if (!auth) ++snapshotRequests;
                const auto body = auth
                    ? QByteArrayLiteral(R"({"access_token":"fixture-only"})")
                    : QJsonDocument(QJsonObject{{"hosts", hosts}}).toJson();
                const int code = auth ? 200 : status;
                QTimer::singleShot(delayMs, socket, [socket, body, code] {
                    socket->write("HTTP/1.1 " + QByteArray::number(code)
                        + " Result\r\nContent-Type: application/json\r\nContent-Length: "
                        + QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
                    socket->disconnectFromHost();
                });
            });
        });
    }
    std::unique_ptr<MaoziRankClient> client()
    {
        const auto base = QStringLiteral("http://127.0.0.1:%1").arg(server.serverPort());
        return std::make_unique<MaoziRankClient>(QUrl(base + "/auth"),
                                                QUrl(base + "/snapshot"), 2000);
    }
    QTcpServer server;
    QJsonArray hosts{QJsonObject{{"id", "captain"}, {"name", QStringLiteral("尐表哥")},
                                 {"douyu_id", "217331"}, {"team", 0}}};
    int status = 200, delayMs = 0, snapshotRequests = 0;
};

class FakeNotificationSink final : public SystemNotificationSink {
public:
    bool available() const override { return true; }
    void show(const QString &, const QString &) override { ++shownCount; }

    int shownCount = 0;
};

struct FakeDanmakuFactoryState {
    int starts = 0;
    int stops = 0;
    QVector<QString> rooms;
};

class FakeDanmakuClient final : public DanmakuClient {
public:
    FakeDanmakuClient(QString roomId, FakeDanmakuFactoryState *state, QObject *parent)
        : DanmakuClient(parent)
        , roomId_(std::move(roomId))
        , state_(state)
    {
    }

    void start() override
    {
        if (started_) return;
        started_ = true;
        ++state_->starts;
        emit statusChanged({roomId_, DanmakuConnectionState::Connected});
    }

    void stop() override
    {
        if (!started_ || stopped_) return;
        stopped_ = true;
        ++state_->stops;
    }

    void retry() override
    {
        stopped_ = false;
        start();
    }

private:
    QString roomId_;
    FakeDanmakuFactoryState *state_ = nullptr;
    bool started_ = false;
    bool stopped_ = false;
};

} // namespace

class AppControllerTest final : public QObject {
    Q_OBJECT

private slots:
    void defersGuildChecksUntilNavigationOpens();
    void preservesFailedSaveAndRetriesLatestWorkspace();
    void coalescesVolumeSavesAndFlushesOnShutdown();
    void coalescesGuildCacheAndMemberNotifications();
    void persistsEditableEventMapping();
    void addsRoomsThroughModelAndRejectsOverflow();
    void recordsOpenedRoomsForTheLibraryHistory();
    void doesNotExposeSensitivePlaybackMaterial();
    void restoresSavedMetadataIntoRoomModel();
    void persistsGroupPresetAndRoomPresentationSettings();
    void persistsNotificationPreferenceAndUpdatesMonitoringState();
    void persistsNotificationEventPreferences();
    void synchronizesDanmakuFromGlobalRoomLiveAndActiveState();
    void stopsDanmakuBeforeServiceShutdown();
    void activatesGroupAndReplacesActiveRoomSet();
    void managesGroupMembersAndOrder();
    void allowsRoomMembershipInMultipleGroupsAndEnforcesCapacity();
    void assignsLibraryRoomToActiveGroupAfterSwitch();
    void restoresActiveGroupMembershipInOrder();
    void searchesRoomCandidatesAndAddsMetadata();
    void projectsRefreshedRoomMetadataAndStatus();
    void monitorsInactiveFavoriteRoomAndUpdatesLibrary();
    void publishesCommandFailureToToast();
    void restoresLayoutAndRatioFromWorkspacePreset();
    void persistsGlobalAudioPolicy();
    void preservesFavoriteWhenReaddingLibraryRoom();
    void ordersFavoritesByManualOrderAndMovesThem();
    void removesInactiveHistoryWithoutRemovingFavoriteEntry();
    void appliesWorkspacePresetToAllRoomsRepeatedly();
    void restoresPresetRoomsMissingFromLibrary();
    void deletesWorkspacePresetAndPersistsRemoval();
    void defersRequestedRoomRemovalUntilEventLoop();
    void preservesPlaybackAndDanmakuStateWhenHostedInBackground();
    void minimizesWithoutEnteringBackground();
    void persistsCloseBehaviorPreference();
    void clearsUnrememberedCloseBehaviorPreference();
    void exposesUpdateCheckerState();
    void createsEmptyTeamsAndAssignsRosterMembers();
    void movesMemberBetweenTeamsAndLeavesEmptySlots();
    void managesTeamOrderWithoutChangingGroupsOrRooms();
    void rejectsDuplicateTeamNames();
    void persistsTeamDeletion();
    void enforcesTeamLimitsAndRosterMembership();
    void persistsIndependentTeamsAndNavigationVisibility();
    void addsResolvedGuildMemberWithoutUsingOrdinarySearchState();
    void rejectsGuildQuickAddWithoutRoomOrCapacity();
    void enforcesGuildQuickAddLayoutCapacity();
    void exposesTeamImportCommands();
    void previewsImportsAndPersistsTeams();
    void rejectsFailedCancelledAndStaleImports();
    void doesNotPublishImportWhenSaveFails();
    void previewsLiveTeamImportWhenEnabled();
    void importsAllEightCaptainsWithMissingTeams();
    void keepsImportPreviewAfterRoomMetadataUpdates();
    void keepsImportPreviewAfterRankDisplayUpdates();
    void rejectsImportAfterVerifiedRoomIdentityChanges();
    void keepsImportPreviewWhenBundledRoomIsFirstCached();
};

void AppControllerTest::keepsImportPreviewWhenBundledRoomIsFirstCached()
{
    ImportRankServer server;
    QTemporaryDir dir;
    QSettings settings(dir.filePath("workspace.ini"), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink, {}, nullptr, server.client());
    auto *resolver = controller.findChild<GuildRoomResolver *>();
    QVERIFY(resolver != nullptr);
    resolver->setCache({});
    controller.previewMaoziTeamImport();
    QTRY_COMPARE(controller.maoziTeamImport().value("state").toString(), QStringLiteral("preview"));
    GuildRoomCacheEntry cached;
    cached.memberId = QStringLiteral("hamster-005");
    cached.roomId = QStringLiteral("217331");
    cached.verifiedAtMs = 1;
    resolver->setCache({cached});
    QVERIFY(controller.maoziTeamImport().value("canConfirm").toBool());
    QCOMPARE(controller.confirmMaoziTeamImport(), QString());
    QVERIFY(NativeWorkspaceStore(&settings).load().teams.first().memberIds
                .contains(QStringLiteral("hamster-005")));
}

void AppControllerTest::keepsImportPreviewAfterRoomMetadataUpdates()
{
    ImportRankServer server;
    QTemporaryDir dir;
    QSettings settings(dir.filePath("workspace.ini"), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink, {}, nullptr, server.client());
    auto *resolver = controller.findChild<GuildRoomResolver *>();
    QVERIFY(resolver != nullptr);
    GuildRoomCacheEntry cached;
    cached.memberId = QStringLiteral("hamster-005");
    cached.roomId = QStringLiteral("217331");
    cached.anchorName = QStringLiteral("尐表哥-队长");
    cached.verifiedAtMs = 1;
    resolver->setCache({cached});
    controller.previewMaoziTeamImport();
    QTRY_COMPARE(controller.maoziTeamImport().value("state").toString(), QStringLiteral("preview"));
    QVERIFY(controller.maoziTeamImport().value("canConfirm").toBool());
    const auto before = controller.maoziTeamImport().value("teams");
    cached.liveState = QStringLiteral("online");
    cached.liveCheckedAtMs = 50;
    cached.metadataCheckedAtMs = 50;
    cached.verifiedAtMs = 50;
    cached.avatarUrl = QUrl(QStringLiteral("https://example.invalid/avatar.png"));
    QSignalSpy changed(&controller, &AppController::maoziTeamImportChanged);
    resolver->setCache({cached});
    QVERIFY(changed.count() > 0);
    QVERIFY(controller.maoziTeamImport().value("canConfirm").toBool());
    QCOMPARE(controller.maoziTeamImport().value("teams"), before);
    QCOMPARE(controller.confirmMaoziTeamImport(), QString());
    const auto saved = NativeWorkspaceStore(&settings).load();
    QCOMPARE(saved.teams.size(), 1);
    QVERIFY(saved.teams.first().memberIds.contains(QStringLiteral("hamster-005")));
    QCOMPARE(saved.guildRoomCache.first().liveState, cached.liveState);
    QCOMPARE(saved.guildRoomCache.first().avatarUrl, cached.avatarUrl);
    QCOMPARE(saved.guildRoomCache.first().verifiedAtMs, cached.verifiedAtMs);
}

void AppControllerTest::keepsImportPreviewAfterRankDisplayUpdates()
{
    ImportRankServer server;
    QTemporaryDir dir;
    QSettings settings(dir.filePath("workspace.ini"), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink, {}, nullptr, server.client());
    controller.previewMaoziTeamImport();
    QTRY_COMPARE(controller.maoziTeamImport().value("state").toString(), QStringLiteral("preview"));
    const auto before = controller.maoziTeamImport().value("teams");
    auto host = server.hosts.first().toObject();
    host.insert("note", QStringLiteral("更新后的公开备注"));
    host.insert("poster", QStringLiteral("captain-updated.png"));
    host.insert("guild", QStringLiteral("更新后的公会展示"));
    server.hosts[0] = host;
    controller.maoziRank()->refresh();
    QTRY_VERIFY(!controller.maoziRank()->loading());
    QVERIFY(controller.maoziTeamImport().value("canConfirm").toBool());
    QCOMPARE(controller.maoziTeamImport().value("teams"), before);
    QCOMPARE(controller.confirmMaoziTeamImport(), QString());
}

void AppControllerTest::rejectsImportAfterVerifiedRoomIdentityChanges()
{
    ImportRankServer server;
    QTemporaryDir dir;
    QSettings settings(dir.filePath("workspace.ini"), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink, {}, nullptr, server.client());
    auto *resolver = controller.findChild<GuildRoomResolver *>();
    QVERIFY(resolver != nullptr);
    GuildRoomCacheEntry cached;
    cached.memberId = QStringLiteral("hamster-005");
    cached.roomId = QStringLiteral("217331");
    cached.verifiedAtMs = 1;
    resolver->setCache({cached});
    controller.previewMaoziTeamImport();
    QTRY_COMPARE(controller.maoziTeamImport().value("state").toString(), QStringLiteral("preview"));
    cached.roomId = QStringLiteral("999999");
    QSignalSpy changed(&controller, &AppController::maoziTeamImportChanged);
    resolver->setCache({cached});
    QVERIFY(changed.count() > 0);
    QVERIFY(!controller.maoziTeamImport().value("canConfirm").toBool());
    QVERIFY(!controller.confirmMaoziTeamImport().isEmpty());
    QVERIFY(controller.workspace()->teams().isEmpty());
    QVERIFY(NativeWorkspaceStore(&settings).load().teams.isEmpty());
}

void AppControllerTest::importsAllEightCaptainsWithMissingTeams()
{
    ImportRankServer server;
    server.hosts = {};
    const QStringList captainIds{"hamster-005", "hamster-011", "hamster-003", "hamster-015",
                                 "hamster-006", "hamster-050", "hamster-008", "hamster-007"};
    const QStringList rooms{"217331", "7204164", "80432", "6151194",
                            "731252", "11222", "2140934", "2632018"};
    const QStringList expectedTeams{QStringLiteral("蓝队"), QStringLiteral("蓝队"),
                                    QStringLiteral("黑队"), QStringLiteral("黑队"),
                                    QStringLiteral("紫队"), QStringLiteral("紫队"),
                                    QStringLiteral("红队"), QStringLiteral("红队")};
    const auto roster = GuildRoster::bundled();
    int memberCount = 0;
    for (const auto &member : roster) {
        const int captain = captainIds.indexOf(member.id);
        if (member.roomId == QStringLiteral("320155")) continue;
        if (captain < 0 && memberCount >= 36) continue;
        QJsonObject host{{"id", member.id}, {"name", GuildRoster::normalizedName(member.anchorName)},
                          {"douyu_id", captain >= 0 ? rooms.at(captain) : member.roomId}};
        if (captain >= 0) host.insert("team", QJsonValue::Null);
        else host.insert("team", memberCount++ % 4);
        server.hosts.append(host);
    }
    QCOMPARE(server.hosts.size(), 44);
    QTemporaryDir dir;
    QSettings settings(dir.filePath("workspace.ini"), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink, {}, nullptr, server.client());
    controller.previewMaoziTeamImport();
    QTRY_COMPARE(controller.maoziTeamImport().value("state").toString(), QStringLiteral("preview"));
    const auto preview = controller.maoziTeamImport();
    QVERIFY(preview.value("canConfirm").toBool());
    QCOMPARE(preview.value("teams").toList().size(), 4);
    QCOMPARE(preview.value("changedMembers").toInt(), 44);
    QVERIFY(preview.value("unmatchedNames").toStringList().isEmpty());
    QVERIFY(preview.value("conflictNames").toStringList().isEmpty());
    QVERIFY(preview.value("unassignedNames").toStringList().isEmpty());
    for (const auto &team : preview.value("teams").toList()) {
        QCOMPARE(team.toMap().value("sourceCount").toInt(), 11);
        QCOMPARE(team.toMap().value("matchedCount").toInt(), 11);
    }
    QVERIFY(controller.workspace()->teams().isEmpty());
    QCOMPARE(controller.confirmMaoziTeamImport(), QString());
    const auto saved = NativeWorkspaceStore(&settings).load();
    for (int i = 0; i < captainIds.size(); ++i) {
        bool found = false;
        for (const auto &team : saved.teams)
            if (team.name == expectedTeams.at(i) && team.memberIds.contains(captainIds.at(i)))
                found = true;
        QVERIFY2(found, qPrintable(captainIds.at(i)));
    }
    QVERIFY(saved.activeRoomIds.isEmpty());
    controller.previewMaoziTeamImport();
    QTRY_COMPARE(controller.maoziTeamImport().value("state").toString(), QStringLiteral("preview"));
    QCOMPARE(controller.maoziTeamImport().value("changedMembers").toInt(), 0);
    QCOMPARE(controller.maoziTeamImport().value("unchangedMembers").toInt(), 44);
    QCOMPARE(controller.maoziTeamImport().value("createdTeams").toInt(), 0);
    controller.cancelMaoziTeamImport();
}

void AppControllerTest::previewsLiveTeamImportWhenEnabled()
{
    if (!qEnvironmentVariableIsSet("DOUYU_VERIFY_LIVE_TEAM_IMPORT"))
        QSKIP("Opt-in read-only live preview; no local user workspace mutation");
    QTemporaryDir dir;
    QSettings settings(dir.filePath("workspace.ini"), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);
    controller.previewMaoziTeamImport();
    QTRY_VERIFY_WITH_TIMEOUT(controller.maoziTeamImport().value("state")
                            != QStringLiteral("loading"), 40000);
    const auto state = controller.maoziTeamImport();
    QCOMPARE(state.value("state").toString(), QStringLiteral("preview"));
    QVERIFY(state.value("canConfirm").toBool());
    QCOMPARE(state.value("teams").toList().size(), 4);
    QVERIFY(state.value("unmatchedNames").toStringList().isEmpty());
    QVERIFY(state.value("conflictNames").toStringList().isEmpty());
    int sourceCount = 0, matchedCount = 0;
    for (const auto &team : state.value("teams").toList()) {
        sourceCount += team.toMap().value("sourceCount").toInt();
        matchedCount += team.toMap().value("matchedCount").toInt();
    }
    QCOMPARE(sourceCount, matchedCount);
    qInfo() << "Read-only live preview teams:" << state.value("teams").toList().size()
            << "matched members:" << matchedCount;
    QVERIFY(controller.workspace()->teams().isEmpty());
    controller.cancelMaoziTeamImport();
    QVERIFY(NativeWorkspaceStore(&settings).load().teams.isEmpty());
}

void AppControllerTest::previewsImportsAndPersistsTeams()
{
    ImportRankServer server;
    QTemporaryDir dir;
    const auto path = dir.filePath("workspace.ini");
    QSettings settings(path, QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink, {}, nullptr, server.client());
    const auto redId = controller.createTeam(QStringLiteral("红队"));
    const auto oldId = controller.createTeam(QStringLiteral("手动队"));
    const auto roster = controller.guildRoster();
    QString captainId, otherId;
    for (const auto &value : roster) {
        const auto member = value.toMap();
        if (member.value("anchorName").toString() == QStringLiteral("尐表哥"))
            captainId = member.value("id").toString();
        else if (otherId.isEmpty()) otherId = member.value("id").toString();
    }
    QVERIFY(!captainId.isEmpty() && !otherId.isEmpty());
    QCOMPARE(controller.assignGuildMemberToTeam(captainId, oldId), QString());
    QCOMPARE(controller.assignGuildMemberToTeam(otherId, redId), QString());
    controller.addRoom("63136");
    controller.setFavorite("63136", true);
    QTRY_COMPARE(controller.libraryRooms().first().toMap().value("anchorName").toString(),
                 QStringLiteral("Fake Anchor"));
    QTRY_VERIFY(controller.libraryRooms().first().toMap().value("online").toBool());
    QTRY_COMPARE(NativeWorkspaceStore(&settings).load().library.first().metadata.anchorName,
                 QStringLiteral("Fake Anchor"));
    const auto before = NativeWorkspaceStore(&settings).load();
    controller.previewMaoziTeamImport();
    QCOMPARE(controller.maoziTeamImport().value("state").toString(), QStringLiteral("loading"));
    controller.previewMaoziTeamImport();
    QCOMPARE(NativeWorkspaceStore(&settings).load().teams, before.teams);
    QTRY_COMPARE(controller.maoziTeamImport().value("state").toString(), QStringLiteral("preview"));
    QCOMPARE(server.snapshotRequests, 1);
    QVERIFY(controller.maoziTeamImport().value("canConfirm").toBool());
    QCOMPARE(controller.maoziTeamImport().value("changedMembers").toInt(), 1);
    QCOMPARE(controller.confirmMaoziTeamImport(), QString());
    const auto after = NativeWorkspaceStore(&settings).load();
    QCOMPARE(after.teams.first().id, redId);
    QVERIFY(after.teams.first().memberIds.contains(captainId));
    QVERIFY(after.teams.first().memberIds.contains(otherId));
    QVERIFY(after.teams.last().memberIds.isEmpty());
    auto teamsOnly = before;
    teamsOnly.teams = after.teams;
    QCOMPARE(after.library, teamsOnly.library);
    QCOMPARE(after.activeRoomIds, teamsOnly.activeRoomIds);
    QCOMPARE(after.primaryRoomId, teamsOnly.primaryRoomId);
    QCOMPARE(after.audioRoomId, teamsOnly.audioRoomId);
    QCOMPARE(after.groups, teamsOnly.groups);
    QCOMPARE(after.danmaku, teamsOnly.danmaku);
    QCOMPARE(after, teamsOnly);
    QVERIFY(!controller.confirmMaoziTeamImport().isEmpty());
    controller.previewMaoziTeamImport();
    QTRY_COMPARE(controller.maoziTeamImport().value("state").toString(), QStringLiteral("preview"));
    QCOMPARE(controller.maoziTeamImport().value("changedMembers").toInt(), 0);
    QCOMPARE(controller.maoziTeamImport().value("createdTeams").toInt(), 0);
    controller.cancelMaoziTeamImport();
    QSettings restoredSettings(path, QSettings::IniFormat);
    AppController restored(fakeServicePath(), &restoredSettings, &sink);
    QCOMPARE(restored.workspace()->teams(), controller.workspace()->teams());
}

void AppControllerTest::rejectsFailedCancelledAndStaleImports()
{
    ImportRankServer server;
    QTemporaryDir dir;
    QSettings settings(dir.filePath("workspace.ini"), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink, {}, nullptr, server.client());
    controller.createTeam(QStringLiteral("手动队"));
    const auto original = controller.workspace()->teams();
    controller.previewMaoziTeamImport();
    QTRY_COMPARE(controller.maoziTeamImport().value("state").toString(), QStringLiteral("preview"));
    controller.cancelMaoziTeamImport();
    QCOMPARE(controller.maoziTeamImport().value("state").toString(), QStringLiteral("idle"));
    QCOMPARE(controller.workspace()->teams(), original);
    server.status = 500;
    controller.previewMaoziTeamImport();
    QTRY_COMPARE(controller.maoziTeamImport().value("state").toString(), QStringLiteral("error"));
    QVERIFY(!controller.maoziRank()->entries().isEmpty());
    QVERIFY(!controller.confirmMaoziTeamImport().isEmpty());
    QCOMPARE(controller.workspace()->teams(), original);
    server.status = 200;
    server.delayMs = 60;
    controller.previewMaoziTeamImport();
    controller.cancelMaoziTeamImport();
    QTRY_VERIFY(!controller.maoziRank()->loading());
    QCOMPARE(controller.maoziTeamImport().value("state").toString(), QStringLiteral("idle"));
    QCOMPARE(controller.workspace()->teams(), original);
    controller.previewMaoziTeamImport();
    QTRY_COMPARE(controller.maoziTeamImport().value("state").toString(), QStringLiteral("preview"));
    controller.createTeam(QStringLiteral("新增队"));
    const auto changed = controller.workspace()->teams();
    QVERIFY(!controller.confirmMaoziTeamImport().isEmpty());
    QCOMPARE(controller.workspace()->teams(), changed);
    controller.previewMaoziTeamImport();
    QTRY_COMPARE(controller.maoziTeamImport().value("state").toString(), QStringLiteral("preview"));
    server.hosts[0] = QJsonObject{{"id", "captain"}, {"name", QStringLiteral("尐表哥")},
                                  {"douyu_id", "217331"}, {"team", 1}};
    controller.maoziRank()->refresh();
    QTRY_VERIFY(!controller.maoziRank()->loading());
    QVERIFY(!controller.confirmMaoziTeamImport().isEmpty());
    QCOMPARE(controller.workspace()->teams(), changed);
}

void AppControllerTest::doesNotPublishImportWhenSaveFails()
{
    ImportRankServer server;
    QTemporaryDir dir;
    // An existing directory cannot be replaced by a settings file.
    QSettings settings(dir.path(), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink, {}, nullptr, server.client());
    const auto original = controller.workspace()->teams();
    controller.previewMaoziTeamImport();
    QTRY_COMPARE(controller.maoziTeamImport().value("state").toString(), QStringLiteral("preview"));
    QVERIFY(!controller.confirmMaoziTeamImport().isEmpty());
    QCOMPARE(controller.maoziTeamImport().value("state").toString(), QStringLiteral("error"));
    QCOMPARE(controller.workspace()->teams(), original);
    QVERIFY(settings.status() != QSettings::NoError);
}

void AppControllerTest::exposesTeamImportCommands()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("workspace.ini"), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);
    QVERIFY(controller.metaObject()->indexOfMethod("previewMaoziTeamImport()") >= 0);
    QVERIFY(controller.metaObject()->indexOfMethod("confirmMaoziTeamImport()") >= 0);
    QVERIFY(controller.metaObject()->indexOfMethod("cancelMaoziTeamImport()") >= 0);
    QCOMPARE(controller.property("maoziTeamImport").toMap().value("state").toString(),
             QStringLiteral("idle"));
}

void AppControllerTest::preservesPlaybackAndDanmakuStateWhenHostedInBackground()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);
    QCOMPARE(controller.danmaku()->presentationSuspended(), false);
    controller.minimizeToBackground();
    QCOMPARE(controller.backgroundHosted(), true);
    QCOMPARE(controller.danmaku()->presentationSuspended(), true);
    controller.restoreFromBackground();
    QCOMPARE(controller.backgroundHosted(), false);
    QCOMPARE(controller.danmaku()->presentationSuspended(), false);
}

void AppControllerTest::exposesUpdateCheckerState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.currentVersion(), QStringLiteral(DOUYU_APP_VERSION));
    QCOMPARE(controller.updateState(), QStringLiteral("idle"));
    QCOMPARE(controller.updateMessage(), QString());
    QCOMPARE(controller.latestVersion(), QString());
    QVERIFY(!controller.updateReleaseUrl().isValid());
    QVERIFY(!controller.openLatestRelease());
    QVERIFY(!controller.openExternalUrl(QStringLiteral("javascript:alert(1)")));
    QVERIFY(!controller.openExternalUrl(QStringLiteral("http://example.com")));
}

void AppControllerTest::minimizesWithoutEnteringBackground()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    controller.minimizeWindow();
    QCOMPARE(controller.backgroundHosted(), false);
    QCOMPARE(controller.windowMinimized(), true);
    controller.restoreFromMinimized();
    QCOMPARE(controller.windowMinimized(), false);
}

void AppControllerTest::persistsCloseBehaviorPreference()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("workspace.ini"));
    FakeNotificationSink sink;
    {
        QSettings settings(path, QSettings::IniFormat);
        AppController controller(fakeServicePath(), &settings, &sink);
        QCOMPARE(controller.closeBehavior(), QStringLiteral("ask"));
        QVERIFY(controller.setCloseBehavior(QStringLiteral("background"), true));
    }
    QSettings restoredSettings(path, QSettings::IniFormat);
    AppController restored(fakeServicePath(), &restoredSettings, &sink);
    QCOMPARE(restored.closeBehavior(), QStringLiteral("background"));
}

void AppControllerTest::clearsUnrememberedCloseBehaviorPreference()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("workspace.ini"));
    FakeNotificationSink sink;
    {
        QSettings settings(path, QSettings::IniFormat);
        AppController controller(fakeServicePath(), &settings, &sink);
        QVERIFY(controller.setCloseBehavior(QStringLiteral("quit"), true));
        QVERIFY(controller.setCloseBehavior(QStringLiteral("background"), false));
        QVERIFY(controller.setCloseBehavior(QStringLiteral("ask"), false));
        QVERIFY(!settings.contains(QStringLiteral("window/closeBehavior")));
    }
    QSettings restoredSettings(path, QSettings::IniFormat);
    AppController restored(fakeServicePath(), &restoredSettings, &sink);
    QCOMPARE(restored.closeBehavior(), QStringLiteral("ask"));
}

void AppControllerTest::preservesFailedSaveAndRetriesLatestWorkspace()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath("blocked.ini");
    QVERIFY(QDir().mkdir(path));
    QSettings settings(path, QSettings::IniFormat);
    AppController controller(fakeServicePath(), &settings);
    QVERIFY(!controller.createTeam("First").isEmpty());
    QVERIFY(controller.property("workspaceUnsaved").toBool());
    QVERIFY(!controller.createTeam("Latest").isEmpty());
    QVERIFY(QDir().rmdir(path));
    QVERIFY(QMetaObject::invokeMethod(&controller, "retryWorkspaceSave"));
    QVERIFY(!controller.property("workspaceUnsaved").toBool());
    QSettings saved(path, QSettings::IniFormat);
    const auto snapshot = NativeWorkspaceStore(&saved).load();
    QCOMPARE(snapshot.teams.size(), 2);
    QCOMPARE(snapshot.teams.last().name, QStringLiteral("Latest"));
}

void AppControllerTest::coalescesVolumeSavesAndFlushesOnShutdown()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("workspace.ini"), QSettings::IniFormat);
    AppController controller(fakeServicePath(), &settings);
    QCOMPARE(controller.addRoom("63136"), QString());
    const int savedVolume = NativeWorkspaceStore(&settings).load().library.first().volume;
    for (int volume = 10; volume <= 30; ++volume)
        QCOMPARE(controller.setVolume("63136", volume), QString());
    QCOMPARE(NativeWorkspaceStore(&settings).load().library.first().volume, savedVolume);
    QTRY_COMPARE_WITH_TIMEOUT(
        NativeWorkspaceStore(&settings).load().library.first().volume, 30, 1000);
    QCOMPARE(controller.setVolume("63136", 40), QString());
    controller.shutdown();
    QCOMPARE(NativeWorkspaceStore(&settings).load().library.first().volume, 40);
}

void AppControllerTest::coalescesGuildCacheAndMemberNotifications()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("workspace.ini"), QSettings::IniFormat);
    AppController controller(fakeServicePath(), &settings);
    auto *resolver = controller.findChild<GuildRoomResolver *>();
    QVERIFY(resolver);
    QSignalSpy changed(&controller, &AppController::guildRosterChanged);
    for (int index = 0; index < 58; ++index) {
        resolver->cacheChanged();
        resolver->memberChanged(QStringLiteral("fixture"));
    }
    QCOMPARE(changed.count(), 0);
    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 1000);
}

void AppControllerTest::persistsEditableEventMapping()
{
    QTemporaryDir directory;
    QSettings settings(directory.filePath("mapping.ini"), QSettings::IniFormat);
    const QString mapping = QStringLiteral(R"({"version":1,"event":"fixture","members":[{"roomId":"217331","name":"尐表哥","role":"member","team":null}]})");
    {
        AppController controller(fakeServicePath(), &settings);
        QVERIFY(controller.saveEventMapping(mapping).isEmpty());
        QVERIFY(!controller.workspaceUnsaved());
        QVERIFY(!controller.saveEventMapping("{}").isEmpty());
    }
    AppController restored(fakeServicePath(), &settings);
    QCOMPARE(QJsonDocument::fromJson(restored.eventMappingJson().toUtf8()),
             QJsonDocument::fromJson(mapping.toUtf8()));
}

void AppControllerTest::addsRoomsThroughModelAndRejectsOverflow()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    const int maxRooms = RoomCapacity::currentLimits().maxLayoutRooms;
    for (int index = 0; index < maxRooms; ++index) {
        QCOMPARE(controller.addRoom(QString::number(63136 + index)), QString());
    }
    QCOMPARE(controller.addRoom(QStringLiteral("999999")),
             QStringLiteral("最多添加 %1 个房间").arg(maxRooms));
    QCOMPARE(controller.rooms()->rowCount(), maxRooms);
    QVERIFY(controller.setLayout(QStringLiteral("primary-two")));
    QCOMPARE(controller.rooms()->rowCount(), maxRooms);
}

void AppControllerTest::defersRequestedRoomRemovalUntilEventLoop()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QVERIFY(QMetaObject::invokeMethod(&controller, "requestRemoveRoom",
                                      Q_ARG(QString, QStringLiteral("63136"))));
    QCOMPARE(controller.rooms()->rowCount(), 1);
    QTRY_COMPARE(controller.rooms()->rowCount(), 0);
}

void AppControllerTest::recordsOpenedRoomsForTheLibraryHistory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    const QVariantList rooms = controller.libraryRooms();
    QCOMPARE(rooms.size(), 1);
    const QVariantMap entry = rooms.first().toMap();
    QCOMPARE(entry.value(QStringLiteral("roomId")).toString(), QStringLiteral("63136"));
    QVERIFY(entry.value(QStringLiteral("active")).toBool());
    QVERIFY(entry.value(QStringLiteral("lastOpenedAtMs")).toLongLong() > 0);
}

void AppControllerTest::removesInactiveHistoryWithoutRemovingFavoriteEntry()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.setFavorite(QStringLiteral("63136"), true), QString());
    QCOMPARE(controller.removeRoom(QStringLiteral("63136")), QString());

    QCOMPARE(controller.removeHistoryRoom(QStringLiteral("63136")), QString());
    const QVariantMap entry = controller.libraryRooms().first().toMap();
    QCOMPARE(entry.value(QStringLiteral("roomId")).toString(), QStringLiteral("63136"));
    QVERIFY(entry.value(QStringLiteral("favorite")).toBool());
    QCOMPARE(entry.value(QStringLiteral("lastOpenedAtMs")).toLongLong(), 0);
    QVERIFY(!entry.value(QStringLiteral("active")).toBool());
}

void AppControllerTest::doesNotExposeSensitivePlaybackMaterial()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    const QString message = controller.fixedPlaybackMessage(QStringLiteral("UNSAFE_STREAM_URL"));
    QCOMPARE(message, QStringLiteral("播放地址不可用"));
    QVERIFY(!message.contains(QStringLiteral("://")));
    QVERIFY(!message.contains(QStringLiteral("token"), Qt::CaseInsensitive));
}

void AppControllerTest::restoresSavedMetadataIntoRoomModel()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    settings.setValue(
        QStringLiteral("DouyuMonitor/nativeWorkspaceV1"),
        QByteArray(R"JSON({"version":2,"library":[{"roomId":"63136","metadata":{"roomId":"63136","anchorName":"已保存主播","title":"已保存标题","category":"游戏","viewerLabel":"1.2万"},"requestedQuality":"auto","favorite":false,"lastOpenedAtMs":0,"volume":100,"danmakuEnabled":true}],"groups":[],"activeRoomIds":["63136"],"activeGroupId":"","primaryRoomId":"63136","audioRoomId":"","presets":[]})JSON"));
    FakeNotificationSink sink;

    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.rooms()->rowCount(), 1);
    const QModelIndex room = controller.rooms()->index(0, 0);
    QCOMPARE(controller.rooms()->data(room, RoomListModel::AnchorNameRole).toString(),
             QStringLiteral("已保存主播"));
    QCOMPARE(controller.rooms()->data(room, RoomListModel::TitleRole).toString(),
             QStringLiteral("已保存标题"));
}

void AppControllerTest::persistsGroupPresetAndRoomPresentationSettings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("workspace.ini"));
    FakeNotificationSink sink;

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        AppController controller(fakeServicePath(), &settings, &sink);
        QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
        QVERIFY(controller.rooms()
                    ->data(controller.rooms()->index(0, 0), RoomListModel::DanmakuEnabledRole)
                    .toBool());
        const QString groupId = controller.createGroup(QStringLiteral("赛事"));
        QVERIFY(!groupId.isEmpty());
        QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63136"), groupId), QString());
        QCOMPARE(controller.setVolume(QStringLiteral("63136"), 35), QString());
        controller.toggleDanmaku(QStringLiteral("63136"));
        const QString presetId = controller.saveWorkspacePreset(QStringLiteral("比赛日"));
        QVERIFY(!presetId.isEmpty());
    }

    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    AppController restored(fakeServicePath(), &restoredSettings, &sink);
    QCOMPARE(restored.workspace()->groups().size(), 1);
    QCOMPARE(restored.workspace()->presets().size(), 1);
    QCOMPARE(restored.rooms()->data(restored.rooms()->index(0, 0), RoomListModel::VolumeRole).toInt(),
             35);
    QVERIFY(!restored.rooms()
                 ->data(restored.rooms()->index(0, 0), RoomListModel::DanmakuEnabledRole)
                 .toBool());
}

void AppControllerTest::restoresLayoutAndRatioFromWorkspacePreset()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63137")), QString());
    bool changed = false;
    QVERIFY(QMetaObject::invokeMethod(&controller,
                                      "setLayout",
                                      Q_RETURN_ARG(bool, changed),
                                      Q_ARG(QString, QStringLiteral("primary"))));
    QVERIFY(changed);
    QVERIFY(QMetaObject::invokeMethod(&controller,
                                      "setPrimaryRoomRatio",
                                      Q_RETURN_ARG(bool, changed),
                                      Q_ARG(double, 0.67)));
    QVERIFY(changed);
    const QString presetId = controller.saveWorkspacePreset(QStringLiteral("主画面"));
    QVERIFY(!presetId.isEmpty());

    QVERIFY(QMetaObject::invokeMethod(&controller,
                                      "setLayout",
                                      Q_RETURN_ARG(bool, changed),
                                      Q_ARG(QString, QStringLiteral("grid-3x3"))));
    QVERIFY(changed);
    QVERIFY(controller.applyWorkspacePreset(presetId).isEmpty());
    QCOMPARE(controller.workspace()->layoutId(), QStringLiteral("primary"));
    QCOMPARE(controller.workspace()->primaryRoomRatio(), 0.67);
}

void AppControllerTest::persistsGlobalAudioPolicy()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QVERIFY(controller.setAudioMode(QStringLiteral("multi")));
    QVERIFY(controller.setGlobalMuted(true));
    QCOMPARE(controller.workspace()->audioMode(), QStringLiteral("multi"));
    QVERIFY(controller.workspace()->globalMuted());
    QVERIFY(!controller.setAudioMode(QStringLiteral("unsupported")));
    QCOMPARE(controller.workspace()->audioMode(), QStringLiteral("multi"));
}

void AppControllerTest::preservesFavoriteWhenReaddingLibraryRoom()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.setFavorite(QStringLiteral("63136"), true), QString());
    QCOMPARE(controller.removeRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.libraryRooms().first().toMap().value(QStringLiteral("favorite")).toBool(), true);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.libraryRooms().first().toMap().value(QStringLiteral("favorite")).toBool(), true);
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(0, 0), RoomListModel::FavoriteRole).toBool(), true);
}

void AppControllerTest::ordersFavoritesByManualOrderAndMovesThem()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63137")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63138")), QString());
    QCOMPARE(controller.setFavorite(QStringLiteral("63136"), true), QString());
    QCOMPARE(controller.setFavorite(QStringLiteral("63137"), true), QString());

    QCOMPARE(controller.libraryRooms().at(0).toMap().value(QStringLiteral("roomId")).toString(),
             QStringLiteral("63136"));
    QCOMPARE(controller.libraryRooms().at(1).toMap().value(QStringLiteral("roomId")).toString(),
             QStringLiteral("63137"));
    QCOMPARE(controller.moveFavoriteRoom(QStringLiteral("63137"), 0), QString());
    QCOMPARE(controller.libraryRooms().at(0).toMap().value(QStringLiteral("roomId")).toString(),
             QStringLiteral("63137"));
    QCOMPARE(controller.libraryRooms().at(1).toMap().value(QStringLiteral("roomId")).toString(),
             QStringLiteral("63136"));
}

void AppControllerTest::appliesWorkspacePresetToAllRoomsRepeatedly()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63137")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63138")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63139")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63140")), QString());
    const QString presetId = controller.saveWorkspacePreset(QStringLiteral("五路"));
    QVERIFY(!presetId.isEmpty());

    QCOMPARE(controller.addRoom(QStringLiteral("63141")), QString());
    QCOMPARE(controller.applyWorkspacePreset(presetId), QString());
    QCOMPARE(controller.rooms()->rowCount(), 5);
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(0, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63136"));
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(1, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63137"));
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(2, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63138"));
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(3, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63139"));
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(4, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63140"));

    QCOMPARE(controller.applyWorkspacePreset(presetId), QString());
    QCOMPARE(controller.rooms()->rowCount(), 5);
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(0, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63136"));
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(1, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63137"));
}

void AppControllerTest::restoresPresetRoomsMissingFromLibrary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    settings.setValue(
        QStringLiteral("DouyuMonitor/nativeWorkspaceV1"),
        QByteArray(R"JSON({"version":2,"library":[{"roomId":"63136","metadata":{"roomId":"63136","anchorName":"主播 1","title":"标题 1","category":"游戏","viewerLabel":"1"},"requestedQuality":"auto","favorite":false,"lastOpenedAtMs":0,"volume":100,"danmakuEnabled":true}],"groups":[],"activeRoomIds":["63136"],"activeGroupId":"","primaryRoomId":"63136","audioRoomId":"63136","presets":[{"id":"p1","name":"五路预设","layoutId":"grid-3x2","activeGroupId":"","primaryRoomId":"63136","audioRoomId":"63136","roomIds":["63136","63137","63138","63139","63140"],"sidebarVisible":true,"danmakuEnabled":true}]})JSON"));
    FakeNotificationSink sink;

    AppController controller(fakeServicePath(), &settings, &sink);
    QCOMPARE(controller.workspace()->presets().size(), 1);
    QCOMPARE(controller.applyWorkspacePreset(QStringLiteral("p1")), QString());
    QCOMPARE(controller.rooms()->rowCount(), 5);
    QCOMPARE(controller.workspace()->primaryRoomId(), QStringLiteral("63136"));
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(4, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63140"));

    QCOMPARE(controller.removeRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.rooms()->rowCount(), 4);
    QCOMPARE(controller.workspace()->primaryRoomId(), QStringLiteral("63137"));
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(0, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63137"));
}

void AppControllerTest::deletesWorkspacePresetAndPersistsRemoval()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("workspace.ini"));
    FakeNotificationSink sink;

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        AppController controller(fakeServicePath(), &settings, &sink);
        QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
        const QString presetId = controller.saveWorkspacePreset(QStringLiteral("可删除"));
        QVERIFY(!presetId.isEmpty());
        QCOMPARE(controller.deleteWorkspacePreset(presetId), QString());
        QVERIFY(controller.workspace()->presets().isEmpty());
        QCOMPARE(controller.deleteWorkspacePreset(presetId), QStringLiteral("未找到该房间"));
    }

    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    AppController restored(fakeServicePath(), &restoredSettings, &sink);
    QVERIFY(restored.workspace()->presets().isEmpty());
}

void AppControllerTest::persistsNotificationPreferenceAndUpdatesMonitoringState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("workspace.ini"));
    FakeNotificationSink sink;

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        AppController controller(fakeServicePath(), &settings, &sink);
        QCOMPARE(controller.setNotificationsEnabled(false), QString());
        QCOMPARE(controller.monitoring()->notificationStatus(), QStringLiteral("disabled"));
    }

    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    AppController restored(fakeServicePath(), &restoredSettings, &sink);
    QCOMPARE(restored.monitoring()->notificationStatus(), QStringLiteral("disabled"));
}

void AppControllerTest::persistsNotificationEventPreferences()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("workspace.ini"));
    FakeNotificationSink sink;

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        AppController controller(fakeServicePath(), &settings, &sink);
        QCOMPARE(controller.setNotificationPreferences(true, false, true, false, true), QString());
        const QVariantMap preferences = controller.notificationPreferences();
        QCOMPARE(preferences.value(QStringLiteral("roomOnline")).toBool(), false);
        QCOMPARE(preferences.value(QStringLiteral("roomOffline")).toBool(), true);
        QCOMPARE(preferences.value(QStringLiteral("playbackFailed")).toBool(), false);
        QCOMPARE(preferences.value(QStringLiteral("playbackRecovered")).toBool(), true);
    }

    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    AppController restored(fakeServicePath(), &restoredSettings, &sink);
    const QVariantMap restoredPreferences = restored.notificationPreferences();
    QCOMPARE(restoredPreferences.value(QStringLiteral("roomOnline")).toBool(), false);
    QCOMPARE(restoredPreferences.value(QStringLiteral("roomOffline")).toBool(), true);
    QCOMPARE(restoredPreferences.value(QStringLiteral("playbackFailed")).toBool(), false);
    QCOMPARE(restoredPreferences.value(QStringLiteral("playbackRecovered")).toBool(), true);
}

void AppControllerTest::synchronizesDanmakuFromGlobalRoomLiveAndActiveState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    FakeDanmakuFactoryState state;
    const DanmakuClientFactory factory = [&state](const QString &roomId, QObject *parent) {
        state.rooms.push_back(roomId);
        return std::unique_ptr<DanmakuClient>(new FakeDanmakuClient(roomId, &state, parent));
    };
    AppController controller(fakeServicePath(), &settings, &sink, factory);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(state.starts, 1, 3000);
    QCOMPARE(controller.danmaku()->activeSessionCountForTest(), 1);

    controller.danmaku()->setGlobalEnabled(false);
    QTRY_COMPARE_WITH_TIMEOUT(state.stops, 1, 1000);
    QCOMPARE(controller.danmaku()->activeSessionCountForTest(), 0);

    controller.danmaku()->setGlobalEnabled(true);
    QTRY_COMPARE_WITH_TIMEOUT(state.starts, 2, 1000);
    controller.toggleDanmaku(QStringLiteral("63136"));
    QTRY_COMPARE_WITH_TIMEOUT(state.stops, 2, 1000);
    QCOMPARE(controller.danmaku()->activeSessionCountForTest(), 0);

    QCOMPARE(controller.removeRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.danmaku()->activeSessionCountForTest(), 0);
}

void AppControllerTest::stopsDanmakuBeforeServiceShutdown()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    FakeDanmakuFactoryState state;
    const DanmakuClientFactory factory = [&state](const QString &roomId, QObject *parent) {
        return std::unique_ptr<DanmakuClient>(new FakeDanmakuClient(roomId, &state, parent));
    };
    auto controller = std::make_unique<AppController>(fakeServicePath(), &settings, &sink, factory);
    QCOMPARE(controller->addRoom(QStringLiteral("63136")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(state.starts, 1, 3000);
    controller->shutdown();
    QCOMPARE(controller->danmaku()->activeSessionCountForTest(), 0);
    QVERIFY(state.stops >= 1);
}

void AppControllerTest::activatesGroupAndReplacesActiveRoomSet()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63137")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63138")), QString());
    const QString groupId = controller.createGroup(QStringLiteral("赛事"));
    QVERIFY(!groupId.isEmpty());
    QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63138"), groupId), QString());
    QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63136"), groupId), QString());

    controller.setActiveGroup(groupId);
    QCOMPARE(controller.rooms()->rowCount(), 2);
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(0, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63138"));
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(1, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63136"));
    QCOMPARE(controller.workspace()->groupItems().first().toMap().value(QStringLiteral("active")).toBool(),
             true);
}

void AppControllerTest::managesGroupMembersAndOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63137")), QString());
    const QString groupId = controller.createGroup(QStringLiteral("管理"));
    QVERIFY(!groupId.isEmpty());
    QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63136"), groupId), QString());
    QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63137"), groupId), QString());
    QCOMPARE(controller.moveRoomInGroup(groupId, QStringLiteral("63137"), -1), QString());
    QCOMPARE(controller.workspace()->groups().first().roomIds,
             QStringList({QStringLiteral("63137"), QStringLiteral("63136")}));
    QCOMPARE(controller.removeRoomFromGroup(groupId, QStringLiteral("63137")), QString());
    QCOMPARE(controller.workspace()->groups().first().roomIds,
             QStringList({QStringLiteral("63136")}));
}

void AppControllerTest::allowsRoomMembershipInMultipleGroupsAndEnforcesCapacity()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    const int maxRooms = RoomCapacity::currentLimits().maxLayoutRooms;
    QJsonArray library;
    for (int index = 0; index < maxRooms + 1; ++index) {
        const QString roomId = QString::number(63136 + index);
        library.append(QJsonObject{{"roomId", roomId},
                                   {"metadata", QJsonObject{{"roomId", roomId},
                                                             {"anchorName", roomId},
                                                             {"title", ""},
                                                             {"category", ""},
                                                             {"viewerLabel", ""}}},
                                   {"requestedQuality", "auto"},
                                   {"favorite", false},
                                   {"lastOpenedAtMs", 0},
                                   {"volume", 100},
                                   {"danmakuEnabled", false}});
    }
    settings.setValue(QStringLiteral("DouyuMonitor/nativeWorkspaceV1"),
                      QJsonDocument(QJsonObject{{"version", 2},
                                                {"library", library},
                                                {"groups", QJsonArray{}},
                                                {"activeRoomIds", QJsonArray{}},
                                                {"activeGroupId", ""},
                                                {"primaryRoomId", ""},
                                                {"audioRoomId", ""},
                                                {"presets", QJsonArray{}}})
                          .toJson(QJsonDocument::Compact));
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    for (int index = 0; index < maxRooms; ++index) {
        QCOMPARE(controller.addRoom(QString::number(63136 + index)), QString());
    }
    const QString firstGroup = controller.createGroup(QStringLiteral("A"));
    const QString secondGroup = controller.createGroup(QStringLiteral("B"));
    for (int index = 0; index < maxRooms; ++index) {
        QCOMPARE(controller.assignRoomToGroup(QString::number(63136 + index), firstGroup), QString());
    }
    QCOMPARE(controller.workspace()->groups().at(0).roomIds.size(), maxRooms);
    const QString overflowRoomId = QString::number(63136 + maxRooms);
    QCOMPARE(controller.assignRoomToGroup(overflowRoomId, firstGroup),
             QStringLiteral("分组最多包含 %1 个房间").arg(maxRooms));
    QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63136"), secondGroup), QString());
    QCOMPARE(controller.workspace()->groups().at(0).roomIds.size(), maxRooms);
    QCOMPARE(controller.workspace()->groups().at(1).roomIds,
             QStringList({QStringLiteral("63136")}));
}

void AppControllerTest::assignsLibraryRoomToActiveGroupAfterSwitch()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63137")), QString());
    QCOMPARE(controller.addRoom(QStringLiteral("63138")), QString());
    const QString groupId = controller.createGroup(QStringLiteral("活动"));
    QVERIFY(!groupId.isEmpty());
    QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63136"), groupId), QString());
    controller.setActiveGroup(groupId);

    QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63138"), groupId), QString());
    QCOMPARE(controller.workspace()->groups().first().roomIds,
             QStringList({QStringLiteral("63136"), QStringLiteral("63138")}));
    QCOMPARE(controller.rooms()->rowCount(), 2);
    QCOMPARE(controller.rooms()->data(controller.rooms()->index(1, 0), RoomListModel::RoomIdRole)
                 .toString(),
             QStringLiteral("63138"));
}

void AppControllerTest::restoresActiveGroupMembershipInOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("workspace.ini"));
    FakeNotificationSink sink;
    QString groupId;
    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        AppController controller(fakeServicePath(), &settings, &sink);
        QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
        QCOMPARE(controller.addRoom(QStringLiteral("63137")), QString());
        groupId = controller.createGroup(QStringLiteral("恢复"));
        QVERIFY(!groupId.isEmpty());
        QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63137"), groupId), QString());
        QCOMPARE(controller.assignRoomToGroup(QStringLiteral("63136"), groupId), QString());
        controller.setActiveGroup(groupId);
    }

    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    AppController restored(fakeServicePath(), &restoredSettings, &sink);
    const QVariantList restoredGroups = restored.workspace()->groupItems();
    QVERIFY(!restoredGroups.isEmpty());
    QCOMPARE(restoredGroups.first().toMap().value(QStringLiteral("active"))
                 .toBool(),
             true);
    QCOMPARE(restored.rooms()->rowCount(), 2);
    QCOMPARE(restored.rooms()->data(restored.rooms()->index(0, 0), RoomListModel::RoomIdRole)
                 .toString(),
             QStringLiteral("63137"));
    QCOMPARE(restored.rooms()->data(restored.rooms()->index(1, 0), RoomListModel::RoomIdRole)
                 .toString(),
             QStringLiteral("63136"));
}

void AppControllerTest::searchesRoomCandidatesAndAddsMetadata()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    controller.searchRooms(QStringLiteral("https://www.douyu.com/63136"));
    QTRY_COMPARE_WITH_TIMEOUT(controller.searchStatus(), QStringLiteral("success"), 3000);
    QCOMPARE(controller.searchResults().size(), 1);
    const QVariantMap candidate = controller.searchResults().first().toMap();
    QCOMPARE(candidate.value(QStringLiteral("roomId")).toString(), QStringLiteral("63136"));
    QCOMPARE(candidate.value(QStringLiteral("anchorName")).toString(), QStringLiteral("Fake Anchor"));

    QCOMPARE(controller.addRoomCandidate(QStringLiteral("63136")), QString());
    QCOMPARE(controller.rooms()->rowCount(), 1);
    const QModelIndex room = controller.rooms()->index(0, 0);
    QCOMPARE(controller.rooms()->data(room, RoomListModel::AnchorNameRole).toString(),
             QStringLiteral("Fake Anchor"));
    QCOMPARE(controller.rooms()->data(room, RoomListModel::TitleRole).toString(),
             QStringLiteral("Fake Room"));
}

void AppControllerTest::projectsRefreshedRoomMetadataAndStatus()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms()->rowCount(), 1, 3000);
    QTRY_COMPARE_WITH_TIMEOUT(
        controller.rooms()->data(controller.rooms()->index(0, 0), RoomListModel::LiveStateRole)
            .toString(),
        QStringLiteral("online"),
        3000);

    const QModelIndex room = controller.rooms()->index(0, 0);
    QTRY_COMPARE_WITH_TIMEOUT(
        controller.rooms()->data(room, RoomListModel::AnchorNameRole).toString(),
        QStringLiteral("Fake Anchor"),
        3000);
    QCOMPARE(controller.rooms()->data(room, RoomListModel::TitleRole).toString(),
             QStringLiteral("Fake Room"));
    QCOMPARE(controller.rooms()->data(room, RoomListModel::ViewerLabelRole).toString(),
             QStringLiteral("1,234"));
    QCOMPARE(controller.rooms()->data(room, RoomListModel::AvatarUrlRole).toUrl(),
             QUrl(QStringLiteral("https://example.invalid/avatar.jpg")));
    QTest::qWait(150);
    QCOMPARE(controller.workspace()->lastMessage(), QString());
}

void AppControllerTest::monitorsInactiveFavoriteRoomAndUpdatesLibrary()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    settings.setValue(
        QStringLiteral("DouyuMonitor/nativeWorkspaceV1"),
        QByteArray(R"JSON({"version":2,"library":[{"roomId":"63136","metadata":{"roomId":"63136","anchorName":"旧主播","title":"旧标题"},"requestedQuality":"auto","favorite":true,"favoriteAddedAtMs":1,"favoriteSortOrder":1,"lastOpenedAtMs":0,"volume":100,"danmakuEnabled":true}],"groups":[],"activeRoomIds":[],"activeGroupId":"","primaryRoomId":"","audioRoomId":"","presets":[]})JSON"));
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QTRY_VERIFY_WITH_TIMEOUT([&] {
        const QVariantList rooms = controller.libraryRooms();
        if (rooms.size() != 1) return false;
        const QVariantMap room = rooms.first().toMap();
        return room.value(QStringLiteral("anchorName")).toString() == QStringLiteral("Fake Anchor")
            && room.value(QStringLiteral("title")).toString() == QStringLiteral("Fake Room")
            && room.value(QStringLiteral("online")).toBool();
    }(), 3000);
    QVERIFY(!controller.libraryRooms().first().toMap().value(QStringLiteral("active")).toBool());
    QCOMPARE(sink.shownCount, 0);
}

void AppControllerTest::publishesCommandFailureToToast()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.retryPlayback(QStringLiteral("999")), QStringLiteral("未找到该房间"));
    QCOMPARE(controller.workspace()->lastMessage(), QStringLiteral("未找到该房间"));
}

void AppControllerTest::createsEmptyTeamsAndAssignsRosterMembers()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    const QString first = controller.createTeam(QStringLiteral("一队"));
    const QString second = controller.createTeam(QStringLiteral("二队"));
    const QString third = controller.createTeam(QStringLiteral("三队"));
    const QString fourth = controller.createTeam(QStringLiteral("四队"));
    QVERIFY(!first.isEmpty());
    QVERIFY(!second.isEmpty());
    QVERIFY(!third.isEmpty());
    QVERIFY(!fourth.isEmpty());

    QCOMPARE(controller.assignGuildMemberToTeam(QStringLiteral("hamster-001"), first), QString());
    QCOMPARE(controller.workspace()->teams().size(), 4);
    QCOMPARE(controller.workspace()->teams().first().memberIds,
             QStringList({QStringLiteral("hamster-001")}));
    QCOMPARE(controller.workspace()->teams().at(1).memberIds, QStringList{});
    QCOMPARE(controller.workspace()->teams().at(3).memberIds, QStringList{});
}

void AppControllerTest::movesMemberBetweenTeamsAndLeavesEmptySlots()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    const QString first = controller.createTeam(QStringLiteral("一队"));
    const QString second = controller.createTeam(QStringLiteral("二队"));
    QCOMPARE(controller.assignGuildMemberToTeam(QStringLiteral("hamster-001"), first), QString());
    QCOMPARE(controller.assignGuildMemberToTeam(QStringLiteral("hamster-001"), second), QString());

    QCOMPARE(controller.workspace()->teams().at(0).memberIds, QStringList{});
    QCOMPARE(controller.workspace()->teams().at(1).memberIds,
             QStringList({QStringLiteral("hamster-001")}));
}

void AppControllerTest::managesTeamOrderWithoutChangingGroupsOrRooms()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addRoom(QStringLiteral("63136")), QString());
    const QString groupId = controller.createGroup(QStringLiteral("旧分组"));
    const QString first = controller.createTeam(QStringLiteral("一队"));
    const QString second = controller.createTeam(QStringLiteral("二队"));
    QCOMPARE(controller.assignGuildMemberToTeam(QStringLiteral("hamster-001"), first), QString());
    QCOMPARE(controller.assignGuildMemberToTeam(QStringLiteral("hamster-002"), first), QString());

    QCOMPARE(controller.moveTeam(first, 1), QString());
    QCOMPARE(controller.workspace()->teams().at(0).id, second);
    QCOMPARE(controller.workspace()->teams().at(1).id, first);
    QCOMPARE(controller.renameTeam(first, QStringLiteral("一队改名")), QString());
    QCOMPARE(controller.workspace()->teams().at(1).name, QStringLiteral("一队改名"));
    QCOMPARE(controller.removeGuildMemberFromTeam(first, QStringLiteral("hamster-002")), QString());
    QCOMPARE(controller.workspace()->teams().at(1).memberIds,
             QStringList({QStringLiteral("hamster-001")}));
    QCOMPARE(controller.deleteTeam(second), QString());
    QCOMPARE(controller.workspace()->teams().size(), 1);

    QCOMPARE(controller.workspace()->groups().size(), 1);
    QCOMPARE(controller.workspace()->groups().first().id, groupId);
    QCOMPARE(controller.rooms()->rowCount(), 1);
    QCOMPARE(controller.workspace()->primaryRoomId(), QStringLiteral("63136"));
}

void AppControllerTest::rejectsDuplicateTeamNames()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    const QString first = controller.createTeam(QStringLiteral("一队"));
    const QString second = controller.createTeam(QStringLiteral("二队"));
    QVERIFY(!first.isEmpty());
    QVERIFY(!second.isEmpty());

    QCOMPARE(controller.createTeam(QStringLiteral(" 一队 ")),
             QStringLiteral("已存在同名队伍"));
    QCOMPARE(controller.workspace()->teams().size(), 2);
    QCOMPARE(controller.renameTeam(second, QStringLiteral("一队")),
             QStringLiteral("已存在同名队伍"));
    QCOMPARE(controller.workspace()->teams().at(1).name, QStringLiteral("二队"));
    QCOMPARE(controller.renameTeam(second, QStringLiteral(" 二队 ")), QString());
    QCOMPARE(controller.workspace()->teams().at(1).name, QStringLiteral("二队"));
}

void AppControllerTest::persistsTeamDeletion()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("workspace.ini"));
    FakeNotificationSink sink;
    QString retainedId;

    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        AppController controller(fakeServicePath(), &settings, &sink);
        const QString removedId = controller.createTeam(QStringLiteral("待删除队伍"));
        retainedId = controller.createTeam(QStringLiteral("保留队伍"));
        QVERIFY(!removedId.isEmpty());
        QVERIFY(!retainedId.isEmpty());

        QCOMPARE(controller.deleteTeam(removedId), QString());
        QCOMPARE(controller.workspace()->teams().size(), 1);
    }

    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    AppController restored(fakeServicePath(), &restoredSettings, &sink);
    QCOMPARE(restored.workspace()->teams().size(), 1);
    QCOMPARE(restored.workspace()->teams().first().id, retainedId);
}
void AppControllerTest::enforcesTeamLimitsAndRosterMembership()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.createTeam(QString(31, QLatin1Char('x'))),
             QStringLiteral("请输入 1 到 30 个字符的队伍名称"));
    QString first;
    for (int index = 0; index < 20; ++index) {
        const QString teamId = controller.createTeam(QStringLiteral("队伍 %1").arg(index));
        QVERIFY(!teamId.isEmpty());
        if (index == 0) first = teamId;
    }
    QCOMPARE(controller.createTeam(QStringLiteral("第二十一队")),
             QStringLiteral("最多创建 20 个队伍"));
    QCOMPARE(controller.assignGuildMemberToTeam(QStringLiteral("unknown-member"), first),
             QStringLiteral("未找到该公会主播"));
    QCOMPARE(controller.assignGuildMemberToTeam(QStringLiteral("hamster-001"),
                                                QStringLiteral("unknown-team")),
             QStringLiteral("未找到该队伍"));
    QCOMPARE(controller.removeGuildMemberFromTeam(QStringLiteral("unknown-team"),
                                                  QStringLiteral("hamster-001")),
             QStringLiteral("未找到该队伍"));
}

void AppControllerTest::defersGuildChecksUntilNavigationOpens()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath("workspace.ini"), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);
    QCOMPARE(controller.rooms()->rowCount(), 0);
    QVERIFY(!controller.workspace()->navigationVisible());
    QTest::qWait(100);
    QVERIFY2(!controller.serviceProcessRunningForTest(),
             "Empty startup launched the service solely to check hidden navigation");
    QCOMPARE(controller.setNavigationVisible(true), QString());
    QTRY_VERIFY(controller.serviceProcessRunningForTest());
    controller.requestQuit();
}

void AppControllerTest::persistsIndependentTeamsAndNavigationVisibility()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString settingsPath = directory.filePath(QStringLiteral("workspace.ini"));
    FakeNotificationSink sink;
    QString teamId;
    {
        QSettings settings(settingsPath, QSettings::IniFormat);
        AppController controller(fakeServicePath(), &settings, &sink);
        QCOMPARE(controller.guildRoster().size(), 58);
        const QVariantMap firstMember = controller.guildRoster().first().toMap();
        QCOMPARE(firstMember.value(QStringLiteral("id")).toString(),
                 QStringLiteral("hamster-001"));
        QCOMPARE(firstMember.value(QStringLiteral("anchorName")).toString(),
                 QStringLiteral("寅子"));
        QCOMPARE(firstMember.value(QStringLiteral("roomId")).toString(),
                 QStringLiteral("71415"));
        QCOMPARE(firstMember.value(QStringLiteral("role")).toString(),
                 QStringLiteral("other"));

        teamId = controller.createTeam(QStringLiteral("持久化队伍"));
        QCOMPARE(controller.assignGuildMemberToTeam(QStringLiteral("hamster-001"), teamId),
                 QString());
        QCOMPARE(controller.setNavigationVisible(true), QString());
    }

    QSettings restoredSettings(settingsPath, QSettings::IniFormat);
    AppController restored(fakeServicePath(), &restoredSettings, &sink);
    QCOMPARE(restored.workspace()->teams().size(), 1);
    QCOMPARE(restored.workspace()->teams().first().id, teamId);
    QCOMPARE(restored.workspace()->teams().first().memberIds,
             QStringList({QStringLiteral("hamster-001")}));
    QVERIFY(restored.workspace()->navigationVisible());
}

void AppControllerTest::addsResolvedGuildMemberWithoutUsingOrdinarySearchState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QSignalSpy rosterChanged(&controller, &AppController::guildRosterChanged);
    QCOMPARE(controller.setGuildMemberRoomId(QStringLiteral("hamster-002"),
                                             QStringLiteral("84452")),
             QString());
    rosterChanged.clear();
    QCOMPARE(controller.addGuildMemberRoom(QStringLiteral("hamster-002")), QString());
    QCOMPARE(controller.rooms()->rowCount(), 1);
    QCOMPARE(controller.rooms()
                 ->data(controller.rooms()->index(0, 0), RoomListModel::RoomIdRole)
                 .toString(),
             QStringLiteral("84452"));
    QCOMPARE(controller.rooms()
                 ->data(controller.rooms()->index(0, 0), RoomListModel::AnchorNameRole)
                 .toString(),
             QStringLiteral("主播阿飞"));
    QTRY_VERIFY_WITH_TIMEOUT(rosterChanged.count() > 0, 3000);
    bool active = false;
    for (const QVariant &value : controller.guildRoster()) {
        const QVariantMap entry = value.toMap();
        if (entry.value(QStringLiteral("id")).toString() == QStringLiteral("hamster-002")) {
            active = entry.value(QStringLiteral("active")).toBool();
        }
    }
    QVERIFY(active);
}

void AppControllerTest::rejectsGuildQuickAddWithoutRoomOrCapacity()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.addGuildMemberRoom(QStringLiteral("hamster-005")), QString());
    QCOMPARE(controller.rooms()->rowCount(), 1);
    QCOMPARE(controller.removeRoom(QStringLiteral("217331")), QString());
    QCOMPARE(controller.setGuildMemberRoomId(QStringLiteral("hamster-002"),
                                             QStringLiteral("84452")),
             QString());
    QCOMPARE(controller.addGuildMemberRoom(QStringLiteral("hamster-002")), QString());
    QCOMPARE(controller.addGuildMemberRoom(QStringLiteral("hamster-002")),
             QStringLiteral("该房间已在列表中"));
}

void AppControllerTest::enforcesGuildQuickAddLayoutCapacity()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    const int maxRooms = RoomCapacity::currentLimits().maxLayoutRooms;
    for (int index = 0; index < maxRooms; ++index) {
        QCOMPARE(controller.addRoom(QString::number(63136 + index)), QString());
    }
    QCOMPARE(controller.addGuildMemberRoom(QStringLiteral("hamster-006")),
             QStringLiteral("最多添加 %1 个房间").arg(maxRooms));
    QCOMPARE(controller.rooms()->rowCount(), maxRooms);
}
QTEST_GUILESS_MAIN(AppControllerTest)

#include "app_controller_test.moc"
