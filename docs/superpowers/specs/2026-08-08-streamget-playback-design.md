# StreamGet FLV Playback Design

## Decision

Add a guarded StreamGet resolver for Douyu's app-search path and play the returned
HTTP-FLV stream in the Electron renderer. The resolver will call
`DouyuLiveStream.fetch_app_stream_data()` only. It will not call
`_fetch_web_stream_url()`, generate `auth`, or reproduce the web signature flow.

The current app already has room search, danmaku sessions, layout state, and a
truthful blocked playback state. This change adds a real playback variant when
StreamGet returns a live FLV address and keeps the existing blocked state for
offline rooms, missing Python dependencies, unsupported responses, or invalid
stream hosts.

## Data Flow

1. The renderer requests `playback.getAvailability` through the preload allowlist.
2. The Electron main process starts a bounded one-shot Python sidecar for one room ID.
3. The sidecar imports `streamget.DouyuLiveStream`, calls the app-search method,
   and writes one JSON response per input line.
4. The main process validates the response, allows only Douyu CDN/edge hosts,
   and returns a single in-memory FLV variant.
5. The renderer creates an `mpegts.js` player, attaches it to a fixed-size video
   element, and destroys it when the room or URL changes.
6. Danmaku remains an independent overlay and keeps its existing lifecycle.

The URL is never logged, stored in workspace state beyond the active session,
written to fixtures, or included in error messages. The sidecar's stderr is
discarded from user-facing output.

## Boundaries

- Supported input: numeric Douyu room IDs already accepted by the app.
- Supported stream: one HTTP-FLV variant returned by the app-search path.
- Quality: the production resolver exposes the returned variant as `auto`; the
  existing quality menu remains disabled when only one real variant exists.
- Offline rooms map to `ROOM_OFFLINE`.
- Missing Python, missing `streamget`, malformed JSON, rejected HTTP responses,
  unsupported URL hosts, and sidecar crashes map to a retryable playback error.
- The web signature path and signed URL generation remain excluded.

## Implementation Units

- `src/main/streamget-bridge.ts`: bounded child-process bridge with project-local Python discovery.
- `scripts/streamget_bridge.py`: small sidecar using StreamGet app-search only.
- `src/infrastructure/streamget-douyu-adapter.ts`: maps sidecar results to the
  existing `DouyuAdapter` contract.
- `src/renderer/components/FlvVideo.tsx`: owns mpegts.js player lifecycle.
- `src/renderer/components/RoomPlaybackSurface.tsx`: selects demo, live video,
  checking, blocked, and error surfaces.
- `src/shared/ipc-contract.ts`, preload, and main handlers: typed allowlist.

## Verification

- Unit tests cover sidecar protocol parsing, host validation, adapter mapping,
  IPC forwarding, and video surface selection.
- Existing tests must remain green.
- A live smoke check uses an online room and reports only URL origin/path and
  player state. It must not print query values.
