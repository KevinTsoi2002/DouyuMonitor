#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QToolButton>
#include <QtTest/QtTest>

#include "app/room_management_dock.h"

namespace {

RoomSnapshots nineRooms()
{
    RoomSnapshots snapshots;
    snapshots.reserve(9);
    for (int index = 0; index < 9; ++index) {
        snapshots.push_back({QString::number(63136 + index),
                             index == 0,
                             RoomSession::State::Idle,
                             StreamQuality::Auto,
                             StreamQuality::Auto});
    }
    return snapshots;
}

RoomSnapshots twoRooms()
{
    return {{QStringLiteral("63136"),
             true,
             RoomSession::State::Ready,
             StreamQuality::Auto,
             StreamQuality::Auto},
            {QStringLiteral("63137"),
             false,
             RoomSession::State::Ready,
             StreamQuality::High,
             StreamQuality::High}};
}

} // namespace

class RoomManagementDockTest final : public QObject {
    Q_OBJECT

private slots:
    void validatesRoomIdAndCapacityBeforeAdd();
    void emitsAddRemovePrimaryAndQualityIntents();
    void rendersSnapshotsWithoutEchoingProgrammaticChanges();
    void displaysOnlyFixedCommandFeedback();
};

void RoomManagementDockTest::validatesRoomIdAndCapacityBeforeAdd()
{
    RoomManagementDock dock;

    QVERIFY(!dock.addButton()->isEnabled());
    dock.roomIdInput()->setText(QStringLiteral("not-a-room"));
    QVERIFY(!dock.addButton()->isEnabled());
    dock.roomIdInput()->setText(QStringLiteral("63136"));
    QVERIFY(dock.addButton()->isEnabled());

    dock.setRooms(nineRooms());
    QVERIFY(!dock.addButton()->isEnabled());
}

void RoomManagementDockTest::emitsAddRemovePrimaryAndQualityIntents()
{
    RoomManagementDock dock;
    dock.setRooms(twoRooms());

    QSignalSpy adds(&dock, &RoomManagementDock::addRequested);
    QSignalSpy removals(&dock, &RoomManagementDock::removeRequested);
    QSignalSpy primaries(&dock, &RoomManagementDock::primaryRequested);
    QSignalSpy qualityChanges(&dock, &RoomManagementDock::requestedQualityChanged);

    dock.roomIdInput()->setText(QStringLiteral("63138"));
    dock.addButton()->click();
    QCOMPARE(adds.count(), 1);
    QCOMPARE(adds.at(0).at(0).toString(), QStringLiteral("63138"));

    QWidget *secondaryRow = dock.rowForRoom(QStringLiteral("63137"));
    QVERIFY(secondaryRow != nullptr);
    secondaryRow->findChild<QToolButton *>(QStringLiteral("primaryButton"))->click();
    QCOMPARE(primaries.count(), 1);
    QCOMPARE(primaries.at(0).at(0).toString(), QStringLiteral("63137"));

    auto *qualityCombo = secondaryRow->findChild<QComboBox *>(QStringLiteral("qualityCombo"));
    QVERIFY(qualityCombo != nullptr);
    qualityCombo->setCurrentIndex(qualityCombo->findData(static_cast<int>(StreamQuality::Super)));
    QCOMPARE(qualityChanges.count(), 1);
    QCOMPARE(qualityChanges.at(0).at(0).toString(), QStringLiteral("63137"));
    QCOMPARE(qualityChanges.at(0).at(1).value<StreamQuality>(), StreamQuality::Super);

    QWidget *primaryRow = dock.rowForRoom(QStringLiteral("63136"));
    QVERIFY(primaryRow != nullptr);
    primaryRow->findChild<QToolButton *>(QStringLiteral("removeButton"))->click();
    QCOMPARE(removals.count(), 1);
    QCOMPARE(removals.at(0).at(0).toString(), QStringLiteral("63136"));
}

void RoomManagementDockTest::rendersSnapshotsWithoutEchoingProgrammaticChanges()
{
    RoomManagementDock dock;
    const RoomSnapshots snapshots = {{QStringLiteral("63136"),
                                      false,
                                      RoomSession::State::Ready,
                                      StreamQuality::High,
                                      StreamQuality::Standard}};
    dock.setRooms(snapshots);

    QWidget *row = dock.rowForRoom(QStringLiteral("63136"));
    QVERIFY(row != nullptr);
    QCOMPARE(row->findChild<QLabel *>(QStringLiteral("effectiveQualityLabel"))->text(),
             QStringLiteral("Effective: Standard"));
    QCOMPARE(row->findChild<QLabel *>(QStringLiteral("policyLabel"))->text(),
             QStringLiteral("Policy active"));

    QSignalSpy qualityChanges(&dock, &RoomManagementDock::requestedQualityChanged);
    dock.setRooms(snapshots);
    QCOMPARE(qualityChanges.count(), 0);
}

void RoomManagementDockTest::displaysOnlyFixedCommandFeedback()
{
    RoomManagementDock dock;

    dock.setCommandResult(RoomCommandResult::InvalidRoomId);
    QCOMPARE(dock.feedbackText(), QStringLiteral("Invalid room ID"));
    dock.setCommandResult(RoomCommandResult::DuplicateRoomId);
    QCOMPARE(dock.feedbackText(), QStringLiteral("Room already exists"));
    dock.setCommandResult(RoomCommandResult::RoomLimitReached);
    QCOMPARE(dock.feedbackText(), QStringLiteral("Maximum of 9 rooms reached"));
    dock.setCommandResult(RoomCommandResult::Accepted);
    QVERIFY(dock.feedbackText().isEmpty());
}

QTEST_MAIN(RoomManagementDockTest)

#include "room_management_dock_test.moc"
