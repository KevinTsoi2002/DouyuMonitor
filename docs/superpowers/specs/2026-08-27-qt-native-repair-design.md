# Qt Native Repair Design

## Goal

Repair the Qt Quick/QML + C++ client against the approved Electron reference without restoring Electron or adding a browser runtime. The shipped client remains Qt Quick, QML, C++, libmpv, and the existing StreamGet sidecar, with a maximum of nine rooms.

## Scope

The repair covers the latest sidebar corrections and the eight reported production defects:

1. Place the sidebar collapse/expand control immediately before the product mark in the title bar.
2. Restore a history view in the sidebar.
3. Establish a reproducible ninth-player load path before changing capacity behavior.
4. Remove idle multi-player polling and unconditional renderer scheduling that inflate CPU use.
5. Align the title bar, sidebar modes, room tiles, and grid behavior with the Electron reference.
6. Replace textual icon placeholders with packaged Qt-only SVG assets.
7. Make `online`, `offline`, and `unknown` display consistently in every QML component.
8. Preserve and show room metadata from valid search responses without requiring unrelated optional fields.
9. Ship a Windows GUI executable with a frameless QML window, draggable title bar, double-click maximize, and no console window.

## Data And Sidebar Design

`NativeRoomRecord.lastOpenedAtMs` is the durable source of history. On a successful room add or re-add, `AppController` records the current UTC epoch milliseconds and persists the workspace. A read-only `RoomLibraryModel` projects the safe fields needed by QML: room ID, anchor name, title, category, viewer label, avatar URL, favorite flag, active flag, last-opened timestamp, and a display-safe live state. It contains no media source, headers, cookies, tokens, signatures, or resolver output.

The sidebar has three modes:

- `current`: active rooms and groups.
- `favorites`: favorite library records.
- `history`: up to 20 library records with nonzero `lastOpenedAtMs`, newest first.

Favorites and history may contain inactive records. Their add action delegates to the existing `AppController.addRoom(roomId)` path, which preserves the nine-room limit and refreshes status through the normal service path.

## Playback Design

The nine-room maximum stays in `MultiRoomCoordinator::kMaxRooms`. A nine-player regression uses fixed local media or the fake sidecar and observes only stable public state and safe error codes. It must prove that all nine `MpvQuickItem` instances reach initialized render contexts before any change claims a ninth-room fix.

`MpvQuickItem` replaces its 20 ms per-instance event timer with libmpv's wakeup callback. The callback schedules one queued GUI-thread event drain per item. The render callback remains the only continuous frame trigger; `MpvRenderer::render()` does not call `update()` unconditionally. This gives idle players no periodic CPU wakeup while allowing libmpv to request a frame.

## Window And UI Design

`douyu_monitor_native` is linked as a Windows GUI executable. `Main.qml` sets `Qt.FramelessWindowHint`; the title-bar background offers drag and double-click maximize only outside interactive controls. The existing controller window actions remain the sole minimization, maximization, and close API.

The title bar matches the reference ordering: sidebar control, product mark, product name. QML uses packaged SVG files for the product mark, menu/collapse, monitoring, workspace, speaker, favorite, group, movement, close, and window actions. The reference remains a behavioral and visual source only, not a shipped dependency.

## Metadata And Status Rules

`RoomListModel` emits `online`, `offline`, or `unknown`; every QML consumer uses those exact values. `RoomSession::applyMetadata()` accepts a response matching the room ID when the anchor name is present. Empty optional title, category, or viewer values do not discard a valid online state or anchor name; persisted metadata only replaces a nonempty field. Invalid room IDs and unsafe avatar URLs remain rejected.

## Verification

Each behavior starts with a focused failing test. Required evidence includes model/controller tests, QML interaction and visual tests at 1280x720 and 1920x1080, nine-player regression, Debug and Release CTest, release dependency validation, executable subsystem inspection, source/package prohibited-runtime scans, and a sensitive-output scan. Runtime logs may contain only fixed error codes and aggregate metrics.

## Non-Goals

This repair does not add Electron, Chromium, React, Node.js, Qt WebEngine, QWidget, QOpenGLWidget, raw Douyu responses, or stored playback credentials. It does not increase capacity beyond nine rooms.
