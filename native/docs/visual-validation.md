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

`qml_close_regression_test` fails only with the offscreen backend. Its nine-player case creates nine player items but no render contexts become ready (`9 players and 0 ready contexts`). The same test was then run with `QT_QPA_PLATFORM=windows`; all three cases passed, including `closesNineAttachedPlayersWithoutLingeringCallbacks`.

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
