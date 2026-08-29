# Main Repository Cleanup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 `main` 整理为只包含当前 Qt 原生产品、对应中文说明和必要历史迁移记录的分支。

**Architecture:** 运行时代码、构建入口和发布资料统一保留在 `native`。迁移前的 Electron/TypeScript 源码与设计计划只保留在 `legacy-framework`，`main` 仅保留 Qt 迁移后仍有追溯价值的文档。

**Tech Stack:** Git、CMake、Qt Quick/QML、C++20、libmpv、PowerShell。

---

### Task 1: 清理旧框架文件

**Files:**
- Delete: 根目录旧 Electron/TypeScript 运行时代码、构建配置、测试和脚本。
- Delete: `docs/superpowers/plans/2026-08-07-*.md` 至 `2026-08-19-*.md`。
- Delete: `docs/superpowers/specs/2026-08-07-*.md` 至 `2026-08-19-*.md`。

- [x] **Step 1: 列出删除范围**

Run: `git diff --cached --name-status`

Expected: 删除清单仅包含旧 Electron/TypeScript 文件及 2026-08-24 之前的设计、计划。

- [x] **Step 2: 删除本地生成物**

Run: `Remove-Item -Force native\dependencies.lock.json`

Expected: 该未跟踪锁定文件不再出现在 `git status --short`。

### Task 2: 收紧忽略规则和中文说明

**Files:**
- Modify: `.gitignore`
- Modify: `README.md`
- Modify: `native/README.md`
- Modify: `docs/文件职责索引.md`
- Create: `docs/superpowers/logs/2026-08-29-main-仓库整理日志.md`

- [x] **Step 1: 校验忽略边界**

Run: `git check-ignore -v --no-index native\CMakeLists.txt native\build-output.txt native\dependencies.lock.json`

Expected: `native/CMakeLists.txt` 没有匹配项；诊断文件和生成锁定文件匹配 `.gitignore`。

- [x] **Step 2: 检查文档引用**

Run: `rg -n "Electron|TypeScript|Set-Location|文件职责索引|中文" README.md native\README.md docs\文件职责索引.md`

Expected: README 说明 Qt-only 边界，构建命令从 `native` 目录执行，文件索引覆盖根目录和受维护文档。

### Task 3: 构建与版本库验证

**Files:**
- Verify: `native/CMakeLists.txt`
- Verify: `native/out/build/windows-x64-release`

- [x] **Step 1: 重新配置并运行 Release CTest**

Run: `cmd.exe /d /c 'call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 && set QT_ROOT=D:\Qt\6.8.3\msvc2022_64 && set MPV_ROOT=D:\DouyuMonitor\.worktrees\codex\qt-libmpv-m0\native\sdk\mpv && cd /d D:\DouyuMonitor\.worktrees\codex\qt-libmpv-m0\native && cmake --preset windows-x64-release && ctest --preset windows-x64-release'`

Expected: CMake 配置成功，Release CTest 全部通过。

- [x] **Step 2: 审核提交边界**

Run: `git diff --check; git status --short; git ls-files | Where-Object { $_ -notlike 'native/*' -and $_ -notlike 'docs/*' -and $_ -notin '.gitignore','README.md' }`

Expected: 无空白错误；根目录受跟踪文件只剩 `.gitignore` 和 `README.md`；旧框架文件不在 `main`。

### Task 4: 提交与推送

**Files:**
- Verify: `main`
- Verify: `origin/main`

- [x] **Step 1: 创建整理提交**

Run: `git commit -m "chore: clean main for Qt-only native runtime"`

Expected: 创建提交 `1f5f0a8`。

- [x] **Step 2: 推送 main**

Run: `git push origin main`

Expected: 远端 `main` 从 `9754b47` 更新至 `1f5f0a8`。
