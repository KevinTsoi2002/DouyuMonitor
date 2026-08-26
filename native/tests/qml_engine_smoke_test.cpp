#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QUrl>
#include <QtTest/QtTest>

class QmlEngineSmokeTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsMainQmlWithoutWarnings();
};

void QmlEngineSmokeTest::loadsMainQmlWithoutWarnings()
{
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

QTEST_MAIN(QmlEngineSmokeTest)

#include "qml_engine_smoke_test.moc"
