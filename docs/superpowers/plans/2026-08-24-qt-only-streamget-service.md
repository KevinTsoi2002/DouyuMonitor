# Qt-only StreamGet Service Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the Qt-only remote-source foundation: a long-lived Python StreamGet service, a typed Qt JSONL client, bounded request handling, and deterministic protocol/package tests.

**Architecture:** The Qt executable is the sole product host. It starts one `streamget_service.exe` child through `QProcess`; the child owns Douyu discovery and StreamGet resolution. Qt validates every response, correlates request IDs, cancels stale work, and never persists playback URLs. M2 stops before loading a remote URL into `PlayerSurface`; that integration is M3.

**Tech Stack:** C++20, Qt 6.8 (Core/Test), CMake/Ninja/MSVC, Python 3.11+, `streamget==4.0.10`, PyInstaller 6.22, Python `unittest`, and CTest.

---

## Scope and file map

Create:

- `native/service/protocol.py`: JSONL validation and fixed error builders.
- `native/service/douyu_backend.py`: room search/metadata and StreamGet resolution.
- `native/service/streamget_service.py`: long-lived JSONL loop, two-worker limit, cancellation, and shutdown.
- `native/service/tests/test_protocol.py`, `test_backend.py`, `test_service.py`.
- `native/service/requirements.txt`, `requirements-build.txt`.
- `native/scripts/bootstrap-streamget-service.ps1`, `build-streamget-service.ps1`.
- `native/src/service/stream_service_protocol.h/.cpp`.
- `native/src/service/streamget_process_client.h/.cpp`.
- `native/tests/stream_service_protocol_test.cpp`.
- `native/tests/streamget_process_client_test.cpp`.
- `native/tests/fake_streamget_service.cpp`.

Modify only:

- `native/CMakeLists.txt`.
- `native/README.md`.
- `docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md` during final evidence recording.

Do not modify Electron, Node, React, or legacy TypeScript files.

### Task 1: Freeze the Python protocol

**Files:** `native/service/protocol.py`, `native/service/tests/test_protocol.py`

- [ ] Write failing `unittest` cases for `ping`, `resolve`, `search`, `cancel`, and `shutdown`; invalid room IDs (`^[0-9]{1,20}$`), unknown qualities, unknown operations, missing/non-integer `requestId`, non-object JSON, success/offline responses, and fixed errors.
- [ ] Run `native\.venv\Scripts\python.exe -m unittest native.service.tests.test_protocol -v`; expect failure because the module does not exist.
- [ ] Implement `ROOM_ID_RE`, `QUALITY_VALUES`, `ErrorCode` (`INVALID_INPUT`, `ROOM_OFFLINE`, `STREAMGET_UNAVAILABLE`, `UNSAFE_STREAM_URL`, `TIMEOUT`, `INVALID_RESPONSE`, `SERVICE_FAILED`), `parse_request`, `success_resolve`, and `error_response`. Expose only `code` and `retryable` for errors; never include exception text, URLs, headers, or credentials.
- [ ] Rerun the focused test; expect PASS.
- [ ] Commit with `git add native/service/protocol.py native/service/tests/test_protocol.py` and `git commit -m "feat: define Qt StreamGet service protocol"`.

### Task 2: Implement the Python backend and service loop

**Files:** `native/service/douyu_backend.py`, `native/service/streamget_service.py`, `native/service/tests/test_backend.py`, `native/service/tests/test_service.py`

- [ ] Write failing backend tests with injected HTTP and StreamGet fakes. Cover numeric room lookup at `https://open.douyucdn.cn/api/RoomApi/room/<roomId>`, text search at `https://www.douyu.com/japi/search/api/searchShow?kw=<query>&page=1&pageSize=20`, field mapping, duplicate room IDs, offline responses, and viewer labels.
- [ ] Add resolver tests for `DouyuLiveStream.fetch_app_stream_data`, HTTP(S) CDN suffix allowlisting (`.douyucdn.cn`, `.douyucdn2.cn`, `.edgesrv.com`), malformed JSON, non-2xx responses, missing fields, unsafe hosts, and resolver exceptions.
- [ ] Run `native\.venv\Scripts\python.exe -m unittest native.service.tests.test_backend -v`; expect failure because the backend module does not exist.
- [ ] Implement `DouyuBackend.search(query)` with `urllib.request`, `Accept: application/json`, and a 10-second timeout. Implement `async resolve(room_id, quality)` with lazy `streamget.DouyuLiveStream` import. Return one initial `auto` FLV variant from the app-search result; do not invent quality variants.
- [ ] Write failing service tests for prompt `ping`, two in-flight operations with a third queued, cancellation by target request ID, fixed error mapping, malformed input recovery, shutdown, and EOF.
- [ ] Implement `run_service` with `asyncio`, `Semaphore(2)`, task map, output lock, `asyncio.to_thread(sys.stdin.readline)`, compact JSONL output, and a `__main__` entry point. `cancel` must prevent a second response for the cancelled task. No diagnostics may be written to stdout.
- [ ] Run `native\.venv\Scripts\python.exe -m unittest discover -s native/service/tests -v`; expect all Python tests to pass without real network access.
- [ ] Commit with `git add native/service` and `git commit -m "feat: add long-lived StreamGet service"`.

### Task 3: Add the Qt protocol value types and codec

**Files:** `native/src/service/stream_service_protocol.h/.cpp`, `native/tests/stream_service_protocol_test.cpp`

- [ ] Write failing QtTest cases for JSON serialization of all five operations; success/offline/error parsing; request ID and room ID preservation; malformed JSON; unknown quality; unsafe schemes/hosts; duplicate variant IDs; and arbitrary error-message rejection.
- [ ] Run `cmake --build native/out/build/windows-x64 --target stream_service_protocol_test --parallel 4` and `ctest --test-dir native/out/build/windows-x64 --output-on-failure -R stream_service_protocol_test`; expect failure because the target does not exist.
- [ ] Define `StreamQuality`, `StreamVariant`, `ServiceRequest`, and `ServiceResponse` in the header. Implement `encodeRequest`, `decodeRequest`, and `decodeResponse` with `QJsonDocument`, `QUrl`, the same room/quality rules, and the same CDN suffix allowlist. Return `std::nullopt` for malformed data; do not expose generic JSON objects.
- [ ] Rerun the focused commands; expect PASS.
- [ ] Commit with `git add native/src/service native/tests/stream_service_protocol_test.cpp` and `git commit -m "feat: add Qt StreamGet protocol codec"`.

### Task 4: Add the Qt `QProcess` client and fake child

**Files:** `native/src/service/streamget_process_client.h/.cpp`, `native/tests/fake_streamget_service.cpp`, `native/tests/streamget_process_client_test.cpp`

- [ ] Write failing tests using `QProcess` and the fake child for lazy start/ping, request correlation, two in-flight plus one queued request, timeout/cancel with late response ignored, malformed output, child crash with pending failure, one restart, bounded shutdown, and exactly one child process.
- [ ] Run `cmake --build native/out/build/windows-x64 --target streamget_process_client_test --parallel 4` and `ctest --test-dir native/out/build/windows-x64 --output-on-failure -R streamget_process_client_test`; expect failure because the client and target do not exist.
- [ ] Implement `StreamgetProcessClient` with one `QProcess`, a FIFO queue, two in-flight slots, line buffering, per-request `QTimer` deadlines, request IDs, callbacks/signals on the owning Qt thread, `cancel`, and bounded `shutdown`. A crash or protocol violation must fail all pending work and allow one clean restart.
- [ ] Implement the fake child with `--delay-ms N`, `--malformed`, `--crash-after N`, and `--ignore-cancel`; it must emit only fixed test data and never contact the network.
- [ ] Rerun the focused client test; expect PASS and no orphan fake child.
- [ ] Commit with `git add native/src/service/streamget_process_client.* native/tests/fake_streamget_service.cpp native/tests/streamget_process_client_test.cpp` and `git commit -m "feat: add bounded Qt StreamGet process client"`.

### Task 5: Add native service bootstrap, packaging, and CMake integration

**Files:** `native/service/requirements.txt`, `requirements-build.txt`, `native/scripts/bootstrap-streamget-service.ps1`, `build-streamget-service.ps1`, `native/CMakeLists.txt`, `native/README.md`

- [ ] Add exactly `streamget==4.0.10` to `native/service/requirements.txt` and `pyinstaller==6.22.0` to `native/service/requirements-build.txt`.
- [ ] Implement `bootstrap-streamget-service.ps1` to create `native\.venv`, install both requirement files, and stop on a non-zero pip exit code.
- [ ] Implement `build-streamget-service.ps1` with `python -m PyInstaller --noconfirm --clean --onefile --name streamget_service --distpath native\out\service --workpath native\out\pyinstaller native\service\streamget_service.py`; verify the exe exists and print only its path.
- [ ] Update `native/CMakeLists.txt` with a `stream_service` static library, the two Qt tests, the fake child dependency, optional Python interpreter discovery under `DOUYU_BUILD_SERVICE_TESTS=ON`, and a `streamget_service_python_tests` CTest entry. Register that test with `-s ${CMAKE_CURRENT_SOURCE_DIR}/service/tests` and `WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}` so it does not depend on the build directory. Preserve all six M1 tests unchanged.
- [ ] Update `native/README.md` with Qt-only bootstrap/build/configure/test/package commands; state that one service child is used per Qt app, URLs remain memory-only, and the package has no Node/Electron dependency.
- [ ] Run `native\.venv\Scripts\python.exe -m PyInstaller ...` through the build script and `Test-Path native\out\service\streamget_service.exe`; expect `True`.
- [ ] Commit with `git add native/CMakeLists.txt native/README.md native/service/requirements.txt native/service/requirements-build.txt native/scripts` and `git commit -m "build: package Qt StreamGet service"`.

### Task 6: Run M2 acceptance and record evidence

**Files:** verify only; update `docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md` after green checks.

- [ ] From an MSVC x64 prompt run `cmake --preset windows-x64` and `cmake --build --preset windows-x64-debug --parallel 4`; expect all native targets to link.
- [ ] Run `native\.venv\Scripts\python.exe -m unittest discover -s native/service/tests -v` and `ctest --preset windows-x64-debug --output-on-failure`; expect all Python tests and all M1/M2 CTest entries to pass.
- [ ] Rerun `ctest --test-dir native/out/build/windows-x64 --output-on-failure -R "stream_service_protocol_test|streamget_process_client_test|streamget_service_python_tests"`; expect all three focus entries green.
- [ ] Inspect captured output for `playbackUrl`, query strings, Cookie, token, signature, or raw Python traceback. The protocol may carry a URL in memory, but logs/files/stderr/crash artifacts must not contain it.
- [ ] Run `git diff --check`, `git status --short`, and `Get-Process | Where-Object { $_.ProcessName -match 'streamget_service|fake_streamget_service|douyu_monitor_native|streamget_process_client_test' }`; expect only the known `.gitignore` line-ending warning, no whitespace errors, and no matching processes.
- [ ] Append measured build, Python, CTest, package, output-scan, and process-cleanup evidence to the local progress log. Create and reread a new Notion M2 page. Mark M3 next only if every check is green.
- [ ] Commit only scoped verification fixes. Do not alter generated build output, legacy Electron files, or unrelated user changes.

## Plan self-review

- **Spec coverage:** Includes the approved Qt-only topology, JSONL contract, bounded concurrency, cancellation, stale-response handling, Python service, Qt client, fake-process tests, packaging, no Electron runtime, and M2 acceptance evidence.
- **Placeholder scan:** No `TBD`, `TODO`, or unspecified implementation step remains; every task names files, commands, and expected outcomes.
- **Type consistency:** Python and Qt use matching `requestId`, `roomId`, `quality`, `variants`, and fixed error-code fields.
- **Scope check:** Remote `PlayerSurface` loading, room UI, multi-room policy, danmaku, persistence, and final product packaging remain M3-M6; M2 is independently testable without a live room.
