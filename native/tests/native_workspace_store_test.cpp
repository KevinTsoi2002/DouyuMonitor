#include <QSettings>
#include <QTemporaryDir>
#include <QtTest/QtTest>

#include "workspace/native_workspace_store.h"

namespace {

NativeWorkspaceSnapshot fixtureWorkspace()
{
    NativeWorkspaceSnapshot snapshot;
    snapshot.activeRoomIds = {QStringLiteral("63136"), QStringLiteral("63137")};
    snapshot.activeGroupId = QStringLiteral("default");
    snapshot.primaryRoomId = QStringLiteral("63137");
    snapshot.audioRoomId = QStringLiteral("63136");

    NativeRoomRecord first;
    first.roomId = QStringLiteral("63136");
    first.metadata = {
        QStringLiteral("63136"),
        QStringLiteral("主播 A"),
        QStringLiteral("标题 A"),
        QStringLiteral("游戏"),
        QStringLiteral("1.2 万"),
        QUrl(QStringLiteral("https://example.invalid/a.png")),
    };
    first.requestedQuality = StreamQuality::High;
    first.favorite = true;
    first.lastOpenedAtMs = 1'700'000'000'000;

    NativeRoomRecord second;
    second.roomId = QStringLiteral("63137");
    second.metadata = {
        QStringLiteral("63137"),
        QStringLiteral("主播 B"),
        QStringLiteral("标题 B"),
        QStringLiteral("赛事"),
        QStringLiteral("8,000"),
        QUrl(),
    };
    second.requestedQuality = StreamQuality::Standard;
    second.lastOpenedAtMs = 1'700'000'000'001;

    snapshot.library = {first, second};
    snapshot.groups = {{
        QStringLiteral("default"),
        QStringLiteral("默认分组"),
        {QStringLiteral("63136"), QStringLiteral("63137")},
    }};
    return snapshot;
}

} // namespace

class NativeWorkspaceStoreTest final : public QObject {
    Q_OBJECT

private slots:
    void roundTripsSafeWorkspaceState();
    void rejectsMalformedAndSensitiveValues();
    void normalizesDuplicateAndInvalidReferences();
};

void NativeWorkspaceStoreTest::roundTripsSafeWorkspaceState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    NativeWorkspaceStore store(&settings);
    const NativeWorkspaceSnapshot expected = fixtureWorkspace();

    QVERIFY(store.save(expected));
    QCOMPARE(store.load(), expected);
}

void NativeWorkspaceStoreTest::rejectsMalformedAndSensitiveValues()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    settings.setValue(
        QStringLiteral("DouyuMonitor/nativeWorkspaceV1"),
        QByteArray(R"({"library":[{"roomId":"63136","playbackUrl":"https://secret.invalid"}]})"));
    NativeWorkspaceStore store(&settings);

    QVERIFY(store.load().library.isEmpty());
}

void NativeWorkspaceStoreTest::normalizesDuplicateAndInvalidReferences()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    NativeWorkspaceStore store(&settings);
    NativeWorkspaceSnapshot snapshot = fixtureWorkspace();
    snapshot.library.append(snapshot.library.front());
    snapshot.activeRoomIds.append(QStringLiteral("99999"));
    snapshot.groups.front().roomIds.append(QStringLiteral("99999"));
    snapshot.primaryRoomId = QStringLiteral("99999");
    snapshot.audioRoomId = QStringLiteral("99999");

    QVERIFY(store.save(snapshot));
    const NativeWorkspaceSnapshot loaded = store.load();
    QCOMPARE(loaded.library.size(), 2);
    QCOMPARE(loaded.activeRoomIds, QStringList({QStringLiteral("63136"), QStringLiteral("63137")}));
    QCOMPARE(loaded.groups.front().roomIds,
             QStringList({QStringLiteral("63136"), QStringLiteral("63137")}));
    QVERIFY(loaded.primaryRoomId.isEmpty());
    QVERIFY(loaded.audioRoomId.isEmpty());
}

QTEST_MAIN(NativeWorkspaceStoreTest)

#include "native_workspace_store_test.moc"
