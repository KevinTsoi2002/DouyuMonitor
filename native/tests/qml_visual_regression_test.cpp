#include <QColor>
#include <QDir>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSize>
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
    return {
        QVariantMap{{QStringLiteral("id"), QStringLiteral("hamster-001")},
                    {QStringLiteral("anchorName"), QStringLiteral("寅子")},
                    {QStringLiteral("roomId"), QStringLiteral("71415")},
                    {QStringLiteral("status"), QStringLiteral("resolved")},
                    {QStringLiteral("active"), false}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("hamster-002")},
                    {QStringLiteral("anchorName"), QStringLiteral("主播阿飞")},
                    {QStringLiteral("roomId"), QStringLiteral("84452")},
                    {QStringLiteral("status"), QStringLiteral("resolved")},
                    {QStringLiteral("active"), false}},
        QVariantMap{{QStringLiteral("id"), QStringLiteral("hamster-003")},
                    {QStringLiteral("anchorName"), QStringLiteral("待确认成员")},
                    {QStringLiteral("roomId"), QString()},
                    {QStringLiteral("status"), QStringLiteral("pending")},
                    {QStringLiteral("active"), false}},
    };
}

class FakeGuildController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList guildRoster READ guildRoster CONSTANT)
    Q_PROPERTY(QVariantList teams READ teams CONSTANT)

public:
    FakeGuildController()
        : roster_(guildFixtures())
    {
    }

    QVariantList guildRoster() const { return roster_; }
    QVariantList teams() const { return teams_; }

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

} // namespace

class QmlVisualRegressionTest final : public QObject {
    Q_OBJECT

private slots:
    void keepsLayoutMatrixContainedAndNonOverlapping();
    void capturesHeaderDualPrimaryStates();
    void capturesRoomControlStates();
    void capturesNavigationAndTeamStates();
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
        {4, QStringLiteral("auto"), QSize(1280, 720), QStringLiteral("layout-auto-4-1280x720")},
        {9, QStringLiteral("auto"), QSize(1920, 1080), QStringLiteral("layout-auto-9-1920x1080")},
        {9, QStringLiteral("primary"), QSize(1920, 1080), QStringLiteral("layout-primary-9-1920x1080")},
        {4, QStringLiteral("primary-two"), QSize(1280, 720), QStringLiteral("layout-primary-two-4-1280x720")},
        {10, QStringLiteral("primary-two"), QSize(1920, 1080), QStringLiteral("layout-primary-two-10-1920x1080")},
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

QTEST_MAIN(QmlVisualRegressionTest)

#include "qml_visual_regression_test.moc"





