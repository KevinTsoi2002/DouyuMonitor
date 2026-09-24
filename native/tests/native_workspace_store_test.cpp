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
    first.favoriteAddedAtMs = 1'700'000'000'000;
    first.favoriteSortOrder = 1'700'000'000'000;

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
    void roundTripsFavoriteAddedTimeAndManualOrder();
    void roundTripsRequestedQualityRate();
    void migratesVersionFourRecordsWithoutQualityRate();
    void migratesVersionThreeFavoritesWithoutLosingMembership();
    void normalizesPresetRoomIdsWithoutDuplicatingEntries();
    void roundTripsIndependentTeamsAndOneTeamPerMember();
    void removesDuplicateTeamMembershipWhenLoading();
    void normalizesTeamFieldsAndLimits();
    void defaultsMissingTeamsAndNavigationVisibility();
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

void NativeWorkspaceStoreTest::normalizesPresetRoomIdsWithoutDuplicatingEntries()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    NativeWorkspaceStore store(&settings);
    NativeWorkspaceSnapshot snapshot = fixtureWorkspace();
    NativeWorkspacePreset preset;
    preset.id = QStringLiteral("preset-rooms");
    preset.name = QStringLiteral("五路");
    preset.roomIds = {QStringLiteral("63136"), QStringLiteral("63137"),
                      QStringLiteral("63138"), QStringLiteral("63139"),
                      QStringLiteral("63140")};
    snapshot.presets = {preset};

    QVERIFY(store.save(snapshot));
    QCOMPARE(store.load().presets.front().roomIds, preset.roomIds);
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

void NativeWorkspaceStoreTest::roundTripsFavoriteAddedTimeAndManualOrder()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    NativeWorkspaceStore store(&settings);
    NativeWorkspaceSnapshot snapshot = fixtureWorkspace();
    snapshot.library.front().favoriteAddedAtMs = 1'700'000'000'010;
    snapshot.library.front().favoriteSortOrder = 20;
    snapshot.library.back().favorite = false;
    snapshot.library.back().favoriteAddedAtMs = 0;
    snapshot.library.back().favoriteSortOrder = 0;

    QVERIFY(store.save(snapshot));
    const NativeWorkspaceSnapshot loaded = store.load();
    QCOMPARE(loaded.library.front().favoriteAddedAtMs, qint64(1'700'000'000'010));
    QCOMPARE(loaded.library.front().favoriteSortOrder, qint64(20));
    QCOMPARE(loaded.library.back().favoriteAddedAtMs, qint64(0));
    QCOMPARE(loaded.library.back().favoriteSortOrder, qint64(0));
}

void NativeWorkspaceStoreTest::roundTripsRequestedQualityRate()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    NativeWorkspaceStore store(&settings);
    NativeWorkspaceSnapshot snapshot = fixtureWorkspace();
    snapshot.library.front().requestedQualityRate = 8;
    snapshot.library.back().requestedQualityRate = -1;

    QVERIFY(store.save(snapshot));
    const NativeWorkspaceSnapshot loaded = store.load();
    QCOMPARE(loaded.version, 6);
    QCOMPARE(loaded.library.front().requestedQualityRate, 8);
    QCOMPARE(loaded.library.back().requestedQualityRate, -1);
    QCOMPARE(loaded, snapshot);
}

void NativeWorkspaceStoreTest::migratesVersionFourRecordsWithoutQualityRate()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    settings.setValue(
        QStringLiteral("DouyuMonitor/nativeWorkspaceV1"),
        QByteArray(R"JSON({"version":4,"library":[{"roomId":"63136","metadata":{"roomId":"63136","anchorName":"主播","title":"标题","category":"游戏","viewerLabel":"1"},"requestedQuality":"high","favorite":false,"lastOpenedAtMs":0,"volume":100,"danmakuEnabled":true,"favoriteAddedAtMs":0,"favoriteSortOrder":0}],"groups":[],"activeRoomIds":["63136"],"activeGroupId":"","primaryRoomId":"63136","audioRoomId":"","presets":[],"danmaku":{"globalEnabled":true,"display":{"durationSeconds":8,"fontSize":24,"opacity":0.85,"region":"top","density":"normal","fontFamily":"simhei","rendering":"native"},"governance":{"enabled":true,"keywordBlacklist":[],"duplicateWindowSeconds":3,"peakProtectionEnabled":true},"roomOverrides":{}},"layoutId":"auto","primaryRoomRatio":0.6,"sidebarVisible":true,"audioMode":"single","globalMuted":false})JSON"));
    NativeWorkspaceStore store(&settings);

    const NativeWorkspaceSnapshot loaded = store.load();
    QCOMPARE(loaded.version, 6);
    QCOMPARE(loaded.library.size(), 1);
    QCOMPARE(loaded.library.front().requestedQuality, StreamQuality::High);
    QCOMPARE(loaded.library.front().requestedQualityRate, -1);
}

void NativeWorkspaceStoreTest::migratesVersionThreeFavoritesWithoutLosingMembership()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    settings.setValue(
        QStringLiteral("DouyuMonitor/nativeWorkspaceV1"),
        QByteArray(R"JSON({"version":3,"library":[{"roomId":"63136","metadata":{"roomId":"63136","anchorName":"主播","title":"标题","category":"游戏","viewerLabel":"1"},"requestedQuality":"auto","favorite":true,"lastOpenedAtMs":1700000000010,"volume":100,"danmakuEnabled":true}],"groups":[{"id":"a","name":"A","roomIds":["63136"]},{"id":"b","name":"B","roomIds":["63136"]}],"activeRoomIds":["63136"],"activeGroupId":"a","primaryRoomId":"63136","audioRoomId":"63136","presets":[],"danmaku":{"globalEnabled":true,"display":{"durationSeconds":8,"fontSize":24,"opacity":0.85,"region":"top","density":"normal","fontFamily":"simhei","rendering":"native"},"governance":{"enabled":true,"keywordBlacklist":[],"duplicateWindowSeconds":3,"peakProtectionEnabled":true},"roomOverrides":{}},"layoutId":"auto","primaryRoomRatio":0.6,"sidebarVisible":true,"audioMode":"single","globalMuted":false})JSON"));
    NativeWorkspaceStore store(&settings);

    const NativeWorkspaceSnapshot loaded = store.load();
    QCOMPARE(loaded.library.size(), 1);
    QVERIFY(loaded.library.front().favorite);
    QCOMPARE(loaded.library.front().favoriteAddedAtMs, qint64(1'700'000'000'010));
    QVERIFY(loaded.library.front().favoriteSortOrder > 0);
    QCOMPARE(loaded.groups.size(), 2);
    QCOMPARE(loaded.groups.at(0).roomIds, QStringList({QStringLiteral("63136")}));
    QCOMPARE(loaded.groups.at(1).roomIds, QStringList({QStringLiteral("63136")}));
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

void NativeWorkspaceStoreTest::roundTripsIndependentTeamsAndOneTeamPerMember()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);

    NativeWorkspaceSnapshot snapshot;
    snapshot.teams = {
        {QStringLiteral("team-a"), QStringLiteral("一队"), {QStringLiteral("hamster-001")}},
        {QStringLiteral("team-b"), QStringLiteral("二队"), {QStringLiteral("hamster-002")}},
        {QStringLiteral("team-c"), QStringLiteral("三队"), {}},
        {QStringLiteral("team-d"), QStringLiteral("四队"), {}},
    };

    NativeWorkspaceStore store(&settings);
    QVERIFY(store.save(snapshot));
    const NativeWorkspaceSnapshot loaded = store.load();
    QCOMPARE(loaded.version, 6);
    QCOMPARE(loaded.teams.size(), 4);
    QCOMPARE(loaded.teams.at(2).memberIds, QStringList{});
    QCOMPARE(loaded.teams.at(3).memberIds, QStringList{});
}

void NativeWorkspaceStoreTest::removesDuplicateTeamMembershipWhenLoading()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);

    NativeWorkspaceSnapshot snapshot;
    snapshot.teams = {
        {QStringLiteral("team-a"), QStringLiteral("一队"), {QStringLiteral("hamster-001")}},
        {QStringLiteral("team-b"), QStringLiteral("二队"), {QStringLiteral("hamster-001")}},
    };

    NativeWorkspaceStore store(&settings);
    QVERIFY(store.save(snapshot));
    const NativeWorkspaceSnapshot loaded = store.load();
    QCOMPARE(loaded.teams.at(0).memberIds, QStringList({QStringLiteral("hamster-001")}));
    QCOMPARE(loaded.teams.at(1).memberIds, QStringList{});
}

void NativeWorkspaceStoreTest::normalizesTeamFieldsAndLimits()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);

    NativeWorkspaceSnapshot snapshot;
    snapshot.teams = {
        {QString(), QStringLiteral("无 ID"), {QStringLiteral("hamster-001")}},
        {QStringLiteral("team-empty-name"), QStringLiteral("   "), {}},
        {QStringLiteral("team-long-name"), QString(31, QLatin1Char('x')), {}},
        {QStringLiteral("team-valid"), QStringLiteral("有效队伍"),
         {QStringLiteral("hamster-001"), QStringLiteral("hamster-001"),
          QStringLiteral("not-a-member"), QStringLiteral("hamster-002")}},
        {QStringLiteral("team-empty"), QStringLiteral("空队伍"), {}},
    };
    for (int index = 0; index < 19; ++index) {
        snapshot.teams.push_back({QStringLiteral("team-%1").arg(index),
                                  QStringLiteral("队伍 %1").arg(index),
                                  {}});
    }

    NativeWorkspaceStore store(&settings);
    QVERIFY(store.save(snapshot));
    const NativeWorkspaceSnapshot loaded = store.load();
    QCOMPARE(loaded.teams.size(), 20);
    QCOMPARE(loaded.teams.at(0).id, QStringLiteral("team-valid"));
    QCOMPARE(loaded.teams.at(0).memberIds,
             QStringList({QStringLiteral("hamster-001"), QStringLiteral("hamster-002")}));
    QCOMPARE(loaded.teams.at(1).id, QStringLiteral("team-empty"));
    QCOMPARE(loaded.teams.at(1).memberIds, QStringList{});
}

void NativeWorkspaceStoreTest::defaultsMissingTeamsAndNavigationVisibility()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    settings.setValue(
        QStringLiteral("DouyuMonitor/nativeWorkspaceV1"),
        QByteArray(R"JSON({"version":5,"library":[],"groups":[],"activeRoomIds":[],"activeGroupId":"","primaryRoomId":"","audioRoomId":"","presets":[],"sidebarVisible":true})JSON"));
    NativeWorkspaceStore store(&settings);

    const NativeWorkspaceSnapshot loaded = store.load();
    QCOMPARE(loaded.version, 6);
    QVERIFY(loaded.teams.isEmpty());
    QVERIFY(!loaded.navigationVisible);
}
QTEST_MAIN(NativeWorkspaceStoreTest)

#include "native_workspace_store_test.moc"
