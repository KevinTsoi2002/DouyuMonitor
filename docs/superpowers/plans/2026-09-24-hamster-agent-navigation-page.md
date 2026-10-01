# 仓鼠特工导航页 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在房间列表入口右侧增加一个可收起的“仓鼠特工导航页”，按队伍展示公会主播并支持快捷加入直播间；队伍管理独立放在设置页，不进入导航页。

**Architecture:** 新增独立的公会名单、队伍和房间号解析领域模型，不复用会影响活动房间集合的旧 `groups` 激活语义。导航页只读取已打包的公会名单、队伍和房间号缓存；设置页负责新建、重命名、排序、删除队伍及调整成员。导航页和房间列表共用左侧面板区域，两者互斥且沿用现有持久化习惯。

**Tech Stack:** Qt 6.8、C++20、Qt Quick/QML、Qt Test、CMake/CTest、现有 `AppController` / `StreamgetProcessClient` 边界。

---

## 锁定决策

- 导航入口位于 `sidebarToggleButton` 右侧、品牌图标左侧。
- 入口图标使用用户提供的 `rs1440.jfif`，运行时从 QML 资源加载，不引用桌面路径。
- 点击导航入口打开或关闭导航页；打开导航页时收起房间列表，打开房间列表时关闭导航页。
- 导航页和房间列表的开关状态都写入工作区快照并在下次启动恢复。
- 队伍是本功能的主分组；导航页按队伍顺序展示，未分配主播放在最后的“未分队”区域。
- 队伍管理入口只放在设置页，导航页不出现新建、重命名、删除或成员分配控件。
- 导航页只展示打包的公会主播名单，不根据 `clubOrgName` 动态扩张列表。
- 名单中的“团长”“队长”“队员”“OB”只用于清理搜索词，不持久化，也不在导航页显示。
- 默认一名主播只能属于一个队伍；未分配主播不属于任何队伍。
- 无法自动确认的房间号显示“待确认”，用户可以手工输入准确房间号；不会静默采用模糊搜索结果。

## 文件结构

**新增资源**

- `native/app/resources/hamster_agent_roster.json`：打包的公会主播名单，使用稳定成员 ID；仅保存公开主播名和已确认房间号。
- `native/app/qml/assets/icons/hamster-agent.jfif`：导航入口图标资源。

**新增领域与适配文件**

- `native/src/workspace/guild_roster.h`：公会名单条目和名单查询接口。
- `native/src/workspace/guild_roster.cpp`：读取内置 JSON、清理角色后缀、提供稳定成员 ID。
- `native/src/workspace/guild_room_resolver.h`：异步解析并缓存公会成员房间号。
- `native/src/workspace/guild_room_resolver.cpp`：串行请求、退避、缓存和人工确认。

**新增界面文件**

- `native/app/qml/components/GuildNavigationPanel.qml`：队伍分组导航页和快捷加入。
- `native/app/qml/components/GuildMemberRow.qml`：单个公会主播行，避免导航页文件持续膨胀。
- `native/app/qml/dialogs/TeamManagerDialog.qml`：设置页打开的队伍管理界面。

**修改文件**

- `native/src/workspace/native_workspace_types.h`
- `native/src/workspace/native_workspace_store.cpp`
- `native/src/ui/app_controller.h`
- `native/src/ui/app_controller.cpp`
- `native/src/ui/workspace_model.h`
- `native/src/ui/workspace_model.cpp`
- `native/app/qml/components/AppHeader.qml`
- `native/app/qml/Main.qml`
- `native/app/qml/pages/SettingsPage.qml`
- `native/CMakeLists.txt`
- `native/tests/guild_roster_test.cpp`
- `native/tests/guild_room_resolver_test.cpp`
- `native/tests/native_workspace_store_test.cpp`
- `native/tests/app_controller_test.cpp`
- `native/tests/workspace_model_test.cpp`
- `native/tests/qml_interaction_test.cpp`
- `docs/文件职责索引.md`
- `docs/superpowers/logs/2026-09-24-hamster-agent-navigation.md`

### Task 1: 建立公会名单资源和领域边界

**Files:**
- Create: `native/app/resources/hamster_agent_roster.json`
- Create: `native/src/workspace/guild_roster.h`
- Create: `native/src/workspace/guild_roster.cpp`
- Create: `native/tests/guild_roster_test.cpp`
- Modify: `native/CMakeLists.txt`

- [ ] **Step 1: 写失败测试，锁定名单清洗规则**

```cpp
#include <QtTest/QtTest>

#include "workspace/guild_roster.h"

class GuildRosterTest final : public QObject {
    Q_OBJECT

private slots:
    void loadsBundledRosterWithoutRoleLabels();
    void stripsRoleSuffixOnlyForSearch();
    void rejectsInvalidRoomIdsFromResource();
};

void GuildRosterTest::loadsBundledRosterWithoutRoleLabels()
{
    const QVector<GuildMember> members = GuildRoster::bundled();
    QCOMPARE(members.size(), 47);
    for (const GuildMember &member : members) {
        QVERIFY(!member.id.isEmpty());
        QVERIFY(!member.anchorName.isEmpty());
        QVERIFY(!member.anchorName.contains(QStringLiteral("-团长")));
        QVERIFY(!member.anchorName.contains(QStringLiteral("-队长")));
        QVERIFY(!member.anchorName.contains(QStringLiteral("-队员")));
        QVERIFY(!member.anchorName.endsWith(QStringLiteral("-OB")));
    }
}

void GuildRosterTest::stripsRoleSuffixOnlyForSearch()
{
    QCOMPARE(GuildRoster::searchName(QStringLiteral("主播阿郎-团长")),
             QStringLiteral("主播阿郎"));
    QCOMPARE(GuildRoster::searchName(QStringLiteral("Zy梓洋-OB")),
             QStringLiteral("Zy梓洋"));
    QCOMPARE(GuildRoster::searchName(QStringLiteral("寅子")),
             QStringLiteral("寅子"));
}

void GuildRosterTest::rejectsInvalidRoomIdsFromResource()
{
    const GuildMember *yinzi = GuildRoster::findByName(QStringLiteral("寅子"));
    QVERIFY(yinzi != nullptr);
    QCOMPARE(yinzi->roomId, QStringLiteral("71415"));
}

QTEST_GUILESS_MAIN(GuildRosterTest)

#include "guild_roster_test.moc"
```

- [ ] **Step 2: 运行测试，确认当前失败**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R guild_roster_test --output-on-failure
```

Expected: 编译失败，`workspace/guild_roster.h` 或 `guild_roster_test` 尚不存在。

- [ ] **Step 3: 创建内置名单资源**

资源 `members` 数组必须包含以下 47 个条目，顺序与此列表一致。已确认的房间号写入 `roomId`，未确认项保持空字符串；`id` 按 `hamster-001` 到 `hamster-047` 顺序编号。

```json
{
  "version": 1,
  "guildName": "仓鼠特工",
  "members": [
    { "id": "hamster-001", "name": "寅子", "roomId": "71415" },
    { "id": "hamster-002", "name": "主播阿飞", "roomId": "84452" },
    { "id": "hamster-003", "name": "午夜抹抹茶", "roomId": "80432" },
    { "id": "hamster-004", "name": "主播阿郎-团长", "roomId": "320155" }
  ]
}
```

资源必须逐项覆盖 `D:\DouyuMonitor\仓鼠特工工会下的主播.txt` 中的全部 47 个非空条目，顺序与该文件一致。每一行的角色后缀只作为搜索清洗来源，不写入 UI 或额外角色字段；已实测确认的房间号写入 `roomId`，其余保持空字符串。不得加入 Cookie、Token、播放地址或原始服务响应。

- [ ] **Step 4: 实现名单领域类型**

`native/src/workspace/guild_roster.h`:

```cpp
#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

struct GuildMember {
    QString id;
    QString anchorName;
    QString searchName;
    QString roomId;

    bool operator==(const GuildMember &) const = default;
};

class GuildRoster final {
public:
    static QVector<GuildMember> bundled();
    static QVector<GuildMember> parse(const QByteArray &json);
    static const GuildMember *findByName(const QString &anchorName);
    static const GuildMember *findById(const QString &memberId);
    static QString searchName(const QString &rawName);
    static QString normalizedName(const QString &rawName);

private:
    static QString normalizedKey(const QString &rawName);
};
```

`native/src/workspace/guild_roster.cpp` 使用 `QJsonDocument::fromJson`，只接受对象根节点、`version == 1`、非空且唯一的 `id`、非空的 `name`、合法或空的十进制 `roomId`。角色清理规则固定为：

```cpp
static const QRegularExpression roleSuffix(
    QStringLiteral(R"(-(?:团长|队长|队员|OB)$)"),
    QRegularExpression::CaseInsensitiveOption);
```

`parse()` 产出的 `anchorName` 和 `searchName` 都已移除该后缀；`GuildMember` 不包含角色字段。

- [ ] **Step 5: 接入 CMake 和资源**

在 `native/CMakeLists.txt` 增加：

```cmake
add_library(douyu_guild STATIC
    src/workspace/guild_roster.cpp
)
target_link_libraries(douyu_guild PUBLIC Qt6::Core)
target_include_directories(douyu_guild PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
qt_add_resources(douyu_guild guild_roster_resources
    PREFIX "/guild"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
        app/resources/hamster_agent_roster.json
)

add_executable(guild_roster_test
    tests/guild_roster_test.cpp
)
target_link_libraries(guild_roster_test PRIVATE
    Qt6::Core
    Qt6::Test
    douyu_guild
)
add_test(NAME guild_roster_test COMMAND guild_roster_test)
```

`GuildRoster::bundled()` 固定读取 `QFile(":/guild/app/resources/hamster_agent_roster.json")`。

- [ ] **Step 6: 运行名单测试**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R guild_roster_test --output-on-failure
```

Expected: `guild_roster_test` 通过。

- [ ] **Step 7: 提交**

```powershell
git add native/app/resources/hamster_agent_roster.json native/src/workspace/guild_roster.h native/src/workspace/guild_roster.cpp native/tests/guild_roster_test.cpp native/CMakeLists.txt
git commit -m "feat: add packaged hamster agent guild roster"
```

### Task 2: 增加独立队伍模型、持久化和控制器命令

**Files:**
- Modify: `native/src/workspace/native_workspace_types.h`
- Modify: `native/src/workspace/native_workspace_store.cpp`
- Modify: `native/src/ui/app_controller.h`
- Modify: `native/src/ui/app_controller.cpp`
- Modify: `native/src/ui/workspace_model.h`
- Modify: `native/src/ui/workspace_model.cpp`
- Test: `native/tests/native_workspace_store_test.cpp`
- Test: `native/tests/app_controller_test.cpp`
- Test: `native/tests/workspace_model_test.cpp`

- [ ] **Step 1: 写失败持久化测试**

在 `native/tests/native_workspace_store_test.cpp` 增加：

```cpp
void roundTripsIndependentTeamsAndOneTeamPerMember()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);

    NativeWorkspaceSnapshot snapshot;
    snapshot.teams = {
        {QStringLiteral("team-a"), QStringLiteral("一队"), {QStringLiteral("hamster-001")}},
        {QStringLiteral("team-b"), QStringLiteral("二队"), {QStringLiteral("hamster-002")}},
        {QStringLiteral("team-c"), QStringLiteral("三队"), {}},
        {QStringLiteral("team-d"), QStringLiteral("四队"), {}},
    };

    NativeWorkspaceStore store(&settings);
    QVERIFY(store.save(snapshot));
    const NativeWorkspaceSnapshot loaded = store.load();
    QCOMPARE(loaded.version, 6);
    QCOMPARE(loaded.teams.size(), 4);
    QCOMPARE(loaded.teams.at(2).memberIds, QStringList{});
    QCOMPARE(loaded.teams.at(3).memberIds, QStringList{});
}

void removesDuplicateTeamMembershipWhenLoading()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);

    NativeWorkspaceSnapshot snapshot;
    snapshot.teams = {
        {QStringLiteral("team-a"), QStringLiteral("一队"), {QStringLiteral("hamster-001")}},
        {QStringLiteral("team-b"), QStringLiteral("二队"), {QStringLiteral("hamster-001")}},
    };

    NativeWorkspaceStore store(&settings);
    QVERIFY(store.save(snapshot));
    const NativeWorkspaceSnapshot loaded = store.load();
    QCOMPARE(loaded.teams.at(0).memberIds, QStringList({QStringLiteral("hamster-001")}));
    QCOMPARE(loaded.teams.at(1).memberIds, QStringList{});
}
```

- [ ] **Step 2: 写失败控制器测试**

在 `native/tests/app_controller_test.cpp` 增加：

```cpp
void createsEmptyTeamsAndAssignsRosterMembers()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    const QString first = controller.createTeam(QStringLiteral("一队"));
    const QString second = controller.createTeam(QStringLiteral("二队"));
    const QString third = controller.createTeam(QStringLiteral("三队"));
    const QString fourth = controller.createTeam(QStringLiteral("四队"));
    QVERIFY(!first.isEmpty());
    QVERIFY(!second.isEmpty());
    QVERIFY(!third.isEmpty());
    QVERIFY(!fourth.isEmpty());

    QCOMPARE(controller.assignGuildMemberToTeam(QStringLiteral("hamster-001"), first), QString());
    QCOMPARE(controller.workspace()->teams().size(), 4);
    QCOMPARE(controller.workspace()->teams().first().memberIds,
             QStringList({QStringLiteral("hamster-001")}));
}

void movesMemberBetweenTeamsAndLeavesEmptySlots()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    const QString first = controller.createTeam(QStringLiteral("一队"));
    const QString second = controller.createTeam(QStringLiteral("二队"));
    QCOMPARE(controller.assignGuildMemberToTeam(QStringLiteral("hamster-001"), first), QString());
    QCOMPARE(controller.assignGuildMemberToTeam(QStringLiteral("hamster-001"), second), QString());

    QCOMPARE(controller.workspace()->teams().at(0).memberIds, QStringList{});
    QCOMPARE(controller.workspace()->teams().at(1).memberIds,
             QStringList({QStringLiteral("hamster-001")}));
}
```

- [ ] **Step 3: 运行测试，确认当前失败**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R "native_workspace_store_test|app_controller_test|workspace_model_test" --output-on-failure
```

Expected: 编译失败，`NativeTeam`、`teams()` 和队伍控制器命令尚不存在。

- [ ] **Step 4: 增加独立队伍类型和 schema v6**

在 `native/src/workspace/native_workspace_types.h` 增加：

```cpp
struct NativeTeam {
    QString id;
    QString name;
    QStringList memberIds;

    bool operator==(const NativeTeam &) const = default;
};

struct NativeWorkspaceSnapshot {
    int version = 6;
    QVector<NativeRoomRecord> library;
    QVector<NativeRoomGroup> groups;
    QVector<NativeTeam> teams;
    QVector<NativeWorkspacePreset> presets;
    // 其余字段保持原定义和顺序不变
};
```

在 `native_workspace_store.cpp`：

- `kCurrentVersion` 改为 `6`。
- 增加 `kMaxTeams = 20`。
- `normalize()` 清理无效队伍 ID、空名称、超过 30 字的名称、无效或重复成员 ID。
- 同一成员只保留在第一个包含它的队伍中。
- 队伍顺序由 `snapshot.teams` 的数组顺序决定。
- `toJson()` 和 `fromJson()` 增加 `teams` 数组；旧快照缺少 `teams` 时使用空数组。

固定 JSON 形状：

```json
{
  "id": "team-a",
  "name": "一队",
  "memberIds": ["hamster-001", "hamster-002"]
}
```

- [ ] **Step 5: 暴露 WorkspaceModel 队伍数据和导航可见状态**

在 `native/src/ui/workspace_model.h` 增加：

```cpp
Q_PROPERTY(QVariantList teams READ teamItems NOTIFY workspaceDataChanged)
Q_PROPERTY(bool navigationVisible READ navigationVisible NOTIFY navigationVisibleChanged)

QVariantList teamItems() const;
bool navigationVisible() const noexcept;
void setNavigationVisible(bool visible);
const QVector<NativeTeam> &teams() const noexcept;
```

`teamItems()` 每项固定返回：

```cpp
QVariantMap{
    {QStringLiteral("id"), team.id},
    {QStringLiteral("name"), team.name},
    {QStringLiteral("memberIds"), team.memberIds},
    {QStringLiteral("memberCount"), team.memberIds.size()},
}
```

`setWorkspaceData()` 参数扩展为队伍、旧分组、预设和活动分组；`refreshPresentation()` 同步发送。

- [ ] **Step 6: 增加控制器命令**

在 `native/src/ui/app_controller.h` 增加：

```cpp
Q_PROPERTY(QVariantList guildRoster READ guildRoster CONSTANT)

QVariantList guildRoster() const;

Q_INVOKABLE QString createTeam(const QString &name);
Q_INVOKABLE QString renameTeam(const QString &teamId, const QString &name);
Q_INVOKABLE QString deleteTeam(const QString &teamId);
Q_INVOKABLE QString moveTeam(const QString &teamId, int delta);
Q_INVOKABLE QString assignGuildMemberToTeam(const QString &memberId,
                                            const QString &teamId);
Q_INVOKABLE QString removeGuildMemberFromTeam(const QString &teamId,
                                              const QString &memberId);
Q_INVOKABLE QString setNavigationVisible(bool visible);
```

命令规则：

- 创建队伍时自动生成 UUID，允许空队伍，最多 20 个队伍。
- 删除队伍时只删除队伍栏位，不删除主播名单、收藏、历史或房间记录。
- 移动队伍时保持成员归属不变。
- 分配到新队伍时先从原队伍移除该成员。
- 无效成员 ID 返回 `未找到该公会主播`；无效队伍返回 `未找到该队伍`。
- 每次成功变更调用 `refreshPresentation()` 和 `persistWorkspace()`。

- [ ] **Step 7: 运行持久化与控制器测试**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R "native_workspace_store_test|app_controller_test|workspace_model_test" --output-on-failure
```

Expected: 三个目标全部通过，旧版本工作区快照仍可加载。

- [ ] **Step 8: 提交**

```powershell
git add native/src/workspace/native_workspace_types.h native/src/workspace/native_workspace_store.cpp native/src/ui/app_controller.h native/src/ui/app_controller.cpp native/src/ui/workspace_model.h native/src/ui/workspace_model.cpp native/tests/native_workspace_store_test.cpp native/tests/app_controller_test.cpp native/tests/workspace_model_test.cpp
git commit -m "feat: persist independent guild teams"
```

### Task 3: 在设置页增加队伍管理，不放入导航页

**Files:**
- Create: `native/app/qml/dialogs/TeamManagerDialog.qml`
- Modify: `native/app/qml/pages/SettingsPage.qml`
- Modify: `native/app/qml/Main.qml`
- Modify: `native/CMakeLists.txt`
- Test: `native/tests/qml_interaction_test.cpp`

- [ ] **Step 1: 写失败的设置页与导航页边界测试**

在 `native/tests/qml_interaction_test.cpp` 增加：

```cpp
void managesTeamsOnlyFromSettings()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);

    QObject *settingsButton = window->findChild<QObject *>(QStringLiteral("settingsButton"));
    QVERIFY(settingsButton != nullptr);
    click(settingsButton);
    QObject *manageTeams = window->findChild<QObject *>(QStringLiteral("manageTeamsButton"));
    QVERIFY(manageTeams != nullptr);
    click(manageTeams);

    QObject *dialog = window->findChild<QObject *>(QStringLiteral("teamManagerDialog"));
    QVERIFY(dialog != nullptr);
    QTRY_VERIFY(dialog->property("visible").toBool());
    QVERIFY(dialog->findChild<QObject *>(QStringLiteral("createTeamButton")) != nullptr);
    QVERIFY(dialog->findChild<QObject *>(QStringLiteral("teamMemberAssignmentList")) != nullptr);
}

void navigationPanelDoesNotExposeTeamManagement()
{
    registerQmlTypes();
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);

    click(window->findChild<QObject *>(QStringLiteral("hamsterNavigationButton")));
    QObject *navigation = window->findChild<QObject *>(QStringLiteral("guildNavigationPanel"));
    QVERIFY(navigation != nullptr);
    QTRY_VERIFY(navigation->property("visible").toBool());
    QVERIFY(navigation->findChild<QObject *>(QStringLiteral("createTeamButton")) == nullptr);
    QVERIFY(navigation->findChild<QObject *>(QStringLiteral("deleteTeamButton")) == nullptr);
    QVERIFY(navigation->findChild<QObject *>(QStringLiteral("teamMemberAssignmentList")) == nullptr);
}
```

- [ ] **Step 2: 运行 QML 测试，确认当前失败**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R qml_interaction_test --output-on-failure
```

Expected: `manageTeamsButton`、`teamManagerDialog` 或导航入口对象不存在。

- [ ] **Step 3: 实现 `TeamManagerDialog.qml`**

对话框对象名固定为 `teamManagerDialog`，包含：

- `teamNameInput`：新队名或重命名输入。
- `createTeamButton`：调用 `controller.createTeam(teamNameInput.text.trim())`。
- `renameTeamButton`：调用 `controller.renameTeam(selectedTeamId, teamNameInput.text.trim())`。
- `teamList`：显示所有队伍，包括空队伍。
- `moveTeamUpButton` / `moveTeamDownButton`：调用 `controller.moveTeam(teamId, -1/1)`。
- `deleteTeamButton`：调用 `controller.deleteTeam(teamId)`。
- `teamMemberAssignmentList`：列出全部公会主播，显示当前队伍；每行提供“加入所选队伍”和“移出队伍”按钮。

对话框不得读取 `groups`、`activeGroupId`，也不得调用 `setActiveGroup`。队伍是独立数据。

- [ ] **Step 4: 在设置页增加唯一管理入口**

在 `SettingsPage.qml` 的“关闭按钮行为”和“更新”之间增加“队伍管理”区块：

```qml
Button {
    objectName: "manageTeamsButton"
    text: "管理队伍"
    onClicked: teamManagerRequested()
}
```

在 `SettingsPage.qml` 增加 `signal teamManagerRequested()`。`Main.qml` 连接该信号并打开 `teamManagerDialog`。

- [ ] **Step 5: 注册 QML 和资源**

在 `native/CMakeLists.txt` 的 `douyu_qml`、`qml_engine_smoke_test`、`qml_visual_smoke_test`、`qml_interaction_test` 和 `qml_close_regression_test` QML 清单中加入：

```text
app/qml/dialogs/TeamManagerDialog.qml
```

不要加入 `GroupManagerDialog.qml`，除非另一个明确功能需要它。

- [ ] **Step 6: 运行队伍管理 UI 测试**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R qml_interaction_test --output-on-failure
```

Expected: `managesTeamsOnlyFromSettings` 和 `navigationPanelDoesNotExposeTeamManagement` 通过。

- [ ] **Step 7: 提交**

```powershell
git add native/app/qml/dialogs/TeamManagerDialog.qml native/app/qml/pages/SettingsPage.qml native/app/qml/Main.qml native/CMakeLists.txt native/tests/qml_interaction_test.cpp
git commit -m "feat: manage guild teams from settings"
```

### Task 4: 增加公会成员房间号解析、缓存和快捷添加

**Files:**
- Create: `native/src/workspace/guild_room_resolver.h`
- Create: `native/src/workspace/guild_room_resolver.cpp`
- Create: `native/tests/guild_room_resolver_test.cpp`
- Modify: `native/src/workspace/native_workspace_types.h`
- Modify: `native/src/workspace/native_workspace_store.cpp`
- Modify: `native/src/ui/app_controller.h`
- Modify: `native/src/ui/app_controller.cpp`
- Modify: `native/CMakeLists.txt`
- Test: `native/tests/app_controller_test.cpp`
- Test: `native/tests/native_workspace_store_test.cpp`

- [ ] **Step 1: 写失败的解析器测试**

```cpp
class GuildRoomResolverTest final : public QObject {
    Q_OBJECT

private slots:
    void resolvesExactAnchorMatchOnly();
    void backsOffAfterFailureInsteadOfBursting();
    void acceptsManualRoomIdAsVerified();
};

void GuildRoomResolverTest::resolvesExactAnchorMatchOnly()
{
    FakeSearchTransport transport;
    transport.results = {
        {QStringLiteral("63136"), QStringLiteral("仓鼠特工阿飞"), QStringLiteral("标题")},
        {QStringLiteral("63137"), QStringLiteral("主播阿飞"), QStringLiteral("标题")},
    };
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({{QStringLiteral("hamster-002"),
                         QStringLiteral("主播阿飞"),
                         QStringLiteral("主播阿飞"),
                         QString()}});
    resolver.start();
    QTRY_COMPARE_WITH_TIMEOUT(resolver.roomIdFor(QStringLiteral("hamster-002")),
                              QStringLiteral("63137"), 1000);
}

void GuildRoomResolverTest::backsOffAfterFailureInsteadOfBursting()
{
    FakeSearchTransport transport;
    transport.failuresRemaining = 1;
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({{QStringLiteral("hamster-002"),
                         QStringLiteral("主播阿飞"),
                         QStringLiteral("主播阿飞"),
                         QString()}});
    resolver.start();
    QTRY_COMPARE_WITH_TIMEOUT(resolver.statusFor(QStringLiteral("hamster-002")),
                              QStringLiteral("retrying"), 1000);
    QCOMPARE(transport.requestCount, 1);
    QVERIFY(resolver.nextRetryDelayMsForTest() >= 3000);
}

void GuildRoomResolverTest::acceptsManualRoomIdAsVerified()
{
    FakeSearchTransport transport;
    GuildRoomResolver resolver(&transport);
    resolver.setRoster({{QStringLiteral("hamster-002"),
                         QStringLiteral("主播阿飞"),
                         QStringLiteral("主播阿飞"),
                         QString()}});
    QCOMPARE(resolver.setManualRoomId(QStringLiteral("hamster-002"),
                                      QStringLiteral("84452")), QString());
    QCOMPARE(resolver.roomIdFor(QStringLiteral("hamster-002")),
             QStringLiteral("84452"));
}
```

`FakeSearchTransport` 是测试内的轻量类，暴露 `search(query)`、`cancel(requestId)`、`requestCount` 和 `nextRetryDelayMsForTest()`；不要在测试中访问真实斗鱼。

- [ ] **Step 2: 运行测试，确认当前失败**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R guild_room_resolver_test --output-on-failure
```

Expected: 解析器和测试目标尚不存在。

- [ ] **Step 3: 增加可持久化房间号缓存**

在 `native_workspace_types.h` 增加：

```cpp
struct GuildRoomCacheEntry {
    QString memberId;
    QString roomId;
    QString anchorName;
    qint64 verifiedAtMs = 0;

    bool operator==(const GuildRoomCacheEntry &) const = default;
};
```

`NativeWorkspaceSnapshot` 增加：

```cpp
QVector<GuildRoomCacheEntry> guildRoomCache;
```

存储层只接受合法 `memberId`、`^[0-9]{1,20}$` 房间号和公开主播昵称；同成员只保留一个有效条目。缓存不保存搜索词、服务响应、Cookie、Token、播放地址或弹幕数据。

- [ ] **Step 4: 实现 `GuildRoomResolver` 的队列和退避**

核心接口：

```cpp
enum class GuildRoomResolutionStatus {
    Idle,
    Resolved,
    Resolving,
    Retrying,
    Unconfirmed,
    Error,
};

class GuildRoomResolver final : public QObject {
    Q_OBJECT

public:
    explicit GuildRoomResolver(SearchTransport *transport, QObject *parent = nullptr);

    void setRoster(QVector<GuildMember> roster);
    void setCache(QVector<GuildRoomCacheEntry> cache);
    QVector<GuildRoomCacheEntry> cache() const;
    void start();
    void stop();

    QString roomIdFor(const QString &memberId) const;
    QString statusFor(const QString &memberId) const;
    QString setManualRoomId(const QString &memberId, const QString &roomId);

signals:
    void memberChanged(QString memberId);
    void cacheChanged();
};
```

规则：

- 同一时刻最多一个公会房间号搜索请求。
- 两个请求之间固定间隔 `1200ms`。
- 首次失败后按 `3000ms`、`15000ms`、`75000ms` 退避，之后保持 `75000ms`。
- 网络失败、超时和服务失败不得触发连续请求。
- 只有清理掉角色后缀后的主播名与返回 `anchorName` 完全相等时才自动接受房间号。
- 多个不同房间号都完全匹配时标记 `unconfirmed`，不写入缓存。
- 缓存命中时不发网络请求。
- 解析器请求 ID 与弹幕、房间状态调度、普通搜索必须是不同请求。

- [ ] **Step 5: 接入 AppController**

在 `AppController` 构造函数初始化解析器，并让 `onServiceResponse()` 和 `onServiceRequestFailed()` 先尝试路由给解析器；只有请求 ID 不属于解析器时才继续现有的收藏监控与普通搜索流程。

增加公开接口：

```cpp
Q_INVOKABLE QString setGuildMemberRoomId(const QString &memberId,
                                         const QString &roomId);
Q_INVOKABLE QString addGuildMemberRoom(const QString &memberId);
```

`addGuildMemberRoom()` 行为：

- 成员没有确认房间号时返回 `请先确认房间号`。
- 房间已在活动列表时返回 `该房间已在列表中`。
- 非双主布局且已有 9 路时返回 `当前布局最多支持 9 个房间`。
- 已达到 10 路时返回 `最多添加 10 个房间`。
- 成功时直接调用协调器，并带上缓存中的公开主播昵称；不得依赖当前普通搜索的 `searchCandidates_`。
- 成功加入后更新历史、持久化工作区并刷新模型。

- [ ] **Step 6: 增加控制器集成测试**

在 `app_controller_test.cpp` 增加：

```cpp
void addsResolvedGuildMemberWithoutUsingOrdinarySearchState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(directory.filePath(QStringLiteral("workspace.ini")), QSettings::IniFormat);
    FakeNotificationSink sink;
    AppController controller(fakeServicePath(), &settings, &sink);

    QCOMPARE(controller.setGuildMemberRoomId(QStringLiteral("hamster-002"),
                                             QStringLiteral("84452")), QString());
    QCOMPARE(controller.addGuildMemberRoom(QStringLiteral("hamster-002")), QString());
    QCOMPARE(controller.rooms()->rowCount(), 1);
    QCOMPARE(controller.rooms()
                 ->data(controller.rooms()->index(0, 0), RoomListModel::RoomIdRole)
                 .toString(),
             QStringLiteral("84452"));
    QCOMPARE(controller.rooms()
                 ->data(controller.rooms()->index(0, 0), RoomListModel::AnchorNameRole)
                 .toString(),
             QStringLiteral("主播阿飞"));
}
```

- [ ] **Step 7: 运行解析和持久化测试**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R "guild_room_resolver_test|native_workspace_store_test|app_controller_test" --output-on-failure
```

Expected: 房间号缓存、退避、人工确认和快捷添加测试全部通过。

- [ ] **Step 8: 提交**

```powershell
git add native/src/workspace/guild_room_resolver.h native/src/workspace/guild_room_resolver.cpp native/tests/guild_room_resolver_test.cpp native/src/workspace/native_workspace_types.h native/src/workspace/native_workspace_store.cpp native/src/ui/app_controller.h native/src/ui/app_controller.cpp native/CMakeLists.txt native/tests/app_controller_test.cpp native/tests/native_workspace_store_test.cpp
git commit -m "feat: cache and quick-add guild member rooms"
```

### Task 5: 增加导航入口和互斥左侧面板

**Files:**
- Modify: `native/app/qml/components/AppHeader.qml`
- Modify: `native/app/qml/Main.qml`
- Modify: `native/CMakeLists.txt`
- Create: `native/app/qml/assets/icons/hamster-agent.jfif`
- Test: `native/tests/qml_interaction_test.cpp`

- [ ] **Step 1: 写失败入口与互斥测试**

```cpp
void placesNavigationEntryAfterSidebarToggleAndBeforeBrand()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);

    QObject *sidebarToggle = window->findChild<QObject *>(QStringLiteral("sidebarToggleButton"));
    QObject *navigationToggle =
        window->findChild<QObject *>(QStringLiteral("hamsterNavigationButton"));
    QObject *brandMark = window->findChild<QObject *>(QStringLiteral("brandMark"));
    QVERIFY(sidebarToggle != nullptr);
    QVERIFY(navigationToggle != nullptr);
    QVERIFY(brandMark != nullptr);
    QVERIFY(sidebarToggle->property("x").toDouble() < navigationToggle->property("x").toDouble());
    QVERIFY(navigationToggle->property("x").toDouble() < brandMark->property("x").toDouble());
}

void makesRoomListAndNavigationPanelsMutuallyExclusive()
{
    QQmlApplicationEngine engine;
    QQuickWindow *window = loadWindow(engine);
    QVERIFY(window != nullptr);

    QObject *roomSidebar = window->findChild<QObject *>(QStringLiteral("roomSidebar"));
    QObject *navigationPanel =
        window->findChild<QObject *>(QStringLiteral("guildNavigationPanel"));
    QVERIFY(roomSidebar != nullptr);
    QVERIFY(navigationPanel != nullptr);

    click(window->findChild<QObject *>(QStringLiteral("hamsterNavigationButton")));
    QTRY_VERIFY(navigationPanel->property("visible").toBool());
    QVERIFY(!roomSidebar->property("visible").toBool());

    click(window->findChild<QObject *>(QStringLiteral("sidebarToggleButton")));
    QTRY_VERIFY(roomSidebar->property("visible").toBool());
    QVERIFY(!navigationPanel->property("visible").toBool());
}
```

- [ ] **Step 2: 运行测试，确认当前失败**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R qml_interaction_test --output-on-failure
```

Expected: 导航入口对象不存在。

- [ ] **Step 3: 接入图标资源**

将用户提供的 `C:\Users\15070\Desktop\rs1440.jfif` 复制为：

```text
native/app/qml/assets/icons/hamster-agent.jfif
```

在 `DOUYU_QML_ICON_ASSETS` 增加：

```cmake
app/qml/assets/icons/hamster-agent.jfif
```

入口显示时使用中心裁剪，避免 1440x1440 原图在 18x18 按钮中缩成无法辨认的小图：

```qml
Image {
    objectName: "hamsterNavigationIcon"
    anchors.centerIn: parent
    width: 18
    height: 18
    source: Qt.resolvedUrl("../assets/icons/hamster-agent.jfif")
    sourceClipRect: Qt.rect(144, 144, 1152, 1152)
    fillMode: Image.PreserveAspectCrop
    smooth: true
    opacity: parent.hovered ? 1 : 0.86
}
```

- [ ] **Step 4: 在 AppHeader 增加入口**

在 `sidebarToggleButton` 后、`brandMark` 前增加：

```qml
ToolButton {
    objectName: "hamsterNavigationButton"
    width: Theme.controlHeight
    height: Theme.controlHeight
    Accessible.name: root.navigationVisible ? "收起仓鼠特工导航" : "展开仓鼠特工导航"
    ToolTip.visible: hovered
    ToolTip.text: Accessible.name
    onClicked: root.toggleNavigation()
    contentItem: Image {
        objectName: "hamsterNavigationIcon"
        anchors.centerIn: parent
        width: 18
        height: 18
        source: Qt.resolvedUrl("../assets/icons/hamster-agent.jfif")
        sourceClipRect: Qt.rect(144, 144, 1152, 1152)
        fillMode: Image.PreserveAspectCrop
        smooth: true
        opacity: parent.hovered ? 1 : 0.86
    }
    background: Rectangle {
        radius: Theme.radiusSmall
        color: parent.down ? Theme.well
             : (parent.hovered ? Theme.controlSurface : "transparent")
    }
}
```

新增：

```qml
property bool navigationVisible: false
signal toggleNavigation()
```

- [ ] **Step 5: 在 Main.qml 建立单左栏布局**

增加计算属性：

```qml
readonly property bool navigationVisible: workspaceModel
                                         ? workspaceModel.navigationVisible
                                         : false
readonly property int roomSidebarWidth:
    sidebarVisible && !navigationVisible ? 268 : 0
readonly property int navigationWidth:
    navigationVisible ? 284 : 0
readonly property int leftPanelWidth:
    Math.max(roomSidebarWidth, navigationWidth)
```

增加函数：

```qml
function toggleNavigationVisibility() {
    if (!appController) return
    const nextVisible = !navigationVisible
    appController.setNavigationVisible(nextVisible)
    if (nextVisible && sidebarVisible) appController.setSidebarVisible(false)
}
```

修改 `toggleSidebarVisibility()`：当新状态为展开时，先调用
`appController.setNavigationVisible(false)`。

`AppHeader` 传入 `navigationVisible` 并连接 `onToggleNavigation`。

左侧布局改为：

```qml
RoomSidebar {
    id: sidebar
    width: root.roomSidebarWidth
    visible: width > 0
}

GuildNavigationPanel {
    id: guildNavigation
    objectName: "guildNavigationPanel"
    width: root.navigationWidth
    visible: width > 0
    anchors.top: header.bottom
    anchors.left: parent.left
    anchors.bottom: parent.bottom
    controller: root.appController
    workspaceModel: root.workspaceModel
}

Item {
    id: leftPanelBoundary
    anchors.top: parent.top
    anchors.bottom: parent.bottom
    anchors.left: parent.left
    width: root.leftPanelWidth
    visible: false
}
```

`WorkspaceGrid`、`WorkspaceStatusBar` 和 `SettingsPage` 的 `anchors.left` 改为
`leftPanelBoundary.right`。`SettingsPage` 继续作为内容页，不因左侧面板切换消失。

- [ ] **Step 6: 注册新 QML 文件**

在所有包含 `RoomSidebar.qml` 的 CMake QML 资源清单中增加：

```text
app/qml/components/GuildNavigationPanel.qml
app/qml/components/GuildMemberRow.qml
```

新增组件文件先在本任务提供最小可用骨架，Task 6 填充完整内容。

- [ ] **Step 7: 运行互斥和入口测试**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R qml_interaction_test --output-on-failure
```

Expected: 入口顺序、打开导航、打开房间列表、回归侧栏测试全部通过。

- [ ] **Step 8: 提交**

```powershell
git add native/app/qml/components/AppHeader.qml native/app/qml/Main.qml native/CMakeLists.txt native/app/qml/assets/icons/hamster-agent.jfif native/app/qml/components/GuildNavigationPanel.qml native/app/qml/components/GuildMemberRow.qml native/tests/qml_interaction_test.cpp
git commit -m "feat: add mutually exclusive guild navigation panel"
```

### Task 6: 实现队伍优先的公会导航页

**Files:**
- Modify: `native/app/qml/components/GuildNavigationPanel.qml`
- Modify: `native/app/qml/components/GuildMemberRow.qml`
- Modify: `native/app/qml/Main.qml`
- Test: `native/tests/qml_interaction_test.cpp`

- [ ] **Step 1: 写失败的队伍分组和快捷添加测试**

```cpp
void groupsGuildNavigationByTeamsWithoutRoleLabels()
{
    FakeGuildNavigationController controller;
    controller.roster = {
        {{QStringLiteral("id"), QStringLiteral("hamster-001")},
         {QStringLiteral("anchorName"), QStringLiteral("寅子")},
         {QStringLiteral("roomId"), QStringLiteral("71415")}},
        {{QStringLiteral("id"), QStringLiteral("hamster-004")},
         {QStringLiteral("anchorName"), QStringLiteral("主播阿郎")},
         {QStringLiteral("roomId"), QStringLiteral("320155")}},
    };
    controller.teams = {
        {{QStringLiteral("id"), QStringLiteral("team-a")},
         {QStringLiteral("name"), QStringLiteral("一队")},
         {QStringLiteral("memberIds"), QStringList{QStringLiteral("hamster-004")}}},
        {{QStringLiteral("id"), QStringLiteral("team-b")},
         {QStringLiteral("name"), QStringLiteral("二队")},
         {QStringLiteral("memberIds"), QStringList{QStringLiteral("hamster-001")}}},
    };

    QQmlApplicationEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qml/components/GuildNavigationPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {QStringLiteral("controller"),
         QVariant::fromValue(static_cast<QObject *>(&controller))},
    }));
    QVERIFY(panel != nullptr);

    QObject *teamsList = panel->findChild<QObject *>(QStringLiteral("guildTeamSections"));
    QVERIFY(teamsList != nullptr);
    QTRY_COMPARE(teamsList->property("count").toInt(), 2);
    QVERIFY(panel->findChild<QObject *>(QStringLiteral("guildMemberRole")) == nullptr);
}

void quickAddsResolvedGuildMember()
{
    FakeGuildNavigationController controller;
    controller.roster = {
        {{QStringLiteral("id"), QStringLiteral("hamster-001")},
         {QStringLiteral("anchorName"), QStringLiteral("寅子")},
         {QStringLiteral("roomId"), QStringLiteral("71415")}},
    };

    QQmlApplicationEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qml/components/GuildNavigationPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {QStringLiteral("controller"),
         QVariant::fromValue(static_cast<QObject *>(&controller))},
    }));
    QVERIFY(panel != nullptr);

    QObject *addButton = panel->findChild<QObject *>(QStringLiteral("guildQuickAddButton"));
    QVERIFY(addButton != nullptr);
    click(addButton);
    QCOMPARE(controller.lastAddedMemberId, QStringLiteral("hamster-001"));
}
```

- [ ] **Step 2: 运行测试，确认当前失败**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R qml_interaction_test --output-on-failure
```

Expected: 队伍区块和快捷添加对象不存在。

- [ ] **Step 3: 生成队伍优先的分组投影**

`GuildNavigationPanel.qml` 暴露只读属性：

```qml
readonly property var roster: controller ? controller.guildRoster : []
readonly property var teams: workspaceModel ? workspaceModel.teams : []
readonly property string query: searchInput.text.trim().toLowerCase()
readonly property var teamSections: root.buildTeamSections()
```

`buildTeamSections()` 固定规则：

- 先按 `teams` 数组顺序生成每个队伍区块，即使该队为空也保留栏位。
- 再生成“未分队”区块，包含未出现在任何 `memberIds` 中的名单成员。
- 搜索框只过滤成员行，不改变队伍顺序。
- 搜索匹配主播名或房间号。
- 不生成角色字段、角色标签或 `role` 属性。

固定输出结构：

```js
{
    teamId: "team-a",
    title: "一队",
    isUnassigned: false,
    members: [/* 过滤后的 roster entries */]
}
```

- [ ] **Step 4: 实现导航页 UI**

面板宽度固定为 `284`，使用 `Theme.managementSurface`、`Theme.border`、
`Theme.text` 和 `Theme.mutedText`。结构：

```text
仓鼠特工
[搜索主播或房间号]
[一队]
  成员行
[二队]
  成员行
[未分队]
  成员行
```

不显示功能说明、角色文案、公会动态状态说明或卡片嵌套。

`GuildMemberRow.qml` 属性：

```qml
property var member: null
property string roomStatus: ""
property bool active: false
property bool canAdd: false
signal addRequested(string memberId)
signal roomIdSubmitted(string memberId, string roomId)
```

行内显示：

- 主播名。
- 已确认时显示房间号；未确认时显示“待确认”。
- 已加入活动列表时显示“已添加”。
- 右侧使用 `plus.svg` 图标按钮，`objectName: "guildQuickAddButton"`。
- 未确认时点击房间号文本打开 `TextField`，提交后调用
  `controller.setGuildMemberRoomId(memberId, roomId)`。

- [ ] **Step 5: 连接快捷添加和容量反馈**

点击 `guildQuickAddButton` 时调用：

```qml
const message = controller.addGuildMemberRoom(member.id)
if (message && message.length > 0) {
    workspaceModel.setLastMessage(message, "error", 2400)
}
```

按钮 `enabled` 条件：

```qml
member.roomId.length > 0
&& !member.active
&& (!workspaceModel || workspaceModel.layoutMode === "primary-two"
    || controller.rooms.roomCount < 9)
&& (!controller.rooms || controller.rooms.roomCount < 10)
```

如果 `controller.rooms` 不可用，保留按钮可用，由 `AppController` 返回准确的容量错误。

- [ ] **Step 6: 增加搜索、空栏位和未分队测试**

在 `qml_interaction_test.cpp` 增加：

```cpp
void filtersGuildNavigationWithoutChangingTeamOrder()
{
    FakeGuildNavigationController controller;
    controller.roster = {
        {{QStringLiteral("id"), QStringLiteral("hamster-001")},
         {QStringLiteral("anchorName"), QStringLiteral("寅子")},
         {QStringLiteral("roomId"), QStringLiteral("71415")}},
        {{QStringLiteral("id"), QStringLiteral("hamster-002")},
         {QStringLiteral("anchorName"), QStringLiteral("主播阿飞")},
         {QStringLiteral("roomId"), QStringLiteral("84452")}},
    };
    controller.teams = {
        {{QStringLiteral("id"), QStringLiteral("team-a")},
         {QStringLiteral("name"), QStringLiteral("一队")},
         {QStringLiteral("memberIds"), QStringList{QStringLiteral("hamster-001")}}},
        {{QStringLiteral("id"), QStringLiteral("team-b")},
         {QStringLiteral("name"), QStringLiteral("二队")},
         {QStringLiteral("memberIds"), QStringList{}}},
    };

    QQmlApplicationEngine engine;
    QQmlComponent component(&engine,
                            QUrl(QStringLiteral("qrc:/qml/components/GuildNavigationPanel.qml")));
    QVERIFY2(component.isReady(), qPrintable(component.errorString()));
    std::unique_ptr<QObject> panel(component.createWithInitialProperties({
        {QStringLiteral("controller"),
         QVariant::fromValue(static_cast<QObject *>(&controller))},
    }));
    QVERIFY(panel != nullptr);

    QObject *search = panel->findChild<QObject *>(QStringLiteral("guildNavigationSearch"));
    QVERIFY(search != nullptr);
    search->setProperty("text", QStringLiteral("阿飞"));
    QTRY_COMPARE(panel->property("visibleMemberCount").toInt(), 1);
    QCOMPARE(panel->property("visibleTeamCount").toInt(), 3);
}
```

`visibleTeamCount` 为 3，因为一队、二队和未分队栏位都保留，只是成员行被过滤。

- [ ] **Step 7: 运行导航页测试**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R qml_interaction_test --output-on-failure
```

Expected: 队伍分组、未分队、空队伍、搜索过滤、人工确认和快捷添加测试全部通过。

- [ ] **Step 8: 提交**

```powershell
git add native/app/qml/components/GuildNavigationPanel.qml native/app/qml/components/GuildMemberRow.qml native/app/qml/Main.qml native/tests/qml_interaction_test.cpp
git commit -m "feat: render team-grouped guild navigation"
```

### Task 7: 文档、知识库和全量验证

**Files:**
- Modify: `README.md`
- Modify: `native/README.md`
- Modify: `docs/文件职责索引.md`
- Create: `docs/superpowers/logs/2026-09-24-hamster-agent-navigation.md`

- [ ] **Step 1: 更新用户文档**

在 `README.md` 和 `native/README.md` 增加：

- 顶栏房间列表按钮右侧的仓鼠特工导航入口。
- 导航页按队伍展示公会主播，未分配主播进入“未分队”。
- 队伍管理位于“设置 -> 队伍管理”。
- 导航页只展示内置公会名单，不显示角色标签。
- 自动房间号无法确认时可在成员行手工输入；快捷加入仍受 9/10 路布局容量限制。

不得声称真实斗鱼长时播放、弹幕稳定性或性能已经通过。

- [ ] **Step 2: 更新文件职责索引**

在 `docs/文件职责索引.md` 增加以下文件职责：

- `native/app/resources/hamster_agent_roster.json`
- `native/src/workspace/guild_roster.*`
- `native/src/workspace/guild_room_resolver.*`
- `native/app/qml/components/GuildNavigationPanel.qml`
- `native/app/qml/components/GuildMemberRow.qml`
- `native/app/qml/dialogs/TeamManagerDialog.qml`
- `native/tests/guild_roster_test.cpp`
- `native/tests/guild_room_resolver_test.cpp`

- [ ] **Step 3: 写知识库日志骨架并填入实际证据**

创建：

```text
docs/superpowers/logs/2026-09-24-hamster-agent-navigation.md
```

固定章节：

```markdown
# 2026-09-24 仓鼠特工导航页

## 目标

## 数据边界

## 队伍规则

## 房间号解析与缓存

## 验证记录

## 计划匹配

## 未验证边界
```

实施完成后再填入实际命令、退出码、测试数量和已知限制；不要预先写“全部通过”。

- [ ] **Step 4: 构建主程序**

Run:

```powershell
$env:QT_ROOT = 'D:\Qt\6.8.3\msvc2022_64'
$env:MPV_ROOT = (Resolve-Path .\sdk\mpv).Path
Set-Location native
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release --target douyu_monitor_native
```

Expected: 配置和主程序构建退出码为 `0`。

- [ ] **Step 5: 运行相关回归测试**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R "guild_roster_test|guild_room_resolver_test|native_workspace_store_test|app_controller_test|workspace_model_test|qml_engine_smoke_test|qml_interaction_test|qml_visual_smoke_test" --output-on-failure
```

Expected: 所列目标全部通过。

- [ ] **Step 6: 运行全量 CTest 和程序自测**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release
.\out\build\windows-x64-release\douyu_monitor_native.exe --self-test
```

Expected:

- 全量 CTest 全部通过，退出码为 `0`。
- 自测输出 `native self-test passed: Qt Quick renderer`。

- [ ] **Step 7: 做静态边界检查**

Run:

```powershell
rg -n "clubOrgName" native/app/qml/components/GuildNavigationPanel.qml native/src/workspace/guild_roster.cpp native/src/workspace/guild_room_resolver.cpp
rg -n "playbackUrl|cookie|token|signature|requestHeaders" native/app/resources/hamster_agent_roster.json native/src/workspace/guild_room_resolver.cpp
git diff --check
git status --short
```

Expected:

- 第一条命令无输出，确认导航页不依赖 `clubOrgName`。
- 第二条命令无输出，确认名单和解析器边界没有敏感字段。
- `git diff --check` 无输出。
- `git status --short` 只显示本功能和实施前已有的用户未跟踪文件。

- [ ] **Step 8: 运行人工界面验收**

启动 Release 程序并逐项确认：

```powershell
.\out\build\windows-x64-release\douyu_monitor_native.exe
```

验收步骤：

1. 点击房间列表按钮右侧的仓鼠图标，确认导航页打开且房间列表收起。
2. 再次点击仓鼠图标，确认导航页关闭。
3. 点击房间列表按钮，确认导航页关闭且房间列表打开。
4. 在设置页创建四个空队伍，重命名并调整顺序，重启程序后确认仍存在。
5. 给一个已确认房间号的成员分配队伍，确认导航页移动到对应队伍。
6. 给一个未确认房间号的成员手工输入房间号，确认显示更新。
7. 点击快捷加入，确认活动房间列表和左侧房间列表同步更新。
8. 确认导航页没有角色标签，也没有队伍创建、删除或成员分配控件。

- [ ] **Step 9: 提交文档和最终验证记录**

```powershell
git add README.md native/README.md docs/文件职责索引.md docs/superpowers/logs/2026-09-24-hamster-agent-navigation.md
git commit -m "docs: describe hamster agent navigation"
```

## 完成标准

- 顶栏入口顺序为“房间列表按钮 -> 仓鼠特工导航按钮 -> 品牌图标”。
- 导航页与房间列表互斥，状态可持久化。
- 队伍栏位独立于旧分组，支持空队伍、重命名、排序、删除和成员调整。
- 导航页只展示内置公会名单，按队伍分组，未分配主播进入“未分队”。
- 导航页不显示团长、队长、队员、OB 等角色标签。
- 已确认房间号可以一键加入；未确认房间号必须人工确认，禁止静默模糊匹配。
- 房间号解析有串行队列、退避和持久化缓存，不会对斗鱼搜索接口连续突发请求。
- 新增领域测试、控制器测试、持久化测试和 QML 测试通过。
- Release 主程序构建、全量 CTest、自测和人工界面验收有实际证据。
- 真实斗鱼长时多路播放、弹幕稳定性和性能仍作为目标环境验证边界明确报告。
