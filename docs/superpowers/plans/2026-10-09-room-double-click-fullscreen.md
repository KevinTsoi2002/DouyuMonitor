# Room Double-Click Fullscreen Implementation Plan

**Goal:** Double-clicking any room picture toggles the whole application's
fullscreen state without changing its room layout.

**Approved behavior:** A second double-click or Escape exits fullscreen and
restores the previous normal/maximized state. Control bars, menus and quality
popups do not trigger fullscreen. No version change, installer, commit or upload.

**Architecture:** RoomTile emits a fullscreen request from a bounded picture
MouseArea. WorkspaceGrid forwards it to Main.qml, which invokes the existing
fullscreen function and AppController command. No player or domain changes.

## Steps

- [x] Add a Windows QML integration test with two fake rooms, actual mouse
  double-clicks, normal/maximized restoration and Escape exit.
- [x] Run the new test before implementation and observe the fullscreen failure.
- [x] Add the bounded RoomTile MouseArea and forward the signal through
  WorkspaceGrid to Main.qml.
- [x] Run the new test; also verify single/right clicks and double-clicks on
  volume/quality/menu controls do not change window visibility.
- [x] Build Release executable and relevant QML targets. Run full engine,
  interaction, visual smoke and Windows close regression tests, plus self-test.
- [x] Check diff scope and report the executable path without publishing.

## Verification Evidence

Windows fullscreen integration: normal and maximized restoration, Escape,
two retained rooms and unchanged layout passed. The isolated interaction test
confirmed no fullscreen request from single/right clicks or control double
clicks. Closing a quality popup on the first click of a double-click originally
caused a fullscreen request; a guard using the system double-click interval
now prevents this and the test passes.

Full suites: interaction 74, engine 4, visual smoke 15, Windows lifecycle 9,
all passed. Offscreen font and incomplete test-controller warnings remain;
the fake-room fixture can emit image connection warnings.
