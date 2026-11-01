# Qt/QML MCP 接入核查

核查日期：2026-10-29。范围：本机工具、现有构建元数据，以及 `docs/ui-redesign/tool-probe` 隔离项目的只读 MCP 调用。

## 结论

本机 Qt Design Studio 4.8.3 自带的独立 QML MCP 服务器能通过 stdio 完成握手、列出工具并读取隔离 QML 文件。当前 Codex 会话没有直接暴露 Qt/QML MCP 工具；本次独立客户端探针不能证明 Codex 全局接入已经生效。

初次核查的旧缓存指向已移除的 Qt 6.8.3，四项测试在加载阶段失败。2026-10-30 已使用 Qt 6.12.0 与 MSVC 在独立 NMake 目录恢复构建及运行验证；初查失败不能代表新目录结果。具体命令、服务载荷修复及权限对照见 [BASELINE_NATIVE.md](BASELINE_NATIVE.md)，当前重构回归见 [IMPLEMENTATION_REPORT.md](IMPLEMENTATION_REPORT.md)。

## 本机安装证据

| 项目 | 实际检查结果 | 检查方式 |
| --- | --- | --- |
| Qt SDK | `D:\Qt\6.12.0\msvc2022_64`，版本 `6.12.0` | `bin\qmake.exe -query QT_VERSION` 返回 `6.12.0` |
| Qt Creator | `D:\Qt\Tools\QtCreator\bin\qtcreator.exe`，版本 `20.0.2` | EXE 的 FileVersion、ProductVersion；安装清单组件 `qt.tools.qtcreator` 为 `20.0.2-0-202609221200` |
| Qt Design Studio | `D:\Qt\Tools\QtDesignStudio\bin\qtdesignstudio.exe`，版本 `4.8.3` | EXE 的 FileVersion、ProductVersion 均为 `4.8.3`，ProductName 为 `Qt Design Studio`；安装清单组件 `qt.tools.qtdesignstudio` 为 `4.8.3-0-202609070646` |
| QML MCP 服务器 | `D:\Qt\Tools\QtDesignStudio\bin\qml_mcp_server\qml_mcp_server.exe` | 文件存在；实际 stdio 探针成功 |
| QML Designer MCP 插件 | `D:\Qt\Tools\QtDesignStudio\lib\qtcreator\plugins\QmlDesignerMcp.dll` | 文件存在；未检查 GUI 中的启用状态 |
| AI Assistant 界面资源 | `D:\Qt\Tools\QtDesignStudio\share\qtcreator\qmldesigner\aiAssistantQmlSources\AiAssistantView.qml` | 文件存在；资源导入 `AiAssistantBackend`；未验证界面运行 |
| CMake / CTest | 均为 `4.4.3` | 各自的 `--version` |
| Ninja | `ninja.exe` 可从 PATH 解析 | `Get-Command ninja` |
| MSVC | `D:\VSBuildTools`，Visual Studio Build Tools 2026，安装版本 `18.10.12201.205` | `vswhere -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64` |
| 编译器环境 | `VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\cl.exe`、`VC\Auxiliary\Build\vcvars64.bat` 存在 | 文件检查；当前普通 PowerShell 的 PATH 未解析到 `cl` |
| mpv SDK | `native\sdk\mpv\include\mpv\client.h`、`libmpv.lib`、`libmpv-2.dll` 存在 | 文件检查 |

安装清单来源为 `D:\Qt\components.xml`。仅提取上表所需的组件名称和版本。

### 版本名称的区别

本次读取的 Qt 官方 MCP 文档页页眉显示 **Qt Design Studio 4.8.2**。本机 EXE 和安装清单确认的是 **4.8.3**。文档页版本不能替代本机安装版本。MCP 握手返回的 `qml_server / 1.25.0` 是服务器自报信息，也不能当作 Qt Design Studio 产品版本。

## 官方启动契约与本机行为

[Qt 官方 MCP 文档](https://doc.qt.io/qtdesignstudio/ai-assistant-mcp.html) 给出的独立服务器配置为：

- 传输：Std I/O。
- Windows 命令：`qml_mcp_server/qml_mcp_server.exe`。
- 参数：`${PROJECT_PATH}`，用于限定项目操作范围。
- 工具：`read_qml`、`create_qml`、`modify_qml`、`delete_qml`、`move_qml`。

本机执行 `qml_mcp_server.exe --help` 时，进程退出码为 0，但 stderr 报告 `Project path does not exist`，并把 `--help` 当作项目路径。这次检查没有取得帮助文本；不能用退出码 0 将其描述为帮助命令成功。

后续独立探针使用实际存在的项目目录作为第一个参数，验证了该用法。整个过程中没有启动 Qt Design Studio GUI。

## 隔离 stdio 探针

夹具仅包含 [Probe.qml](tool-probe/Probe.qml) 与 [Probe.qmlproject](tool-probe/Probe.qmlproject)。QML 文件只声明一个 `Item` 和字符串标记 `isolated-read-probe`，没有网络、播放、用户设置或业务数据。

验证时的进程启动参数：

```text
command = D:\Qt\Tools\QtDesignStudio\bin\qml_mcp_server\qml_mcp_server.exe
argv[1] = D:\DouyuMonitor\docs\ui-redesign\tool-probe
cwd = D:\DouyuMonitor\docs\ui-redesign\tool-probe
transport = stdin / stdout
```

临时 Python 客户端逐行发送 JSON-RPC。每个服务器请求最多等待 10 秒；等待只读参数最多 45 秒。客户端没有保存原始服务器日志。

| 步骤 | 请求 | 实际返回 |
| --- | --- | --- |
| 1 | `initialize`，客户端协议版本 `2024-11-05` | 同版本；`serverInfo.name = qml_server`，`serverInfo.version = 1.25.0`；服务器声明 tools、resources 能力 |
| 2 | `notifications/initialized` | 已发送；该通知不要求响应 |
| 3 | `tools/list` | 返回 `read_qml`、`create_qml`、`modify_qml`、`delete_qml`、`move_qml` |
| 4 | 根据返回的 schema 调用 `read_qml` | `filepath` 是必填字符串，含义为相对项目根目录的 QML 路径 |
| 5 | `tools/call`：`read_qml`，参数 `{"filepath":"Probe.qml"}` | `isError = false`；返回完整夹具内容，包含 `isolated-read-probe` |
| 6 | 关闭服务器 stdin，等待退出 | 服务器退出码 `0`；临时客户端退出码 `0` |

实际只读调用载荷：

```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "method": "tools/call",
  "params": {
    "name": "read_qml",
    "arguments": { "filepath": "Probe.qml" }
  }
}
```

探针后目录仍只有两个夹具文件。`Probe.qml` 的 SHA-256 为 `A30A65324B95AEABBEE2CF3FFB87C46D48D3CCCFBE51B5EBB717CF5C32F6F3B9`；返回文本与该文件内容相符。

本次没有调用写工具，也没有验证截图、渲染、交互、组件编辑或这些写工具的语义。

## 当前 Codex 会话与配置

检查时可调用工具名称中没有 Qt Creator、Qt Design Studio、QML MCP、Context7 或 Serena 对应工具。已配置的服务器不等于当前会话已加载并可调用的服务器。

只读取 `C:\Users\15070\.codex\config.toml` 的服务器名称、命令文件名及 `enabled` 标志，得到以下结果：

| 服务器名称 | 命令文件名 | enabled |
| --- | --- | --- |
| `open-design` | `Open Design.exe` | 未显式设置 |
| `filesystem` | `npx` | 未显式设置 |
| `notion` | 未配置 command | 未显式设置 |
| `playwright` | `npx` | 未显式设置 |
| `sequential-thinking` | `npx` | 未显式设置 |
| `serena` | `serena` | 未显式设置 |
| `node_repl` | `node_repl.exe` | 未显式设置 |
| `qtcreator` | `npx` | 未显式设置 |

没有发现 Qt Design Studio 独立服务器声明。本表不包含 URL、参数、环境变量值或凭据；也不推断 `enabled` 缺省时的运行状态。

若要让 Codex 直接调用 Qt Design Studio MCP，仍需单独完成注册、重新加载和会话内调用验证。此文档只记录已验证的可执行文件、stdio 契约及隔离范围；本次没有修改全局配置。

## 原生构建与测试基线

源码依据：`native/CMakeLists.txt:3` 定义版本 `0.2.20`，`:5` 要求 C++20，`:28` 开始查找 Qt6 模块，`:40` 开始查找 mpv SDK。`native/CMakePresets.json:16` 的 Release 预设使用 Ninja，并从环境读取 `QT_ROOT`、`MPV_ROOT`。

现有构建目录的检查结果：

- 多个缓存仍引用 `D:\Qt\6.8.3\msvc2022_64`；该目录已不存在。
- `native/out/build/windows-x64-release/CMakeCache.txt:288` 的 `QT_ROOT` 为旧目录；`:366` 的 `Qt6_DIR` 为 `Qt6_DIR-NOTFOUND`。
- Release 目录仍有 Qt6Core.dll，产品版本为 `6.8.3.0`，但没有 Qt6Test.dll。
- `dumpbin /DEPENDENTS quality_policy_test.exe` 列出 Qt6Test.dll、Qt6Core.dll 依赖。
- `native/dependencies.lock.json` 不存在；本次未运行依赖安装脚本。

实际执行的测试检查：

```powershell
# 工作目录 D:\DouyuMonitor\native
ctest --preset windows-x64-release -N
ctest --preset windows-x64-release -R '^(stream_service_protocol_test|douyu_danmaku_protocol_test|quality_policy_test|danmaku_governance_test)$' --output-on-failure --timeout 20
```

| 检查 | 退出码与实际结果 | 结论范围 |
| --- | --- | --- |
| CTest 清单 | 退出 `0`，列出 38 项；警告缺少 `native_self_test_test.exe` | 这是现有生成目录的旧清单，不代表当前源码已完整配置或构建 |
| 四项无界面测试 | CTest 退出 `1`；四项均以 `0xc0000135` 结束；0/4 通过，总耗时 16.05 秒 | 均停在进程加载阶段，没有运行到断言；与缺失 Qt Test、过期 SDK 路径一致，尚未通过重建验证根因 |

四个测试源码均使用 `QTEST_GUILESS_MAIN`。本次没有把 Qt 6.12 的 DLL 混入旧构建，没有重新配置或编译，没有启动原生应用、自测、真实播放源、弹幕或 UI 测试。

若要建立当前源码的有效基线，需要在正确 MSVC 环境中，以现有 Qt 6.12.0 与 mpv SDK 配置新的构建目录，再构建并运行相关测试；本次审计没有执行这些步骤。

## 验证边界

### 三个指定官方入口的补充核查

2026-10-29，主代理通过网页工具实际读取用户给定的三个页面，页眉均为4.8.2；本机4.8.3的文件证据仍以本报告安装表为准。

- [AI assistant](https://doc.qt.io/qtdesignstudio/ai-assistant.html)：描述MCP代理能力，要求配置支持的模型提供方API凭据。本次只检查安装资源，没有读取密钥、验证账号或调用GUI助手。
- [MCP servers](https://doc.qt.io/qtdesignstudio/ai-assistant-mcp.html)：确认stdio、Windows可执行文件、项目目录参数与五项QML操作；本机只读探针结果见上文。
- [Figma tutorial](https://doc.qt.io/qtdesignstudio/figma-mcp-tutorial.html)：桌面流程要求Dev/Full seat，在Figma Dev mode启用MCP，复制客户端实际显示的URL，再在Qt Design Studio添加HTTP服务器。读取选中节点后生成QML是AI解释过程，不保证逐像素还原；图片需另行处理。

本会话Figma账户检查返回Starter/View seat；不据此推断所有Figma MCP工具都不可用，但官方桌面联动前置条件尚未满足验证。未启动Figma桌面MCP、未取得客户端显示的地址、未设置连接、未读取选中节点。没有猜测端口、认证或声称Figma→Qt联动成功。

- 已验证：安装文件和版本、独立服务器 stdio 握手、工具发现、隔离文件读取及退出、现有构建缓存和四项测试的实际失败。
- 待验证：Codex 内直接接入、Qt Design Studio GUI 插件启用状态、QML 写操作、可视预览、当前源码的构建和完整测试、真实斗鱼环境。
- 本次产物仅为此文档和隔离 QML 夹具。没有修改全局配置或 `native` 工程；没有保存敏感输出。

参考：[Qt Design Studio 官方 MCP 文档](https://doc.qt.io/qtdesignstudio/ai-assistant-mcp.html)、[MCP 官方 stdio 传输规范](https://modelcontextprotocol.io/specification/2025-03-26/basic/transports)。官网描述与上表中的本机实际结果分别记录，不据文档推断本机运行状态。
