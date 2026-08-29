#include <QtTest/QtTest>

#include "danmaku/danmaku_governance.h"
#include "danmaku/danmaku_types.h"

class DanmakuGovernanceTest final : public QObject {
    Q_OBJECT

private slots:
    void sanitizesAndBoundsChatFields();
    void filtersDuplicatesAndBurstTraffic();
};

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
