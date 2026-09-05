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

#include "ui/app_controller.h"
#include "ui/mpv_quick_item.h"

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
    if (!ok || count <= 0 || count > 9) return 9;
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
    void closesNineAttachedPlayersWithoutLingeringCallbacks();
    void removesAttachedPlayersWhileWindowRemainsOpen();
    void appliesPresetAndRefreshesAllRoomDelegates();
};

void QmlCloseRegressionTest::closesNineAttachedPlayersWithoutLingeringCallbacks()
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
    QTRY_COMPARE_WITH_TIMEOUT(controller.attachedPlayerCountForTest(), roomCount, 5000);
    const auto allPlayersReady = [window, roomCount] {
        window->update();
        const auto players = playersInItemTree(window->contentItem());
        if (players.size() != roomCount) return false;
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
    QVERIFY2(allPlayersReady(), qPrintable(QStringLiteral("expected %1 players and %1 render contexts, got %2 players and %3 ready contexts")
                                                .arg(roomCount)
                                                .arg(players.size())
                                                .arg(readyCount)));
    QTRY_VERIFY_WITH_TIMEOUT(controller.serviceProcessRunningForTest(), 5000);

    window->close();
    QTRY_VERIFY_WITH_TIMEOUT(!window->isVisible(), 5000);
    engine.reset();
    QTest::qWait(250);
    QTRY_COMPARE_WITH_TIMEOUT(controller.attachedPlayerCountForTest(), 0, 5000);
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

    QQuickWindow *window = loadWindowWithFakeRooms(*engine, controller, 3);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QTRY_COMPARE_WITH_TIMEOUT(controller.attachedPlayerCountForTest(), 3, 5000);

    QCOMPARE(controller.removeRoom(QStringLiteral("63137")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms()->rowCount(), 2, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.attachedPlayerCountForTest(), 2, 5000);
    QVERIFY(window->isVisible());

    QCOMPARE(controller.removeRoom(QStringLiteral("63136")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms()->rowCount(), 1, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.attachedPlayerCountForTest(), 1, 5000);
    QVERIFY(window->isVisible());

    QCOMPARE(controller.removeRoom(QStringLiteral("63138")), QString());
    QTRY_COMPARE_WITH_TIMEOUT(controller.rooms()->rowCount(), 0, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(controller.attachedPlayerCountForTest(), 0, 5000);
    QVERIFY(window->isVisible());

    window->close();
    engine.reset();
    QTest::qWait(250);
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

    window->close();
    engine.reset();
    QTest::qWait(250);
    QVERIFY(!controller.serviceProcessRunningForTest());
}

QTEST_MAIN(QmlCloseRegressionTest)

#include "qml_close_regression_test.moc"
