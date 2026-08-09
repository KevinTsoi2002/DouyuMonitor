# 需求审计缺口修复实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** 修复审计发现的历史记录和资料刷新行为，同步最新设计文档，生成最新 Windows 安装包，并完成一次真实 Electron 弹幕连接验证。

**Architecture:** 保持现有 Zustand store、Electron IPC 和 StreamGet 边界。重复添加只更新历史时间并继续返回 `duplicate`；资料刷新根据最新在线状态重建会话状态，在线恢复时复用既有播放源检查入口。第 4、6、7 项保留为已确认限制，不在本计划中实现。

**Tech Stack:** TypeScript、React、Zustand、Vitest、Electron、Playwright、electron-builder

---

### Task 1：修复资料库行为

**Files:**
- Modify: `tests/workspace-store.test.ts`
- Modify: `src/renderer/store/workspace-store.ts`

- [x] **Step 1:** 增加重复添加更新时间和离线恢复播放检查的失败测试。
- [x] **Step 2:** 运行针对性测试确认当前实现失败。
- [x] **Step 3:** 实现最小 store 修复并保持现有返回值兼容。
- [x] **Step 4:** 运行 store 测试和完整测试。

### Task 2：同步文档和用户界面文案

**Files:**
- Modify: `docs/superpowers/specs/2026-08-10-room-library-and-compact-player-design.md`
- Modify: `docs/superpowers/plans/2026-08-10-room-library-and-compact-player.md`
- Modify: `src/renderer/components/AddRoomDialog.tsx`

- [x] **Step 1:** 记录当前列表只显示状态指示器、分区只在历史/收藏显示。
- [x] **Step 2:** 标记已完成计划步骤，并保留清晰度、压力测试和 Mock 头像限制。
- [x] **Step 3:** 将重复添加提示统一为“直播间列表”。
- [x] **Step 4:** 运行文案和应用烟雾测试。

### Task 3：生成交付包

**Files:**
- Generated: `release/DouyuMonitor-Setup-0.1.0-x64.exe`

- [x] **Step 1:** 运行 `npm run dist:win`。
- [x] **Step 2:** 检查安装包、解包目录和 sidecar 时间戳与当前源码构建一致。

### Task 4：真实 Electron 弹幕验证

- [x] **Step 1:** 使用临时用户目录启动 Electron，添加在线房间并监听弹幕状态。
- [x] **Step 2:** 只记录连接状态、消息计数和控制台错误数量，不记录弹幕正文或播放 URL 查询参数。
- [x] **Step 3:** 关闭 Electron 并确认进程退出。

### 明确保留的限制

- 生产环境当前只有一个合规 `auto` 清晰度变体。
- 不执行 9 路两小时压力测试。
- Vite Mock 数据不补真实主播头像，继续使用首字回退。
