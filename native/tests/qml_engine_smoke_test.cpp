#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QColor>
#include <QDir>
#include <QImage>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QUrl>
#include <QtTest/QtTest>

#include "ui/mpv_quick_item.h"
#include "test_font_support.h"

namespace {

void registerQmlTypes()
{
    static const int registered = qmlRegisterType<MpvQuickItem>("DouyuNative", 1, 0, "MpvQuickItem");
    Q_UNUSED(registered);
}

} // namespace

class QmlEngineSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void loadsMainQmlWithoutWarnings();
    void loadsModuleWithThemeSingletonColors();
    void compiledModulePaletteTracksThemeSwitches();
};

void QmlEngineSmokeTest::initTestCase()
{
    QVERIFY2(DouyuTestSupport::prepareTestFonts(),
             "No Simplified Chinese font is available for QML tests (check QT_QPA_FONTDIR)");
}

void QmlEngineSmokeTest::loadsMainQmlWithoutWarnings()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

    QVERIFY2(!engine.rootObjects().isEmpty(), "Main.qml did not create a root object");
    QCOMPARE(warnings.count(), 0);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QCOMPARE(window->height(), 800);
    QCOMPARE(window->title(), QStringLiteral("CSTG特勤终端"));
}

void QmlEngineSmokeTest::loadsModuleWithThemeSingletonColors()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    engine.loadFromModule(QStringLiteral("DouyuMonitor"), QStringLiteral("Main"));

    QVERIFY2(!engine.rootObjects().isEmpty(), "Module Main.qml did not create a root object");
    QCOMPARE(warnings.count(), 0);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QCOMPARE(window->color(), QColor(QStringLiteral("#0e1312")));

    QObject *sidebar = window->findChild<QObject *>(QStringLiteral("roomSidebar"));
    QVERIFY(sidebar != nullptr);
    QCOMPARE(sidebar->property("color").value<QColor>(), QColor(QStringLiteral("#161c1b")));

    QObject *statusBar = window->findChild<QObject *>(QStringLiteral("workspaceStatusBar"));
    QVERIFY(statusBar != nullptr);
    QCOMPARE(statusBar->property("color").value<QColor>(), QColor(QStringLiteral("#1e2624")));
}

void QmlEngineSmokeTest::compiledModulePaletteTracksThemeSwitches()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    // loadFromModule exercises the same AOT QML cache as the production executable.
    engine.loadFromModule(QStringLiteral("DouyuMonitor"), QStringLiteral("Main"));
    QVERIFY(!engine.rootObjects().isEmpty());
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);

    QList<QObject *> paletteOwners{window};
    QVERIFY(QTest::qWaitForWindowExposed(window));
    QVERIFY(window->setProperty("reducedMotion", true));
    const QString captureDirectory = QCoreApplication::applicationDirPath()
                                     + "/verification/theme-controls";
    QVERIFY(QDir().mkpath(captureDirectory));
    for (const char *name : {"addRoomDialog", "closeBehaviorDialog", "notificationSettingsDialog",
                             "teamManagerDialog", "danmakuSettingsPanel", "workspacePresetsPanel",
                             "monitoringDrawer"}) {
        auto *owner = window->findChild<QObject *>(QString::fromLatin1(name));
        QVERIFY2(owner != nullptr, name);
        paletteOwners.append(owner);
    }
    QList<QObject *> buttons;
    for (const char *name : {"watchToolsButton", "themeToggleButton", "emptyAddRoomButton",
                             "emptyNavigationButton"}) {
        auto *button = window->findChild<QObject *>(QString::fromLatin1(name));
        QVERIFY2(button != nullptr, name);
        QVERIFY(button->property("contentItem").value<QObject *>() != nullptr);
        buttons.append(button);
    }
    auto *nativeSwitch = window->findChild<QObject *>(QStringLiteral("danmakuGlobalToggle"));
    QVERIFY(nativeSwitch != nullptr);
    QVERIFY(!nativeSwitch->property("enabled").toBool());
    auto *nativeSwitchLabel = nativeSwitch->property("contentItem").value<QObject *>();
    QVERIFY(nativeSwitchLabel != nullptr);

    for (int step = 0; step < 5; ++step) {
        const bool day = step % 2 != 0;
        const QColor text(day ? "#1b281b" : "#e9f0ee");
        const QColor disabledText(day ? "#525f52" : "#9aa8a3");
        const QColor accent(day ? "#295a2c" : "#59dbb9");
        const QColor accentText(day ? "#fbfcfa" : "#071713");
        QTRY_COMPARE(window->color(), QColor(day ? "#f0f3ed" : "#0e1312"));
        for (auto *owner : paletteOwners) {
            const QByteArray context = owner->objectName().toUtf8() + " step=" + QByteArray::number(step);
            QTRY_COMPARE(QQmlProperty::read(owner, "palette.windowText").value<QColor>(), text);
            QTRY_COMPARE(QQmlProperty::read(owner, "palette.buttonText").value<QColor>(), text);
            QTRY_COMPARE(QQmlProperty::read(owner, "palette.highlight").value<QColor>(), accent);
            QTRY_VERIFY2(QQmlProperty::read(owner, "palette.disabled.windowText").value<QColor>()
                         == disabledText, context.constData());
        }
        for (auto *button : buttons) {
            auto *label = button->property("contentItem").value<QObject *>();
            const bool primary = button->objectName() == QStringLiteral("emptyAddRoomButton");
            QTRY_COMPARE(label->property("color").value<QColor>(), primary ? accentText : text);
        }
        QTRY_COMPARE(nativeSwitchLabel->property("color").value<QColor>(), disabledText);
        if (step < 2) {
            window->update();
            QTest::qWait(100);
            const QImage image = window->grabWindow();
            QVERIFY(!image.isNull());
            QVERIFY(image.save(captureDirectory + (day ? "/compiled-day.png" : "/compiled-night.png")));
        }
        if (step < 4)
            QVERIFY(QMetaObject::invokeMethod(window, "toggleTheme"));
    }
    QCOMPARE(warnings.count(), 0);
}

QTEST_MAIN(QmlEngineSmokeTest)

#include "qml_engine_smoke_test.moc"
