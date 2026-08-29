# Qt P1 控件、清晰度与全屏 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task with review checkpoints.

**Goal:** 完成顶部控件、动态清晰度、默认音频焦点、品牌清理和应用级全屏。

**Architecture:** StreamGet 的 variants 在 C++ 中缓存为安全清晰度选项并随房间快照发布；QML 顶部声音总控只调用 AppController；ApplicationWindow 维护全屏前状态，C++ 负责窗口状态切换。

**Tech Stack:** Qt 6.8, Qt Quick/QML, C++17, QtTest, CMake/Ninja。

---

### Task 1: 顶部图标、品牌和声音总控

**Files:** `native/app/qml/components/AppHeader.qml`, `native/app/qml/Main.qml`, `native/tests/qml_interaction_test.cpp`

- [x] 增加图标语义、品牌副标题移除和声音总控 Popover 的失败交互测试。
- [x] 用 bookmark 图标区分工作区预设与 layout-grid；将静音和单/多声道按钮收进声音总控菜单。
- [x] 运行 QML 交互测试并检查控件对象名和可访问名称。

### Task 2: 长标题稳定布局

**Files:** `native/app/qml/components/RoomTile.qml`, `native/tests/qml_interaction_test.cpp`, `native/tests/qml_visual_smoke_test.cpp`

- [x] 增加长标题视觉/结构测试。
- [x] 将顶部信息区改为剩余宽度布局，标题 `ElideRight`，右侧操作区固定宽度。
- [x] 运行交互与视觉测试并检查 1280x720、1920x1080、窄窗口截图。

### Task 3: 动态清晰度选项

**Files:** `native/src/workspace/room_workspace_types.h`, `native/src/workspace/room_session.*`, `native/src/workspace/multi_room_coordinator.*`, `native/src/ui/room_list_model.*`, `native/app/qml/components/RoomTile.qml`, focused tests

- [x] 为 variants 安全投影建立失败测试，禁止播放地址进入模型角色。
- [x] 缓存每房间清晰度选项，发布 `id/label/quality`，解析完成后更新；无效当前项回退自动。
- [x] 将 QML ComboBox 改为模型驱动并接入现有 setQuality。
- [x] 运行协议、会话、协调器、模型和 QML 回归。

### Task 4: 音频默认焦点与应用级全屏

**Files:** `native/src/workspace/multi_room_coordinator.*`, `native/src/ui/app_controller.*`, `native/app/qml/Main.qml`, `native/app/qml/components/AppHeader.qml`, `native/app/qml/components/WindowControls.qml`, tests

- [x] 增加首房间默认音频焦点和删除后回退测试。
- [x] 增加全屏/退出全屏命令、F11/Escape 和标题栏按钮，恢复进入前窗口状态。
- [x] 运行窗口交互、关闭回归和音频策略测试。

### Task 5: 全量验证与记录

**Files:** `docs/superpowers/logs/2026-08-24-qt-libmpv-native-progress.md`

- [x] 运行完整 Debug/Release CTest、截图、diff 检查、禁止运行时和敏感输出扫描。
- [x] 追加本地日志；Notion MCP 本轮未提供可调用连接。
