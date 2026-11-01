# CSTG 正式设计系统 v1

确认：2026-10-30 Asia/Shanghai。用户已确认R3当前版本。本规范继承实际评审网页；对应Figma文件和执行状态见FIGMA_STATE.json，尚未创建的设计对象不得视为已经交付。

## 原则与框架

以直播画面和内容查找为主要任务。观看页保持视频网格常驻，不因导航、主题或抽屉操作重建播放器。工具、房间、布局、音频和弹幕设置使用按需浮层；常态不占画面轨道。导航、榜单、房间库和设置保留全部真实能力。

标题栏36px，侧栏80px；侧栏CSTG字标16px，导航/榜单品牌图片40px，线性图标26px。标题栏使用用户提供的日夜图片字标，210×24px容器等比居中。标题栏和侧栏在沉浸观看中隐藏，使用边缘提示和T/Esc访问工具/退出。保留既有应用快捷键，并以输入焦点和弹层优先处理Escape。

## 色彩与Token

唯一机读来源：[design-tokens.json](design-tokens.json)。C1夜间、C3白天共20个颜色角色，由已确认网页canvas输出为sRGB HEX。`bg/surface/raised`用于页面/面板/控件，`line/control-line`区分布局分隔和控件边界；`text/muted/subtle`分三级阅读；`accent/on-accent/accent-soft`用于主操作和选中态。live/warning/error保持文字与图标双重语义。video系列固定深色，白天模式的视频与画内控件仍使用其专用Token。

圆角3px，边框1px，键盘焦点2px。间距阶梯4/8/12/16/24/32px，控件基本高度32px。紧凑表格按内容至少48px，说明或多行身份需要自动增高，不能固定高度裁切长文本。

## 字体与字号

正文14px、控件12px、辅助11px、分组标题16px、页面标题24px。正文和关键数字不得随窗口缩小而继续缩小。Qt使用已验证Windows字体栈（Segoe UI与Microsoft YaHei UI）；Figma服务只提供Inter与Noto Sans SC，采用这两种对应字体并在页面标明平台字体映射。字标直接使用图片，不再用字体模拟；正式Qt验收以Windows真实字体排版为准。

## 公共组件及状态

组件范围：Button（主/次/文字/危险、默认/悬停/按下/禁用/焦点）、IconButton、TextField/Search、ComboBox/Filter、Toggle、Tabs、NavigationItem、StatusBadge、MemberRow、RankRow、MetricCell、Drawer、Dialog、Tooltip、Toast、Empty/Error/Loading、VideoOverlayControls。设计变量先于组件；颜色、间距和圆角按Token绑定。组件对象名与现有自动化接口在可行时保留。

抽屉宽度按内容设320–400px，覆盖当前内容，不修改WorkspaceGrid的宽高；可关闭、Escape关闭、关闭后回焦点。真实播放器的房间、画质、声音焦点与重试状态直接绑定AppController/RoomSession公开模型，不能用网页假数据替换。

错误显示用户可理解的固定提示与可行恢复操作，不显示服务原始诊断、URL、Token等。加载、缺失与异常分别呈现。日夜切换不可重置业务选择、布局、声音或弹幕，主题偏好需独立持久化，不更改workspace schema。

## 验收

1280×800、1366×768、1920×1080及现有960×600最小窗口；两主题、键盘焦点、长文本、全部真实页面与弹层；确认视频网格不受抽屉开关影响。Qt运行检查、烟测和生命周期回归见IMPLEMENTATION_PLAN.md及后续验收记录。网页验证不能代替Qt验证。
