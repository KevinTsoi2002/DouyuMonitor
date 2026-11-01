#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QtTest/QtTest>

#include <memory>

#include "test_font_support.h"

class FakeDanmakuQmlController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap displaySettings READ displaySettings NOTIFY settingsChanged)

public:
    QVariantMap displaySettings() const
    {
        return settings_;
    }

    void enqueue(QVariantMap message)
    {
        messages_.append(std::move(message));
        emit messageAvailable(messages_.last().value(QStringLiteral("roomId")).toString());
    }

    int clearCount() const noexcept
    {
        return clearCount_;
    }

    QString lastClearedRoomId() const
    {
        return lastClearedRoomId_;
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

    Q_INVOKABLE void clearRoom(const QString &roomId)
    {
        ++clearCount_;
        lastClearedRoomId_ = roomId;
        messages_.clear();
    }

signals:
    void messageAvailable(const QString &roomId);
    void settingsChanged();

private:
    QVariantMap settings_{{QStringLiteral("durationSeconds"), 5},
                          {QStringLiteral("fontSize"), 20},
                          {QStringLiteral("opacity"), 0.9},
                          {QStringLiteral("region"), QStringLiteral("top")},
                          {QStringLiteral("density"), QStringLiteral("massive")},
                          {QStringLiteral("fontFamily"), QStringLiteral("microsoft-yahei")},
                          {QStringLiteral("rendering"), QStringLiteral("native")}};
    QList<QVariantMap> messages_;
    int clearCount_ = 0;
    QString lastClearedRoomId_;
};

namespace {

QVariantMap message(const QString &id, const QString &text)
{
    return {{QStringLiteral("id"), id},
            {QStringLiteral("roomId"), QStringLiteral("63136")},
            {QStringLiteral("nickname"), QStringLiteral("tester")},
            {QStringLiteral("text"), text}};
}

QQuickItem *createOverlay(QQmlEngine &engine,
                          QQuickWindow &window,
                          FakeDanmakuQmlController &controller)
{
    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/qml/components/DanmakuOverlay.qml")));
    if (!component.isReady()) return nullptr;
    QObject *created = component.createWithInitialProperties({
        {QStringLiteral("roomId"), QStringLiteral("63136")},
        {QStringLiteral("enabled"), true},
        {QStringLiteral("controller"), QVariant::fromValue(static_cast<QObject *>(&controller))},
        {QStringLiteral("width"), 320},
        {QStringLiteral("height"), 180},
    });
    auto *overlay = qobject_cast<QQuickItem *>(created);
    if (overlay == nullptr) {
        delete created;
        return nullptr;
    }
    overlay->setParentItem(window.contentItem());
    return overlay;
}

QList<QObject *> activeLines(QQuickItem *overlay)
{
    return overlay->findChildren<QObject *>(QStringLiteral("danmakuLine"));
}

bool rectanglesOverlap(QObject *left, QObject *right)
{
    const qreal leftX = left->property("x").toReal();
    const qreal rightX = right->property("x").toReal();
    const qreal leftY = left->property("y").toReal();
    const qreal rightY = right->property("y").toReal();
    const qreal leftRight = leftX + left->property("width").toReal();
    const qreal rightRight = rightX + right->property("width").toReal();
    const qreal leftBottom = leftY + left->property("height").toReal();
    const qreal rightBottom = rightY + right->property("height").toReal();
    return leftX < rightRight && rightX < leftRight && leftY < rightBottom
        && rightY < leftBottom;
}

} // namespace

class QmlDanmakuOverlayTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void launchesAQueuedMessageIntoTheConfiguredRegion();
    void rendersMessageTextAsPlainText();
    void doesNotReuseAnUnsafeLane();
    void keepsLaneSpacingSafeForOutlinedText();
    void relayoutsActiveDanmakuWhenContainerHeightChanges();
    void shrinksLanesWithoutOverlappingActiveLines();
    void clearsActiveAndQueuedMessagesWhenDisabled();
    void pausesHiddenOverlayWithoutConsumingQueuedMessages();
    void keepsLaunchingDuringContinuousMessageArrival();
};

void QmlDanmakuOverlayTest::initTestCase()
{
    QVERIFY2(DouyuTestSupport::prepareTestFonts(),
             "No Simplified Chinese font is available for QML tests (check QT_QPA_FONTDIR)");
}

void QmlDanmakuOverlayTest::pausesHiddenOverlayWithoutConsumingQueuedMessages()
{
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(320, 180);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    FakeDanmakuQmlController controller;
    QQuickItem page(window.contentItem());
    std::unique_ptr<QQuickItem> overlay(createOverlay(engine, window, controller));
    QVERIFY(overlay);
    overlay->setParentItem(&page);
    page.setVisible(false);
    QVERIFY(!overlay->isVisible());
    controller.enqueue(message("hidden", "queued while hidden"));
    QTest::qWait(250);
    QCOMPARE(activeLines(overlay.get()).size(), 0);
    QCOMPARE(controller.clearCount(), 0);
    page.setVisible(true);
    QVERIFY(overlay->isVisible());
    QCOMPARE(overlay->property("presentationSuspended").toBool(), false);
    QTRY_COMPARE_WITH_TIMEOUT(activeLines(overlay.get()).size(), 1, 1000);
}

void QmlDanmakuOverlayTest::keepsLaunchingDuringContinuousMessageArrival()
{
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(320, 180);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));
    FakeDanmakuQmlController controller;
    std::unique_ptr<QQuickItem> overlay(createOverlay(engine, window, controller));
    QVERIFY(overlay);
    QTimer arrivals;
    int nextId = 0;
    connect(&arrivals, &QTimer::timeout, &controller, [&] {
        controller.enqueue(message(QString::number(++nextId), "continuous"));
    });
    arrivals.start(20);
    QTRY_VERIFY_WITH_TIMEOUT(!activeLines(overlay.get()).isEmpty(), 400);
    QVERIFY(arrivals.isActive());
}

void QmlDanmakuOverlayTest::launchesAQueuedMessageIntoTheConfiguredRegion()
{
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(320, 180);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    FakeDanmakuQmlController controller;
    controller.enqueue(message(QStringLiteral("1"), QStringLiteral("first")));
    std::unique_ptr<QQuickItem> overlay(createOverlay(engine, window, controller));
    QVERIFY(overlay != nullptr);

    QTRY_VERIFY_WITH_TIMEOUT(activeLines(overlay.get()).size() == 1, 1500);
    QObject *line = activeLines(overlay.get()).constFirst();
    QVERIFY(line->property("y").toDouble() >= 0);
    QVERIFY(line->property("y").toDouble() < overlay->height() / 2.0);
}

void QmlDanmakuOverlayTest::rendersMessageTextAsPlainText()
{
    QQmlEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qml/components/DanmakuLine.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));

    const QString payload = QStringLiteral("<b>test</b><img src=\"x\"><a href=\"u\">link</a>");
    std::unique_ptr<QObject> line(component.createWithInitialProperties({
        {QStringLiteral("messageText"), payload},
        {QStringLiteral("laneIndex"), 0},
        {QStringLiteral("laneTop"), 0},
        {QStringLiteral("lineWidth"), 320},
        {QStringLiteral("containerWidth"), 320},
        {QStringLiteral("durationMs"), 1000},
        {QStringLiteral("fontSize"), 20},
        {QStringLiteral("fontFamily"), QStringLiteral("Arial")},
        {QStringLiteral("messageOpacity"), 1.0},
        {QStringLiteral("rendering"), QStringLiteral("native")},
        {QStringLiteral("launchedAt"), 0.0},
    }));
    QVERIFY2(line != nullptr, qPrintable(component.errorString()));

    QObject *foreground = line->findChild<QObject *>(QStringLiteral("danmakuForegroundText"));
    QObject *shadow = line->findChild<QObject *>(QStringLiteral("danmakuShadowText"));
    QVERIFY(foreground != nullptr);
    QVERIFY(shadow != nullptr);
    QCOMPARE(foreground->property("text").toString(), payload);
    QCOMPARE(shadow->property("text").toString(), payload);
    QCOMPARE(foreground->property("textFormat").toInt(), 0);
    QCOMPARE(shadow->property("textFormat").toInt(), 0);
}

void QmlDanmakuOverlayTest::doesNotReuseAnUnsafeLane()
{
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(320, 180);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    FakeDanmakuQmlController controller;
    controller.enqueue(message(QStringLiteral("1"), QString(70, QChar('a'))));
    controller.enqueue(message(QStringLiteral("2"), QString(70, QChar('b'))));
    std::unique_ptr<QQuickItem> overlay(createOverlay(engine, window, controller));
    QVERIFY(overlay != nullptr);

    QTRY_VERIFY_WITH_TIMEOUT(activeLines(overlay.get()).size() == 2, 2000);
    const QList<QObject *> lines = activeLines(overlay.get());
    QVERIFY(lines.at(0)->property("y").toDouble() != lines.at(1)->property("y").toDouble());
}

void QmlDanmakuOverlayTest::keepsLaneSpacingSafeForOutlinedText()
{
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(320, 180);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    FakeDanmakuQmlController controller;
    controller.enqueue(message(QStringLiteral("1"), QStringLiteral("first")));
    controller.enqueue(message(QStringLiteral("2"), QStringLiteral("second")));
    std::unique_ptr<QQuickItem> overlay(createOverlay(engine, window, controller));
    QVERIFY(overlay != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(activeLines(overlay.get()).size() == 2, 2000);

    const QList<QObject *> lines = activeLines(overlay.get());
    const qreal fontSize = lines.constFirst()->property("fontSize").toDouble();
    const qreal firstTop = lines.at(0)->property("y").toDouble();
    const qreal secondTop = lines.at(1)->property("y").toDouble();
    const qreal laneGap = qAbs(secondTop - firstTop);
    const qreal minimumSafeGap = fontSize * 1.6;
    QVERIFY2(laneGap >= minimumSafeGap,
             qPrintable(QStringLiteral("danmaku lane gap %1 is smaller than safe gap %2")
                            .arg(laneGap)
                            .arg(minimumSafeGap)));
}

void QmlDanmakuOverlayTest::relayoutsActiveDanmakuWhenContainerHeightChanges()
{
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(320, 180);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    FakeDanmakuQmlController controller;
    controller.enqueue(message(QStringLiteral("1"), QStringLiteral("first")));
    controller.enqueue(message(QStringLiteral("2"), QStringLiteral("second")));
    std::unique_ptr<QQuickItem> overlay(createOverlay(engine, window, controller));
    QVERIFY(overlay != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(activeLines(overlay.get()).size() == 2, 2000);

    const QList<QObject *> before = activeLines(overlay.get());
    QVERIFY(before.at(0)->property("laneIndex").toInt()
            != before.at(1)->property("laneIndex").toInt());
    overlay->setHeight(140);
    QTRY_VERIFY_WITH_TIMEOUT(activeLines(overlay.get()).size() == 2, 2000);
    const QList<QObject *> after = activeLines(overlay.get());
    for (QObject *line : after) {
        QVERIFY(line->property("laneIndex").toInt() >= 0);
        QVERIFY(line->property("y").toDouble() >= 0);
        QVERIFY(line->property("y").toDouble() + line->property("height").toDouble()
                <= overlay->height());
    }
    QVERIFY(after.at(0)->property("laneIndex").toInt()
            != after.at(1)->property("laneIndex").toInt());
}

void QmlDanmakuOverlayTest::shrinksLanesWithoutOverlappingActiveLines()
{
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(320, 180);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    FakeDanmakuQmlController controller;
    controller.enqueue(message(QStringLiteral("1"), QString(70, QChar('a'))));
    controller.enqueue(message(QStringLiteral("2"), QString(70, QChar('b'))));
    std::unique_ptr<QQuickItem> overlay(createOverlay(engine, window, controller));
    QVERIFY(overlay != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(activeLines(overlay.get()).size() == 2, 2000);

    overlay->setHeight(30);
    QTRY_VERIFY_WITH_TIMEOUT(activeLines(overlay.get()).size() <= 1, 1000);
    const QList<QObject *> lines = activeLines(overlay.get());
    for (int index = 0; index < lines.size(); ++index) {
        for (int other = index + 1; other < lines.size(); ++other)
            QVERIFY(!rectanglesOverlap(lines.at(index), lines.at(other)));
    }

    controller.enqueue(message(QStringLiteral("3"), QString(70, QChar('c'))));
    QTRY_VERIFY_WITH_TIMEOUT(activeLines(overlay.get()).size() <= 1, 1000);
    const QList<QObject *> after = activeLines(overlay.get());
    for (int index = 0; index < after.size(); ++index) {
        for (int other = index + 1; other < after.size(); ++other)
            QVERIFY(!rectanglesOverlap(after.at(index), after.at(other)));
    }
}

void QmlDanmakuOverlayTest::clearsActiveAndQueuedMessagesWhenDisabled()
{
    QQmlEngine engine;
    QQuickWindow window;
    window.resize(320, 180);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    FakeDanmakuQmlController controller;
    controller.enqueue(message(QStringLiteral("1"), QStringLiteral("first")));
    controller.enqueue(message(QStringLiteral("2"), QStringLiteral("second")));
    std::unique_ptr<QQuickItem> overlay(createOverlay(engine, window, controller));
    QVERIFY(overlay != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(activeLines(overlay.get()).size() == 1, 1500);

    overlay->setProperty("enabled", false);
    QTRY_COMPARE_WITH_TIMEOUT(activeLines(overlay.get()).size(), 0, 1000);
    QCOMPARE(controller.clearCount(), 0);
    QCOMPARE(controller.lastClearedRoomId(), QString());
}

QTEST_MAIN(QmlDanmakuOverlayTest)

#include "qml_danmaku_overlay_test.moc"
