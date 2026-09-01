#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickWindow>
#include <QQuickItem>
#include <QPointF>
#include <QSize>
#include <QtTest/QtTest>

#include "ui/mpv_quick_item.h"
#include "ui/room_list_model.h"
#include "ui/workspace_model.h"

#include <memory>

namespace {

class FakeGroupController final : public QObject {
    Q_OBJECT

public:
    Q_INVOKABLE void setActiveGroup(const QString &groupId) { activatedGroups.push_back(groupId); }
    Q_INVOKABLE QString removeRoomFromGroup(const QString &groupId, const QString &roomId)
    {
        removedMembers.push_back(groupId + QStringLiteral(":") + roomId);
        return {};
    }
    Q_INVOKABLE QString moveRoomInGroup(const QString &groupId,
                                        const QString &roomId,
                                        int delta)
    {
        movedMembers.push_back(groupId + QStringLiteral(":") + roomId + QStringLiteral(":")
                             + QString::number(delta));
        return {};
    }

    QStringList activatedGroups;
    QStringList removedMembers;
    QStringList movedMembers;
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
        }};
    }

    QString searchStatus() const { return QStringLiteral("success"); }
    QString searchError() const { return {}; }
    Q_INVOKABLE void searchRooms(const QString &) {}
    Q_INVOKABLE QString addRoomCandidate(const QString &roomId)
    {
        addedRoomId = roomId;
        return {};
    }

    QString addedRoomId;
};

class FakeHeaderController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(WorkspaceModel *workspace READ workspace CONSTANT)

public:
    FakeHeaderController()
        : workspaceModel(nullptr, this)
    {
        workspaceModel.setAudioPolicy(QStringLiteral("single"), false);
    }

    WorkspaceModel *workspace() noexcept { return &workspaceModel; }

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

    WorkspaceModel workspaceModel;
    bool lastMuted = false;
    QString lastAudioMode;
    bool fullScreenToggled = false;
};

class FakeRoomController final : public QObject {
    Q_OBJECT

public:
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

    QString lastVolumeRoom;
    int lastVolume = -1;
    QString refreshedRoom;
    QString movedRoom;
    int movedDelta = 0;
    QString removedRoom;
    QStringList attachedRooms;
    QStringList detachedRooms;
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
        {QStringLiteral("effectiveQuality"), QStringLiteral("auto")},
        {QStringLiteral("availableQualities"), QVariantList{
            QVariantMap{{QStringLiteral("id"), QStringLiteral("auto")},
                        {QStringLiteral("label"), QStringLiteral("自动")},
                        {QStringLiteral("quality"), QStringLiteral("auto")}},
            QVariantMap{{QStringLiteral("id"), QStringLiteral("high")},
                        {QStringLiteral("label"), QStringLiteral("高清")},
                        {QStringLiteral("quality"), QStringLiteral("high")}},
        }},
        {QStringLiteral("muted"), true},
        {QStringLiteral("volume"), 100},
        {QStringLiteral("danmakuEnabled"), true},
        {QStringLiteral("danmakuState"), danmakuState},
        {QStringLiteral("danmakuErrorCode"), QStringLiteral("NONE")},
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

} // namespace

class QmlInteractionTest final : public QObject {
    Q_OBJECT

private slots:
    void opensAddRoomDialogAndRejectsInvalidRoomId();
    void opensMonitoringDanmakuAndWorkspaceSurfaces();
    void togglesSidebarAndHandlesRetainedShortcuts();
    void handlesLegacyShortcutParity();
    void rendersOnlineStatusForOnlineToken();
    void showsDanmakuDisplayGovernanceAndStatsTabs();
    void showsDanmakuStatusOnRoomTile();
    void rendersRoomAvatarFromModelRole();
    void placesSidebarToggleBeforeBrandAndOpensHistory();
    void closesToastFromQml();
    void autoDismissesToastBySeverity();
    void usesFramelessWindowWithTitleBarInteractions();
    void rendersAndActivatesGroupTabs();
    void exposesGroupMemberManagementControls();
    void rendersAndAddsSearchCandidate();
    void exposesOnlyAutomaticAndPrimaryLayoutOptions();
    void laysOutFiveAutomaticRoomsInThreeAndTwoRows();
    void laysOutPrimaryRoomsAcrossFullHeight();
    void exposesGlobalAudioControls();
    void distinguishesWorkspaceAndLayoutActions();
    void groupsSoundControlsAndExposesFullscreen();
    void positionsSoundPopoverLikeDanmakuPanel();
    void truncatesLongRoomTitleBeforeActions();
    void exposesRoomVolumeAndRefreshControls();
    void defersRoomRemovalUntilAfterQmlHandlerReturns();
    void rebindsPlayerWhenRoomIdentityChanges();
    void rendersFallbackMetadataAndUnknownStatus();
    void exposesRoomOrderingControls();
    void disablesOrderingAtListBoundaries();
    void exposesRoomDragAndDropSurface();
    void refreshesRoomSidebarAfterPresetLikeModelUpdate();
};

void QmlInteractionTest::opensAddRoomDialogAndRejectsInvalidRoomId()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(window->isVisible());

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

void QmlInteractionTest::togglesSidebarAndHandlesRetainedShortcuts()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(window->isVisible());

    QObject *sidebar = window->findChild<QObject *>(QStringLiteral("roomSidebar"));
    QVERIFY(sidebar != nullptr);
    QVERIFY(sidebar->property("visible").toBool());
    click(window->findChild<QObject *>(QStringLiteral("sidebarToggleButton")));
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
    QVERIFY(sidebar->property("visible").toBool());
    press(Qt::Key_S);
    QTRY_VERIFY(!sidebar->property("visible").toBool());
    press(Qt::Key_S);
    QTRY_VERIFY(sidebar->property("visible").toBool());

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

void QmlInteractionTest::showsDanmakuStatusOnRoomTile()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomTile.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    std::unique_ptr<QObject> connected(
        component.createWithInitialProperties(roomTileProperties(QStringLiteral("connected"))));
    QVERIFY2(connected != nullptr, qPrintable(component.errorString()));
    QObject *indicator = connected->findChild<QObject *>(QStringLiteral("danmakuConnectedIndicator"));
    QVERIFY(indicator != nullptr);
    QVERIFY(indicator->property("visible").toBool());

    std::unique_ptr<QObject> blocked(
        component.createWithInitialProperties(roomTileProperties(QStringLiteral("platform-blocked"))));
    QVERIFY2(blocked != nullptr, qPrintable(component.errorString()));
    QObject *retry = blocked->findChild<QObject *>(QStringLiteral("danmakuRetryAction"));
    QVERIFY(retry != nullptr);
    QVERIFY(retry->property("visible").toBool());
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
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(window->isVisible());

    QObject *toggle = window->findChild<QObject *>(QStringLiteral("sidebarToggleButton"));
    QObject *brandMark = window->findChild<QObject *>(QStringLiteral("brandMark"));
    QVERIFY(toggle != nullptr);
    QVERIFY(brandMark != nullptr);
    QVERIFY(toggle->property("x").toDouble() < brandMark->property("x").toDouble());

    click(window->findChild<QObject *>(QStringLiteral("historyTab")));
    QObject *libraryView = window->findChild<QObject *>(QStringLiteral("roomLibraryView"));
    QVERIFY(libraryView != nullptr);
    QTRY_VERIFY(libraryView->property("visible").toBool());
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

void QmlInteractionTest::rendersAndActivatesGroupTabs()
{
    registerQmlTypes();
    WorkspaceModel workspace(nullptr);
    workspace.setWorkspaceData(
        {{QStringLiteral("group-a"), QStringLiteral("赛事"), {QStringLiteral("63136")} },
         {QStringLiteral("group-b"), QStringLiteral("关注"), {QStringLiteral("63137")} }},
        {},
        QStringLiteral("group-a"));
    FakeGroupController controller;
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/RoomSidebar.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> sidebar(component.createWithInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("workspaceModel"),
         QVariant::fromValue(static_cast<QObject *>(&workspace))},
        {QStringLiteral("roomModel"), QVariantList{}},
    }));
    QVERIFY2(sidebar != nullptr, qPrintable(component.errorString()));

    QObject *tabs = sidebar->findChild<QObject *>(QStringLiteral("groupTabs"));
    QVERIFY(tabs != nullptr);
    QObject *repeater = sidebar->findChild<QObject *>(QStringLiteral("groupTabRepeater"));
    QVERIFY(repeater != nullptr);
    QCOMPARE(repeater->property("count").toInt(), 2);
    QQuickItem *firstGroupTabItem = nullptr;
    QVERIFY(QMetaObject::invokeMethod(repeater,
                                      "itemAt",
                                      Q_RETURN_ARG(QQuickItem *, firstGroupTabItem),
                                      Q_ARG(int, 0)));
    QObject *firstGroupTab = firstGroupTabItem;
    QVERIFY(firstGroupTab != nullptr);
    QVERIFY(QMetaObject::invokeMethod(firstGroupTab, "clicked"));
    QCOMPARE(controller.activatedGroups, QStringList({QStringLiteral("group-a")}));
}

void QmlInteractionTest::exposesGroupMemberManagementControls()
{
    registerQmlTypes();
    WorkspaceModel workspace(nullptr);
    workspace.setWorkspaceData(
        {{QStringLiteral("group-a"), QStringLiteral("赛事"),
          {QStringLiteral("63136"), QStringLiteral("63137")} }},
        {},
        {});
    FakeGroupController controller;
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qml/dialogs/GroupManagerDialog.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> dialog(component.createWithInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("workspaceModel"),
         QVariant::fromValue(static_cast<QObject *>(&workspace))},
        {QStringLiteral("selectedGroupId"), QStringLiteral("group-a")},
    }));
    QVERIFY2(dialog != nullptr, qPrintable(component.errorString()));

    QObject *memberList = dialog->findChild<QObject *>(QStringLiteral("groupMemberList"));
    QVERIFY(memberList != nullptr);
    QCOMPARE(memberList->property("count").toInt(), 2);
    QQuickItem *firstMemberRowItem = nullptr;
    QVERIFY(QMetaObject::invokeMethod(memberList,
                                      "itemAt",
                                      Q_RETURN_ARG(QQuickItem *, firstMemberRowItem),
                                      Q_ARG(double, 1.0),
                                      Q_ARG(double, 1.0)));
    QObject *firstMemberRow = firstMemberRowItem;
    QVERIFY(firstMemberRow != nullptr);
    QVERIFY(firstMemberRow->findChild<QObject *>(QStringLiteral("removeGroupMemberButton")) != nullptr);
    QVERIFY(firstMemberRow->findChild<QObject *>(QStringLiteral("moveGroupMemberUpButton")) != nullptr);
    QVERIFY(firstMemberRow->findChild<QObject *>(QStringLiteral("moveGroupMemberDownButton")) != nullptr);
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
    QVERIFY(QMetaObject::invokeMethod(dialog.get(), "open"));
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

void QmlInteractionTest::exposesOnlyAutomaticAndPrimaryLayoutOptions()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QVERIFY(window->isVisible());

    QObject *layoutButton = window->findChild<QObject *>(QStringLiteral("layoutMenuButton"));
    QVERIFY(layoutButton != nullptr);
    click(layoutButton);
    QObject *layoutMenu = window->findChild<QObject *>(QStringLiteral("layoutMenu"));
    QVERIFY(layoutMenu != nullptr);
    QTRY_VERIFY(layoutMenu->property("visible").toBool());

    QVERIFY(window->findChild<QObject *>(QStringLiteral("autoLayoutOption")) != nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("primaryLayoutOption")) != nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("singleLayoutOption")) == nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("grid2LayoutOption")) == nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("grid3LayoutOption")) == nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("grid3x3LayoutOption")) == nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("primaryTwoLayoutOption")) == nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("splitHorizontalLayoutOption")) == nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("splitVerticalLayoutOption")) == nullptr);
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
    QTRY_COMPARE_WITH_TIMEOUT(toast->property("message").toString(), QString(), 500);
}

void QmlInteractionTest::exposesGlobalAudioControls()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/AppHeader.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    FakeHeaderController controller;
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(800, 120));
    hostWindow.show();
    std::unique_ptr<QObject> header(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("width"), 800},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
    }));
    QVERIFY2(header != nullptr, qPrintable(component.errorString()));

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
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);
    QObject *workspace = window->findChild<QObject *>(QStringLiteral("workspaceButton"));
    QObject *layout = window->findChild<QObject *>(QStringLiteral("layoutMenuButton"));
    QVERIFY(workspace != nullptr);
    QVERIFY(layout != nullptr);
    QObject *workspaceIcon = window->findChild<QObject *>(QStringLiteral("workspacePresetIcon"));
    QObject *layoutIcon = window->findChild<QObject *>(QStringLiteral("layoutMenuIcon"));
    QVERIFY(workspaceIcon != nullptr);
    QVERIFY(layoutIcon != nullptr);
    QVERIFY(workspaceIcon->property("source").toUrl()
                != layoutIcon->property("source").toUrl());
    QVERIFY(!window->findChild<QObject *>(QStringLiteral("nativeQtWorkspaceSubtitle")));
}

void QmlInteractionTest::groupsSoundControlsAndExposesFullscreen()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/AppHeader.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    FakeHeaderController controller;
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(800, 320));
    hostWindow.show();
    std::unique_ptr<QObject> header(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("width"), 800},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
    }));
    QVERIFY2(header != nullptr, qPrintable(component.errorString()));
    QObject *sound = header->findChild<QObject *>(QStringLiteral("soundMasterButton"));
    QObject *popover = header->findChild<QObject *>(QStringLiteral("soundMasterPopover"));
    QVERIFY(sound != nullptr);
    QVERIFY(popover != nullptr);
    QVERIFY(!popover->property("visible").toBool());
    click(sound);
    QTRY_VERIFY(popover->property("visible").toBool());
    const QPointF popupAnchor = popover->property("soundAnchor").toPointF();
    qInfo() << "sound anchor" << popupAnchor << "header height" << header->property("height")
            << "sound height" << sound->property("height");
    QVERIFY(popupAnchor.x() >= 0);
    QVERIFY(popupAnchor.y() >= 0);
    auto *soundItem = qobject_cast<QQuickItem *>(sound);
    auto *headerItem = qobject_cast<QQuickItem *>(header.get());
    QVERIFY(soundItem != nullptr);
    QVERIFY(headerItem != nullptr);
    QVERIFY(headerItem->z() > 0);
    const QPointF buttonInHeader = soundItem->mapToItem(headerItem, 0, 0);
    QVERIFY(qAbs(popupAnchor.x() - buttonInHeader.x()) < 1.0);
    QVERIFY(qAbs(popupAnchor.y() - header->property("height").toDouble() - 4.0) < 1.0);
    const qreal popupX = popover->property("x").toDouble();
    QVERIFY(popupX > 0);
    QVERIFY(popupX + popover->property("width").toDouble() <= hostWindow.width());
    QVERIFY(header->findChild<QObject *>(QStringLiteral("globalMuteButton")) != nullptr);
    QVERIFY(header->findChild<QObject *>(QStringLiteral("audioSingleButton")) != nullptr);
    QVERIFY(header->findChild<QObject *>(QStringLiteral("audioMultiButton")) != nullptr);

    click(sound);
    QTRY_VERIFY(!popover->property("visible").toBool());

    QObject *fullscreen = header->findChild<QObject *>(QStringLiteral("fullscreenButton"));
    QVERIFY(fullscreen != nullptr);
    QVERIFY(QMetaObject::invokeMethod(fullscreen, "clicked"));
    QVERIFY(controller.fullScreenToggled);
}

void QmlInteractionTest::positionsSoundPopoverLikeDanmakuPanel()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/AppHeader.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    FakeHeaderController controller;
    QQuickWindow hostWindow;
    hostWindow.resize(QSize(800, 320));
    hostWindow.show();
    std::unique_ptr<QObject> header(component.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
        {QStringLiteral("width"), 800},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
    }));
    QVERIFY2(header != nullptr, qPrintable(component.errorString()));
    QObject *sound = header->findChild<QObject *>(QStringLiteral("soundMasterButton"));
    QObject *popover = header->findChild<QObject *>(QStringLiteral("soundMasterPopover"));
    QVERIFY(sound != nullptr);
    QVERIFY(popover != nullptr);
    click(sound);
    QTRY_VERIFY(popover->property("visible").toBool());
    const qreal expectedX = std::max<qreal>(12.0, hostWindow.width()
                                                     - popover->property("width").toDouble()
                                                     - 72.0);
    QCOMPARE(popover->property("x").toDouble(), expectedX);
    QCOMPARE(popover->property("y").toDouble(), header->property("height").toDouble() + 8.0);
    click(sound);
    QTRY_VERIFY(!popover->property("visible").toBool());
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

void QmlInteractionTest::exposesRoomVolumeAndRefreshControls()
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

    QObject *slider = tile->findChild<QObject *>(QStringLiteral("roomVolumeSlider"));
    QObject *refresh = tile->findChild<QObject *>(QStringLiteral("refreshRoomAction"));
    QVERIFY(slider != nullptr);
    QVERIFY(refresh != nullptr);
    QCOMPARE(slider->objectName(), QStringLiteral("roomVolumeSlider"));

    slider->setProperty("value", 0.35);
    QVERIFY(QMetaObject::invokeMethod(slider, "moved"));
    QCOMPARE(controller.lastVolumeRoom, QStringLiteral("63136"));
    QCOMPARE(controller.lastVolume, 35);

    QVERIFY(QMetaObject::invokeMethod(refresh, "clicked"));
    QCOMPARE(controller.refreshedRoom, QStringLiteral("63136"));
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
    QVERIFY(anchor != nullptr);
    QVERIFY(title != nullptr);
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
    QVERIFY(QMetaObject::invokeMethod(list,
                                      "itemAtIndex",
                                      Q_RETURN_ARG(QQuickItem *, rowItem),
                                      Q_ARG(int, 0)));
    QVERIFY(rowItem != nullptr);
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
    QVERIFY(QMetaObject::invokeMethod(list, "itemAtIndex",
                                      Q_RETURN_ARG(QQuickItem *, firstRow), Q_ARG(int, 0)));
    QVERIFY(QMetaObject::invokeMethod(list, "itemAtIndex",
                                      Q_RETURN_ARG(QQuickItem *, lastRow), Q_ARG(int, 1)));
    QVERIFY(firstRow != nullptr);
    QVERIFY(lastRow != nullptr);
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

void QmlInteractionTest::exposesRoomDragAndDropSurface()
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
    QVERIFY(QMetaObject::invokeMethod(list, "itemAtIndex",
                                      Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, 0)));
    QVERIFY(row != nullptr);
    QVERIFY(row->findChild<QObject *>(QStringLiteral("roomDragHandle")) != nullptr);
    QVERIFY(row->findChild<QObject *>(QStringLiteral("roomDropArea")) != nullptr);
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
    QVERIFY(qAbs((tiles.at(3)->x() + tiles.at(3)->width()) - (tiles.at(4)->x())) > 1.0);
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

    for (const int count : {1, 2, 3, 4, 5, 6, 7, 8, 9}) {
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
            QVERIFY(tiles.at(1)->x() > primary->x() + primary->width());
            QCOMPARE(tiles.at(1)->x(), tiles.at(count - 1)->x());
            if (count > 2) QVERIFY(tiles.at(count - 1)->y() > tiles.at(1)->y());
        } else {
            QVERIFY(primary->x() > 0.0);
            QVERIFY(primary->x() + primary->width() < surface->width());
            const int leftCount = count == 5 || count == 6 ? 2 : (count == 7 || count == 8 ? 3 : 4);
            const int rightCount = count - 1 - leftCount;
            for (int i = 0; i < leftCount; ++i) {
                QVERIFY(tiles.at(i + 1)->x() < primary->x());
                QVERIFY(tiles.at(i + 1)->y() >= 0.0);
            }
            for (int i = 0; i < rightCount; ++i) {
                QVERIFY(tiles.at(leftCount + i + 1)->x() > primary->x() + primary->width());
                QVERIFY(tiles.at(leftCount + i + 1)->y() >= 0.0);
            }
            if (count == 8) {
                for (int i = 1; i < rightCount; ++i) {
                    QVERIFY(tiles.at(leftCount + i + 1)->y() > tiles.at(leftCount + i)->y());
                }
                QVERIFY(qAbs(primary->width() - (surface->width() - 2.0 * 8.0) * 3.0 / 5.0) < 0.1);
            }
            if (count == 9) {
                for (int i = 1; i < rightCount; ++i) {
                    QVERIFY(tiles.at(leftCount + i + 1)->y() > tiles.at(leftCount + i)->y());
                }
                QVERIFY(qAbs(primary->width() - (surface->width() - 2.0 * 8.0) * 3.0 / 5.0) < 0.1);
            }
        }
    }
}

QTEST_MAIN(QmlInteractionTest)

#include "qml_interaction_test.moc"
