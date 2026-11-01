# QML Theme And Controls Implementation Plan

**Goal:** Extend the existing dark theme and share Basic control styling without
redesigning pages or changing controller/player behavior.

**Architecture:** Keep `Theme.qml` as the only token source. Basic-derived controls
retain Qt input, accessibility and model semantics. Consumers replace styling
only; IDs, object names, geometry and handlers remain in place.

**Tech Stack:** Qt 6.8.3, Qt Quick Controls Basic, C++20, CMake/Ninja/MSVC.

## Checkpoints

- [x] Confirm current Qt SDK, Basic style, consumers and dirty worktree.
- [x] Run the existing engine and interaction tests before changes.
- [x] Add failing palette expectations and focused reusable-control tests.
- [x] Extend theme tokens and add reusable controls under `app/qml/controls`.
- [x] Register controls in the module and all resource-based QML test targets.
- [x] Integrate controls in dialogs, panels and existing settings controls.
- [x] Build the application and run QML, interaction, visual and close tests.
- [x] Inspect native Qt screenshots and review the final diff.

## Scope

Modify the theme, root palette, shared controls, their QML resource registration,
style-only consumers and tests. Do not edit room lists, playback toolbars,
window controls, stream services or persistence. Preserve the existing rank-page
and rank-link test changes.

Keep existing public theme names. Use dark text on filled orange actions for
contrast; use restrained tinted backgrounds for checked/selected controls.
Preserve Qt's ComboBox editable content and delegate/model behavior.

## Verification

Use Visual Studio x64 Developer PowerShell in `native`, with the existing
`QT_ROOT` and `MPV_ROOT`; do not bootstrap or install dependencies.

```powershell
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
ctest --preset windows-x64-release -R '^(qml_controls_style_test|qml_engine_smoke_test|qml_interaction_test|qml_visual_smoke_test|qml_visual_regression_test|qml_close_regression_test|qml_danmaku_overlay_test)$'
```

Native fixtures must not restore user settings or use live streams. Inspect
screenshots before accepting any visual baseline changes; preserve existing
baselines unless explicitly approved. Report intentional baseline differences
separately from functional failures.

## Outcome

Release build and nine selected CTest suites passed. Shared controls also passed
nine Qt Test cases on the Windows platform backend. The existing pixel regression
suite reports three changed captures across two test cases; baselines were not
updated. See `native/docs/qml-theme-phase1-verification.md` for evidence and limits.
