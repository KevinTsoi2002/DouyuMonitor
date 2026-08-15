# 播放失败自动恢复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为每个直播间增加有上限、可取消、可观测的播放失败自动恢复。

**Architecture:** 在 Renderer 内使用独立的播放恢复控制器维护每房间的退避次数和定时器。RoomPlaybackSurface 继续调用既有的播放源刷新入口，FlvVideo 通过 `playing` 事件通知恢复成功。

**Tech Stack:** React、TypeScript、Vitest、mpegts.js。

---

### Task 1: 退避控制器

**Files:**
- Create: `src/renderer/playback-recovery.ts`
- Test: `tests/playback-recovery.test.ts`

- [x] 定义 1/2/4/8 秒退避、4 次上限和控制器生命周期接口。
- [x] 覆盖重复错误只安排一个定时器、达到上限停止、播放恢复重置、销毁取消定时器。

### Task 2: 播放器成功事件

**Files:**
- Modify: `src/renderer/components/FlvVideo.tsx`
- Test: `tests/flv-video.test.tsx`

- [x] 增加可选 `onPlaying` 回调。
- [x] 绑定并清理 `HTMLVideoElement` 的 `playing` 监听。

### Task 3: 播放表面接入恢复

**Files:**
- Modify: `src/renderer/components/RoomPlaybackSurface.tsx`
- Test: `tests/room-playback-live.test.tsx`, `tests/room-playback-surface.test.tsx`

- [x] 将播放器错误转为自动恢复控制器事件。
- [x] 自动恢复调用现有房间播放源刷新，手动重试清零次数。
- [x] 在自动恢复等待与达到上限时显示对应状态。

### Task 4: 验证

- [x] 运行恢复、FLV 和播放表面测试。
- [x] 运行 `npm run typecheck`。
- [x] 运行完整 `npm test` 和 `npm run build`。
