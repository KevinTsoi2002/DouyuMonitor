# 斗鱼播放源能力检测与阻塞状态设计

## 1. 设计结论

本阶段不实现斗鱼视频播放。应用只接受无需登录、Cookie、私有签名或访问控制绕过的独立播放器直连。公开接口未提供此类 URL 时，Electron 明确显示“暂无合规播放源”，并禁用清晰度控制。

应用新增播放源能力检测边界，为未来正式授权来源保留 `available` 分支。当前 Douyu HTTP 实现只返回 `blocked`，不会调用需要签名的 `getH5Play`，也不会把推测 URL 或 Mock 数据展示为真实播放。

## 2. 研究证据

2026-08-07 的只读探测得到以下结果：

- `open.douyucdn.cn/api/RoomApi/room/63136` 返回房间元数据，但响应不再包含 `hls_url`、`rtmp_url` 或 `rtmp_live`。
- `www.douyu.com/betard/63136` 返回 `room.multirates`，当前示例为“蓝光10M/type=0”，但不返回独立播放器 URL。
- 另一个在线高热度房间的 `betard` 响应只返回“超清/type=0”，说明 `multirates` 是展示信息，不能证明存在公开直连。
- 未签名调用 `www.douyu.com/lapi/live/getH5Play/63136` 返回 HTTP 200 和 `error=-9`。
- 现有 Electron UI 的清晰度选择只修改 Zustand 本地状态。它不请求播放源，也不驱动播放器。
- `src/renderer/main.tsx` 当前向 Electron 和浏览器同时注入 3 个 Mock 房间。生产窗口会展示假房间和“模拟画面”。

证据强度：实时响应与本地源码属于高强度证据。斗鱼没有为本项目提供稳定 SDK 或公开播放契约，因此端点长期稳定性未知。

## 3. 目标与非目标

### 3.1 目标

- 为房间查询公开播放能力。
- 区分在线状态与播放源状态。
- 展示公开响应中的清晰度名称，但不把名称当作可播放变体。
- 在 Electron 中移除默认 Mock 房间和模拟播放画面。
- 通过白名单 IPC 暴露播放能力查询。
- 为未来官方或授权播放源保留小型、类型化接口。

### 3.2 非目标

- 不实现 `getH5Play` 签名。
- 不执行或改写斗鱼页面中的混淆 JavaScript。
- 不嵌入斗鱼官方网页或播放器页面。
- 不猜测 FLV/HLS URL、清晰度后缀或 CDN 参数。
- 不实现 `HTMLVideoElement`、MSE、libmpv、FFmpeg 或转码。
- 不持久化播放 URL、签名、Cookie、设备 ID 或请求头。
- 不改变弹幕实现。

## 4. 方案比较

### 4.1 能力检测与阻塞状态

这是采用方案。应用实现完整的状态、IPC 和 UI，但当前生产适配器返回阻塞结果。

优点：用户看到真实状态；代码可以测试；未来授权来源可接入同一边界。代价：本阶段仍没有视频画面。

### 4.2 只记录研究结论

该方案不改应用。开发成本最低，但 Electron 会继续展示 Mock 房间、模拟画面和无效清晰度选择，用户无法区分真实能力与演示数据。

### 4.3 立即抽象多平台 StreamProvider

该方案先建立通用直播平台层。当前只有一个平台且没有可用播放源，通用抽象无法由真实实现验证，会增加无效接口和测试成本。

## 5. 领域模型

新增以下类型，字段名在 Domain、IPC、Preload 和 Renderer 中保持一致：

```ts
export interface ObservedStreamQuality {
  id: string;
  label: string;
  providerType: number;
}

export interface StreamVariant {
  id: string;
  label: string;
  quality: StreamQuality;
  playbackUrl: string;
  container: 'hls' | 'flv';
}

export type StreamBlockReason =
  | 'ROOM_OFFLINE'
  | 'NO_PUBLIC_SOURCE'
  | 'SIGNATURE_REQUIRED';

export type StreamAvailability =
  | {
      kind: 'available';
      roomId: string;
      variants: StreamVariant[];
      checkedAt: string;
    }
  | {
      kind: 'blocked';
      roomId: string;
      reason: StreamBlockReason;
      observedQualities: ObservedStreamQuality[];
      checkedAt: string;
    };
```

`available` 只允许受信任适配器返回经过白名单验证的 `https` URL。当前 `DouyuHttpAdapter` 不产生该分支。浏览器 Mock 可以在测试和演示模式返回 `available`，但 Electron 生产 IPC 不接受 `mock:` URL。

`blocked.reason` 的判断规则：

- 房间不在线时返回 `ROOM_OFFLINE`。
- 在线房间存在 `multirates` 但没有公开直连时返回 `SIGNATURE_REQUIRED`。
- 在线房间既没有公开直连，也没有可识别的清晰度信息时返回 `NO_PUBLIC_SOURCE`。

## 6. 组件边界

### 6.1 DouyuAdapter

接口新增：

```ts
getStreamAvailability(roomId: string): Promise<StreamAvailability>;
```

`DouyuHttpAdapter` 请求 `https://www.douyu.com/betard/{roomId}`，只读取：

- `room.room_id`
- `room.show_status`
- `room.multirates[].name`
- `room.multirates[].type`

适配器不读取页面脚本，不调用 `getH5Play`。响应缺少必要结构时抛出 `PROTOCOL_CHANGED`；网络和 HTTP 失败复用 `NETWORK_UNAVAILABLE`。

### 6.2 IPC

新增频道：

```ts
playback.getAvailability
```

请求：

```ts
interface GetStreamAvailabilityRequest {
  roomId: string;
}
```

返回：

```ts
type GetStreamAvailabilityResult = IpcResult<StreamAvailability>;
```

IPC 校验 `roomId` 为 1 至 20 位数字。Preload 只新增 `window.appApi.getStreamAvailability(roomId)`，不暴露通用 `invoke`。

### 6.3 Renderer Adapter 与 Store

`createRendererDouyuAdapter()` 在 Electron 中调用 Preload API。浏览器模式继续使用 Mock adapter。

`RoomSession` 新增：

```ts
type PlaybackAvailabilityStatus = 'checking' | 'available' | 'blocked' | 'error';

interface RoomSession {
  playbackAvailabilityStatus: PlaybackAvailabilityStatus;
  streamAvailability?: StreamAvailability;
  playbackError?: string;
}
```

Store 暴露 `refreshStreamAvailability(roomId)`。添加房间后，Store 创建稳定的 `RoomSession`，再启动能力检测。单房间失败只更新该房间，不影响其他房间。

### 6.4 启动数据

Electron 检测到 `window.appApi` 时使用空的 `initialRooms`。Vite 浏览器原型继续加载 3 个 Mock 房间。

这项变化只移除生产窗口中的假数据，不删除 Mock adapter、浏览器演示或现有 UI 测试夹具。

## 7. 数据流

1. 用户通过房间号或主播名搜索真实房间。
2. 用户添加候选房间。
3. Store 创建 `RoomSession`，状态为 `checking`。
4. Renderer Adapter 调用 `window.appApi.getStreamAvailability(roomId)`。
5. Main IPC 调用 `DouyuHttpAdapter.getStreamAvailability(roomId)`。
6. Adapter 读取 `betard` 的在线状态与 `multirates`。
7. Adapter 返回 `blocked`；Store 只更新对应房间。
8. RoomTile 显示真实元数据和阻塞原因。

同一房间重新检查时，Store 保留上一结果，直到新结果返回。旧请求结果不得覆盖已移除房间。

## 8. UI 行为

### 8.1 Checking

画面区域显示加载指示和“正在检查播放源”。标题、主播、分类和在线状态继续可见。清晰度控件禁用。

### 8.2 Blocked

画面区域不渲染 `.signal-scene` 的 Mock 中心标记。它显示：

- 主文案：“暂无合规播放源”
- `SIGNATURE_REQUIRED` 辅助文案：“斗鱼当前只提供需签名的播放接口”
- `NO_PUBLIC_SOURCE` 辅助文案：“斗鱼当前未提供公开直连”
- `ROOM_OFFLINE` 辅助文案：“主播当前未开播”

如果 `observedQualities` 非空，禁用的清晰度菜单显示这些真实标签，例如“蓝光10M”。没有观察结果时显示“不可用”。

移除、主画面、声音焦点和弹幕按钮保持可用；声音按钮在没有播放器时禁用，避免暗示正在输出音频。

### 8.3 Browser Demo

浏览器原型继续显示“模拟画面”和现有质量选项。界面必须保留“模拟画面”标识，不能伪装成真实播放。

## 9. 错误与安全

- Adapter 使用 10 秒超时。
- 所有 URL 固定在代码中的斗鱼 HTTPS 白名单，不接收 Renderer 提供的 URL。
- 日志不记录响应正文、Cookie、签名或播放 URL。
- IPC 错误继续使用固定中文消息。
- `blocked` 是正常业务结果，不作为 `UNKNOWN` 错误处理。
- 响应结构变化返回 `PROTOCOL_CHANGED`，RoomTile 显示“播放能力检查失败”和重试按钮。
- 应用不在网络失败时回退 Mock。

## 10. 测试策略

### 10.1 Domain 与 Adapter

- 在线房间 + `multirates` 映射为 `SIGNATURE_REQUIRED`。
- 离线房间映射为 `ROOM_OFFLINE`。
- 无清晰度信息映射为 `NO_PUBLIC_SOURCE`。
- 清晰度按 `type` 生成稳定 ID，重复项去重。
- 非法响应映射为 `PROTOCOL_CHANGED`。
- HTTP 和超时映射为 `NETWORK_UNAVAILABLE`。
- 测试断言不会请求 `getH5Play`。

### 10.2 IPC 与 Preload

- 频道名称稳定。
- 非数字、空值和超长 roomId 在 Main 前被拒绝。
- Preload 只调用 `playback.getAvailability`。
- `window.appApi` 不暴露 `invoke`、`ipcRenderer` 或 Node 对象。

### 10.3 Store 与 UI

- 新房间经历 `checking -> blocked`。
- 已移除房间忽略迟到结果。
- 一个房间失败不改变其他房间。
- 阻塞房间禁用清晰度与音频按钮。
- Electron 启动不注入 Mock 房间。
- 浏览器模式保留 Mock 房间和“模拟画面”。

### 10.4 运行验证

- 运行 `npm test`、`npm run typecheck` 和 `npm run build`。
- 浏览器回归检查演示模式。
- Electron 实机添加 `63136`，确认显示真实元数据、“暂无合规播放源”和观察到的清晰度标签。
- DevTools 确认 Preload 只增加 `getStreamAvailability`。
- 网络断开时确认单房间显示检查失败，不出现模拟数据。

## 11. 验收标准

- Electron 首屏不显示默认 Mock 房间。
- 添加在线房间后，应用不显示模拟画面。
- 房间 `63136` 的能力检查返回 `blocked`，且不包含播放 URL。
- UI 显示真实 `multirates` 标签并禁用选择。
- 应用运行期间不请求 `getH5Play`。
- 应用不生成签名，不执行斗鱼页面脚本。
- 网络和协议错误不会泄露上游详情，也不会影响其他房间。
- 浏览器原型继续支持现有模拟工作流。

## 12. 后续条件

只有斗鱼提供无需私有签名的公开直连，或用户提供合法授权的正式接口后，项目才进入 PlayerAdapter 实现。届时需要单独设计 URL 白名单、有效期、请求头隔离、播放器状态机、清晰度切换回滚和多路性能预算。
