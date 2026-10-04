# Maozi Team Import Implementation Plan

> **For agentic workers:** Use `executing-plans` for inline execution with checkpoints. No delegation, commits, publishing, or destructive cleanup is authorized.

**Goal:** Add a refreshed, previewed, user-confirmed import of verified Maozi teams into the existing navigation roster.

**Architecture:** The rank client exposes validated team identifiers and fresh snapshot status. A pure workspace planner matches members and proposes changes; AppController owns refresh, preview validity, persistence, and publication. TeamManagerDialog only renders import state and invokes controller commands.

**Tech Stack:** Qt 6.8.3, C++20, QML, QSettings, CMake, Qt Test.

**Approved Spec:** `docs/superpowers/specs/2026-10-04-maozi-team-import-design.md`.

## Checkpoint 0: Current Contract

- [x] Read the existing rank parser, controller commands, workspace normalization, and team dialog.
- [x] Confirm that existing uncommitted changes belong to the earlier Debug work and must remain untouched.
- [x] Establish current page evidence: four teams, 44 names matched to 58 bundled members.
- [x] Verify team identifier/name correspondence from authorized current public business data.

Execution of Checkpoint 1 depends on the last item. Do not infer numeric identifiers from button order. Do not silently change the old numeric mapping. No web credentials or raw responses should be saved.

On 2026-10-04, the browser's old tab was absent and recreation was denied by its safety review. Renewed authorization is needed before another browser attempt. Do not use alternate network tools to bypass this denial.

Record only the verified field types and mapping in the approved spec after access is restored. Unknown fields must fail closed for import.

## Checkpoint 1: Rank Data Contract

**Files:** `native/src/app/maozi_rank_client.h`, `native/src/app/maozi_rank_client.cpp`, `native/tests/maozi_rank_client_test.cpp`.

- [ ] Extend the existing CloudBaseFixture with synthetic public business samples using the verified contract, not an actual raw response.
- [ ] Cover each verified team identifier/name pair, null/unassigned teams, an unknown identifier, malformed field types, and duplicate host identities.
- [ ] Run the added tests before changing the parser; verify failures concern old mapping or missing contract validation.
- [ ] Expose a stable team identifier alongside the validated name. Keep unknown identifiers distinguishable from unassigned hosts.
- [ ] Add explicit completion evidence for each requested snapshot so the controller cannot mistake preserved stale entries for a successful refresh.
- [ ] Re-run the complete `maozi_rank_client_test`.

Refresh completion contract:

```cpp
signals:
    void snapshotRefreshFinished(bool success);
```

Successful parsing emits completion only after applying the full snapshot. Request, authentication, timeout, and parsing failures emit failure once for that refresh. Version checks must not masquerade as snapshot completion.

## Checkpoint 2: Pure Import Planner

**Files:** create `native/src/workspace/maozi_team_import.h` and `.cpp`; create `native/tests/maozi_team_import_test.cpp`; register these in `native/CMakeLists.txt` using the existing workspace library/test pattern.

Use this public contract:

```cpp
struct MaoziTeamImportPlan {
    QVector<NativeTeam> teams;
    QVariantList previewTeams;
    QStringList unmatchedNames;
    QStringList conflictNames;
    QString error;
    int createdTeams = 0;
    int changedMembers = 0;
    int unchangedMembers = 0;
};

class MaoziTeamImport final {
public:
    static MaoziTeamImportPlan build(
        const QVector<NativeTeam> &currentTeams,
        const QVector<GuildMember> &roster,
        const QVector<GuildRoomCacheEntry> &roomCache,
        const QVariantList &rankEntries);
};
```

- [ ] Add tests for ordinary room identity, canonical-name fallback for vanity IDs, role suffixes, and the existing explicit alias.
- [ ] Add tests for same-name local team reuse, migration from another team, unrelated-member preservation, empty-old-team preservation, and repeat import with zero changes.
- [ ] Add tests for incompatible room/name identities, duplicate host identity, cross-team member duplication, ambiguous names, and unknown members.
- [ ] Add tests for leaders excluded from teams, unassigned hosts left unchanged, duplicate local team names rejected, and the 20-team limit.
- [ ] Configure/build this test and observe failures before implementing matching or mutation.
- [ ] Implement indexed candidate matching without collapsing duplicate identities into a single hash entry.
- [ ] Generate preview and candidate teams together. Mutation takes place only in the candidate copy.
- [ ] Rebuild and run the entire planner test.

Representative test assertion:

```cpp
const auto plan = MaoziTeamImport::build(currentTeams, roster, roomCache, entries);
QVERIFY(plan.error.isEmpty());
QCOMPARE(plan.teams.at(0).id, existingTeamId);
QVERIFY(plan.teams.at(0).memberIds.contains(importedMemberId));
QVERIFY(plan.teams.at(0).memberIds.contains(unrelatedManualMemberId));
QCOMPARE(plan.changedMembers, 1);
const auto repeated = MaoziTeamImport::build(plan.teams, roster, roomCache, entries);
QCOMPARE(repeated.createdTeams, 0);
QCOMPARE(repeated.changedMembers, 0);
```

Use fixture-owned IDs/names and real planner calls. An invalid team contract rejects the entire plan; member-level conflicts are reported and left unchanged.

## Checkpoint 3: Controller And Persistence

**Files:** `native/src/ui/app_controller.h`, `.cpp`, `native/tests/app_controller_test.cpp`; persistence tests in `native/tests/native_workspace_store_test.cpp`.

Controller public commands and presentation:

```cpp
Q_INVOKABLE void previewMaoziTeamImport();
Q_INVOKABLE QString confirmMaoziTeamImport();
Q_INVOKABLE void cancelMaoziTeamImport();
Q_PROPERTY(QVariantMap maoziTeamImport
           READ maoziTeamImport NOTIFY maoziTeamImportChanged)
```

Presentation states: `idle`, `loading`, `preview`, `error`. The presentation map holds teams, counts, unmatched/conflict lists, a fixed safe error message, and `canConfirm`.

- [ ] Test that starting a request leaves local teams unchanged.
- [ ] Test refresh failure with existing entries: no preview and no import from stale data.
- [ ] Test cancel while loading and cancel after preview.
- [ ] Test duplicate request/confirmation prevention.
- [ ] Test stale preview after rank data or local team changes.
- [ ] Test success saves and restores the same team IDs/member ownership.
- [ ] Test save failure does not publish candidate teams or report success.
- [ ] Test active rooms, room IDs, favorites, history, and playback commands remain unchanged.
- [ ] Run the focused controller tests before adding production commands.
- [ ] Connect explicit snapshot completion to planner construction only for the current import request.
- [ ] Capture the exact previewed teams and rank entries. Confirm compares current state against these snapshots, not only a possibly absent version string.
- [ ] Confirm on a complete candidate workspace; save before replacing the application snapshot. Follow existing workspace normalization and signal emission.
- [ ] Preserve failed-save candidate isolation; do not retry persistence through an unrelated state mutation.
- [ ] Run controller and persistence tests.

## Checkpoint 4: Team Management UI

**Files:** `native/app/qml/dialogs/TeamManagerDialog.qml`, `native/tests/qml_interaction_test.cpp`, relevant visual tests.

- [ ] Add interaction tests asserting refresh command, disabled repeat clicks, preview-only state, cancel, confirm, and error state.
- [ ] Run those tests to establish the missing UI behavior.
- [ ] Add the import command to the existing team dialog.
- [ ] Replace the dialog content with a preview while import is active, rather than stacking dialogs.
- [ ] Render each team's source count, matched count, retained local members, member list, total changes, and unmatched/conflict names.
- [ ] Provide confirmation only when `canConfirm` is true; provide cancellation and safe loading/error presentation.
- [ ] Use the existing Theme and constrained scroll area; guard absent controller properties in standalone fixtures.
- [ ] Run interaction, engine smoke, and visual smoke tests.
- [ ] Inspect screenshots at the dialog's supported minimum size and normal desktop size before accepting any baseline updates.

## Checkpoint 5: Integration Evidence

- [ ] Run rank client, planner, controller, and workspace persistence tests.
- [ ] Run QML engine, interaction, visual smoke, and Windows close regression tests.
- [ ] Build the executable from current source in both 16-room and 24-room configurations, preserving existing build settings.
- [ ] Run `--self-test` on each freshly built executable.
- [ ] Verify the real refreshed preview against the authorized page; do not alter the user's workspace to test confirmation without consent. Use an isolated test workspace.
- [ ] Review `git diff --check`, scoped diffs, and repository status for secrets, unrelated changes, and generated artifacts.
- [ ] Record actual commands/results and limitations in this plan. Do not describe unit tests as real-stream performance validation.

Developer environment:

```powershell
cmd /c "call D:\VSBuildTools\VC\Auxiliary\Build\vcvars64.bat >nul && cmake --build D:\DouyuMonitor\native\out\build\windows-x64-release --target maozi_rank_client_test maozi_team_import_test app_controller_test native_workspace_store_test qml_engine_smoke_test qml_interaction_test qml_visual_smoke_test douyu_monitor_native"
ctest --test-dir D:\DouyuMonitor\native\out\build\windows-x64-release --output-on-failure -j 1
git diff --check
```

Configure target registration before requesting the new test target. If the existing NMake dependency files are corrupt, use the established Ninja build or a separate configured build; do not manually patch generated dependency files.

## Status

Implementation is in progress. Design and plan are approved.

### Execution Evidence

- Verified public DOM mapping on 2026-10-04: 0=red, 1=black, 2=purple, 3=blue.
- Rank parser exposes validated team IDs and explicit refresh completion. Tests cover all four IDs, null/missing, wrong types, fractional IDs, timeout-once behavior, and failure.
- Pure import planner is implemented; tests cover identity conflicts, duplicate host IDs, cache verification, preserved members/empty teams, repeat import, leaders, unassigned hosts, duplicate local teams, and capacity.
- Controller and QML implementation are connected. Test-first failures were observed for missing commands, missing import button, and missing state changes.
- Controller tests cover save/restore, no playback/library changes, refresh failure with retained entries, loading/preview cancellation, repeated requests, stale local/rank state, and failed save isolation.
- QML interaction test covers refresh, disabled/loading controls, preview, cancel/close, confirmation, error, and four-team screenshots.
- Before final refinements, release16 selected CTest was 8/8 and beta24 selected CTest was 6/6.
- Later incremental builds produced `0xc0000409` in controller and close tests. Generated Ninja dependency prefixes did not match compiler output; stale header-dependent object files were implicated. Clean rebuilding both configurations restored these tests. Do not manually edit generated rules.
- Final release16 selected CTest: 8/8, 39.13 seconds. Final beta24 selected CTest: 8/8, 47.41 seconds. Both configurations were clean-built.
- Both freshly built executables returned exit 0 for `--self-test` with `native self-test passed: Qt Quick renderer`. This is a renderer check, not live multi-stream performance acceptance.
- Windows-backend team-import interaction test passed without customization warnings after restricting this dialog to the Basic controls style. Four-team long-name preview is captured at 640x720 and 1280x900; offscreen snapshots are not evidence of native Chinese font rendering.
- The opt-in isolated read-only live preview returned four teams and 36 assigned members with zero identity conflicts/unmatched assigned members. This does not prove all 44 page members import.
- Follow-up browser access was denied. Do not retry via other external access methods. Current captain assignment difference is an unresolved acceptance gap.
- No installer, commit, tag, push, release, or user-workspace confirmation was performed.

The retained old Debug changes in `MaoziRankPage.qml`, existing controller/resolver/interaction tests, and local diagnostics must remain intact.

### Checkpoint Summary

- Rank data contract, pure planner, controller/persistence, and preview UI are implemented with focused red/green evidence.
- Integration builds and local regressions are complete subject to final results above.
- End-to-end live membership acceptance remains incomplete: the 36/44 member difference needs renewed browser access and source-contract verification. Do not describe the feature as fully accepted until resolved.
- No commit or publication is requested. Keep the current workspace changes.

### Captain Assignment Follow-Up

- [x] User supplied a 2026-10-04 captain ranking screenshot after browser access remained blocked.
- [x] Recorded only public captain room/team assignments in the spec; no browser credentials or raw response were collected.
- [x] Reproduced missing captain membership with a local fixture: a null team parsed as unassigned instead of the screenshot-confirmed blue team.
- [x] Added a narrow fallback for the eight screenshot-confirmed captain rooms, only when the snapshot team is null/missing. Explicit valid snapshot teams take precedence; malformed values remain invalid.
- [x] Added local parser tests for all eight IDs, missing/null variants, leader/member non-fallback, explicit reassignment, and malformed captain team rejection.
- [x] Added a synthetic 44-host controller fixture with eight captains and 36 members. Preview has four teams with 11 matches each; confirmation saves all captain ownership; repeat import changes nothing and opens no rooms.
- [x] Focused release16 rank/controller CTest passed 2/2 before final clean rebuild.
- [x] Clean rebuilt both configurations with the eight relevant test targets and `douyu_monitor_native`.
- [x] Release16 focused CTest passed 8/8 in 49.36 seconds.
- [x] Beta24 focused CTest passed 8/8 in 55.08 seconds.
- [x] Release16 fresh executable `douyu_monitor_native.exe --self-test` exited 0.
- [x] Beta24 fresh executable `douyu_monitor_native.exe --self-test` exited 0.
- [x] `git diff --check` passed; no installer, commit, tag, push, or release was created.
- [x] Record final local evidence below.

The captain fallback is static screenshot-confirmed business data. It does not establish an unknown
server-side captain assignment field or guarantee detection of future reassignments if the snapshot
continues returning null. Live verification remains blocked; do not run the opt-in external preview
to bypass the browser denial. No installer or remote publication is authorized.

### Final Local Evidence

Fresh executable paths:

- `native/out/build/windows-x64-release/douyu_monitor_native.exe` (16-room profile)
- `native/out/build/windows-x64-beta24/douyu_monitor_native.exe` (24-room beta profile)

Both files were rebuilt on 2026-10-04 and passed the renderer self-test. The focused CTest
selection passed in both configurations. These checks do not replace live multi-stream or
future server-side captain-field validation.

The release16 import interaction test was also rerun directly with the Windows backend
and exited 0 with no warnings. Fresh 640x720 and 1280x900 preview screenshots were inspected:
Chinese text, four-team long names, summaries, conflict text, and buttons fit without overlap.

### Confirmation Invalidation Regression

User reported that a displayed confirm button rejected the import as stale on 2026-10-04.
Inspection and local failing tests established two problems: full cache/entry equality included
display-only data, and resolver cache changes did not notify `maoziTeamImportChanged`.
An additional failing test covered first caching an already-bundled room identity.

The controller now compares sorted import identity/ownership fields and the deduplicated union
of bundled and verified cached room identities. Display-only updates and sorting do not invalidate
the preview; actual room, host, role, team, local-team, or format changes remain guarded.
Resolver cache changes now notify the preview property. Saving uses the latest workspace copy,
preserving updated avatar/live cache metadata instead of restoring stale preview metadata.

- [x] Red evidence: three controller cases failed at missing notification/display-data invalidation.
- [x] Red evidence: first caching a bundled room rejected confirmation.
- [x] Rebuilt both executable profiles and relevant tests after the scoped controller fix.
- [x] Release16 eight-target CTest passed 8/8 in 43.20 seconds.
- [x] Beta24 eight-target CTest passed 8/8 in 47.57 seconds.
- [x] Both fresh executable renderer self-tests exited 0.
- [x] Directly reran the four new confirmation regression cases plus existing stale/failure/cancel checks: 5 cases passed.
- [x] `git diff --check` passed; both executable timestamps reflect the rebuilt source.

All reproduction fixtures use localhost. No new external access, user workspace mutation,
installer, commit, push, or release is part of this follow-up.
