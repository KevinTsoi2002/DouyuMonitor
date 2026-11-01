#include <QtTest/QtTest>
#include <QJsonDocument>
#include <QJsonObject>

#include "app/s1_archive.h"

class S1ArchiveTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsCompleteSeason();
    void rejectsMalformedAndDuplicateRecords();
};

void S1ArchiveTest::loadsCompleteSeason()
{
    const QVariantMap archive = S1Archive::bundled();
    QCOMPARE(archive.value("version").toInt(), 1);
    QCOMPARE(archive.value("sourceUrl").toString(),
             QStringLiteral("https://cstg-zy.com/#/squad/s1"));
    const auto ranking = archive.value("ranking").toMap().value("participants").toList();
    const auto regular = archive.value("regularSeason").toMap().value("matches").toList();
    const auto finals = archive.value("finals").toMap().value("matches").toList();
    const auto football = archive.value("football").toMap().value("matches").toList();
    const auto placement = archive.value("placement").toMap();
    QCOMPARE(ranking.size(), 46);
    QCOMPARE(regular.size(), 31);
    QCOMPARE(finals.size(), 4);
    QCOMPARE(football.size(), 10);
    const auto leaderboards = archive.value("football").toMap()
                                  .value("leaderboards").toMap();
    QCOMPARE(leaderboards.value("goals").toList().size(), 10);
    QCOMPARE(leaderboards.value("assists").toList().size(), 4);
    QCOMPARE(leaderboards.value("mvp").toList().size(), 7);
    QCOMPARE(leaderboards.value("assists").toList().first().toMap().value("name").toString(),
             QStringLiteral("小婷快跑-"));
    QCOMPARE(placement.value("participants").toList().size(), 36);
    QCOMPARE(placement.value("sessions").toList().size(), 6);
    QVERIFY(regular.first().toMap().value("detail").toString().contains("第 1 场"));
    QVERIFY(football.first().toMap().value("detail").toMap()
            .value("sections").toList().size() >= 3);
}

void S1ArchiveTest::rejectsMalformedAndDuplicateRecords()
{
    QString error;
    QVERIFY(S1Archive::parse(QByteArrayLiteral("{}"), &error).isEmpty());
    QVERIFY(!error.isEmpty());
    QVariantMap archive = S1Archive::bundled();
    QVariantMap ranking = archive.value("ranking").toMap();
    QVariantList entries = ranking.value("participants").toList();
    entries.append(entries.first());
    ranking.insert("participants", entries);
    archive.insert("ranking", ranking);
    QVERIFY(S1Archive::parse(QJsonDocument(QJsonObject::fromVariantMap(archive)).toJson(),
                            &error).isEmpty());
    QVERIFY(error.contains("duplicate"));
}

QTEST_GUILESS_MAIN(S1ArchiveTest)
#include "s1_archive_test.moc"
