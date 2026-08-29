#include <QtTest>
#include <QSignalSpy>

#include "ui/monitoring_model.h"
#include "ui/workspace_model.h"

class WorkspaceModelTest final : public QObject {
    Q_OBJECT

private slots:
    void mapsRoomLimitToFixedChineseFeedback();
    void updatesAudioRoomIncrementally();
    void exposesDefaultAudioMode();
    void projectsCoordinatorWorkspaceState();
    void publishesOnlyControllerSuppliedFeedback();
    void exposesToastSeverityAndTimeout();
    void exposesToastDismissalToQml();
    void monitoringModelExposesOnlyFixedHealthLabels();
};

void WorkspaceModelTest::mapsRoomLimitToFixedChineseFeedback()
{
    WorkspaceModel model(nullptr);

    const QString message = model.commandMessage(RoomCommandResult::RoomLimitReached);
    QCOMPARE(message, QStringLiteral("最多添加 9 个房间"));
    QVERIFY(!message.contains(QStringLiteral("://")));
}

void WorkspaceModelTest::updatesAudioRoomIncrementally()
{
    WorkspaceModel model(nullptr);
    QSignalSpy changes(&model, &WorkspaceModel::audioRoomIdChanged);

    model.setAudioRoomId(QStringLiteral("63136"));

    QCOMPARE(model.audioRoomId(), QStringLiteral("63136"));
    QCOMPARE(changes.count(), 1);
    model.setAudioRoomId(QStringLiteral("63136"));
    QCOMPARE(changes.count(), 1);
}

void WorkspaceModelTest::exposesDefaultAudioMode()
{
    WorkspaceModel model(nullptr);

    QCOMPARE(model.audioMode(), QStringLiteral("single"));
    QVERIFY(!model.globalMuted());
}

void WorkspaceModelTest::projectsCoordinatorWorkspaceState()
{
    WorkspaceModel model(nullptr);

    model.setCoordinatorState(QStringLiteral("grid-2x2"),
                              QStringLiteral("63137"),
                              QStringLiteral("63136"));

    QCOMPARE(model.layoutId(), QStringLiteral("grid-2x2"));
    QCOMPARE(model.primaryRoomId(), QStringLiteral("63137"));
    QCOMPARE(model.audioRoomId(), QStringLiteral("63136"));
}

void WorkspaceModelTest::publishesOnlyControllerSuppliedFeedback()
{
    WorkspaceModel model(nullptr);

    model.setLastMessage(QStringLiteral("播放地址不可用"));

    QCOMPARE(model.lastMessage(), QStringLiteral("播放地址不可用"));
    QVERIFY(!model.lastMessage().contains(QStringLiteral("://")));
}

void WorkspaceModelTest::exposesToastSeverityAndTimeout()
{
    WorkspaceModel model(nullptr);

    model.setLastMessage(QStringLiteral("房间数据已刷新"), QStringLiteral("success"), 1200);

    QCOMPARE(model.lastMessage(), QStringLiteral("房间数据已刷新"));
    QCOMPARE(model.lastMessageLevel(), QStringLiteral("success"));
    QCOMPARE(model.lastMessageTimeoutMs(), 1200);
}

void WorkspaceModelTest::exposesToastDismissalToQml()
{
    WorkspaceModel model(nullptr);

    QVERIFY(model.metaObject()->indexOfMethod("setLastMessage(QString)") >= 0);
    model.setLastMessage(QStringLiteral("通知"));
    QVERIFY(QMetaObject::invokeMethod(&model, "setLastMessage",
                                      Q_ARG(QString, QString())));
    QCOMPARE(model.lastMessage(), QString());
}

void WorkspaceModelTest::monitoringModelExposesOnlyFixedHealthLabels()
{
    MonitoringModel model;
    model.setCounts(2, 3, 1);
    model.setStatus(MonitoringModel::NotificationStatus::Enabled,
                    MonitoringModel::RecoveryStatus::Recovering);

    QCOMPARE(model.onlineCount(), 2);
    QCOMPARE(model.offlineCount(), 3);
    QCOMPARE(model.errorCount(), 1);
    QCOMPARE(model.notificationStatus(), QStringLiteral("enabled"));
    QCOMPARE(model.recoveryStatus(), QStringLiteral("recovering"));
}

QTEST_GUILESS_MAIN(WorkspaceModelTest)

#include "workspace_model_test.moc"
