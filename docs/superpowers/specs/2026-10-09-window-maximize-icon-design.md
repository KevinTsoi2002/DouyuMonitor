# Maximize Button State Design

## Approved Interaction

- A normal window displays the existing single-square maximize icon.
- A maximized window displays an overlapping-square restore icon.
- The accessibility name and tooltip change together: "最大化窗口" / "还原窗口".
- The existing maximize/restore command remains unchanged.
- The button and icon dimensions remain fixed; no animation is added.

## Implementation Boundary

`native/app/qml/components/WindowControls.qml` reads the actual containing
window's visibility using Qt Quick's window attachment. It does not maintain
a separate state or infer state from button clicks. This covers title-bar
double clicks and operating-system window changes as well as button clicks.

The restore asset is registered in `DOUYU_QML_ICON_ASSETS` in
`native/CMakeLists.txt`, using the same visual style as the existing window icons.
An unattached control uses the normal-window icon. Full-screen behavior stays
unchanged and uses the normal-window icon, matching the current maximize action.

## Verification

Add tests for normal, maximized, and restored window states, tooltip and
accessibility text, asset loading, and stable control dimensions. Observe the
new tests fail before implementation, then pass with the change.

Build the native Release executable and affected QML tests. Run QML interaction,
engine, visual smoke, and Windows window-lifecycle regression tests.

## Scope

Preserve the preceding taskbar-minimize fix and all unrelated local files.
Do not update the version, create an installer, commit, or upload to GitHub.

## Review

The user approved the written specification before implementation.
