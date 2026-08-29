# Native Qt Danmaku Migration Design

**Date:** 2026-08-27

**Status:** Approved design; implementation has not started.

## Goal

Migrate the legacy application's real Douyu danmaku capability to the Qt Quick/QML + C++ product. The native application will receive, reconnect, govern, display, configure, persist, and safely shut down danmaku for up to nine active rooms without Electron, Chromium, React, Node.js, Qt WebEngine, QWidget, or QOpenGLWidget.

## Scope

This milestone includes:

- The legacy Douyu binary frame protocol, WSS connection lifecycle, login/group join, heartbeat, endpoint rotation, bounded retry, and platform-block handling.
- A C++ session manager with one eligible connection per active room and a maximum of nine sessions.
- Message sanitization, de-duplication, bounded queues, keyword filtering, repeated-message suppression, peak-rate protection, and statistics.
- Global and per-room danmaku controls, display settings, global governance defaults, per-room governance overrides, and workspace-preset persistence.
- A QML scrolling overlay with collision-free lanes, display regions, density pacing, font choices, opacity, duration, and native/advanced rendering choices.
- Safe connection state presentation in room tiles and the danmaku settings panel.
- Deterministic tests, manual live validation, milestone logging, and a Notion progress page after completion.

This milestone does not add a new streaming source, change the libmpv playback pipeline, redesign group switching, or add search/layout/preset features outside the data fields required to preserve danmaku settings in existing presets.

## Reference Behavior

The implementation must reproduce the behavior represented by these legacy modules:

- `src/infrastructure/douyu-danmaku/protocol.ts`
- `src/infrastructure/douyu-danmaku/client.ts`
- `src/main/danmaku-session-manager.ts`
- `src/renderer/danmaku/danmaku-governance.ts`
- `src/renderer/danmaku/danmaku-settings.ts`
- `src/renderer/danmaku/danmaku-lane-scheduler.ts`
- `src/renderer/components/DanmakuOverlay.tsx`
- `src/renderer/components/DanmakuSettingsPanel.tsx`

The native UI reference points are `native/app/qml/components/RoomTile.qml` and `native/app/qml/panels/DanmakuSettingsPanel.qml`.

## Architecture

### Ownership

`AppController` remains the only application-level orchestrator. It owns a new `DanmakuController`, which owns a `DanmakuSessionManager`. The session manager owns zero to nine `DouyuDanmakuClient` instances. A client owns its transport and timers through QObject parent-child ownership, so its timers and socket are destroyed with the client.

`DanmakuController` is the QML boundary. It exposes only sanitized messages, bounded counters, display/governance settings, and safe status values. It never exposes a WSS endpoint, raw frame, cookie, authentication detail, stream URL, resolver response, or mpv diagnostic.

### Native Modules

Create `native/src/danmaku/` with the following responsibilities:

| Module | Responsibility |
| --- | --- |
| `douyu_danmaku_protocol.*` | Encode legacy client frames; incrementally decode server frames; reject repeated-length, protocol, terminator, UTF-8, and frame-size violations. |
| `danmaku_socket.*` | Testable socket abstraction and the production `QWebSocket` adapter. |
| `douyu_danmaku_client.*` | Per-room WSS lifecycle, login/group join, heartbeat, connection status, endpoint rotation, bounded retry, and authentication blocking. |
| `danmaku_governance.*` | Sanitization, message de-duplication, filter decisions, rate protection, queue-overflow accounting, and statistics. |
| `danmaku_session_manager.*` | Maximum-nine session ownership, eligible-room synchronization, per-room queues, and message/status forwarding. |
| `danmaku_controller.*` | AppController/QML-facing settings, per-room overrides, queued-message access, and safe presentation data. |

`native/CMakeLists.txt` will add the Qt WebSockets component and link it only to the danmaku transport target and its consumers. No new process, browser runtime, or WebEngine module is permitted.

### Transport and State Machine

Each client connects to the six legacy WSS proxy endpoints in a rotated order. On socket open it sends the legacy `loginreq` and `joingroup` frames, starts a 45 second heartbeat, and starts a 10 second handshake timer. A valid `loginres`, `setmsggroup`, or same-room `chatmsg` transitions the safe UI state to `connected`.

The safe public states are `idle`, `connecting`, `connected`, `reconnecting`, `failed`, and `platform-blocked`. The public error codes are limited to `NETWORK_UNAVAILABLE`, `HANDSHAKE_TIMEOUT`, `PROTOCOL_CHANGED`, `RETRY_EXHAUSTED`, and `AUTH_REQUIRED`.

Retry delays are 1, 2, 4, 8, 15, and 15 seconds with bounded jitter. The retry cycle ends as `failed` after the final retry. An explicit transport authentication request, policy-close authentication evidence, or matching authentication response text enters `platform-blocked` and does not retry automatically. Other WSS handshake failures use the bounded network retry path; the implementation must not parse an error string to infer an HTTP status. A manual retry restarts the cycle.

### Eligibility and Shutdown

`AppController` recomputes eligibility whenever room snapshots, active room IDs, per-room danmaku state, global danmaku state, or workspace restoration changes. A room is eligible only when all conditions hold:

1. It belongs to the current active workspace and the session manager has capacity.
2. Its library record has `danmakuEnabled == true`.
3. The global master setting is enabled.
4. Its resolved live status is `Online`.

Ineligible rooms stop their client and clear both pending and active display messages. `AppController::shutdown()` stops every danmaku session before it detaches players, stops StreamGet, and allows the window to close. Repeated stop or shutdown calls are harmless.

## Data and Persistence

### Runtime Data

The C++ boundary uses these conceptual data types:

- `DanmakuMessage`: ID, room ID, nickname, text, and UTC receipt time.
- `DanmakuConnectionStatus`: room ID, safe state, attempt number, and optional safe error code.
- `DanmakuDisplaySettings`: duration, font size, opacity, region, density, font family, and rendering mode.
- `DanmakuGovernanceSettings`: enabled, keyword blacklist, duplicate window seconds, and peak protection enabled.
- `DanmakuGovernanceStats`: level, recent rate, peak rate, filtered, duplicates, rate-limited, queue-overflow, and upstream-dropped counters.

Messages are normalized before queueing: control characters are removed, line breaks become spaces, text is limited to 200 Unicode code points, nickname to 40, and message ID to 200. The per-room recent-ID set holds at most 200 IDs. The display queue holds at most 100 messages; dropping old queued items increments `queueOverflow`.

The governance rules match the legacy defaults: a 3 second input window, 60 second statistics window, 1 second accepted window, `crowded` limit 20, `burst` limit 10, global keyword blacklist maximum 50 entries, and keyword maximum 40 Unicode code points. Per-room governance can override only governance fields; display settings remain global, matching the legacy product.

### Workspace Version 3

`NativeWorkspaceSnapshot::version` becomes 3. It gains a native danmaku configuration object containing:

- `globalEnabled`
- Global display settings
- Global governance settings
- A map of per-room governance overrides

`NativeWorkspacePreset` gains the same danmaku configuration so applying a preset restores its master state, display settings, governance defaults, and retained per-room overrides.

Migration behavior is explicit:

- Version 1 remains readable under existing normalization rules.
- Version 2 preserves each room's `danmakuEnabled` setting. The new current-workspace global master defaults to `false`, because version 2 had no real network danmaku client and must not create connections merely by upgrading.
- Version 2 preset `danmakuEnabled` maps to its version 3 global master setting.
- New clean workspaces default global danmaku to `true`; newly added rooms default `danmakuEnabled` to `true`, matching the legacy product's normal behavior.

Serialization permits only the declared setting fields. It does not persist raw chat, protocol frames, endpoints, cookies, tokens, signatures, playback URLs, request headers, raw StreamGet responses, or mpv diagnostics.

## QML Presentation

### Controller and Room Roles

`AppController` exposes the danmaku controller to QML. `RoomListModel` gains roles for safe danmaku connection state, safe error code, and governance statistics required by tile presentation. Existing per-room `danmakuEnabled` remains a presentation role.

`RoomTile.qml` keeps its danmaku action button. A connected tile shows the legacy-style green state indicator. A failed or blocked tile has a tooltip and retry action that use the safe status only. No transport detail is displayed.

### Overlay

Add `native/app/qml/components/DanmakuOverlay.qml` and a small helper for lane selection. The overlay is a non-interactive child above the mpv item and below the tile controls. It requests a sanitized queued message only at the selected density interval, measures text, selects a collision-free lane, and drives a linear x-position animation from the right edge to beyond the left edge.

The scheduler reproduces the legacy rules:

- Regions: full height, top half, or bottom half.
- Lane line height: `fontSize * 1.35`.
- Density lane ratios and launch intervals: massive `1.0 / 80 ms`, normal `0.7 / 180 ms`, reduced `0.4 / 360 ms`.
- A lane can be reused only when the previous line cannot collide with the candidate line during its remaining travel.

Changing the global master or a room toggle immediately clears active visual items and pending messages for that room. Settings changes apply to subsequent messages; the active item's scheduled duration and geometry remain stable until it exits.

The native rendering mode uses a readable dark outline. The advanced mode uses a stronger outline and shadow while avoiding extra effects modules or duplicated animation trees that would add avoidable cost across nine tiles.

### Settings Panel

Replace the current room-checkbox-only panel with the legacy-equivalent three tabs:

1. **Display:** duration, size, opacity, region, density, font, and rendering mode, with reset actions.
2. **Governance:** global or one-room scope, governance master, peak protection, duplicate window, blacklist editing, and clear room override.
3. **Statistics:** level, recent and peak rate, filtering, duplicate, rate-limit, queue-overflow counters, and scoped reset.

The panel retains the existing Qt Quick visual language, accessible names, keyboard behavior, and compact product layout.

## Verification Plan

All automated tests use a fake socket and controllable clock/timers. They never make a real Douyu request.

| Test area | Required coverage |
| --- | --- |
| Protocol | Login frame encoding, fragmented decoding, multiple frames, malformed lengths, unexpected protocol, missing terminator, invalid UTF-8, and 1 MiB rejection. |
| Client | Login/group join, heartbeat, handshake success/timeout, endpoint rotation, retry timing, protocol/network failures, explicit authentication block, manual retry, and idempotent stop. |
| Session manager | Nine-room maximum, eligibility synchronization, message sanitization, 200-ID de-duplication, 100-message queue, queue-drop stats, and lifecycle cleanup. |
| Governance | Blacklist, duplicates, high-traffic levels, rate limit, settings validation, room overrides, and statistics reset. |
| Persistence | Version 1/2 migration, version 3 round trip, preset round trip, value clamping, and no sensitive fields in serialized output. |
| AppController | Global/per-room/live/active eligibility transitions, tile state refresh, manual retry, room removal, and shutdown ordering. |
| QML | Three setting tabs, global and room toggles, status indicator, overlay launch, immediate clearing, and deterministic visual smoke coverage with fixed safe fixture messages. |

Final verification runs full Debug and Release CTest suites. Manual validation runs the packaged native application with one real live room, confirms connection, scrolling text, controls, and clean shutdown. A live nine-room network run is recorded as an environment-dependent manual observation only; deterministic tests prove the nine-session cap and cleanup behavior.

## Acceptance Criteria

- The native-only runtime receives and displays real Douyu chat for an eligible live room.
- The client reconnects safely and presents safe state/error information.
- A maximum of nine active danmaku sessions can exist.
- Display and governance settings match the legacy feature set and persist through restart and preset application.
- A per-room or global disable immediately clears displayed and queued messages and stops tracking that room.
- Offline, removed, inactive, and shutdown rooms do not retain a socket or timer.
- No sensitive transport, playback, authentication, or raw diagnostic data crosses into QML or persistence.
- New and existing automated tests pass in Debug and Release.
- After implementation, progress is compared against this design and the implementation plan, appended to the sanitized local log, and written to a newly created and fetched Notion child page before the next milestone starts.
