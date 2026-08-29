# Native Qt Danmaku Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver real, safe, native Qt danmaku reception and display for up to nine eligible Douyu rooms, with legacy-equivalent settings, governance, persistence, and lifecycle behavior.

**Architecture:** C++ owns framing, STT parsing, WebSocket transport, deterministic timers, connection state, session queues, governance, and workspace data. `AppController` owns the danmaku controller and synchronizes it from coordinator snapshots; QML consumes only sanitized messages and safe status/settings to render per-tile scrolling overlays and controls.

**Tech Stack:** C++20, Qt 6 Core/QML/Quick/QuickControls2/WebSockets/Test, CMake/Ninja, QML, libmpv, existing StreamGet sidecar.

---

## File Structure

| Path | Responsibility |
| --- | --- |
| `native/src/danmaku/douyu_danmaku_protocol.*` | STT serialization/parsing and legacy binary frame encoding/decoding. |
| `native/src/danmaku/danmaku_types.*` | Shared messages, safe state/error enums, settings, overrides, and statistics. |
| `native/src/danmaku/danmaku_governance.*` | Input cleanup, de-duplication, keyword/repeat/rate decisions, and stats. |
| `native/src/danmaku/danmaku_socket.*` | Abstract socket plus `QWebSocket` adapter. |
| `native/src/danmaku/danmaku_timer_scheduler.*` | Production QTimer scheduler and testable timer interface. |
| `native/src/danmaku/douyu_danmaku_client.*` | One-room connection state machine. |
| `native/src/danmaku/danmaku_session_manager.*` | Maximum-nine session lifecycle, queues, and eligibility. |
| `native/src/danmaku/danmaku_controller.*` | AppController/QML boundary and setting mutation API. |
| `native/app/qml/components/DanmakuOverlay.qml` | Per-tile display queue consumer, track selection, and animation. |
| `native/app/qml/components/DanmakuLine.qml` | One active danmaku visual item. |
| `native/app/qml/components/DanmakuLaneScheduler.js` | Pure QML lane geometry and collision helper. |
| `native/tests/*danmaku*_test.cpp` | No-network unit, integration, and QML tests. |

All commands below run from `D:\DouyuMonitor\.worktrees\codex\qt-libmpv-m0\native` unless a different working directory is stated.

Set the native SDK environment before configuring:

```powershell
$env:QT_ROOT = 'D:\Qt\6.8.3\msvc2022_64'
$env:MPV_ROOT = 'D:\DouyuMonitor\.worktrees\codex\qt-libmpv-m0\native\sdk\mpv'
```

The active worktree is intentionally dirty. Do not reset, checkout, stage, commit, or delete unrelated changes.

### Task 1: Protocol and Build Boundary

**Files:**
- Create: `native/src/danmaku/douyu_danmaku_protocol.h`
- Create: `native/src/danmaku/douyu_danmaku_protocol.cpp`
- Create: `native/tests/douyu_danmaku_protocol_test.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Add a failing protocol test target and tests.**

Create `native/tests/douyu_danmaku_protocol_test.cpp` with the test class and the four initial behavior tests below. The header intentionally does not exist yet, so the first build must fail because the behavior is missing.

```cpp
#include <QtTest/QtTest>

#include "danmaku/douyu_danmaku_protocol.h"

class DouyuDanmakuProtocolTest final : public QObject {
    Q_OBJECT

private slots:
    void encodesLoginFrameWithLittleEndianLengths();
    void decodesFragmentedServerFrames();
    void decodesMultipleFramesFromOneChunk();
    void rejectsRepeatedLengthMismatch();
    void rejectsUnexpectedServerProtocol();
    void rejectsMissingTerminator();
    void rejectsInvalidUtf8();
    void rejectsOversizedFrames();
    void roundTripsEscapedSttFields();
};

void DouyuDanmakuProtocolTest::encodesLoginFrameWithLittleEndianLengths()
{
    const QByteArray frame = DouyuDanmakuProtocol::encodeFrame(
        DouyuDanmakuProtocol::serializeStt({{QStringLiteral("type"), QStringLiteral("loginreq")},
                                             {QStringLiteral("roomid"), QStringLiteral("63136")}}));
    QCOMPARE(qFromLittleEndian<quint32>(frame.constData()), quint32(frame.size() - 4));
    QCOMPARE(qFromLittleEndian<quint16>(frame.constData() + 8), quint16(689));
    QCOMPARE(frame.back(), '\0');
}

void DouyuDanmakuProtocolTest::decodesFragmentedServerFrames()
{
    DouyuDanmakuProtocol::FrameDecoder decoder;
    const QByteArray frame = DouyuDanmakuProtocol::encodeFrame(
        QStringLiteral("type@=chatmsg/rid@=63136/txt@=hello/"), 690);
    QCOMPARE(decoder.push(frame.first(7)).frames, QStringList{});
    const auto result = decoder.push(frame.sliced(7));
    QCOMPARE(result.error, DouyuDanmakuProtocol::FrameError::None);
    QCOMPARE(result.frames, QStringList({QStringLiteral("type@=chatmsg/rid@=63136/txt@=hello/")}));
}

void DouyuDanmakuProtocolTest::rejectsOversizedFrames()
{
    DouyuDanmakuProtocol::FrameDecoder decoder;
    QByteArray frame(12, '\0');
    qToLittleEndian<quint32>(1024U * 1024U + 1U, frame.data());
    QCOMPARE(decoder.push(frame).error, DouyuDanmakuProtocol::FrameError::FrameTooLarge);
}

void DouyuDanmakuProtocolTest::roundTripsEscapedSttFields()
{
    const auto parsed = DouyuDanmakuProtocol::parseStt(
        DouyuDanmakuProtocol::serializeStt({{QStringLiteral("txt"), QStringLiteral("a@b/c")}}));
    QCOMPARE(parsed.value(QStringLiteral("txt")), QStringLiteral("a@b/c"));
}

QTEST_GUILESS_MAIN(DouyuDanmakuProtocolTest)
#include "douyu_danmaku_protocol_test.moc"
```

Implement the four omitted rejection tests by starting from a valid `690` frame, then independently changing its second length word, protocol word, trailing byte, and UTF-8 payload. `decodesMultipleFramesFromOneChunk` concatenates two valid server frames and asserts both STT strings are returned in input order.

Add `WebSockets` to the existing Qt component discovery and register this test target after `stream_service_protocol_test`:

```cmake
find_package(Qt6 CONFIG REQUIRED COMPONENTS
    Gui OpenGL Qml Quick QuickControls2 WebSockets Test QuickTest
)

add_executable(douyu_danmaku_protocol_test
    tests/douyu_danmaku_protocol_test.cpp
)
target_link_libraries(douyu_danmaku_protocol_test PRIVATE Qt6::Core Qt6::Test)
target_include_directories(douyu_danmaku_protocol_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
add_test(NAME douyu_danmaku_protocol_test COMMAND douyu_danmaku_protocol_test)
```

- [ ] **Step 2: Configure and run the test to verify the expected failure.**

Run:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64-debug --target douyu_danmaku_protocol_test
```

Expected: compilation fails because `danmaku/douyu_danmaku_protocol.h` is absent or the referenced namespace/functions are undefined.

- [ ] **Step 3: Implement the minimal protocol module.**

Create the public API below. Keep the protocol module QObject-free and deterministic.

```cpp
namespace DouyuDanmakuProtocol {

enum class FrameError { None, InvalidLength, RepeatedLengthMismatch, UnexpectedProtocol,
                        MissingTerminator, InvalidUtf8, FrameTooLarge };

struct FrameDecodeResult {
    QStringList frames;
    FrameError error = FrameError::None;
};

QString escapeStt(const QString &value);
QString unescapeStt(const QString &value);
QString serializeStt(const QMap<QString, QString> &fields);
QMap<QString, QString> parseStt(const QString &raw);
QByteArray encodeFrame(const QString &payload, quint16 protocol = 689);

class FrameDecoder final {
public:
    FrameDecodeResult push(QByteArrayView chunk);
    void clear();

private:
    QByteArray pending_;
};

} // namespace DouyuDanmakuProtocol
```

Implement `@ -> @A`, `/ -> @S`, and reverse those two substitutions. Encode the two 32-bit length fields and 16-bit protocol in little endian. The decoder must preserve incomplete bytes, accept only server protocol `690`, reject a total frame larger than 1 MiB, reject malformed terminators and repeated lengths, validate UTF-8 with `QStringDecoder`, and return no subsequent frames after the first error.

Add the two protocol source files to the test target and link it to `Qt6::Core` and `Qt6::Test`.

- [ ] **Step 4: Run the focused test green.**

Run:

```powershell
cmake --build --preset windows-x64-debug --target douyu_danmaku_protocol_test
ctest --preset windows-x64-debug -R '^douyu_danmaku_protocol_test$'
```

Expected: one CTest target passes with zero failures.

- [ ] **Step 5: Check the scoped diff without committing.**

Run:

```powershell
git diff --check -- native/CMakeLists.txt native/src/danmaku/douyu_danmaku_protocol.h native/src/danmaku/douyu_danmaku_protocol.cpp native/tests/douyu_danmaku_protocol_test.cpp
```

Expected: no output.

### Task 2: Shared Types and Governance

**Files:**
- Create: `native/src/danmaku/danmaku_types.h`
- Create: `native/src/danmaku/danmaku_governance.h`
- Create: `native/src/danmaku/danmaku_governance.cpp`
- Create: `native/tests/danmaku_governance_test.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Write failing governance tests.**

Create `native/tests/danmaku_governance_test.cpp` with a fixed UTC clock and tests that call `sanitizeMessage` and `applyGovernance`:

```cpp
void DanmakuGovernanceTest::sanitizesAndBoundsChatFields()
{
    const auto message = DanmakuGovernance::sanitizeMessage(
        QStringLiteral("63136"), QStringLiteral("id\n"),
        QString(50, QChar('n')), QString(210, QChar('x')), QDateTime::fromMSecsSinceEpoch(0, Qt::UTC));
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
        {QStringLiteral("1"), QStringLiteral("63136"), QStringLiteral("a"), QStringLiteral("spoiler"), now},
        {QStringLiteral("2"), QStringLiteral("63136"), QStringLiteral("a"), QStringLiteral("same"), now},
        {QStringLiteral("3"), QStringLiteral("63136"), QStringLiteral("a"), QStringLiteral("same"), now.addMSecs(10)},
    };
    const auto accepted = DanmakuGovernance::apply(messages, settings, runtime, now.addMSecs(10));
    QCOMPARE(accepted.size(), 1);
    QCOMPARE(runtime.stats.filtered, 1);
    QCOMPARE(runtime.stats.duplicates, 1);
}
```

Register `danmaku_governance_test` with the new source files and `Qt6::Core`/`Qt6::Test`.

- [ ] **Step 2: Run the test to verify it fails.**

Run:

```powershell
cmake --build --preset windows-x64-debug --target danmaku_governance_test
```

Expected: compilation fails because the types and governance APIs do not exist.

- [ ] **Step 3: Implement shared types and governance.**

Define the following types in `danmaku_types.h` and keep all fields safe for QML projection:

```cpp
enum class DanmakuConnectionState { Idle, Connecting, Connected, Reconnecting, Failed, PlatformBlocked };
enum class DanmakuErrorCode { None, NetworkUnavailable, HandshakeTimeout, ProtocolChanged,
                              RetryExhausted, AuthRequired };
enum class DanmakuRegion { Full, Top, Bottom };
enum class DanmakuDensity { Massive, Normal, Reduced };
enum class DanmakuFontFamily { SimHei, MicrosoftYaHei };
enum class DanmakuRendering { Native, Advanced };

struct DanmakuMessage { QString id; QString roomId; QString nickname; QString text; QDateTime receivedAtUtc; };
struct DanmakuConnectionStatus { QString roomId; DanmakuConnectionState state = DanmakuConnectionState::Idle; int attempt = 0; DanmakuErrorCode errorCode = DanmakuErrorCode::None; };
struct DanmakuDisplaySettings { int durationSeconds = 8; int fontSize = 24; qreal opacity = 0.9; DanmakuRegion region = DanmakuRegion::Full; DanmakuDensity density = DanmakuDensity::Normal; DanmakuFontFamily fontFamily = DanmakuFontFamily::MicrosoftYaHei; DanmakuRendering rendering = DanmakuRendering::Native; };
struct DanmakuGovernanceSettings { bool enabled = true; QStringList keywordBlacklist; int duplicateWindowSeconds = 3; bool peakProtectionEnabled = true; };
struct DanmakuGovernanceOverride { std::optional<bool> enabled; std::optional<QStringList> keywordBlacklist; std::optional<int> duplicateWindowSeconds; std::optional<bool> peakProtectionEnabled; };
struct DanmakuGovernanceStats { QString level = QStringLiteral("normal"); qreal recentRate = 0; qreal peakRate = 0; int filtered = 0; int duplicates = 0; int rateLimited = 0; int queueOverflow = 0; int upstreamDropped = 0; };
```

`DanmakuGovernance::sanitizeMessage` removes C0/DEL controls, replaces CR/LF runs with one space, trims, and applies the exact 200/40/200 Unicode-code-point limits. `DanmakuGovernance::apply` must update the supplied runtime, filter blacklist entries case-insensitively, suppress adjacent equivalent messages inside the configured window, and limit accepted messages to 20 or 10 per second when peak protection selects `crowded` or `burst`. Keep a 3 second input window, 60 second stat window, and 1 second accepted window. Add `validatedDisplaySettings`, `validatedGovernanceSettings`, and `resolvedGovernance` helpers that enforce the documented value ranges.

- [ ] **Step 4: Run governance tests green.**

Run:

```powershell
cmake --build --preset windows-x64-debug --target danmaku_governance_test
ctest --preset windows-x64-debug -R '^danmaku_governance_test$'
```

Expected: one focused target passes and has no network activity.

- [ ] **Step 5: Verify protocol regression safety.**

Run:

```powershell
ctest --preset windows-x64-debug -R '^(douyu_danmaku_protocol_test|danmaku_governance_test)$'
git diff --check -- native/src/danmaku/danmaku_types.h native/src/danmaku/danmaku_governance.h native/src/danmaku/danmaku_governance.cpp native/tests/danmaku_governance_test.cpp
```

Expected: both CTest targets pass and the diff check has no output.

### Task 3: Deterministic Socket, Timer, and Client State Machine

**Files:**
- Create: `native/src/danmaku/danmaku_socket.h`
- Create: `native/src/danmaku/danmaku_socket.cpp`
- Create: `native/src/danmaku/danmaku_timer_scheduler.h`
- Create: `native/src/danmaku/danmaku_timer_scheduler.cpp`
- Create: `native/src/danmaku/douyu_danmaku_client.h`
- Create: `native/src/danmaku/douyu_danmaku_client.cpp`
- Create: `native/tests/douyu_danmaku_client_test.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Write the client test with fake socket and scheduler.**

Define test-only `FakeDanmakuSocket` and `FakeDanmakuTimerScheduler` inside `native/tests/douyu_danmaku_client_test.cpp`. The fake socket records `connectTo`, `sendBinary`, and `close`; it exposes methods to emit `connected`, `binaryFrame`, `networkFailure`, `closed`, and `authenticationRequested`. The fake scheduler stores callbacks by generated ID and provides `advanceBy(int milliseconds)`.

Add tests for these exact behaviors:

```cpp
void DouyuDanmakuClientTest::sendsLoginJoinAndHeartbeatAfterOpening();
void DouyuDanmakuClientTest::becomesConnectedAfterLoginResponse();
void DouyuDanmakuClientTest::rotatesEndpointsAndRetriesWithConfiguredDelays();
void DouyuDanmakuClientTest::blocksOnExplicitAuthenticationAndWaitsForManualRetry();
void DouyuDanmakuClientTest::stopsTimersAndIgnoresLateSocketEvents();
```

For the first test, decode the fake socket's binary writes using `FrameDecoder` and assert STT fields `type=loginreq, roomid=63136`, `type=joingroup, rid=63136, gid=-9999`, then `type=mrkl` after advancing the heartbeat interval. For the fourth test, emit `authenticationRequested`, assert `PlatformBlocked/AuthRequired`, advance 60 seconds, assert no reconnect, call `retry()`, and assert a new connection attempt.

- [ ] **Step 2: Run the test to verify it fails.**

Run:

```powershell
cmake --build --preset windows-x64-debug --target douyu_danmaku_client_test
```

Expected: compilation fails because the socket, scheduler, and client interfaces are absent.

- [ ] **Step 3: Implement the transport boundary and client.**

Use this public boundary:

```cpp
class DanmakuSocket : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void connectTo(const QUrl &url) = 0;
    virtual void sendBinary(const QByteArray &frame) = 0;
    virtual void close() = 0;
signals:
    void connected();
    void binaryFrameReceived(const QByteArray &frame);
    void networkFailure();
    void closed(int code, const QString &reason);
    void authenticationRequested();
};

class DanmakuTimerScheduler {
public:
    using TimerId = quint64;
    using Callback = std::function<void()>;
    virtual ~DanmakuTimerScheduler() = default;
    virtual TimerId once(int delayMs, Callback callback) = 0;
    virtual TimerId repeating(int intervalMs, Callback callback) = 0;
    virtual void cancel(TimerId id) = 0;
};

class DanmakuClient : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void retry() = 0;
signals:
    void statusChanged(const DanmakuConnectionStatus &status);
    void chatReceived(const QString &roomId, const QMap<QString, QString> &rawChat);
};

class DouyuDanmakuClient final : public DanmakuClient {
    Q_OBJECT
public:
    void start() override;
    void stop() override;
    void retry() override;
};

using DanmakuClientFactory = std::function<std::unique_ptr<DanmakuClient>(
    const QString &roomId, QObject *parent)>;
```

`QtDanmakuSocket` wraps `QWebSocket`, forwards only binary frames and generic network failures, forwards `authenticationRequired` as the explicit authentication signal, and closes the socket when that signal is observed. It must not parse `errorString()` for HTTP status information. `QtDanmakuTimerScheduler` owns one QTimer per scheduled ID and deletes timers on cancel or one-shot completion.

`DouyuDanmakuClient` owns a socket and a decoder per connection generation. It sends login and group frames on `connected`, starts the 45 second heartbeat and 10 second handshake timers, emits `Connected` after a valid login/group/chat frame, and resets the failure count after a stable 60 second connection. It uses six WSS URLs in rotation and retry delays `{1000, 2000, 4000, 8000, 15000, 15000}` with `0.8 + random * 0.4` jitter. It treats explicit authentication, policy close `1008` with the legacy authentication evidence, and a matching `loginres/error` message as `PlatformBlocked`; generic handshake failures follow the retry path. The session manager and AppController tests use a `DanmakuClient` subclass instead of a real socket.

Add a `douyu_danmaku` static library for the protocol, types, governance, socket, scheduler, and client sources. Link it to `Qt6::Core` and `Qt6::WebSockets`. Link the test to `douyu_danmaku` and `Qt6::Test`.

- [ ] **Step 4: Run the focused client tests green.**

Run:

```powershell
cmake --build --preset windows-x64-debug --target douyu_danmaku_client_test
ctest --preset windows-x64-debug -R '^douyu_danmaku_client_test$'
```

Expected: one focused target passes without opening a real network connection.

- [ ] **Step 5: Run the three foundational targets.**

Run:

```powershell
ctest --preset windows-x64-debug -R '^(douyu_danmaku_protocol_test|danmaku_governance_test|douyu_danmaku_client_test)$'
git diff --check -- native/src/danmaku native/tests/douyu_danmaku_client_test.cpp native/CMakeLists.txt
```

Expected: all three targets pass and the diff check has no output.

### Task 4: Session Manager and Bounded Queues

**Files:**
- Create: `native/src/danmaku/danmaku_session_manager.h`
- Create: `native/src/danmaku/danmaku_session_manager.cpp`
- Create: `native/tests/danmaku_session_manager_test.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Write failing session manager tests.**

Create fakes using the public client factory from Task 3. Cover the exact tests below:

```cpp
void DanmakuSessionManagerTest::startsAtMostNineEligibleRooms();
void DanmakuSessionManagerTest::stopsRoomsThatBecomeIneligible();
void DanmakuSessionManagerTest::deduplicatesAndBoundsEachRoomQueue();
void DanmakuSessionManagerTest::appliesGovernanceBeforeQueueing();
void DanmakuSessionManagerTest::clearsQueuesAndStopsAllOnShutdown();
```

For the capacity test synchronize ten online, enabled room descriptors and assert nine fakes started plus one `Idle` status. For queue behavior, emit 101 distinct accepted raw chats for room `63136`, assert `pendingCount("63136") == 100`, `takeNextMessage("63136")` returns IDs `2` through `101`, and `queueOverflow == 1`.

- [ ] **Step 2: Run the test to verify it fails.**

Run:

```powershell
cmake --build --preset windows-x64-debug --target danmaku_session_manager_test
```

Expected: compilation fails because `DanmakuSessionManager` is absent.

- [ ] **Step 3: Implement the session manager.**

Use this API:

```cpp
struct DanmakuRoomEligibility {
    QString roomId;
    bool active = false;
    bool roomEnabled = false;
    bool globalEnabled = false;
    bool live = false;
    DanmakuGovernanceSettings governance;
};

class DanmakuSessionManager final : public QObject {
    Q_OBJECT
public:
    void synchronize(const QVector<DanmakuRoomEligibility> &rooms);
    void retry(const QString &roomId);
    std::optional<DanmakuMessage> takeNextMessage(const QString &roomId);
    int pendingCount(const QString &roomId) const;
    int activeSessionCount() const;
    DanmakuConnectionStatus statusForRoom(const QString &roomId) const;
    DanmakuGovernanceStats statsForRoom(const QString &roomId) const;
    void clearStats(const QString &roomId);
    void stopAll();
signals:
    void roomStateChanged(const QString &roomId);
    void messageAvailable(const QString &roomId);
};
```

Store sessions in `QHash<QString, Session>` and preserve a session only while an eligibility descriptor has all four booleans true. Stop and remove all other sessions. Normalize raw chat through Task 2, retain only 200 seen IDs, apply resolved governance before enqueueing, queue at most 100 accepted messages, and emit `messageAvailable` only when a queue transitions from empty to non-empty. Never retain raw frame/STT data after conversion to `DanmakuMessage`.

- [ ] **Step 4: Run focused manager tests green.**

Run:

```powershell
cmake --build --preset windows-x64-debug --target danmaku_session_manager_test
ctest --preset windows-x64-debug -R '^danmaku_session_manager_test$'
```

Expected: the focused target passes with all fake client lifecycle assertions satisfied.

- [ ] **Step 5: Check all danmaku logic tests.**

Run:

```powershell
ctest --preset windows-x64-debug -R '^(douyu_danmaku_protocol_test|danmaku_governance_test|douyu_danmaku_client_test|danmaku_session_manager_test)$'
git diff --check -- native/src/danmaku/danmaku_session_manager.h native/src/danmaku/danmaku_session_manager.cpp native/tests/danmaku_session_manager_test.cpp
```

Expected: four targets pass and no whitespace error is reported.

### Task 5: Version 3 Workspace Settings and Presets

**Files:**
- Modify: `native/src/workspace/native_workspace_types.h`
- Modify: `native/src/workspace/native_workspace_store.cpp`
- Modify: `native/tests/native_workspace_store_test.cpp`

- [ ] **Step 1: Extend workspace-store tests before data structures.**

Add these test slots and assertions:

```cpp
void NativeWorkspaceStoreTest::migratesVersionTwoDanmakuSettingsSafely();
void NativeWorkspaceStoreTest::roundTripsVersionThreeDanmakuSettingsAndOverrides();

void NativeWorkspaceStoreTest::migratesVersionTwoDanmakuSettingsSafely()
{
    // Write a version 2 JSON fixture with one room enabled and one preset with danmakuEnabled=true.
    const NativeWorkspaceSnapshot loaded = store.load();
    QVERIFY(!loaded.danmaku.globalEnabled);
    QVERIFY(loaded.library.front().danmakuEnabled);
    QVERIFY(loaded.presets.front().danmaku.globalEnabled);
}
```

The version 3 round-trip fixture must set global enabled, non-default display values, a blacklist, a room-specific duplicate window override, and the same configuration inside one preset. Compare the entire loaded snapshot with the expected snapshot. Add a JSON assertion that saved bytes do not contain `token`, `cookie`, `playbackUrl`, `requestHeaders`, `signature`, `endpoint`, or `raw`.

- [ ] **Step 2: Run the new persistence tests to verify they fail.**

Run:

```powershell
cmake --build --preset windows-x64-debug --target native_workspace_store_test
ctest --preset windows-x64-debug -R '^native_workspace_store_test$'
```

Expected: compilation fails because `NativeDanmakuConfiguration` and the version 3 migration fields are absent.

- [ ] **Step 3: Implement schema version 3.**

Add these native workspace types, using Task 2 setting types:

```cpp
struct NativeDanmakuConfiguration {
    bool globalEnabled = true;
    DanmakuDisplaySettings display;
    DanmakuGovernanceSettings governance;
    QMap<QString, DanmakuGovernanceOverride> roomOverrides;
    bool operator==(const NativeDanmakuConfiguration &) const = default;
};
```

Add `NativeDanmakuConfiguration danmaku;` to `NativeWorkspaceSnapshot` and `NativeWorkspacePreset`; remove the standalone preset `danmakuEnabled` field. Set `NativeWorkspaceSnapshot::version` and `kCurrentVersion` to 3. Serialize a strict `danmaku` object with `globalEnabled`, `display`, `governance`, and `roomOverrides`; serialize the same object into presets.

Accept versions 1, 2, and 3. Version 2 current-workspace migration must retain each `NativeRoomRecord::danmakuEnabled` and initialize `snapshot.danmaku.globalEnabled = false`. Version 2 preset migration must map its existing boolean to `preset.danmaku.globalEnabled`. Normalize every display/governance/override field through Task 2 validation, remove overrides for unknown library room IDs, and preserve the existing sensitive-key rejection behavior.

- [ ] **Step 4: Run persistence tests green.**

Run:

```powershell
cmake --build --preset windows-x64-debug --target native_workspace_store_test
ctest --preset windows-x64-debug -R '^native_workspace_store_test$'
```

Expected: the focused CTest target passes, including version 1/2 compatibility cases.

- [ ] **Step 5: Re-run the persistence suite and inspect the schema diff.**

Run:

```powershell
ctest --preset windows-x64-debug -R '^native_workspace_store_test$'
git diff --check -- native/src/workspace/native_workspace_types.h native/src/workspace/native_workspace_store.cpp native/tests/native_workspace_store_test.cpp
```

Expected: the persistence target passes and the diff check has no output. Do not build AppController consumers until Task 6 updates their constructor and preset call sites.

### Task 6: AppController, Room Roles, and Safe QML Boundary

**Files:**
- Create: `native/src/danmaku/danmaku_controller.h`
- Create: `native/src/danmaku/danmaku_controller.cpp`
- Modify: `native/src/ui/app_controller.h`
- Modify: `native/src/ui/app_controller.cpp`
- Modify: `native/src/ui/room_list_model.h`
- Modify: `native/src/ui/room_list_model.cpp`
- Modify: `native/tests/app_controller_test.cpp`
- Modify: `native/tests/room_list_model_test.cpp`
- Modify: `native/tests/qml_close_regression_test.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Write failing controller and role tests.**

Add an injectable fake danmaku client factory to `AppController` test construction. Add these exact test slots:

```cpp
void AppControllerTest::synchronizesDanmakuFromGlobalRoomLiveAndActiveState();
void AppControllerTest::stopsDanmakuBeforeServiceShutdown();
void RoomListModelTest::exposesSafeDanmakuPresentationRoles();
```

The AppController eligibility test must add room `63136`, make the fake room status `Online`, enable the room and global master, and assert one fake client starts. It must then independently disable the global master, toggle the room, mark it offline, remove it, and apply an empty active-room set; after each transition assert the fake client is stopped. The room-role test must assert only `connected`, `platform-blocked`, `AUTH_REQUIRED`, and numeric counters are exposed; it must not expose a URL, endpoint, raw message, or raw error.

For the close regression test, pass the same fake danmaku factory and assert `activeSessionCountForTest() == 0` after `window->close()` and engine destruction.

- [ ] **Step 2: Run the tests to verify they fail.**

Run:

```powershell
cmake --build --preset windows-x64-debug --target app_controller_test room_list_model_test qml_close_regression_test
```

Expected: compilation fails because `AppController::danmaku`, its control API, role additions, and factory injection do not exist.

- [ ] **Step 3: Implement the native UI boundary.**

`DanmakuController` owns `DanmakuSessionManager`, stores the current `NativeDanmakuConfiguration`, and exposes the following QML-safe surface:

```cpp
Q_PROPERTY(bool globalEnabled READ globalEnabled NOTIFY settingsChanged)
Q_PROPERTY(QVariantMap displaySettings READ displaySettings NOTIFY settingsChanged)
Q_PROPERTY(QVariantMap governanceSettings READ governanceSettings NOTIFY settingsChanged)

Q_INVOKABLE void setGlobalEnabled(bool enabled);
Q_INVOKABLE void setDisplaySetting(const QString &key, const QVariant &value);
Q_INVOKABLE void setGovernanceSetting(const QString &roomId, const QString &key, const QVariant &value);
Q_INVOKABLE void clearRoomGovernanceOverride(const QString &roomId);
Q_INVOKABLE QVariantMap takeNextMessage(const QString &roomId);
Q_INVOKABLE QVariantMap statusForRoom(const QString &roomId) const;
Q_INVOKABLE QVariantMap statsForRoom(const QString &roomId) const;
Q_INVOKABLE void clearStats(const QString &roomId);
Q_INVOKABLE void retry(const QString &roomId);
Q_INVOKABLE void clearRoom(const QString &roomId);
```

Add `Q_PROPERTY(DanmakuController *danmaku READ danmaku CONSTANT)` to `AppController`. Construct it with the production QWebSocket client factory by default and a fake factory only in tests. In `onSnapshotsChanged`, `toggleDanmaku`, `removeRoom`, `applyWorkspacePreset`, `restoreWorkspace`, and `shutdown`, update the controller configuration and call a single private `synchronizeDanmaku()` helper. That helper creates `DanmakuRoomEligibility` values from coordinator room snapshots, snapshot active IDs, each library record, global configuration, and resolved room governance overrides.

When creating a new library record in `onSnapshotsChanged`, initialize `danmakuEnabled` to `true`. On any controller setting change, copy its configuration into `snapshot_.danmaku`, update presentation, and persist once unless restoration is active. When saving/applying a preset, copy the full `NativeDanmakuConfiguration` instead of a standalone boolean.

Extend `RoomPresentationSettings` and `RoomListModel` roles with `danmakuState`, `danmakuErrorCode`, and safe scalar statistics. `refreshPresentation()` queries the controller for each active room. Add a test-only `activeSessionCountForTest()` only under `DOUYU_TESTING`.

Add the controller/session sources to the `douyu_danmaku` library. Link `app_controller_test`, `qml_close_regression_test`, and `douyu_monitor_native` to `douyu_danmaku`; include relevant sources in existing executable source lists only when they are not already supplied by the library.

- [ ] **Step 4: Run AppController and role tests green.**

Run:

```powershell
cmake --build --preset windows-x64-debug --target app_controller_test room_list_model_test qml_close_regression_test
ctest --preset windows-x64-debug -R '^(app_controller_test|room_list_model_test|qml_close_regression_test)$'
```

Expected: all three targets pass without opening a real danmaku socket.

- [ ] **Step 5: Check the controller integration diff.**

Run:

```powershell
git diff --check -- native/src/danmaku/danmaku_controller.h native/src/danmaku/danmaku_controller.cpp native/src/ui/app_controller.h native/src/ui/app_controller.cpp native/src/ui/room_list_model.h native/src/ui/room_list_model.cpp native/tests/app_controller_test.cpp native/tests/room_list_model_test.cpp native/tests/qml_close_regression_test.cpp
```

Expected: no output.

### Task 7: QML Overlay and Lane Scheduler

**Files:**
- Create: `native/app/qml/components/DanmakuOverlay.qml`
- Create: `native/app/qml/components/DanmakuLine.qml`
- Create: `native/app/qml/components/DanmakuLaneScheduler.js`
- Modify: `native/app/qml/components/RoomTile.qml`
- Modify: `native/app/qml/Main.qml`
- Modify: `native/tests/qml_interaction_test.cpp`
- Create: `native/tests/qml_danmaku_overlay_test.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Write a failing QML overlay test.**

Create a small C++ `FakeDanmakuQmlController` in `native/tests/qml_danmaku_overlay_test.cpp` with `takeNextMessage`, `statusForRoom`, `messageAvailable`, `settingsChanged`, and `clearRoom` behavior. Load `DanmakuOverlay.qml` with room `63136`, a fixed `320 x 180` item size, and this fake controller. Add these tests:

```cpp
void QmlDanmakuOverlayTest::launchesAQueuedMessageIntoTheConfiguredRegion();
void QmlDanmakuOverlayTest::doesNotReuseAnUnsafeLane();
void QmlDanmakuOverlayTest::clearsActiveAndQueuedMessagesWhenDisabled();
```

Seed two fixed messages, inspect the created QML line objects by `objectName`, and assert their y positions are in the selected top/bottom/full region. Disable the overlay and assert the active line count becomes zero and the fake controller receives `clearRoom("63136")`.

- [ ] **Step 2: Run the overlay test to verify it fails.**

Run:

```powershell
cmake --build --preset windows-x64-debug --target qml_danmaku_overlay_test
```

Expected: the QML resource or component cannot be loaded because the overlay files and test target are absent.

- [ ] **Step 3: Implement QML overlay components.**

In `DanmakuLaneScheduler.js`, export `lanes(height, fontSize, region, density)` and `selectLane(lanes, active, candidate)` with the geometry formulas in the approved design. The returned lane object contains `index` and `top`; candidate and active objects contain `laneIndex`, `width`, `containerWidth`, `launchedAt`, `durationMs`, and `fontSize`.

`DanmakuOverlay.qml` accepts `roomId`, `enabled`, and `controller`. It reads `controller.displaySettings`, uses a Timer set to the density interval, requests one map via `takeNextMessage(roomId)`, calculates text width before launch, selects a safe lane, and creates a `DanmakuLine.qml` object. It maintains a bounded active item array, removes an item on animation completion, and clears active items plus calls `controller.clearRoom(roomId)` whenever `enabled` becomes false.

`DanmakuLine.qml` uses one animated root `Item`, a foreground `Text` with `Text.Outline`, and one `NumberAnimation` from `parent.width` to `-implicitWidth`. The native rendering mode shows only the foreground with a one-pixel outline. Advanced mode enables one offset shadow `Text` child inside the same animated root, then uses bolder foreground text and a darker outline; it does not add an effects module or a second animation tree. Set `clip: true`, `enabled: false` for pointer handling, and `z` below tile controls.

Extend `RoomTile.qml` with required `danmakuState` and `danmakuErrorCode` properties, place `DanmakuOverlay` after the mpv Loader and before visual controls, derive `enabled` from the per-room flag and `controller.danmaku.globalEnabled`, and add a small connected indicator to the existing danmaku action. A failed/blocked action opens `controller.danmaku.retry(roomId)` while the existing toggle behavior remains unchanged. In the same task, add `danmakuState: "idle"` and `danmakuErrorCode: ""` to every `previewRooms` element in `Main.qml`, and add those two fields to the `RoomTile.qml` initial-properties map in the existing `rendersOnlineStatusForOnlineToken` test so every previously passing QML target still constructs the tile.

Register all three QML files in `qt_add_qml_module` and every qrc list used by QML smoke, visual, interaction, close-regression, and the new overlay test. Add the test target with Qt Core/Gui/Qml/Quick/Test and the resources it needs.

- [ ] **Step 4: Run the overlay test green.**

Run:

```powershell
cmake --build --preset windows-x64-debug --target qml_danmaku_overlay_test
ctest --preset windows-x64-debug -R '^qml_danmaku_overlay_test$'
```

Expected: the focused QML test passes headlessly with `QT_QPA_PLATFORM=offscreen`.

- [ ] **Step 5: Run existing QML regressions.**

Run:

```powershell
ctest --preset windows-x64-debug -R '^(qml_engine_smoke_test|qml_visual_smoke_test|qml_interaction_test|qml_close_regression_test|qml_danmaku_overlay_test)$'
git diff --check -- native/app/qml/components/DanmakuOverlay.qml native/app/qml/components/DanmakuLine.qml native/app/qml/components/DanmakuLaneScheduler.js native/app/qml/components/RoomTile.qml native/tests/qml_danmaku_overlay_test.cpp native/CMakeLists.txt
```

Expected: all selected QML targets pass and no whitespace issue is reported.

### Task 8: Legacy-Equivalent Settings Panel and Visual Integration

**Files:**
- Modify: `native/app/qml/panels/DanmakuSettingsPanel.qml`
- Modify: `native/app/qml/Main.qml`
- Modify: `native/app/qml/components/RoomTile.qml`
- Modify: `native/tests/qml_interaction_test.cpp`
- Modify: `native/tests/qml_visual_smoke_test.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: Extend QML interaction tests before changing the panel.**

Add these test slots to `native/tests/qml_interaction_test.cpp`:

```cpp
void QmlInteractionTest::showsDanmakuDisplayGovernanceAndStatsTabs();
void QmlInteractionTest::showsDanmakuStatusOnRoomTile();
```

The first test opens `danmakuSettingsPanel`, finds `danmakuDisplayTab`, `danmakuGovernanceTab`, and `danmakuStatsTab`, clicks each, and asserts the corresponding content item is visible. The second constructs `RoomTile.qml` with `danmakuState="connected"` and asserts an object named `danmakuConnectedIndicator` is visible, then constructs `danmakuState="platform-blocked"` and asserts a retry-capable danmaku action is present.

- [ ] **Step 2: Run the interaction test to verify it fails.**

Run:

```powershell
cmake --build --preset windows-x64-debug --target qml_interaction_test
ctest --preset windows-x64-debug -R '^qml_interaction_test$'
```

Expected: the new object names and visible panel surfaces are absent.

- [ ] **Step 3: Replace the settings panel with the three-tab native UI.**

Keep `Popup` and the project palette, but increase the panel height only enough for a scrollable compact layout. Give the three tab buttons exact object names `danmakuDisplayTab`, `danmakuGovernanceTab`, and `danmakuStatsTab`; give the sections `danmakuDisplaySection`, `danmakuGovernanceSection`, and `danmakuStatsSection`.

The display tab binds sliders and segmented controls to `controller.danmaku.setDisplaySetting`: duration slider maps 0-100 to 15-4 seconds, font size 14-36, opacity 0.3-1.0, and the selected region/density/font/rendering token. The governance tab uses a global/room scope ComboBox, binds the four governance fields to `setGovernanceSetting`, limits keyword entry through the C++ validator, and calls `clearRoomGovernanceOverride` only for a room scope. The stats tab reads `statsForRoom` for a selected room or sums the exposed safe stats for all active rooms, then calls `clearStats` for the scope.

Add a compact global master toggle at the panel top bound to `controller.danmaku.globalEnabled`. Preserve every per-room toggle in the list, and bind settings signal changes so the panel updates without reopen. `Main.qml` must continue to pass `controller` and `roomModel`; update preview room objects with default `danmakuState: "idle"`, `danmakuErrorCode: ""`, and zero-valued safe counters so QML smoke mode remains valid.

Update `qml_visual_smoke_test.cpp` to create one safe fixed danmaku fixture through the QML fake controller and verify the captured image contains an overlay line inside a room tile without overlapping the top or bottom controls. Keep the existing `shell-1920x1080.png` snapshot path and add an explicit nonblank-region assertion for the danmaku fixture crop.

- [ ] **Step 4: Run interaction and visual tests green.**

Run:

```powershell
cmake --build --preset windows-x64-debug --target qml_interaction_test qml_visual_smoke_test
ctest --preset windows-x64-debug -R '^(qml_interaction_test|qml_visual_smoke_test)$'
```

Expected: both focused targets pass. Inspect the generated visual smoke PNG with the repository's existing image-check workflow and confirm the fixture text remains inside the tile.

- [ ] **Step 5: Run the complete native Debug suite.**

Run:

```powershell
cmake --build --preset windows-x64-debug
ctest --preset windows-x64-debug
git diff --check
```

Expected: all Debug CTest targets pass and `git diff --check` reports no whitespace errors.

### Task 9: Release Verification, Live Check, and Required Progress Records

**Files:**
- Modify: `docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md`
- Create: one sanitized Notion child page after verification succeeds

- [ ] **Step 1: Build and run the complete Release suite.**

Run:

```powershell
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
ctest --preset windows-x64-release
```

Expected: the complete Release CTest suite passes. If any target fails, stop and return to the task owning that failure; do not proceed to the live check.

- [ ] **Step 2: Run a controlled manual live validation.**

Launch the packaged native executable:

```powershell
& .\out\build\windows-x64-release\douyu_monitor_native.exe
```

In the running application, add one known live Douyu room, keep global and room danmaku enabled, open the settings panel, and validate: connection indicator changes to connected, sanitized scrolling text appears, display settings affect subsequent messages, governance changes affect counters, global disable clears the overlay, room disable stops that room, retry is available after a forced fake failure in the test build, and closing the window leaves no PowerShell or child process behind. Record only outcomes, state names, and counts; do not record chat content, endpoints, URLs, cookies, tokens, or diagnostics.

- [ ] **Step 3: Perform a bounded nine-room lifecycle check.**

Use the deterministic test suite as the proof of the nine-session limit. Optionally add up to nine live rooms only when the environment and room availability allow it; record it as an environment-dependent observation. Do not treat unavailable streams or network throttling as an automated regression.

- [ ] **Step 4: Compare the result against the approved artifacts.**

Read and check every acceptance item in:

```text
docs/superpowers/specs/2026-08-27-qt-native-danmaku-migration-design.md
docs/superpowers/plans/2026-08-27-qt-native-danmaku-migration.md
```

Append a dated, sanitized result to `docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md` containing completed tasks, Debug/Release CTest totals, manual observation result, known environment limits, and the exact next migration priority. Do not include sensitive runtime data.

- [ ] **Step 5: Write and fetch the Notion progress page.**

Create one new child page under the existing native migration progress page. Include the same sanitized milestone summary, plan/design alignment, verification totals, limitations, and next priority. Fetch the new page after writing it to confirm that Notion stored the content. If the Notion MCP connection fails, record the local log and report the connection blocker without exposing authorization data.

- [ ] **Step 6: Final source and artifact integrity check.**

Run:

```powershell
git diff --check
git status --short
```

Expected: no whitespace errors. Report the danmaku files changed separately from pre-existing dirty worktree files; do not reset, stage, commit, or remove any unrelated file.
