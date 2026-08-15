# 快捷键与通知实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 Electron 多直播间工作区中加入窗口内快捷键、应用内 Toast 和可选系统通知，并保持浏览器开发模式可降级运行。

**Architecture:** Renderer 负责快捷键、Toast、状态转移和通知策略；面板状态由 `App` 统一持有。系统通知通过 preload 暴露的两个固定 IPC 通道交给主进程创建，应用偏好使用独立 localStorage key 持久化，不进入工作区或预设快照。

**Tech Stack:** React、TypeScript、Zustand、Electron `Notification`、Vitest、现有 CSS/Lucide 图标。

---

## 文件边界

- `src/shared/ipc-contract.ts`：通知通道、请求校验和结果类型。
- `src/main/system-notifications.ts`：Electron 系统通知能力封装，隔离平台 API。
- `src/main/ipc-handlers.ts`、`src/main/main.ts`：注册并注入通知 handler。
- `src/preload/bridge.ts`、`src/shared/window-api.d.ts`：最小化 preload API。
- `src/renderer/notifications/app-preferences.ts`：系统通知开关的独立持久化。
- `src/renderer/notifications/toast-context.tsx`、`src/renderer/components/ToastViewport.tsx`：Toast 状态和视图。
- `src/renderer/notifications/notification-policy.ts`：状态转移、去重和限流的纯逻辑。
- `src/renderer/notifications/notification-context.tsx`：偏好、支持性查询、IPC/fallback 编排。
- `src/renderer/shortcuts.ts`、`src/renderer/hooks/use-app-shortcuts.ts`：快捷键匹配和生命周期管理。
- `src/renderer/App.tsx`、`src/renderer/components/AppHeader.tsx`：面板状态提升和快捷键 action 接线。
- `src/renderer/components/MonitoringStatusPanel.tsx`、`src/renderer/styles.css`：系统通知设置和 Toast 样式。
- `tests/ipc-contract.test.ts`、`tests/ipc-handlers.test.ts`、`tests/preload-bridge.test.ts`：IPC 回归与安全边界。
- 新增 `tests/app-preferences.test.ts`、`tests/notification-policy.test.ts`、`tests/notifications.test.tsx`、`tests/shortcuts.test.ts`、`tests/app-shortcuts.test.tsx`、`tests/toast.test.tsx`。

## Task 1: 建立系统通知 IPC 边界

**Files:**
- Create: `src/main/system-notifications.ts`
- Modify: `src/shared/ipc-contract.ts`
- Modify: `src/main/ipc-handlers.ts`
- Modify: `src/main/main.ts`
- Modify: `src/preload/bridge.ts`
- Modify: `src/shared/window-api.d.ts`
- Test: `tests/ipc-contract.test.ts`, `tests/ipc-handlers.test.ts`, `tests/preload-bridge.test.ts`

- [ ] **Step 1: 写 IPC contract 失败测试**：断言 `notifications.getSupport`、`notifications.show` 通道存在且含点号；校验非空标题/正文、80/240 字符上限和非法 payload；断言旧的通道列表仍保持不变。
- [ ] **Step 2: 运行 contract 测试确认失败**：运行 `npm test -- tests/ipc-contract.test.ts`，预期因通道和校验函数不存在而失败。
- [ ] **Step 3: 实现最小 contract**：增加 `SystemNotificationRequest`、`isValidSystemNotificationRequest`、支持结果类型和两个通道；所有错误继续使用现有 `IpcResult`。
- [ ] **Step 4: 写主进程 handler 失败测试**：用 fake notifier 覆盖支持、成功显示、unsupported、抛错和非法参数；断言传入 notifier 的只有标题和正文，不带事件对象或原始异常。
- [ ] **Step 5: 运行 handler 测试确认失败**：运行 `npm test -- tests/ipc-handlers.test.ts`，预期 fake handler 尚未注册。
- [ ] **Step 6: 实现通知服务和 handler**：在 `system-notifications.ts` 封装 `Notification.isSupported()` 与 `new Notification({ title, body }).show()`；`registerIpcHandlers` 接收可注入服务，主进程通过 `createSystemNotificationService()` 注入，测试可使用 fake/unsupported 服务。
- [ ] **Step 7: 写 preload 失败测试并实现 bridge**：测试 `getSystemNotificationSupport` 和 `showSystemNotification` 只能调用固定通道；更新 `AppApi`、`createAppApi` 和 `window-api.d.ts`，不暴露通用 `invoke`。
- [ ] **Step 8: 运行任务测试**：运行 `npm test -- tests/ipc-contract.test.ts tests/ipc-handlers.test.ts tests/preload-bridge.test.ts`，预期全部通过。
- [ ] **Step 9: 提交**：`git add src/shared/ipc-contract.ts src/main/system-notifications.ts src/main/ipc-handlers.ts src/main/main.ts src/preload/bridge.ts src/shared/window-api.d.ts tests/ipc-contract.test.ts tests/ipc-handlers.test.ts tests/preload-bridge.test.ts && git commit -m "feat: add system notification ipc"`。

## Task 2: 实现独立应用偏好持久化

**Files:**
- Create: `src/renderer/notifications/app-preferences.ts`
- Test: `tests/app-preferences.test.ts`

- [ ] **Step 1: 写失败测试**：覆盖默认关闭、合法 round-trip、错误 schema/version、非法布尔值、损坏 JSON 和 storage `getItem/setItem` 抛错时的内存降级。
- [ ] **Step 2: 运行测试确认失败**：运行 `npm test -- tests/app-preferences.test.ts`，预期模块尚不存在。
- [ ] **Step 3: 实现偏好模块**：导出 `APP_PREFERENCES_STORAGE_KEY`、版本化 `AppPreferences`、`loadAppPreferences` 和 `saveAppPreferences`；复用 `WorkspaceStorage` 形状但使用独立 key，默认 `{ systemNotificationsEnabled: false }`，不写入房间数据。
- [ ] **Step 4: 运行测试确认通过**：再次运行 `npm test -- tests/app-preferences.test.ts`。
- [ ] **Step 5: 提交**：`git add src/renderer/notifications/app-preferences.ts tests/app-preferences.test.ts && git commit -m "feat: persist notification preferences"`。

## Task 3: 建立 Toast 状态层和视图

**Files:**
- Create: `src/renderer/notifications/toast-context.tsx`
- Create: `src/renderer/components/ToastViewport.tsx`
- Modify: `src/renderer/styles.css`
- Test: `tests/toast.test.tsx`

- [ ] **Step 1: 写失败测试**：验证 `pushToast` 返回 id、按 FIFO 保持最多 3 条、默认时长（普通 3500ms、错误 6000ms）、手动关闭、action 按钮和 `role=status/alert`。
- [ ] **Step 2: 运行测试确认失败**：运行 `npm test -- tests/toast.test.tsx`。
- [ ] **Step 3: 实现 Toast provider**：用 reducer/定时器维护 `ToastItem[]`，暴露 `useToast`，对 action 异常做安全处理；清理卸载时的所有 timer。
- [ ] **Step 4: 实现 ToastViewport**：使用 Lucide 状态图标、可聚焦关闭按钮、可见 action 文本和无障碍 role；不依赖固定宽度，长文本允许换行。
- [ ] **Step 5: 增加响应式样式**：Toast 固定在标题栏下方右侧，移动端改为左右 12px，最大宽度不造成横向溢出，沿用现有颜色变量和 4px 圆角。
- [ ] **Step 6: 运行测试确认通过**：运行 `npm test -- tests/toast.test.tsx`。
- [ ] **Step 7: 提交**：`git add src/renderer/notifications/toast-context.tsx src/renderer/components/ToastViewport.tsx src/renderer/styles.css tests/toast.test.tsx && git commit -m "feat: add in-app toast notifications"`。

## Task 4: 实现通知策略纯逻辑

**Files:**
- Create: `src/renderer/notifications/notification-policy.ts`
- Test: `tests/notification-policy.test.ts`

- [ ] **Step 1: 写失败测试**：使用可注入时间构造房间快照，覆盖首次基线无事件、播放失败、恢复成功、开播/下播、房间移除、同 key 5 分钟去重和 60 秒 6 条全局限流。
- [ ] **Step 2: 运行测试确认失败**：运行 `npm test -- tests/notification-policy.test.ts`。
- [ ] **Step 3: 实现策略**：定义 `NotificationEvent` 和 `createNotificationPolicy({ now, dedupeWindowMs, rateLimit })`；按 `roomId/eventType/diagnosticCode` 生成 key，首次 `seed` 只记录状态，后续 `diff` 返回有序事件；不保存原始错误或播放 URL。
- [ ] **Step 4: 运行测试确认通过**：运行 `npm test -- tests/notification-policy.test.ts`。
- [ ] **Step 5: 提交**：`git add src/renderer/notifications/notification-policy.ts tests/notification-policy.test.ts && git commit -m "feat: add notification transition policy"`。

## Task 5: 编排偏好、IPC 和状态观察

**Files:**
- Create: `src/renderer/notifications/notification-context.tsx`
- Test: `tests/notifications.test.tsx`
- Modify: `src/renderer/App.tsx`

- [ ] **Step 1: 写失败集成测试**：在 fake `window.appApi` 支持、不可用和调用失败三种环境下，验证首次房间快照不通知；状态转移调用系统通知；关闭开关、浏览器模式或 IPC 失败时只对明确失败/恢复动作使用 Toast。
- [ ] **Step 2: 运行测试确认失败**：运行 `npm test -- tests/notifications.test.tsx`。
- [ ] **Step 3: 实现 NotificationProvider**：在 ToastProvider 内部读取 `WorkspaceState.rooms`，初始化 policy 基线；查询系统通知支持性；加载/保存偏好；把策略事件转换为安全标题/正文，执行去重限流后调用 `window.appApi`，并处理 unsupported/异常 fallback。
- [ ] **Step 4: 接入 App provider 层**：让 `App` 返回 `ToastProvider -> NotificationProvider -> AppShell`，保持所有现有 `WorkspaceProvider` 调用方不需额外包裹；渲染 `ToastViewport` 一次。
- [ ] **Step 5: 运行集成测试确认通过**：运行 `npm test -- tests/notifications.test.tsx tests/app-smoke.test.tsx`。
- [ ] **Step 6: 提交**：`git add src/renderer/notifications/notification-context.tsx src/renderer/App.tsx src/renderer/main.tsx tests/notifications.test.tsx && git commit -m "feat: connect room status notifications"`。

## Task 6: 实现窗口内快捷键并统一面板状态

**Files:**
- Create: `src/renderer/shortcuts.ts`
- Create: `src/renderer/hooks/use-app-shortcuts.ts`
- Modify: `src/renderer/App.tsx`
- Modify: `src/renderer/components/AppHeader.tsx`
- Test: `tests/shortcuts.test.ts`, `tests/app-shortcuts.test.tsx`

- [ ] **Step 1: 写快捷键纯函数失败测试**：覆盖 6 个组合键、大小写 key、Ctrl/Shift 必须存在、Alt/Meta/repeat 拒绝、input/textarea/select/contenteditable 拒绝和普通按钮接受。
- [ ] **Step 2: 运行纯函数测试确认失败**：运行 `npm test -- tests/shortcuts.test.ts`。
- [ ] **Step 3: 实现 matcher**：导出稳定的快捷键表和 `matchesShortcut`/`isEditableTarget`，不注册 Electron global shortcut，不依赖 Zustand。
- [ ] **Step 4: 写 App 集成失败测试**：渲染 `WorkspaceProvider + App`，派发 keydown，验证添加弹窗、工作区/监控/弹幕面板和侧栏开关；输入控件内派发事件不改变 UI；无主房间刷新显示 Toast。
- [ ] **Step 5: 运行集成测试确认失败**：运行 `npm test -- tests/app-shortcuts.test.tsx`。
- [ ] **Step 6: 提升 App 面板状态并实现 hook**：把 `danmakuSettingsOpen`、`monitoringOpen`、`workspaceOpen` 从 `AppHeader` 提升到 `App`；`AppHeader` 接受受控 props 和互斥切换回调；`useAppShortcuts` 挂载一个 keydown listener，调用同一组 UI action，监听器内部捕获 action 异常并推送 Toast。
- [ ] **Step 7: 接入主房间刷新**：快捷键读取 `primaryRoomId` 和 `refreshStreamAvailability`；无主房间推送明确 Toast，有主房间调用现有动作，不改变周期刷新策略。
- [ ] **Step 8: 运行测试确认通过**：运行 `npm test -- tests/shortcuts.test.ts tests/app-shortcuts.test.tsx tests/app-smoke.test.tsx`。
- [ ] **Step 9: 提交**：`git add src/renderer/shortcuts.ts src/renderer/hooks/use-app-shortcuts.ts src/renderer/App.tsx src/renderer/components/AppHeader.tsx tests/shortcuts.test.ts tests/app-shortcuts.test.tsx tests/app-smoke.test.tsx && git commit -m "feat: add focused app shortcuts"`。

## Task 7: 在监控面板加入系统通知开关

**Files:**
- Modify: `src/renderer/components/MonitoringStatusPanel.tsx`
- Modify: `src/renderer/styles.css`
- Test: `tests/monitoring-status-panel.test.tsx`

- [ ] **Step 1: 写失败 UI 测试**：支持环境显示可操作开关，关闭/开启状态有 `aria-checked` 或原生 checkbox 语义；不支持环境显示禁用状态和解释，不影响房间状态列表。
- [ ] **Step 2: 运行测试确认失败**：运行 `npm test -- tests/monitoring-status-panel.test.tsx`。
- [ ] **Step 3: 实现设置行**：从 `useNotifications` 读取支持性和开关，切换后立即持久化；开启失败显示 Toast 并回滚为关闭；保持 Escape、刷新和原有汇总行为。
- [ ] **Step 4: 增加紧凑响应式样式**：沿用监控面板视觉层级，开关可键盘操作，1280px/390px 不溢出。
- [ ] **Step 5: 运行测试确认通过**：运行 `npm test -- tests/monitoring-status-panel.test.tsx`。
- [ ] **Step 6: 提交**：`git add src/renderer/components/MonitoringStatusPanel.tsx src/renderer/styles.css tests/monitoring-status-panel.test.tsx && git commit -m "feat: add notification preference control"`。

## Task 8: 完成回归与浏览器验证

**Files:**
- Test: existing full suite plus targeted UI checks.

- [ ] **Step 1: 运行完整自动化测试**：`npm test`，记录总数和失败信息；失败时先修复根因再继续，不跳过测试。
- [ ] **Step 2: 运行类型检查和构建**：`npm run typecheck`、`npm run build`。
- [ ] **Step 3: 启动开发服务并用 Playwright 验证 1280px**：确认 6 个快捷键、Toast 自动关闭/手动关闭、监控面板开关、面板互斥和无横向溢出。
- [ ] **Step 4: 用 Playwright 验证 390px**：确认 Toast 换行、监控面板开关和侧栏操作不遮挡、不溢出。
- [ ] **Step 5: 检查敏感信息边界**：对新增源码、测试输出和文档执行 `rg -n "wsAuth|token=|cookie=|playbackUrl" src tests docs/superpowers`，确认通知和 IPC 测试没有写入播放 URL、token、Cookie 或原始异常。
- [ ] **Step 6: 检查最终 diff**：`git diff --check`、`git status --short`；确认只包含本功能文件及必要 README 更新。
- [ ] **Step 7: 提交验证结果**：若验证阶段产生必要的测试或文档调整，将这些实际修改加入暂存区并提交 `test: verify shortcuts and notifications`；没有新增修改时保留前序提交，不创建空提交。

## 完成判定

- 6 个快捷键仅在应用窗口聚焦且非编辑控件中生效。
- 系统通知开关独立持久化，Electron 支持时走 IPC，不支持时可解释地降级为 Toast。
- 首次加载和周期刷新不产生重复通知；失败、恢复、开播、下播只按状态转移并受去重/限流约束。
- `npm test`、`npm run typecheck`、`npm run build` 和 1280/390px Playwright 验证通过。
