# 斗鱼实时弹幕接入设计

## 1. 设计结论

Electron 主进程负责斗鱼弹幕连接。每个已添加房间最多持有一条 WebSocket 连接，主进程解析协议后只向 Renderer 推送规范化的普通文字弹幕。Renderer 不接触 WebSocket、原始协议包或斗鱼页面脚本。

本阶段采用社区公开的匿名只读协议。应用不登录斗鱼账号，不发送弹幕，不保存弹幕历史，也不引入播放地址、Cookie、签名或设备标识。斗鱼要求登录、签名或其他访问控制时，客户端停止连接并报告平台阻塞。

首版只处理 `chatmsg`。礼物、进房提示、系统广播、语音弹幕和其他消息类型不进入 IPC。

## 2. 范围

### 2.1 目标

- 为最多 9 个已添加房间建立相互隔离的实时弹幕会话。
- 在对应房间画面上显示“昵称：内容”。
- 支持显示、隐藏、房间删除和窗口关闭时的完整生命周期。
- 处理二进制半包、粘包、多帧、心跳、断线重连和端点轮换。
- 通过类型化 IPC 推送消息与连接状态。
- 在高流量房间限制内存、IPC 和渲染负载。
- 保留浏览器开发预览的明确 Mock 模式，Electron 只使用真实弹幕源。

### 2.2 非目标

- 不登录斗鱼用户账号或开放平台账号。
- 不发送弹幕、礼物或其他互动指令。
- 不保存、搜索、导出或回放弹幕历史。
- 不解析用户 UID、头像、等级、粉丝牌或礼物价值。
- 不实现视频播放、清晰度切换或斗鱼播放地址签名。
- 不执行斗鱼页面 JavaScript，不嵌入斗鱼网页，不猜测受保护接口参数。
- 不承诺非官方协议长期稳定。

## 3. 协议证据与限制

2026-08-07 的只读研究得到以下结果：

- 斗鱼开放平台的公开目录仍列出“房间弹幕”“拉取弹幕”“接入弹幕”和“TCP弹幕接入”。具体文档要求开发者登录。入口为 <https://open.douyu.com/source/api/63>，目录接口为 <https://open.douyu.com/api/open/book/inx?type=0>。
- npm 包 [`@blackyu-he/douyu-danmu`](https://www.npmjs.com/package/@blackyu-he/douyu-danmu) 于 2025-07-23 更新，仍使用 `wss://danmuproxy.douyu.com:8501-8506/`、`loginreq`、`joingroup` 和 `mrkl`。该包的仓库当前无法公开访问，且发布代码没有完整校验帧长度和重连状态，不适合作为项目依赖。
- [`biliLive-tools`](https://github.com/renmu123/biliLive-tools/tree/master/packages/DouYuRecorder/src/dy_client) 在 2026-08-06 仍维护 TypeScript 客户端。其实现使用相同端点、匿名入组、45 秒心跳、长度帧解码和断线重连。
- 对 `danmuproxy.douyu.com:8501` 的 TLS/HTTP 探测返回 `101 Switching Protocols`，说明至少一个 WebSocket 端点在研究时可接受升级。该结果不证明协议获得斗鱼长期支持。

斗鱼官方目录属于高强度证据。活跃开源实现和实时端点探测属于中等强度证据。匿名消息格式、错误码和长期可用性没有公开稳定契约，项目必须把这些内容封装在可替换的基础设施模块中。

当前环境没有可用的 Context7 和 GitHub MCP 连接器。研究改用斗鱼公开接口、npm 元数据和 GitHub 公共 API，没有使用登录态或私有凭据。

## 4. 方案比较

### 4.1 主进程协议适配器

采用此方案。项目使用 `ws` 处理 WebSocket 传输，并实现限定范围的帧编解码和 STT 解析。主进程管理连接、重连和批量推送。

该方案把非官方协议集中在一个模块内，可以针对边界条件编写单元测试。项目只增加一个成熟的传输依赖，不引入录制器、播放器或礼物模型。

### 4.2 直接依赖小型弹幕包

`@blackyu-he/douyu-danmu` 的发布代码按 NUL 字节切分消息，没有完整校验重复长度字段、帧上限和残缺帧。它也缺少可用的公开源码仓库和完整重连控制。项目不采用该方案。

### 4.3 Renderer 直接连接

Renderer 直连会把协议、重连和资源管理放入 UI 生命周期。多个房间和热更新会增加重复连接与监听器泄漏风险，也会扩大 Preload 之外的网络能力。项目不采用该方案。

## 5. 架构

### 5.1 模块边界

新增以下模块，具体文件名允许实施计划按现有目录惯例微调：

- `src/infrastructure/douyu-danmaku/protocol.ts`：帧编解码、长度校验和 UTF-8 解码。
- `src/infrastructure/douyu-danmaku/stt.ts`：斗鱼键值文本的转义、反转义与平面对象解析。
- `src/infrastructure/douyu-danmaku/client.ts`：单房间连接、握手、心跳、端点轮换和重连。
- `src/infrastructure/douyu-danmaku/types.ts`：原始消息的最小内部类型。
- `src/main/danmaku-session-manager.ts`：会话去重、房间上限、消息批处理和窗口清理。
- `src/shared/danmaku-contract.ts`：IPC 请求、事件、状态和运行时校验。
- `src/renderer/store/danmaku-store.ts`：按房间隔离的连接状态、去重集合和限长队列。

`DanmakuOverlay` 只消费 `danmaku-store`。`workspace-store` 继续拥有房间列表和 `danmakuEnabled`，不承载高频消息。

### 5.2 数据流

1. 用户添加房间，`workspace-store` 创建 `RoomSession`。
2. Renderer 通过 Preload 调用 `startDanmaku(roomId)`。
3. Main 校验房间号，并让 `DanmakuSessionManager` 创建或复用会话。
4. `DouyuDanmakuClient` 连接一个端点，发送匿名登录和入组消息。
5. 客户端解析服务端帧，只将 `type=chatmsg` 交给会话管理器。
6. 会话管理器规范化字段、去重、限流，并按房间批量发送 IPC 事件。
7. `danmaku-store` 更新对应房间，`DanmakuOverlay` 渲染新消息。
8. 用户删除房间时 Renderer 调用 `stopDanmaku(roomId)`。窗口销毁时 Main 停止该窗口拥有的全部会话。

隐藏弹幕不会断开连接。Store 在 `danmakuEnabled=false` 时丢弃该房间的新消息。用户再次显示弹幕后只看到后续消息。

## 6. 协议处理

### 6.1 端点

客户端只连接以下固定白名单：

```text
wss://danmuproxy.douyu.com:8501/
wss://danmuproxy.douyu.com:8502/
wss://danmuproxy.douyu.com:8503/
wss://danmuproxy.douyu.com:8504/
wss://danmuproxy.douyu.com:8505/
wss://danmuproxy.douyu.com:8506/
```

客户端随机选择首个端点，后续重连按端口轮换。Renderer 不能提供或覆盖端点 URL。

### 6.2 发送消息

客户端只发送四类消息：

- 打开连接后发送 `type@=loginreq/roomid@={roomId}/`。
- 打开连接后发送 `type@=joingroup/rid@={roomId}/gid@=-9999/`。
- 连接存续期间每 45 秒发送 `type@=mrkl/`。
- 主动停止时发送 `type@=logout/`，随后关闭 WebSocket。

客户端使用 12 字节小端序头部。发送帧的协议类型为 `689`，负载以 NUL 结尾。客户端不发送 Cookie、token、设备 ID 或用户信息。

### 6.3 接收帧

解码器保留未消费字节，并循环读取完整帧：

- 至少收到 12 字节后读取第一个长度字段。
- 第一个长度与第二个长度必须一致。
- 帧总长度为第一个长度加 4 字节。
- 长度必须覆盖头部和 NUL 结尾，且不能超过 1 MiB。
- 服务端协议类型只接受 `690`。未知类型作为协议错误处理。
- 解码器去除末尾 NUL，并使用严格 UTF-8 解码。
- 同一 WebSocket 消息中的多个帧逐个输出，残缺帧留到下一批字节。

单个非法帧会终止当前连接。客户端切换端点重连，不把损坏字节继续送入解析器。

### 6.4 STT 与消息过滤

STT 解析器按 `/` 分段，并在每段第一个 `@=` 处分隔键值。解析器先把 `@S` 还原为 `/`，再把 `@A` 还原为 `@`。首版只解析平面对象。

只有以下字段进入规范化流程：

- `type`
- `rid`
- `cid`
- `nn`
- `txt`

客户端要求 `type=chatmsg`，并校验 `rid` 与当前房间一致。缺少昵称时使用“匿名用户”，缺少 `cid` 时使用房间会话内单调递增的序号生成本地 ID。空文本不进入 IPC。

## 7. 共享契约

### 7.1 状态与消息

```ts
export type DanmakuConnectionState =
  | 'idle'
  | 'connecting'
  | 'connected'
  | 'reconnecting'
  | 'failed'
  | 'platform-blocked';

export interface DanmakuMessage {
  id: string;
  roomId: string;
  nickname: string;
  text: string;
  receivedAt: string;
}

export interface DanmakuStatus {
  roomId: string;
  state: DanmakuConnectionState;
  attempt?: number;
  errorCode?:
    | 'NETWORK_UNAVAILABLE'
    | 'HANDSHAKE_TIMEOUT'
    | 'PROTOCOL_CHANGED'
    | 'AUTH_REQUIRED'
    | 'RETRY_EXHAUSTED';
}

export type DanmakuEvent =
  | { type: 'messages'; roomId: string; messages: DanmakuMessage[]; dropped: number }
  | { type: 'status'; status: DanmakuStatus };
```

`receivedAt` 使用本机接收时间的 ISO 字符串。应用不把它描述为斗鱼服务端发送时间。

### 7.2 IPC 与 Preload

新增白名单频道：

```text
danmaku.start
danmaku.stop
danmaku.event
```

Preload 暴露：

```ts
startDanmaku(roomId: string): Promise<IpcResult<void>>;
stopDanmaku(roomId: string): Promise<IpcResult<void>>;
onDanmakuEvent(listener: (event: DanmakuEvent) => void): () => void;
```

Main 在调用会话管理器前校验 `roomId` 为 1 至 20 位数字。Preload 的事件订阅返回解绑函数，不暴露 `ipcRenderer`、通用 `invoke` 或通用 `on`。

`start` 和 `stop` 都具备幂等性。同一窗口重复启动同一房间不会创建第二条连接。会话管理器按 WebContents 所有权清理资源，为未来多窗口保留正确行为。

## 8. 连接状态与重连

### 8.1 建连

客户端建立 WebSocket 后发送登录和入组消息，并启动 10 秒握手超时。收到合法 `loginres`、合法入组响应或当前房间的第一条合法服务端消息后，客户端进入 `connected`。

客户端在连接稳定 60 秒或收到第一条合法 `chatmsg` 后清零失败计数。

### 8.2 重连

网络错误、异常关闭、握手超时和协议错误触发重连。延迟序列为 1、2、4、8、15、15 秒，每次加入正负 20% 随机抖动。客户端最多连续尝试 6 次，并在每次尝试时切换端点。

第 6 次失败后状态变为 `failed`，错误码为 `RETRY_EXHAUSTED`。失败状态提供独立的重试操作，新的手动启动会重置计数。

### 8.3 平台阻塞

客户端只在获得明确证据时使用 `platform-blocked`：

- 所有端点的 WebSocket 升级都返回 HTTP 401 或 403。
- 服务端返回可识别的登录、token 或签名要求。
- WebSocket 以策略拒绝关闭，且原因明确包含认证要求。

DNS、TLS 超时、普通断网、单端点失败和未知关闭原因使用 `failed` 或 `reconnecting`。协议结构变化使用 `PROTOCOL_CHANGED`，不把网络故障误报为平台限制。

## 9. 负载与数据保护

会话管理器为每个房间保留最多 100 条待推送消息。它每 250ms 发送一批，每批最多包含 10 条最新消息。队列溢出时丢弃最旧消息，并在 `dropped` 中报告数量。该限制把单房间 IPC 速率控制在每秒 40 条以内。

Renderer 为每个房间保留最多 50 条待显示消息。叠加层每秒最多启动 3 条动画，并限制同时可见数量。空间不足时移除最旧的可见消息。应用优先保证界面响应和多房间隔离，不承诺在高热度房间逐条显示全部弹幕。

主进程按 `cid` 保存最近 200 个 ID 用于去重。没有 `cid` 的消息使用本地会话 ID，不跨重连去重。

规范化时执行以下限制：

- 昵称最多保留 40 个 Unicode 字符。
- 文本最多保留 200 个 Unicode 字符。
- 字符串去除 NUL 和不可见控制字符，保留换行以外的普通 Unicode 文本时将换行转换为空格。
- 主进程和 Renderer 不记录昵称、文本或原始消息正文到日志。

## 10. Renderer 行为

`DanmakuOverlay` 删除 `mock-danmaku` 的生产依赖，并修复当前错误编码的无障碍标签。Electron 只从 `window.appApi.onDanmakuEvent` 接收消息。

每条消息显示“昵称：内容”，沿用现有半透明背景、文本阴影和紧凑字号。动画只播放一次，约 7 秒后移除。叠加层不接受点击，不遮挡房间控制按钮。

弹幕按钮保留现有显示与隐藏语义。旁边的状态图标和 Tooltip 告知用户连接中、重连中、连接失败或平台阻塞。`failed` 状态显示带 `RotateCw` 图标的重试按钮；`platform-blocked` 状态不提供自动绕过或反复重试。界面不显示协议说明、端点地址或调试字段。

没有合规视频直链时，房间仍显示现有播放平台阻塞占位画面。实时弹幕覆盖在同一播放区域上，用户可以独立关闭弹幕。

浏览器开发预览通过单独的 Mock `DanmakuSource` 产生演示消息，并保留“模拟画面”标识。Mock 不能在 Electron 环境或生产 IPC 失败时作为回退。

## 11. 错误处理与清理

- 删除房间时先停止会话，再移除 Renderer 中的消息和状态。
- 窗口销毁或应用退出时，Main 清理对应定时器、WebSocket 和监听器。
- Preload 返回的解绑函数只移除该 Renderer 的 IPC 监听器，不改变房间会话状态。
- 主动停止发送 `logout`；连接未打开时直接取消连接并禁止重连。
- Main 捕获所有会话错误，一个房间失败不能关闭其他房间的连接。
- IPC 事件只发送到拥有订阅的 WebContents。销毁后的 WebContents 不接收事件。
- 应用不因实时连接失败切换到 Mock 数据。

## 12. 测试策略

### 12.1 协议单元测试

- 编码后的长度字段、协议类型和 NUL 结尾正确。
- 解码器处理单帧、多帧、半包和跨消息粘包。
- 解码器拒绝重复长度不一致、过短、超出 1 MiB 和未知协议类型的帧。
- STT 处理 `@A`、`@S`、空值和第一个 `@=` 分隔规则。
- 消息过滤只接受当前房间的 `chatmsg`。

### 12.2 客户端测试

使用可注入的 WebSocket 工厂和伪时钟验证：

- 打开后发送登录、入组和 45 秒心跳。
- 主动停止发送登出并禁止重连。
- 端点轮换、退避序列、抖动边界和 6 次上限正确。
- 握手超时、非法帧和异常关闭产生预期状态。
- 连接稳定后重置失败计数。

### 12.3 会话与 IPC 测试

- 同一房间重复启动只创建一条连接。
- 第 10 个不同房间被拒绝。
- 两个房间的消息、状态和错误相互隔离。
- 批处理、队列溢出和 `dropped` 计数正确。
- 房间删除和 WebContents 销毁会停止会话。
- Preload 只暴露三个限定接口，事件解绑后不再调用监听器。

### 12.4 Store 与组件测试

- Store 按房间保存状态，并按 `cid` 去重。
- 隐藏弹幕时丢弃消息，再次显示后只展示新消息。
- 队列保持 50 条上限，过期消息按时移除。
- `DanmakuOverlay` 显示昵称和文本，动画不会循环。
- 失败状态的重试按钮重置连接计数，平台阻塞状态不显示重试按钮。
- Electron 路径不读取 Mock 数据；浏览器预览保留明确的 Mock 标记。

### 12.5 运行验证

- 运行 `npm test`、`npm run typecheck` 和 `npm run build`。
- 使用 Playwright 检查桌面与移动视口的浏览器 Mock 预览，确认文字不溢出或遮挡控制项。
- 启动 Electron，添加在线房间 `63136`，确认连接状态进入 `connected` 或给出可解释错误。
- 在线房间出现新 `chatmsg` 时确认消息显示在对应画面，其他房间不显示该消息。
- 测试期间没有新弹幕时，只记录连接成功，不声称收到真实消息。
- 关闭窗口后检查进程、连接和计时器是否退出。

## 13. 验收标准

- Electron 添加在线房间后启动一条匿名只读弹幕连接。
- 每个房间只显示自己的普通文字弹幕。
- 弹幕隐藏、再次显示、删除房间和关闭应用均符合生命周期规则。
- 连接中断会按规定重连，达到上限后停止。
- 明确认证限制会显示平台阻塞，普通网络错误不会误报。
- 高流量不会让单房间队列超过设计上限，也不会阻塞其他房间。
- 应用不保存弹幕，不记录正文，不发送互动消息。
- 应用不引入斗鱼登录、签名、Cookie、页面脚本或视频播放绕过。
- 自动化测试、类型检查、构建、浏览器 UI 和 Electron 运行验证通过后，项目才能声明本阶段完成。

## 14. 后续边界

斗鱼修改或关闭匿名协议后，项目暂停弹幕连接并保留平台阻塞状态。团队只有在斗鱼提供公开接口或用户提供合法授权的开发者凭据后，才能设计新的官方适配器。该变更需要单独的设计与验收，不得在当前适配器中加入私有签名或登录逻辑。
