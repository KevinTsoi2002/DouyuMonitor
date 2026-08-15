# 监控状态面板 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Electron 多直播间工作区增加可操作的右侧监控状态抽屉。

**Architecture:** WorkspaceStore 保存低频播放诊断，DanmakuStore 继续拥有弹幕连接状态。纯视图模型负责状态标签、严重度和汇总，React 面板通过细粒度 selector 订阅两类 Store。

**Tech Stack:** React、TypeScript、Zustand、Vitest、Lucide、CSS。

---

### Task 1: Workspace 播放诊断状态

**Files:**
- Modify: `src/renderer/store/workspace-store.ts`
- Modify: `src/renderer/components/RoomTile.tsx`
- Modify: `src/renderer/components/RoomPlaybackSurface.tsx`
- Test: `tests/workspace-store.test.ts`

- [x] 写入失败测试：播放源检查记录 `playbackCheckedAt`，恢复诊断只更新目标房间，刷新开始清除旧诊断。
- [x] 增加 `PlaybackRecoveryDiagnostic` 和 `reportPlaybackRecovery`。
- [x] 将 RoomPlaybackSurface 的恢复状态通过 RoomTile 上报 Store。
- [x] 运行 `npx vitest run tests/workspace-store.test.ts tests/playback-recovery.test.ts tests/room-playback-live.test.tsx`。

### Task 2: 状态视图模型

**Files:**
- Create: `src/renderer/monitoring-status.ts`
- Test: `tests/monitoring-status.test.ts`

- [x] 写入失败测试：覆盖四项汇总、恢复中、恢复耗尽、平台阻塞、弹幕错误和最近错误优先级。
- [x] 实现 `getMonitoringSummary`、`getRoomMonitoringView` 和 `formatMonitoringTime`。
- [x] 运行 `npx vitest run tests/monitoring-status.test.ts`。

### Task 3: 细粒度弹幕状态 Hook

**Files:**
- Modify: `src/renderer/store/danmaku-context.tsx`
- Test: `tests/danmaku-store.test.ts`

- [x] 使用现有 DanmakuStatus 引用实现 `useDanmakuStatus(roomId)`。
- [x] 实现返回 primitive 的 `useDanmakuIssueCount(roomIds)`，只统计 `failed` 和 `platform-blocked`。
- [x] 运行 `npx vitest run tests/danmaku-store.test.ts tests/danmaku-contract.test.ts`。

### Task 4: 右侧监控面板

**Files:**
- Create: `src/renderer/components/MonitoringStatusPanel.tsx`
- Modify: `src/renderer/components/AppHeader.tsx`
- Modify: `src/renderer/styles.css`
- Create: `tests/monitoring-status-panel.test.tsx`

- [x] 写入失败组件测试：入口按钮、汇总、房间详情、空状态和操作按钮。
- [x] 实现顶栏入口、互斥面板状态、Escape 关闭和房间操作。
- [x] 增加桌面右侧抽屉与移动端全宽样式。
- [x] 运行 `npx vitest run tests/monitoring-status-panel.test.tsx tests/app-smoke.test.tsx tests/renderer-build-config.test.ts`。

### Task 5: 验证

- [x] 运行 `npm test`。
- [x] 运行 `npm run typecheck`。
- [x] 运行 `npm run build`。
- [x] 使用浏览器验证桌面和移动视口的打开、关闭、布局与控制台状态。
