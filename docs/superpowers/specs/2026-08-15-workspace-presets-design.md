# 工作区预设设计

## 目标

为多直播间监看场景提供可命名、可切换的工作区预设。预设保存一组完整的监看配置，加载时按预设恢复直播间、布局和播放设置。

## 已确认范围

- 顶栏增加独立的“工作区”入口。
- 预设保存直播间快照、显示顺序、布局、主画面、主画面比例、音频焦点、清晰度、音量、弹幕开关、全局弹幕设置和房间级弹幕治理覆盖。
- 加载预设时，以预设里的直播间为准。即使房间后来从当前列表移除，也要重新加入当前工作区。
- 房间无法取得有效播放源时保留房间，显示平台阻塞或检查失败状态，不删除预设内容。
- 历史、收藏、分组和其他预设保持不变。
- 删除当前预设只回到“未保存工作区”，不删除房间库或其他用户数据。

## 用户界面

### 顶栏入口

在现有弹幕设置和监控状态入口附近增加工作区按钮。按钮名称使用当前预设名；没有当前预设时显示“未保存工作区”。按钮需要提供 `aria-expanded`、`aria-controls` 和描述性 tooltip。

### 预设面板

面板显示：

- 当前预设名称和未保存变化提示
- 预设列表
- 每项的名称、直播间数量、布局名称和最近更新时间
- 保存当前工作区
- 更新当前预设
- 加载预设
- 重命名预设
- 删除预设

点击加载时，如果当前配置相对当前预设存在用户修改，先显示确认提示。加载完成后关闭面板并显示结果提示。加载失败时保留当前工作区，不清空已有房间。

面板支持 Escape 和点击外部关闭。窄屏使用固定抽屉宽度，不能造成横向溢出或遮挡主要操作。

## 数据模型

```ts
interface WorkspacePreset {
  id: string;
  name: string;
  createdAt: string;
  updatedAt: string;
  rooms: WorkspacePresetRoom[];
  roomOrder: string[];
  layoutId: LayoutId;
  primaryRoomId?: string;
  primaryRoomRatio: PrimaryRoomRatio;
  audioRoomId?: string;
  globalDanmakuEnabled: boolean;
  globalMuted: boolean;
  danmakuSettings: DanmakuSettings;
  danmakuGovernanceOverrides: Record<string, DanmakuGovernanceOverride>;
}

interface WorkspacePresetRoom {
  roomId: string;
  anchorName: string;
  title: string;
  category: string;
  avatarUrl?: string;
  online: boolean;
  status: RoomStatus;
  quality: StreamQuality;
  volume: number;
  danmakuEnabled: boolean;
}
```

运行时字段 `playbackAvailabilityStatus`、播放源、错误信息、检查时间和恢复诊断不进入预设。加载后重新执行现有 metadata/playback 检查。

预设名称去除首尾空白，不能为空，最大长度为 40 个字符；同名预设不允许创建。最多保留 20 个预设，超出时阻止创建并给出明确提示。

## 状态与持久化

在 `WorkspaceState` 增加：

- `workspacePresets: WorkspacePreset[]`
- `activeWorkspacePresetId?: string`
- `saveWorkspacePreset(name: string): string | undefined`
- `updateWorkspacePreset(id: string): boolean`
- `loadWorkspacePreset(id: string): Promise<boolean>`
- `renameWorkspacePreset(id: string, name: string): boolean`
- `deleteWorkspacePreset(id: string): boolean`

工作区快照 schema 从当前版本 4 升到版本 5。旧版本迁移为空预设列表，非法或不完整的预设条目在读取时过滤。当前工作区仍按现有方式自动持久化，预设只额外保存命名快照。

未保存变化使用只包含用户可控字段的规范化指纹比较。在线状态、播放源、错误信息和检查时间变化不触发未保存提示。

## 加载流程

1. 根据预设中的 `roomOrder` 生成目标房间列表，并补上快照中存在但顺序数组遗漏的房间。
2. 用预设快照重建当前房间和房间库条目，不调用会修改历史的普通“新增房间”流程。
3. 恢复布局、主画面、比例、音频焦点、弹幕设置和房间级治理覆盖。
4. 清理不在预设里的当前房间，但保留房间库、历史、收藏和分组中的独立记录。
5. 对恢复后的在线房间触发现有 metadata/playback 检查；未开播、平台阻塞或检查失败的房间继续保留。
6. 将 `activeWorkspacePresetId` 指向加载的预设，并持久化当前工作区。

加载过程中如果预设不存在、数据校验失败或恢复操作抛出异常，返回失败结果并保持加载前的工作区状态。

## 测试与验收

- 持久化：schema v5、旧版本迁移、预设字段 round-trip、非法条目过滤和 20 项上限。
- Store：保存、更新、加载、重命名、删除、同名校验、未保存指纹和恢复已移除房间。
- 加载错误：播放源检查失败不删除房间，恢复失败不破坏当前工作区。
- UI：打开/关闭面板、创建、加载、更新、重命名、删除、确认提示和空状态。
- 响应式：1280px 与 390px 下入口、列表和操作按钮不溢出。
- 回归：现有房间库、历史、收藏、分组、布局、弹幕治理、播放恢复和监控状态测试全部通过。

## 非目标

- 不做云端同步、账号登录或跨设备同步。
- 不改变历史、收藏和分组的语义。
- 不保存播放源 URL、签名参数、Cookie 或其他临时凭据。
