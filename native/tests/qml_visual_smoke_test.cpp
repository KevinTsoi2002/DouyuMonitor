#include <QColor>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSValue>
#include <QDateTime>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSize>
#include <QtTest/QtTest>
#include <QtMath>
#include <utility>

#include "ui/mpv_quick_item.h"
#include "workspace/guild_roster.h"
#include "test_font_support.h"

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

class OfflineShellVisualController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList guildRoster READ guildRoster CONSTANT)
    Q_PROPERTY(QVariantList teams READ teams CONSTANT)
    Q_PROPERTY(QVariantMap s1Archive READ s1Archive CONSTANT)
    Q_PROPERTY(QObject *maoziRank READ unavailableObject CONSTANT)
    Q_PROPERTY(QObject *rooms READ unavailableObject CONSTANT)

public:
    OfflineShellVisualController(QVariantList roster, QVariantMap archive)
        : roster_(std::move(roster)), archive_(std::move(archive))
    {
    }

    QVariantList guildRoster() const { return roster_; }
    QVariantList teams() const { return {}; }
    QVariantMap s1Archive() const { return archive_; }
    QObject *unavailableObject() const { return nullptr; }
    Q_INVOKABLE void refreshGuildLiveStatus() {}
    Q_INVOKABLE QString addGuildMemberRoom(const QString &)
    {
        return QStringLiteral("离线视觉夹具不执行房间操作");
    }
    Q_INVOKABLE QString setGuildMemberRoomId(const QString &, const QString &)
    {
        return QStringLiteral("离线视觉夹具不执行房间操作");
    }
    Q_INVOKABLE bool openExternalUrl(const QString &) { return false; }

private:
    QVariantList roster_;
    QVariantMap archive_;
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
    if (auto *item = qobject_cast<QQuickItem *>(object)) {
        return {item->mapToScene(QPointF()).toPoint(),
                QSize(qRound(item->width()), qRound(item->height()))};
    }
    return {
        qRound(object->property("x").toDouble()),
        qRound(object->property("y").toDouble()),
        qRound(object->property("width").toDouble()),
        qRound(object->property("height").toDouble()),
    };
}

QRect sceneRect(QQuickItem *item)
{
    const QPoint topLeft = item->mapToScene(QPointF(0, 0)).toPoint();
    return {topLeft, QSize(qRound(item->width()), qRound(item->height()))};
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

QList<QQuickItem *> itemsByObjectName(QQuickItem *item, const QString &objectName)
{
    QList<QQuickItem *> result;
    if (item->objectName() == objectName) result.append(item);
    for (QQuickItem *child : item->childItems()) result.append(itemsByObjectName(child, objectName));
    return result;
}

QVariant qmlValue(QObject *object, const char *property)
{
    const QVariant value = object->property(property);
    return value.metaType() == QMetaType::fromType<QJSValue>() ? value.value<QJSValue>().toVariant() : value;
}

QQuickItem *containingScrollView(QQuickItem *item)
{
    for (QQuickItem *parent = item->parentItem(); parent != nullptr; parent = parent->parentItem()) {
        if (parent->property("availableWidth").isValid() && parent->property("contentHeight").isValid()
            && parent->property("contentItem").isValid()) return parent;
    }
    return nullptr;
}

QString subtreeText(QQuickItem *item)
{
    QString text = item->property("text").toString();
    for (QQuickItem *child : item->childItems()) text += QLatin1Char('\n') + subtreeText(child);
    return text;
}

bool fitsHorizontally(QQuickItem *item, QQuickItem *viewport)
{
    const QRect rect = sceneRect(item);
    const QRect bounds = sceneRect(viewport);
    return rect.width() > 0 && rect.left() >= bounds.left() - 1 && rect.right() <= bounds.right() + 1;
}

bool revealInScrollView(QQuickItem *scroll, QQuickItem *target)
{
    QObject *flickable = scroll->property("contentItem").value<QObject *>();
    if (flickable == nullptr) return false;
    const qreal current = flickable->property("contentY").toReal();
    const qreal maximum = qMax(qreal(0), flickable->property("contentHeight").toReal()
                                          - flickable->property("height").toReal());
    const qreal targetY = target->mapToScene(QPointF()).y() - scroll->mapToScene(QPointF()).y() + current;
    return flickable->setProperty("contentY", qBound(qreal(0), targetY, maximum));
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

QVariantList roomFixtures(int count)
{
    QVariantList rooms;
    for (int index = 0; index < count; ++index) {
        rooms.push_back(QVariantMap{
            {QStringLiteral("roomId"), QStringLiteral("fixture-%1").arg(index + 1)},
            {QStringLiteral("anchorName"), QStringLiteral("验收房间 %1").arg(index + 1)},
            {QStringLiteral("title"), QStringLiteral("多路画布视觉验收直播间 %1").arg(index + 1)},
            {QStringLiteral("category"), QStringLiteral("视觉验收")},
            {QStringLiteral("viewerLabel"), QStringLiteral("--")},
            {QStringLiteral("avatarUrl"), QUrl()},
            {QStringLiteral("liveState"), index == 0 ? QStringLiteral("online") : QStringLiteral("offline")},
            {QStringLiteral("playbackState"), QStringLiteral("idle")},
            {QStringLiteral("primary"), index == 0},
            {QStringLiteral("favorite"), false},
            {QStringLiteral("audioFocused"), false},
            {QStringLiteral("requestedQuality"), QStringLiteral("auto")},
            {QStringLiteral("requestedQualityRate"), -1},
            {QStringLiteral("effectiveQuality"), QStringLiteral("auto")},
            {QStringLiteral("availableQualities"), QVariantList{}},
            {QStringLiteral("muted"), true},
            {QStringLiteral("volume"), 100},
            {QStringLiteral("danmakuEnabled"), false},
            {QStringLiteral("danmakuState"), QStringLiteral("idle")},
            {QStringLiteral("danmakuErrorCode"), QStringLiteral("NONE")},
            {QStringLiteral("renderEnabled"), true},
            {QStringLiteral("index"), index},
        });
    }
    return rooms;
}

QList<QQuickItem *> roomTiles(QQuickItem *surface)
{
    QList<QQuickItem *> tiles;
    for (QQuickItem *child : surface->childItems()) {
        if (child->property("roomId").isValid()) tiles.push_back(child);
    }
    return tiles;
}

void addWindowSizeRows()
{
    QTest::addColumn<QSize>("size");
    QTest::newRow("960x600") << QSize(960, 600);
    QTest::newRow("1280x720") << QSize(1280, 720);
    QTest::newRow("1280x800") << QSize(1280, 800);
    QTest::newRow("1366x768") << QSize(1366, 768);
    QTest::newRow("1600x900") << QSize(1600, 900);
    QTest::newRow("1920x1080") << QSize(1920, 1080);
}

} // namespace

class QmlVisualSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void loadsReleasedModuleWithVisualAnchors();
    void showsStructuredEmptyWorkspace();
    void showsStatusBarForOccupiedWorkspace();
    void usesStableRoomCardInformationZones();
    void hasReferenceGeometryAt1280x720();
    void clampsNarrowWindowToSafeMinimum();
    void hasNoTextOverlapAt1600x900();
    void hasNoTextOverlapAt1920x1080();
    void usesAssetBackedIcons();
    void rendersDanmakuFixtureInsideRoomTile();
    void showsDanmakuSettingsPanelInsideViewport();
    void keepsSurfacesWithinViewportAcrossWindowSizes_data();
    void keepsSurfacesWithinViewportAcrossWindowSizes();
    void keepsVisibleRoomMetadataClearOfActions_data();
    void keepsVisibleRoomMetadataClearOfActions();
    void capturesReferenceRoomCounts();
    void showsSoundPanelInsideViewport();
    void capturesApprovedShellMatrix_data();
    void capturesApprovedShellMatrix();
};

void QmlVisualSmokeTest::initTestCase()
{
    QVERIFY2(DouyuTestSupport::prepareTestFonts(),
             "No Simplified Chinese font is available for QML tests (check QT_QPA_FONTDIR)");
}

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
    QCOMPARE(status->property("color").value<QColor>(), QColor(QStringLiteral("#1e2624")));
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

void QmlVisualSmokeTest::usesStableRoomCardInformationZones()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine, QSize(1600, 900));
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickItem *tile = previewRoomTile(window->contentItem());
    QVERIFY(tile != nullptr);
    auto *topMetadata = tile->findChild<QQuickItem *>(QStringLiteral("roomTopMetadata"));
    auto *title = tile->findChild<QQuickItem *>(QStringLiteral("roomTitleText"));
    auto *actions = tile->findChild<QQuickItem *>(QStringLiteral("roomActionBar"));
    QObject *surface = tile->findChild<QObject *>(QStringLiteral("roomCardSurface"));
    QObject *primaryLabel = tile->findChild<QObject *>(QStringLiteral("primaryRoomBadge"));
    QVERIFY(topMetadata != nullptr);
    QVERIFY(title != nullptr);
    QVERIFY(actions != nullptr);
    QVERIFY(surface != nullptr);
    QVERIFY(primaryLabel != nullptr);
    QVERIFY(primaryLabel->property("visible").toBool());
    QCOMPARE(primaryLabel->property("text").toString(), QStringLiteral("主画面"));
    QCOMPARE(surface->property("frameColor").value<QColor>(),
             tile->property("accentColor").value<QColor>());
    QVERIFY(sceneRect(title).right() < sceneRect(actions).left());
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
    QCOMPARE(qRound(header->property("height").toDouble()), 36);
    QObject *rail = window->findChild<QObject *>(QStringLiteral("appNavigationRail"));
    QVERIFY(rail != nullptr);
    QCOMPARE(qRound(rail->property("width").toDouble()), 80);
    QCOMPARE(itemRect(grid).left(), 80);

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

    click(window->findChild<QObject *>("sidebarToggleButton"));
    auto *drawer = window->findChild<QObject *>("roomToolsDrawer");
    QVERIFY(drawer != nullptr);
    QTRY_VERIFY(drawer->property("visible").toBool());
    QTRY_VERIFY(qobject_cast<QQuickItem *>(sidebar)->isVisible());

    const QRect headerRect = itemRect(header);
    const QRect sidebarRect = itemRect(sidebar);
    const QRect gridRect = itemRect(grid);
    const QRect toastRect = itemRect(toast);
    QVERIFY(!headerRect.intersects(sidebarRect));
    QVERIFY(!headerRect.intersects(gridRect));
    QVERIFY(!headerRect.intersects(toastRect));
    QVERIFY(sidebarRect.width() <= 400);


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

    click(window->findChild<QObject *>("sidebarToggleButton"));
    auto *drawer = window->findChild<QObject *>("roomToolsDrawer");
    QVERIFY(drawer != nullptr);
    QTRY_VERIFY(drawer->property("visible").toBool());
    QTRY_VERIFY(qobject_cast<QQuickItem *>(sidebar)->isVisible());

    const QRect headerRect = itemRect(header);
    const QRect sidebarRect = itemRect(sidebar);
    const QRect gridRect = itemRect(grid);
    const QRect toastRect = itemRect(toast);
    QVERIFY(!headerRect.intersects(sidebarRect));
    QVERIFY(!headerRect.intersects(gridRect));
    QVERIFY(!headerRect.intersects(toastRect));
    QVERIFY(sidebarRect.width() <= 400);


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

    for (const QString &objectName : {QStringLiteral("appIcon"),
                                      QStringLiteral("hamsterNavigationIcon"),
                                      QStringLiteral("brandIcon"),
                                      QStringLiteral("windowCloseIcon")}) {
        QObject *icon = window->findChild<QObject *>(objectName);
        QVERIFY2(icon != nullptr, qPrintable(objectName));
        QVERIFY(!icon->property("source").toUrl().isEmpty());
        QTRY_COMPARE_WITH_TIMEOUT(icon->property("status").toInt(), 1, 1000);
    }

    QObject *navigationIcon =
        window->findChild<QObject *>(QStringLiteral("hamsterNavigationIcon"));
    QVERIFY(navigationIcon != nullptr);
    QCOMPARE(navigationIcon->property("width").toInt(), 40);
    QCOMPARE(navigationIcon->property("height").toInt(), 40);
    QVERIFY(navigationIcon->property("source").toUrl().toString().endsWith(
        QStringLiteral("icon-navigation.png")));

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
    QVERIFY(line->y() >= 36);
    QVERIFY(line->y() + line->height() <= tile->height() - 58);

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
    QQuickWindow *window = loadWindow(engine, QSize(1600, 900));
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
    saveScreenshot(image, QStringLiteral("danmaku-panel-1600x900.png"));
}

void QmlVisualSmokeTest::keepsSurfacesWithinViewportAcrossWindowSizes_data()
{
    addWindowSizeRows();
}

void QmlVisualSmokeTest::keepsSurfacesWithinViewportAcrossWindowSizes()
{
    QFETCH(QSize, size);
    registerQmlTypes();
    const struct Surface {
        const char *button;
        const char *panel;
    } surfaces[] = {
        {"danmakuButton", "danmakuSettingsPanel"},
        {"monitoringButton", "monitoringDrawer"},
        {"workspaceButton", "workspacePresetsPanel"},
        {"soundMasterButton", "soundMasterPopover"},
        {"layoutMenuButton", "layoutMenu"},
    };

    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine, size);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QCOMPARE(window->size(), size);
    QTRY_COMPARE_WITH_TIMEOUT(window->contentItem()->width(), qreal(size.width()), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->contentItem()->height(), qreal(size.height()), 1000);

    const QRect viewport(QPoint(0, 0), size);
    for (const QString &name : {QStringLiteral("appHeader"),
                                QStringLiteral("roomSidebar"),
                                QStringLiteral("workspaceGrid"),
                                QStringLiteral("workspaceStatusBar")}) {
        auto *item = window->findChild<QQuickItem *>(name);
        QVERIFY2(item != nullptr, qPrintable(name));
        QTRY_VERIFY2_WITH_TIMEOUT(viewport.contains(sceneRect(item)), qPrintable(name), 1000);
    }

    const QImage shell = window->grabWindow();
    QVERIFY(!shell.isNull());
    QVERIFY(shell.rect().contains(viewport));
    QVERIFY(shell.pixelColor(300, 60) != QColor(QStringLiteral("#000000")));

    for (const Surface &surface : surfaces) {
        auto *tools = window->findChild<QObject *>("watchToolsPanel");
        if (!tools->property("visible").toBool()) click(window->findChild<QObject *>("watchToolsButton"));
        click(window->findChild<QObject *>(QString::fromLatin1(surface.button)));
        QObject *panel = window->findChild<QObject *>(QString::fromLatin1(surface.panel));
        QVERIFY2(panel != nullptr, surface.panel);
        QTRY_VERIFY(panel->property("visible").toBool());
        QTRY_COMPARE(panel->property("opacity").toDouble(), 1.0);
        QTRY_VERIFY2_WITH_TIMEOUT(viewport.contains(itemRect(panel)), surface.panel, 1000);

        const QImage image = window->grabWindow();
        QVERIFY(!image.isNull());
        const QRect panelRect = itemRect(panel);
        QVERIFY(image.rect().contains(panelRect));
        QVERIFY(image.pixelColor(panelRect.topLeft() + QPoint(8, 8))
                != QColor(QStringLiteral("#000000")));
        QVERIFY(brightPixels(image, panelRect) > 0);
        saveScreenshot(image, QStringLiteral("viewport-%1-%2x%3.png")
                                  .arg(QString::fromLatin1(surface.panel))
                                  .arg(size.width()).arg(size.height()));
        QVERIFY(QMetaObject::invokeMethod(panel, "close"));
        QTRY_VERIFY(!panel->property("visible").toBool());
    }
}

void QmlVisualSmokeTest::keepsVisibleRoomMetadataClearOfActions_data()
{
    addWindowSizeRows();
}

void QmlVisualSmokeTest::keepsVisibleRoomMetadataClearOfActions()
{
    QFETCH(QSize, size);
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine, size);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QTRY_COMPARE_WITH_TIMEOUT(window->contentItem()->width(), qreal(size.width()), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(window->contentItem()->height(), qreal(size.height()), 1000);
    QQuickItem *tile = previewRoomTile(window->contentItem());
    QVERIFY(tile != nullptr);
    QVERIFY(QMetaObject::invokeMethod(tile, "revealControls"));
    auto *bottomBar = tile->findChild<QQuickItem *>(QStringLiteral("roomBottomBar"));
    auto *actions = tile->findChild<QQuickItem *>(QStringLiteral("roomActionBar"));
    QVERIFY(bottomBar != nullptr);
    QVERIFY(actions != nullptr);
    QTRY_COMPARE(bottomBar->opacity(), 1.0);
    QVERIFY(bottomBar->isVisible());
    saveScreenshot(window->grabWindow(), QStringLiteral("room-metadata-%1x%2.png")
                                            .arg(size.width()).arg(size.height()));

    for (const QString &name : {QStringLiteral("roomAnchorName"),
                                QStringLiteral("roomTitleText")}) {
        auto *text = tile->findChild<QQuickItem *>(name);
        QVERIFY2(text != nullptr, qPrintable(name));
        if (!text->isVisible()) continue;
        const QRect textRect = sceneRect(text);
        const QRect actionsRect = sceneRect(actions);
        const QString location = QStringLiteral("%1 %2x%3 density=%4 tileWidth=%5 "
                                                 "text=(%6,%7 %8x%9) actions=(%10,%11 %12x%13)")
                                     .arg(name).arg(size.width()).arg(size.height())
                                     .arg(tile->property("density").toString()).arg(tile->width())
                                     .arg(textRect.x()).arg(textRect.y())
                                     .arg(textRect.width()).arg(textRect.height())
                                     .arg(actionsRect.x()).arg(actionsRect.y())
                                     .arg(actionsRect.width()).arg(actionsRect.height());
        QVERIFY2(text->width() > 0 && text->height() > 0, qPrintable(location));
        QVERIFY2(sceneRect(bottomBar).contains(textRect), qPrintable(location));
        QVERIFY2(!textRect.intersects(actionsRect), qPrintable(location));
    }
}

void QmlVisualSmokeTest::capturesReferenceRoomCounts()
{
    registerQmlTypes();

    const struct Fixture {
        int count;
        QSize size;
        QString name;
    } fixtures[] = {
        {1, QSize(1280, 720), QStringLiteral("one-room-1280x720.png")},
        {4, QSize(1600, 900), QStringLiteral("four-rooms-1600x900.png")},
        {9, QSize(1920, 1080), QStringLiteral("nine-rooms-1920x1080.png")},
        {12, QSize(1920, 1080), QStringLiteral("twelve-rooms-1920x1080.png")},
        {16, QSize(1920, 1080), QStringLiteral("sixteen-rooms-1920x1080.png")},
        {24, QSize(1920, 1080), QStringLiteral("twenty-four-rooms-1920x1080.png")},
    };

    for (const Fixture &fixture : fixtures) {
        QQmlApplicationEngine engine;
        QQmlComponent component(&engine,
                                QUrl(QStringLiteral("qrc:/qml/components/WorkspaceGrid.qml")));
        QVERIFY2(component.isReady(), qPrintable(component.errorString()));

        QQuickWindow hostWindow;
        hostWindow.resize(fixture.size);
        hostWindow.show();
        QVERIFY(QTest::qWaitForWindowExposed(&hostWindow));
        const QSize contentSize(qRound(hostWindow.contentItem()->width()),
                                qRound(hostWindow.contentItem()->height()));
        QVERIFY(contentSize.width() > 0);
        QVERIFY(contentSize.height() > 0);

        std::unique_ptr<QObject> grid(component.createWithInitialProperties({
            {QStringLiteral("parent"), QVariant::fromValue(hostWindow.contentItem())},
            {QStringLiteral("width"), contentSize.width()},
            {QStringLiteral("height"), contentSize.height()},
            {QStringLiteral("roomModel"), roomFixtures(fixture.count)},
            {QStringLiteral("layoutMode"), QStringLiteral("auto")},
            {QStringLiteral("primaryRoomId"), QStringLiteral("fixture-1")},
        }));
        QVERIFY2(grid != nullptr, qPrintable(component.errorString()));

        auto *surface = grid->findChild<QQuickItem *>(QStringLiteral("layoutSurface"));
        QVERIFY(surface != nullptr);
        const QList<QQuickItem *> tiles = roomTiles(surface);
        QCOMPARE(tiles.size(), fixture.count);

        const QImage image = hostWindow.grabWindow();
        QVERIFY(!image.isNull());
        const QRect viewport(QPoint(0, 0), hostWindow.size());
        const QRect surfaceRect = sceneRect(surface);
        QVERIFY(viewport.contains(surfaceRect));
        QVERIFY(image.rect().contains(surfaceRect));
        QVERIFY(image.pixelColor(surfaceRect.center()) != QColor(QStringLiteral("#000000")));
        for (QQuickItem *tile : tiles) {
            QVERIFY(viewport.contains(sceneRect(tile)));
            auto *title = tile->findChild<QQuickItem *>(QStringLiteral("roomTitleText"));
            auto *actions = tile->findChild<QQuickItem *>(QStringLiteral("roomActionBar"));
            QVERIFY(title != nullptr);
            QVERIFY(actions != nullptr);
            QVERIFY(!sceneRect(title).intersects(sceneRect(actions)));
        }
        saveScreenshot(image, fixture.name);
    }
}

void QmlVisualSmokeTest::capturesApprovedShellMatrix_data()
{
    QTest::addColumn<QSize>("size");
    QTest::addColumn<bool>("day");
    for (const auto &size : {QSize(960, 600), QSize(1280, 800), QSize(1366, 768), QSize(1920, 1080)}) {
        for (const bool day : {false, true}) {
            const QByteArray name = QStringLiteral("%1-%2x%3").arg(day ? "day" : "night")
                                        .arg(size.width()).arg(size.height()).toLatin1();
            QTest::newRow(name.constData()) << size << day;
        }
    }
}

void QmlVisualSmokeTest::capturesApprovedShellMatrix()
{
    QFETCH(QSize, size);
    QFETCH(bool, day);
    registerQmlTypes();
    const QString archivePath = QFINDTESTDATA("../app/resources/cstg_s1_archive.json");
    QVERIFY(!archivePath.isEmpty());
    QFile archiveFile(archivePath);
    QVERIFY(archiveFile.open(QIODevice::ReadOnly));
    QJsonParseError archiveError;
    const QJsonDocument archiveDocument = QJsonDocument::fromJson(archiveFile.readAll(), &archiveError);
    QCOMPARE(archiveError.error, QJsonParseError::NoError);
    QVERIFY(archiveDocument.isObject());
    const QVector<GuildMember> members = GuildRoster::bundled();
    const QVariantMap archive = archiveDocument.object().toVariantMap();
    QCOMPARE(members.size(), 75);
    QCOMPARE(archive.value(QStringLiteral("ranking")).toMap()
                 .value(QStringLiteral("participants")).toList().size(), 46);
    QCOMPARE(archive.value(QStringLiteral("placement")).toMap()
                 .value(QStringLiteral("participants")).toList().size(), 36);
    QVariantList roster;
    int directoryCount = 0;
    const QString portraitFixture = QStringLiteral("qrc:/qml/assets/brand/icon-app.png");
    for (const GuildMember &member : members) {
        if (member.directoryListed) ++directoryCount;
        roster.append(QVariantMap{
            {QStringLiteral("id"), member.id},
            {QStringLiteral("anchorName"), member.directoryName.isEmpty() ? member.anchorName : member.directoryName},
            {QStringLiteral("pinyinKey"), member.pinyinKey},
            {QStringLiteral("roomId"), member.roomId},
            {QStringLiteral("directoryListed"), member.directoryListed},
            {QStringLiteral("directoryOrder"), member.directoryOrder},
            {QStringLiteral("directoryName"), member.directoryName},
            {QStringLiteral("gender"), member.gender},
            {QStringLiteral("status"), QString()}, {QStringLiteral("avatarUrl"), portraitFixture},
            {QStringLiteral("liveState"), QStringLiteral("unknown")},
            {QStringLiteral("active"), false}, {QStringLiteral("role"), QStringLiteral("other")},
            {QStringLiteral("rankMatched"), false}, {QStringLiteral("rankHostId"), QString()},
            {QStringLiteral("radarDimensions"), QVariantList{}}, {QStringLiteral("score"), -1},
            {QStringLiteral("grade"), QString()}, {QStringLiteral("gradeColor"), QString()},
            {QStringLiteral("placementAverage"), -1}, {QStringLiteral("placementScoredSessions"), 0},
            {QStringLiteral("playValue"), QString()}, {QStringLiteral("playValueBombed"), 0},
            {QStringLiteral("playValueUpdatedAt"), QString()},
        });
    }
    QCOMPARE(directoryCount, 62);
    OfflineShellVisualController fixture(roster, archive);
    QQmlApplicationEngine engine;
    auto *window = loadWindow(engine, size);
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));
    // Capture settled colors, not the middle of the 120 ms theme transition.
    QVERIFY(window->setProperty("reducedMotion", true));
    auto *navigation = window->findChild<QObject *>("guildNavigationPanel");
    auto *ranking = window->findChild<QObject *>("maoziRankPage");
    QVERIFY(navigation && ranking);
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    const QVariant fixtureObject = QVariant::fromValue(static_cast<QObject *>(&fixture));
    QVERIFY(navigation->setProperty("controller", fixtureObject));
    QVERIFY(ranking->setProperty("controller", fixtureObject));
    if (day) click(window->findChild<QObject *>("themeToggleButton"));
    QTRY_COMPARE(window->color(), QColor(day ? "#f0f3ed" : "#0e1312"));
    const QString prefix = QStringLiteral("repair-%1-%2x%3-").arg(day ? "day" : "night")
                               .arg(size.width()).arg(size.height());
    QJsonArray captures;
    const auto capture = [&](const QString &name) {
        const QImage image = window->grabWindow();
        QVERIFY(!image.isNull());
        const QString filename = prefix + name + ".png";
        saveScreenshot(image, filename);
        captures.append(QJsonObject{{"file", filename}, {"state", name},
                                    {"pixelWidth", image.width()}, {"pixelHeight", image.height()}});
    };
    auto *grid = window->findChild<QQuickItem *>("workspaceGrid");
    auto *header = window->findChild<QQuickItem *>("appHeader");
    auto *rail = window->findChild<QQuickItem *>("appNavigationRail");
    QVERIFY(grid && header && rail);
    QCOMPARE(header->height(), qreal(36));
    QCOMPARE(rail->width(), qreal(80));
    QTRY_COMPARE(grid->width(), qreal(size.width() - 80));
    QTRY_COMPARE(grid->height(), qreal(size.height() - 80));
    const QRectF geometry(grid->position(), grid->size());
    const QRect viewport(QPoint(), size);
    const struct Page { const char *button; const char *object; const char *name; } pages[] = {
        {"watchWorkspaceButton", "workspaceGrid", "watch"},
        {"hamsterNavigationButton", "guildNavigationPanel", "navigation"},
        {"maoziRankButton", "maoziRankPage", "ranking"},
        {"navigationSettingsButton", "settingsPage", "settings"},
    };
    for (const Page &page : pages) {
        click(window->findChild<QObject *>(QString::fromLatin1(page.button)));
        auto *item = window->findChild<QQuickItem *>(QString::fromLatin1(page.object));
        QVERIFY2(item != nullptr, page.object);
        QTRY_VERIFY(item->isVisible());
        QTRY_VERIFY(viewport.contains(sceneRect(item)));
        if (QByteArray(page.name) == "navigation") {
            QTRY_COMPARE(navigation->property("visibleMemberCount").toInt(), directoryCount);
        } else if (QByteArray(page.name) == "ranking") {
            auto *list = ranking->findChild<QObject *>("maoziRankList");
            QVERIFY(list != nullptr);
            QTRY_COMPARE(list->property("count").toInt(), 46);
            QVERIFY(ranking->property("sourceStatus").toString().contains("2026-10-27"));
            QVERIFY(ranking->property("archivedRanking").toBool());
            QQuickItem *row = nullptr;
            QTRY_VERIFY(QMetaObject::invokeMethod(list, "itemAtIndex",
                                                 Q_RETURN_ARG(QQuickItem *, row), Q_ARG(int, 0))
                        && row != nullptr);
            auto *score = row->findChild<QObject *>("maoziCellScore");
            QVERIFY(score != nullptr);
            QTRY_COMPARE(score->property("text").toString(), QStringLiteral("15.5"));
        }
        QTest::qWait(100);
        capture(QString::fromLatin1(page.name));
    }

    // Exercise the actual detail components, including regions below the fold.
    click(window->findChild<QObject *>("maoziRankButton"));
    QVERIFY(ranking->setProperty("selectedRankIndex", 0));
    auto *rankingItem = qobject_cast<QQuickItem *>(ranking);
    QVERIFY(rankingItem != nullptr);
    auto *rankScroll = ranking->findChild<QQuickItem *>("maoziRankDetailScroll");
    auto *rankContent = ranking->findChild<QQuickItem *>("maoziRankParticipantDetail");
    auto *radar = ranking->findChild<QQuickItem *>("maoziRankRadar");
    auto *portrait = ranking->findChild<QQuickItem *>("maoziRankDetailAvatar");
    QVERIFY(rankScroll && rankContent && radar && portrait);
    QTRY_VERIFY(rankScroll->isVisible());
    QTRY_COMPARE(radar->property("validValueCount").toInt(), 9);
    QVERIFY(radar->property("completeData").toBool());
    QTRY_COMPARE(portrait->property("status").toInt(), 1); // QQuickImage::Ready
    QCOMPARE(portrait->property("source").toUrl(), QUrl(portraitFixture));
    QCOMPARE(qmlValue(rankContent, "record").toMap().value("score").toString(), QStringLiteral("15.51"));
    QTest::qWait(100);
    QVERIFY(viewport.contains(sceneRect(rankScroll)));
    QVERIFY(fitsHorizontally(rankContent, rankScroll));
    QVERIFY(fitsHorizontally(radar, rankScroll));
    QVERIFY(radar->width() >= 250 && radar->height() >= 250);
    const auto dimensionValues = itemsByObjectName(rankingItem, QStringLiteral("maoziRankDimensionValue"));
    QCOMPARE(dimensionValues.size(), 9);
    for (QQuickItem *value : dimensionValues) {
        QVERIFY(fitsHorizontally(value, rankScroll));
        QVERIFY(!value->property("text").toString().isEmpty());
    }
    QList<QRect> radarLabelRects;
    for (QQuickItem *label : radar->childItems()) {
        if (!label->property("text").toString().contains(QLatin1Char('\n'))) continue;
        const QRect labelRect = sceneRect(label);
        QVERIFY(sceneRect(radar).contains(labelRect));
        // Text items reserve a wider centered slot; compare the actual glyph bounds.
        const QSize paintedSize(qCeil(label->property("contentWidth").toReal()),
                                qCeil(label->property("contentHeight").toReal()));
        QVERIFY(paintedSize.width() > 0 && paintedSize.height() > 0);
        const QRect rect(labelRect.center() - QPoint(paintedSize.width() / 2, paintedSize.height() / 2), paintedSize);
        for (const QRect &other : radarLabelRects) QVERIFY(!rect.intersects(other));
        radarLabelRects.append(rect);
    }
    QCOMPARE(radarLabelRects.size(), 9);
    capture(QStringLiteral("ranking-detail-header"));
    QVERIFY(revealInScrollView(rankScroll, radar));
    QTest::qWait(100);
    capture(QStringLiteral("ranking-detail-radar"));
    if (rankScroll->height() < radar->height()) {
        QVERIFY(revealInScrollView(rankScroll, dimensionValues.constLast()));
        QTest::qWait(100);
        capture(QStringLiteral("ranking-detail-lower"));
    }
    click(ranking->findChild<QObject *>("maoziRankDetailBack"));
    QTRY_COMPARE(ranking->property("selectedRankIndex").toInt(), -1);

    const struct ArchivePage {
        const char *tab;
        const char *panel;
        const char *name;
        int rounds;
        const char *leadingScore;
    } archives[] = {
        {"maoziRegularSeasonTab", "regularSeasonPanel", "regular-detail", 6, "6分"},
        {"maoziFinalsTab", "finalsPanel", "finals-detail", 9, "26分"},
    };
    for (const ArchivePage &entry : archives) {
        click(ranking->findChild<QObject *>(QString::fromLatin1(entry.tab)));
        auto *panel = ranking->findChild<QQuickItem *>(QString::fromLatin1(entry.panel));
        QVERIFY(panel != nullptr);
        QTRY_VERIFY(panel->isVisible());
        QVERIFY(panel->setProperty("selectedIndex", 0));
        QQuickItem *outcome = nullptr;
        QTRY_VERIFY((outcome = itemByObjectName(panel, QStringLiteral("seasonMatchOutcome"))) != nullptr);
        auto *scroll = containingScrollView(outcome);
        QVERIFY(scroll != nullptr);
        QTRY_VERIFY(viewport.contains(sceneRect(scroll)));
        QVERIFY(fitsHorizontally(outcome, scroll));
        const auto standings = itemsByObjectName(panel, QStringLiteral("seasonStandingRow"));
        const auto rounds = itemsByObjectName(panel, QStringLiteral("seasonRoundCard"));
        QCOMPARE(standings.size(), 4);
        QCOMPARE(rounds.size(), entry.rounds);
        QVERIFY(subtreeText(standings.constFirst()).contains(QString::fromUtf8(entry.leadingScore)));
        for (QQuickItem *row : standings) QVERIFY(fitsHorizontally(row, scroll));
        for (QQuickItem *round : rounds) QVERIFY(fitsHorizontally(round, scroll));
        QTest::qWait(100);
        capture(QString::fromLatin1(entry.name) + QStringLiteral("-header"));
        QVERIFY(revealInScrollView(scroll, rounds.constFirst()));
        QTest::qWait(100);
        capture(QString::fromLatin1(entry.name) + QStringLiteral("-rounds"));
        QVERIFY(rounds.constLast()->setProperty("expanded", true));
        QTest::qWait(100);
        QVERIFY(revealInScrollView(scroll, rounds.constLast()));
        QTest::qWait(100);
        capture(QString::fromLatin1(entry.name) + QStringLiteral("-lower"));
        auto *sourceToggle = itemByObjectName(panel, QStringLiteral("seasonArchiveSourceToggle"));
        auto *source = itemByObjectName(panel, QStringLiteral("seasonArchiveSourceText"));
        QVERIFY(sourceToggle && source);
        click(sourceToggle);
        QTRY_VERIFY(source->isVisible());
        QVERIFY(source->property("text").toString().contains(QString::fromUtf8(entry.leadingScore)));
        QVERIFY(fitsHorizontally(source, scroll));
        click(panel->findChild<QObject *>("seasonArchiveBack"));
        QTRY_COMPARE(panel->property("selectedIndex").toInt(), -1);
    }

    click(ranking->findChild<QObject *>("maoziFootballTab"));
    auto *football = ranking->findChild<QQuickItem *>("footballPanel");
    QVERIFY(football != nullptr);
    QTRY_VERIFY(football->isVisible());
    QVERIFY(football->setProperty("selectedIndex", 0));
    QQuickItem *scoreboard = nullptr;
    QTRY_VERIFY((scoreboard = itemByObjectName(football, QStringLiteral("footballScoreboard"))) != nullptr);
    auto *footballScroll = containingScrollView(scoreboard);
    auto *scoreText = itemByObjectName(football, QStringLiteral("footballScoreText"));
    QVERIFY(footballScroll && scoreText);
    QTRY_VERIFY(viewport.contains(sceneRect(footballScroll)));
    QCOMPARE(scoreText->property("text").toString(), QStringLiteral("5 : 1"));
    const auto lineups = itemsByObjectName(football, QStringLiteral("footballLineupPlayer"));
    const auto events = itemsByObjectName(football, QStringLiteral("footballEventRow"));
    const auto votes = itemsByObjectName(football, QStringLiteral("footballMvpRow"));
    QCOMPARE(lineups.size(), 8);
    QCOMPARE(events.size(), 31);
    QCOMPARE(votes.size(), 18);
    QVERIFY(fitsHorizontally(scoreboard, footballScroll));
    for (const auto &section : {lineups, events, votes}) {
        for (QQuickItem *row : section) QVERIFY(fitsHorizontally(row, footballScroll));
    }
    QVERIFY(subtreeText(events.constFirst()).contains(QStringLiteral("上半场开始")));
    QVERIFY(subtreeText(events.constLast()).contains(QStringLiteral("下半场结束")));
    QTest::qWait(100);
    capture(QStringLiteral("football-detail-scoreboard"));
    QVERIFY(revealInScrollView(footballScroll, events.constFirst()));
    QTest::qWait(100);
    capture(QStringLiteral("football-detail-events"));
    QVERIFY(revealInScrollView(footballScroll, votes.constFirst()));
    QTest::qWait(100);
    capture(QStringLiteral("football-detail-mvp"));
    auto *footballSourceToggle = itemByObjectName(football, QStringLiteral("footballArchiveSourceToggle"));
    auto *footballSource = itemByObjectName(football, QStringLiteral("footballArchiveSourceText"));
    QVERIFY(footballSourceToggle && footballSource);
    click(footballSourceToggle);
    QTRY_VERIFY(footballSource->isVisible());
    QVERIFY(footballSource->property("text").toString().contains(QStringLiteral("31+4′")));
    QVERIFY(fitsHorizontally(footballSource, footballScroll));
    click(football->findChild<QObject *>("seasonArchiveBack"));
    QTRY_COMPARE(football->property("selectedIndex").toInt(), -1);

    click(window->findChild<QObject *>("watchWorkspaceButton"));
    click(window->findChild<QObject *>("watchToolsButton"));
    auto *tools = window->findChild<QObject *>("watchToolsPanel");
    QVERIFY(tools != nullptr);
    QTRY_VERIFY(tools->property("visible").toBool());
    QVERIFY(viewport.contains(itemRect(tools)));
    QCOMPARE(window->findChild<QQuickItem *>("workspaceGrid"), grid);
    QCOMPARE(QRectF(grid->position(), grid->size()), geometry);
    capture(QStringLiteral("tools-overview"));
    for (const auto &entry : {std::pair{"soundMasterButton", "soundMasterPopover"},
                              std::pair{"layoutMenuButton", "layoutMenu"}}) {
        click(tools->findChild<QObject *>(QString::fromLatin1(entry.first)));
        auto *section = tools->findChild<QQuickItem *>(QString::fromLatin1(entry.second));
        QVERIFY(section != nullptr);
        QTRY_VERIFY(section->isVisible());
        QVERIFY(viewport.contains(sceneRect(section)));
        QTest::qWait(100);
        capture(QString::fromLatin1(entry.second));
        click(tools->findChild<QObject *>("watchToolsBackButton"));
        QTRY_VERIFY(!section->isVisible());
    }
    click(tools->findChild<QObject *>("sidebarToggleButton"));
    auto *drawer = window->findChild<QObject *>("roomToolsDrawer");
    QVERIFY(drawer != nullptr);
    QTRY_VERIFY(drawer->property("visible").toBool());
    QTRY_VERIFY(!tools->property("visible").toBool());
    capture(QStringLiteral("room-drawer"));
    click(window->findChild<QObject *>("closeRoomToolsButton"));
    QTRY_VERIFY(!drawer->property("visible").toBool());

    click(window->findChild<QObject *>("hamsterNavigationButton"));
    auto *filter = navigation->findChild<QObject *>("guildStatusFilter");
    QVERIFY(filter != nullptr);
    auto *filterPopup = filter->property("popup").value<QObject *>();
    QVERIFY(filterPopup != nullptr);
    QVERIFY(QMetaObject::invokeMethod(filterPopup, "open"));
    QTRY_VERIFY(filterPopup->property("visible").toBool());
    QTest::qWait(100);
    capture(QStringLiteral("navigation-filter"));
    QVERIFY(QMetaObject::invokeMethod(filterPopup, "close"));
    QTRY_VERIFY(!filterPopup->property("visible").toBool());

    click(window->findChild<QObject *>("watchWorkspaceButton"));
    QCOMPARE(window->findChild<QQuickItem *>("workspaceGrid"), grid);
    QCOMPARE(QRectF(grid->position(), grid->size()), geometry);
    if (day) {
        window->setProperty("showPreviewRooms", false);
        auto *emptyAdd = window->findChild<QQuickItem *>("emptyAddRoomButton");
        QVERIFY(emptyAdd != nullptr);
        QTRY_VERIFY(emptyAdd->isVisible());
        capture(QStringLiteral("empty-watch"));
    }
    const QJsonObject manifest{
        {"capturedAtUtc", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)},
        {"theme", day ? "day" : "night"}, {"width", size.width()}, {"height", size.height()},
        {"provenance", "Offline test fixture: GuildRoster::bundled() directory metadata and immutable S1 archive; no current live status or playback is asserted."},
        {"portraitPolicy", "Every fixture roster avatar uses qrc:/qml/assets/brand/icon-app.png. It verifies image loading and identity fallback only; it is not a real participant portrait or proof of network availability."},
        {"imagePolicy", "Unmodified QQuickWindow grabWindow output; no painted provenance banner."},
        {"rosterCount", int(members.size())}, {"directoryCount", directoryCount},
        {"rankingCount", 46}, {"captures", captures},
    };
    QFile manifestFile(QStringLiteral(QML_VERIFICATION_DIR) + '/' + prefix + "manifest.json");
    QVERIFY(manifestFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    const QByteArray manifestData = QJsonDocument(manifest).toJson(QJsonDocument::Indented);
    QCOMPARE(manifestFile.write(manifestData), qint64(manifestData.size()));
    manifestFile.close();
    QCOMPARE(warnings.count(), 0);
}

void QmlVisualSmokeTest::showsSoundPanelInsideViewport()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine, QSize(1600, 900));
    QVERIFY(window != nullptr);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    click(window->findChild<QObject *>(QStringLiteral("watchToolsButton")));
    click(window->findChild<QObject *>(QStringLiteral("soundMasterButton")));
    QObject *panel = window->findChild<QObject *>(QStringLiteral("soundMasterPopover"));
    QVERIFY(panel != nullptr);
    QTRY_VERIFY(panel->property("visible").toBool());
    QTRY_COMPARE(panel->property("opacity").toDouble(), 1.0);

    const QRect panelRect = itemRect(panel);
    QVERIFY(QRect(QPoint(0, 0), window->size()).contains(panelRect));
    const QImage image = window->grabWindow();
    QVERIFY(!image.isNull());
    QVERIFY(image.rect().contains(panelRect));
    QVERIFY(image.pixelColor(panelRect.center()) != QColor(QStringLiteral("#000000")));
    saveScreenshot(image, QStringLiteral("sound-panel-1600x900.png"));
}

QTEST_MAIN(QmlVisualSmokeTest)

#include "qml_visual_smoke_test.moc"
