# Project Audit Fixes Implementation Plan

> **For agentic workers:** Use subagent-driven development for the independent tasks. Each behavior change starts with a regression test that reproduces the current defect. Do not commit or change unrelated dirty files.

**Goal:** Fix the confirmed security, state, persistence, notification, accessibility, and image fallback defects from the 2026-11-01 audit, and resolve or clearly diagnose the remaining network and test reliability risks.

**Architecture:** Preserve the existing Qt Quick/QML + C++20 client and the Python StreamGet subprocess. Keep business rules in their current C++ service/model boundaries. Harden external network input at the request/parser boundaries and preserve existing protocol and persistence compatibility.

**Tech Stack:** Qt 6.12, C++20, QML, Python 3.13, unittest, CMake, CTest.

---

## Task 1: Verify TLS for StreamGet requests

**Files:**
- Modify: `native/service/douyu_backend.py`
- Test: `native/service/tests/test_backend.py`
- Inspect: locked StreamGet implementation under `native/.venv/Lib/site-packages/streamget/`

- [x] Add a regression test proving production Douyu resolution constructs HTTPS requests with certificate verification enabled.
- [x] Reproduce the installed StreamGet request helper's `verify=False` behavior with an isolated test/probe.
- [x] Enable certificate verification through the narrowest maintainable production seam without changing the C++/Python protocol or exposing request diagnostics.
- [x] Run the Python service suite; final result: 62/62 passed.

## Task 2: Recover expired rank sessions and clear failed live status

**Files:**
- Modify: `native/src/app/maozi_rank_client.cpp`, `native/src/app/maozi_rank_client.h`
- Test: `native/tests/maozi_rank_client_test.cpp`
- Modify: `native/src/workspace/guild_room_resolver.cpp`
- Test: `native/tests/guild_room_resolver_test.cpp`

- [x] Add local-server coverage for a cached rank token receiving 401/403; verify one re-authentication and bounded version retry, with no retry loop on repeated authorization failure.
- [x] Reproduce repeated use of the expired token before implementing recovery.
- [x] Add resolver coverage for `SERVICE_FAILED` after cached `online`; require live state to become `unknown` while retaining verified identity metadata.
- [x] Reproduce the stale `online` status before implementing the state transition.
- [x] Implement bounded session recovery and clear failed live status; focused targets and final full CTest passed.

## Task 3: Preserve workspace, audio, and playback recovery state

**Files:**
- Modify: `native/src/workspace/native_workspace_store.cpp`
- Test: `native/tests/native_workspace_store_test.cpp`
- Modify: `native/src/ui/app_controller.cpp`
- Test: `native/tests/app_controller_test.cpp`
- Modify: `native/src/workspace/notification_policy.cpp`
- Test: `native/tests/notification_policy_test.cpp`

- [x] Add a persistence round-trip test for the supported `primary` layout and reproduce its normalization to `auto`.
- [x] Add an apply-preset test where an intentionally empty audio focus stays empty and all rooms remain muted.
- [x] Add a notification sequence test for `Error -> Pending -> Playing` and require one recovery notification.
- [x] Implement the state fixes and run the focused CTest targets; all passed in the final full suite.

## Task 4: Restore keyboard access and image fallbacks

**Files:**
- Modify: `native/app/qml/pages/MaoziRankPage.qml`
- Modify: `native/app/qml/components/GuildNavigationPanel.qml`
- Modify: `native/app/qml/components/RoomLibraryView.qml`
- Test: `native/tests/qml_interaction_test.cpp`

- [x] Add QML interaction assertions that rank and match-history entries are keyboard focusable and activate their existing detail actions with Enter/Space.
- [x] Add a navigation-avatar test proving the validated fallback URL is attempted after the primary image fails.
- [x] Add a room-library image failure test proving the existing text placeholder becomes visible on `Image.Error`.
- [x] Run focused and full QML interaction coverage. A match-history row regression exposed that focus alone did not activate `ConsoleButton`; explicit Return/Enter/Space handlers now open details, and the final full suite passed.

## Task 5: Bound external image and update responses

**Files:**
- Inspect and, where supported by existing tests, modify: `native/src/app/maozi_rank_client.cpp`, `native/src/app/update_checker.cpp`
- Test: `native/tests/maozi_rank_client_test.cpp`, relevant update-checker test

- [x] Add local HTTP fixture coverage for oversized rank and update responses, including responses without a trustworthy `Content-Length`.
- [x] Add poster URL cases for non-HTTPS, local/private destinations, and allowed production CDN hosts.
- [x] Enforce bounded reads and a narrow remote-image policy without rejecting validated production assets.
- [x] Run relevant client tests; final full CTest passed.

## Task 6: Bound WebSocket connection attempts

**Files:**
- Modify: `native/src/danmaku/douyu_danmaku_client.cpp`, `native/src/danmaku/douyu_danmaku_client.h`
- Test: `native/tests/douyu_danmaku_client_test.cpp`

- [x] Add fake-socket coverage proving the connection attempt expires when `connected` never fires and stop/retry/late events cancel or ignore the deadline.
- [x] Confirm the pre-fix fake-socket behavior had no deadline before `connected`.
- [x] Add an attempt-scoped connection deadline separate from the post-connect login handshake timeout.
- [x] Run Danmaku client and related session/lifecycle targets; final full CTest passed.

## Task 7: Diagnose the intermittent StreamGet process test

**Files:**
- Inspect: `native/tests/streamget_process_client_test.cpp`
- Modify only if justified: `native/CMakeLists.txt`, the focused test harness, or the failing assertion/test timing.

- [ ] Capture actionable QtTest failure details under CTest and identify the failing child-process boundary. Earlier failures had empty output; the intermittent root cause remains unresolved.
- [x] Reproduce the intermittent failure in isolated runs before changing behavior; both passing and failing outcomes were observed.
- [ ] Apply a root-cause fix while preserving process creation, cancellation, timeout, and stderr-drain assertions.
- [x] Run the focused target and full serial suite repeatedly. The latest two full runs both passed 39/39, but that does not establish the earlier intermittent failure's root cause is fixed.

## Task 8: Integrate, verify, and record

- [x] Build the affected targets and production app with the Windows x64 Release toolchain.
- [x] Run Python service tests (62/62), the QML interaction test, two full serial CTest runs (39/39 each), and the production self-test (exit 0).
- [x] Run `git diff --check` and inspect workspace status; preserve the pre-existing dirty tree and generated/user files.
- [x] Update `docs/ui-redesign/PROJECT_AUDIT_2026-11-01.md` with fixes, evidence, and remaining limits.
- [x] Append and read back implementation checkpoint 105 in the existing Notion project page.

## Implementation status (2026-11-01, Asia/Shanghai)

Confirmed audit findings in Tasks 1–6 were addressed in the current workspace and the final Release validation passed. The previously intermittent `streamget_process_client_test` passed in both latest full runs, but the earlier empty-output failures were not traced to a specific child-process boundary, so Task 7 remains partially open. Real Douyu long-duration playback/danmaku, network-blackhole behavior, device DPI/GPU coverage, real installer install/uninstall, and a full dependency CVE scan remain outside the verified scope.

The added match-history keyboard regression initially failed because the focused `ConsoleButton` did not activate from a Return key in the test window. `SeasonArchivePanel.qml` now explicitly maps Return, Enter, and Space to the existing detail-selection action; focused interaction, full QML interaction CTest, and the final full suite passed.
