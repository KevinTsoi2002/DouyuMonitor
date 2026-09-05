#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSize>
#include <QtTest/QtTest>

#include "ui/mpv_quick_item.h"

class FakeDanmakuVisualController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool globalEnabled READ globalEnabled CONSTANT)
    Q_PROPERTY(QVariantMap displaySettings READ displaySettings CONSTANT)

public:
    bool globalEnabled() const noexcept
    {
        return true;
    }

    QVariantMap displaySettings() const
    {
        return {{QStringLiteral("durationSeconds"), 8},
                {QStringLiteral("fontSize"), 20},
                {QStringLiteral("opacity"), 0.9},
                {QStringLiteral("region"), QStringLiteral("full")},
                {QStringLiteral("density"), QStringLiteral("massive")},
                {QStringLiteral("fontFamily"), QStringLiteral("microsoft-yahei")},
                {QStringLiteral("rendering"), QStringLiteral("native")}};
    }

    void enqueueFixture()
    {
        messages_.append({{QStringLiteral("id"), QStringLiteral("fixture")},
                          {QStringLiteral("roomId"), QStringLiteral("preview-1")},
                          {QStringLiteral("nickname"), QStringLiteral("fixture")},
                          {QStringLiteral("text"), QStringLiteral("弹幕测试")}});
        emit messageAvailable(QStringLiteral("preview-1"));
    }

    Q_INVOKABLE QVariantMap takeNextMessage(const QString &roomId)
    {
        if (messages_.isEmpty() || messages_.first().value(QStringLiteral("roomId")).toString() != roomId) {
            return {};
        }
        return messages_.takeFirst();
    }

    Q_INVOKABLE QVariantMap statusForRoom(const QString &) const
    {
        return {{QStringLiteral("state"), QStringLiteral("connected")}};
    }

    Q_INVOKABLE void clearRoom(const QString &)
    {
        messages_.clear();
    }

signals:
    void messageAvailable(const QString &roomId);

private:
    QList<QVariantMap> messages_;
};

class FakeRoomTileVisualController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *danmaku READ danmaku CONSTANT)

public:
    explicit FakeRoomTileVisualController(FakeDanmakuVisualController *danmaku)
        : danmaku_(danmaku)
    {
    }

    QObject *danmaku() const
    {
        return danmaku_;
    }

    Q_INVOKABLE void attachPlayer(const QString &, MpvQuickItem *) {}
    Q_INVOKABLE void detachPlayer(const QString &, MpvQuickItem *) {}

private:
    FakeDanmakuVisualController *danmaku_ = nullptr;
};

namespace {

QQuickWindow *loadWindow(QQmlApplicationEngine &engine, const QSize &size, bool showPreviewRooms = true)
{
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return nullptr;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (window == nullptr) return nullptr;
    window->setProperty("showPreviewRooms", showPreviewRooms);

    // The offscreen platform does not enforce Window minimum dimensions during resize.
    const QSize boundedSize(qMax(size.width(), window->minimumWidth()),
                            qMax(size.height(), window->minimumHeight()));
    window->resize(boundedSize);
    window->show();
    return window;
}

void registerQmlTypes()
{
    static const int registered = qmlRegisterType<MpvQuickItem>("DouyuNative", 1, 0, "MpvQuickItem");
    Q_UNUSED(registered);
}

void saveScreenshot(const QImage &image, const QString &name)
{
    const QString directory = QStringLiteral(QML_VERIFICATION_DIR);
    QVERIFY2(QDir().mkpath(directory), "Could not create QML verification directory");
    QVERIFY2(image.save(directory + QLatin1Char('/') + name), "Could not save QML verification screenshot");
}

QRect itemRect(QObject *object)
{
    return {
        qRound(object->property("x").toDouble()),
        qRound(object->property("y").toDouble()),
        qRound(object->property("width").toDouble()),
        qRound(object->property("height").toDouble()),
    };
}

void click(QObject *object)
{
    QVERIFY(object != nullptr);
    QVERIFY(QMetaObject::invokeMethod(object, "clicked"));
}

QQuickItem *previewRoomTile(QQuickItem *item)
{
    if (item->property("roomId").toString() == QStringLiteral("preview-1")
        && item->property("danmakuState").isValid()
        && item->property("playbackState").isValid()) {
        return item;
    }
    for (QQuickItem *child : item->childItems()) {
        if (QQuickItem *tile = previewRoomTile(child)) return tile;
    }
    return nullptr;
}

QQuickItem *itemByObjectName(QQuickItem *item, const QString &objectName)
{
    if (item->objectName() == objectName) return item;
    for (QQuickItem *child : item->childItems()) {
        if (QQuickItem *match = itemByObjectName(child, objectName)) return match;
    }
    return nullptr;
}

int brightPixels(const QImage &image, const QRect &rect)
{
    int count = 0;
    for (int y = rect.top(); y <= rect.bottom(); ++y) {
        for (int x = rect.left(); x <= rect.right(); ++x) {
            const QColor color = image.pixelColor(x, y);
            if (color.red() > 180 && color.green() > 180 && color.blue() > 180) ++count;
        }
    }
    return count;
}

} // namespace

class QmlVisualSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsReleasedModuleWithVisualAnchors();
    void showsStructuredEmptyWorkspace();
    void showsStatusBarForOccupiedWorkspace();
    void hasReferenceGeometryAt1280x720();
    void clampsNarrowWindowToSafeMinimum();
    void hasNoTextOverlapAt1600x900();
    void hasNoTextOverlapAt1920x1080();
    void usesAssetBackedIcons();
    void rendersDanmakuFixtureInsideRoomTile();
    void showsDanmakuSettingsPanelInsideViewport();
};

void QmlVisualSmokeTest::loadsReleasedModuleWithVisualAnchors()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    engine.loadFromModule(QStringLiteral("DouyuMonitor"), QStringLiteral("Main"));

    QVERIFY2(!engine.rootObjects().isEmpty(), "Released QML module did not create Main");
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("appHeader")) != nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("roomSidebar")) != nullptr);
    QVERIFY(window->findChild<QObject *>(QStringLiteral("workspaceGrid")) != nullptr);
}

void QmlVisualSmokeTest::showsStructuredEmptyWorkspace()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine, QSize(1280, 720), false);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *empty = window->findChild<QObject *>(QStringLiteral("emptyWorkspaceState"));
    QObject *status = window->findChild<QObject *>(QStringLiteral("workspaceStatusBar"));
    QVERIFY(empty != nullptr);
    QVERIFY(status != nullptr);
    QVERIFY(empty->property("visible").toBool());
    QCOMPARE(status->property("onlineCount").toInt(), 0);
    QCOMPARE(status->property("color").value<QColor>(), QColor(QStringLiteral("#202731")));
    QObject *grid = window->findChild<QObject *>(QStringLiteral("workspaceGrid"));
    QVERIFY(grid != nullptr);
    QVERIFY(QRect(QPoint(0, 0), window->size()).contains(itemRect(status)));
    QVERIFY(itemRect(grid).bottom() < itemRect(status).top());

    saveScreenshot(window->grabWindow(), QStringLiteral("empty-shell-1280x720.png"));
}

void QmlVisualSmokeTest::showsStatusBarForOccupiedWorkspace()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine, QSize(1600, 900));
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *empty = window->findChild<QObject *>(QStringLiteral("emptyWorkspaceState"));
    QObject *status = window->findChild<QObject *>(QStringLiteral("workspaceStatusBar"));
    QVERIFY(empty != nullptr);
    QVERIFY(status != nullptr);
    QVERIFY(!empty->property("visible").toBool());
    QObject *grid = window->findChild<QObject *>(QStringLiteral("workspaceGrid"));
    QVERIFY(grid != nullptr);
    QVERIFY(QRect(QPoint(0, 0), window->size()).contains(itemRect(status)));
    QVERIFY(itemRect(grid).bottom() < itemRect(status).top());

    saveScreenshot(window->grabWindow(), QStringLiteral("occupied-shell-1600x900.png"));
}

void QmlVisualSmokeTest::hasReferenceGeometryAt1280x720()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine, QSize(1280, 720));
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *header = window->findChild<QObject *>(QStringLiteral("appHeader"));
    QObject *sidebar = window->findChild<QObject *>(QStringLiteral("roomSidebar"));
    QObject *grid = window->findChild<QObject *>(QStringLiteral("workspaceGrid"));
    QVERIFY(header != nullptr);
    QVERIFY(sidebar != nullptr);
    QVERIFY(grid != nullptr);
    QCOMPARE(qRound(header->property("height").toDouble()), 52);
    QCOMPARE(qRound(sidebar->property("width").toDouble()), 268);

    const QImage image = window->grabWindow();
    QVERIFY(!image.isNull());
    QVERIFY(image.pixelColor(300, 60) != QColor(QStringLiteral("#000000")));
    saveScreenshot(image, QStringLiteral("shell-1280x720.png"));
}

void QmlVisualSmokeTest::clampsNarrowWindowToSafeMinimum()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine, QSize(390, 844));
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QVERIFY(window->width() >= 960);
    QVERIFY(window->height() >= 600);

    QObject *header = window->findChild<QObject *>(QStringLiteral("appHeader"));
    QObject *sidebar = window->findChild<QObject *>(QStringLiteral("roomSidebar"));
    QObject *grid = window->findChild<QObject *>(QStringLiteral("workspaceGrid"));
    QVERIFY(header != nullptr);
    QVERIFY(sidebar != nullptr);
    QVERIFY(grid != nullptr);

    const QRect viewport(QPoint(0, 0), window->size());
    QVERIFY(viewport.contains(itemRect(header)));
    QVERIFY(viewport.contains(itemRect(sidebar)));
    QVERIFY(viewport.contains(itemRect(grid)));

    const QImage image = window->grabWindow();
    QVERIFY(!image.isNull());
    QVERIFY(image.pixelColor(300, 60) != QColor(QStringLiteral("#000000")));
    saveScreenshot(image, QStringLiteral("shell-390x844-clamped.png"));
}

void QmlVisualSmokeTest::hasNoTextOverlapAt1920x1080()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine, QSize(1920, 1080));
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *header = window->findChild<QObject *>(QStringLiteral("appHeader"));
    QObject *sidebar = window->findChild<QObject *>(QStringLiteral("roomSidebar"));
    QObject *grid = window->findChild<QObject *>(QStringLiteral("workspaceGrid"));
    QObject *toast = window->findChild<QObject *>(QStringLiteral("toastViewport"));
    QVERIFY(header != nullptr);
    QVERIFY(sidebar != nullptr);
    QVERIFY(grid != nullptr);
    QVERIFY(toast != nullptr);

    const QRect headerRect = itemRect(header);
    const QRect sidebarRect = itemRect(sidebar);
    const QRect gridRect = itemRect(grid);
    const QRect toastRect = itemRect(toast);
    QVERIFY(!headerRect.intersects(sidebarRect));
    QVERIFY(!headerRect.intersects(gridRect));
    QVERIFY(!headerRect.intersects(toastRect));
    QVERIFY(!sidebarRect.intersects(gridRect));
    QVERIFY(!sidebarRect.intersects(toastRect));

    const QImage image = window->grabWindow();
    QVERIFY(!image.isNull());
    saveScreenshot(image, QStringLiteral("occupied-shell-1920x1080.png"));
}

void QmlVisualSmokeTest::hasNoTextOverlapAt1600x900()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine, QSize(1600, 900));
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QObject *header = window->findChild<QObject *>(QStringLiteral("appHeader"));
    QObject *sidebar = window->findChild<QObject *>(QStringLiteral("roomSidebar"));
    QObject *grid = window->findChild<QObject *>(QStringLiteral("workspaceGrid"));
    QObject *toast = window->findChild<QObject *>(QStringLiteral("toastViewport"));
    QVERIFY(header != nullptr);
    QVERIFY(sidebar != nullptr);
    QVERIFY(grid != nullptr);
    QVERIFY(toast != nullptr);

    const QRect headerRect = itemRect(header);
    const QRect sidebarRect = itemRect(sidebar);
    const QRect gridRect = itemRect(grid);
    const QRect toastRect = itemRect(toast);
    QVERIFY(!headerRect.intersects(sidebarRect));
    QVERIFY(!headerRect.intersects(gridRect));
    QVERIFY(!headerRect.intersects(toastRect));
    QVERIFY(!sidebarRect.intersects(gridRect));
    QVERIFY(!sidebarRect.intersects(toastRect));

    const QImage image = window->grabWindow();
    QVERIFY(!image.isNull());
    saveScreenshot(image, QStringLiteral("shell-1600x900.png"));
}

void QmlVisualSmokeTest::usesAssetBackedIcons()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine, QSize(1280, 720));
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    for (const QString &objectName : {QStringLiteral("headerSidebarIcon"),
                                      QStringLiteral("windowCloseIcon")}) {
        QObject *icon = window->findChild<QObject *>(objectName);
        QVERIFY2(icon != nullptr, qPrintable(objectName));
        QVERIFY(!icon->property("source").toUrl().isEmpty());
    }

    QVERIFY(QFile::exists(QStringLiteral(":/qml/assets/icons/star.svg")));
}

void QmlVisualSmokeTest::rendersDanmakuFixtureInsideRoomTile()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine, QSize(1920, 1080));
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickItem *tile = previewRoomTile(window->contentItem());
    QVERIFY(tile != nullptr);

    FakeDanmakuVisualController danmaku;
    FakeRoomTileVisualController controller(&danmaku);
    tile->setProperty("controller", QVariant::fromValue(static_cast<QObject *>(&controller)));
    tile->setProperty("danmakuEnabled", true);
    danmaku.enqueueFixture();

    QTRY_VERIFY_WITH_TIMEOUT(itemByObjectName(tile, QStringLiteral("danmakuLine")) != nullptr, 1500);
    QTest::qWait(700);

    auto *line = itemByObjectName(tile, QStringLiteral("danmakuLine"));
    QVERIFY(line != nullptr);
    QVERIFY(line->y() >= 38);
    QVERIFY(line->y() + line->height() <= tile->height() - 56);

    const QImage image = window->grabWindow();
    QVERIFY(!image.isNull());
    const QPoint sceneTopLeft = tile->mapToScene(QPointF(0, 0)).toPoint();
    const QRect crop(sceneTopLeft, QSize(qRound(tile->width()), qRound(tile->height())));
    QVERIFY(image.rect().contains(crop));
    QVERIFY(brightPixels(image, crop) > 10);
    saveScreenshot(image, QStringLiteral("shell-1920x1080.png"));

    tile->setProperty("controller", QVariant::fromValue(static_cast<QObject *>(nullptr)));
    tile->setProperty("danmakuEnabled", false);
    QCoreApplication::processEvents();
}

void QmlVisualSmokeTest::showsDanmakuSettingsPanelInsideViewport()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine, QSize(1280, 720));
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    click(window->findChild<QObject *>(QStringLiteral("danmakuButton")));
    QObject *panel = window->findChild<QObject *>(QStringLiteral("danmakuSettingsPanel"));
    QVERIFY(panel != nullptr);
    QTRY_VERIFY(panel->property("visible").toBool());

    const QRect panelRect = itemRect(panel);
    QVERIFY(QRect(QPoint(0, 0), window->size()).contains(panelRect));

    const QImage image = window->grabWindow();
    QVERIFY(!image.isNull());
    QVERIFY(image.rect().contains(panelRect));
    QVERIFY(image.pixelColor(panelRect.x() + 8, panelRect.y() + 8) != QColor(QStringLiteral("#000000")));
    saveScreenshot(image, QStringLiteral("danmaku-settings-1280x720.png"));
}

QTEST_MAIN(QmlVisualSmokeTest)

#include "qml_visual_smoke_test.moc"
