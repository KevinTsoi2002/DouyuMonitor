#include <QtTest/QtTest>

#include "danmaku/danmaku_governance.h"
#include "danmaku/danmaku_types.h"

class DanmakuGovernanceTest final : public QObject {
    Q_OBJECT

private slots:
    void sanitizesAndBoundsChatFields();
    void filtersDuplicatesAndBurstTraffic();
    void boundsRepeatedTimestampStorage();
    void keepsInclusiveRateWindowBoundaries();
    void handlesClockRollbackAndKeywordChanges();
};

void DanmakuGovernanceTest::boundsRepeatedTimestampStorage()
{
    DanmakuGovernanceRuntime runtime;
    DanmakuGovernanceSettings settings;
    settings.enabled = false;
    const auto now = QDateTime::fromMSecsSinceEpoch(100'000, Qt::UTC);
    const QVector<DanmakuMessage> batch(10'000, {"id", "room", "user", "text", now});
    QCOMPARE(DanmakuGovernance::apply(batch, settings, runtime, now).size(), batch.size());
    QVERIFY(runtime.inputTimestampsMs.size() <= 1);
    QVERIFY(runtime.acceptedTimestampsMs.size() <= 1);
    QCOMPARE(runtime.stats.recentRate, 3333.33);
}

void DanmakuGovernanceTest::keepsInclusiveRateWindowBoundaries()
{
    DanmakuGovernanceRuntime runtime;
    DanmakuGovernanceSettings settings;
    settings.duplicateWindowSeconds = 1;
    const auto now = QDateTime::fromMSecsSinceEpoch(100'000, Qt::UTC);
    const QVector<DanmakuMessage> batch(100, {"id", "room", "user", "same", now});
    settings.enabled = false;
    DanmakuGovernance::apply(batch, settings, runtime, now);
    QCOMPARE(runtime.stats.level, QStringLiteral("burst"));
    settings.enabled = true;
    const QVector<DanmakuMessage> message{{"new", "room", "user", "different", now}};
    QVERIFY(DanmakuGovernance::apply(message, settings, runtime, now.addMSecs(1000)).isEmpty());
    QCOMPARE(runtime.stats.rateLimited, 1);
    QCOMPARE(DanmakuGovernance::apply(message, settings, runtime, now.addMSecs(1001)).size(), 0);
    const QVector<DanmakuMessage> other{{"other", "room", "user", "other", now}};
    QCOMPARE(DanmakuGovernance::apply(other, settings, runtime, now.addMSecs(1001)).size(), 1);
    DanmakuGovernance::apply({}, settings, runtime, now.addMSecs(3000));
    QCOMPARE(runtime.stats.recentRate, 34.33);
    DanmakuGovernance::apply({}, settings, runtime, now.addMSecs(3001));
    QCOMPARE(runtime.stats.recentRate, 1.0);
}

void DanmakuGovernanceTest::handlesClockRollbackAndKeywordChanges()
{
    DanmakuGovernanceRuntime runtime;
    DanmakuGovernanceSettings settings;
    const auto now = QDateTime::fromMSecsSinceEpoch(100'000, Qt::UTC);
    const QVector<DanmakuMessage> message{{"id", "room", "user", "text", now}};
    QCOMPARE(DanmakuGovernance::apply(message, settings, runtime, now).size(), 1);
    DanmakuGovernance::apply({}, settings, runtime, now.addMSecs(-1));
    QCOMPARE(runtime.stats.recentRate, 0.0);
    settings.keywordBlacklist = {"TEXT"};
    QVERIFY(DanmakuGovernance::apply(message, settings, runtime, now.addMSecs(1)).isEmpty());
    QCOMPARE(runtime.stats.filtered, 1);
    settings.keywordBlacklist.clear();
    QCOMPARE(DanmakuGovernance::apply(message, settings, runtime, now.addSecs(4)).size(), 1);
}

void DanmakuGovernanceTest::sanitizesAndBoundsChatFields()
{
    const auto message = DanmakuGovernance::sanitizeMessage(
        QStringLiteral("63136"), QStringLiteral("id\n"),
        QString(50, QChar('n')), QString(210, QChar('x')),
        QDateTime::fromMSecsSinceEpoch(0, Qt::UTC));
    QVERIFY(message.has_value());
    QCOMPARE(message->nickname.size(), 40);
    QCOMPARE(message->text.size(), 200);
    QVERIFY(!message->id.contains('\n'));
}

void DanmakuGovernanceTest::filtersDuplicatesAndBurstTraffic()
{
    DanmakuGovernanceRuntime runtime;
    DanmakuGovernanceSettings settings;
    settings.keywordBlacklist = {QStringLiteral("spoiler")};
    const auto now = QDateTime::fromMSecsSinceEpoch(10'000, Qt::UTC);
    const QVector<DanmakuMessage> messages = {
        {QStringLiteral("1"), QStringLiteral("63136"), QStringLiteral("a"),
         QStringLiteral("spoiler"), now},
        {QStringLiteral("2"), QStringLiteral("63136"), QStringLiteral("a"),
         QStringLiteral("same"), now},
        {QStringLiteral("3"), QStringLiteral("63136"), QStringLiteral("a"),
         QStringLiteral("same"), now.addMSecs(10)},
    };
    const auto accepted = DanmakuGovernance::apply(messages, settings, runtime,
                                                   now.addMSecs(10));
    QCOMPARE(accepted.size(), 1);
    QCOMPARE(runtime.stats.filtered, 1);
    QCOMPARE(runtime.stats.duplicates, 1);
}

QTEST_GUILESS_MAIN(DanmakuGovernanceTest)

#include "danmaku_governance_test.moc"
