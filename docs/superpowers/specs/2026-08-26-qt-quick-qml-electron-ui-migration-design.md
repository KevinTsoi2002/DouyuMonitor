# Qt Quick/QML Electron UI Migration Design

## Status

Approved on 2026-08-26. Qt Quick/QML with C++ is the sole maintained desktop
UI runtime. The legacy Electron implementation remains a visual and behavioral
reference only; it is not a supported fallback runtime.

## Goal

Replace the native application's QWidget shell with a full Qt Quick/QML UI.
The result preserves the Electron application's Chinese UI, layout, visual
hierarchy, feedback states, and interactions. Existing Qt/C++ workspace,
status, notification, StreamGet, and libmpv business logic remains the source
of truth.

## Constraints

- The shipped application must not use Electron, Chromium, React, Node.js, or
  Qt WebEngine.
- The main UI must not mix QWidget and Qt Quick surfaces.
- The product supports at most nine rooms.
- The Electron UI is the acceptance baseline for text, dark palette, compact
  spacing, 44-pixel title bar, left sidebar, controls, panels, dialogs, toasts,
  and keyboard flows.
- UI, tests, logs, and Notion pages must not persist playback URLs, cookies,
  tokens, signatures, raw StreamGet results, or raw mpv diagnostics.
- Playback retains the current RoomApi plus StreamGet app-search acquisition
  path.

## Preserved C++ Services

The migration retains these ownership and policy boundaries:

- MultiRoomCoordinator: room capacity, ordering, primary room, and quality.
- RoomSession: per-room lifecycle and media controls.
- RoomStatusScheduler: online and offline refresh scheduling.
- NativeWorkspaceStore: workspace persistence.
- NotificationPolicy and WindowsNotificationService: preferences and delivery.
- StreamgetProcessClient: bounded StreamGet child-process protocol.
- RemotePlaybackController: authorized media-source loading.

QML never owns or dereferences these business-layer objects directly.

## Target Architecture

The process starts with QGuiApplication and QQmlApplicationEngine, then loads
app/qml/Main.qml. A C++ AppController owns UI-facing models and converts QML
intent into coordinator and service calls.

    Main.qml
      -> AppController
         -> RoomListModel / WorkspaceModel / MonitoringModel
            -> MultiRoomCoordinator / RoomStatusScheduler / NativeWorkspaceStore
               -> RoomSession / RemotePlaybackController / StreamgetProcessClient
                  -> MpvQuickItem / libmpv render API

Proposed source layout:

    native/
      app/
        main.cpp
        qml/
          Main.qml
          components/
            AppHeader.qml
            RoomSidebar.qml
            RoomTile.qml
            WorkspaceGrid.qml
            WindowControls.qml
            ToastViewport.qml
          panels/
            DanmakuSettingsPanel.qml
            MonitoringStatusPanel.qml
            WorkspacePresetsPanel.qml
          dialogs/
            AddRoomDialog.qml
            GroupManagerDialog.qml
            NotificationSettingsDialog.qml
      src/
        ui/
          app_controller.*
          room_list_model.*
          workspace_model.*
          monitoring_model.*
          mpv_quick_item.*

The existing QWidget main window, sidebar, docks, dialogs, and player surface
are removed only after their QML replacements pass functional and visual
acceptance.

## Player Rendering

MpvQuickItem replaces PlayerSurface, which currently inherits QOpenGLWidget.
Each visible RoomTile hosts one MpvQuickItem and each item owns one libmpv
handle plus render context.

The item must expose playback state, a safe error label, first-frame status,
pause, mute, volume, stop, and retry. It updates only the affected Qt Quick
scene-graph item when libmpv requests a frame and reacts to geometry changes
without recreating any room model.

All libmpv callbacks are marshaled onto the Qt UI thread before they modify
QML-visible state. Teardown is ordered as follows:

    stop media work
    -> stop event dispatch
    -> clear render callbacks
    -> destroy render context
    -> terminate mpv
    -> release Qt Quick resources

Resolved stream URLs remain inside C++ media handling and never enter a
QML-visible model.

## QML State Contract

RoomListModel provides ordered sidebar rows: room ID, anchor name, title,
category, safe avatar URL, online state, primary flag, favorite/group metadata,
requested quality, effective quality, and playback state.

WorkspaceModel exposes sidebar visibility, active layout, primary room, audio
mode, global mute, global danmaku setting, presets, and the maximum room
count. It supplies the existing commands:

    addRoom(roomId)
    removeRoom(roomId)
    moveRoom(roomId, direction)
    setPrimaryRoom(roomId)
    setAudioRoom(roomId)
    setQuality(roomId, quality)
    setVolume(roomId, volume)
    toggleDanmaku(roomId)
    retryPlayback(roomId)
    refreshRoom(roomId)
    saveWorkspacePreset(name)

MonitoringModel provides safe summary counts and non-sensitive health,
recovery, and notification labels. All three models make incremental row and
property updates; a timer or one room event must not rebuild every tile.

## Electron-to-QML Mapping

| Electron reference | QML replacement | Preserved behavior |
| --- | --- | --- |
| AppHeader.tsx | AppHeader.qml | 44-pixel frameless title bar, drag region, global controls, window controls |
| RoomSidebar.tsx | RoomSidebar.qml | tabs, filters, search, quick add, groups, favorite/order/remove actions |
| WorkspaceGrid.tsx | WorkspaceGrid.qml | grid and primary-room layouts to nine rooms |
| RoomTile.tsx | RoomTile.qml | metadata, controls, menu, status, hover-hide behavior |
| RoomPlaybackSurface.tsx | MpvQuickItem plus QML overlay | playback presentation and retry states |
| React popovers | QML Popup | danmaku settings and workspace presets |
| MonitoringStatusPanel.tsx | QML Drawer | right-side monitoring and notification status |
| React dialogs | QML Dialog | add room, groups, and notification settings |
| ToastViewport.tsx | ToastViewport.qml | success, warning, and error feedback |

## Visual and Interaction Baseline

At 1280 by 720, Qt UI preserves the Electron arrangement:

- 44-pixel top bar.
- 268-pixel expanded left room sidebar.
- Central responsive grid.
- Dark background, raised panels, thin borders, orange emphasis, and matching
  Chinese labels.
- Live, offline, reconnecting, and error status pills.
- Compact icon controls with equivalent tooltips and disabled state.
- Right monitoring drawer and top-anchored settings and preset popups.
- Tile controls visible on input activity and hidden after inactivity when
  neither focus nor a menu owns the tile.

At 1280 by 720 and 1920 by 1080, labels, icon controls, panels, and dialog
content must not overlap or clip.

## Window and Shortcut Behavior

The frameless QQuickWindow provides a custom top bar. Its drag region moves
the native window; explicit controls minimize, toggle maximization, and close.
C++ owns Windows-native operations and exposes only safe signals and invokables.

Existing shortcuts retain their meanings: add room, toggle sidebar, open
workspace/monitoring/danmaku controls, and refresh the main room.

## State and Failure Ownership

QML presents only the following stable states:

    idle, loading, playing, paused, offline, reconnecting, error

The coordinator, room session, and playback controller own legal transitions,
retry policy, capacity validation, source authorization, and notifications.

## Migration Sequence

### M1: Qt Quick Foundation

- Add Qt Quick, QML, and Quick Controls dependencies.
- Add QGuiApplication, QQmlApplicationEngine, resources, frameless window, and
  theme tokens.
- Produce an empty visual shell.

### M2: C++ to QML State Bridge

- Add AppController, RoomListModel, WorkspaceModel, and MonitoringModel.
- Bind current coordinator, persistence, scheduler, and notifications.
- Test model updates and command-result mapping in C++.

### M3: Full Electron UI Parity

- Migrate title bar, sidebar, grid, overlays, menus, dialogs, drawers, popups,
  toasts, and shortcuts.
- Compare layouts against Electron at both required viewport sizes.

### M4: libmpv Qt Quick Renderer

- Implement MpvQuickItem and transfer one-room playback first.
- Validate one, four, and nine items through render, resize, and teardown.
- Remove the QOpenGLWidget player after the Qt Quick renderer passes.

### M5: Release Acceptance

- Run Debug and Release builds, CTest, native self-test, and package smoke.
- Exercise sidebar, dialogs, panels, layouts, and close flow.
- Compare screenshots with the Electron reference.
- Scan generated output and logs for sensitive playback material.

## Acceptance Criteria

- The package has no Electron, Chromium, React, Node.js, or Qt WebEngine
  dependency.
- No QWidget main window or QWidget playback surface remains in the shipped UI.
- All described Electron-visible controls and state feedback appear in QML with
  equivalent behavior.
- Add/remove/reorder/group/favorite, primary room, audio focus, quality,
  volume, danmaku, presets, monitoring, notifications, and keyboard flows
  work.
- A tenth room is rejected with fixed local feedback.
- One, four, and nine room layouts work during resize and layout changes.
- Closing releases all playback and service work without heap corruption, stale
  callbacks, or remaining native/service processes.
- Tests, builds, self-test, package smoke, screenshot checks, and sensitive
  output scans pass before release.

## Non-goals

- A maintained Electron fallback or hybrid UI.
- New providers or a replacement playback resolver.
- Logging live playback source material.
- Browser-based embedded content.

## Progress Tracking

After every phase, compare its evidence with this design and the approved
implementation plan. Record a local progress entry, create a Notion log page,
and read it back before marking the phase synchronized or beginning the next
phase.
