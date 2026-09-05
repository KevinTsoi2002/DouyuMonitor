#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QColor>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QUrl>
#include <QtTest/QtTest>

#include "ui/mpv_quick_item.h"

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
    void loadsMainQmlWithoutWarnings();
    void loadsModuleWithThemeSingletonColors();
};

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
    QCOMPARE(window->height(), 720);
    QCOMPARE(window->title(), QStringLiteral("斗鱼多房间监控"));
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
    QCOMPARE(window->color(), QColor(QStringLiteral("#171b22")));

    QObject *sidebar = window->findChild<QObject *>(QStringLiteral("roomSidebar"));
    QVERIFY(sidebar != nullptr);
    QCOMPARE(sidebar->property("color").value<QColor>(), QColor(QStringLiteral("#202731")));

    QObject *statusBar = window->findChild<QObject *>(QStringLiteral("workspaceStatusBar"));
    QVERIFY(statusBar != nullptr);
    QCOMPARE(statusBar->property("color").value<QColor>(), QColor(QStringLiteral("#202731")));
}

QTEST_MAIN(QmlEngineSmokeTest)

#include "qml_engine_smoke_test.moc"
