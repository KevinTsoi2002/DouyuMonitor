# Qt-only M3 Single-room Remote Playback Design

**Date:** 2026-08-24  
**Status:** Approved for spec review  
**Product direction:** Qt + libmpv is the only maintained runtime.

## Goal

Add the first safe single-room remote playback lifecycle on top of the M2
StreamGet service boundary. The feature resolves one room, validates one typed
stream variant, loads it into the existing libmpv surface, and deterministically
handles cancellation, stop/release, service errors, and stale responses.

M3 has an API/test entry only. It does not add room search, room-library UI,
multi-room scheduling, or a new quality policy.

## Non-goals

- No Electron, Node, React, TypeScript, or IPC compatibility layer.
- No live Douyu credentials or live network dependency in automated tests.
- No persistence, logging, telemetry, or crash artifact containing a playback
  URL, query string, Cookie, token, signature, or resolver traceback.
- No multi-room workspace, search/catalog, danmaku, or adaptive quality policy.
- No claim of deterministic remote first-frame timing without a controlled
  network fixture; the existing local PPM first-frame tests remain the render
  regression gate.

## Architecture

```text
StreamgetProcessClient
        |
        v
RemotePlaybackController ---- generation / cancellation / error mapping
        |
        v
MediaSource::RemoteStream ---- validated URL held in memory
        |
        v
PlayerSurface::loadSource()
```

`RemotePlaybackController` receives a non-owning `StreamgetProcessClient`,
starts one resolve request at a time for the current room generation, and emits
typed source/error events. It never writes URLs to logs or files. The caller
decides when to pass the emitted source to `PlayerSurface`.

## Typed media source

Extend `MediaSource` with a `RemoteStream` kind while preserving the existing
`LocalFile` behavior:

```cpp
class MediaSource final {
public:
    enum class Kind { LocalFile, RemoteStream };

    static std::optional<MediaSource> fromDescriptor(const QString &descriptor);
    static std::optional<MediaSource> fromRemoteVariant(
        const QString &roomId, const StreamVariant &variant);

    Kind kind() const noexcept;
    QString localPath() const;
    QUrl remoteUrl() const;
    QString roomId() const;
    QString variantId() const;
    StreamQuality quality() const noexcept;
    QString container() const;
    QString stableDescription() const;
};
```

`fromRemoteVariant()` is the only production construction path for a remote
source. It repeats the M2 HTTP(S), credential rejection, CDN suffix, room ID,
and non-empty variant checks before storing the URL. `stableDescription()` is a
fixed value such as `remote-stream`; it never contains the URL or metadata
query.

## Remote playback controller

Create `native/src/media/remote_playback_controller.h/.cpp` with this API:

```cpp
class RemotePlaybackController final : public QObject {
    Q_OBJECT

public:
    enum class State { Idle, Resolving, Ready, Error };

    explicit RemotePlaybackController(StreamgetProcessClient *client,
                                      QObject *parent = nullptr);

    quint64 resolve(const QString &roomId, StreamQuality quality);
    void cancel();
    void stop();
    void release();

    State state() const noexcept;
    quint64 generation() const noexcept;

signals:
    void sourceReady(MediaSource source);
    void failed(QString errorCode);
    void stateChanged(State state);
};
```

Behavior:

- `resolve()` increments generation, cancels the prior request if present, and
  enters `Resolving`.
- A response is accepted only when both its request ID and generation match the
  current operation, it is successful/live, and it contains at least one
  validated variant. The first service-provided variant is used; M3 does not
  invent or reorder quality variants.
- `ROOM_OFFLINE`, `TIMEOUT`, `SERVICE_FAILED`, and other fixed service errors
  emit `failed(code)` and enter `Error` without exposing diagnostics.
- `cancel()`, `stop()`, and `release()` increment generation, cancel the active
  request, and prevent late responses from emitting `sourceReady`.
- `release()` returns the controller to `Idle`; it does not own or persist a
  `MediaSource` after release.

## PlayerSurface integration

Add one typed entry point:

```cpp
bool PlayerSurface::loadSource(const MediaSource &source);
```

`loadLocalMedia(path)` remains as a compatibility wrapper that constructs a
local source and delegates to `loadSource()`. `loadSource()` dispatches local
paths and validated remote URLs to the existing libmpv `loadfile` command. The
URL is converted to UTF-8 only for the in-memory libmpv call. Existing playback
states and stop/release cleanup remain unchanged.

M3 does not add visible room controls. Tests and future UI code can connect
`sourceReady` to `PlayerSurface::loadSource()` without changing the current
MainWindow layout.

## Error and security boundaries

- Protocol decoding remains responsible for JSON shape, request correlation,
  fixed error codes, and playback URL allowlisting.
- `MediaSource::fromRemoteVariant()` is a second validation boundary against a
  compromised or malformed child response.
- Controller errors contain only fixed codes. No exception text, URL, headers,
  cookies, signature, or traceback crosses the boundary.
- PlayerSurface exposes only stable state and fixed user-safe errors; raw mpv
  diagnostics are not persisted.

## Deterministic tests

Create `native/tests/remote_playback_controller_test.cpp` and extend the media
source/player tests:

- Remote source factory accepts allowed CDN HTTP(S) variants and rejects bad
  schemes, credentials, unsafe hosts, malformed room IDs, empty IDs, and empty
  metadata. Stable descriptions contain no URL/query/credential text.
- Controller with the existing fake child covers resolve success, offline/error
  mapping, cancellation, timeout, late response suppression, stale generation,
  and one clean service restart. Tests assert typed `sourceReady` fields rather
  than raw JSON or URL logs.
- `PlayerSurface::loadSource()` preserves all local lifecycle tests and rejects
  invalid source objects without corrupting `Idle`/`Error`/`Ended` state.
- Automated tests remain offline and credential-free. A controlled live smoke
  test for remote first frame is a later acceptance activity, not an M3 unit
  test dependency.

## Acceptance

M3 is complete when:

1. A single room can resolve into a validated in-memory `MediaSource::RemoteStream`.
2. The caller can load that source through `PlayerSurface::loadSource()`.
3. Cancel, stop, release, timeout, service failure, and stale responses are
   deterministic and covered by tests.
4. Existing local media build, CTest, and self-test behavior remains green.
5. No URL or credential material appears in captured logs, stderr, files, or
   crash artifacts.
