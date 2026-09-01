#include <QSignalSpy>
#include <QtTest>

#include "ui/room_list_model.h"

namespace {
RoomSnapshot makeSnapshot(const QString &roomId, bool primary)
{
    RoomSnapshot snapshot;
    snapshot.roomId = roomId;
    snapshot.isPrimary = primary;
    snapshot.metadata.roomId = roomId;
    snapshot.metadata.anchorName = QStringLiteral("主播 A");
    snapshot.metadata.title = QStringLiteral("直播标题");
    snapshot.metadata.category = QStringLiteral("游戏");
    snapshot.metadata.viewerLabel = QStringLiteral("1.2 万");
    snapshot.liveStatus = RoomLiveStatus::Online;
    snapshot.playbackHealth = RoomPlaybackHealth::Playing;
    return snapshot;
}
} // namespace

class RoomListModelTest final : public QObject {
    Q_OBJECT

private slots:
    void exposesSafeRoleNames();
    void updatesOneChangedRoomWithoutModelReset();
    void removesRoomWithoutResettingSurvivingDelegates();
    void exposesSafeDanmakuPresentationRoles();
    void exposesAvailableQualityOptionsWithoutPlaybackUrl();
};

void RoomListModelTest::exposesSafeRoleNames()
{
    RoomListModel model;
    const QHash<int, QByteArray> roles = model.roleNames();

    QCOMPARE(roles.value(RoomListModel::RoomIdRole), QByteArrayLiteral("roomId"));
    QCOMPARE(roles.value(RoomListModel::AnchorNameRole), QByteArrayLiteral("anchorName"));
    QCOMPARE(roles.value(RoomListModel::PlaybackStateRole), QByteArrayLiteral("playbackState"));
    QVERIFY(!roles.values().contains(QByteArrayLiteral("mediaSource")));
}

void RoomListModelTest::updatesOneChangedRoomWithoutModelReset()
{
    RoomListModel model;
    QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
    QSignalSpy changes(&model, &QAbstractItemModel::dataChanged);
    model.applySnapshots({makeSnapshot(QStringLiteral("63136"), true),
                          makeSnapshot(QStringLiteral("63137"), false)});
    changes.clear();

    RoomSnapshot updated = makeSnapshot(QStringLiteral("63137"), false);
    updated.metadata.anchorName = QStringLiteral("主播 B");
    model.applySnapshots({makeSnapshot(QStringLiteral("63136"), true), updated});

    QCOMPARE(resets.count(), 0);
    QCOMPARE(changes.count(), 1);
    QCOMPARE(model.data(model.index(1, 0), RoomListModel::AnchorNameRole).toString(),
             QStringLiteral("主播 B"));
}

void RoomListModelTest::removesRoomWithoutResettingSurvivingDelegates()
{
    RoomListModel model;
    QSignalSpy resets(&model, &QAbstractItemModel::modelReset);
    QSignalSpy removals(&model, &QAbstractItemModel::rowsRemoved);
    model.applySnapshots({makeSnapshot(QStringLiteral("63136"), true),
                          makeSnapshot(QStringLiteral("63137"), false)});
    resets.clear();

    model.applySnapshots({makeSnapshot(QStringLiteral("63136"), true)});

    QCOMPARE(resets.count(), 0);
    QCOMPARE(removals.count(), 1);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), RoomListModel::RoomIdRole).toString(),
             QStringLiteral("63136"));
}

void RoomListModelTest::exposesSafeDanmakuPresentationRoles()
{
    RoomListModel model;
    const QHash<int, QByteArray> roles = model.roleNames();

    QCOMPARE(roles.value(RoomListModel::DanmakuStateRole), QByteArrayLiteral("danmakuState"));
    QCOMPARE(roles.value(RoomListModel::DanmakuErrorCodeRole),
             QByteArrayLiteral("danmakuErrorCode"));
    QCOMPARE(roles.value(RoomListModel::DanmakuFilteredRole),
             QByteArrayLiteral("danmakuFiltered"));
    QVERIFY(!roles.values().contains(QByteArrayLiteral("danmakuRaw")));
    QVERIFY(!roles.values().contains(QByteArrayLiteral("endpoint")));

    model.applySnapshots({makeSnapshot(QStringLiteral("63136"), true)});
    RoomPresentationSettings presentation;
    presentation.danmakuState = QStringLiteral("connected");
    presentation.danmakuErrorCode = QStringLiteral("NONE");
    presentation.danmakuFiltered = 4;
    model.applyPresentationSettings({{QStringLiteral("63136"), presentation}});

    const QModelIndex index = model.index(0, 0);
    QCOMPARE(model.data(index, RoomListModel::DanmakuStateRole).toString(),
             QStringLiteral("connected"));
    QCOMPARE(model.data(index, RoomListModel::DanmakuErrorCodeRole).toString(),
             QStringLiteral("NONE"));
    QCOMPARE(model.data(index, RoomListModel::DanmakuFilteredRole).toInt(), 4);
}

void RoomListModelTest::exposesAvailableQualityOptionsWithoutPlaybackUrl()
{
    RoomListModel model;
    RoomSnapshot snapshot = makeSnapshot(QStringLiteral("63136"), true);
    snapshot.availableQualities = {
        QVariantMap{{QStringLiteral("id"), QStringLiteral("high")},
                    {QStringLiteral("label"), QStringLiteral("高清")},
                    {QStringLiteral("quality"), QStringLiteral("high")}},
    };
    model.applySnapshots({snapshot});
    const QVariant value = model.data(model.index(0, 0), RoomListModel::AvailableQualitiesRole);
    QVERIFY(value.canConvert<QVariantList>());
    const QVariantList options = value.toList();
    QCOMPARE(options.size(), 1);
    QVERIFY(!options.first().toMap().contains(QStringLiteral("playbackUrl")));
    QCOMPARE(options.first().toMap().value(QStringLiteral("label")).toString(),
             QStringLiteral("高清"));
}

QTEST_GUILESS_MAIN(RoomListModelTest)

#include "room_list_model_test.moc"
