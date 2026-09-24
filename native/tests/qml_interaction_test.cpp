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
    Q_PROPERTY(RoomListModel *rooms READ rooms CONSTANT)

public:
    FakeHeaderController()
        : workspaceModel(nullptr, this)
    {
        workspaceModel.setAudioPolicy(QStringLiteral("single"), false);
    }

    WorkspaceModel *workspace() noexcept { return &workspaceModel; }
    RoomListModel *rooms() noexcept { return &roomModel; }

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

    WorkspaceModel workspaceModel;
    RoomListModel roomModel;
    bool lastMuted = false;
    QString lastAudioMode;
    QString lastLayout;
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

    QString lastVolumeRoom;
    int lastVolume = -1;
    QString lastQualityRoom;
    int lastQuality = -1;
    int lastQualityRate = -2;
    QString refreshedRoom;
    QString movedRoom;
    int movedDelta = 0;
    QString removedRoom;
    QStringList attachedRooms;
    QStringList detachedRooms;
};

class FakePresetController final : public QObject {
    Q_OBJECT

public:
    Q_INVOKABLE QString saveWorkspacePreset(const QString &) { return {}; }
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

class FakeUpdateController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString updateState READ updateState NOTIFY updateStateChanged)
    Q_PROPERTY(QString updateMessage READ updateMessage NOTIFY updateStateChanged)
    Q_PROPERTY(QUrl updateReleaseUrl READ updateReleaseUrl NOTIFY updateStateChanged)

public:
    QString updateState() const { return updateState_; }
    QString updateMessage() const { return updateMessage_; }
    QUrl updateReleaseUrl() const { return updateReleaseUrl_; }

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
    void updateStateChanged();

private:
    QString updateState_ = QStringLiteral("idle");
    QString updateMessage_;
    QUrl updateReleaseUrl_;
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

} // namespace

class QmlInteractionTest final : public QObject {
    Q_OBJECT

private slots:
    void opensAddRoomDialogAndRejectsInvalidRoomId();
    void opensMonitoringDanmakuAndWorkspaceSurfaces();
    void closesTransientSurfacesFromExplicitActions();
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
    void doesNotExposeGroupManagementControls();
    void rendersAndAddsSearchCandidate();
    void exposesSupportedLayoutOptions();
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
    void exposesRoomVolumeAndRefreshControls();
    void switchesRoomQualityByStreamRate();
    void keepsQualityLabelGeometryWhileHoveringSelector();
    void rendersQualityPopupOptions();
    void rendersAvailableQualitiesFromModelRole();
    void defersRoomRemovalUntilAfterQmlHandlerReturns();
    void rebindsPlayerWhenRoomIdentityChanges();
    void rendersFallbackMetadataAndUnknownStatus();
    void exposesRoomOrderingControls();
    void disablesOrderingAtListBoundaries();
    void doesNotExposeRoomDragAndDropSurface();
    void deletesWorkspacePresetFromPanel();
    void defersWorkspacePresetApplyUntilPopupHandlerReturns();
    void refreshesRoomSidebarAfterPresetLikeModelUpdate();
    void keepsTransientDialogSurfacesDark();
    void keepsInputAndMenuControlsOnDarkTheme();
    void exposesFavoriteTitleNotificationPreference();
    void checksForUpdatesFromSettingsPage();
};

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
    QCOMPARE(notificationFooter->property("color").value<QColor>(), QColor(QStringLiteral("#202731")));
    QObject *notificationLabel = notification->findChild<QObject *>(QStringLiteral("notificationEnabledLabel"));
    QVERIFY(notificationLabel != nullptr);
    QCOMPARE(notificationLabel->property("color").value<QColor>(), QColor(QStringLiteral("#eef2f7")));

    QQmlComponent addRoomComponent(&engine,
                                   QUrl(QStringLiteral("qrc:/qml/dialogs/AddRoomDialog.qml")));
    QVERIFY2(addRoomComponent.isReady(), qPrintable(addRoomComponent.errorString()));
    std::unique_ptr<QObject> addRoom(addRoomComponent.createWithInitialProperties({
        {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
    }));
    QVERIFY(addRoom != nullptr);
    QObject *addRoomFooter = addRoom->findChild<QObject *>(QStringLiteral("addRoomDialogFooter"));
    QVERIFY(addRoomFooter != nullptr);
    QCOMPARE(addRoomFooter->property("color").value<QColor>(), QColor(QStringLiteral("#202731")));
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
    QCOMPARE(roomInput->property("color").value<QColor>(), QColor(QStringLiteral("#eef2f7")));

    QQmlApplicationEngine mainEngine;
    QQuickWindow *window = loadWindow(mainEngine);
    QVERIFY(window != nullptr);
    auto *layoutButton = window->findChild<QObject *>(QStringLiteral("layoutMenuButton"));
    QVERIFY(layoutButton != nullptr);
    QVERIFY(QMetaObject::invokeMethod(layoutButton, "clicked"));
    QObject *layoutMenu = window->findChild<QObject *>(QStringLiteral("layoutMenu"));
    QVERIFY(layoutMenu != nullptr);
    QTRY_VERIFY(layoutMenu->property("visible").toBool());
    QObject *menuBackground = layoutMenu->findChild<QObject *>(QStringLiteral("layoutMenuBackground"));
    QVERIFY(menuBackground != nullptr);
    QCOMPARE(menuBackground->property("color").value<QColor>(), QColor(QStringLiteral("#202731")));
    QObject *menuItemLabel = layoutMenu->findChild<QObject *>(QStringLiteral("layoutMenuItemLabel"));
    QVERIFY(menuItemLabel != nullptr);
    QCOMPARE(menuItemLabel->property("color").value<QColor>(), QColor(QStringLiteral("#eef2f7")));

    QObject *dualPrimary = layoutMenu->findChild<QObject *>(QStringLiteral("dualPrimaryLayoutOption"));
    QVERIFY(dualPrimary != nullptr);
    QObject *dualPrimaryBackground =
        dualPrimary->findChild<QObject *>(QStringLiteral("dualPrimaryLayoutOptionBackground"));
    QVERIFY(dualPrimaryBackground != nullptr);
    dualPrimary->setProperty("highlighted", true);
    QCOMPARE(dualPrimaryBackground->property("color").value<QColor>(),
             QColor(QStringLiteral("#12171e")));

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
    QObject *fullscreenIcon = window->findChild<QObject *>(QStringLiteral("fullscreenIcon"));
    QVERIFY(closeIcon != nullptr);
    QVERIFY(minimizeIcon != nullptr);
    QVERIFY(maximizeIcon != nullptr);
    QVERIFY(fullscreenIcon != nullptr);
    for (QObject *icon : {closeIcon, minimizeIcon, maximizeIcon, fullscreenIcon}) {
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

    click(window->findChild<QObject *>(QStringLiteral("soundMasterButton")));
    QObject *soundMaster = window->findChild<QObject *>(QStringLiteral("soundMasterPopover"));
    QVERIFY(soundMaster != nullptr);
    QTRY_VERIFY(soundMaster->property("visible").toBool());
    QObject *closeSoundMaster = soundMaster->findChild<QObject *>(
        QStringLiteral("closeSoundMasterButton"));
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

void QmlInteractionTest::enablesDualPrimaryLayoutAfterFourthRoomIsAdded()
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

void QmlInteractionTest::usesGroupedHeaderControls()
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

    for (const QString &objectName : {QStringLiteral("sidebarToggleButton"),
                                      QStringLiteral("danmakuButton"),
                                      QStringLiteral("monitoringButton"),
                                      QStringLiteral("workspaceButton"),
                                      QStringLiteral("layoutMenuButton"),
                                      QStringLiteral("soundMasterButton"),
                                      QStringLiteral("fullscreenButton")}) {
        QObject *control = header->findChild<QObject *>(objectName);
        QVERIFY(control != nullptr);
        QCOMPARE(control->property("width").toInt(), 32);
        QCOMPARE(control->property("height").toInt(), 32);
    }

    QObject *sound = header->findChild<QObject *>(QStringLiteral("soundMasterButton"));
    QObject *popover = header->findChild<QObject *>(QStringLiteral("soundMasterPopover"));
    QObject *close = header->findChild<QObject *>(QStringLiteral("closeSoundMasterButton"));
    QVERIFY(sound != nullptr);
    QVERIFY(popover != nullptr);
    QVERIFY(close != nullptr);
    click(sound);
    QTRY_VERIFY(popover->property("visible").toBool());
    QVERIFY(popover->property("y").toDouble()
            >= sound->property("y").toDouble() + sound->property("height").toDouble());
    click(close);
    QTRY_VERIFY(!popover->property("visible").toBool());

    QObject *windowControls = header->findChild<QObject *>(QStringLiteral("windowControls"));
    QVERIFY(windowControls != nullptr);
    for (const QString &objectName : {QStringLiteral("minimizeButton"),
                                      QStringLiteral("maximizeButton"),
                                      QStringLiteral("closeButton")}) {
        QObject *control = windowControls->findChild<QObject *>(objectName);
        QVERIFY(control != nullptr);
        QCOMPARE(control->property("width").toInt(), 32);
        QCOMPARE(control->property("height").toInt(), 32);
    }
    QCOMPARE(windowControls->findChild<QObject *>(QStringLiteral("minimizeButton"))
                 ->property("accessibilityLabel").toString(),
             QStringLiteral("最小化窗口"));
    QCOMPARE(windowControls->findChild<QObject *>(QStringLiteral("maximizeButton"))
                 ->property("accessibilityLabel").toString(),
             QStringLiteral("最大化窗口"));
    QCOMPARE(windowControls->findChild<QObject *>(QStringLiteral("closeButton"))
                 ->property("accessibilityLabel").toString(),
             QStringLiteral("关闭窗口"));
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
    QObject *windowControls = header->findChild<QObject *>(QStringLiteral("windowControls"));
    QVERIFY(windowControls != nullptr);
    QObject *maximize = windowControls->findChild<QObject *>(QStringLiteral("maximizeButton"));
    QObject *maximizeIcon = windowControls->findChild<QObject *>(QStringLiteral("maximizeIcon"));
    QObject *fullscreenIcon = header->findChild<QObject *>(QStringLiteral("fullscreenIcon"));
    QVERIFY(maximize != nullptr);
    QVERIFY(maximizeIcon != nullptr);
    QVERIFY(fullscreenIcon != nullptr);
    QCOMPARE(maximize->property("accessibilityLabel").toString(), QStringLiteral("最大化窗口"));
    QCOMPARE(fullscreen->property("accessibilityLabel").toString(), QStringLiteral("全屏播放"));
    QVERIFY(maximizeIcon->property("source").toUrl() != fullscreenIcon->property("source").toUrl());
    QVERIFY(maximizeIcon->property("source").toUrl().toString().contains(QStringLiteral("window-maximize.svg")));
    QVERIFY(fullscreenIcon->property("source").toUrl().toString().contains(QStringLiteral("window-fullscreen.svg")));
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
    QObject *topBar = tile->findChild<QObject *>(QStringLiteral("roomTopBar"));
    QObject *topActions = tile->findChild<QObject *>(QStringLiteral("roomTopActions"));
    QVERIFY(slider != nullptr);
    QVERIFY(refresh != nullptr);
    QVERIFY(topBar != nullptr);
    QVERIFY(topActions != nullptr);
    QVERIFY(topActions->property("width").toDouble() >= 32.0);
    QVERIFY(topBar->property("z").toDouble() > 3.0);
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
    QVERIFY(QMetaObject::invokeMethod(presetList,
                                      "itemAtIndex",
                                      Q_RETURN_ARG(QQuickItem *, presetRow),
                                      Q_ARG(int, 0)));
    QVERIFY(presetRow != nullptr);
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
    QVERIFY(QMetaObject::invokeMethod(presetList, "itemAtIndex",
                                      Q_RETURN_ARG(QQuickItem *, presetRow), Q_ARG(int, 0)));
    QVERIFY(presetRow != nullptr);
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
