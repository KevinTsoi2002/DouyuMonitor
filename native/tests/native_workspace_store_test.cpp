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
    void roundTripsPresentationSettingsAndPresets();
    void loadsVersionOneSnapshotsWithoutPresets();
    void defaultsInvalidPersistedVolume();
    void migratesVersionTwoDanmakuSettingsSafely();
    void roundTripsVersionThreeDanmakuSettingsAndOverrides();
    void roundTripsAudioModeAndGlobalMute();
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

void NativeWorkspaceStoreTest::roundTripsPresentationSettingsAndPresets()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    NativeWorkspaceStore store(&settings);
    NativeWorkspaceSnapshot snapshot = fixtureWorkspace();
    snapshot.library.front().volume = 35;
    snapshot.library.front().danmakuEnabled = true;
    NativeWorkspacePreset preset;
    preset.id = QStringLiteral("preset-1");
    preset.name = QStringLiteral("比赛日");
    preset.layoutId = QStringLiteral("grid-2x2");
    preset.activeGroupId = QStringLiteral("default");
    preset.primaryRoomId = QStringLiteral("63137");
    preset.audioRoomId = QStringLiteral("63136");
    preset.roomIds = {QStringLiteral("63136"), QStringLiteral("63137")};
    preset.sidebarVisible = false;
    preset.danmaku.globalEnabled = true;
    snapshot.presets = {preset};

    QVERIFY(store.save(snapshot));
    QCOMPARE(store.load(), snapshot);
}

void NativeWorkspaceStoreTest::roundTripsAudioModeAndGlobalMute()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    NativeWorkspaceStore store(&settings);
    NativeWorkspaceSnapshot snapshot = fixtureWorkspace();
    snapshot.audioMode = QStringLiteral("multi");
    snapshot.globalMuted = true;
    NativeWorkspacePreset preset;
    preset.id = QStringLiteral("audio-preset");
    preset.name = QStringLiteral("多房声音");
    preset.audioMode = QStringLiteral("multi");
    preset.globalMuted = true;
    snapshot.presets = {preset};

    QVERIFY(store.save(snapshot));
    const NativeWorkspaceSnapshot loaded = store.load();
    QCOMPARE(loaded.audioMode, QStringLiteral("multi"));
    QVERIFY(loaded.globalMuted);
    QCOMPARE(loaded.presets.front().audioMode, QStringLiteral("multi"));
    QVERIFY(loaded.presets.front().globalMuted);
}

void NativeWorkspaceStoreTest::loadsVersionOneSnapshotsWithoutPresets()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    settings.setValue(QStringLiteral("DouyuMonitor/nativeWorkspaceV1"), QByteArray(R"({"version":1,"library":[],"groups":[],"activeRoomIds":[],"activeGroupId":"","primaryRoomId":"","audioRoomId":""})"));
    NativeWorkspaceStore store(&settings);

    const NativeWorkspaceSnapshot loaded = store.load();
    QVERIFY(loaded.presets.isEmpty());
}

void NativeWorkspaceStoreTest::defaultsInvalidPersistedVolume()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    settings.setValue(QStringLiteral("DouyuMonitor/nativeWorkspaceV1"), QByteArray(R"({"version":2,"library":[{"roomId":"63136","metadata":{"roomId":"63136","anchorName":"主播","title":"标题","category":"游戏","viewerLabel":"1"},"requestedQuality":"auto","favorite":false,"lastOpenedAtMs":0,"volume":101,"danmakuEnabled":true}],"groups":[],"activeRoomIds":["63136"],"activeGroupId":"","primaryRoomId":"","audioRoomId":"","presets":[]})"));
    NativeWorkspaceStore store(&settings);

    const NativeWorkspaceSnapshot loaded = store.load();
    QCOMPARE(loaded.library.size(), 1);
    QCOMPARE(loaded.library.front().volume, 100);
    QVERIFY(loaded.library.front().danmakuEnabled);
}

void NativeWorkspaceStoreTest::migratesVersionTwoDanmakuSettingsSafely()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    settings.setValue(QStringLiteral("DouyuMonitor/nativeWorkspaceV1"), QByteArray(R"JSON({"version":2,"library":[{"roomId":"63136","metadata":{"roomId":"63136","anchorName":"主播","title":"标题","category":"游戏","viewerLabel":"1"},"requestedQuality":"auto","favorite":false,"lastOpenedAtMs":0,"volume":100,"danmakuEnabled":true}],"groups":[],"activeRoomIds":["63136"],"activeGroupId":"","primaryRoomId":"","audioRoomId":"","presets":[{"id":"p1","name":"旧预设","layoutId":"single","activeGroupId":"","primaryRoomId":"63136","audioRoomId":"","roomIds":["63136"],"sidebarVisible":true,"danmakuEnabled":true}]})JSON"));
    NativeWorkspaceStore store(&settings);

    const NativeWorkspaceSnapshot loaded = store.load();
    QVERIFY(!loaded.danmaku.globalEnabled);
    QVERIFY(loaded.library.front().danmakuEnabled);
    QVERIFY(loaded.presets.front().danmaku.globalEnabled);
}

void NativeWorkspaceStoreTest::roundTripsVersionThreeDanmakuSettingsAndOverrides()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    NativeWorkspaceStore store(&settings);
    NativeWorkspaceSnapshot expected = fixtureWorkspace();
    expected.danmaku.globalEnabled = false;
    expected.danmaku.display.durationSeconds = 12;
    expected.danmaku.display.fontSize = 30;
    expected.danmaku.display.opacity = 0.65;
    expected.danmaku.governance.keywordBlacklist = {QStringLiteral("spoiler")};
    expected.danmaku.roomOverrides[QStringLiteral("63136")].duplicateWindowSeconds = 7;
    NativeWorkspacePreset preset;
    preset.id = QStringLiteral("preset-3");
    preset.name = QStringLiteral("新预设");
    preset.roomIds = {QStringLiteral("63136"), QStringLiteral("63137")};
    preset.danmaku = expected.danmaku;
    expected.presets = {preset};

    QVERIFY(store.save(expected));
    QCOMPARE(store.load(), expected);
    const QByteArray saved = settings.value(QStringLiteral("DouyuMonitor/nativeWorkspaceV1"))
                                 .toByteArray();
    for (const QByteArray &key : {QByteArrayLiteral("token"), QByteArrayLiteral("cookie"),
                                  QByteArrayLiteral("playbackUrl"), QByteArrayLiteral("requestHeaders"),
                                  QByteArrayLiteral("signature"), QByteArrayLiteral("endpoint"),
                                  QByteArrayLiteral("raw")}) {
        QVERIFY2(!saved.contains(key), key.constData());
    }
}

QTEST_MAIN(NativeWorkspaceStoreTest)

#include "native_workspace_store_test.moc"
