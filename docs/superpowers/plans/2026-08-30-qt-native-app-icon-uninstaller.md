# Qt Native App Icon And Uninstaller Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 Qt 原生应用、安装包和快捷方式应用统一图标，并提供可双击运行、可在 Windows 设置中发现的 `.exe` 卸载器。

**Architecture:** 版本库保存 SVG 设计源，并通过同一几何设计生成多尺寸 `.ico`。CMake 将该资源嵌入主程序与无控制台的 C++ 卸载器；安装脚本复制卸载器、创建卸载快捷方式并登记当前用户卸载项。卸载器确认后清理用户入口、登记项和安装目录。

**Tech Stack:** CMake、C++20、Win32 API、Windows 资源编译器、PowerShell、IExpress。

---

### Task 1: 图标资源与构建接入

**Files:**
- Create: `native/app/assets/douyu_monitor.svg`
- Create: `native/app/assets/douyu_monitor.ico`
- Create: `native/app/resources/app_icon.rc`
- Modify: `native/CMakeLists.txt`
- Modify: `native/scripts/test-installer-script.ps1`

- [x] **Step 1: 写入图标资源缺失的回归检查**

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File native\scripts\test-installer-script.ps1`

Expected: 失败并报告缺少应用图标和资源嵌入检查。

- [x] **Step 2: 创建 B 方案 SVG 和多尺寸 ICO**

实现深橙色直播监看徽章的 SVG，并生成含 `16, 24, 32, 48, 64, 128, 256` 像素图层的 `douyu_monitor.ico`。

- [x] **Step 3: 将图标资源嵌入 Windows 可执行文件**

在 CMake 的 `WIN32` 范围内将 `app_icon.rc` 加入主程序和卸载器目标。

- [x] **Step 4: 运行回归检查**

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File native\scripts\test-installer-script.ps1`

Expected: `installer script regression test passed`。

### Task 2: 原生卸载器和安装器入口

**Files:**
- Create: `native/app/uninstaller_main.cpp`
- Modify: `native/CMakeLists.txt`
- Modify: `native/scripts/build-windows-installer.ps1`
- Modify: `native/scripts/test-installer-script.ps1`

- [x] **Step 1: 写入独立卸载器的失败断言**

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File native\scripts\test-installer-script.ps1`

Expected: 失败并报告缺少 `Uninstall DouyuMonitor.exe`、开始菜单卸载入口或 `HKCU` 卸载登记。

- [x] **Step 2: 实现无控制台 C++ 卸载器**

实现 `wWinMain`，确认后删除当前用户快捷方式和 `HKCU` 登记项，并用隐藏 `cmd.exe` 在自身退出后删除应用目录。

- [x] **Step 3: 更新安装器载荷和注册逻辑**

将卸载器和图标复制到载荷；创建“卸载 DouyuMonitor”开始菜单快捷方式，并写入 `DisplayName`、`DisplayIcon`、`UninstallString`、`InstallLocation` 和 `NoModify/NoRepair`。

- [x] **Step 4: 运行脚本回归检查**

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File native\scripts\test-installer-script.ps1`

Expected: `installer script regression test passed`。

### Task 3: 构建、临时安装验收与文档

**Files:**
- Modify: `README.md`
- Modify: `native/README.md`
- Modify: `docs/文件职责索引.md`
- Modify: `docs/superpowers/logs/2026-08-29-main-仓库整理日志.md`

- [x] **Step 1: 重新构建 Release 和安装包**

Run: `cmake --build --preset windows-x64-release --target douyu_monitor_native douyu_monitor_uninstaller` and `powershell.exe -NoProfile -ExecutionPolicy Bypass -File native\scripts\build-windows-installer.ps1`

Expected: 主程序、卸载器和 `native/out/installer/DouyuMonitor-Setup.exe` 均存在。

- [x] **Step 2: 在临时目录安装并核验入口**

使用 `DOUYU_INSTALL_ROOT`、`DOUYU_SKIP_LAUNCH=1` 执行生成的 `install.ps1`；验证两个可执行文件、两个桌面/主程序快捷方式、开始菜单卸载快捷方式和当前用户卸载登记。

- [x] **Step 3: 清理临时安装、运行 Release CTest 和文档校对**

Run: `ctest --preset windows-x64-release` and `git diff --check`

Expected: `28/28` 或更多测试全部通过，且无空白错误。

- [x] **Step 4: 提交并推送**

Run: `git commit -m "feat: add native app icon and uninstaller"` then `git push origin main`

Expected: 本地工作树干净，`origin/main` 指向新提交。
