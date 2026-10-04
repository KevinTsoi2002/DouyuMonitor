# QML Visual Validation

## Release build

The release executable was built from `native` with:

```powershell
cmake --build --preset windows-x64-release --target douyu_monitor_native -- -j1
```

The command completed successfully and produced `douyu_monitor_native.exe`.

## Native QQuickWindow captures

`qml_visual_smoke_test` captures the released QML module through `QQuickWindow::grabWindow()`.
The following images were generated under `out/verification/qml`:

| Image | Viewport |
| --- | --- |
| `empty-shell-1280x720.png` | 1280 x 720 |
| `one-room-1280x720.png` | 1280 x 720 |
| `three-rooms-1600x900.png` | 1600 x 900 |
| `five-rooms-1600x900.png` | 1600 x 900 |
| `nine-rooms-1920x1080.png` | 1920 x 1080 |
| `danmaku-panel-1600x900.png` | 1600 x 900 |
| `sound-panel-1600x900.png` | 1600 x 900 |

The visual test asserts that each canvas has non-black pixels, the layout surface and every room card remain inside the QQuickWindow viewport, and each room title does not intersect its action bar. The panel captures assert the panel rectangle remains inside the viewport and renders non-black content.

## Regression suites

The following selected suite was run after the release build:

```powershell
ctest --preset windows-x64-release -R '^(qml_engine_smoke_test|qml_visual_smoke_test|qml_interaction_test|qml_close_regression_test|multi_room_coordinator_test|mpv_quick_item_test)$' --output-on-failure
```

Five tests passed under the configured CTest environment:

- `qml_engine_smoke_test`
- `qml_visual_smoke_test`
- `qml_interaction_test`
- `multi_room_coordinator_test`
- `mpv_quick_item_test`

`qml_close_regression_test` runs under `QT_QPA_PLATFORM=windows` with
`DOUYU_PERF_ROOM_COUNT=16`. It verifies that 16 active rooms create 16 decoded
players, that all 16 render contexts become ready, and that closing the window
releases every attached player. The `windows-x64-beta24` build raises the same
room and decode capacity to 24 and is validated with
`DOUYU_PERF_ROOM_COUNT=24`.

## Packaged module metadata

The generated `out/build/windows-x64-release/DouyuMonitor/qmldir` contains one `Theme` singleton registration:

```text
singleton Theme 1.0 app/qml/Theme.qml
```

The file uses Windows CRLF line endings, so the line-ending-tolerant check is:

```powershell
rg -n '^singleton Theme 1\.0 app/qml/Theme\.qml\r?$' out/build/windows-x64-release/DouyuMonitor/qmldir
```

No browser mockup was used for this validation.

## Deterministic visual regression suite

`qml_visual_regression_test` adds deterministic pixel baselines on top of the
existing structural smoke checks. Structural assertions remain the primary
guard for containment, overlap, minimum target sizes, and layout eligibility.
Pixel comparison catches unintended changes in spacing, colors, clipping, and
visual state that are difficult to express as geometry assertions.

Build the target before updating or running the suite:

```powershell
cmake --build --preset windows-x64-release --target qml_visual_regression_test -j1
ctest --preset windows-x64-release -R '^qml_visual_regression_test$' --output-on-failure -V
```

The current state matrix is:

| Baseline | State |
| --- | --- |
| `layout-auto-1-1280x720.png` | One room, automatic layout |
| `layout-auto-4-1280x720.png` | Four rooms, automatic layout |
| `layout-auto-9-1920x1080.png` | Nine rooms, automatic layout |
| `layout-auto-12-1920x1080.png` | Twelve rooms, automatic layout |
| `layout-auto-16-1920x1080.png` | Sixteen rooms, automatic layout |
| `layout-auto-24-1920x1080.png` | Twenty-four rooms, automatic layout |
| `layout-primary-9-1920x1080.png` | Nine rooms, primary-room layout |
| `layout-primary-12-1920x1080.png` | Twelve rooms, primary-room layout |
| `layout-primary-16-1920x1080.png` | Sixteen rooms, primary-room layout |
| `layout-primary-24-1920x1080.png` | Twenty-four rooms, primary-room layout |
| `layout-primary-two-4-1280x720.png` | Four rooms, dual-primary layout |
| `layout-primary-two-10-1920x1080.png` | Ten rooms, dual-primary layout |
| `layout-primary-two-16-1920x1080.png` | Sixteen rooms, dual-primary layout |
| `layout-primary-two-24-1920x1080.png` | Twenty-four rooms, dual-primary layout |
| `header-dual-disabled-960x260.png` | Dual-primary option disabled below four rooms |
| `header-dual-enabled-960x260.png` | Dual-primary option enabled with four rooms |
| `room-quality-popup-640x420.png` | Quality selector popup open |
| `room-menu-640x420.png` | Room action menu open |
| `guild-navigation-284x720.png` | Guild navigation panel populated |
| `team-manager-720x760.png` | Team manager dialog open |

Pixel comparison converts both images to `Format_ARGB32`. A pixel counts as
different when any color channel differs by more than `16`. The test fails when
more than `1%` of the image pixels differ. Failed captures and red diff masks
are written under each build profile's `verification/qml-regression` directory. The comparison helper also
accepts ignored rectangles for known dynamic regions, but the current state
matrix does not use that mechanism because the fixtures are deterministic.

Baselines are inputs to the test, not automatically accepted output. A missing
baseline fails the test. After a visual change has been inspected and approved,
update baselines explicitly:

```powershell
.\scripts\update-visual-baselines.ps1 -Approve
```

The script sets `DOUYU_UPDATE_VISUAL_BASELINES=1`, reruns the test, and writes
the current captures to `tests/visual/baselines`. Review every changed PNG
before committing it. The script builds the visual test target before updating
the baselines and stops if that build fails.

The suite runs with `QT_QPA_PLATFORM=offscreen` and the Basic Qt Quick Controls
style for repeatable automation. This is not a replacement for real Windows
window acceptance: DPI scaling, GPU rendering, native popup placement,
multi-monitor behavior, and long-running playback still require validation on
the target desktop.
