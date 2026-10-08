#include <QSettings>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QSGRendererInterface>
#include <QQuickWindow>
#include <QVariant>
#include <QAbstractItemModel>
#include <QtTest/QtTest>

#include <memory>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include "ui/app_controller.h"
#include "ui/mpv_quick_item.h"
#include "workspace/room_capacity.h"

#ifndef FAKE_STREAMGET_SERVICE_PATH
#define FAKE_STREAMGET_SERVICE_PATH "fake_streamget_service"
#endif

namespace {

QString fakeServicePath()
{
    return QString::fromLocal8Bit(FAKE_STREAMGET_SERVICE_PATH);
}

void registerQmlTypes()
{
    static const int registered =
        qmlRegisterType<MpvQuickItem>("DouyuNative", 1, 0, "MpvQuickItem");
    Q_UNUSED(registered);
}

int requestedRoomCount()
{
    bool ok = false;
    const int count = qEnvironmentVariableIntValue("DOUYU_PERF_ROOM_COUNT", &ok);
    const int maxRooms = RoomCapacity::currentLimits().maxLayoutRooms;
    if (!ok || count <= 0 || count > maxRooms) return maxRooms;
    return count;
}

QQuickWindow *loadWindowWithFakeRooms(QQmlApplicationEngine &engine,
                                      AppController &controller,
                                      int roomCount)
{
    for (int index = 0; index < roomCount; ++index) {
        if (!controller.addRoom(QString::number(63136 + index)).isEmpty()) return nullptr;
    }

    registerQmlTypes();
    engine.setInitialProperties({
        {QStringLiteral("appController"), QVariant::fromValue(static_cast<QObject *>(&controller))},
    });
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return nullptr;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (window == nullptr) return nullptr;

    controller.setMainWindow(window);
    window->resize(QSize(1280, 720));
    window->show();
    return window;
}

QList<MpvQuickItem *> playersInItemTree(QQuickItem *item)
{
    QList<MpvQuickItem *> players;
    if (item == nullptr) return players;

    if (auto *player = qobject_cast<MpvQuickItem *>(item); player != nullptr) {
        players.append(player);
    }
    for (QQuickItem *child : item->childItems()) {
        players.append(playersInItemTree(child));
    }
    return players;
}

} // namespace

class QmlCloseRegressionTest final : public QObject {
    Q_OBJECT

private slots:
    void closesAttachedPlayersWithoutLingeringCallbacks();
    void removesAttachedPlayersWhileWindowRemainsOpen();
    void appliesPresetAndRefreshesAllRoomDelegates();
    void closeDialogHasNonOverlappingRememberRow();
    void cancelCloseDialogLeavesControllerStateUnchanged();
    void nativeMinimizeRestoreKeepsControllerInSync();
    void doubleClicksRoomPictureToToggleFullscreen();
    void maximizesAndRestoresAfterFullscreen();
};

void QmlCloseRegressionTest::doubleClicksRoomPictureToToggleFullscreen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    AppController controller(fakeServicePath(), &settings);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindowWithFakeRooms(engine, controller, 2);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QQuickItem *surface = window->findChild<QQuickItem *>(QStringLiteral("layoutSurface"));
    QVERIFY(surface != nullptr);
    const auto tiles = [surface] {
        QList<QQuickItem *> result;
        for (QQuickItem *child : surface->childItems()) {
            if (child->property("roomId").isValid()) result.append(child);
        }
        return result;
    };
    QTRY_COMPARE(tiles().size(), 2);
    const QString layout = controller.workspace()->layoutMode();
    const QString primaryRoom = controller.workspace()->primaryRoomId();
    const auto doubleClickPicture = [window](QQuickItem *tile) {
        const QPoint position = tile->mapToScene(
            QPointF(tile->width() / 2, tile->height() / 2)).toPoint();
        QTest::mouseDClick(window, Qt::LeftButton, Qt::NoModifier, position);
    };

    for (QWindow::Visibility initial : {QWindow::Windowed, QWindow::Maximized}) {
        if (initial == QWindow::Maximized) window->showMaximized();
        else window->showNormal();
        QTRY_COMPARE(window->visibility(), initial);
        doubleClickPicture(tiles().at(0));
        QTRY_COMPARE(window->visibility(), QWindow::FullScreen);
        doubleClickPicture(tiles().at(1));
        QTRY_COMPARE(window->visibility(), initial);
        doubleClickPicture(tiles().at(1));
        QTRY_COMPARE(window->visibility(), QWindow::FullScreen);
        window->requestActivate();
        QTRY_VERIFY(window->isActive());
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_COMPARE(window->visibility(), initial);
        QCOMPARE(controller.rooms()->rowCount(), 2);
        QCOMPARE(controller.workspace()->layoutMode(), layout);
        QCOMPARE(controller.workspace()->primaryRoomId(), primaryRoom);
        QVERIFY(!controller.backgroundHosted());
        QVERIFY(!controller.windowMinimized());
    }
    window->showNormal();
    QTRY_COMPARE(window->visibility(), QWindow::Windowed);
    QSignalSpy restoredFrame(window, &QQuickWindow::frameSwapped);
    window->update();
    QVERIFY(restoredFrame.wait(2000));
    QQuickItem *tile = tiles().at(0);
    QQuickItem *pictureArea = tile->findChild<QQuickItem *>(
        QStringLiteral("roomPictureDoubleClickArea"));
    QVERIFY(pictureArea != nullptr);
    QCOMPARE(pictureArea->property("acceptedButtons").value<Qt::MouseButtons>(),
             Qt::LeftButton);
    QQuickItem *topBar = tile->findChild<QQuickItem *>(QStringLiteral("roomTopBar"));
    QQuickItem *bottomBar = tile->findChild<QQuickItem *>(QStringLiteral("roomBottomBar"));
    QVERIFY(topBar != nullptr);
    QVERIFY(bottomBar != nullptr);
    const QRectF pictureRect = pictureArea->mapRectToItem(
        tile, QRectF(0, 0, pictureArea->width(), pictureArea->height()));
    const QRectF topBarRect = topBar->mapRectToItem(
        tile, QRectF(0, 0, topBar->width(), topBar->height()));
    const QRectF bottomBarRect = bottomBar->mapRectToItem(
        tile, QRectF(0, 0, bottomBar->width(), bottomBar->height()));
    QVERIFY(!pictureRect.intersects(topBarRect));
    QVERIFY(!pictureRect.intersects(bottomBarRect));

    for (const QString &name : {QStringLiteral("roomVolumeSlider"),
                                QStringLiteral("roomQualitySelector"),
                                QStringLiteral("roomTopActions")}) {
        QQuickItem *control = tile->findChild<QQuickItem *>(name);
        QVERIFY(control != nullptr);
        const QRectF controlRect = control->mapRectToItem(
            tile, QRectF(0, 0, control->width(), control->height()));
        QVERIFY(!pictureRect.intersects(controlRect));
    }
    QTRY_VERIFY(pictureArea->property("enabled").toBool());
    tile->setProperty("menuOpen", true);
    QTRY_VERIFY(!pictureArea->property("enabled").toBool());
    tile->setProperty("menuOpen", false);
    QObject *popup = tile->findChild<QObject *>(QStringLiteral("roomQualityPopup"));
    QVERIFY(popup != nullptr);
    QVERIFY(QMetaObject::invokeMethod(popup, "open"));
    QTRY_VERIFY(popup->property("visible").toBool());
    QTRY_VERIFY(!pictureArea->property("enabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(popup, "close"));
    controller.requestQuit();
}

void QmlCloseRegressionTest::maximizesAndRestoresAfterFullscreen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    AppController controller(fakeServicePath(), &settings);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindowWithFakeRooms(engine, controller, 0);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    controller.toggleFullScreen();
    QTRY_COMPARE(window->visibility(), QWindow::FullScreen);

    controller.toggleMaximizedWindow();
    QTRY_COMPARE(window->visibility(), QWindow::Maximized);

    controller.toggleMaximizedWindow();
    QTRY_COMPARE(window->visibility(), QWindow::Windowed);
    controller.requestQuit();
}

void QmlCloseRegressionTest::nativeMinimizeRestoreKeepsControllerInSync()
{
#ifdef Q_OS_WIN
    if (QGuiApplication::platformName() != QStringLiteral("windows")) {
        QSKIP("native taskbar window state requires the Windows platform backend");
    }
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    AppController controller(fakeServicePath(), &settings);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindowWithFakeRooms(engine, controller, 0);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    const HWND handle = reinterpret_cast<HWND>(window->winId());
    QVERIFY2((GetWindowLongPtrW(handle, GWL_STYLE) & WS_MINIMIZEBOX) != 0,
             "taskbar minimize requires WS_MINIMIZEBOX on the native window");

    for (int cycle = 0; cycle < 3; ++cycle) {
        SendMessageW(handle, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        QTRY_COMPARE(window->visibility(), QWindow::Minimized);
        QTRY_VERIFY(controller.windowMinimized());
        QVERIFY(!controller.backgroundHosted());
        QVERIFY(IsIconic(handle));

        SendMessageW(handle, WM_SYSCOMMAND, SC_RESTORE, 0);
        QTRY_COMPARE(window->visibility(), QWindow::Windowed);
        QTRY_VERIFY(!controller.windowMinimized());
        QVERIFY(!controller.backgroundHosted());
        QVERIFY(!IsIconic(handle));
    }
#else
    QSKIP("native taskbar window state is Windows-only");
#endif
}

void QmlCloseRegressionTest::closeDialogHasNonOverlappingRememberRow()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    AppController controller(fakeServicePath(), &settings);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQmlApplicationEngine engine;
    registerQmlTypes();
    engine.setInitialProperties({
        {QStringLiteral("appController"), QVariant::fromValue(static_cast<QObject *>(&controller))},
    });
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());

    auto *dialog = engine.rootObjects().constFirst()->findChild<QObject *>(
        QStringLiteral("closeBehaviorDialog"));
    QVERIFY(dialog != nullptr);
    QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
    QTRY_VERIFY(dialog->property("visible").toBool());

    auto *indicator = dialog->findChild<QObject *>(QStringLiteral("rememberChoiceIndicator"));
    auto *label = dialog->findChild<QObject *>(QStringLiteral("rememberChoiceLabel"));
    QVERIFY(indicator != nullptr);
    QVERIFY(label != nullptr);
    auto *indicatorItem = qobject_cast<QQuickItem *>(indicator);
    auto *labelItem = qobject_cast<QQuickItem *>(label);
    QVERIFY(indicatorItem != nullptr);
    QVERIFY(labelItem != nullptr);
    const QRectF indicatorRect = indicatorItem->mapRectToItem(
        nullptr, QRectF(0, 0, indicatorItem->width(), indicatorItem->height()));
    const QRectF labelRect = labelItem->mapRectToItem(
        nullptr, QRectF(0, 0, labelItem->width(), labelItem->height()));
    QVERIFY2(!indicatorRect.intersects(labelRect),
             "remember checkbox indicator overlaps label");
    dialog->setProperty("visible", false);
}

void QmlCloseRegressionTest::cancelCloseDialogLeavesControllerStateUnchanged()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    AppController controller(fakeServicePath(), &settings);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    QQmlApplicationEngine engine;
    registerQmlTypes();
    engine.setInitialProperties({
        {QStringLiteral("appController"), QVariant::fromValue(static_cast<QObject *>(&controller))},
    });
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *dialog = engine.rootObjects().constFirst()->findChild<QObject *>(
        QStringLiteral("closeBehaviorDialog"));
    QVERIFY(dialog != nullptr);
    QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
    QTRY_VERIFY(dialog->property("visible").toBool());
    auto *cancel = dialog->findChild<QObject *>(QStringLiteral("cancelCloseButton"));
    QVERIFY(cancel != nullptr);
    QVERIFY(QMetaObject::invokeMethod(cancel, "click"));
    QTRY_VERIFY(!dialog->property("visible").toBool());
    QVERIFY(!controller.quitRequested());
    QVERIFY(!controller.backgroundHosted());
}

void QmlCloseRegressionTest::closesAttachedPlayersWithoutLingeringCallbacks()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    AppController controller(fakeServicePath(), &settings);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    auto engine = std::make_unique<QQmlApplicationEngine>();

    const int roomCount = requestedRoomCount();
    const int expectedPlayerCount = qMin(roomCount,
                                         RoomCapacity::currentLimits().defaultDecodedRooms);
    QQuickWindow *window = loadWindowWithFakeRooms(*engine, controller, roomCount);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms()->rowCount(), roomCount, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.attachedPlayerCountForTest(),
                              expectedPlayerCount, 5000);
    const auto allPlayersReady = [window, expectedPlayerCount] {
        window->update();
        const auto players = playersInItemTree(window->contentItem());
        if (players.size() != expectedPlayerCount) return false;
        for (MpvQuickItem *player : players) {
            if (player == nullptr || !player->isRenderContextReady()) return false;
        }
        return true;
    };
    QElapsedTimer rendererWait;
    rendererWait.start();
    while (!allPlayersReady() && rendererWait.elapsed() < 10000) {
        QTest::qWait(50);
    }
    const auto players = playersInItemTree(window->contentItem());
    int readyCount = 0;
    for (MpvQuickItem *player : players) {
        if (player != nullptr && player->isRenderContextReady()) ++readyCount;
    }
    if (!allPlayersReady()) {
        if (qEnvironmentVariable("QT_QPA_PLATFORM") != QStringLiteral("offscreen")) {
            QVERIFY2(false, qPrintable(QStringLiteral("expected %1 players and %1 render contexts, got %2 players and %3 ready contexts")
                                            .arg(expectedPlayerCount)
                                            .arg(players.size())
                                            .arg(readyCount)));
        }
        qInfo() << "offscreen Qt platform did not provide OpenGL render contexts; continuing lifecycle checks";
    }
    QTRY_VERIFY_WITH_TIMEOUT(controller.serviceProcessRunningForTest(), 5000);

    // Drive the same close-to-tray operation directly so the test remains
    // deterministic on headless Windows runners where QWindow::close() may not
    // dispatch the QML onClosing handler.
    controller.closeToTray();
    QTRY_VERIFY_WITH_TIMEOUT(!window->isVisible(), 5000);
    QVERIFY(controller.serviceProcessRunningForTest());
    engine.reset();
    QTest::qWait(250);
    QTRY_COMPARE_WITH_TIMEOUT(controller.attachedPlayerCountForTest(), 0, 5000);
    controller.requestQuit();
    QVERIFY(!controller.serviceProcessRunningForTest());
}

void QmlCloseRegressionTest::removesAttachedPlayersWhileWindowRemainsOpen()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    AppController controller(fakeServicePath(), &settings);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    auto engine = std::make_unique<QQmlApplicationEngine>();

    const int roomCount = requestedRoomCount();
    QQuickWindow *window = loadWindowWithFakeRooms(*engine, controller, roomCount);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    const int expectedPlayerCount = qMin(
        roomCount,
        RoomCapacity::currentLimits().defaultDecodedRooms);
    QTRY_COMPARE_WITH_TIMEOUT(controller.attachedPlayerCountForTest(),
                              expectedPlayerCount,
                              5000);

    for (int index = roomCount - 1; index >= 0; --index) {
        const QString roomId = QString::number(63136 + index);
        QCOMPARE(controller.removeRoom(roomId), QString());
        QTRY_COMPARE_WITH_TIMEOUT(controller.rooms()->rowCount(), index, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(
            controller.attachedPlayerCountForTest(),
            qMin(index, RoomCapacity::currentLimits().defaultDecodedRooms),
            5000);
        QVERIFY(window->isVisible());
    }
    QVERIFY(window->isVisible());

    controller.closeToTray();
    QTRY_VERIFY_WITH_TIMEOUT(!window->isVisible(), 5000);
    QVERIFY(controller.serviceProcessRunningForTest());
    engine.reset();
    QTest::qWait(250);
    controller.requestQuit();
    QVERIFY(!controller.serviceProcessRunningForTest());
}

void QmlCloseRegressionTest::appliesPresetAndRefreshesAllRoomDelegates()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    settings.setValue(
        QStringLiteral("DouyuMonitor/nativeWorkspaceV1"),
        QByteArray(R"JSON({"version":3,"library":[{"roomId":"63136","metadata":{"roomId":"63136","anchorName":"主播 1"},"requestedQuality":"auto","favorite":false,"lastOpenedAtMs":0,"volume":100,"danmakuEnabled":true}],"groups":[],"activeRoomIds":["63136"],"activeGroupId":"","primaryRoomId":"63136","audioRoomId":"","presets":[{"id":"p1","name":"五路","layoutId":"auto","activeGroupId":"","primaryRoomId":"63136","audioRoomId":"","roomIds":["63136","63137","63138","63139","63140"],"sidebarVisible":true,"primaryRoomRatio":0.6,"audioMode":"single","globalMuted":false,"danmaku":{"globalEnabled":false,"display":{"durationSeconds":8,"fontSize":24,"opacity":0.85,"region":"top","density":"normal","fontFamily":"simhei","rendering":"native"},"governance":{"enabled":true,"keywordBlacklist":[],"duplicateWindowSeconds":3,"peakProtectionEnabled":true},"roomOverrides":{}}}],"danmaku":{"globalEnabled":false,"display":{"durationSeconds":8,"fontSize":24,"opacity":0.85,"region":"top","density":"normal","fontFamily":"simhei","rendering":"native"},"governance":{"enabled":true,"keywordBlacklist":[],"duplicateWindowSeconds":3,"peakProtectionEnabled":true},"roomOverrides":{}}})JSON"));
    AppController controller(fakeServicePath(), &settings);
    QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
    auto engine = std::make_unique<QQmlApplicationEngine>();

    registerQmlTypes();
    engine->setInitialProperties({
        {QStringLiteral("appController"), QVariant::fromValue(static_cast<QObject *>(&controller))},
    });
    engine->load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    QVERIFY(!engine->rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine->rootObjects().constFirst());
    QVERIFY(window != nullptr);
    controller.setMainWindow(window);
    window->resize(QSize(1280, 720));
    window->show();
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms()->rowCount(), 1, 5000);
    QObject *grid = window->findChild<QObject *>(QStringLiteral("workspaceGrid"));
    QObject *sidebar = window->findChild<QObject *>(QStringLiteral("roomSidebar"));
    QVERIFY(grid != nullptr);
    QVERIFY(sidebar != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(grid->property("roomCount").toInt(), 1, 5000);

    QCOMPARE(controller.applyWorkspacePreset(QStringLiteral("p1")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms()->rowCount(), 5, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(grid->property("roomCount").toInt(), 5, 5000);
    QObject *roomList = sidebar->findChild<QObject *>(QStringLiteral("roomList"));
    QVERIFY(roomList != nullptr);
    QObject *roomModel = roomList->property("model").value<QObject *>();
    QVERIFY(roomModel != nullptr);
    auto *roomItemModel = qobject_cast<QAbstractItemModel *>(roomModel);
    QVERIFY(roomItemModel != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(roomItemModel->rowCount(), 5, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(roomList->property("count").toInt(), 5, 5000);

    for (int index = 0; index < 5; ++index) {
        QQuickItem *row = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT(QMetaObject::invokeMethod(roomList, "itemAtIndex",
            Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, index)) && row, 5000);
        QCOMPARE(row->property("roomId").toString(), QString::number(63136 + index));
    }
    QCOMPARE(controller.setPrimaryRoom("63140"), QString());
    QCOMPARE(controller.removeRoom("63138"), QString());
    QCOMPARE(controller.removeRoom("63139"), QString());
    QCOMPARE(controller.applyWorkspacePreset("p1"), QString());
    QTRY_COMPARE_WITH_TIMEOUT(roomList->property("count").toInt(), 5, 5000);
    QQuickItem *firstRow = nullptr;
    QTRY_VERIFY_WITH_TIMEOUT(QMetaObject::invokeMethod(roomList, "itemAtIndex",
        Q_RETURN_ARG(QQuickItem *, firstRow), Q_ARG(int, 0)) && firstRow, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(firstRow->property("primary").toBool(), 5000);
    for (const QVariant &value : sidebar->property("libraryRooms").toList()) {
        const QVariantMap entry = value.toMap();
        QVERIFY(entry.value("active").toBool());
        QVERIFY(entry.value("lastOpenedAtMs").toLongLong() > 0);
    }

    controller.closeToTray();
    QTRY_VERIFY_WITH_TIMEOUT(!window->isVisible(), 5000);
    QVERIFY(controller.serviceProcessRunningForTest());
    engine.reset();
    QTest::qWait(250);
    controller.requestQuit();
    QVERIFY(!controller.serviceProcessRunningForTest());
}

QTEST_MAIN(QmlCloseRegressionTest)

#include "qml_close_regression_test.moc"
