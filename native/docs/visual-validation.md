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
| `layout-primary-9-1920x1080.png` | Nine rooms, primary-room layout |
| `layout-primary-12-1920x1080.png` | Twelve rooms, primary-room layout |
| `layout-primary-16-1920x1080.png` | Sixteen rooms, primary-room layout |
| `layout-auto-24-1920x1080.png` | Twenty-four rooms, automatic layout |
| `layout-primary-24-1920x1080.png` | Twenty-four rooms, primary-room layout |
| `layout-primary-two-4-1280x720.png` | Four rooms, dual-primary layout |
| `layout-primary-two-10-1920x1080.png` | Ten rooms, dual-primary layout |
| `layout-primary-two-16-1920x1080.png` | Sixteen rooms, dual-primary layout |
| `layout-primary-two-24-1920x1080.png` | Twenty-four rooms, dual-primary layout |
| `watch-tools-dual-disabled-960x600.png` | Dual-primary option disabled |
| `watch-tools-dual-enabled-960x600.png` | Dual-primary option enabled |
| `room-quality-popup-640x420.png` | Quality selector popup open |
| `room-menu-640x420.png` | Room action menu open |
| `guild-navigation-shell-1280x800.png` | Guild navigation shell |
| `guild-member-hover-shell-1280x800.png` | Guild member hover state |
| `team-manager-720x760.png` | Team manager dialog open |
| `maozi-rank-1920x1080.png` | Maozi rank page |
| `maozi-placement-1920x1080.png` | Maozi placement page |

The suite compares these 23 exact names. It does not enumerate the baseline
directory. Five older PNGs remain there for historical reference and are not
current test inputs: `header-dual-disabled-960x260.png`,
`header-dual-enabled-960x260.png`, `guild-navigation-284x720.png`,
`maozi-play-value-1920x1080.png`, and `twenty-four-rooms-1920x1080.png`.
The first two use the retired 960x260 capture size; their current Watch Tools
states use the 960x600 captures listed above.

Pixel comparison converts both images to `Format_ARGB32`. A pixel counts as
different when any color channel differs by more than `16`. The test fails when
more than `1%` of the image pixels differ. Failed captures and red diff masks
are written under each build profile's `verification/qml-regression` directory.
The comparison helper also accepts ignored rectangles for known dynamic
regions, but the current state matrix does not use that mechanism because the
fixtures are deterministic.

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

The visual regression target runs with `QT_QPA_PLATFORM=windows` and the Basic
Qt Quick Controls style, as configured in `native/CMakeLists.txt`. Other QML
tests may use the offscreen platform. This automated check is not a replacement
for desktop acceptance: DPI scaling, GPU rendering, native popup placement,
multi-monitor behavior, and long-running playback still require validation on
the target desktop.

## Screenshot Fonts (2026-10-15)

`tests/test_font_support.h` prepares the font environment in the six QML test
executables before they load a QML component. On Windows with the offscreen
backend, it sets `QT_QPA_FONTDIR` to `%WINDIR%/Fonts` if the caller has not supplied
a directory. This replaces the plugin's otherwise empty build-local
`lib/fonts` search path. The tests fail during initialization if no Simplified
Chinese font is available.

The offscreen tests prefer the installed `Microsoft YaHei UI` family, then
`Microsoft YaHei`, and finally an available Simplified Chinese family. They
retain the application's initial font size and weight. The Windows backend
retains its native font selection. No font files are copied or bundled, and the
released application and user font settings are unchanged.

`qml_controls_style_test::rendersChineseTextWithoutMissingGlyphs` compares
captures of one real QML button displaying two different Chinese labels with
the default, control, Microsoft YaHei and SimHei fonts. Identical images fail
the test: the original box fallback renders both labels identically even
though Qt reports nonzero glyph indexes. This regression was observed failing
without font preparation and passing after the fix.

Rebuild and validate from `native` in the existing x64 Developer PowerShell:

```powershell
cmake --build --preset windows-x64-release --target qml_controls_style_test qml_engine_smoke_test qml_visual_smoke_test qml_visual_regression_test qml_interaction_test qml_danmaku_overlay_test
ctest --preset windows-x64-release -R '^qml_(controls_style|engine_smoke|visual_smoke|visual_regression|interaction|danmaku_overlay)_test$' --output-on-failure
```

The checked-in menu baseline contains square fallback glyphs. Correct Chinese
rendering changes pixels as well as text metrics, so old baselines must still be
reviewed explicitly before replacement. Font preparation does not alter the
comparison threshold or automatically update a baseline.

Validation for this fix (Qt 6.8.3, Windows x64 Release):

- All six affected test executables were recompiled and linked successfully.
- Selected CTest: 5/6 suites passed in 40.73 seconds. Controls, engine, visual
  smoke, interaction and danmaku tests passed.
- The direct Windows-backend controls run passed 11/11 Qt Test cases.
- The visual regression run reported five failed cases; all failure messages
  were pixel-baseline mismatches. No baseline or threshold was changed.
- Settings captures at 1600 and 640 pixels wide, the scrolled 26-role fixture,
  and the room menu were inspected with readable Chinese text.
- The missing font directory warning is gone. The offscreen font scan still
  emits `FT_New_Face failed with index 0 : 2` for a system font; it does not
  prevent the Chinese rendering regression from passing.

Fresh logs are stored in ignored `out/verification/font-final-build.txt`,
`font-final-ctest.txt`, `font-final-visual.txt` and
`font-controls-windows-final.txt`. This focused test-environment fix does not
constitute a full CTest, live-stream, installer or performance acceptance.
