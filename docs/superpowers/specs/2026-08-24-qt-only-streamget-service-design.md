# Qt-only StreamGet Service Design

**Date:** 2026-08-24  
**Status:** Approved design, pending implementation plan  
**Product direction:** Qt + libmpv is the only maintained product runtime.

## Decision

The native Qt application is the sole product host. Remote room discovery and
StreamGet resolution stay behind one long-lived Python service process because
the resolver dependency is Python-based. The Qt application communicates with
that service through a private stdin/stdout JSONL protocol.

No Electron runtime, Electron IPC, TypeScript adapter, or compatibility layer is
part of the maintained product path.

## Goals

- Provide a typed Qt boundary for room discovery, stream resolution, cancel,
  health, and shutdown operations.
- Reuse the existing StreamGet app-search path without moving signatures,
  cookies, or resolver internals into Qt.
- Keep one service process per Qt application with bounded request concurrency.
- Deliver validated HTTP(S) playback variants to the GUI-thread-owned libmpv
  surface without persisting or logging sensitive source data.
- Make failures, cancellation, restart, and stale responses deterministic and
  testable without real network access.
- Produce a standalone Windows package containing Qt, libmpv, and the service
  executable, with no Node or Electron runtime.

## Non-goals

- Maintaining the legacy Electron application or its IPC contracts.
- Reimplementing Douyu signing, cookie handling, or StreamGet internals in C++.
- Supporting arbitrary remote hosts, user-supplied renderer URLs, or raw URL
  logging.
- Adding multi-room behavior before the single-room remote lifecycle is stable.
- Claiming hardware-decoder or software-fallback performance without measured
  evidence.

## Architecture

### Qt application

The Qt process owns the product lifecycle, UI, workspace state, and libmpv
rendering. Its infrastructure layer contains:

- `StreamResolver`: typed asynchronous API for `resolve`, `cancel`, and
  `shutdown`.
- `StreamgetProcessClient`: one `QProcess`, JSONL framing, request correlation,
  timeout handling, restart, and stderr suppression.
- `RoomCatalog`: typed room search and metadata operations backed by the same
  service boundary.
- `MediaSource`: validated local or resolved remote source descriptor. The
  `PlayerSurface` receives only validated descriptors and remains owned by the
  GUI thread.

### Python service

`streamget_service.exe` is a long-lived process built from the existing
`streamget` dependency. It owns network discovery, room metadata lookup, and
app-search stream resolution. It emits protocol JSON only on stdout and
converts all implementation exceptions to fixed error codes.

A resolver URL may appear only in the in-memory stdout protocol payload needed
by Qt to start playback. It must never be copied to stderr, logs, files,
telemetry, or crash artifacts. Cookies, signatures, and tracebacks never enter
any protocol payload.

### Process topology

```text
Qt UI / workspace
        |
        v
Qt StreamResolver + request queue
        |
        | stdin/stdout JSONL (private child process)
        v
streamget_service.exe
        |
        v
StreamGet app-search path / Douyu network
```

The service is started lazily before the first remote operation and shut down
gracefully during application exit. A crash or protocol violation invalidates
all pending requests, reports a retryable domain error, and permits one clean
restart on the next operation.

## Protocol

The protocol is Qt-specific and intentionally small. Every request and response
is one UTF-8 JSON object followed by `\n`.

### Requests

```json
{"requestId":1,"op":"ping"}
{"requestId":2,"op":"resolve","roomId":"63136","quality":"auto"}
{"requestId":3,"op":"search","query":"63136"}
{"requestId":4,"op":"cancel","targetRequestId":2}
{"requestId":5,"op":"shutdown"}
```

`roomId` must match `^[0-9]{1,20}$`. `quality` is one of `auto`, `original`,
`super`, `high`, or `standard`; the service may return the nearest supported
variant but must preserve the requested room identity.

### Responses

Every response echoes `requestId`.

```json
{"requestId":2,"ok":true,"roomId":"63136","isLive":true,"variants":[{"id":"flv-auto","quality":"auto","label":"StreamGet FLV","container":"flv","playbackUrl":"https://..."}]}
{"requestId":2,"ok":true,"roomId":"63136","isLive":false,"variants":[]}
{"requestId":2,"ok":false,"error":{"code":"TIMEOUT","retryable":true}}
```

Allowed error codes are `INVALID_INPUT`, `ROOM_OFFLINE`,
`STREAMGET_UNAVAILABLE`, `UNSAFE_STREAM_URL`, `TIMEOUT`, `INVALID_RESPONSE`,
and `SERVICE_FAILED`. Error messages are fixed user-safe strings; diagnostics
are never returned in protocol payloads.

The service validates HTTP(S) URLs against the Douyu CDN allowlist. Qt repeats
the validation before creating a `MediaSource`, so a compromised or malformed
service response cannot inject an arbitrary renderer URL.

## Lifecycle and concurrency

- Qt maintains one monotonically increasing `requestId` sequence per service
  instance.
- The request queue has an initial maximum of two in-flight resolve/search
  operations. Additional work waits in Qt rather than spawning more services.
- Each operation has a 20-second deadline. Timeout sends `cancel`; if the
  service cannot acknowledge cancellation, Qt ignores the late response.
- Removing a room, switching source, or stopping playback invalidates the
  associated request generation before issuing a new request.
- A response is applied only when both `requestId` and room generation still
  match. Stale responses cannot overwrite newer availability or error state.
- `shutdown` is best-effort and bounded. Process termination is a final cleanup
  path after the normal shutdown deadline.

## Feature phases

### M2: Service and protocol foundation

Implement the long-lived service, JSONL schema, Qt process client, bounded
queue, restart behavior, and protocol tests using fake service fixtures.

### M3: Single-room remote playback

Add remote `MediaSource` creation, resolve-to-load flow, first-frame detection,
stop/release, cancellation, and stale-response tests. Keep the existing local
fixture path as a regression test.

### M4: Room catalog and single-window workflow

Add search, room metadata, online/offline state, retry actions, and safe user
messages through Qt-only services.

### M5: Multi-room workspace and quality policy

Add four-room stability first, then eight-room bounded scheduling, effective
quality transitions, per-room cancellation, and sustained playback probes.

### M6: Product completion

Add danmaku, workspace persistence, notifications, Windows packaging, update
workflow, and final performance acceptance for the Qt-only product.

## Testing and acceptance

### Automated tests

- Python service tests cover schema validation, malformed input, URL allowlist,
  error sanitization, cancellation, and concurrent request limits.
- Qt tests use a fake service process to cover startup, framing, correlation,
  timeout, cancel, crash/restart, and shutdown.
- Player tests cover remote first frame, source replacement, stop/release,
  stale response rejection, and recovery after service failure.
- All tests run without real Douyu credentials or network access.

### Performance evidence

Record single-request P50/P95 resolution latency, four- and eight-room first
frame latency, CPU peak, working set, queue depth, child-process count, and
cancellation time. The service count must remain one per Qt application.

### Release gate

The packaged executable must launch with only Qt, libmpv, and the bundled
`streamget_service.exe` dependencies. No Node or Electron runtime may be
required. A controlled live smoke test must complete one resolve, first frame,
stop, and release without persisting or printing a playback URL or credential.

## Review gate

Implementation may begin only after the M2 plan is written from this design.
The first implementation slice must remain Qt-only, use fake service fixtures
for deterministic tests, and preserve the existing local-media lifecycle
tests.
