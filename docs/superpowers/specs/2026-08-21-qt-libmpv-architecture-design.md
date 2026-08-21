# Qt + libmpv 架构重构开发设计 v0.1

> 文档状态：开发设计 v0.1（D 方案）。关联文档：性能优化计划 v1.0、项目开发设计 v0.1、需求分析 v0.1。

## 1. 背景与决策依据

现有 Electron + React + mpegts.js 1.8.1 架构在 9 路直播场景下存在两个无法在框架内解决的限制：

- mpegts.js 的 worker 依赖 webpack 分 chunk 构建；在 Vite 打包与 UMD 环境下 webworkify 入口指向主库导出对象而非 worker 构造函数，`enableWorker: true` 必然崩溃。
- 解复用、MSE 追加全部跑在渲染主线程，9 路并发时主线程成为硬瓶颈；弹幕 DOM 与 MediaSource 缓冲在高峰时放大 CPU 与内存压力。

结论：保留 Electron 壳只能通过 WebCodecs 自建解码管线获得同等收益，但工程风险更高。D 方案选择 Qt + libmpv 原生内核，把解码与渲染移出浏览器进程模型，换取确定性的 CPU、内存与低延迟控制。

## 2. 目标与非目标

### 2.1 目标

- 9 路 1080p 直播同时播放，总 CPU 占用明显低于现 Electron 版本。
- 单路首帧小于 3 秒，断流自动恢复小于 5 秒，画质切换小于 2 秒。
- 弹幕峰值 30 条/秒/房间不掉帧，9 路叠加无 DOM 堆积。
- 4 小时长稳运行无持续内存增长。
- 保留现有功能面：房间搜索、分组、历史、布局、画质策略、弹幕设置、系统通知。

### 2.2 非目标

- 不追求跨平台（首个版本仅 Windows x64）。
- 不在本阶段做移动端或浏览器版本。
- 不保留 Electron 渲染进程作为主运行形态；Electron 版本仅作对照基线。

## 3. 总体架构

### 3.1 分层

```text
┌──────────────────────────────────────────────┐
│ UI Layer（Qt Widgets + QOpenGLWidget）        │
│ 主窗口 / 房间网格 / 侧边栏 / 弹幕 Overlay     │
├──────────────────────────────────────────────┤
│ Application Layer（QObject / Controller）     │
│ WorkspaceController / DanmakuController       │
│ QualityPolicy / NotificationService           │
├──────────────────────────────────────────────┤
│ Media Layer（libmpv）                         │
│ PlayerController x9 / hwdec / render context  │
├──────────────────────────────────────────────┤
│ Infrastructure Layer                          │
│ DouyuHttpAdapter / DanmakuSocket /           │
│ StreamGetSidecar / StreamProxy               │
└──────────────────────────────────────────────┘
```

### 3.2 进程模型

- 单主进程承载 UI 与 9 路播放器（mpv 多 context 支持）。
- 每路播放器一个独立 `QThread` 与 `mpv_render_context`，避免单路阻塞影响全局。
- 弹幕 WebSocket 与 StreamGet sidecar 为独立线程；UI 线程只做状态发布。

## 4. 技术选型

| 组件 | 选型 | 说明 |
| --- | --- | --- |
| 框架 | Qt 6.8 LTS（Widgets） | 成熟稳定，窗口嵌入成熟 |
| 语言 | C++20 / MSVC 2022 | 与 Qt/CMake 生态一致 |
| 构建 | CMake + vcpkg | 依赖可复现 |
| 播放内核 | libmpv（mpv 0.38+） | 原生解码、直播调优成熟 |
| 渲染 | QOpenGLWidget + mpv_render_context | d3d11 硬件解码路径 |
| 弹幕渲染 | QPainter 叠加纹理 | 帧驱动，无 DOM |
| WebSocket | Qt WebSockets | 弹幕协议接入 |
| 持久化 | QSettings + JSON 快照 | 复用现有 schema |
| 测试 | GoogleTest + Qt Test | 单元与集成 |
| 性能基准 | 独立 Python 采样脚本 | 与现 baseline 对标 |

## 5. 目录与模块设计

```text
douyu-monitor-native/
├─ CMakeLists.txt
├─ src/
│  ├─ app/            # 入口、生命周期、单实例
│  ├─ core/           # 领域层移植
│  │  ├─ room-registry.h
│  │  ├─ layout-engine.h
│  │  ├─ input-resolver.h
│  │  └─ quality-policy.h
│  ├─ media/          # libmpv 封装
│  │  ├─ player-controller.h
│  │  ├─ player-surface.h
│  │  └─ render-thread.h
│  ├─ danmaku/        # 协议、调度、渲染
│  │  ├─ protocol-parser.h
│  │  ├─ lane-scheduler.h
│  │  ├─ danmaku-overlay.h
│  │  └─ message-store.h
│  ├─ infra/          # HTTP、WS、sidecar、代理
│  │  ├─ douyu-http-adapter.h
│  │  ├─ danmaku-socket.h
│  │  ├─ streamget-sidecar.h
│  │  └─ stream-proxy.h
│  ├─ ui/             # Widgets
│  │  ├─ main-window.h
│  │  ├─ workspace-grid.h
│  │  ├─ room-sidebar.h
│  │  └─ settings-panel.h
│  └─ persistence/    # 快照读写
├─ tests/
├─ scripts/
│  ├─ streamget_bridge_server.py   # 复用现有 sidecar
│  └─ perf-baseline.py
└─ third_party/
```

## 6. 播放内核设计

### 6.1 mpv 实例配置

- 每房间一个独立 `mpv_handle`，线程模型：播放线程 + 渲染线程分离。
- 直播参数：`cache=yes`、`demuxer-max-bytes=64MiB`、`stream-buffer-size=2MiB`。
- 解码：`hwdec=d3d11va`，失败自动回退 `hwdec=no`（软解兜底）。
- 延迟：`video-timing-offset` 自适应，`audio-buffer=0.5`，避免多路音画不同步。

### 6.2 渲染与画面合成

- `PlayerSurface` 继承 `QOpenGLWidget`，用 `mpv_render_context_render` 输出帧。
- 每路叠加弹幕层：QPainter 在 RGBA 纹理上绘制后与视频帧合成，或使用独立 Qt Quick 项叠加。
- 主次布局沿用现有规则：单路 100%、2x2、3x2、3x3、主次比例可调。

### 6.3 音视频策略

- 默认多路静音，仅主房间出音；单路模式自动恢复音量。
- 音量、静音状态按房间持久化，沿用现有 `room-volume` 语义。

## 7. 弹幕子系统设计

- 协议解析移植现有 `socket.ts` 逻辑：WebSocket 二进制帧到消息结构，复用零拷贝解析思路。
- 去重与 pending 队列：移植 `danmaku-store` 的去重表与上限策略。
- Lane 调度：移植已完成的 per-lane 队列 + 二分查找，无 React 调度开销。
- 渲染：每房间一个 `DanmakuOverlay`，QTimer 16ms 帧驱动，QPainter 绘制，控制每路最大活跃弹幕数（如 60）。
- 高峰降级：超过阈值时合并/丢弃低优先级消息，保证渲染帧率稳定。

## 8. 数据流与状态管理

- 状态模型：`WorkspaceModel`（QObject）承载房间列表、布局、设置；`QAbstractListModel` 驱动 UI。
- 更新语义：按 `roomId` 增量更新，广播仅通知受影响视图，避免全量刷新。
- 持久化：内存模型变更后 500ms 防抖写入 JSON 快照，schema 兼容现有 `WORKSPACE_SCHEMA_VERSION`。

## 9. StreamGet sidecar 集成

- 直接复用 `scripts/streamget_bridge_server.py` 与 PyInstaller 产物。
- `StreamgetSidecar`（QProcess + JSON-RPC over stdio）负责启动、请求、超时、崩溃重启。
- 并发上限 2，保持现有 resolution queue 语义。
- 打包时随应用分发 `streamget_bridge_server.exe` 与 Python 运行时依赖。

## 10. UI/UX 要点

- 无边框主窗口 + 自定义标题栏，复用现有最小化/最大化/关闭交互。
- 侧边栏：直播间列表、收藏、历史、分组管理；保持当前信息密度。
- 房间卡片：状态、在线人数、画质、静音、移除；右键菜单沿用现有操作面。
- 设置面板：弹幕开关/密度/字号、默认布局、画质策略、开机自启。

## 11. 性能指标与验收

### 11.1 基准配置

- 8 核 x64 Windows 11、9 路 1080p 直播、弹幕高峰。

### 11.2 指标

| 指标 | 目标 |
| --- | --- |
| 9 路总 CPU | 低于 Electron 基线 50% 以上 |
| 峰值内存（9 路） | 不超过 700 MB |
| 单路首帧 | 不超过 3 s |
| 断流自动恢复 | 不超过 5 s |
| 画质切换 | 不超过 2 s |
| 弹幕渲染 | 30 条/秒/房间不掉帧 |
| 长稳 | 4 小时无持续内存增长 |

### 11.3 验收流程

- 等价功能回归清单与需求分析逐项对照。
- 性能采样脚本对标现 `performance-baseline`。
- 崩溃恢复、断网重连、代理异常注入测试。

## 12. 迁移路线与里程碑

- M0 环境与原型（2-3 人日）：Qt/vcpkg/mpv 集成跑通单路。
- M1 应用骨架（5-7 人日）：主窗口、生命周期、单实例、自定义标题栏。
- M2 多路网格（5-8 人日）：9 路 PlayerSurface、布局、静音策略、质量切换。
- M3 弹幕（6-10 人日）：协议、调度、渲染、设置。
- M4 房间与持久化（5-8 人日）：搜索、收藏、历史、分组、快照。
- M5 基础设施（4-6 人日）：StreamGet sidecar、通知、代理、错误恢复。
- M6 性能与验收（5-8 人日）：基准、调优、长稳、回归。

合计 32-50 人日，可并行拆分 UI 与媒体两条线。

## 13. 测试策略

- 单元：GoogleTest 覆盖解析器、Lane 调度、布局、质量策略、快照序列化。
- 集成：Qt Test 覆盖控制器状态机、sidecar 交互、断流恢复。
- 性能：Python 脚本采集 CPU/内存/帧率，与 Electron 基线对比。
- E2E：手动验收清单 + 自动化冒烟（启动、加房、切布局、关闭）。

## 14. 风险与缓解

| 风险 | 影响 | 缓解 |
| --- | --- | --- |
| Qt LGPL 合规 | 分发受限 | 动态链接 Qt，遵循 LGPL 通知要求 |
| d3d11 硬件解码兼容性 | 部分 GPU 黑屏 | 软解自动回退 + 启动自检 |
| 远程桌面/虚拟显卡 | 渲染异常 | 检测后切换软件渲染 |
| 团队 C++ 技能 | 进度风险 | 先原型后扩展，核心模块小步交付 |
| 斗鱼接口变更 | 播放/弹幕失效 | 复用现有 sidecar 与协议层，集中适配 |
| 双栈并行维护 | 成本增加 | Electron 仅保留基线，冻结新功能 |

## 15. 决策记录

- ADR-001：选 Qt Widgets 而非 QML。理由：窗口嵌入与 9 路视频 surface 更直接，团队可预测性更高。
- ADR-002：选 libmpv 而非 VLC。理由：直播低延迟参数更细、单实例轻量、API 更贴近播放器内核。
- ADR-003：播放直连斗鱼签名 URL，代理作为可选开关。理由：mpv 无 CORS 限制，减少一层转发开销；代理保留用于调试与未来鉴权。
- ADR-004：弹幕自绘而非 QML Canvas。理由：QPainter 在 QOpenGLWidget 上开销可控，行为与现有算法一一对应。
