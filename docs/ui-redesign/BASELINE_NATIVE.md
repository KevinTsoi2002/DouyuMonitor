# 原生 UI 实施前基线

核验日期：2026-10-30（Asia/Shanghai）。仓库：`D:\DouyuMonitor`，分支 `main`，HEAD `968557b`。基线包含当前工作区既有的 QML、C++、测试及 CMake 未提交修改，不等同于干净 HEAD。

本轮四个 QML 目标已在 Qt 6.12.0 / MSVC 环境构建成功；引擎、交互、视觉烟测与关闭回归均取得通过结果。关闭回归须在获准启动子进程的执行环境运行，沙箱内的失败对照保留在下文。服务包名册资源过期已通过正式流程重打包、校验并推广到标准目录，原包有完整备份。本报告记录的是 UI 实施前已经生成的测试程序，不证明随后 UI 修改已经通过。

本基线任务未修改生产源码、测试断言、打包脚本或全局配置，未初始化 Serena，未请求真实直播源。产物限于本报告、忽略构建目录、服务包及备份。现有视觉测试按项目定义更新 `native/out/verification/qml` 中的 PNG；未删除输出。

## 已确认环境

| 工具 | 本轮实际路径 / 版本 | 验证 |
| --- | --- | --- |
| Qt SDK | `D:\Qt\6.12.0\msvc2022_64` / 6.12.0 | `qmake.exe -query QT_VERSION` 返回 `6.12.0` |
| Qt Core / Test / 平台插件 | 同 SDK 的 `Qt6Core.dll`、`Qt6Test.dll`、`qwindows.dll`、`qoffscreen.dll` 均为 6.12.0.0 | 文件版本信息 |
| CMake / CTest | `C:\Program Files\CMake\bin` / 4.4.3 | 两者 `--version` |
| Ninja | `C:\Users\15070\AppData\Local\Microsoft\WinGet\Packages\Ninja-build.Ninja_Microsoft.Winget.Source_8wekyb3d8bbwe\ninja.exe` / 1.13.2 | `Get-Command ninja`、`ninja --version` |
| Visual Studio | `D:\VSBuildTools` / Developer PowerShell 2026 v18.10.0 | `vswhere` 和启动横幅 |
| MSVC | `D:\VSBuildTools\VC\Tools\MSVC\14.51.36231\bin\HostX64\x64\cl.exe` / 19.51.36257 | `Get-Command cl`、`cl /Bv`；后者因未给源文件另报 D8003，仅作为版本探针 |
| Windows SDK | 10.0.26100.0 | CMake 检测到该目录下 `rc.exe`、`mt.exe` |
| mpv SDK | `D:\DouyuMonitor\native\sdk\mpv` | `include\mpv\client.h`、`libmpv.lib`、`libmpv-2.dll` 存在 |

普通 PowerShell 未解析到 `cl`；必须先导入当前安装的开发者环境。`msvc2022_64` 是 Qt 套件目录名，本机实际编译器安装版本如上，不应据目录名写成 Visual Studio 2022。

`native/CMakeLists.txt` 当前版本为 `0.2.20`、C++20。默认配置输出 `Phase16 (16 rooms, 16 decoded)`，`DOUYU_ROOM_CAPACITY_PHASE24=OFF`，关闭回归测试使用 `DOUYU_PERF_ROOM_COUNT=16`。这是当前工作区事实，与旧容量说明有差异；本轮未改变容量配置。

## 与上轮审计的区别

[MCP_SETUP.md](MCP_SETUP.md) 记录旧 Qt 6.8.3 构建缓存、缺少 Qt6Test.dll，以及四项测试以 `0xc0000135` 在加载阶段退出。[UI_AUDIT.md](UI_AUDIT.md) 只进行源码审计，没有执行 QML 测试。本轮不重跑混用旧 DLL 的目录，也没有把 Qt 6.12 DLL 复制到旧 Qt 6.8.3 产物旁。

## 可复现配置

以下命令在新的 PowerShell 子进程中执行，工作目录均为 `D:\DouyuMonitor\native`；环境设置只对该进程及其子进程有效。

```powershell
& 'D:\VSBuildTools\Common7\Tools\Launch-VsDevShell.ps1' `
    -Arch amd64 -HostArch amd64 -SkipAutomaticLocation
$env:QT_ROOT = 'D:\Qt\6.12.0\msvc2022_64'
$env:MPV_ROOT = 'D:\DouyuMonitor\native\sdk\mpv'
```

首次按照现有 Release 预设使用 Ninja，并仅覆盖构建目录：

```powershell
cmake --preset windows-x64-release -B out/build/ui-redesign-baseline-qt612
```

CMake 识别 MSVC 后停在 `Detecting CXX compiler ABI info` 超过 2 分钟。生成目录中已有 ABI `.obj` 和 PDB，Ninja 进程 CPU 为 0、未产生链接结果；未取得成功退出码。本轮停止自己启动的 CMake/Ninja 两个进程，保留目录。通过 CIM 查询额外进程信息被系统拒绝访问，未提升权限。不能把这次停滞归因于项目源码；Ninja 的具体根因尚未确认。

更换生成器，其他 SDK、编译器和 Release 配置保持一致，并使用第二个新目录：

```powershell
cmake -S . -B out/build/ui-redesign-baseline-qt612-nmake `
    -G 'NMake Makefiles' -DCMAKE_BUILD_TYPE=Release `
    '-DQT_ROOT=D:/Qt/6.12.0/msvc2022_64' `
    '-DMPV_ROOT=D:/DouyuMonitor/native/sdk/mpv'
```

实际退出码 `0`；配置 60.7 秒、生成 2.9 秒。编译器 ABI、C++ 特性、Qt 和 mpv 检测通过。`WrapVulkanHeaders` 缺失是本次配置中的非致命提示，未阻止生成。缓存确认 `Qt6_DIR` 指向 6.12.0，构建类型为 Release。

## 构建与测试记录

```powershell
cmake --build out/build/ui-redesign-baseline-qt612-nmake --target `
    qml_engine_smoke_test qml_interaction_test `
    qml_visual_smoke_test qml_close_regression_test
```

首次构建在完成 `douyu_qml.lib` 后退出 `1`：`douyu_qmlplugin` 的 AutoMoc 启动 `moc.exe` 时报告 `libuv process spawn failed: operation not permitted`。此前 QML 缓存及其 C++ 编译已完成，没有出现源码错误诊断。

随后针对同一目录、同一四目标命令请求构建子进程权限复核，自动审批允许执行；恢复后同一 AutoMoc 步骤通过。这一对照支持该中断来自执行环境的子进程限制，未改源码或手工生成 moc 产物。恢复后的构建命令最终退出 `0`，四个目标全部链接成功。

构建中 `tests/fake_streamget_service.cpp:181` 出现 C4834（忽略 `[[nodiscard]]` 返回值）警告；未按此警告改动测试源码。部分 `/showIncludes` 输出存在控制台编码乱码，未影响本次 NMake 编译。

测试分别在目标生成后运行，未同时运行多个 QML 测试进程：

```powershell
ctest --test-dir out/build/ui-redesign-baseline-qt612-nmake `
    -R '^qml_engine_smoke_test$' --output-on-failure --timeout 120
ctest --test-dir out/build/ui-redesign-baseline-qt612-nmake `
    -R '^qml_interaction_test$' --output-on-failure --timeout 120
ctest --test-dir out/build/ui-redesign-baseline-qt612-nmake `
    -R '^qml_visual_smoke_test$' --output-on-failure --timeout 120
ctest --test-dir out/build/ui-redesign-baseline-qt612-nmake `
    -R '^qml_close_regression_test$' --output-on-failure --timeout 180
```

| CTest 项目 | 实际结果 | 后端 / 范围 |
| --- | --- | --- |
| `qml_engine_smoke_test` | 退出 `0`；1/1 通过；3.67 秒 | CTest 明确指定 offscreen；Main 资源、模块与 Theme 单例加载 |
| `qml_interaction_test` | 退出 `0`；1/1 通过；27.19 秒 | CTest 明确指定 Windows；现有交互断言 |
| `qml_visual_smoke_test` | 退出 `0`；1/1 通过；26.71 秒 | 未额外覆盖平台；现有几何/截图烟测；不代表截图已人工验收 |
| `qml_close_regression_test` | 权限对照后退出 `0`；1/1 通过；24.03 秒 | CTest 明确指定 Windows、16 路配置；本地假服务与播放器生命周期；见下文沙箱差异 |

CTest 的 Qt 运行环境由新目录的 `CTestTestfile.cmake` 设置：`PATH` 加入 Qt 6.12 的 `bin`，`QT_PLUGIN_PATH`、`QML2_IMPORT_PATH` 均指向同 SDK，并使用 Basic 控件样式。本轮没有更改测试定义或关闭断言。

视觉烟测按现有 `QML_VERIFICATION_DIR` 定义写入 `native/out/verification/qml`。本轮新时间戳的输出包括空壳、单/四/九/十六路示例、960×600 至 1920×1080 的外壳与面板截图；本基线任务只确认测试运行及文件生成，未声称人工视觉验收或性能达标。测试源还有 24 路图形夹具，这不改变本次原生配置的 16 路默认容量。

## 关闭回归的执行环境对照

首次完整日志 `native/out/build/ui-redesign-baseline-qt612-nmake/qml_close_regression-baseline.txt` 显示 7 passed、3 failed。失败分别位于 `closesAttachedPlayersWithoutLingeringCallbacks`、`removesAttachedPlayersWhileWindowRemainsOpen` 和 `appliesPresetAndRefreshesAllRoomDelegates` 的 `serviceProcessRunningForTest()` 断言，且伴随 `QProcess: CreateFile failed` 拒绝访问；不是 OpenGL 上下文断言失败。

未重新编译，先以同一测试程序、Qt 路径、Windows 后端和 16 路环境运行第一项：沙箱内退出 `1`，2 passed、1 failed，24.601 秒，重复在服务启动断言失败。随后仅请求工具执行权限复核，自动审批允许后，同一项退出 `0`，3 passed、0 failed，7.125 秒。证据分别为 `qml-close-service-sandbox-check.txt` 与 `qml-close-service-permission-check.txt`。这个单变量对照支持沙箱子进程限制是本次失败原因，不需要修改服务客户端或关闭断言。

完整复核命令也经自动审批允许，在 `native` 中执行：

```powershell
ctest --test-dir out/build/ui-redesign-baseline-qt612-nmake `
    -R '^qml_close_regression_test$' --output-on-failure --timeout 180 `
    --output-log out/build/ui-redesign-baseline-qt612-nmake/qml-close-baseline-permission-ctest.txt
```

实际退出 `0`、1/1 通过、24.03 秒。未降低房间数量、切换 offscreen、删除渲染检查或弱化任何断言。单项通过日志仍有引擎销毁时的 QML delegate 创建中警告；通过结果不能证明该警告不存在。

## 完整应用载荷的独立阻塞与修复

应用目标的 POST_BUILD 会调用 `cmake/copy_streamget_service.cmake`，其中先执行服务包 provenance 校验。对既有包直接运行同一只读校验：

```powershell
cmake '-DPACKAGE_DIR:PATH=D:/DouyuMonitor/native/out/service/streamget_service' `
    '-DNATIVE_ROOT:PATH=D:/DouyuMonitor/native' `
    -P cmake/verify_streamget_service_package.cmake
```

实际退出码 `1`，报告：

```text
StreamGet provenance source hash mismatch:
app/resources/hamster_agent_roster.json
```

旧包最后写入时间为 2026-10-22。逐项比对 manifest 的 11 个输入，只有名册文件不一致，服务源码、依赖锁和打包脚本等其余 10 项一致。旧包内嵌名册哈希与其 manifest 均为 `bf48d8f8ef4d3639511d87cae50d3b8ba9e306e282ebee12dad35d8ca61e4ff7`，当前工作区名册为 `a0613cedd596a96685d2b985260a307237be1dbc9f4780943571197b852a1fd6`。因此校验准确阻止了过期资料载荷；没有改写旧 manifest 或绕过校验。

### 可逆重打包

先将标准旧包完整复制到 `native/out/service-backups/streamget_service-ui-redesign-20261030-001`，逐个比对 114 个文件的 SHA-256 和文件总数，全部一致，总大小 24,426,398 字节。未覆盖已有备份。

锁校验 `native/.venv/Scripts/python.exe native/service/verify_dependency_lock.py` 退出 `0`。正式打包脚本在新路径运行，未更改虚拟环境或脚本：

```powershell
# 工作目录 D:\DouyuMonitor\native
.\scripts\build-streamget-service.ps1 `
    -DistPath 'D:\DouyuMonitor\native\out\service-ui-redesign-20261030-001' `
    -WorkPath 'D:\DouyuMonitor\native\out\pyinstaller-ui-redesign-20261030-001'
cmake '-DPACKAGE_DIR:PATH=D:/DouyuMonitor/native/out/service-ui-redesign-20261030-001/streamget_service' `
    '-DNATIVE_ROOT:PATH=D:/DouyuMonitor/native' `
    -P cmake/verify_streamget_service_package.cmake
```

两条命令均退出 `0`。新 manifest 记录 CPython 3.13.15、StreamGet 4.0.10、PyInstaller 6.22.0、pip 26.2.1，以及当前 dirty worktree。对隔离包 EXE 通过 stdin 依次发送正整数 requestId 的 `ping` 和 `shutdown`；20 秒限时内退出 `0`，收到准确的 `pong: true` 与 `shutdown: true` 响应，stderr 为 0 字节。该探针没有发出 resolve/search/status 请求。

确认隔离包后，将标准旧目录改名保留为 `native/out/service/streamget_service.pre-ui-redesign-20261030-001`，再通过正式复制脚本推广：

```powershell
cmake '-DSERVICE_SOURCE:PATH=D:/DouyuMonitor/native/out/service-ui-redesign-20261030-001/streamget_service' `
    '-DSERVICE_DEST:PATH=D:/DouyuMonitor/native/out/service' `
    -P cmake/copy_streamget_service.cmake
```

实际退出 `0`；脚本对源包和目标包的 provenance 均验证通过。标准目录现在包含当前名册资源。独立备份、原目录保留件与隔离候选包均未删除。

### 服务包回归检查

`powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\scripts\test-installer-script.ps1` 退出 `0`，输出 `installer script regression test passed`。

`python -m unittest native.service.tests.test_package_provenance native.service.tests.test_dependency_lock` 首次在沙箱内得到 7 个环境错误，均发生在 Python 临时目录的子文件创建/清理权限上。仅将 `TEMP`、`TMP` 与测试已支持的 `DOUYU_STEP47_TEMP` 指向仓库内隔离路径后仍出现同类拒绝访问。保持该环境与测试不变，经工具执行权限复核允许后重跑，退出 `0`、12 tests、2.350 秒、`OK`。测试覆盖相符源与载荷、拒绝过期源/变化载荷、复制前后校验、依赖闭包等；未改测试实现。

## 验证边界与建议

- 后续 UI 编译可复用 `native/out/build/ui-redesign-baseline-qt612-nmake` 和上述进程环境；不要直接复用过期 Qt 6.8.3 目录。
- 标准服务包已经通过正式重打包、复制前后 provenance、本地 ping/shutdown 和相关 12 项回归检查；完整应用重新构建、自测和安装载荷验收仍需在 UI 修改稳定后执行。
- 本轮尚不能证明完整 CTest、原生应用部署、安装包、真实斗鱼播放/弹幕、长时性能或视觉效果通过。
- Notion 待主代理同步；本报告不是已经同步的证明。
