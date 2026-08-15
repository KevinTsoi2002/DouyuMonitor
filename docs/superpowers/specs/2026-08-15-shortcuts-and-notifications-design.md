# 快捷键与通知设计

## 目标

为 DouyuMonitor 增加应用内快捷键、Toast 通知和可选的 Windows 系统通知，帮助用户在不离开监控画面的情况下完成常用操作，并及时获知播放和直播状态变化。

本设计只覆盖当前 Electron/Renderer 架构内的交互与状态反馈，不改变斗鱼播放源解析、签名、弹幕协议或状态刷新频率。

## 已确认范围

快捷键仅在应用窗口获得焦点时生效，不注册 Electron `globalShortcut`，也不影响其他应用。

| 快捷键 | 操作 |
| --- | --- |
| `Ctrl+Shift+A` | 打开添加直播间 |
| `Ctrl+Shift+W` | 打开/关闭工作区预设 |
| `Ctrl+Shift+M` | 打开/关闭监控状态 |
| `Ctrl+Shift+D` | 打开/关闭弹幕设置 |
| `Ctrl+Shift+S` | 展开/收起侧栏 |
| `Ctrl+Shift+R` | 刷新主直播间状态 |

快捷键触发条件：

- 只接受 `Ctrl + Shift` 组合；带有 `Alt` 或 `Meta` 的事件不匹配。
- 当前焦点位于 `input`、`textarea`、`select` 或 `contenteditable` 元素时忽略。
- 忽略 `event.repeat`，长按不会重复打开面板或重复刷新。
- 匹配成功后阻止浏览器默认行为。
- 没有主直播间时，`Ctrl+Shift+R` 显示 Toast“请先设置主直播间”，不发起空刷新。

通知策略：

- 普通用户操作、快捷键无效原因和设置结果使用应用内 Toast。
- 播放失败、自动恢复成功、直播间开播/下播使用系统通知（用户启用后）。
- 弹幕消息和周期性状态刷新本身不发送通知。
- 系统通知默认关闭，用户可在监控状态面板中启用；偏好跨工作区和重启持久化。

## 方案选择

### 方案 A：Renderer 监听 + Electron Notification IPC（推荐）

Renderer 统一处理快捷键、Toast 和状态转移判断；系统通知只通过 preload 暴露的最小 IPC 方法交给主进程创建 `Notification`。通知策略可在浏览器开发模式和 Electron 模式保持一致，且不会把 Node/Electron API 暴露给页面。

优点：符合现有安全边界，测试可以覆盖纯函数和 React 行为；不会产生全局快捷键冲突。代价是需要扩展 IPC contract、preload bridge 和主进程 handler。

### 方案 B：全部由 Electron 主进程处理

主进程注册快捷键和观察窗口状态，再把事件推回 Renderer。该方案对系统通知直接，但全局/窗口焦点边界复杂，且主进程需要理解 Renderer 的面板状态，不符合当前 UI 状态归属。

### 方案 C：只使用浏览器 Web Notification API

实现简单，但 Electron 的权限和行为与浏览器开发模式不一致，无法保证打包应用的 Windows 通知体验，也会扩大 Renderer 对权限状态的处理范围。

采用方案 A。

## 架构与模块边界

### 快捷键控制器

新增一个无 UI 的 Renderer 模块（建议为 `src/renderer/shortcuts.ts`）和一个 React hook（建议为 `src/renderer/hooks/use-app-shortcuts.ts`）：

- `matchesShortcut(event, shortcut)`：纯函数，负责组合键和可编辑目标判断。
- `useAppShortcuts(actions)`：在组件挂载时注册单个 `window.keydown` 监听，在卸载时移除。
- `actions` 由 `App` 提供，包含打开添加房间、切换三个面板、切换侧栏、刷新主房间和发送 Toast 的回调。
- 面板的开关状态提升到 `App`，`AppHeader` 接收受控状态和切换回调；这样快捷键和鼠标点击走同一条状态路径，避免两个来源互相覆盖。
- 快捷键成功执行不额外弹 Toast；无主房间刷新、刷新失败等需要解释的结果才提示。

快捷键控制器不直接依赖 Zustand，也不读取 DOM 以外的全局状态，因此可以通过合成 `KeyboardEvent` 独立测试。

### Toast 状态层

新增轻量的 Renderer Toast provider（建议为 `src/renderer/notifications/toast-context.tsx`）和展示组件（建议为 `src/renderer/components/ToastViewport.tsx`）：

```ts
type ToastLevel = 'info' | 'success' | 'warning' | 'error';

interface ToastInput {
  level: ToastLevel;
  message: string;
  durationMs?: number;
  action?: { label: string; run: () => void };
}

interface ToastItem extends ToastInput {
  id: string;
  createdAt: number;
}
```

- `pushToast(input)` 返回 Toast id；`dismissToast(id)` 可主动关闭。
- 默认显示在窗口右上角、标题栏下方；最多同时显示 3 条，超出的旧消息先移除。
- `info/success/warning` 默认 3.5 秒自动关闭；`error` 默认 6 秒，仍可手动关闭。
- 使用 `role="status"` 展示普通消息，错误使用 `role="alert"`；按钮提供可见文本、键盘焦点和 `aria-label`。
- Toast 只负责短反馈，不保存到工作区预设，也不写入历史记录。

### 系统通知 IPC

在 `src/shared/ipc-contract.ts` 增加两个白名单通道：

- `notifications.getSupport`：返回 `{ supported: boolean }`。
- `notifications.show`：接收 `{ title: string; body: string }`，主进程只创建系统通知，不接受 URL、HTML、脚本或任意窗口控制参数。

在 `src/preload/bridge.ts` 的 `AppApi` 暴露：

```ts
getSystemNotificationSupport(): Promise<IpcResult<{ supported: boolean }>>;
showSystemNotification(input: { title: string; body: string }): Promise<IpcResult<void>>;
```

`src/main/ipc-handlers.ts` 校验标题和正文均为非空字符串并限制长度（标题 80 字符、正文 240 字符）。主进程使用 Electron `Notification.isSupported()` 和 `new Notification({ title, body }).show()`；任何创建失败都返回可处理的 IPC 错误，不把原始异常或敏感参数传回 Renderer。

Renderer 只通过 preload 调用 IPC。浏览器开发模式或旧版 preload 没有这些方法时，通知服务视为不可用：系统通知不发送，事件仍转成 Toast；设置面板显示“当前运行环境不支持系统通知”，而不是抛出异常。

### 通知偏好持久化

新增独立的应用偏好存储（建议 key：`douyu-monitor.preferences.v1`），不放入工作区快照或工作区预设，避免切换预设意外改变通知开关：

```ts
interface AppPreferences {
  systemNotificationsEnabled: boolean;
}
```

- 首次运行默认为 `false`。
- 读取时对 JSON、版本和布尔字段做校验；非法值回退默认值，不阻止应用启动。
- 设置开关后立即写入；写入失败仅显示 Toast，不影响监控状态。
- 不保存通知历史、房间播放 URL、token、Cookie 或弹幕内容。

### 状态转移观察器与策略

新增无 UI 的通知策略模块（建议为 `src/renderer/notifications/notification-policy.ts`），由 `App` 或专用 hook 订阅 `WorkspaceState.rooms`：

1. 首次挂载只建立基线，不发送任何状态通知。
2. 仅对同一 `roomId` 的前后状态进行比较。
3. 播放失败：从非问题状态进入 `error`、`blocked`、`recovery-exhausted` 或平台阻塞时触发。
4. 自动恢复成功：从上述问题/恢复中状态回到 `playing` 或可用播放状态时触发。
5. 开播/下播：`online` 从 `false` 变 `true` 或从 `true` 变 `false` 时触发。首次加载不算开播事件。
6. 房间移除、预设切换或应用重新加载不补发历史状态通知。

去重和限流：

- 去重 key 为 `${roomId}:${eventType}:${diagnosticCode}`；同一 key 在 5 分钟内只允许发送一次。
- 全局最多发送 6 条系统通知/60 秒；超过后抑制系统通知，但保留 Toast/监控面板状态。
- 抑制期间发生的同类事件不排队，不在稍后集中补发，避免过时消息打扰用户。
- 失败后再次恢复、再失败会形成新的状态转移；同一诊断码仍受 5 分钟去重窗口约束。

系统通知关闭或不可用时，策略仍记录基线并通过 Toast（仅对明确的失败/恢复动作）给出反馈；高频开播/下播事件在关闭系统通知时不转换成 Toast。

## 数据流

```text
KeyboardEvent
  -> useAppShortcuts
  -> App action / WorkspaceStore action
  -> Toast provider (必要时)

WorkspaceState.rooms
  -> notification-policy (首帧建基线、后续比较)
  -> dedupe + rate limit
  -> preferences enabled?
       -> window.appApi.showSystemNotification (Electron)
       -> Toast fallback (browser/IPC failure)
```

所有系统通知文本都由 Renderer 根据房间名、房间号和状态标签生成，禁止携带播放源 URL、查询参数、签名、Cookie 或原始错误对象。

## 错误处理

- IPC 参数非法：返回 `INVALID_INPUT`，Renderer 显示通用 Toast。
- Electron 不支持系统通知：开关不可用并提示原因；监控功能和 Toast 继续工作。
- 通知创建失败：不重试同一通知，不阻塞房间刷新；显示一次错误 Toast，并在当前会话内短暂抑制相同错误。
- 偏好读写失败：使用内存默认值，显示一次警告 Toast。
- 快捷键 action 抛错：由 action 自己转换为 Toast；快捷键监听器不让异常冒泡到 `window`。

## 测试设计

### 单元测试

- 快捷键：正确匹配 6 个组合键、忽略可编辑元素、忽略 `repeat`、忽略 Alt/Meta、无主房间刷新提示。
- Toast：入队/移除、最多 3 条、默认时长、手动关闭和 action 调用。
- 偏好：默认值、合法 round-trip、非法 JSON/版本回退、写入异常降级。
- 通知策略：首次基线不通知、播放失败/恢复、开播/下播转移、房间移除、5 分钟去重、6/分钟限流。
- IPC contract、主进程 handler 和 preload bridge：通道白名单、参数校验、支持性结果和失败映射。

### React/集成测试

- `App` 键盘事件会切换对应面板，鼠标与快捷键状态一致；面板互斥关系保持不变。
- Toast viewport 的无障碍属性、关闭按钮和响应式定位。
- 系统通知开关在支持/不支持环境下的显示、持久化和 fallback。
- App smoke 确认无 `window.appApi` 的浏览器开发模式不报错。

### 浏览器/Electron 验证

- Playwright 在 1280px 和 390px 验证快捷键打开/关闭面板、Toast 不造成横向溢出。
- Electron 开发或打包运行一次支持查询，验证系统通知 IPC 只传递标题和正文；不记录通知原始 payload、播放 URL 或凭据。
- 回归运行 `npm test`、`npm run typecheck` 和 `npm run build`。

## 非目标

- 不使用 Electron `globalShortcut`，不提供全局热键。
- 不实现通知中心历史、系统通知点击跳转、声音提示或跨设备同步。
- 不改变周期性状态刷新策略、播放源解析/签名流程、弹幕内容和工作区预设数据模型。
- 不在通知或 Toast 中显示敏感播放信息。

## 验收标准

1. 6 个快捷键在应用聚焦时可用，在输入控件中不会抢占用户输入。
2. 无主直播间刷新时有明确 Toast；有主直播间时调用现有刷新动作。
3. 系统通知开关可持久化，关闭时不产生系统通知。
4. 失败、恢复、开播、下播只在状态转移时通知，首次加载和高频刷新不重复通知。
5. Electron 不支持或浏览器开发模式下功能降级为可解释的 Toast，不出现未捕获异常。
6. 单元、集成、类型检查、构建和 1280/390px UI 验证通过。
