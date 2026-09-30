#include <QColor>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSize>
#include <QVariantList>
#include <QVariantMap>
#include <QtTest/QtTest>

#include <memory>

#include "ui/mpv_quick_item.h"
#include "visual_test_support.h"

namespace {

void registerQmlTypes()
{
    static const int registered = qmlRegisterType<MpvQuickItem>("DouyuNative", 1, 0, "MpvQuickItem");
    Q_UNUSED(registered);
}

QUrl qmlSource(const QString &relativePath)
{
    return QUrl::fromLocalFile(QStringLiteral(QML_TEST_SOURCE_DIR)
                               + QLatin1Char('/') + relativePath);
}

std::unique_ptr<QObject> createQmlObject(QQmlApplicationEngine &engine,
                                         QQuickWindow &window,
                                         const QString &relativePath,
                                         const QVariantMap &properties,
                                         QString *error)
{
    QQmlComponent component(&engine, qmlSource(relativePath));
    if (!component.isReady()) {
        if (error != nullptr) *error = component.errorString();
        return {};
    }
    QVariantMap initial = properties;
    initial.insert(QStringLiteral("parent"),
                   QVariant::fromValue(window.contentItem()));
    if (error != nullptr) error->clear();
    return std::unique_ptr<QObject>(component.createWithInitialProperties(initial));
}

QQuickWindow *createHostWindow(const QSize &size)
{
    auto *window = new QQuickWindow;
    window->resize(size);
    window->show();
    if (!QTest::qWaitForWindowExposed(window)) {
        delete window;
        return nullptr;
    }
    return window;
}

QQuickItem *layoutSurface(QObject *grid)
{
    return grid->findChild<QQuickItem *>(QStringLiteral("layoutSurface"));
}

QList<QQuickItem *> roomTiles(QQuickItem *surface)
{
    QList<QQuickItem *> tiles;
    for (QQuickItem *child : surface->childItems()) {
        if (child->property("roomId").isValid()) tiles.push_back(child);
    }
    return tiles;
}

void verifyNoOverlaps(const QList<QQuickItem *> &tiles)
{
    for (int firstIndex = 0; firstIndex < tiles.size(); ++firstIndex) {
        for (int secondIndex = firstIndex + 1; secondIndex < tiles.size(); ++secondIndex) {
            QVERIFY2(!visualRectsOverlap(visualSceneRect(tiles.at(firstIndex)),
                                         visualSceneRect(tiles.at(secondIndex))),
                     qPrintable(QStringLiteral("Tiles %1 and %2 overlap")
                                    .arg(firstIndex)
                                    .arg(secondIndex)));
        }
    }
}

void verifyStableTileZones(const QList<QQuickItem *> &tiles)
{
    for (QQuickItem *tile : tiles) {
        QQuickItem *topMetadata = tile->findChild<QQuickItem *>(QStringLiteral("roomTopMetadata"));
        QQuickItem *topActions = tile->findChild<QQuickItem *>(QStringLiteral("roomTopActions"));
        QQuickItem *title = tile->findChild<QQuickItem *>(QStringLiteral("roomTitleText"));
        QQuickItem *actions = tile->findChild<QQuickItem *>(QStringLiteral("roomActionBar"));
        QVERIFY(topMetadata != nullptr);
        QVERIFY(topActions != nullptr);
        QVERIFY(title != nullptr);
        QVERIFY(actions != nullptr);
        QVERIFY(!visualRectsOverlap(visualSceneRect(topMetadata), visualSceneRect(topActions)));
        QVERIFY(!visualRectsOverlap(visualSceneRect(title), visualSceneRect(actions)));
    }
}

void verifyBaseline(const QImage &image, const QString &name)
{
    const VisualComparisonResult result = visualCompareWithBaseline(image, name);
    QVERIFY2(result.ok, qPrintable(result.message));
}

class FakeRooms final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int roomCount READ roomCount NOTIFY roomCountChanged)

public:
    int roomCount() const noexcept { return roomCount_; }

    void setRoomCount(int count)
    {
        if (roomCount_ == count) return;
        roomCount_ = count;
        emit roomCountChanged();
    }

signals:
    void roomCountChanged();

private:
    int roomCount_ = 0;
};

class FakeHeaderController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject *rooms READ rooms CONSTANT)
    Q_PROPERTY(QObject *workspace READ workspace CONSTANT)

public:
    QObject *rooms() noexcept { return &rooms_; }
    QObject *workspace() const noexcept { return nullptr; }
    Q_INVOKABLE bool setLayout(const QString &) { return true; }

    FakeRooms rooms_;
};

QVariantList guildFixtures()
{
    const QString avatarUrl = QUrl::fromLocalFile(
        QStringLiteral(QML_TEST_SOURCE_DIR)
        + QStringLiteral("/assets/icons/hamster-agent.svg")).toString();
    return {
        QVariantMap{{QStringLiteral("id"), QStringLiteral("hamster-001")},
                    {QStringLiteral("anchorName"), QStringLiteral("寅子")},
                    {QStringLiteral("pinyinKey"), QStringLiteral("Y")},
                    {QStringLiteral("roomId"), QStringLiteral("71415")},
                    {QStringLiteral("role"), QStringLiteral("member")},
                    {QStringLiteral("rankMatched"), true},
                    {QStringLiteral("placementAverage"), 88.5},
                    {QStringLiteral("placementScoredSessions"), 3},
                    {QStringLiteral("playValue"), 6.4},
                    {QStringLiteral("status"), QStringLiteral("resolved")},
                    {QStringLiteral("avatarUrl"), avatarUrl},
                    {QStringLiteral("liveState"), QStringLiteral("online")},
                    {QStringLiteral("radarDimensions"), QVariantList{
                         QVariantMap{{QStringLiteral("name"), QStringLiteral("力量")},
                                     {QStringLiteral("average"), 18.0},
                                     {QStringLiteral("count"), 100}},
                         QVariantMap{{QStringLiteral("name"), QStringLiteral("体力")},
                                     {QStringLiteral("average"), 16.0},
                                     {QStringLiteral("count"), 100}},
                         QVariantMap{{QStringLiteral("name"), QStringLiteral("财力")},
                                     {QStringLiteral("average"), 12.0},
                                     {QStringLiteral("count"), 100}},
                     }},
                    {QStringLiteral("active"), false}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("hamster-002")},
                    {QStringLiteral("anchorName"), QStringLiteral("主播阿飞")},
                    {QStringLiteral("pinyinKey"), QStringLiteral("Z")},
                    {QStringLiteral("roomId"), QStringLiteral("84452")},
                    {QStringLiteral("role"), QStringLiteral("member")},
                    {QStringLiteral("status"), QStringLiteral("resolved")},
                    {QStringLiteral("liveState"), QStringLiteral("offline")},
                    {QStringLiteral("active"), false}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("hamster-003")},
                    {QStringLiteral("anchorName"), QStringLiteral("待确认成员")},
                    {QStringLiteral("pinyinKey"), QStringLiteral("D")},
                    {QStringLiteral("roomId"), QString()},
                    {QStringLiteral("role"), QStringLiteral("other")},
                    {QStringLiteral("status"), QStringLiteral("pending")},
                    {QStringLiteral("liveState"), QStringLiteral("unknown")},
                    {QStringLiteral("active"), false}},
    };
}

class FakeGuildController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList guildRoster READ guildRoster CONSTANT)
    Q_PROPERTY(QVariantList teams READ teams CONSTANT)
    Q_PROPERTY(QObject *maoziRank READ maoziRank CONSTANT)

public:
    FakeGuildController()
        : roster_(guildFixtures())
    {
    }

    QVariantList guildRoster() const { return roster_; }
    QVariantList teams() const { return teams_; }
    QObject *maoziRank() const { return nullptr; }

    Q_INVOKABLE QString setGuildMemberRoomId(const QString &, const QString &) { return {}; }
    Q_INVOKABLE QString addGuildMemberRoom(const QString &) { return {}; }
    Q_INVOKABLE QString createTeam(const QString &) { return {}; }
    Q_INVOKABLE QString renameTeam(const QString &, const QString &) { return {}; }
    Q_INVOKABLE QString moveTeam(const QString &, int) { return {}; }
    Q_INVOKABLE QString deleteTeam(const QString &) { return {}; }
    Q_INVOKABLE QString assignGuildMemberToTeam(const QString &, const QString &) { return {}; }
    Q_INVOKABLE QString removeGuildMemberFromTeam(const QString &, const QString &) { return {}; }

private:
    QVariantList roster_;
    QVariantList teams_;
};

class FakeMaoziRankClient final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries CONSTANT)

public:
    QVariantList entries() const
    {
        QVariantList result{
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("host-1")},
                {QStringLiteral("rank"), 1},
                {QStringLiteral("name"), QStringLiteral("尐表哥")},
                {QStringLiteral("roomId"), QStringLiteral("217331")},
                {QStringLiteral("note"), QStringLiteral("宝石海里-颜大星/死撑")},
                {QStringLiteral("teamName"), QStringLiteral("未分队")},
                {QStringLiteral("grade"), QStringLiteral("S+")},
                {QStringLiteral("gradeColor"), QStringLiteral("#ffdf7e")},
                {QStringLiteral("score"), 16.3},
                {QStringLiteral("voters"), 483},
                {QStringLiteral("live"), false},
                {QStringLiteral("posterUrl"),
                 QStringLiteral("https://6479-dy656750-d6g192t6k4a51aa36-1309340272.tcb.qcloud.la/posters/webp/6632.webp?imageMogr2/thumbnail/64x64/format/jpg")},
            },
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("host-2")},
                {QStringLiteral("rank"), 2},
                {QStringLiteral("name"), QStringLiteral("李李超欧")},
                {QStringLiteral("roomId"), QStringLiteral("12485490")},
                {QStringLiteral("note"), QStringLiteral("超巨/李主任")},
                {QStringLiteral("teamName"), QStringLiteral("未分队")},
                {QStringLiteral("grade"), QStringLiteral("S")},
                {QStringLiteral("gradeColor"), QStringLiteral("#ffc93c")},
                {QStringLiteral("score"), 15.3},
                {QStringLiteral("voters"), 341},
                {QStringLiteral("live"), true},
                {QStringLiteral("posterUrl"), QString()},
            },
        };
        for (int index = 3; index <= 15; ++index) {
            result.push_back(QVariantMap{
                {QStringLiteral("id"), QStringLiteral("host-%1").arg(index)},
                {QStringLiteral("rank"), index},
                {QStringLiteral("name"), QStringLiteral("验收主播 %1").arg(index)},
                {QStringLiteral("roomId"), QStringLiteral("1000%1").arg(index)},
                {QStringLiteral("note"), QStringLiteral("视觉验收 / 满屏列表")},
                {QStringLiteral("teamName"), QStringLiteral("未分队")},
                {QStringLiteral("grade"), index % 3 == 0 ? QStringLiteral("S-")
                                                         : QStringLiteral("A")},
                {QStringLiteral("gradeColor"), index % 3 == 0
                                                   ? QStringLiteral("#e0a51e")
                                                   : QStringLiteral("#a78bfa")},
                {QStringLiteral("score"), 14.5 - index * 0.2},
                {QStringLiteral("voters"), 300 - index * 7},
                {QStringLiteral("live"), index % 4 != 0},
                {QStringLiteral("posterUrl"), QString()},
            });
        }
        return result;
    }
};

class FakeMaoziController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(FakeMaoziRankClient *maoziRank READ maoziRank CONSTANT)

public:
    FakeMaoziRankClient *maoziRank() noexcept { return &client; }
    FakeMaoziRankClient client;
};

} // namespace

class QmlVisualRegressionTest final : public QObject {
    Q_OBJECT

private slots:
    void keepsLayoutMatrixContainedAndNonOverlapping();
    void capturesHeaderDualPrimaryStates();
    void capturesRoomControlStates();
    void capturesNavigationAndTeamStates();
    void keepsMaoziToolbarVisibleAndColumnsAligned();
};

void QmlVisualRegressionTest::keepsLayoutMatrixContainedAndNonOverlapping()
{
    registerQmlTypes();

    const struct Fixture {
        int count;
        QString layout;
        QSize size;
        QString name;
    } fixtures[] = {
        {1, QStringLiteral("auto"), QSize(1280, 720), QStringLiteral("layout-auto-1-1280x720")},
        {4, QStringLiteral("auto"), QSize(1280, 720), QStringLiteral("layout-auto-4-1280x720")},
        {9, QStringLiteral("auto"), QSize(1920, 1080), QStringLiteral("layout-auto-9-1920x1080")},
        {12, QStringLiteral("auto"), QSize(1920, 1080), QStringLiteral("layout-auto-12-1920x1080")},
        {16, QStringLiteral("auto"), QSize(1920, 1080), QStringLiteral("layout-auto-16-1920x1080")},
        {9, QStringLiteral("primary"), QSize(1920, 1080), QStringLiteral("layout-primary-9-1920x1080")},
        {12, QStringLiteral("primary"), QSize(1920, 1080), QStringLiteral("layout-primary-12-1920x1080")},
        {16, QStringLiteral("primary"), QSize(1920, 1080), QStringLiteral("layout-primary-16-1920x1080")},
        {24, QStringLiteral("auto"), QSize(1920, 1080), QStringLiteral("layout-auto-24-1920x1080")},
        {24, QStringLiteral("primary"), QSize(1920, 1080), QStringLiteral("layout-primary-24-1920x1080")},
        {4, QStringLiteral("primary-two"), QSize(1280, 720), QStringLiteral("layout-primary-two-4-1280x720")},
        {10, QStringLiteral("primary-two"), QSize(1920, 1080), QStringLiteral("layout-primary-two-10-1920x1080")},
        {16, QStringLiteral("primary-two"), QSize(1920, 1080), QStringLiteral("layout-primary-two-16-1920x1080")},
        {24, QStringLiteral("primary-two"), QSize(1920, 1080), QStringLiteral("layout-primary-two-24-1920x1080")},
    };

    for (const Fixture &fixture : fixtures) {
        std::unique_ptr<QQuickWindow> window(createHostWindow(fixture.size));
        QVERIFY(window != nullptr);
        QQmlApplicationEngine engine;
        QString error;
        const QSize contentSize(qRound(window->contentItem()->width()),
                                qRound(window->contentItem()->height()));
        std::unique_ptr<QObject> grid(createQmlObject(
            engine,
            *window,
            QStringLiteral("components/WorkspaceGrid.qml"),
            {
                {QStringLiteral("width"), contentSize.width()},
                {QStringLiteral("height"), contentSize.height()},
                {QStringLiteral("roomModel"), visualRoomFixtures(fixture.count)},
                {QStringLiteral("layoutMode"), fixture.layout},
                {QStringLiteral("primaryRoomId"), QStringLiteral("fixture-1")},
                {QStringLiteral("secondaryPrimaryRoomId"), QStringLiteral("fixture-2")},
            },
            &error));
        QVERIFY2(grid != nullptr, qPrintable(error));

        QQuickItem *surface = layoutSurface(grid.get());
        QVERIFY(surface != nullptr);
        const QList<QQuickItem *> tiles = roomTiles(surface);
        QCOMPARE(tiles.size(), fixture.count);

        const QRectF surfaceRect = visualSceneRect(surface);
        const QRectF viewport(QPointF(0, 0), QSizeF(window->size()));
        QVERIFY(viewport.contains(surfaceRect));
        for (QQuickItem *tile : tiles) {
            QVERIFY(viewport.contains(visualSceneRect(tile)));
            QVERIFY(surfaceRect.contains(visualSceneRect(tile)));
        }
        verifyNoOverlaps(tiles);
        verifyStableTileZones(tiles);

        if (fixture.layout == QStringLiteral("primary-two")) {
            QQuickItem *firstPrimary = tiles.at(0);
            QQuickItem *secondPrimary = tiles.at(1);
            QQuickItem *firstBottom = tiles.at(2);
            QVERIFY(qAbs(firstPrimary->width() - secondPrimary->width()) <= 0.5);
            QVERIFY(qAbs(firstPrimary->height() - secondPrimary->height()) <= 0.5);
            QVERIFY(firstPrimary->height() > firstBottom->height());
            QVERIFY(firstPrimary->width() * firstPrimary->height()
                    > firstBottom->width() * firstBottom->height());
        }

        const QImage image = visualCapture(window.get(), fixture.name);
        QVERIFY(!image.isNull());
        verifyBaseline(image, fixture.name);
    }
}

void QmlVisualRegressionTest::capturesHeaderDualPrimaryStates()
{
    registerQmlTypes();
    FakeHeaderController controller;

    std::unique_ptr<QQuickWindow> window(createHostWindow(QSize(960, 260)));
    QVERIFY(window != nullptr);
    QQmlApplicationEngine engine;
    QString error;
    std::unique_ptr<QObject> header(createQmlObject(
        engine,
        *window,
        QStringLiteral("components/AppHeader.qml"),
        {
            {QStringLiteral("width"), window->width()},
            {QStringLiteral("height"), 52},
            {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        },
        &error));
    QVERIFY2(header != nullptr, qPrintable(error));

    QObject *layoutButton = header->findChild<QObject *>(QStringLiteral("layoutMenuButton"));
    QObject *layoutMenu = header->findChild<QObject *>(QStringLiteral("layoutMenu"));
    QObject *dualOption = header->findChild<QObject *>(QStringLiteral("dualPrimaryLayoutOption"));
    QVERIFY(layoutButton != nullptr);
    QVERIFY(layoutMenu != nullptr);
    QVERIFY(dualOption != nullptr);

    QVERIFY(QMetaObject::invokeMethod(layoutButton, "clicked"));
    QTRY_VERIFY(layoutMenu->property("visible").toBool());
    QVERIFY(!dualOption->property("enabled").toBool());
    const QImage disabled = visualCapture(window.get(), QStringLiteral("header-dual-disabled-960x260"));
    QVERIFY(!disabled.isNull());
    verifyBaseline(disabled, QStringLiteral("header-dual-disabled-960x260"));
    QVERIFY(QMetaObject::invokeMethod(layoutMenu, "close"));
    QTRY_VERIFY(!layoutMenu->property("visible").toBool());

    controller.rooms_.setRoomCount(4);
    QVERIFY(QMetaObject::invokeMethod(layoutButton, "clicked"));
    QTRY_VERIFY(layoutMenu->property("visible").toBool());
    QVERIFY(dualOption->property("enabled").toBool());
    const QImage enabled = visualCapture(window.get(), QStringLiteral("header-dual-enabled-960x260"));
    QVERIFY(!enabled.isNull());
    verifyBaseline(enabled, QStringLiteral("header-dual-enabled-960x260"));
}

void QmlVisualRegressionTest::capturesRoomControlStates()
{
    registerQmlTypes();
    std::unique_ptr<QQuickWindow> window(createHostWindow(QSize(640, 420)));
    QVERIFY(window != nullptr);

    QQmlApplicationEngine engine;
    QString error;
    QVariantMap properties = visualRoomFixtures(1, true, true).constFirst().toMap();
    properties.insert(QStringLiteral("width"), 640);
    properties.insert(QStringLiteral("height"), 360);
    std::unique_ptr<QObject> tile(createQmlObject(
        engine,
        *window,
        QStringLiteral("components/RoomTile.qml"),
        properties,
        &error));
    QVERIFY2(tile != nullptr, qPrintable(error));

    QObject *quality = tile->findChild<QObject *>(QStringLiteral("roomQualitySelector"));
    QObject *qualityPopup = quality->property("popup").value<QObject *>();
    QObject *topActions = tile->findChild<QObject *>(QStringLiteral("roomTopActions"));
    QVERIFY(quality != nullptr);
    QVERIFY(qualityPopup != nullptr);
    QVERIFY(topActions != nullptr);

    tile->setProperty("controlsVisible", true);
    QVERIFY(QMetaObject::invokeMethod(qualityPopup, "open"));
    QTRY_VERIFY(qualityPopup->property("visible").toBool());
    const QImage qualityImage = visualCapture(window.get(), QStringLiteral("room-quality-popup-640x420"));
    QVERIFY(!qualityImage.isNull());
    verifyBaseline(qualityImage, QStringLiteral("room-quality-popup-640x420"));
    QVERIFY(QMetaObject::invokeMethod(qualityPopup, "close"));
    QTRY_VERIFY(!qualityPopup->property("visible").toBool());

    QVERIFY(QMetaObject::invokeMethod(topActions, "clicked"));
    QTRY_VERIFY(tile->property("menuOpen").toBool());
    const QImage menuImage = visualCapture(window.get(), QStringLiteral("room-menu-640x420"));
    QVERIFY(!menuImage.isNull());
    verifyBaseline(menuImage, QStringLiteral("room-menu-640x420"));
}

void QmlVisualRegressionTest::capturesNavigationAndTeamStates()
{
    registerQmlTypes();
    FakeGuildController controller;

    std::unique_ptr<QQuickWindow> navigationWindow(createHostWindow(QSize(284, 720)));
    QVERIFY(navigationWindow != nullptr);
    QQmlApplicationEngine navigationEngine;
    QString error;
    std::unique_ptr<QObject> navigation(createQmlObject(
        navigationEngine,
        *navigationWindow,
        QStringLiteral("components/GuildNavigationPanel.qml"),
        {
            {QStringLiteral("width"), 284},
            {QStringLiteral("height"), 720},
            {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        },
        &error));
    QVERIFY2(navigation != nullptr, qPrintable(error));
    QTRY_COMPARE(navigation->property("visibleMemberCount").toInt(), 3);

    const QImage navigationImage =
        visualCapture(navigationWindow.get(), QStringLiteral("guild-navigation-284x720"));
    QVERIFY(!navigationImage.isNull());
    verifyBaseline(navigationImage, QStringLiteral("guild-navigation-284x720"));

    std::unique_ptr<QQuickWindow> dialogWindow(createHostWindow(QSize(720, 760)));
    QVERIFY(dialogWindow != nullptr);
    QQmlApplicationEngine dialogEngine;
    std::unique_ptr<QObject> dialog(createQmlObject(
        dialogEngine,
        *dialogWindow,
        QStringLiteral("dialogs/TeamManagerDialog.qml"),
        {
            {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        },
        &error));
    QVERIFY2(dialog != nullptr, qPrintable(error));
    QVERIFY(QMetaObject::invokeMethod(dialog.get(), "open"));
    QTRY_VERIFY(dialog->property("visible").toBool());

    const QImage dialogImage =
        visualCapture(dialogWindow.get(), QStringLiteral("team-manager-720x760"));
    QVERIFY(!dialogImage.isNull());
    verifyBaseline(dialogImage, QStringLiteral("team-manager-720x760"));
}

void QmlVisualRegressionTest::keepsMaoziToolbarVisibleAndColumnsAligned()
{
    registerQmlTypes();
    FakeMaoziController controller;
    std::unique_ptr<QQuickWindow> window(createHostWindow(QSize(1920, 1080)));
    QVERIFY(window != nullptr);

    QQmlApplicationEngine engine;
    QString error;
    std::unique_ptr<QObject> page(createQmlObject(
        engine,
        *window,
        QStringLiteral("pages/MaoziRankPage.qml"),
        {
            {QStringLiteral("width"), window->width()},
            {QStringLiteral("height"), window->height()},
            {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        },
        &error));
    QVERIFY2(page != nullptr, qPrintable(error));

    const QRect viewport(QPoint(0, 0), window->size());
    const QStringList controls{
        QStringLiteral("maoziSearchField"),
        QStringLiteral("maoziLiveFilter"),
        QStringLiteral("maoziRefreshButton"),
        QStringLiteral("maoziTitle"),
        QStringLiteral("maoziSubtitle"),
    };
    for (const QString &name : controls) {
        QQuickItem *control = visualItemByObjectName(window->contentItem(), name);
        QVERIFY2(control != nullptr, qPrintable(QStringLiteral("Missing control: %1").arg(name)));
        const QRectF rect = visualSceneRect(control);
        QVERIFY2(viewport.contains(rect.toRect()),
                 qPrintable(QStringLiteral("%1 is outside viewport: %2,%3 %4x%5")
                                .arg(name)
                                .arg(rect.x())
                                .arg(rect.y())
                                .arg(rect.width())
                                .arg(rect.height())));
        QVERIFY(rect.width() > 0 && rect.height() > 0);
    }

    QQuickItem *toolbar = visualItemByObjectName(window->contentItem(), QStringLiteral("maoziToolbar"));
    QQuickItem *title = visualItemByObjectName(window->contentItem(), QStringLiteral("maoziTitle"));
    QQuickItem *subtitle = visualItemByObjectName(window->contentItem(), QStringLiteral("maoziSubtitle"));
    QVERIFY(toolbar != nullptr);
    QVERIFY(title != nullptr);
    QVERIFY(subtitle != nullptr);
    QVERIFY(visualSceneRect(toolbar).contains(visualSceneRect(title)));
    QVERIFY(visualSceneRect(toolbar).contains(visualSceneRect(subtitle)));
    QVERIFY(title->height() > 0);
    QVERIFY(subtitle->height() > 0);
    QVERIFY(!visualRectsOverlap(visualSceneRect(title), visualSceneRect(subtitle)));

    QQuickItem *avatar = visualItemByObjectName(window->contentItem(), QStringLiteral("maoziAvatarImage"));
    QVERIFY(avatar != nullptr);
    QVERIFY(avatar->property("source").toUrl().isValid());
    QTRY_VERIFY_WITH_TIMEOUT(avatar->property("status").toInt() == 1, 3000);
    QVERIFY(avatar->property("visible").toBool());

    struct ColumnFixture {
        QString headerName;
        QString dataName;
    };
    const QList<ColumnFixture> columns{
        {QStringLiteral("maoziHeaderRank"), QStringLiteral("maoziCellRank")},
        {QStringLiteral("maoziHeaderHost"), QStringLiteral("maoziCellHost")},
        {QStringLiteral("maoziHeaderRoom"), QStringLiteral("maoziCellRoom")},
        {QStringLiteral("maoziHeaderTeam"), QStringLiteral("maoziCellTeam")},
        {QStringLiteral("maoziHeaderGrade"), QStringLiteral("maoziCellGrade")},
        {QStringLiteral("maoziHeaderScore"), QStringLiteral("maoziCellScore")},
        {QStringLiteral("maoziHeaderVoters"), QStringLiteral("maoziCellVoters")},
        {QStringLiteral("maoziHeaderStatus"), QStringLiteral("maoziCellStatus")},
    };
    for (const ColumnFixture &column : columns) {
        QQuickItem *header = visualItemByObjectName(window->contentItem(), column.headerName);
        QQuickItem *cell = visualItemByObjectName(window->contentItem(), column.dataName);
        QVERIFY2(header != nullptr, qPrintable(QStringLiteral("Missing header: %1").arg(column.headerName)));
        QVERIFY2(cell != nullptr, qPrintable(QStringLiteral("Missing cell: %1").arg(column.dataName)));
        QCOMPARE(qRound(cell->width()), qRound(header->width()));
        const qreal headerCenter = visualSceneRect(header).center().x();
        const qreal cellCenter = visualSceneRect(cell).center().x();
        QVERIFY2(qAbs(headerCenter - cellCenter) <= 1.0,
                 qPrintable(QStringLiteral("%1 center %2 != %3 center %4")
                                .arg(column.headerName)
                                .arg(headerCenter)
                                .arg(column.dataName)
                                .arg(cellCenter)));
    }

    const QImage image = visualCapture(window.get(), QStringLiteral("maozi-rank-1920x1080"));
    QVERIFY(!image.isNull());
    verifyBaseline(image, QStringLiteral("maozi-rank-1920x1080"));
}

QTEST_MAIN(QmlVisualRegressionTest)

#include "qml_visual_regression_test.moc"





