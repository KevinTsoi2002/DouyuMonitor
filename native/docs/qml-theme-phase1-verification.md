# QML Theme Phase 1 Verification

Date: 2026-10-14.

## Scope

`app/qml/Theme.qml` remains the only theme source. It defines surfaces, semantic
text/status colors, interaction colors, palettes, spacing, radii, typography and
control sizes. Existing token names remain available.

`app/qml/controls` contains Basic-derived Button, ToolButton, TextField, ComboBox,
CheckBox, RadioButton and ToolTip components plus a shared button background.
`highlighted` selects a filled primary button; `checked` selects a tinted
background; `destructive` affects hover/pressed feedback only. ToolButton retains
attached ToolTip text metadata and also shows a styled tooltip on keyboard focus.
Qt's native input, model roles, editable ComboBox, check states and signals remain
in use. Explicit dimensions in consumers take precedence over shared defaults.

Dialogs, panels and existing settings controls consume these components without
changing their business handlers or page structure. Room/sidebar and playback
toolbar components keep their specialized rendering. Their remaining literal
colors belong to later phases. No C++ business implementation changed.

## Environment And Commands

Qt SDK: `D:\Qt\6.8.3\msvc2022_64` (qmake and Qt Test report 6.8.3).
Release build uses CMake/Ninja/MSVC and the existing mpv SDK.
No dependencies were installed.

Run from `native`:

```powershell
& D:\VSBuildTools\Common7\Tools\Launch-VsDevShell.ps1 -Arch amd64 -HostArch amd64 -SkipAutomaticLocation -NoLogo
$env:QT_ROOT = 'D:\Qt\6.8.3\msvc2022_64'
$env:MPV_ROOT = 'D:\DouyuMonitor\native\sdk\mpv'
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release --target douyu_monitor_native qml_controls_style_test qml_engine_smoke_test qml_interaction_test qml_visual_smoke_test qml_close_regression_test qml_visual_regression_test qml_danmaku_overlay_test
ctest --preset windows-x64-release -R '^(qml_controls_style_test|qml_engine_smoke_test|qml_interaction_test|qml_visual_smoke_test|qml_close_regression_test|qml_danmaku_overlay_test|mpv_quick_item_test|multi_room_coordinator_test|douyu_monitor_native_self_test)$' --output-on-failure
```

Final selected CTest result: 9/9 suites passed, 64.75 seconds.
An additional native Windows run of `qml_controls_style_test` passed all nine
cases, with no QML warnings. It checks text contrast, dimensions, hover/press,
keyboard focus/activation, disabled labels/icons, editable input, ComboBox roles,
three-state checkboxes, radio exclusivity, tooltip focus and dialog loading.

Native screenshots were inspected under:
`out/build/windows-x64-release/verification/theme-controls/windows/`.
They include the control fixture and AddRoom, CloseBehavior,
NotificationSettings and TeamManager dialogs. Offscreen captures are stored
separately because that backend reports a missing font directory and renders
square fallback glyphs in this environment.

The new palette assertion was first observed failing against the old theme.
The disabled-label regression was also observed failing before its fix.
QML packaged-module registration and resource-based loading both passed.
`git diff --check` passed, with an existing line-ending conversion notice.
Pre-existing rank URL edits and untracked user files were preserved.

## Remaining Limits

The existing `qml_visual_regression_test` is not green against the old theme.
Geometry assertions and room/header baselines passed, but three images differ:

| Image | Changed Pixels |
| --- | --- |
| guild-navigation-284x720 | 2.071% |
| team-manager-720x760 | 6.871% |
| maozi-rank-1920x1080 | 2.468% |

The test reports five passed and two failed cases (one case compares two changed
images). Actual images and red difference masks remain under
`out/build/windows-x64-release/verification/qml-regression/`.
No comparison thresholds were relaxed and no baselines were overwritten.
Visual approval is still needed before updating those baselines.

The full CTest suite, installer, live-stream soak tests, multi-monitor DPI,
large OS font settings and screen readers were not validated in this phase.
Existing incomplete fake-controller warnings in the interaction suite were not
treated as production failures or addressed through unrelated business edits.
Context7 and Serena were unavailable; API choices were checked against the
installed Qt 6.8.3 Basic QML implementation and local type metadata.
