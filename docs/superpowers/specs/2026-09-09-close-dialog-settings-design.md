# 关闭弹窗与设置页设计

## 目标

修复关闭弹窗复选框重叠，增加“取消”操作，并在左侧房间列表底部加入设置入口。设置页承载关闭行为配置和检查更新占位项，沿用 Qt Quick/QML + C++20 + QSettings。

## 交互设计

### 关闭弹窗

- 标题：关闭窗口。
- 行为按钮：进入后台、完全退出。
- 次要按钮：取消。
- 取消只关闭弹窗，不调用 `AppController` 的退出或后台方法。
- “记住我的选择”由显式布局行承载：indicator 与文本是两个独立子项，indicator 使用固定尺寸，文本使用剩余宽度并允许省略/换行。

### 设置入口

- `RoomSidebar` 底部固定显示齿轮图标和“设置”文字。
- 点击后主内容区域切换到设置页，房间列表仍保留。
- 设置页顶部显示返回监控工作区按钮。

### 设置页

#### 关闭行为

- 单选：每次询问、进入后台、完全退出。
- 复选：记住我的选择。
- 更改立即写入 `QSettings`，键名沿用 `window/closeBehavior`。
- “每次询问”清除持久化关闭动作；未勾选记忆时同样清除持久化值。

#### 检查更新

- 提供“检查更新”按钮和状态文本。
- 点击只显示“功能即将上线”，不访问网络、不调用更新服务、不修改更新配置。

## 架构与数据流

- `AppController::closeBehavior` 是关闭策略唯一状态源。
- QML 设置页通过现有 `setCloseBehavior(behavior, persist)` 与 `clearCloseBehavior()` 更新状态。
- `Main.qml` 增加当前视图状态；播放器网格、房间会话和弹幕模型不因切换设置页而销毁。
- `RoomSidebar` 通过信号通知根页面打开设置视图。
- 设置页仅负责展示与用户操作，不复制关闭逻辑。

## 文件范围

- 修改 `native/app/qml/dialogs/CloseBehaviorDialog.qml`
- 修改 `native/app/qml/components/RoomSidebar.qml`
- 修改 `native/app/qml/Main.qml`
- 新增 `native/app/qml/pages/SettingsPage.qml`
- 修改 `native/CMakeLists.txt`
- 视需要扩展 `native/src/ui/app_controller.*` 的设置接口
- 修改/新增 `native/tests/app_controller_test.cpp`、`native/tests/qml_close_regression_test.cpp`

## 验证标准

1. checkbox indicator 与文本边界不重叠，窗口缩放后仍保持间距。
2. 点击取消后弹窗关闭，`quitRequested` 与 `backgroundHosted` 不改变。
3. 左侧设置入口可达，返回按钮恢复监控工作区。
4. 设置项重启前后保持一致，且“每次询问/不记住”会清理旧持久化动作。
5. 检查更新只显示占位提示，不产生网络请求。
