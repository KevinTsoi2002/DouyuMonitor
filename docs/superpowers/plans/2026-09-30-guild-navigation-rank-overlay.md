# 导航页野榜同步与悬停卡片 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在公会导航页中按“团长、队伍、队长、队员、其他”展示主播，并用野榜的角色、雷达评分、定级赛总评和游乐值驱动悬停卡片；导航页可见期间每 60 秒检查一次版本，仅在快照变化时刷新数据。

**Architecture:** `MaoziRankClient` 负责匿名会话、轻量版本检查和完整快照解析，并构建不可变的匹配索引。`AppController` 负责把野榜条目、导航名单和持久化队伍组合成 QML 可直接消费的投影；QML 只负责渲染、悬停和详情跳转。角色以野榜身份集合为权威来源；“其他”只包含导航页中存在但野榜快照中不存在的主播。

**Tech Stack:** Qt 6.8、C++20、Qt Network、Qt Quick/QML、Qt Test、CMake/CTest、现有 CloudBase 野榜接口。

---

## 锁定决策

- 野榜身份集合是角色权威来源，本地名单后缀不参与角色划分。
- 队长由野榜 `CAPTAIN_DOUYU_IDS` 对应的房间号决定；团长由野榜 `LEADER_DOUYU_IDS` 决定；其余出现在野榜中的主播为队员。
- “其他”只包含导航页名单中存在、但当前野榜 `hosts` 中不存在的主播。
- 导航排序层级固定为：

```text
团长
  按主播昵称拼音首字母

队伍
  队长
    按主播昵称拼音首字母
  队员
    按主播昵称拼音首字母
  其他
    按主播昵称拼音首字母
```

- 团长位于所有队伍之前且不重复放入队伍。没有野榜记录的导航成员保留在原有队伍下并归入“其他”。
- 主播名匹配优先于房间号匹配；靓号、普通房间号差异不视为冲突。
- 已确认的导航名到野榜名列映射集中维护；当前固定包含
  `雾蒙蒙y -> 雾萌萌y`，匹配时不得使用模糊包含规则。
- 悬停卡片使用一个共享顶层 `Popup`，不放入 `Flickable` 内部，避免被 `clip: true` 裁切。
- 悬停只读取内存索引，不触发网络请求。
- 导航页可见时每 60 秒调用一次 `fn_rank_check`；版本变化后才调用 `fn_rank_snapshot`。
- 导航页关闭或应用进入后台时暂停检查；恢复可见时立即检查一次。
- 同步失败保留最后一次成功快照，并显示“数据可能已过期”。
- 定级赛使用网页“得分统计”的“计算总分（总评）”口径。

## 数据契约

### 野榜角色

固定身份集合必须来源于野榜前端当前规则，并在 C++ 中集中定义：

```cpp
const QSet<QString> kCaptainRoomIds{
    QStringLiteral("731252"),
    QStringLiteral("7204164"),
    QStringLiteral("80432"),
    QStringLiteral("6151194"),
    QStringLiteral("217331"),
    QStringLiteral("2632018"),
    QStringLiteral("11222"),
    QStringLiteral("2140934"),
};
const QSet<QString> kLeaderRoomIds{
    QStringLiteral("320155"),
};
```

角色枚举固定为 `leader`、`captain`、`member`、`other`。

### 定级赛总评

每个项目/场次只统计野榜 `placement` 中 `isMember(host)` 的主播。单项得分规则：

```text
N <= 1                  -> 94
有效成绩                -> 46 + 48 * ((N - rank) / (N - 1))^0.6
无成绩、invalid、弃权     -> 40（该场已有任何有效成绩时）
该场完全无有效成绩        -> 不产生得分
```

同一名次使用并列名次，并采用 competition rank：下一次名次跳过并列人数。多张 `metric == "score"` 试卷先求该主播的试卷总分，再按总分排名套公式。总评是所有已计分场次得分的算术平均值，保留两位小数用于悬停显示。

### 导航成员投影

`AppController.guildRoster` 每项至少增加：

```cpp
{
    {QStringLiteral("role"), role},
    {QStringLiteral("pinyinKey"), pinyinKey},
    {QStringLiteral("rankMatched"), matched},
    {QStringLiteral("rankHostId"), hostId},
    {QStringLiteral("radarDimensions"), dimensions},
    {QStringLiteral("placementAverage"), placementAverage},
    {QStringLiteral("placementScoredSessions"), scoredSessions},
    {QStringLiteral("playValue"), playValue},
    {QStringLiteral("playValueUpdatedAt"), playValueUpdatedAt},
}
```

`role` 只允许 `leader/captain/member/other`；`pinyinKey` 使用大写 A-Z，非中文和数字名称统一放入 `#`。

## 文件结构

**修改**

- `native/src/app/maozi_rank_client.h`：增加版本检查、同步状态、角色和导航匹配投影。
- `native/src/app/maozi_rank_client.cpp`：解析 `placement/playvalue`、执行定级赛复算、建立匹配索引。
- `native/tests/maozi_rank_client_test.cpp`：扩展快照 fixture，覆盖角色、定级赛、游乐值和版本检查。
- `native/src/workspace/guild_roster.h`：给领域名单增加拼音排序键。
- `native/src/workspace/guild_roster.cpp`：从资源读取或计算 `pinyinKey`。
- `native/app/resources/hamster_agent_roster.json`：一次性写入 58 名成员的 `pinyinKey`。
- `native/tests/guild_roster_test.cpp`：验证全部成员的排序键合法且稳定。
- `native/src/ui/app_controller.h`：暴露导航数据版本和同步状态查询。
- `native/src/ui/app_controller.cpp`：把野榜索引合并进 `guildRoster` 投影。
- `native/app/qml/components/GuildNavigationPanel.qml`：角色和队伍分层排序、可见期同步调度。
- `native/app/qml/components/GuildMemberRow.qml`：发出悬停请求和详情请求。
- `native/app/qml/Main.qml`：承载共享悬停卡片和详情跳转。
- `native/CMakeLists.txt`：注册新 QML 组件和测试。
- `native/tests/qml_interaction_test.cpp`：验证排序、悬停、详情跳转和同步触发。
- `native/tests/qml_visual_regression_test.cpp`：验证卡片不裁切、不遮挡导航行。
- `native/docs/visual-validation.md`：增加悬停卡片视觉检查说明。
- `docs/文件职责索引.md`：补充新组件和数据流。

**新增**

- `native/app/qml/components/GuildRankHoverCard.qml`：共享悬停卡片，显示雷达图、定级赛总评、游乐值。

## Task 1: 为导航名单增加拼音排序键

**Files:**
- Modify: `native/app/resources/hamster_agent_roster.json`
- Modify: `native/src/workspace/guild_roster.h`
- Modify: `native/src/workspace/guild_roster.cpp`
- Test: `native/tests/guild_roster_test.cpp`

- [ ] **Step 1: 写失败测试，锁定排序键**

在 `native/tests/guild_roster_test.cpp` 增加声明：

```cpp
void exposesStablePinyinKeysForEveryMember();
```

增加实现：

```cpp
void GuildRosterTest::exposesStablePinyinKeysForEveryMember()
{
    const QVector<GuildMember> members = GuildRoster::bundled();
    QCOMPARE(members.size(), 58);

    for (const GuildMember &member : members) {
        QVERIFY2(!member.pinyinKey.isEmpty(), qPrintable(member.anchorName));
        QVERIFY2(member.pinyinKey.size() == 1, qPrintable(member.anchorName));
        const QChar key = member.pinyinKey.at(0);
        QVERIFY2((key >= QLatin1Char('A') && key <= QLatin1Char('Z'))
                     || key == QLatin1Char('#'),
                 qPrintable(QStringLiteral("%1 -> %2").arg(member.anchorName, member.pinyinKey)));
    }

    QCOMPARE(GuildRoster::findByName(QStringLiteral("寅子"))->pinyinKey,
             QStringLiteral("Y"));
    QCOMPARE(GuildRoster::findByName(QStringLiteral("主播阿飞"))->pinyinKey,
             QStringLiteral("Z"));
    QCOMPARE(GuildRoster::findByName(QStringLiteral("阿愈Ayu"))->pinyinKey,
             QStringLiteral("A"));
    QCOMPARE(GuildRoster::findByName(QStringLiteral("bulaQoQ"))->pinyinKey,
             QStringLiteral("B"));
}
```

- [ ] **Step 2: 运行测试，确认失败**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R guild_roster_test --output-on-failure
```

Expected: 编译失败，`GuildMember` 没有 `pinyinKey`。

- [ ] **Step 3: 扩展名单数据契约**

在 `native/src/workspace/guild_roster.h` 修改：

```cpp
struct GuildMember {
    QString id;
    QString anchorName;
    QString searchName;
    QString roomId;
    QString pinyinKey;

    bool operator==(const GuildMember &) const = default;
};
```

在 `native/app/resources/hamster_agent_roster.json` 为每个成员增加固定 `pinyinKey`。使用下列完整映射：

```json
{
  "hamster-001": "Y", "hamster-002": "Z", "hamster-003": "W",
  "hamster-004": "Z", "hamster-005": "S", "hamster-006": "Z",
  "hamster-007": "G", "hamster-008": "L", "hamster-009": "M",
  "hamster-010": "Z", "hamster-011": "N", "hamster-012": "E",
  "hamster-013": "A", "hamster-014": "N", "hamster-015": "S",
  "hamster-016": "N", "hamster-017": "K", "hamster-018": "L",
  "hamster-019": "S", "hamster-020": "C", "hamster-021": "I",
  "hamster-022": "Y", "hamster-023": "N", "hamster-024": "Q",
  "hamster-025": "X", "hamster-026": "X", "hamster-027": "X",
  "hamster-028": "X", "hamster-029": "F", "hamster-030": "X",
  "hamster-031": "F", "hamster-032": "Y", "hamster-033": "T",
  "hamster-034": "M", "hamster-035": "X", "hamster-036": "M",
  "hamster-037": "C", "hamster-038": "X", "hamster-039": "Y",
  "hamster-040": "S", "hamster-041": "N", "hamster-042": "Q",
  "hamster-043": "D", "hamster-044": "T", "hamster-045": "S",
  "hamster-046": "B", "hamster-047": "S", "hamster-048": "W",
  "hamster-049": "X", "hamster-050": "W", "hamster-051": "L",
  "hamster-052": "F", "hamster-053": "Z", "hamster-054": "B",
  "hamster-055": "Y", "hamster-056": "X", "hamster-057": "A",
  "hamster-058": "H"
}
```

每条 JSON 条目增加字段，例如：

```json
{ "id": "hamster-001", "name": "寅子", "roomId": "71415", "pinyinKey": "Y" }
```

- [ ] **Step 4: 解析并校验排序键**

在 `native/src/workspace/guild_roster.cpp` 的 `parse()` 中读取：

```cpp
const QString pinyinKey =
    object.value(QStringLiteral("pinyinKey")).toString().trimmed().toUpper();
```

加入有效性条件：

```cpp
const bool validPinyinKey =
    pinyinKey.size() == 1
    && ((pinyinKey.at(0) >= QLatin1Char('A') && pinyinKey.at(0) <= QLatin1Char('Z'))
        || pinyinKey.at(0) == QLatin1Char('#'));
if (id.isEmpty() || rawName.isEmpty() || ids.contains(id)
    || (!roomId.isEmpty() && !isValidRoomId(roomId))
    || !validPinyinKey) {
    continue;
}
```

构造成员时传入：

```cpp
members.push_back({
    id,
    anchorName,
    searchName(rawName),
    roomId,
    pinyinKey,
});
```

- [ ] **Step 5: 运行名单测试**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R guild_roster_test --output-on-failure
```

Expected: `guild_roster_test` 通过，58 条名单全部有稳定排序键。

- [ ] **Step 6: 提交**

```powershell
git add native/app/resources/hamster_agent_roster.json native/src/workspace/guild_roster.h native/src/workspace/guild_roster.cpp native/tests/guild_roster_test.cpp
git commit -m "feat: add stable pinyin keys to guild roster"
```

## Task 2: 扩展野榜客户端的数据模型和定级赛复算

**Files:**
- Modify: `native/src/app/maozi_rank_client.h`
- Modify: `native/src/app/maozi_rank_client.cpp`
- Test: `native/tests/maozi_rank_client_test.cpp`

- [ ] **Step 1: 写失败测试，锁定角色、游乐值和总评**

在 `native/tests/maozi_rank_client_test.cpp` 增加声明：

```cpp
void exposesRolesPlacementAveragePlayValueAndLookupIndexes();
void recalculatesPlacementTotalWithCompetitionRanking();
```

在 `snapshotBody()` 中增加队长和团长条目，并加入最小可复算的 placement：

```cpp
QJsonObject captainHost{
    {QStringLiteral("id"), QStringLiteral("host-captain")},
    {QStringLiteral("name"), QStringLiteral("尐表哥")},
    {QStringLiteral("guild"), QStringLiteral("仓鼠特工")},
    {QStringLiteral("douyu_id"), QStringLiteral("217331")},
    {QStringLiteral("team"), 0},
};
QJsonObject leaderHost{
    {QStringLiteral("id"), QStringLiteral("host-leader")},
    {QStringLiteral("name"), QStringLiteral("主播阿郎")},
    {QStringLiteral("guild"), QStringLiteral("仓鼠特工")},
    {QStringLiteral("douyu_id"), QStringLiteral("320155")},
    {QStringLiteral("team"), QJsonValue::Null},
};
```

加入 `placements` 对象和游乐值：

```cpp
auto result = [](const QString &hostId, int value, bool invalid = false) {
    return QJsonObject{
        {QStringLiteral("slot"), 1},
        {QStringLiteral("stage"), 1},
        {QStringLiteral("value"), invalid ? QJsonValue::Null : QJsonValue(value)},
        {QStringLiteral("host_id"), hostId},
        {QStringLiteral("invalid"), invalid},
    };
};
const QJsonObject day{
    {QStringLiteral("ok"), true},
    {QStringLiteral("day"), QStringLiteral("2026-09-28")},
    {QStringLiteral("events"), QJsonArray{
         QJsonObject{{QStringLiteral("slot"), 1}, {QStringLiteral("name"), QStringLiteral("测试")}},
         QJsonObject{{QStringLiteral("slot"), 2}, {QStringLiteral("name"), QStringLiteral("未开始")}},
     }},
    {QStringLiteral("stages"), QJsonArray{
         QJsonObject{{QStringLiteral("slot"), 1}, {QStringLiteral("stage"), 1},
                     {QStringLiteral("metric"), QStringLiteral("time")},
                     {QStringLiteral("dir"), QStringLiteral("asc")}},
     }},
    {QStringLiteral("results"), QJsonArray{
         result(QStringLiteral("host-1"), 10),
         result(QStringLiteral("host-2"), 20),
     }},
};
QJsonObject placements{
    {QStringLiteral("2026-09-28"), day},
};
```

在根对象加入：

```cpp
{QStringLiteral("placement"), placements},
{QStringLiteral("playvalue"), QJsonArray{
    QJsonObject{{QStringLiteral("uid"), QStringLiteral("100001")},
                {QStringLiteral("nickname"), QStringLiteral("寅子")},
                {QStringLiteral("points"), 6.4},
                {QStringLiteral("bombed"), 1},
                {QStringLiteral("updated_at"), QStringLiteral("2026-09-30T12:26:01+08:00")}},
}},
```

增加断言：

```cpp
void MaoziRankClientTest::exposesRolesPlacementAveragePlayValueAndLookupIndexes()
{
    CloudBaseFixture fixture;
    QVERIFY(fixture.listen());
    MaoziRankClient client(fixture.authUrl(), fixture.snapshotUrl(), 1000);

    client.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(client.loading(), false, 3000);
    QCOMPARE(client.errorMessage(), QString());

    const QVariantMap captain = client.entryForRoomId(QStringLiteral("217331"));
    QCOMPARE(captain.value(QStringLiteral("role")).toString(), QStringLiteral("captain"));
    const QVariantMap leader = client.entryForRoomId(QStringLiteral("320155"));
    QCOMPARE(leader.value(QStringLiteral("role")).toString(), QStringLiteral("leader"));
    const QVariantMap member = client.entryForRoomId(QStringLiteral("71415"));
    QCOMPARE(member.value(QStringLiteral("role")).toString(), QStringLiteral("member"));
    QCOMPARE(member.value(QStringLiteral("playValue")).toDouble(), 6.4);
    QCOMPARE(member.value(QStringLiteral("playValueBombed")).toInt(), 1);
    QCOMPARE(member.value(QStringLiteral("placementAverage")).toDouble(), 94.0);
}

void MaoziRankClientTest::recalculatesPlacementTotalWithCompetitionRanking()
{
    QCOMPARE(MaoziRankClient::placementScore(3, 1), 94.0);
    QVERIFY(qAbs(MaoziRankClient::placementScore(3, 2) - 77.6746) < 0.01);
    QCOMPARE(MaoziRankClient::placementScore(3, 3), 46.0);
    QCOMPARE(MaoziRankClient::placementScore(1, 1), 94.0);
}
```

- [ ] **Step 2: 运行测试，确认失败**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R maozi_rank_client_test --output-on-failure
```

Expected: 编译失败，客户端尚无 `placementScore()` 和角色/游乐值投影。

- [ ] **Step 3: 增加公开接口**

在 `native/src/app/maozi_rank_client.h` 增加公开方法和静态函数：

```cpp
Q_INVOKABLE QVariantMap entryForRoomId(const QString &roomId) const;
Q_INVOKABLE QVariantMap entryForName(const QString &name) const;
Q_INVOKABLE void checkForChanges();
bool syncPending() const noexcept { return syncPending_; }
QString snapshotVersion() const { return snapshotVersion_; }
QString lastSyncError() const { return lastSyncError_; }

static double placementScore(int competitorCount, int rank);

signals:
    void syncStateChanged();
```

增加私有方法和成员：

```cpp
void requestVersionCheck(const QString &accessToken);
void handleVersionReply(QNetworkReply *reply);
void applyEntries(QVariantList entries,
                  int totalVoters,
                  const QString &updatedAt,
                  const QString &version);
static QString roleForRoomId(const QString &roomId);

QHash<QString, int> entryIndexByRoomId_;
QHash<QString, int> entryIndexByName_;
QString snapshotVersion_;
QString lastSyncError_;
bool syncPending_ = false;
```

- [ ] **Step 4: 完善匿名会话和轻量版本检查**

保持首次 `refresh()` 拉取完整快照。新增 `checkForChanges()`，它只调用：

```text
POST /v1/rdb/rest/rpc/fn_rank_check
body: {"p_dev": false}
```

版本请求成功后比较 `v`。一致时只发出 `syncStateChanged()`，不请求完整快照；不一致时调用 `requestSnapshot()`。版本检查失败时设置：

```cpp
lastSyncError_ = QStringLiteral("数据可能已过期");
```

并保留 `entries_`，不得清空现有导航数据。

- [ ] **Step 5: 解析角色、定级赛和游乐值**

在 `native/src/app/maozi_rank_client.cpp` 增加固定集合：

```cpp
const QSet<QString> kCaptainRoomIds{
    QStringLiteral("731252"), QStringLiteral("7204164"),
    QStringLiteral("80432"), QStringLiteral("6151194"),
    QStringLiteral("217331"), QStringLiteral("2632018"),
    QStringLiteral("11222"), QStringLiteral("2140934"),
};
const QSet<QString> kLeaderRoomIds{
    QStringLiteral("320155"),
};
```

角色函数：

```cpp
QString MaoziRankClient::roleForRoomId(const QString &roomId)
{
    if (kLeaderRoomIds.contains(roomId)) return QStringLiteral("leader");
    if (kCaptainRoomIds.contains(roomId)) return QStringLiteral("captain");
    return QStringLiteral("member");
}
```

在每条野榜条目中加入：

```cpp
{QStringLiteral("role"), roleForRoomId(roomId)},
{QStringLiteral("playValue"), QJsonValue::Null},
{QStringLiteral("playValueBombed"), 0},
{QStringLiteral("playValueUpdatedAt"), QString()},
{QStringLiteral("placementAverage"), -1.0},
{QStringLiteral("placementScoredSessions"), 0},
```

构建游乐值索引：

```cpp
QHash<QString, QJsonObject> playValuesByName;
for (const QJsonValue &value : root.value(QStringLiteral("playvalue")).toArray()) {
    const QJsonObject item = value.toObject();
    const QString key = item.value(QStringLiteral("nickname")).toString().trimmed().toCaseFolded();
    if (!key.isEmpty()) playValuesByName.insert(key, item);
}
```

按规范化主播名把 `points/bombed/updated_at` 写入对应条目。

实现定级赛复算。核心静态得分函数固定为：

```cpp
double MaoziRankClient::placementScore(int competitorCount, int rank)
{
    if (rank <= 0) return 0.0;
    if (competitorCount <= 1) return 94.0;
    const double normalized =
        static_cast<double>(competitorCount - rank) / (competitorCount - 1);
    return 46.0 + 48.0 * std::pow(normalized, 0.6);
}
```

对每个日期和 slot：

- 收集该场所有有效成绩并按 `metric/dir` 排序。
- 使用 competition rank；并列值共享最小名次，下一名次增加并列人数。
- 每名主播累加该场得分。
- 总评为已计分场次平均，未计分场次不参与平均。

- [ ] **Step 6: 构建匹配索引并排序**

在 `applyEntries()` 中重建：

```cpp
entryIndexByRoomId_.clear();
entryIndexByName_.clear();
for (int index = 0; index < entries_.size(); ++index) {
    const QVariantMap entry = entries_.at(index).toMap();
    const QString roomId = entry.value(QStringLiteral("roomId")).toString().trimmed();
    const QString nameKey = entry.value(QStringLiteral("name"))
                                .toString().trimmed().toCaseFolded();
    if (!roomId.isEmpty()) entryIndexByRoomId_.insert(roomId, index);
    if (!nameKey.isEmpty()) entryIndexByName_.insert(nameKey, index);
}
```

`entryForName()` 在哈希查询前应用集中维护的已知别名映射：

```cpp
static QString canonicalLookupName(const QString &name)
{
    static const QHash<QString, QString> knownAliases{
        {QStringLiteral("雾蒙蒙y"), QStringLiteral("雾萌萌y")},
    };
    const QString trimmed = name.trimmed();
    return knownAliases.value(trimmed, trimmed);
}
```

`entryForRoomId()` 和 `entryForName()` 只做哈希查询，返回空映射表示未命中。

- [ ] **Step 7: 运行客户端测试**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R maozi_rank_client_test --output-on-failure
```

Expected: `maozi_rank_client_test` 通过，角色、定级赛总评、游乐值和两个匹配索引均有断言。

- [ ] **Step 8: 提交**

```powershell
git add native/src/app/maozi_rank_client.h native/src/app/maozi_rank_client.cpp native/tests/maozi_rank_client_test.cpp
git commit -m "feat: parse rank roles placement and play values"
```

## Task 3: 合并导航名单和野榜索引

**Files:**
- Modify: `native/src/ui/app_controller.h`
- Modify: `native/src/ui/app_controller.cpp`
- Test: `native/tests/app_controller_test.cpp`

- [ ] **Step 1: 写失败测试，锁定匹配和“其他”规则**

在 `native/tests/app_controller_test.cpp` 增加：

```cpp
void joinsGuildRosterWithRankEntryByPreferredIdentity();
void marksGuildMembersMissingFromRankAsOther();
```

测试必须使用可控的伪野榜客户端，而不是真实网络。断言：

```cpp
// 寅子同时有普通房间号和野榜条目时，名称匹配成功。
QCOMPARE(entry.value(QStringLiteral("role")).toString(), QStringLiteral("member"));
QCOMPARE(entry.value(QStringLiteral("rankMatched")).toBool(), true);

// 导航页中存在但野榜 hosts 中不存在的主播。
QCOMPARE(entry.value(QStringLiteral("role")).toString(), QStringLiteral("other"));
QCOMPARE(entry.value(QStringLiteral("rankMatched")).toBool(), false);
QCOMPARE(entry.value(QStringLiteral("placementAverage")).toDouble(), -1.0);
```

- [ ] **Step 2: 运行测试，确认失败**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R app_controller_test --output-on-failure
```

Expected: 编译失败，`guildRoster` 投影还没有野榜字段。

- [ ] **Step 3: 在 AppController 合并投影**

在 `AppController::guildRoster()` 中，保持已有实时房间状态字段，并增加野榜合并。匹配顺序固定为：

```cpp
QVariantMap rankEntry;
if (maoziRank_ != nullptr) {
    rankEntry = maoziRank_->entryForName(member.anchorName);
    if (rankEntry.isEmpty()) rankEntry = maoziRank_->entryForRoomId(resolvedRoomId);
    if (rankEntry.isEmpty() && member.searchName != member.anchorName) {
        rankEntry = maoziRank_->entryForName(member.searchName);
    }
}
const bool matched = !rankEntry.isEmpty();
const QString role = matched
    ? rankEntry.value(QStringLiteral("role")).toString()
    : QStringLiteral("other");
```

投影至少增加：

```cpp
{QStringLiteral("pinyinKey"), member.pinyinKey},
{QStringLiteral("role"), role},
{QStringLiteral("rankMatched"), matched},
{QStringLiteral("rankHostId"), rankEntry.value(QStringLiteral("id"))},
{QStringLiteral("radarDimensions"), rankEntry.value(QStringLiteral("dimensions"))},
{QStringLiteral("placementAverage"),
 rankEntry.value(QStringLiteral("placementAverage"), -1.0)},
{QStringLiteral("placementScoredSessions"),
 rankEntry.value(QStringLiteral("placementScoredSessions"), 0)},
{QStringLiteral("playValue"), rankEntry.value(QStringLiteral("playValue"))},
{QStringLiteral("playValueBombed"),
 rankEntry.value(QStringLiteral("playValueBombed"), 0)},
{QStringLiteral("playValueUpdatedAt"),
 rankEntry.value(QStringLiteral("playValueUpdatedAt"))},
```

在野榜 `entriesChanged` 或 `syncStateChanged` 时发出 `guildRosterChanged()`，让悬停卡片和导航页即时更新。

- [ ] **Step 4: 暴露同步状态**

在 `AppController` 增加只读属性：

```cpp
Q_PROPERTY(bool rankSyncPending READ rankSyncPending NOTIFY rankSyncStateChanged)
Q_PROPERTY(QString rankSyncError READ rankSyncError NOTIFY rankSyncStateChanged)
```

实现委托给 `MaoziRankClient`，不要复制状态。

- [ ] **Step 5: 运行控制器测试**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R app_controller_test --output-on-failure
```

Expected: `app_controller_test` 通过，名称匹配优先，“其他”分类和空数据回退正确。

- [ ] **Step 6: 提交**

```powershell
git add native/src/ui/app_controller.h native/src/ui/app_controller.cpp native/tests/app_controller_test.cpp
git commit -m "feat: merge rank data into guild navigation"
```

## Task 4: 导航页按野榜角色和队伍排序

**Files:**
- Modify: `native/app/qml/components/GuildNavigationPanel.qml`
- Test: `native/tests/qml_interaction_test.cpp`

- [ ] **Step 1: 写失败测试，锁定层级和拼音排序**

在 `native/tests/qml_interaction_test.cpp` 增加：

```cpp
void ordersGuildNavigationByRoleTeamAndPinyin();
```

构造包含下列成员的假数据：

```cpp
leader:      主播阿郎, pinyinKey Q
captain:     尐表哥,   pinyinKey S
member:      阿飞,     pinyinKey A
other:       白小帅子, pinyinKey B
```

断言 `navigationRows` 的 `rowType/title/memberId` 顺序固定为：

```text
header 团长
member 主播阿郎
header 一队
header 队长
member 尐表哥
header 队员
member 阿飞
header 其他
member 白小帅子
```

- [ ] **Step 2: 运行测试，确认失败**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R qml_interaction_test --output-on-failure
```

Expected: 当前导航页没有角色子标题，测试失败。

- [ ] **Step 3: 重构队伍区块构建**

在 `GuildNavigationPanel.qml` 中把 `buildTeamSections()` 拆成纯排序函数：

```js
function roleOrder(role) {
    if (role === "captain") return 0
    if (role === "member") return 1
    return 2
}

function compareMembers(left, right) {
    return String(left.pinyinKey || "#").localeCompare(String(right.pinyinKey || "#"))
        || String(left.anchorName || "").localeCompare(String(right.anchorName || ""))
}

function sortMembers(members) {
    return members.sort(compareMembers)
}
```

固定输出：

```js
{
    teamId: "...",
    title: "...",
    isUnassigned: false,
    groups: [
        { role: "captain", title: "队长", members: [] },
        { role: "member", title: "队员", members: [] },
        { role: "other", title: "其他", members: [] }
    ]
}
```

团长单独收集为一个前置区块：

```js
{
    teamId: "",
    title: "团长",
    isLeaderSection: true,
    members: sortedLeaders
}
```

搜索仍只过滤成员，不改变团长、队伍和角色标题顺序。

- [ ] **Step 4: 投影导航行**

`rebuildNavigationRows()` 固定追加：

```js
navigationRows.append({
    rowType: "header",
    title: section.title,
    memberId: "",
    anchorName: "",
    roomId: "",
    roomStatus: "",
    avatarUrl: "",
    liveState: "unknown",
    memberActive: false
})
navigationRows.append({
    rowType: "subheader",
    title: group.title,
    memberId: "",
    anchorName: "",
    roomId: "",
    roomStatus: "",
    avatarUrl: "",
    liveState: "unknown",
    memberActive: false
})
```

行高固定为：`header = 24`、`subheader = 20`、`member = 46`。

- [ ] **Step 5: 运行导航测试**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R qml_interaction_test --output-on-failure
```

Expected: 角色层级、队伍层级和拼音排序断言通过。

- [ ] **Step 6: 提交**

```powershell
git add native/app/qml/components/GuildNavigationPanel.qml native/tests/qml_interaction_test.cpp
git commit -m "feat: order guild navigation by rank role and pinyin"
```

## Task 5: 增加共享悬停卡片和详情跳转

**Files:**
- Create: `native/app/qml/components/GuildRankHoverCard.qml`
- Modify: `native/app/qml/components/GuildMemberRow.qml`
- Modify: `native/app/qml/components/GuildNavigationPanel.qml`
- Modify: `native/app/qml/Main.qml`
- Modify: `native/CMakeLists.txt`
- Test: `native/tests/qml_interaction_test.cpp`

- [ ] **Step 1: 写失败测试，锁定悬停和详情**

增加：

```cpp
void showsGuildRankHoverCardAfterDelay();
void opensMaoziRankDetailsForHoveredMember();
```

第一条断言悬停 500 ms 后共享卡片可见，并显示：

```text
主播名
定级赛总评
游乐值
雷达图
```

第二条断言点击“查看详情”后：

```cpp
QCOMPARE(window->property("currentView").toString(), QStringLiteral("maoziRank"));
QCOMPARE(page->property("query").toString(), QStringLiteral("寅子"));
```

- [ ] **Step 2: 运行测试，确认失败**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R qml_interaction_test --output-on-failure
```

Expected: `guildRankHoverCard` 不存在。

- [ ] **Step 3: 创建共享悬停卡片**

创建 `native/app/qml/components/GuildRankHoverCard.qml`。属性固定为：

```qml
property var member: null
property bool hasRankData: false
signal detailsRequested(string query)
```

卡片结构：

```text
头像 + 主播名 + 角色/队伍
雷达图
综合评级 + 综合评分
定级赛总评 + 已计分场次
游乐值 + 更新时间
查看详情
```

雷达图使用一个 `Canvas`，维度来自 `member.radarDimensions`。没有野榜数据时隐藏雷达图和数值，显示“暂无野榜数据”。卡片固定宽度 `320`，最大高度 `420`，使用 `Theme.managementSurface` 和 `Theme.border`。

- [ ] **Step 4: 从成员行发出悬停事件**

在 `GuildMemberRow.qml` 增加：

```qml
signal hoverEntered(string memberId)
signal hoverExited(string memberId)
signal detailsRequested(string memberId)

HoverHandler {
    id: memberHover
}

onHoveredChanged: {
    if (hovered) root.hoverEntered(root.memberId)
    else root.hoverExited(root.memberId)
}
```

`GuildNavigationPanel.qml` 将事件冒泡到 `Main.qml`。

- [ ] **Step 5: 在 Main.qml 承载单个 Popup**

在 `Main.qml` 顶层增加：

```qml
GuildRankHoverCard {
    id: guildRankHoverCard
    objectName: "guildRankHoverCard"
    parent: root.contentItem
    z: 120
    visible: false
    member: null
}
```

增加状态和函数：

```qml
property string hoveredGuildMemberId: ""
property var hoveredGuildMember: null
Timer {
    id: guildHoverTimer
    interval: 450
    repeat: false
    onTriggered: {
        root.hoveredGuildMember = root.guildMemberById(root.hoveredGuildMemberId)
        guildRankHoverCard.member = root.hoveredGuildMember
        guildRankHoverCard.visible = root.hoveredGuildMember !== null
    }
}
```

悬停进入只重置计时器；离开时停止并隐藏。卡片位置放在导航行右侧，并在右边界不足时翻转到左侧。

- [ ] **Step 6: 接通详情跳转**

`GuildRankHoverCard.detailsRequested(query)` 连接到：

```qml
onDetailsRequested: function(query) {
    guildRankHoverCard.visible = false
    root.currentView = "maoziRank"
    if (root.appController && root.appController.maoziRank) {
        root.appController.maoziRank.refresh()
    }
    maoziRankPage.query = query
}
```

- [ ] **Step 7: 注册 QML 文件**

在 `native/CMakeLists.txt` 的所有 QML 清单、资源清单和测试资源清单中，紧邻 `GuildMemberRow.qml` 增加：

```cmake
app/qml/components/GuildRankHoverCard.qml
```

- [ ] **Step 8: 运行交互测试**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R qml_interaction_test --output-on-failure
```

Expected: 悬停延迟、共享卡片、无数据回退、详情跳转和查询预填全部通过。

- [ ] **Step 9: 提交**

```powershell
git add native/app/qml/components/GuildRankHoverCard.qml native/app/qml/components/GuildMemberRow.qml native/app/qml/components/GuildNavigationPanel.qml native/app/qml/Main.qml native/CMakeLists.txt native/tests/qml_interaction_test.cpp
git commit -m "feat: add guild navigation rank hover card"
```

## Task 6: 导航页可见期自动同步

**Files:**
- Modify: `native/app/qml/components/GuildNavigationPanel.qml`
- Modify: `native/app/qml/Main.qml`
- Modify: `native/src/app/maozi_rank_client.cpp`
- Test: `native/tests/maozi_rank_client_test.cpp`
- Test: `native/tests/qml_interaction_test.cpp`

- [ ] **Step 1: 写失败测试，锁定版本检查和暂停行为**

在 `maozi_rank_client_test.cpp` 增加：

```cpp
void skipsSnapshotWhenVersionIsUnchanged();
```

让 fixture 记录 `/fn_rank_check` 和 `/fn_rank_snapshot` 请求次数。第一次 `refresh()` 后记录一次快照基线；再次调用 `checkForChanges()` 且版本一致时，断言完整快照请求数不变。

在 `qml_interaction_test.cpp` 增加：

```cpp
void checksRankVersionOnlyWhileGuildNavigationIsVisible();
```

断言导航面板显示时启动 60 秒计时器，隐藏时停止计时器。

- [ ] **Step 2: 运行测试，确认失败**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R "maozi_rank_client_test|qml_interaction_test" --output-on-failure
```

Expected: 无版本检查或可见期计时器，测试失败。

- [ ] **Step 3: 实现 60 秒可见期调度**

在 `GuildNavigationPanel.qml` 增加：

```qml
property bool syncActive: root.visible
Timer {
    id: rankSyncTimer
    interval: 60000
    repeat: true
    running: root.syncActive
    onTriggered: root.checkRankForChanges()
}

function checkRankForChanges() {
    if (!root.controller || !root.controller.maoziRank) return
    root.controller.maoziRank.checkForChanges()
}

onVisibleChanged: {
    if (visible) {
        checkRankForChanges()
    } else if (rankSyncTimer.running) {
        rankSyncTimer.restart()
    }
}
Component.onCompleted: if (visible) checkRankForChanges()
```

`Main.qml` 将窗口最小化或后台托管时把导航 `syncActive` 置为 `false`；恢复时立即检查一次。位于 `MaoziRankPage` 时不要重复启动第二套导航计时器。

- [ ] **Step 4: 失败时保留最后一次成功数据**

确保版本检查和快照错误路径：

- 不清空 `entries_`
- 不清空匹配索引
- 不触发导航列表消失
- 设置 `lastSyncError = "数据可能已过期"`
- 悬停卡片显示同步错误提示

- [ ] **Step 5: 运行同步测试**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R "maozi_rank_client_test|qml_interaction_test" --output-on-failure
```

Expected: 版本一致不拉快照，版本变化拉取快照，导航隐藏时计时器停止，失败保留旧数据。

- [ ] **Step 6: 提交**

```powershell
git add native/src/app/maozi_rank_client.cpp native/app/qml/components/GuildNavigationPanel.qml native/app/qml/Main.qml native/tests/maozi_rank_client_test.cpp native/tests/qml_interaction_test.cpp
git commit -m "feat: sync guild rank data while navigation is visible"
```

## Task 7: 视觉回归、文档和全量验证

**Files:**
- Modify: `native/tests/qml_visual_regression_test.cpp`
- Modify: `native/docs/visual-validation.md`
- Modify: `docs/文件职责索引.md`

- [ ] **Step 1: 增加悬停卡片视觉断言**

在 `qml_visual_regression_test.cpp` 增加：

```cpp
void keepsGuildRankHoverCardInsideViewport();
```

使用 284x720 导航窗口和一条有完整野榜数据的主播。执行：

```cpp
QQuickItem *row = visualItemByObjectName(window->contentItem(), QStringLiteral("guildMemberRow"));
QVERIFY(row != nullptr);
QTest::mouseMove(window.get(), row->mapToScene(QPointF(row->width() / 2, row->height() / 2)).toPoint());
QTRY_VERIFY_WITH_TIMEOUT(
    visualItemByObjectName(window->contentItem(), QStringLiteral("guildRankHoverCard"))->isVisible(),
    1500);
const QRectF cardRect =
    visualSceneRect(visualItemByObjectName(window->contentItem(), QStringLiteral("guildRankHoverCard")));
QVERIFY(visualSceneRect(window->contentItem()).contains(cardRect));
QVERIFY(cardRect.width() >= 300.0);
QVERIFY(cardRect.height() >= 240.0);
```

捕获基线：

```cpp
const QImage image = visualCapture(window.get(), QStringLiteral("guild-rank-hover-card-284x720"));
QVERIFY(!image.isNull());
verifyBaseline(image, QStringLiteral("guild-rank-hover-card-284x720"));
```

- [ ] **Step 2: 运行视觉回归并生成基线**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R qml_visual_regression_test --output-on-failure
```

Expected: 第一次生成并写入新基线；再次运行通过，卡片完全位于窗口内且不与导航行重叠。

- [ ] **Step 3: 更新视觉验证文档**

在 `native/docs/visual-validation.md` 增加检查项：

- 悬停 450 ms 后共享卡片出现一次。
- 卡片不被导航页 `Flickable` 裁切。
- 卡片在 284x720 左栏内完整可见。
- 无野榜数据时显示“暂无野榜数据”，不显示空 Canvas。
- 网络失败时显示“数据可能已过期”，原数据仍可见。
- 详情跳转后野榜页搜索框预填主播名。

- [ ] **Step 4: 更新文件职责索引**

在 `docs/文件职责索引.md` 增加：

- `native/app/qml/components/GuildRankHoverCard.qml`：共享悬停卡片、雷达图、定级赛总评和游乐值展示。
- `MaoziRankClient`：匿名会话、版本检查、快照解析、角色归属、定级赛复算和匹配索引。
- `GuildNavigationPanel.qml`：团长、队伍、队长、队员、其他分层和可见期同步调度。

- [ ] **Step 5: 构建相关目标**

Run:

```powershell
$env:QT_ROOT = 'D:\Qt\6.8.3\msvc2022_64'
$env:MPV_ROOT = (Resolve-Path .\sdk\mpv).Path
Set-Location native
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release --target douyu_monitor_native guild_roster_test maozi_rank_client_test app_controller_test qml_interaction_test qml_visual_regression_test
```

Expected: 全部目标构建退出码为 `0`。

- [ ] **Step 6: 运行定向回归**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release -R "guild_roster_test|maozi_rank_client_test|app_controller_test|qml_engine_smoke_test|qml_interaction_test|qml_visual_smoke_test|qml_visual_regression_test" --output-on-failure
```

Expected: 所列目标全部通过。

- [ ] **Step 7: 运行全量验证**

Run:

```powershell
Set-Location native
ctest --preset windows-x64-release
.\out\build\windows-x64-release\douyu_monitor_native.exe --self-test
```

Expected:

- 全量 CTest 全部通过，退出码为 `0`。
- 自测输出 `native self-test passed: Qt Quick renderer`。

- [ ] **Step 8: 静态边界检查**

Run:

```powershell
Set-Location D:\DouyuMonitor
rg -n "CAPTAIN_DOUYU_IDS|LEADER_DOUYU_IDS|731252|320155" native/src/app/maozi_rank_client.cpp
rg -n "cookie|token|signature|requestHeaders|playbackUrl" native/app/resources/hamster_agent_roster.json
git diff --check
git status --short
```

Expected:

- 第一条只命中集中定义的野榜身份集合。
- 第二条无输出。
- `git diff --check` 无输出。
- `git status --short` 只显示预期功能文件以及实施前已有的用户未跟踪文件。

- [ ] **Step 9: 人工界面验收**

启动：

```powershell
.\native\out\build\windows-x64-release\douyu_monitor_native.exe
```

验收：

1. 打开导航页，确认团长位于最上方，之后按队伍展示队长、队员、其他。
2. 确认每个小分类内部按主播昵称拼音首字母排序。
3. 确认野榜无记录的主播归入“其他”，且仍可加入直播间。
4. 悬停有野榜记录的主播，确认出现雷达图、定级赛总评和游乐值。
5. 悬停野榜无记录的主播，确认显示“暂无野榜数据”。
6. 点击“查看详情”，确认跳转野榜页并预填主播名。
7. 断开网络后等待同步，确认旧数据保留并显示“数据可能已过期”。
8. 隐藏导航页超过一个同步周期，确认没有新版本检查请求；重新显示后立即检查。

- [ ] **Step 10: 提交文档和验证记录**

```powershell
git add native/tests/qml_visual_regression_test.cpp native/docs/visual-validation.md docs/文件职责索引.md
git commit -m "test: cover guild rank hover synchronization"
```

## 完成标准

- 角色只来源于野榜身份集合；导航名单后缀不参与角色判定。
- “其他”只包含导航页存在但野榜快照不存在的主播。
- 导航层级固定为团长、队伍、队长、队员、其他，并在小分类内按拼音首字母排序。
- 定级赛总评按网页“得分统计”的 competition rank、单项公式和场次平均复算。
- 悬停卡片显示雷达图、定级赛总评、游乐值和数据更新时间，无野榜数据时有明确回退。
- 导航页可见时每 60 秒调用轻量版本接口，版本变化才拉取完整快照。
- 导航页关闭或应用后台时停止轮询，恢复可见时立即检查一次。
- 同步失败保留最后一次成功数据并显示过期提示。
- 名称匹配优先于房间号匹配；靓号不产生误判。
- 新增名单、客户端、控制器、QML 交互和视觉回归测试通过。
- Release 主程序构建、全量 CTest、自测和人工界面验收有实际证据。
- 真实斗鱼长时播放、弹幕稳定性和性能仍作为目标环境验证边界单独报告。
