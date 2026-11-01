# Maximize Button State Implementation Plan

> Execute inline in the existing workspace using executing-plans. Preserve the
> preceding taskbar fix. No commits, version changes, installers, or uploads.

**Goal:** Synchronize the maximize icon and its labels with actual window state.

**Architecture:** WindowControls binds to its containing Window attachment,
not click history. The existing AppController command remains unchanged.

**Tech Stack:** Qt 6.8.3 QML, QtTest, CMake, Windows x64.

## Task 1: Red Test

- [x] Add `updatesMaximizeIconWithWindowState` to
  `native/tests/qml_interaction_test.cpp`.
  Load Main.qml, assert normal icon and label, call `showMaximized()`,
  assert restore icon and label, call `showNormal()` and verify reversal.
  Read `Accessible.name` and `ToolTip.text` using QQmlProperty.
  Check Image.Ready and fixed button/icon dimensions in all states.
- [x] Build `qml_interaction_test` and run the new test on Windows.
  It must fail because the maximized icon remains `window-maximize.svg`.

## Task 2: Implementation

- [x] Bind `readonly property bool maximized` to
  `root.Window.visibility === Window.Maximized` in WindowControls.qml.
- [x] Bind accessibilityLabel to `qsTr("还原窗口")` when maximized and
  `qsTr("最大化窗口")` otherwise. Keep ToolTip.text bound to Accessible.name.
- [x] Bind the existing Image source to window-restore.svg/window-maximize.svg,
  keeping dimensions 15x15 and sourceSize 15x15.
- [x] Add the matching overlapping-square asset and register it in the shared
  `DOUYU_QML_ICON_ASSETS` list in native/CMakeLists.txt.

## Task 3: Verification

- [x] Run the new test on Windows and offscreen.
- [x] Build the native executable, QML engine, interaction, visual smoke,
  and close regression targets with windows-x64-release.
- [x] Run full engine, interaction, visual smoke tests; run close regression
  with QT_QPA_PLATFORM=windows. Capture normal/maximized window-control crops
  from the Windows run and inspect both.
- [x] Run the executable's --self-test, check git diff --check and scope,
  and report the executable path. Do not mutate remote state.
