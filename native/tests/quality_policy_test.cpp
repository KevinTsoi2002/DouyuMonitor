#include <QtTest/QtTest>

#include "workspace/quality_policy.h"

class QualityPolicyTest final : public QObject {
    Q_OBJECT

private slots:
    void keepsUserQualityForUpToFourRooms();
    void adaptsFiveToNineRooms();
    void recommendsStableGridBoundaries();
};

void QualityPolicyTest::keepsUserQualityForUpToFourRooms()
{
    QCOMPARE(resolveRoomQuality(1, true, StreamQuality::High).effectiveQuality,
             StreamQuality::High);
    QCOMPARE(resolveRoomQuality(4, false, StreamQuality::Original).effectiveQuality,
             StreamQuality::Original);
    QCOMPARE(resolveRoomQuality(5, false, StreamQuality::Original).userQuality,
             StreamQuality::Original);
}

void QualityPolicyTest::adaptsFiveToNineRooms()
{
    QCOMPARE(resolveRoomQuality(5, true, StreamQuality::Auto).effectiveQuality,
             StreamQuality::Original);
    QCOMPARE(resolveRoomQuality(5, false, StreamQuality::Original).effectiveQuality,
             StreamQuality::Standard);
    QCOMPARE(resolveRoomQuality(9, false, StreamQuality::High).effectiveQuality,
             StreamQuality::Standard);
}

void QualityPolicyTest::recommendsStableGridBoundaries()
{
    QCOMPARE(recommendedGridId(-1), QStringLiteral("single"));
    QCOMPARE(recommendedGridId(0), QStringLiteral("single"));
    QCOMPARE(recommendedGridId(1), QStringLiteral("single"));
    QCOMPARE(recommendedGridId(4), QStringLiteral("grid-2x2"));
    QCOMPARE(recommendedGridId(6), QStringLiteral("grid-3x2"));
    QCOMPARE(recommendedGridId(9), QStringLiteral("grid-3x3"));
    QCOMPARE(recommendedGridId(10), QStringLiteral("grid-3x3"));
}

QTEST_GUILESS_MAIN(QualityPolicyTest)

#include "quality_policy_test.moc"
