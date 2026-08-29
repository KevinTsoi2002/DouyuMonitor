# Qt P1 Room Search Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task with verification checkpoints.

**Goal:** Restore the audited P1 workflow for searching by room number, Douyu link, or anchor name and adding a selected candidate with its public metadata.

**Architecture:** Keep the existing StreamGet JSON search boundary and `RoomSearchResult` type. `AppController` owns one cancellable search request, exposes sanitized status/results to QML, and caches candidate metadata only in memory until the user adds a candidate. `AddRoomDialog.qml` renders the candidate list and calls a typed room-id action; direct room adds remain backward compatible.

**Tech Stack:** Qt 6, Qt Quick/QML, C++20, QtTest, existing Python StreamGet service.

---

### Task 1: Define the failing native search contract

**Files:**
- Modify: `native/tests/app_controller_test.cpp`

- [x] **Step 1: Add a test for link normalization, result exposure, and metadata-preserving add.**

```cpp
void searchesRoomCandidatesAndAddsMetadata();

void AppControllerTest::searchesRoomCandidatesAndAddsMetadata()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    controller.searchRooms(QStringLiteral("https://www.douyu.com/63136"));
    QTRY_COMPARE_WITH_TIMEOUT(controller.searchStatus(), QStringLiteral("success"), 3000);
    QCOMPARE(controller.searchResults().size(), 1);
    const QVariantMap candidate = controller.searchResults().first().toMap();
    QCOMPARE(candidate.value(QStringLiteral("roomId")).toString(), QStringLiteral("63136"));
    QCOMPARE(candidate.value(QStringLiteral("anchorName")).toString(), QStringLiteral("Fake Anchor"));

    QCOMPARE(controller.addRoomCandidate(QStringLiteral("63136")), QString());
    QCOMPARE(controller.rooms()->rowCount(), 1);
    const QModelIndex room = controller.rooms()->index(0, 0);
    QCOMPARE(controller.rooms()->data(room, RoomListModel::AnchorNameRole).toString(),
             QStringLiteral("Fake Anchor"));
    QCOMPARE(controller.rooms()->data(room, RoomListModel::TitleRole).toString(),
             QStringLiteral("Fake Room"));
}
```

- [x] **Step 2: Register the test slot and run it to confirm the missing API fails.**

Run: `cmake --build --preset windows-x64-debug --target app_controller_test; ctest --preset windows-x64-debug -R app_controller_test`

Expected: compile failure because `AppController::searchRooms`, `searchStatus`, `searchResults`, and `addRoomCandidate` are not yet exposed.

### Task 2: Define the failing QML candidate flow

**Files:**
- Modify: `native/tests/qml_interaction_test.cpp`

- [x] **Step 1: Add a fake search controller and candidate interaction test.**

The fake controller exposes `searchResults`, `searchStatus`, and `searchError` properties, records the selected room id from `addRoomCandidate`, and the test asserts that `searchResultList` and `addSearchResultButton` are visible for one result.

- [x] **Step 2: Run the focused QML test before changing QML.**

Run: `cmake --build --preset windows-x64-debug --target qml_interaction_test; ctest --preset windows-x64-debug -R qml_interaction_test`

Expected: failure because the dialog has no search result list or candidate action.

### Task 3: Implement the native search bridge

**Files:**
- Modify: `native/src/ui/app_controller.h`
- Modify: `native/src/ui/app_controller.cpp`

- [x] **Step 1: Add QML properties, signals, and invokables.**

Expose `QVariantList searchResults`, `QString searchStatus`, and `QString searchError`; add `searchRooms(const QString&)` and `addRoomCandidate(const QString&)`.

- [x] **Step 2: Normalize input and cancel stale requests.**

Accept numeric ids, `http`/`https` Douyu URLs whose first path segment is numeric, and non-empty anchor-name text. Reject empty or overlong input with a safe user message. Cancel the previous request before starting a new one.

- [x] **Step 3: Handle only the tracked response and preserve metadata.**

Match the response request id, publish sanitized candidate maps, map service failures to a generic search error, and cache `RoomMetadata` by room id. `addRoomCandidate` calls `addRoomDetailed` with cached metadata, then follows the existing history/persistence path.

- [x] **Step 4: Run the native test and confirm it passes.**

Run: `cmake --build --preset windows-x64-debug --target app_controller_test; ctest --preset windows-x64-debug -R app_controller_test`

Expected: PASS.

### Task 4: Implement the QML search UI

**Files:**
- Modify: `native/app/qml/dialogs/AddRoomDialog.qml`

- [x] **Step 1: Replace the numeric-only input contract.**

Allow free text, call `controller.searchRooms`, and bind status/error/results through safe controller properties.

- [x] **Step 2: Render selectable candidates.**

Add a `ListView` named `searchResultList` with avatar fallback, anchor name, title, room id/category, online label, and an icon action named `addSearchResultButton` that calls `controller.addRoomCandidate(roomId)` and closes on success.

- [x] **Step 3: Run the QML test and confirm it passes.**

Run: `cmake --build --preset windows-x64-debug --target qml_interaction_test; ctest --preset windows-x64-debug -R qml_interaction_test`

Expected: PASS.

### Task 5: Full verification and audit log

**Files:**
- Modify: `docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md`

- [x] **Step 1: Run native focused and full Debug/Release suites.**

Run: `cmake --build --preset windows-x64-debug; ctest --preset windows-x64-debug -j 1; cmake --build --preset windows-x64-release; ctest --preset windows-x64-release -j 1`

- [x] **Step 2: Run diff and sensitive-output checks.**

Run: `git diff --check; rg -n -i "token|cookie|signature|playbackUrl|raw frame" docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md`

Expected: no whitespace errors and no sensitive values in the log.

- [x] **Step 3: Append the sanitized implementation entry.**

Record changed files, fresh test totals, plan alignment, Notion authentication status, and the next audited P1 row. Never record playback URLs, credentials, raw frames, or raw diagnostics.
