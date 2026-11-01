#include <QColor>
#include <QDir>
#include <QFont>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QSize>
#include <QVariantList>
#include <QVariantMap>
#include <QtTest/QtTest>

#include <memory>

#include "ui/mpv_quick_item.h"
#include "visual_test_support.h"
#include "test_font_support.h"

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
    window->setFlags(Qt::Window | Qt::FramelessWindowHint);
    window->setColor(QColor(QStringLiteral("#0e1312")));
    window->resize(size);
    window->show();
    if (!QTest::qWaitForWindowExposed(window)) {
        delete window;
        return nullptr;
    }
    return window;
}

QQuickItem *visualItemByObjectNameAndText(QQuickItem *root,
                                          const QString &objectName,
                                          const QString &text)
{
    if (root == nullptr) return nullptr;
    if (root->objectName() == objectName
        && root->property("text").toString() == text) {
        return root;
    }
    for (QQuickItem *child : root->childItems()) {
        if (QQuickItem *match = visualItemByObjectNameAndText(child, objectName, text)) {
            return match;
        }
    }
    return nullptr;
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

void verifyNoOverlapByName(QQuickItem *root,
                           const QString &firstName,
                           const QString &secondName)
{
    QQuickItem *first = visualItemByObjectName(root, firstName);
    QQuickItem *second = visualItemByObjectName(root, secondName);
    QVERIFY2(first != nullptr, qPrintable(QStringLiteral("Missing item: %1").arg(firstName)));
    QVERIFY2(second != nullptr, qPrintable(QStringLiteral("Missing item: %1").arg(secondName)));
    QVERIFY2(!visualRectsOverlap(visualSceneRect(first), visualSceneRect(second)),
             qPrintable(QStringLiteral("%1 overlaps %2").arg(firstName).arg(secondName)));
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
                    {QStringLiteral("score"), 15.36},
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
    Q_PROPERTY(QObject *rooms READ emptyObject CONSTANT)
    Q_PROPERTY(QObject *workspace READ emptyObject CONSTANT)
    Q_PROPERTY(QObject *monitoring READ emptyObject CONSTANT)
    Q_PROPERTY(QObject *danmaku READ emptyObject CONSTANT)
    Q_PROPERTY(QVariantList libraryRooms READ emptyList CONSTANT)

public:
    FakeGuildController()
        : roster_(guildFixtures())
    {
    }

    QVariantList guildRoster() const { return roster_; }
    QVariantList teams() const { return teams_; }
    QObject *maoziRank() const { return nullptr; }
    QObject *emptyObject() const { return nullptr; }
    QVariantList emptyList() const { return {}; }

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
    Q_PROPERTY(QVariantList placementEntries READ placementEntries CONSTANT)
    Q_PROPERTY(QVariantMap placementColumns READ placementColumns CONSTANT)
    Q_PROPERTY(QVariantList playValueEntries READ playValueEntries CONSTANT)

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
                // Deterministic image fixture; public avatar decoding is checked separately.
                {QStringLiteral("posterUrl"), qmlSource(QStringLiteral("assets/brand/icon-app.png")).toString()},
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

    QVariantList placementEntries() const
    {
        return {
            QVariantMap{
                {QStringLiteral("id"), QStringLiteral("host-1")},
                {QStringLiteral("name"), QStringLiteral("尐表哥")},
                {QStringLiteral("roomId"), QStringLiteral("217331")},
                {QStringLiteral("posterUrl"), QString()},
                {QStringLiteral("placementRank"), 1},
                {QStringLiteral("placementAverage"), 87.0},
                {QStringLiteral("placementScoredSessions"), 6},
                {QStringLiteral("placementSessions"),
                 QVariantMap{{QStringLiteral("2026-09-28:1"), 93.0},
                             {QStringLiteral("2026-09-28:2"), 85.4},
                             {QStringLiteral("2026-09-29:1"), 92.3},
                             {QStringLiteral("2026-09-29:2"), 90.6},
                             {QStringLiteral("2026-09-30:1"), 88.0},
                             {QStringLiteral("2026-09-30:2"), 72.6}}},
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
            {QStringLiteral("2026-09-29:2"),
             QVariantMap{{QStringLiteral("key"), QStringLiteral("2026-09-29:2")},
                         {QStringLiteral("label"), QStringLiteral("29晚")}}},
            {QStringLiteral("2026-09-30:1"),
             QVariantMap{{QStringLiteral("key"), QStringLiteral("2026-09-30:1")},
                         {QStringLiteral("label"), QStringLiteral("30午")}}},
            {QStringLiteral("2026-09-30:2"),
             QVariantMap{{QStringLiteral("key"), QStringLiteral("2026-09-30:2")},
                         {QStringLiteral("label"), QStringLiteral("30晚")}}},
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
};

class FakeMaoziController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(FakeMaoziRankClient *maoziRank READ maoziRank CONSTANT)

public:
    FakeMaoziRankClient *maoziRank() noexcept { return &client; }
    Q_INVOKABLE bool openExternalUrl(const QString &) { return true; }
    FakeMaoziRankClient client;
};

} // namespace

class QmlVisualRegressionTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void keepsLayoutMatrixContainedAndNonOverlapping();
    void capturesHeaderDualPrimaryStates();
    void capturesRoomControlStates();
    void capturesNavigationAndTeamStates();
    void keepsMaoziToolbarVisibleAndColumnsAligned();
};

void QmlVisualRegressionTest::initTestCase()
{
    QVERIFY2(DouyuTestSupport::prepareTestFonts(),
             "No Simplified Chinese font is available for QML tests (check QT_QPA_FONTDIR)");
}

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

    QList<QImage> images;
    QStringList imageNames;
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
        images.push_back(image);
        imageNames.push_back(fixture.name);
    }

    for (qsizetype index = 0; index < images.size(); ++index) {
        verifyBaseline(images.at(index), imageNames.at(index));
    }
}

void QmlVisualRegressionTest::capturesHeaderDualPrimaryStates()
{
    registerQmlTypes();
    FakeHeaderController controller;

    std::unique_ptr<QQuickWindow> window(createHostWindow(QSize(960, 600)));
    QVERIFY(window != nullptr);
    QCOMPARE(window->color(), QColor(QStringLiteral("#0e1312")));
    QQmlApplicationEngine engine;
    QString error;
    std::unique_ptr<QObject> header(createQmlObject(
        engine,
        *window,
        QStringLiteral("components/WatchToolsPanel.qml"),
        {
            {QStringLiteral("width"), 360},
            {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        },
        &error));
    QVERIFY2(header != nullptr, qPrintable(error));
    QVERIFY(QMetaObject::invokeMethod(header.get(), "open"));
    QTRY_VERIFY(header->property("visible").toBool());

    QObject *layoutButton = header->findChild<QObject *>(QStringLiteral("layoutMenuButton"));
    QObject *layoutMenu = header->findChild<QObject *>(QStringLiteral("layoutMenu"));
    QObject *dualOption = header->findChild<QObject *>(QStringLiteral("dualPrimaryLayoutOption"));
    QVERIFY(layoutButton != nullptr);
    QVERIFY(layoutMenu != nullptr);
    QVERIFY(dualOption != nullptr);

    QVERIFY(QMetaObject::invokeMethod(layoutButton, "clicked"));
    QTRY_VERIFY(layoutMenu->property("visible").toBool());
    QCOMPARE(header->property("currentSection").toString(), QStringLiteral("layout"));
    auto *layoutSection = qobject_cast<QQuickItem *>(layoutMenu);
    QVERIFY(layoutSection != nullptr);
    QTRY_VERIFY(QRectF(QPointF(), window->size()).contains(
        QRectF(layoutSection->mapToScene(QPointF()), layoutSection->size())));
    QTest::qWait(100);
    QVERIFY(!header->findChild<QObject *>(QStringLiteral("watchToolsOverview"))
                 ->property("visible").toBool());
    QVERIFY(!dualOption->property("enabled").toBool());
    const QImage disabled = visualCapture(window.get(), QStringLiteral("watch-tools-dual-disabled-960x600"));
    QVERIFY(!disabled.isNull());
    QVERIFY(QMetaObject::invokeMethod(layoutMenu, "close"));
    QTRY_VERIFY(!layoutMenu->property("visible").toBool());
    QCOMPARE(header->property("currentSection").toString(), QStringLiteral("overview"));

    controller.rooms_.setRoomCount(4);
    QVERIFY(QMetaObject::invokeMethod(layoutButton, "clicked"));
    QTRY_VERIFY(layoutMenu->property("visible").toBool());
    QVERIFY(dualOption->property("enabled").toBool());
    QTest::qWait(100);
    const QImage enabled = visualCapture(window.get(), QStringLiteral("watch-tools-dual-enabled-960x600"));
    QVERIFY(!enabled.isNull());
    verifyBaseline(disabled, QStringLiteral("watch-tools-dual-disabled-960x600"));
    verifyBaseline(enabled, QStringLiteral("watch-tools-dual-enabled-960x600"));
}

void QmlVisualRegressionTest::capturesRoomControlStates()
{
    registerQmlTypes();
    std::unique_ptr<QQuickWindow> window(createHostWindow(QSize(640, 420)));
    QVERIFY(window != nullptr);
    QCOMPARE(window->color(), QColor(QStringLiteral("#0e1312")));

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
    QTRY_COMPARE(qualityPopup->property("opacity").toDouble(), 1.0);
    const QImage qualityImage = visualCapture(window.get(), QStringLiteral("room-quality-popup-640x420"));
    QVERIFY(!qualityImage.isNull());
    QVERIFY(QMetaObject::invokeMethod(qualityPopup, "close"));
    QTRY_VERIFY(!qualityPopup->property("visible").toBool());

    QVERIFY(QMetaObject::invokeMethod(topActions, "clicked"));
    QTRY_VERIFY(tile->property("menuOpen").toBool());
    const QImage menuImage = visualCapture(window.get(), QStringLiteral("room-menu-640x420"));
    QVERIFY(!menuImage.isNull());
    verifyBaseline(qualityImage, QStringLiteral("room-quality-popup-640x420"));
    verifyBaseline(menuImage, QStringLiteral("room-menu-640x420"));
}

void QmlVisualRegressionTest::capturesNavigationAndTeamStates()
{
    registerQmlTypes();
    FakeGuildController controller;

    QQmlApplicationEngine navigationEngine;
    navigationEngine.load(qmlSource(QStringLiteral("Main.qml")));
    QVERIFY(!navigationEngine.rootObjects().isEmpty());
    auto *navigationWindow = qobject_cast<QQuickWindow *>(navigationEngine.rootObjects().constFirst());
    QVERIFY(navigationWindow != nullptr);
    QVERIFY(navigationWindow->setProperty("reducedMotion", true));
    navigationWindow->resize(QSize(1280, 800));
    navigationWindow->setProperty("appController", QVariant::fromValue(static_cast<QObject *>(&controller)));
    QVERIFY(QTest::qWaitForWindowExposed(navigationWindow));
    auto *navigationButton = navigationWindow->findChild<QObject *>(QStringLiteral("hamsterNavigationButton"));
    QVERIFY(navigationButton != nullptr);
    QVERIFY(QMetaObject::invokeMethod(navigationButton, "clicked"));
    auto *navigation = navigationWindow->findChild<QQuickItem *>(QStringLiteral("guildNavigationPanel"));
    QVERIFY(navigation != nullptr);
    QTRY_VERIFY(navigation->isVisible());
    QString error;
    QTRY_COMPARE(navigation->size(), QSizeF(1200, 764));
    QTRY_COMPARE(navigation->property("visibleMemberCount").toInt(), 3);
    const QRectF navigationViewport(QPointF(), QSizeF(navigationWindow->size()));
    for (const char *name : {"guildNavigationTitle", "guildRefreshStatusButton", "guildAddRoomButton",
                             "guildNavigationSearch", "guildStatusFilter", "guildGenderFilter_all",
                             "guildGenderFilter_m", "guildGenderFilter_f"}) {
        QQuickItem *control = nullptr;
        QTRY_VERIFY2((control = visualItemByObjectName(navigation, QString::fromLatin1(name))) != nullptr, name);
        QTRY_VERIFY(navigationViewport.contains(visualSceneRect(control)));
    }
    QQuickItem *rankedMemberName = nullptr;
    QTRY_VERIFY((rankedMemberName = visualItemByObjectNameAndText(
                     navigation, QStringLiteral("guildMemberName"), QStringLiteral("寅子"))) != nullptr);
    QQuickItem *rankedMemberRow = rankedMemberName;
    while (rankedMemberRow != nullptr
           && rankedMemberRow->objectName() != QStringLiteral("guildMemberRow")) {
        rankedMemberRow = rankedMemberRow->parentItem();
    }
    QVERIFY(rankedMemberRow != nullptr);
    auto *avatarImage = visualItemByObjectName(rankedMemberRow, QStringLiteral("guildMemberAvatarImage"));
    QVERIFY(avatarImage != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(avatarImage->property("status").toInt(), 1, 3000);
    const QImage navigationImage =
        visualCapture(navigationWindow, QStringLiteral("guild-navigation-shell-1280x800"));
    QVERIFY(!navigationImage.isNull());

    const QPoint hoverPoint = rankedMemberName->mapToScene(
        QPointF(rankedMemberName->width() / 2, rankedMemberName->height() / 2)).toPoint();
    QTest::mouseMove(navigationWindow, hoverPoint, 50);
    QTRY_VERIFY(rankedMemberRow->property("hovered").toBool());
    QTest::qWait(650);
    QVERIFY(!navigationWindow->findChild<QObject *>("guildRankHoverCard"));
    QVERIFY(rankedMemberRow->property("hovered").toBool());
    const QImage hoverImage = visualCapture(navigationWindow, QStringLiteral("guild-member-hover-shell-1280x800"));
    QVERIFY(!hoverImage.isNull());

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
    QTest::qWait(100);
    const auto *content = qvariant_cast<QQuickItem *>(dialog->property("contentItem"));
    QVERIFY(content != nullptr);
    const QRectF dialogRect(dialog->property("x").toDouble(), dialog->property("y").toDouble(),
                            dialog->property("width").toDouble(), dialog->property("height").toDouble());
    for (const auto &name : {"moveTeamUpButton", "moveTeamDownButton", "deleteTeamButton"}) {
        auto *button = dialog->findChild<QQuickItem *>(QString::fromLatin1(name));
        QVERIFY(button != nullptr);
        QVERIFY2(dialogRect.contains(visualSceneRect(button)), name);
    }

    const QImage dialogImage =
        visualCapture(dialogWindow.get(), QStringLiteral("team-manager-720x760"));
    QVERIFY(!dialogImage.isNull());
    verifyBaseline(navigationImage, QStringLiteral("guild-navigation-shell-1280x800"));
    verifyBaseline(hoverImage, QStringLiteral("guild-member-hover-shell-1280x800"));
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
        QStringLiteral("maoziOpenSiteButton"),
        QStringLiteral("maoziRefreshButton"),
        QStringLiteral("maoziRankingTab"),
        QStringLiteral("maoziPlacementTab"),
        QStringLiteral("maoziRegularSeasonTab"),
        QStringLiteral("maoziFinalsTab"),
        QStringLiteral("maoziFootballTab"),
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
    QQuickItem *backButton = visualItemByObjectName(window->contentItem(), QStringLiteral("maoziBackButton"));
    QQuickItem *title = visualItemByObjectName(window->contentItem(), QStringLiteral("maoziTitle"));
    QQuickItem *subtitle = visualItemByObjectName(window->contentItem(), QStringLiteral("maoziSubtitle"));
    QVERIFY(toolbar != nullptr);
    QVERIFY(backButton != nullptr);
    QVERIFY(title != nullptr);
    QVERIFY(subtitle != nullptr);
    QCOMPARE(title->property("font").value<QFont>().pixelSize(), 24);
    QVERIFY(visualSceneRect(toolbar).contains(visualSceneRect(title)));
    QVERIFY(visualSceneRect(toolbar).contains(visualSceneRect(subtitle)));
    QVERIFY(title->height() > 0);
    QVERIFY(subtitle->height() > 0);
    QVERIFY(!visualRectsOverlap(visualSceneRect(title), visualSceneRect(subtitle)));
    QVERIFY(visualSceneRect(title).left() >= visualSceneRect(backButton).right() + 8.0);
    verifyNoOverlapByName(window->contentItem(),
                          QStringLiteral("maoziBackButton"),
                          QStringLiteral("maoziTitle"));
    verifyNoOverlapByName(window->contentItem(),
                          QStringLiteral("maoziBackButton"),
                          QStringLiteral("maoziSubtitle"));

    QQuickItem *rankHeader = visualItemByObjectName(window->contentItem(), QStringLiteral("maoziHeaderRank"));
    QQuickItem *rankList = visualItemByObjectName(window->contentItem(), QStringLiteral("maoziRankList"));
    QQuickItem *headerBar =
        visualItemByObjectName(window->contentItem(), QStringLiteral("maoziRankingHeaderBar"));
    QVERIFY(rankHeader != nullptr);
    QVERIFY(rankList != nullptr);
    QVERIFY(headerBar != nullptr);
    QTRY_VERIFY(rankList->isVisible() && rankList->width() > 0 && rankList->height() > 0);
    QTRY_VERIFY(headerBar->isVisible() && headerBar->width() > 0 && headerBar->height() > 0);
    QTRY_VERIFY(!visualRectsOverlap(visualSceneRect(headerBar),
                                    visualSceneRect(rankList)));

    QVERIFY(headerBar != nullptr);
    QCOMPARE(qRound(headerBar->height()), 36);
    const QRectF tableRect = visualSceneRect(headerBar->parentItem());
    const QRectF headerRect = visualSceneRect(headerBar);
    const QRectF rankListRect = visualSceneRect(rankList);
    QVERIFY(qAbs(headerRect.top() - tableRect.top()) <= 1.0);
    QVERIFY(qAbs(rankListRect.top() - headerRect.bottom()) <= 1.0);
    QQuickItem *widthProbe =
        visualItemByObjectName(window->contentItem(), QStringLiteral("maoziListWidthProbe"));
    QVERIFY(widthProbe != nullptr);
    QVERIFY(widthProbe->property("tableWidth").toDouble()
            <= widthProbe->property("contentWidth").toDouble());

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

    QQuickItem *statusCell =
        visualItemByObjectName(window->contentItem(), QStringLiteral("maoziCellStatus"));
    QQuickItem *statusDot =
        visualItemByObjectName(window->contentItem(), QStringLiteral("maoziLiveStatusDot"));
    QVERIFY(statusCell != nullptr);
    QVERIFY(statusDot != nullptr);
    QQuickItem *rankRow = statusCell->parentItem() ? statusCell->parentItem()->parentItem() : nullptr;
    QVERIFY(rankRow != nullptr);
    QCOMPARE(QQmlProperty::read(rankRow, QStringLiteral("border.width")).toInt(), 0);
    QVERIFY(statusDot->width() > 0);
    QVERIFY(statusDot->height() > 0);

    // Earlier visual cases move the desktop pointer over a row; reset it so a
    // stale hover state cannot contaminate this page's pixel baseline.
    QTest::mouseMove(window.get(), QPoint(window->width() - 1, window->height() - 1));
    const QImage image = visualCapture(window.get(), QStringLiteral("maozi-rank-1920x1080"));
    QVERIFY(!image.isNull());

    QObject *placementTab =
        page->findChild<QObject *>(QStringLiteral("maoziPlacementTab"));
    QObject *placementList =
        page->findChild<QObject *>(QStringLiteral("maoziPlacementList"));
    QVERIFY(QMetaObject::invokeMethod(placementTab, "clicked"));
    QTRY_VERIFY(placementList->property("visible").toBool());
    QTRY_COMPARE(placementList->property("count").toInt(), 1);
    const QImage placementImage =
        visualCapture(window.get(), QStringLiteral("maozi-placement-1920x1080"));
    QVERIFY(!placementImage.isNull());

    for (const auto &[buttonName, tabName] : {
             qMakePair("maoziRegularSeasonTab", "regularSeason"),
             qMakePair("maoziFinalsTab", "finals"),
             qMakePair("maoziFootballTab", "football")}) {
        QObject *tab = page->findChild<QObject *>(QString::fromLatin1(buttonName));
        QVERIFY(tab != nullptr);
        QVERIFY(QMetaObject::invokeMethod(tab, "clicked"));
        QTRY_COMPARE(page->property("activeTab").toString(), QString::fromLatin1(tabName));
    }
    QVERIFY(page->findChild<QObject *>(QStringLiteral("maoziPlayValueTab")) == nullptr);
    verifyBaseline(image, QStringLiteral("maozi-rank-1920x1080"));
    verifyBaseline(placementImage, QStringLiteral("maozi-placement-1920x1080"));
}

QTEST_MAIN(QmlVisualRegressionTest)

#include "qml_visual_regression_test.moc"

