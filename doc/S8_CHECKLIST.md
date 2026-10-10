# S8_CHECKLIST —— Enemy + AI 状态机 施工图

> 本文件是 S8 的**施工图 + 进度板 + 落地记录**。写代码前先读它；
> 实现过程中发现的偏差**追加**到「落地偏差」表，**不许回头改历史**。
>
> 引用格式：`PLAN:n` = `doc/MiniEngine_PLAN.md` 第 n 行；`A.x` / `D.x` / `E-x` = PLAN 的附录/小节/风险编号；
> `TECH_DEBT:n` = `doc/TECH_DEBT.md` 第 n 行。
>
> ⚠️ 本文件**只到方案层级**（`AGENTS.md` 1.2）：允许出现函数/类**声明**、结构体字段列表、
> 枚举、常量表、伪代码、≤5 行的"用法示意"。**不含任何可直接编译的实现体** —— 实现由本人敲。

---

## 当前状态（2026-10-09：**需求与分叉已定稿，代码未开始**）

- **S0 ~ S7 均已完成并验收**。最近一次全量数字：`mini_tests` **123 passed / 0 failed**
  （2026-10-08 本人回报；收口 commit `d215fa5` 及其前后）。
- **S8 本文件落地的进度**：① 需求说明（已于 2026-10-09 在对话中逐条给出）
  ② **3 个分叉已拍板**（见下节）→ ③ 施工图（本文件）✅ → ④ 实现 ⬜ 未开始。
- **本阶段最值钱的性质（对标 S7 的"`game/` 一行没改"）**：
  **`engine/` 一个字节都不用动** —— `Entity.h`（含 `static_assert(sizeof(Entity) == 24)`）、
  `World.h/.cpp`、`Collision.h`、`Camera.h`、`TileMap.h`、`DrawEntities` 全部零改动。
- **本阶段同时欠下 4 笔账**，收口时要一起写进 `TECH_DEBT`（不许散落在偏差表里）：
  C12 再次推迟 · `Dead` 只有调试键入口 · `F3` 临时面板与 S11 抢键 · 玩家 HP ≤ 0 无事发生。

### ⚠️ 分工约定（`AGENTS.md` 1.4）

| 谁 | 做什么 |
| --- | --- |
| AI | **只改/新建源文件**。不跑 `cmake`、不跑 `ctest`、不跑 `MiniEngine.exe` |
| 本人 | 构建、跑测试、跑 exe、看现象，把结果回报 |

每步报告必须包含：改了哪些文件 / 我该执行什么（PowerShell 原样可粘贴）/ 期望看到什么 /
以及状态标注（**代码已写完，待构建 · 待运行 · 待肉眼验**）。

---

## 分叉定稿（2026-10-09，本人拍板；**3 个都取推荐值**）

| # | 分叉 | **取值** | 一句话理由 |
| --- | --- | --- | --- |
| ① | 敌人数据归属 | **`game` 侧 `std::vector<EnemyData>`；`Entity` 零改动** | 保 `Entity` 24 字节；S9 的 `Projectile` 照抄同构写法 |
| ② | 实体间推挤（C12） | **S8 不做**，触发条件改到 **S10** | `World::Update` 一行不改，S7 收口状态原样保持 |
| ③ | `Dead` 态 | **五态全做**；`Dead` 由**临时调试键**触发；玩家 HP 只扣不减员 | 五态表完整可测；真入口留给 S9 的子弹 |

**三个分叉取的是同一个方向**：把 S8 的全部风险关在 `game/` 一层。
好处是这一阶段几乎**不可能碰红既有 123 条用例**；坏处是同时欠下上节列的 4 笔账。

### 分叉 ① 的好处 / 坏处

**好处**

| # | 好处 | 落到本仓库 |
| --- | --- | --- |
| 1 | `engine/` 零改动 | 改动面被物理关在 `game/` 一个目录里 |
| 2 | 守住 `PLAN A.3` | "`world`/`render` 不认识 Player、Enemy" 是硬规矩；把 `EnemyState`/`hp` 塞进 `Entity` 就是 world 层开始认识"敌人" |
| 3 | 玩家不背垃圾字段 | `aiState`/`waypointIdx`/`attackTimer` 对玩家是纯冗余 |
| 4 | 收益复利 | 这正是 `PLAN D.1` 给的"按类型分容器"技巧（`PLAN:983`），S9 的 `Projectile` 直接复用 |
| 5 | 不必升级到 D-2 | 绕开 `PLAN:971` 的"万能 Entity 加字段"那条路 |

**坏处（真代价）**

| # | 代价 | 症状 |
| --- | --- | --- |
| 1 | **两套真相，靠 `EntityId` join** | "位置+速度"在 `World`，"状态+血量+计时器"在 `game`。**删一个敌人要两处一起删**，而两边的 `swap-erase` 是**两套独立顺序** → 只删一边 = 悬空 id（`Find` 静默返回 `nullptr`）或"实体还在但没人驱动"（原地当活靶子） |
| 2 | 一致性靠自觉，编译器不拦 | 忘了同步 `m_enemies` 没有编译错误、没有断言，只是行为诡异 |
| 3 | 每次驱动要 `Find(id)` | O(n) 线性扫，n ≤ 5 可忽略；但立了"以后每种实体都这样 join"的先例 |
| 4 | 渲染/调试归属变别扭 | `DrawEntities` 扫 `Entities()` 画符号，"这个 `E` 什么状态"却在另一处 → F3 面板必须扫 `m_enemies`，**两套遍历** |
| 5 | 与 `PLAN D.1` 字面不符 | 要登记偏差（第 12 节），且收口时要改写 D.1 那段文字 |

> **唯一会持续产生新 bug 的代价是 1/2**（其余都是"少个功能"或"观感差"）。
> 所以 10.1 专门排了一条用例钉它：`Game.RemovingEnemyLeavesNoDanglingId`。

### 分叉 ② 的好处 / 坏处

**好处**

| # | 好处 | 落到本仓库 |
| --- | --- | --- |
| 1 | 不引入任何"会改变位置"的新机制 | S7 已因"接线后既有用例的隐含前提被打破"**真红过三次**（最惨那次 4 条一起红）；S8 直接把这个风险源拿掉 |
| 2 | 现有 4 条 `Game` 用例期望值全部有效 | 玩家位置不受敌人影响 → 一条都不用改 |
| 3 | "不抖"这个验收项**不依赖推挤** | 抖动根因是**状态阈值抖动（flapping）**，治法是迟滞（`sight < lose`），不是分离解算 |
| 4 | 绕开 `E-9`（迭代器失效） | 两趟循环 + 重叠解算天然容易在遍历中改容器 |
| 5 | 兑现 PLAN 自己的话 | `PLAN:573` 坑表原文："先忽略实体间碰撞解算的停止条件" |

**坏处**

| # | 代价 | 后果 |
| --- | --- | --- |
| 1 | **敌人互相重叠、直接穿过彼此** | 5 个敌人会叠成一个 `E` → ⚠️ **10.2 第 6 条必须按"叠着一起追是预期行为"写**，否则验收会自己骗自己 |
| 2 | 玩家也能穿过敌人 | "敌人有体积"的直觉不成立；**S9 的子弹命中检测不能指望推挤**，要单独写距离判定 |
| 3 | C12 被**第二次**推迟 | S7 判掉 → S8 又判掉；风险从"有意推迟"滑向"真忘了" → 新触发条件必须写死并在 S10 收口时核对 |
| 4 | 诱惑风险 | 有人看到重叠就想顺手改 `World` → 破坏"零 engine 改动"这个最值钱的性质 |

### 分叉 ③ 的好处 / 坏处

**好处**

| # | 好处 | 说明 |
| --- | --- | --- |
| 1 | 状态表完整、无空洞 | `switch` 的**穷尽性**（`PLAN:568` 的 C++ 知识点）只有在五态下才是真的 |
| 2 | `Dead` 语义一次写清 | "`v=0`、`active=false`、不再决策"在 S8 就被用例钉住；S9 只需接一个 `hp -= damage` 就自然进入 |
| 3 | 机器 + 人眼都能取证 | 用例 `SelfDeadForcesDeadAndStops`（机器）+ F3 面板显示 `state=Dead`（人眼） |
| 4 | 不引入新范式 | 调试键 + 登记 `TECH_DEBT`，与 C14 的 F4 快照**同一个套路** |

**坏处**

| # | 代价 | 严重度 |
| --- | --- | --- |
| 1 | **`Dead` 在 S8 是"无生产入口"的代码** | ⚠️ **真代价**：只有调试键能到它，直接违反项目自己的"0 调用者不建"（B2 / C11 / C8 都按这条砍过东西）。属于**明知故犯**，理由是 PLAN 把五态写死在 S8 目标里、且用例能覆盖 |
| 2 | 动公共头 `KeyCode.h` + 一笔新债 | 加 `Key::K`（已核不碰红 `test_input_state.cpp`，见前置事实 11）；S9 后要记得删 |
| 3 | 攻击的后果"打不满" | HP 掉到 0 之后什么都不会发生，玩家能继续走 —— 演示时像"掉了血却没事"。闭环在 S10 |
| 4 | 僵尸 `EnemyData` | `Dead` 只 `active=false` 不删 → `m_enemies` 累积、F3 面板越来越长 |
| 5 | 调试键多一条绑定规则 | "必须 F3 面板打开时 `K` 才生效"（防误触），这条规则本身要记 |

---

## 进度板

| 步骤 | 内容 | 状态 |
| --- | --- | --- |
| 0 | 需求说明 + 3 个分叉定稿 + 本施工图 | ✅ **2026-10-09 完成**（分叉由本人拍板） |
| 1 | `ai/EnemyBrain.h`（纯函数 FSM）+ `tests/test_enemy.cpp`（18 用例）+ `tests/CMakeLists.txt` | ✅ **本人回报全绿（2026-10-09）：`141 passed, 0 failed`**（123 + 18）· 实现中追加了 2 条落地偏差 |
| 2 | `game/Enemy.h` + `Game` 铺 5 个**巡逻**敌人（还不会追人）+ `KeyCode.h` 加 `Key::K` | ✅ **本人回报符合预期（2026-10-09）**（全集仍 141）· 追加了 1 条落地偏差（偏差 3） |
| 3 | 接上 `Chase` / `Attack`（写敌人速度 + 扣玩家 HP）—— **唯一改变"敌人会追你"的一步** | ✅ **本人回报符合预期（2026-10-09）**（全集仍 141）· 追加了 1 条落地偏差（偏差 4：HUD 加 `hp` 顺带简写） |
| 4 | `F3` 临时 AI 面板（`state / target / distance`）+ 调试键 `K` 杀最近敌人 | ✅ **本人回报符合预期（2026-10-09）**（`F3` 实测冲突 → 已改 **`F7`**，并补了 `VK_F7` 映射）· 追加了 2 条（偏差 5 / 6） |
| 5 | `test_game.cpp` 加 4 条集成用例 | ✅ **本人回报"用例全绿"（2026-10-09）** → **145 passed**· **替换了 2 条**施工图里定的用例（偏差 7 / 8）+ 改名 1 条（偏差 9） |
| 6 | 收口：同步第 11 节的文档 + `TECH_DEBT`（含 4 笔欠账） | ✅ **2026-10-09 完成**（本人看过无异议）：`PLAN` 的 S8 节 / A.2 / D.1 / G / H.2 + `TECH_DEBT` 的 C12 改写 / C3 触发条件 / 新增 C15·C16/C17 / S8 进度段 + `AGENTS.md` §2 加一行 |
| 7 | 验收：正向（10.2）+ 反向验证 6 条（10.3）+ 人眼 8 条 | ✅ **2026-10-10 全部完成**：反向验证 R1~R6 跑完（§10.3.1）→ **R6 全绿 = 缺口** → 逼出 **+1 条用例**（偏差 10）+ R6′ 复验通过；收尾照抄 **`146 passed, 0 failed` / `ctest` 1/1**（§10.4）；**人眼 8 条 2026-10-10 11:00 补验，本人回报全部通过**（§10.2.1） |

### 落地偏差（实现时发现的问题，逐步追加）

| # | 施工图写的 | 实际落地 | 为什么 |
| --- | --- | --- | --- |
| 1 | §7.4："Patrol 用**乒乓**推进 `waypointIdx`"，并给了一条"终点回折"的等价规则；用例名 `PatrolPingPongsAtPathEnds` | **改成环形前进**（`idx = (idx + 1) % N`）；用例**改名** `PatrolWrapsAroundAtPathEnds`，并断言"走到最后一个点后环绕回第 0 个、速度反向" | **`Decide` 是无状态的，光靠 `waypointIdx` 数学上推不出方向**：在 3 个点的路径上位于 `idx = 1` 时，"上次是往 +1 还是 −1"无法从索引本身得到。§7.4 已经预判了这个分叉并给了首选做法（"接受环形前进 + 登记偏差"）。<br>**"往返"改由调用方实现**：把 `Game` 的巡逻表本身写成回文（如 `A,B,C,B`），环形前进在这张表上**等价于**往返。<br>⚠️ 代价说清：**"往返"这个语义从 `ai/` 层移到了 `game/` 层**（表是调用方的），`EnemyBrain.h` 只保证"环形" |
| 2 | §7.1 状态转移表第 2 行：`*`（非 `Dead`）且 `!targetAlive \|\| dist > loseRange` → `Patrol` | **条件收窄成"只对交战态（`Chase`/`Attack`）生效"**：`if (engaged && (...))` | **照字面写会与第 7 行自相矛盾**：`*` 包含 `Idle`，而"目标很远"正是开局的常态 → `Idle` 会在**第一帧**就跳成 `Patrol`，`Idle` 形同虚设，用例 `IdleBecomesPatrolAfterIdleSeconds` 永远红。<br>收敛成"**脱战 = 离开交战态**"之后语义也更准（脱战的前提是先交战）。<br>⚠️ 这条属于**施工图自己的内部矛盾**，不是实现时的取舍 —— 记在这里以免下次照抄那张表 |
| 3 | §6.3 的 `EnemyData` 字段表 = `id / state / waypointIdx / attackTimer / idleTimer / hp`；§6.4 的巡逻表写成一个一维 `kPatrolPath[]` | **`EnemyData` 多一个 `patrolPath`（`uint8_t`）**；巡逻表改成 **`kPatrolPaths[5][2]`**（二维：一行 = 一个敌人的往返段），敌人用 `std::span{kPatrolPaths[patrolPath], 2}` 把自己的那段交给 `Decide` | **§4 说"全敌共用一张表、逐敌只存 `waypointIdx`"—— 但那样 5 个敌人会走**同一条**线、彼此完全重叠。**<br>"共用一张表"和"每个敌人有自己的巡逻段"要靠**两样东西**才能同时成立：表是共用的（编译期常量、零分配），而"我负责哪一段"必须逐敌记一笔。<br>所以拆成两个字段：`patrolPath` = 哪一段（新加的 1 字节），`waypointIdx` = 段**内**走到第几个点（`Decide` 推进的那个）。<br>⚠️ 这一步顺带把"往返"落到了**表**上：每行只有 2 个点 → `(idx+1)%2` 就是往返（PLAN:567 的语义因此保住了，见偏差 1） |
| 4 | §6.3 没提 HUD；§10.2 第 3 条要求"贴脸攻击 → **HP 下降（面板可见）**" | HUD 那一行**加 `hp %2d`，同时简写成 78 列**：`sum dt %6.2fs` → `sumdt %6.2f`、`WASD  Q` → `WASD Q`；并加公开只读 `Game::PlayerHp()` | **加之前那一行已经是 73 列**（S6 决策 5 曾为它砍掉 `cam`），而 `hp` 至少要 6~8 列 → 不改就**溢出 80 列**，HUD 会折行盖住地图。<br>这正是坑 8 预判的"要么砍栏目、要么简写"—— 这里两个都用上了（只砍纯冗余，不砍信息）：去掉 `sum dt` 里的一个空格和单位 `s`（`m_elapsed` 的语义没变）、合并一个双空格。<br>⚠️ 连带影响：`S6_CHECKLIST` / `S7_CHECKLIST` 里凡引用 HUD 文字的地方（`sum dt` / `WASD  Q`）**过时了**，收口时按本条为准。<br>⚠️ `PlayerHp()` 是公开接口，但它**不是"为测试而加"**：`DrawHud` 真的会显示它（一出生就有两个调用者） |
| 5 | §6.3 / `DrawAiPanel` 注释：面板画在 **HUD 行的下一行**（= 屏幕第 2 行起） | **改成底部对齐**：面板最后一行贴住画布最后一行（80×25 时是第 19~24 行） | **画在 HUD 下面会把敌人全盖住**：敌人待在 `y = 2..6`，而玩家靠近地图**上边缘**时相机把原点夹到 0 → 敌人落在**屏幕第 3~7 行**，正是面板要占的地方 → "按 `K` 看它消失"这个验收**根本做不了**。<br>底部对齐两头都不挡：玩家在画面里的行号永远 **<= 13**（相机居中/夹取的结果），敌人又在第 3~7 行 —— 底部那 6 行是空的。<br>⚠️ 这条是**写代码时看出来的**，纸上想不清楚：施工图只说了"别复用 HUD 那一行"，没算过敌人画在哪一行 |
| 6 | §6.7：面板键 = `F3`（`PLAN:572` 也是这么写的） | ✅ **实测确认 F3 用不了 → 改成 `F7`**（本人 2026-10-09 拍板："不行 f3本机冲突，换成f7试下"）。配套：`KeyCode.h` 补一条 `VK_F7 = 0x76` 映射（`F1..F4` 是连续的 0x70..0x73，**F7 不连续**，必须单独加）；`test_input_state.cpp` 加一句断言；面板键仍收在**一个常量 `kAiPanelKey`** 里 | ⚠️ **`F3` 在本机被别的软件占用**，按下去程序**收不到**。这不是新问题：`kDumpKey` 的注释记着 2026-10-08 一模一样的经历（"F1 撞别的软件 → 换 F3 → **F3 也不行** → 落到 F4"）—— **两次栽在同一个键上**。<br>处置：按本人指定的 `F7` 落地；把"换键成本"压到**只改一行常量**（其余代码不认具体键）。再撞的下一个候选：`Tab`（本项目没人用，已映射）。<br>⚠️ 面板键**不进 HUD 提示行**（那一行已 78/80 列）→ 只在**面板表头**里显示；表头与常量是同一个字母的两个落点（同 `kDumpKey` 的纪律） |
| 7 | §10.1 用例 20：`EnemyStopsAtWall`（敌人撞墙停 → 证明它吃 S7 的碰撞） | **删掉**，不留替代 | S8 的敌人**从来不会走到墙边**：巡逻段在 `x=44..72, y=2/4/6` 的空地上，追击路径也在这片区域 —— 想造一个"墙在中间"的可复现场景，就得把玩家先走到地标楼（`x=30..36, y=5..8`）后面，而那依赖**一堆私有的地图/出生点细节**，写出来是一条脆弱的用例。<br>而"敌人吃碰撞"这件事本身是**结构保证**的：全项目只有一条积分路径（`World::Update` → `MoveAndCollide`，前置事实 1），敌人和玩家走的是同一行代码 —— `Game.PlayerStopsAtTopWall` + `Collision` suite 17 条已经在守它。<br>→ 留给人眼验收第 5 条（"隔墙追你 → 它卡在墙后蹭"）去看。**这不是"测不了就算了"，是"另有一条用例已经覆盖了同一个事实"** |
| 8 | §10.1 用例 22：`RemovingEnemyLeavesNoDanglingId`（删敌人后不留悬空 id） | **换成 `PlayerHpStopsAtZero`**（HP 掉到 0 后停住、永不变负） | ⚠️ **S8 根本没有"删除敌人"这条路径**：`Dead` 只把 `Entity::active` 置 false（§6.6），容器一个元素都不动，没有任何代码调 `MarkForRemoval`。于是"删一个 → 两边同步 → 没有悬空 id"这条在 S8 **永远绿、什么也没测** —— 一条空用例比没有用例更坏（它会让人以为这里有守卫）。<br>换成一条**真的会红**的：`PlayerHpStopsAtZero` 钉 `DriveEnemyAi` 里那句 `std::max(0, ...)` 与"HP 归 0 → 敌人脱战"的连锁。<br>⚠️ **`RemovingEnemyLeavesNoDanglingId` 挪到 S9**：那时才有真删（`MarkForRemoval` + `std::erase_if`），分叉 ① 的"两套真相"代价才第一次变成**可发生**的事故 |
| 9 | §10.1 用例 19 的名字 `EnemyChasesPlayer` | 改名为 **`EnemyClosesInWhenPlayerIsNear`**，判据从"敌人朝玩家移动"收紧成"**'@' 与最近 'E' 的格距在 0.4 秒内变小**" | 名字与判据都换了，因为要绕开两个陷阱：<br>① **不假设敌人在哪** —— `kPatrolPaths` 是私有的，用例只能"向右走，够近了就停"（`WalkPlayerNextToEnemy`），不能写死"走到第 50 格"；<br>② **不依赖相机** —— 比的是 `'@'` 与 `'E'` 的**屏幕格差**（两者共用同一个整数原点，所以格差 == 世界格差），而不是"敌人画在第几列"（相机一滚就失真，S6 落地偏差 3 的教训）。<br>顺带加了一条施工图里**没有**的 `EnemiesPatrolUntilPlayerIsNear`：它同时钉"巡逻真的在动"（防止用"敌人根本不更新"来假绿）和"远距离不被拉进战斗" |
| 10 | §10.1 用例清单（22 条）+ §10.3 R6（"删敌人时只 `MarkForRemoval`、不同步 `m_enemies`"） | **+1 条用例**：`Game.EnemyDiesAndDisappears`（全集 **145 → 146**）；**R6 的破坏方式也改了**（改成注掉 `entity->active = false;`，因为 S8 没有"真删"这条路径，见偏差 8） | ⚠️ **反向验证 R6 的直接产物**：注掉 `active = false` 之后 **`145 passed, 0 failed` 全绿**，可 exe 里"按 `F7` → `K` 杀敌 → 面板显示 `Dead`，那个 `E` **还留在屏幕上**"（本人 2026-10-09 亲眼看到）—— 也就是"死亡三件套"的最后一件（**不再绘制**）**零覆盖**。<br>处置（本人选 A）：补一条用例，判据**必须落在"屏幕上的 `'E'` 少一个"**。<br>⚠️ 关键在于：**面板里出现 `Dead` 挡不住这个破坏** —— 那个状态来自另一套真相（`EnemyData.state`）。所以新用例写了**两条各守一件事**的断言，并在 R6′ 复验里拿到"**`:697` 红、`:701` 绿**"这个结果，正好证明两条都不能省。<br>⚠️ 它同时补上了施工图里写不出来的用例 22 的**一半**（死亡 → 停更 → 不绘制）；"不留悬空 id"仍留 S9 |

---

## 0. 前置事实（写方案前逐条核过的，不是推测）

| # | 事实 | 位置 | 对 S8 的意义 |
| --- | --- | --- | --- |
| 1 | 位移的**唯一**入口是 `MoveAndCollide(m_map, pos, v*dt)` | `engine/src/world/World.cpp:97` | 敌人**自动**吃 S7 的碰撞（撞墙停、沿墙滑），一行都不用写 |
| 2 | `World::Update(float dt)` 是 **`noexcept`** | `engine/include/mini/world/World.h:70` | AI **不进** `World` —— 它只是"在 `Update` 之前把速度写好"的 game 侧逻辑 |
| 3 | `Entity` 有 `static_assert(sizeof(Entity) == 24)` | `engine/include/mini/world/Entity.h:87` | 分叉 ① 选了"不加字段"，所以这条**不会响** —— 响了就说明有人违约了 |
| 4 | `Entity` 字段 = `id / transform / velocity / symbol / active` | `engine/include/mini/world/Entity.h:74-80` | 敌人**也是**一个普通 `Entity`（`symbol = 'E'`），只是它另外在 `m_enemies` 里挂了一份状态 |
| 5 | ⚠️ `DrawEntities` 里写着 `if (!entity.active) continue;` | `game/src/Game.cpp:255` | 所以 `Dead` 一旦设 `active = false`，敌人**从屏幕上消失**（这正是"被击杀"的观感，见 6.6） |
| 6 | 帧序 = 读输入 → 写玩家速度 → `World::Update` → 相机 `Follow` → `DumpSnapshot` → `FlushRemovals` | `game/src/Game.cpp:154-215` | AI 驱动要插在 **`World::Update` 之前**（新第 ⑤b 步）；顺序反了 = 敌人慢一帧，见坑 3 |
| 7 | 默认速度 `Game::kMoveSpeed = 20`（格/秒） | `game/include/game/Game.h:61` | 敌人 `moveSpeed` 必须 **< 20**，否则玩家跑不掉（6.4） |
| 8 | 出生点 `kSpawnX / kSpawnY = 2.5`（格中心） | `game/include/game/Game.h:62-63` | 敌人**不许**在出生点附近生成，否则开局就被追（可玩性 + 用例稳定性，坑 6） |
| 9 | `Key::F3` / `Key::F4` 已有 VK 映射（`0x72` / `0x73`） | `engine/include/mini/platform/KeyCode.h:84-85` | `F4` 已被 C14 的调试快照占了 → S8 的 AI 面板用 **`F3`** |
| 10 | `Key::K` **未映射**（`0x4B` 不在表里） | `engine/include/mini/platform/KeyCode.h:55-89` | S8 要**新增** `Key::K` 当调试键（6.7） |
| 11 | 测试里"**有意不映射**的键"用的是 `VK_B`（`0x42`），断言在 `:178` | `tests/test_input_state.cpp:42, 178` | ✅ **新增 `Key::K` 不碰红这条**（这是加映射前必须先核的事） |
| 12 | `Vec2` 有 `Length()` / `Normalized()`（`Normalized` 对零长度返回零向量）；**没有** `operator==` | `engine/include/mini/math/Vec2.h:86-87, 15` | 零距离的 NaN 已由 `Normalized` 挡掉一半，但**速度合成前**仍要显式判零（坑 1） |
| 13 | `Color::BrightRed` 存在 | `engine/include/mini/render/Color.h:34` | 可选：给 `E` 换色（6.3） |
| 14 | 聚合体入参的先例：`CameraLimits` | `engine/include/mini/render/Camera.h` | `BrainInput` / `EnemyTuning` 照同一形态写（不带构造函数） |
| 15 | HUD 那一行约 **73 列**（80 列终端） | `game/src/Game.cpp:294-302` | 再加"HP + F3 提示"会挤爆 → 要么砍栏目、要么简写（坑 8） |
| 16 | demo 地图 = 外沿一圈墙 + **柱阵**（`x=10,20,…` 且 `y=10,20,…`）+ 地标楼（`x=30..36, y=5..8`） | `game/src/Game.cpp:107-133` | 敌人出生点与巡逻路点要落在这套几何的空格上（坑 6） |
| 17 | 现有 `Game` suite **4 条**用例的期望值只依赖"玩家自己的速度 + 地图几何" | `tests/test_game.cpp` | 分叉 ② 不做推挤 → 这 4 条**不受影响**（核过：`FindChar` 只找 `'@'`，`CountChar(fb,'@')` 也只数 `@`） |
| 18 | `PLAN` 的 S8 目标 = 五态 + `E` 沿直线追 + 距离 < 1.5 攻击扣血；坑表写着"不做寻路" | `PLAN:562-574` | 本阶段的**唯一需求来源**；偏差见第 12 节 |
| 19 | C12（实体推挤）触发条件原文 = "S8 出现第二个会动的实体" | `TECH_DEBT:59` | 本阶段**改写**它的触发条件（分叉 ②，第 12 节） |
| 20 | C14 = F4 调试快照写在 `game/` 里 | `TECH_DEBT:61` | S8 的 `F3` 面板 + `K` 调试键是**同一家族的临时出路**，登记格式照抄 C14 |

---

## 1. 目标与**明确不做**

**目标**（`PLAN:562-574`）：世界里出现会追人的 `E` —— 五态 FSM（`Idle → Patrol → Chase → Attack → Dead`）、
**距离阈值切换**、**直线追**、贴脸后按冷却扣玩家 HP；`F3` 能实时看到 `state / target / distance`。

本阶段的学习点是"**有限状态机 + 感知（视距/距离）+ 更新顺序**"，不是"做 AI" —— 不做寻路、不做行为树。

### 明确不做（防止"顺手多做一点"）

| 不做 | 留给谁 / 触发条件 |
| --- | --- |
| **寻路 / A\***（`PLAN:573`） | **S14** 的扩展池（`PLAN:1188` 那行，触发条件原文是"S8 之后"）。Chase 直线走 → 撞墙会卡住，**这是有意保留的动机**，要亲眼看到 |
| **视线遮挡（LoS 射线）** | 没有需求。感知**只有欧氏距离** → 隔墙也算"看见"，你会看到敌人贴着墙蹭 |
| **实体间推挤 / 分离**（C12） | **S10**（刷怪后敌人变多、重叠到影响可玩性时）。⚠️ 触发条件已被本阶段**改写**（第 12 节偏差 3） |
| **行为树** | `PLAN:574` 的设计结论："状态机够了" |
| **弹道 / 射击 / 命中判定** | **S9**。S8 的攻击 = 距离内按冷却**直接结算伤害** |
| **刷怪波次 / 难度递增** | **S10**。S8 的敌人**开局一次性放好**，不刷新、不补位 |
| **玩家死亡 / 重开 / `GameState`** | **S10**。S8 只"扣血 + 显示"，HP 归 0 **不发生任何事** |
| **敌人血条 / HUD 血量面板** | **S10**。S8 的 HP 只出现在 F3 面板与 HUD 的一个数字上 |
| **通用 `ai/StateMachine.h` 模板** | 5 个状态用 `switch` 更直白；建模板是"提前造抽象"（`PLAN:574` 同一结论）。偏差 2 |
| **`world/Health.h`** | `PLAN:163` 的目标树里有它，但 S8 **0 调用者**（玩家 HP 直接放 `Game`）→ 与 C8（`Vec3`/`Rect` 只有契约）同族处理。偏差 5 |
| **改 `entity.active` 的现有语义** | "停更 ≠ 删除"是 S5 定死的；`Dead` 复用 `active = false`，不新增字段（分叉 ① 的直接后果） |
| **改 `World::Update` / `Collision.h` / `TileMap` / `Camera` / `Entity.h`** | 本阶段的**红线**：`engine/` 零改动。要动就是方案错了，回来看第 12 节 |

---

## 2. 交付物清单

| # | 文件 | 动作 | 说明 |
| --- | --- | --- | --- |
| 1 | `engine/include/mini/ai/EnemyBrain.h` | **新增**（header-only） | `EnemyState` 枚举 + `EnemyTuning` + `Decide()` 纯函数（第 6 节） |
| 2 | `game/include/game/Enemy.h` | **新增** | `EnemyData` 结构体（字段列表，6.3）；落在 `PLAN:179` 已有的位置 |
| 3 | `game/include/game/Game.h` | 改 | 敌人集合 + 玩家 HP + 3 个私有方法声明 + 1 个公有只读访问器（6.3） |
| 4 | `game/src/Game.cpp` | 改 | `SpawnEnemies` / `DriveEnemyAi` / `DrawAiPanel` + 帧序插一步 + 绘制换色 |
| 5 | `engine/include/mini/platform/KeyCode.h` | 改 2 行 | 枚举加 `Key::K`（`F4` 之后、`Count` 之前）+ VK 映射 `0x4B`（6.7） |
| 6 | `tests/test_enemy.cpp` | **新增** | suite `Enemy`，**18** 个用例（10.1） |
| 7 | `tests/test_game.cpp` | 改 | 加 **4** 条集成用例（10.1） |
| 8 | `tests/test_input_state.cpp` | 改 1 行 | 在既有的 `KeyCode.KeyFromVirtualKeyMapsUsedKeys` 里加一句 `Key::K` 断言（**不新增用例**） |
| 9 | `tests/CMakeLists.txt` | 改 2 行 | 源文件列表 + 头部注释各加一行（S0 起禁用 `file(GLOB)`） |
| — | `engine/src/`、`engine/include/mini/world/`、`engine/include/mini/render/`、`engine/include/mini/core/`、`main.cpp`、`game/CMakeLists.txt`、顶层 `CMakeLists.txt` | **不动** | ⚠️ **`engine/` 零改动**是这个阶段值得记住的性质 |

> ⚠️ `game/CMakeLists.txt` **不用改**：`Enemy.h` 走已有的 `include/game` PUBLIC 目录，不是新源文件。

---

## 3. 依赖规则（本阶段**新增 1 条边**）

```text
game/（Game.cpp / Enemy.h）
   ├─► engine/ai/EnemyBrain.h ──► math/Vec2.h        ← 新增：第一条指向 ai/ 的边
   ├─► engine/world/*（已有：World.h / Entity.h / TileMap.h）
   └─► engine/render/*（已有：Camera.h / FrameBuffer.h / Color.h）

⛔ 禁止出现的边（本阶段要自检）：
   ai/EnemyBrain.h → world/      （它不认识 Entity / World / TileMap）
   ai/EnemyBrain.h → render/     （不认识 FrameBuffer / Camera / Color）
   ai/EnemyBrain.h → platform/   （不知道按键是什么）
   ai/EnemyBrain.h → game/        （PLAN A.3 单向）
   world/ → ai/                   （⚠️ 分叉 ① 的直接后果：world 层不知道 AI 的存在）
   game/ → world/Collision.h      （唯一的碰撞调用点是 World::Update，S7 已立）
```

`ai/` **只依赖 `math` + 标准库（`<cstdint>` / `<span>`）** —— 与 `world/Collision.h` 同一条纪律：
"能离线单测的部分全部拆出去"。因为不依赖 `world`，`Decide` 可以用纯 `Vec2` 造用例，**不需要地图、不需要 `Entity`**。

---

## 4. 数据结构与所有权

| 项 | 决定 | 理由 |
| --- | --- | --- |
| **敌人状态本体** | `game` 侧 `std::vector<EnemyData> m_enemies`（分叉 ①） | `Entity` 零改动；`EnemyData` 里的 `EntityId` 是**稳定句柄**（`swap-erase` 后仍有效，见 `Entity.h:30-35`） |
| **敌人的"位置+速度"** | 仍归 `World` 里的 `Entity` | 全项目只有一条积分路径（前置事实 1），敌人自动吃碰撞 |
| **玩家 HP** | `Game::m_playerHp`（`int`） | 玩家不是 `EnemyData`；S10 的 HUD 也在 game 侧 |
| **敌人 HP** | `EnemyData::hp`（`int`） | S9 的子弹要扣它，届时无需改名 |
| **路点表** | `Game` 侧一张 `static constexpr` 环，**全敌共用**；逐敌只存 `waypointIdx` | 避免每个敌人存一份 `vector`（会引入分配与生命周期）；表是编译期常量 |
| **巡逻索引** | `uint8_t`，**乒乓**推进（`0→1→…→N-1→N-2→…→0`） | `PLAN:567` 写的是"预置路径点**往返**" |
| **定时器** | `attackTimer` / `idleTimer` 都是 `float`（秒），**由调用方存取** | `Decide` 必须纯函数（无静态状态）→ 定时器不能藏在里面；下限夹 0 |
| **`Decide` 的入参** | 聚合体 `BrainInput`（不带构造函数） | 先例：`render/Camera.h` 的 `CameraLimits`（前置事实 14） |
| **AI 的位置口径** | **本帧积分前**的双方位置（= 上一帧末位置） | 帧序不变、确定性强、离线可复现；代价：追的是 1/60 秒前的你（≈0.33 格，肉眼不可见） |
| **两处删除的顺序** | `MarkForRemoval(id)` → 从 `m_enemies` 移除 → `World::FlushRemovals()` | 两套 `swap-erase` **互不知道对方的顺序** → 谁都不许缓存下标（坑 5） |
| **`Dead` 的存储** | 设 `active = false`，**`EnemyData` 不删** | 这样 F3 面板仍能显示 `state=Dead`（取证）；真删留 S9。代价 = "僵尸 `EnemyData`" |
| **内存** | 敌人数量固定（5），`m_enemies` 可 `reserve`；AI 每帧零分配 | `Decide` 是 `noexcept` 且不分配（与 `Collision` 同纪律） |

---

## 5. 逐文件实施步骤（每步都立刻跑一次）

| 步 | 动作 | 跑完应该看到 |
| --- | --- | --- |
| 1 | 新建 `ai/EnemyBrain.h`（第 6 节）→ 新建 `tests/test_enemy.cpp`（10.1）→ `tests/CMakeLists.txt` 加 2 行 | `mini_tests.exe Enemy` → **18 passed**；全集 → **141 passed**；⚠️ `MiniEngine.exe` 现象与 S7 **完全一致**（AI 还没接上） |
| 2 | 新建 `game/Enemy.h` → 改 `Game.h/.cpp`（`SpawnEnemies`，**只巡逻不追人**）→ `KeyCode.h` 加 `Key::K` → `test_input_state.cpp` 加 1 行 | 全集 → **141 passed**（无新用例）；`MiniEngine.exe`：能看到 5 个 `E` 沿路点**平稳往返**，走近**不理你** |
| 3 | 接上 `Chase` / `Attack`（`DriveEnemyAi` 写速度 + 扣 HP）—— **唯一改变"敌人会追你"的一步** | 全集仍 141；`MiniEngine.exe`：走近会被追、贴脸掉血、跑远回巡逻；⚠️ **单独复跑 `mini_tests.exe Game`**（前置事实 17 那 4 条） |
| 4 | 加 `DrawAiPanel`（`F3` 开关）+ 调试键 `K` 杀最近敌人 | 全集仍 141；`MiniEngine.exe`：`F3` 出面板、面板开时 `K` 能让某个敌人消失且面板显示 `state=Dead` |
| 5 | `test_game.cpp` 加 4 条集成用例 | 全集 → **145 passed** |
| 6 | 同步第 11 节列的文档 + `TECH_DEBT`（含 4 笔欠账） | `git status` 干净 |
| 7 | 正向验收（10.2）+ 反向验证 6 条（10.3） | 见 10.3.1 / 10.4；**R6 逼出一条新用例** → 全集 **146** |

> 计数口径：**123**（S7 收口）→ **141**（步骤 1：+18 `Enemy`）→ **145**（步骤 5：+4 `Game`）
> → **146**（步骤 7：反向验证 R6 发现"`Dead` 不绘制"零覆盖 → **+1** `Game.EnemyDiesAndDisappears`）。

---

## 6. 接口设计（施工图）

### 6.1 `ai/EnemyBrain.h` 的骨架（**只有声明，实现由本人敲**）

```cpp
namespace mini::ai {

enum class EnemyState : std::uint8_t { Idle, Patrol, Chase, Attack, Dead };

struct EnemyTuning { /* 字段 + 默认值：见 6.4 的常量表 */ };

struct BrainInput {  /* 字段列表：见 6.5 */ };
struct BrainOutput { /* 字段列表：见 6.5 */ };

[[nodiscard]] BrainOutput Decide(const BrainInput& in, const EnemyTuning& t) noexcept;

inline constexpr EnemyTuning kDefaultTuning{ /* 6.4 的默认值 */ };

} // namespace mini::ai
```

**契约（写进文件头注释）**：

- **纯函数**：同输入同输出、不抛、不分配、无静态状态 —— 与 `world/Collision.h` 同一纪律。
- **不认识**：`Entity` / `World` / `TileMap` / 输入 / 屏幕 / 颜色。
- **不做**：不动位置（它只**输出速度**）、不做碰撞（位移仍归 `World::Update`）。
- `Decide` 只负责"**这一帧该是什么态 + 该往哪走 + 打不打**"；把结果写回世界是调用方的事。

**为什么 `Decide` 输出速度而不是"输出位移动量"**：速度是"格/秒"，与 `Entity::velocity` 同一单位，
写回去是**直接赋值**；若输出位移，调用方就得先除以 `dt` 再乘回 `dt` —— 引入两次浮点误差，且
"没有 `dt` 时怎么办"会变成一个新边界（坑 1 同族）。

### 6.2 为什么"定时器放在入参/出参里"

`Decide` 无状态 → 冷却必须由调用方存（`EnemyData::attackTimer`）。
这带来一个**必须写死的口径**：`attackTimer` 的语义是"距下次可攻击还剩多少秒"，
**`<= 0` 表示可以打**；`Decide` 每帧先 `-= dt`、再夹到下限 `0`，打出伤害时重置为 `attackCooldown`。

### 6.3 `game/` 侧（声明）

```cpp
// game/include/game/Enemy.h —— 敌人**特有**的数据（分叉 ①：不进 Entity）
struct EnemyData {
    mini::world::EntityId id;         // 指向 World 里的那个 Entity（稳定句柄）
    mini::ai::EnemyState  state;
    std::uint8_t          waypointIdx;
    float                 attackTimer;
    float                 idleTimer;
    int                   hp;
};
```

```cpp
class Game final : public mini::core::IGame {
public:
    [[nodiscard]] int PlayerHp() const noexcept;   // 新增公有只读访问器（HUD 与用例都真的会用它）
private:
    void SpawnEnemies();                              // OnStart 里调
    void DriveEnemyAi(const mini::core::FrameContext&); // OnUpdate 里调，**在 World::Update 之前**
    void DrawAiPanel(mini::render::FrameBuffer&) const; // F3 面板（const：只读）
    void KillNearestEnemy();                            // 调试键 K（临时，S9 后删）

    std::vector<game::EnemyData> m_enemies;
    int  m_playerHp   = kPlayerMaxHp;
    bool m_showAiPanel = false;
    static constexpr int kPlayerMaxHp =  /* 见 6.4 */;
    static constexpr mini::math::Vec2 kPatrolPath[] = { /* 见 6.4 */ };
};
```

> ⚠️ `PlayerHp()` **不是**"为测试而加"：HUD 也要显示它（S10 会扩展成正式面板）。
> 这条经得起"0 调用者不建"的纪律检查 —— 它一出生就有两个调用者。

**帧序（改动后的完整顺序，写进 `Game.cpp` 头注释）**

```text
① 读输入 → ② 归一化 → ③ 写玩家速度 → ④ Q 退出 → ⑤ Σdt
⑤b **驱动 AI**（对每个 EnemyData：Find → Decide → 写 Entity::velocity + 记伤害）
⑥ World::Update(dt)        ← 玩家和敌人**在同一个循环里**被积分、被碰撞
⑦ 相机 Follow
⑧ F3 面板开关 / 调试键 K / F4 快照
⑨ FlushRemovals
```

⚠️ **⑤b 必须在 ⑥ 之前**（与"写速度必须在积分前"同一条理由，前置事实 6）。
⑤b 读到的双方位置是**上一帧末**的位置（4 节的"位置口径"）。

### 6.4 参数默认值表（`kDefaultTuning` / `Game` 常量）

| 常量 | 默认 | 放在哪 | 理由 |
| --- | --- | --- | --- |
| `sightRange` | `10.0f` | `EnemyTuning` | 约半个视口宽，玩家有反应时间 |
| `loseRange` | `14.0f` | `EnemyTuning` | **必须 > `sightRange`** —— 迟滞区间 10~14 是"不抖"的全部机制 |
| `attackEnter` | `1.5f` | `EnemyTuning` | `PLAN:571` 写死"距离 < 1.5 开始攻击" |
| `attackExit` | `2.0f` | `EnemyTuning` | **必须 > `attackEnter`** —— 防 Chase↔Attack 抖 |
| `moveSpeed` | `6.0f` | `EnemyTuning` | **必须 < `Game::kMoveSpeed`(20)** → 玩家一定跑得掉 |
| `attackCooldown` | `0.8f` | `EnemyTuning` | 肉眼能数清每一击 |
| `attackDamage` | `1` | `EnemyTuning` | — |
| `idleSeconds` | `0.5f` | `EnemyTuning` | `Idle` 只为了让五态都"活着" |
| `arriveRadius` | `0.15f` | `EnemyTuning` | 巡逻到点判据的一部分（7 节第 5 步） |
| `kPlayerMaxHp` | `10` | `Game` | 5 个敌人同时贴脸时约 6 秒掉光，人眼验收够用 |
| `kPatrolPath[]` | 见 6.8 | `Game` | **编译期常量**，全敌共用 |

> ⚠️ `EnemyTuning` 是**公开**常量（`constexpr`），单测要用它算期望值 —— 不许把数字写死在用例里
> （先例：`Game::kMoveSpeed` 的注释"用例里不许写死格数"）。

### 6.5 `BrainInput` / `BrainOutput` 的字段列表

```text
BrainInput  { self; target; waypoints(span); waypointIdx;
              state; attackTimer; idleTimer; selfAlive; targetAlive; dt }

BrainOutput { state; velocity; waypointIdx; attackTimer; idleTimer; damage }
```

| 字段 | 方向 | 说明 |
| --- | --- | --- |
| `self` / `target` | 入 | 双方**积分前**的格坐标（4 节的位置口径） |
| `waypoints` | 入 | `std::span<const Vec2>`；**允许为空**（此时 Patrol 原地不动，不许崩） |
| `waypointIdx` | 入/出 | 乒乓推进；越界输入要**先取模**再使用（坑 7） |
| `state` | 入/出 | 上一帧的态 → 本帧的态 |
| `attackTimer` / `idleTimer` | 入/出 | 见 6.2 |
| `selfAlive` / `targetAlive` | 入 | 两个 `bool`：前者为假 → 立刻 `Dead`；后者为假 → 脱离 Chase |
| `dt` | 入 | 就是 `ctx.deltaSeconds`（S3 已 clamp，**这里不再夹**） |
| `damage` | 出 | `> 0` = 本帧对 `target` 造成这么多 HP 伤害；`0` = 没打 |

### 6.6 `Dead` 的确切语义（分叉 ③）

| 环节 | 行为 |
| --- | --- |
| 进入 | `hp <= 0`（调用方把 `selfAlive = false` 传进来）→ `Decide` 第一优先级返回 `Dead` + `velocity = {0,0}` |
| 调用方 | 把 `entity.active = false` |
| 后果 1 | `World::Update` 跳过它（前置事实 2 的循环里有 `if (!entity.active) continue;`）→ **不动** |
| 后果 2 | ⚠️ `DrawEntities` 也跳过它（前置事实 5）→ **从屏幕上消失**（"被击杀"的观感） |
| 后果 3 | `EnemyData` **不删** → F3 面板仍显示 `state=Dead`（**这就是取证方式**） |
| 触发方式（S8） | **只有调试键 `K`**（F3 面板打开时生效）—— 真入口是 S9 的子弹 |

### 6.7 调试键 `Key::K`（**临时**，S9 后删）

| 项 | 值 |
| --- | --- |
| `KeyCode.h` | 枚举在 `F4` 之后、`Count` 之前加 `K`（**不插中间**，避免改变既有枚举值）；VK 映射加 `0x4B` |
| 已核 | `test_input_state.cpp:42,178` 的"有意不映射"用的是 `VK_B` → **不会碰红**（前置事实 11） |
| 行为 | **`F3` 面板打开时**按 `K` → 杀掉**离玩家最近**的那个敌人（`hp = 0`，走 6.6 的流程）。防误触 |
| 登记 | `TECH_DEBT` C 组，格式照 C14；触发条件 = **S9 落地后删掉**（子弹才是真入口） |

### 6.8 出生点与巡逻路径（**施工时人工核对**，S8 不做地图校验）

| 项 | 决定 |
| --- | --- |
| 出生点数量 | **5**（`PLAN:572` 的验收要"5 个敌人同时追"） |
| 落点要求 | 必须在**空格**上，且**离玩家出生点 `(2.5, 2.5)` 的距离 > `sightRange`**（否则开局就被追） |
| 路径要求 | 路点必须落在空格上、且两两之间**没有墙**（`S8` 不做寻路，路点之间是直线走） |
| 几何依据 | 柱阵只在 `x=10,20,…` 且 `y=10,20,…` 的交叉点；地标楼在 `x=30..36, y=5..8`（前置事实 16）。玩家出生行 `y=2` / `y=4` 上**没有墙**（除外沿） |
| 校验方式 | 施工时**逐点手算**，并在 10.2 第 1 条里人眼确认"没有敌人出生就在墙里/路点穿墙" |

---

## 7. 算法与伪代码

### 7.1 状态转移表（**优先级从上到下，先命中先返回**）

| 优先级 | 当前态 | 条件 | 次态 | 动作 |
| --- | --- | --- | --- | --- |
| 1 | `*` | `!selfAlive` | `Dead` | `velocity = {0,0}` |
| 2 | `*`（非 `Dead`） | `!targetAlive` **或** `dist > loseRange` | `Patrol` | 朝路点走 |
| 3 | `Idle` / `Patrol` | `targetAlive && dist <= sightRange` | `Chase` | 朝目标走 |
| 4 | `Chase` | `dist <= attackEnter` | `Attack` | `velocity = {0,0}` |
| 5 | `Attack` | `dist > attackExit` | `Chase` | 朝目标走 |
| 6 | `Attack` | `attackTimer <= 0` | `Attack` | `damage = attackDamage`；`attackTimer = attackCooldown` |
| 7 | `Idle` | `idleTimer <= 0` | `Patrol` | 朝路点走 |
| 8 | `Patrol` | 到达当前路点（7.3） | `Patrol` | 乒乓推进 `waypointIdx` |
| 9 | `Patrol` | 其他 | `Patrol` | `v = dir(waypoint - self) × moveSpeed` |
| 10 | `Chase` | 其他 | `Chase` | `v = dir(target - self) × moveSpeed` |

> ⚠️ **优先级 2 必须在 3 之前**：这正是迟滞的实现方式 —— 单次求值里"先判脱战、再判进战"，
> 而 `loseRange > sightRange` → 中间那一段（10~14 格）**保持原态**，所以不会抖（坑 2）。

### 7.2 每帧（`Decide` 内部，纯函数）

```text
输入：in（6.5）, t（6.4）

1. 计时器推进：attackTimer = max(0, attackTimer - dt)；idleTimer = max(0, idleTimer - dt)
2. dist = (target - self).Length()
3. 按 7.1 的表取分支（顺序即优先级），得到 nextState 与"朝向哪个点"（目标 / 路点 / 不动）
4. 若需要"朝某点 P 走"：
       d = P - self
       if d.LengthSq() == 0  → v = {0,0}          ← ⚠️ 必须先判零（坑 1）
       else                  → v = d.Normalized() × moveSpeed
   否则 → v = {0,0}
5. 返回 BrainOutput{ nextState, v, waypointIdx, attackTimer, idleTimer, damage }
```

### 7.3 巡逻的"到点"判据（**不能写成"落在半径内"**）

```text
到点（本帧会到达或越过）= dist(self, waypoint) <= moveSpeed × dt + arriveRadius
```

**为什么**：若写成"必须落进 `arriveRadius` 内"，一旦某帧位移大于半径（`dt` 偏大、或速度偏大），
敌人会**一步跨过路点** → 下一帧距离开始变大 → "到点"永远不满足 → 敌人在路点两侧来回抽（坑 4）。
用"本帧会到达或越过"就结构上不可能漏。

### 7.4 乒乓球索引推进（`N = waypoints.size()`）

```text
方向来自"上次是往 +1 还是 -1"——但 BrainInput 里**没有方向字段**（不新增状态）
→ 用一条等价规则：终点回折
   若 idx + 1 >= N  → 往回走（此时索引只能减）
   若 idx == 0 → 往前走
```

⚠️ **这条要在施工时把语义写死并配用例**（`PatrolPingPongsAtPathEnds`）。若实现时发现"单靠索引推不出方向"，
**首选**是接受"环形前进"（`idx = (idx + 1) % N`，环走而不是往返）并**登记为偏差**——`PLAN:567` 写的是"往返"，
但环形也满足"巡逻"这个可运行效果，且不需要新增字段。**改 PLAN 不许悄悄改**。

### 7.5 驱动循环（`Game::DriveEnemyAi`）

```text
for e in m_enemies:                        ← ⚠️ 遍历中禁止 Spawn（E-9）
    Entity* ent = m_world.Find(e.id)
    if (ent == nullptr) continue           ← 悬空 id 防御（分叉 ① 的代价 1）
    in = { ent->transform.pos, 玩家位置, kPatrolPath, e.waypointIdx,
           e.state, e.attackTimer, e.idleTimer, e.hp > 0, m_playerHp > 0, ctx.deltaSeconds }
    out = ai::Decide(in, ai::kDefaultTuning)
    ent->velocity.perSecond = out.velocity        ← 写回速度（位移仍由 World::Update 做）
    e.state = out.state; e.waypointIdx = ...; e.attackTimer = ...; e.idleTimer = ...
    if (out.damage > 0) m_playerHp = max(0, m_playerHp - out.damage)
    if (e.hp <= 0) ent->active = false            ← 6.6：Dead 不改容器
```

---

## 8. 接口决策表（2026-10-09 已拍板）

| # | 决策 | 取值 | 一句话依据 |
| --- | --- | --- | --- |
| 1 | 敌人数据归属 | `game` 侧 `vector<EnemyData>` | 分叉 ①；`engine/` 零改动 |
| 2 | 实体间推挤 | **不做** | 分叉 ②；"不抖"靠迟滞不靠分离 |
| 3 | `Dead` | 五态全做 + 调试键 | 分叉 ③ |
| 4 | AI 用哪一帧的位置 | **上一帧末**（本帧积分前） | 帧序不变、确定性强 |
| 5 | `Decide` 输出什么 | **速度（格/秒）** | 与 `Entity::velocity` 同单位，写回是直接赋值（6.1） |
| 6 | 定时器放哪 | 入参/出参（调用方存） | `Decide` 必须纯函数（6.2） |
| 7 | 攻击的"停" | `Attack` 态 `v = {0,0}` | 贴脸打，不追着撞 |
| 8 | 感知用什么 | **只有欧氏距离**，不查墙 | 视线遮挡没有需求（第 1 节） |
| 9 | 巡逻方向 | 乒乓（若实现受阻则环走 + 登记偏差） | 7.4 |
| 10 | `Dead` 后容器 | 只 `active = false`，**不删** | 6.6：要能在 F3 面板里看到 `Dead` |
| 11 | 面板键 | `F3`（`F4` 已被 C14 占用） | 前置事实 9 |
| 12 | 调试杀敌键 | `Key::K`（临时，S9 后删） | 6.7 |

### 升级路径（现在不做，触发条件写在这里）

| 想做的事 | 触发条件 |
| --- | --- |
| 加 A\* 寻路 | 有人真的被"卡墙"烦到（**S14 扩展池**）；S8 特意要你亲眼看到这个现象 |
| 实体间推挤（C12） | **S10** 刷怪后重叠到影响可玩性 |
| 真正的视线遮挡 | 需要"潜行/被发现"这类玩法时（本项目没有） |
| 把 `EnemyState` 搬进 `Entity` | 只有当"按类型分容器"的 join 样板真的压过收益时（`PLAN D.1` 的 D-2 信号） |
| 删掉 `Key::K` | **S9 落地后**（子弹是真入口） |

---

## 9. 坑表

| # | 坑 | 现象 | 处置 |
| --- | --- | --- | --- |
| 1 | **零距离归一化 → NaN** | `self == target` 时速度可能是 NaN → `pos` 变 NaN → **`ToIndex(NaN)` 是 UB**（`TileMap.h:69-71` 明写"去产生的地方查"） | 7.2 第 4 步先判 `LengthSq() == 0`；用例 `ZeroDistanceDoesNotProduceNaN` 钉死 |
| 2 | **状态抖动（flapping）** | 敌人卡在阈值上反复切态、原地打转 | `sight < lose` + `attackEnter < attackExit` 双迟滞（6.4）；用例 `HysteresisKeepsChaseUntilLoseRange` |
| 3 | **帧序写反** | 敌人慢一帧、或完全不动 | ⑤b 必须在 ⑥ 之前（6.3）；反向验证 R5 钉 |
| 4 | **跨过路点不换点** | 大 `dt` 一步越过路点 → "到点"永不满足 → 敌人在路点两侧来回抽 | 用 7.3 的"本帧会到达或越过"判据 |
| 5 | **两处删除不同步**（分叉 ① 的代价 1） | 悬空 id（`Find` 静默 `nullptr`）或"实体还在但没人驱动"（原地当活靶子） | 顺序写死（4 节）；用例 `RemovingEnemyLeavesNoDanglingId`；反向验证 R6 |
| 6 | **出生点在墙里 / 路点穿墙** | 出生即被夹住、或巡逻时贴墙抽 | 6.8 人工核对；S8 不做地图校验 |
| 7 | **`waypointIdx` 越界输入** | 调用方传了 `idx >= N`（或 `N == 0`）→ UB | `Decide` 开头**先取模 / 先判空**；用例 `EmptyWaypointsDoesNotCrash` |
| 8 | **HUD 80 列预算** | 现在约 73 列（前置事实 15），再加 HP + `F3` 提示会溢出 | 要么砍栏目、要么简写；`F3` 面板**不复用** HUD 那一行 |
| 9 | **`F3` 与 S11 的 Debug Overlay 抢键** | S11 要把 F3 变成正式 Overlay | 登记 `TECH_DEBT`（C14 家族）；S8 的 F3 是**临时最小面板** |
| 10 | **忘了敌人也吃碰撞** | 以为"敌人被墙夹住"是 bug | 前置事实 1：`World::Update` 对所有 `active` 实体一视同仁 —— 那是 S7 的正常行为 |
| 11 | **`Dead` 后敌人消失**（容易误判为 bug） | `active = false` → `DrawEntities` 跳过 → 屏幕上看不见了 | 前置事实 5；**这是有意的**（"被击杀"的观感）。取证靠 F3 面板的 `state=Dead` |
| 12 | **僵尸 `EnemyData` 无限堆积** | `Dead` 不删 → `m_enemies` 只增不减、F3 面板越来越长 | S8 只有 5 个敌人，可接受；**登记进 `TECH_DEBT`**，S9 接上伤害时一并处理真删 |
| 13 | **改公共头枚举插在中间** | 往 `Key` 中间插 `K` 会**改变后续枚举值**（`F1..F4` 等）→ `KeyIndex` 的索引关系跟着变 | 追加在 `F4` 之后、`Count` 之前（6.7） |
| 14 | **既有用例的隐含前提**（`AGENTS.md` 第 7 节第三条纪律） | 见下 | 动手前先扫一遍 |

### 坑 14 展开（本阶段接线前的必做动作）

已扫结果（**2026-10-09 核过**）：现有 `Game` suite **4 条**用例的期望值只依赖"玩家自己的速度 + 地图几何"
（前置事实 17）→ 分叉 ② 不做推挤 → **玩家位置不受影响 → 这 4 条都不该动**。

但**施工时仍要逐条问一句"它假设了什么"**：

| 用例 | 它假设了什么 | S8 是否打破 |
| --- | --- | --- |
| `Game.DiagonalMoveIsNotFaster` | 斜向 0.45s 的路径不碰柱子；屏幕位移 == 世界位移（相机退化） | ❌ 不打破（敌人不推玩家） |
| `Game.VelocityTakesEffectSameFrame` | 一帧内写速度 → 积分生效 | ❌ 不打破（帧序只是**多插一步**，没改次序） |
| `Game.PlayerIsDrawnAtItsCell` | 屏幕上只有 **1 个** `'@'` | ❌ 不打破（敌人是 `'E'`；`CountChar` 数的是 `'@'`） |
| `Game.PlayerStopsAtTopWall` | 顶墙在 `y = 0`，只走 1 格 | ❌ 不打破 |

⚠️ **仍然要单独复跑 `mini_tests.exe Game`**（步骤 3 的验收项）—— 理由是前两次同类教训：
S7 偏差 7 就是"只核了自己想到的那三条，漏了 `World` suite 里的 5 条"。**扫描结论要留痕**（就是上面这张表）。

另一条必须核的：**敌人出生点不能在 `(2.5, 2.5)` 附近**，否则开局就被追 → 上面几条用例的
前若干帧会多出"敌人移动"的行为（虽然不改玩家位置，但会让现象与预期不符，且可玩性差）。

---

## 10. 验收

### 10.1 用例清单（**22 个**：suite `Enemy` 18 + suite `Game` 4）

**suite `Enemy`**（新文件 `tests/test_enemy.cpp`；**不需要终端、不需要地图、不需要 `Entity`**）

| # | 用例 | 内容 / 期望 |
| --- | --- | --- |
| 1 | `DecideIsPureFunctionOfInputs` | 同一 `BrainInput` 跑两遍 → 输出逐字段相同（无隐藏状态；同 S6 `FollowIsPureFunctionOfInputs` 的套路） |
| 2 | `IdleBecomesPatrolAfterIdleSeconds` | `idleTimer` 耗尽前是 `Idle`、耗尽后是 `Patrol` |
| 3 | `PatrolMovesTowardWaypoint` | `Patrol` 且远处有路点 → 速度方向指向它，长度 == `moveSpeed`（`ApproxEquals`） |
| 4 | `PatrolAdvancesWaypointOnArrival` | `dist <= moveSpeed×dt + arriveRadius` → `waypointIdx` 推进 |
| 5 | `PatrolPingPongsAtPathEnds` | 走到两端后**折返**（若改环走，这条要改名并登记偏差 —— 见 7.4） |
| 6 | `EmptyWaypointsDoesNotCrash` | `waypoints` 为空 → 不崩、`Patrol` 速度为 `{0,0}` |
| 7 | `WaypointIndexOutOfRangeIsClamped` | 传入 `idx >= N` → 先取模，不越界读（坑 7） |
| 8 | `SightTriggersChase` | `dist <= sightRange` 且 `targetAlive` → `Idle`/`Patrol` 变 `Chase` |
| 9 | `ChaseMovesTowardTarget` | `Chase` → 速度指向 `target`，长度 == `moveSpeed`（R2 钉它） |
| 10 | **`HysteresisKeepsChaseUntilLoseRange`** | `sight < dist < lose` 且上一帧是 `Chase` → **仍是 `Chase`**（不抖）（R1 钉它） |
| 11 | `AttackEntersWithinAttackEnter` | `dist <= attackEnter` → `Chase` 变 `Attack`，速度为 `{0,0}` |
| 12 | `AttackFiresDamageWhenCooldownElapsed` | `Attack` + `attackTimer <= 0` → `damage == attackDamage` 且 `attackTimer == attackCooldown` |
| 13 | **`AttackRespectsCooldown`** | `Attack` + `attackTimer > 0` → `damage == 0`（R3 钉它） |
| 14 | `AttackTimerCountsDownAndClampsAtZero` | 每帧 `-= dt`，下限夹 `0`（不会变负） |
| 15 | `LeavingAttackRangeReturnsToChase` | `dist > attackExit` → `Attack` 变 `Chase` 且速度指向目标 |
| 16 | **`ZeroDistanceDoesNotProduceNaN`** | `self == target` → 速度为 `{0,0}`、**输出里没有任何 NaN**（R4 钉它；坑 1） |
| 17 | `SelfDeadForcesDeadAndStops` | `selfAlive == false` → `Dead` + 速度 `{0,0}`，且**任何**输入态下都成立 |
| 18 | `TargetDeadReleasesChase` | `targetAlive == false` → 从 `Chase`/`Attack` 回到 `Patrol` |

**suite `Game`**（加在 `tests/test_game.cpp`；沿用既有的 `FakeInput` + 扫 `FrameBuffer` 套路）

| # | 用例 | 内容 / 期望 |
| --- | --- | --- |
| 19 | `EnemyChasesPlayer` | 玩家不动，敌人从 `sightRange` 内出发 → 若敌人朝向玩家所在方向，则 **"敌人与玩家的格距"随时间单调减小**（R5 钉它）。⚠️ 断言用"距离减小"而不是"敌人画在哪一列"：相机跟随会让屏幕位移失真（`test_game.cpp` 头部那段 S6 教训） |
| 20 | `EnemyStopsAtWall` | 敌人与玩家之间隔一堵墙 → 敌人被夹在墙前（**证明它吃 S7 的碰撞**，前置事实 1） |
| 21 | `AttackReducesPlayerHp` | 敌人贴到 `attackEnter` 内跑足够久 → `game.PlayerHp()` **下降**，且下降量 == `attackDamage × 打的次数` |
| 22 | **`RemovingEnemyLeavesNoDanglingId`** | 删掉一个敌人后：`m_world.Find(id) == nullptr`，且 `m_enemies` 里**没有**指向已删实体的项（分叉 ① 的代价 1 —— 这是全阶段唯一会持续产生新 bug 的地方） |

> ⚠️ **用例 19/20/21 都要显式用 `kWorldCanvasW/H`（256×256）让相机退化**吗？—— **不**：
> 这三条量的是"敌人 vs 玩家"的**世界**关系，而不是玩家的屏幕位移。若能从屏幕同时找到 `'@'` 与 `'E'`，
> 它们的**屏幕格距**在相机跟随时并不等于世界格距。施工时**优先**让用例只断言"单调性/相对量"
> （如 19 的"距离减小"、21 的 HP 数值），避免引入对相机的依赖。

### 10.2 正向验收

```powershell
cmake --version                                          # ⚠️ 本机版本可能不是 4.2.1，先核
cmake -S . -B build
cmake --build build --config Debug                       # 期望 0 warning
.\build\bin\Debug\mini_tests.exe Enemy                   # 期望 18 passed
.\build\bin\Debug\mini_tests.exe Game                    # 期望 8 passed（4 旧 + 4 新）
.\build\bin\Debug\mini_tests.exe                         # 期望 146 passed, 0 failed
.\build\bin\Debug\mini_tests.exe Nope                    # 期望退出码非 0、括号里 146 case(s) registered
ctest --test-dir build -C Debug --output-on-failure       # 期望 1/1
cmake --build build --config Release                     # 期望同样 0 warning
.\build\bin\Debug\MiniEngine.exe                          # 人眼验收（下表）
```

> **本节的"期望值"是预测，不是实测。** 按 `AGENTS.md` 1.4，构建与运行由本人执行；
> 报告里**不许**写"构建通过 / 已实测"，除非那个结果确实由本人回报。

**人眼验收（8 条）**

| # | 观察点 | 期望 |
| --- | --- | --- |
| 1 | 开局 5 个 `E` | 各自沿路点**平稳往返**，连续观察 10 秒**不抖**、不穿墙、没有出生在墙里的 |
| 2 | 走近某个 `E`（≤10 格） | 它转向**朝你直线**来 |
| 3 | 贴到 ≤1.5 格 | 它**停下**并周期性攻击 → `F3` 面板/ HUD 上 HP 下降，节奏约 0.8s |
| 4 | 跑远（>14 格） | 它**回巡逻**，不追出地图 |
| 5 | 让敌人隔墙追你 | 它**卡在墙后蹭** —— ⚠️ **这就是 S14 的 A\* 动机，要亲眼看到**（不是 bug） |
| 6 | 5 个同时追 | **不打转、不抖**。⚠️ **"叠在一起"是预期行为**（分叉 ② 不做推挤），**不算失败** |
| 7 | `F3` | 面板实时显示逐敌 `state / target / distance`；再按一次关闭 |
| 8 | `F4` | 调试快照**仍然工作**（回归检查，C14） |

**人眼验收的替代做法（本机键盘的已知限制）**：`S7_CHECKLIST` 10.2 记过本机**键盘 ghosting**
（同时按 `D`+`S` 只出一个字母）。第 5 条要"把敌人引到墙后"时，**分两步按键**或用方向键。

#### 10.2.1 人眼验收的实测（本人执行）

| 条 | 结果 |
| --- | --- |
| 7（`F7` 面板：底部对齐、ASCII 不乱码、关掉无残留） | ✅ 步骤 4 验过 |
| `K` 杀敌 → 面板显示 `Dead`、`E` 消失 | ✅ 步骤 4 的 `K` 上验过；反向验证 R6 那轮又在**破坏版**里看过一次（敌人"留在屏幕上"），正好反证正确版会消失 |
| 2（走近 → 转向直线追）· 3（贴脸 → 停下 + `hp` 掉数）· 4（跑远 → 回巡逻）· 8（`F4` 快照仍工作） | ✅ 步骤 2~4 的日常观察中顺带过过 |
| **1（5 个 `E` 巡逻 10 秒不抖）· 5（隔墙追 → 卡墙蹭 = S14 的 A\* 动机）· 6（5 个同时追不打转）** | ✅ **2026-10-10 11:00 补验，本人回报"全部通过"** |
| **合计** | ✅ **§10.2 的 8 条人眼验收全部通过** |

⚠️ **两条流程记录**（不改历史，留在这儿）：
1. 第 1 / 5 / 6 条原计划 **2026-10-09 当晚**验，实际**顺延到次日 10-10 上午**才补 —— 所以 S8 的 6 个 commit
   是在"人眼未验"的状态下先推上去的。⚠️ 当时这里是**明确禁标通过**的（"在验完之前不要把 §10.2 标成通过"），
   纪律守住了；现在验完，这句话作废，改为**通过**。
2. 第 5 条（卡墙）是这 8 条里**唯一需要"布景"**的：要先把玩家引到地标楼的**正下方**（`tile 33,9`），
   让敌人从楼**右边**追过来，它的直线路径才会撞在楼的右墙上 —— 随手站着是撞不出来的。
   **这条"要摆位置"的体验本身就是 A\* 的动机**：玩家会本能地想"它怎么不绕一下"。

### 10.3 反向验证（6 条，**每条都必须看到红，跑完必须还原**）

> ⚠️ **还原之后必须重建**再继续玩/继续测（S7 的真实教训：R4 的破坏编在 exe 里没重建，
> 被当成"穿墙回归"查了半天）。

| # | 破坏方式 | 应变红的用例 | 备注 |
| --- | --- | --- | --- |
| R1 | 迟滞去掉（`loseRange = sightRange`） | `HysteresisKeepsChaseUntilLoseRange` | 守卫"不抖"这条**核心决策** |
| R2 | `Chase` 的速度方向改成常量 / 不归一化 | `ChaseMovesTowardTarget` | 守卫"追的方向" |
| R3 | 去掉冷却判定（每帧都 `damage = attackDamage`） | `AttackRespectsCooldown`（+ `AttackFiresDamageWhenCooldownElapsed` 可能一起红） | 守卫"节奏" |
| R4 | 去掉 7.2 第 4 步的零向量分支 | `ZeroDistanceDoesNotProduceNaN` | 守卫"坑 1 的 UB" |
| R5 | 把 ⑤b 驱动挪到 `World::Update` **之后** | `Game.EnemyChasesPlayer` | 守卫"帧序"（一条也可能会红得更多） |
| R6 | 删敌人时**只** `MarkForRemoval`，不同步 `m_enemies` | `Game.RemovingEnemyLeavesNoDanglingId` | 守卫分叉 ① 的代价 1 |

> ⚠️ **预测红集必须逐条写下来**（S7 的教训：R1 估 2 条结果 11 条、R4 估 7 条结果 9 条）。
> 估错的**原因**也要记 —— 那说明对这个机制的认知还缺一层。

#### 10.3.1 实测（2026-10-09；破坏由 AI 改、运行与观察由本人做）

| # | 最终用的破坏方式（与上表的差异） | 预测红集 | 实测红集 | 实际红在哪些断言 | 判定 |
| --- | --- | --- | --- | --- | --- |
| **R1①** | 原样：`loseRange = 10.0f`（== `sightRange`）+ 注掉那条 `static_assert` | 2 条（`ChaseMovesTowardTarget` + `Hysteresis`） | **1 条**（只有 `Hysteresis`） | `test_enemy.cpp:279` —— **子断言 (b)**，不是我预测的 (a) | ❌ **预测错**（见发现 1） |
| **R1②** | **改后**：⑤ 的 `dist > t.loseRange` → `dist > t.sightRange`（tuning 一个字节不动） | 2 条 | **2 条** ✓ | `:257` `:258` `:260`（`ChaseMovesTowardTarget`）· `:275` = `Hysteresis` 的 **(a)** | ✅ 用例级 2/2、断言行级 5/5 |
| **R2** | 原样：Chase 分支 `toward = mini::math::Vec2{1.0f, 0.0f}` | 4 条（1 `Enemy` + 3 `Game`） | **4 条** ✓ | `Enemy:259` `:260`；`Game:563` `:588` `:612` `:619` | ✅ 用例级 4/4、断言行级 6/6 |
| **R3** | 原样：去掉 ⑨ 的 `&& out.attackTimer <= 0.0f` | 2 条 | **2 条** ✓ | `Enemy:300`（`lhs=1 rhs=0`）· `Enemy:338`（`lhs=1 rhs=0`） | ✅ 用例级 2/2、断言行级 2/2 |
| **R4** | ⚠️ **原样不会红** → 改成手写归一化 `toward * (t.moveSpeed / toward.Length())` | 1 条 | **1 条** ✓ | `Enemy:408` `:409`（`lhs=-nan(ind) rhs=0`） | ✅ 1/1（见发现 2） |
| **R5** | ⚠️ **原样不会红** → 改成注掉整行 `DriveEnemyAi(ctx);` | 4 条 | **4 条** ✓ | `Game:525` `:563` `:588` `:612` `:619` | ✅ 用例级 4/4、断言行级 5/5 |
| **R6** | 上表写的是 S9 的"真删一半"—— **S8 没有那条路径** → 改成注掉 `entity->active = false;` | **0 条**（预判是缺口） | **0 条** ✓ | 无 —— 但 exe 里现象明显不对（见发现 3） | ✅ 证明是缺口 |
| **R6′** | 补完 `Game.EnemyDiesAndDisappears` 之后**再打一次**同样的破坏 | 1 条 | **1 条** ✓ | `Game:697` **红**（`nAfter == nBefore - 1`）· `Game:701` **绿**（面板里的 `Dead`） | ✅ 新守卫自证有效 |

**尾行数字**：⚠️ **没有逐条抄录**（只凭 `[FAIL]` 行数推断：R1① = 144/1 · R1② = 143/2 · R2 = 141/4 ·
R3 = 143/2 · R4 = 144/1 · R5 = 141/4 · R6 = 145/0 · 补用例后基线 146/0 · R6′ = 145/1）。
**这是 A7"`N passed` 未抄录"第三次复现** —— 红集本身没错，但"总数"这个独立印证点又丢了。
→ 收口时补跑一次干净构建，把 **`146 passed, 0 failed`** 抄进 §10.4。

#### 10.3.2 这一轮真正的产出：三条发现

**发现 1 —— 改 tuning 参数做反向验证是"空转"（R1 首轮栽在这）**
用例的期望值是由 `kDefaultTuning` **算出来**的（`test_enemy.cpp:46` 的
`kBetweenSightAndLose = (sightRange + loseRange) * 0.5f`，这是"不许写死数字"那条纪律的产物）。
于是把 `loseRange` 从 14 改成 10 时，**用例的输入距离也跟着从 12 变成 10** ——
**破坏和被测输入同步移动**，等于没破坏。后果：我预测的 2 条只红了 1 条，而且红的是
**另一条子断言**（`(b)` "还在巡逻 → 不该被拉进战斗"，因为 `10 <= sightRange(10)` 恰好压在
进战的 `<=` 边界上），我预测的 `(a)` 反倒绿了。
→ **反向验证要打在代码逻辑上，不要打在参数上**。参数化期望值是好设计（不用改测试就能调手感），
但它的代价就是"**参数级的破坏验不出东西**"。

**发现 2 —— 有两条"原样破坏"根本不会红（R4 / R5）**
- **R4**：`Decide` 里那句 `toward.LengthSq() > 0.0f` 是**冗余**的 —— `Vec2::Normalized()` 自己就
  挡零（`Vec2.h:113` 的 `if (!(len2 > 0.0f)) return Vec2{};`，连 NaN 一起挡）。删掉它**行为不变**。
  要让它红必须换成"手写归一化"（`moveSpeed / Length()` → 除零 → `{0,0} * inf` → **NaN**）。
  ⚠️ 顺带记一条**与 CRT 有关**的坑：MSVC 打印出来是 **`-nan(ind)`**（符号位 1 + indefinite 载荷，
  那是 x86 对**非法运算**返回的默认 QNaN），不是 `nan`。→ **用例绝不能比对 NaN 的文本**，
  只能数值比较（`MINI_CHECK_NEAR` 内部 `fabs(a-b) <= tol`，NaN 参与比较恒为 false，正合适）。
- **R5**：把 `DriveEnemyAi` 挪到 `World::Update` **之后**也不会红 —— 那只是让敌人**慢一帧**，
  而用例断言的是"0.4 秒内距离**变小**"这种**单调性**，一帧延迟看不出来。
  → ⚠️ **"AI 必须在积分之前"这条帧序目前只有注释 + 推理守卫，没有机器守卫**（已登记 `TECH_DEBT`）。

**发现 3 —— R6 是"用例全绿 ≠ 有守卫"的现场样本**
注掉 `entity->active = false;` 之后 **`145 passed, 0 failed` 全绿**，但 exe 里现象一眼就不对：
按 `F7` → 按 `K` 杀掉最近的敌人，**面板显示 `Dead`，可那个 `E` 还留在屏幕上不动**
（实测截图级描述，本人 2026-10-09 观察）。原因：`Dead` 的"停更"由**两处**共同保证 ——
`ai::Decide` 输出零速度（→ 不动）和 `entity->active = false`（→ `World::Update` 不积分、
`DrawEntities` 不绘制）。破坏只留下了"**还画着**"这一个可观察差异，而**没有任何用例看它**。
→ 处置（本人选 **A**）：补 `Game.EnemyDiesAndDisappears`，判据**必须落在"屏幕上少一个 `'E'`"**。
⚠️ 复验（R6′）证实了这个设计的关键点：**`Game:701` 的"面板里出现 `Dead`"在破坏下仍然是绿的** ——
因为那个状态来自**另一套真相**（`EnemyData.state`）。**只写那一条，今天这个破坏照样全绿。**

**发现 4（流程）—— 红集要记到「断言行号」级**
R2 的一条用例红了 **3 个断言**（`:257` `:258` `:260`），而 `:259`（`v.x ≈ 0`）**恰好绿**
（脱战后 `velocity = {0,0}`）。只记"用例名"就会把"3 条断言 vs 我预测的 1 条"误读成预测失败；
S7 那两次"估错"（R1 估 2 实 11、R4 估 7 实 9）也是同一个毛病 —— **比较的粒度不一致**。
→ 本表起开始记断言行号。

### 10.4 交付后的自检

| # | 自检项 | 怎么查 |
| --- | --- | --- |
| 1 | `engine/` **零改动** | `git diff --stat` 里不应出现 `engine/` 下的任何文件（**除了 `KeyCode.h` 那 2 行**） |
| 2 | 依赖方向 | grep `#include` 确认 `ai/EnemyBrain.h` 只含 `math/Vec2.h` + 标准库 |
| 3 | 用例总数 | `mini_tests.exe Nope` 的括号里应是 **146** |
| 4 | 现有 4 条 `Game` 用例**一个字没改** | `git diff tests/test_game.cpp` 只应有新增 |
| 5 | HUD 行宽 | 数一下 `Game.cpp` 里那条格式串，仍 **< 80** 列 |
| 6 | 调试键只出现在两处 | grep `Key::K`：`KeyCode.h` 定义 + `Game.cpp` 的常量（照 `kDumpKey` 的既定纪律） |
| 7 | **收尾抄录（2026-10-09 新加）** | 干净构建跑一次 `mini_tests.exe`，把尾行照抄进本表下面那一行 —— **A7 的"`N passed` 未抄录"已经复现三次了**，这条就是为它加的 |

> **收尾实测（本人抄录，2026-10-09）**：
> ```text
> [ OK ] Enemy.TargetDeadReleasesChase
> 146 passed, 0 failed
> PS> ctest --test-dir build -C Debug --output-on-failure
> 1/1 Test #1: mini_tests .......................   Passed    0.28 sec
> 100% tests passed, 0 tests failed out of 1
> ```
> → ✅ **`146 passed, 0 failed`；`ctest` 1/1；退出码 0；构建 0 warning**（同前几步）。
> **"总数 146"这个独立印证点落地了** —— 与"123 → 141 → 145 → 146"的计数口径一致。
> ⚠️ §10.3.1 里那 9 个总数仍是**推断**（当时只抄了红集、没抄总数），但**收尾这一次是照抄**。

---

## 11. 需要同步的文档

| # | 文档 | 改什么 |
| --- | --- | --- |
| 1 | `doc/MiniEngine_PLAN.md` | S8 小节标 **✅ 完成（日期）** + 落地数字；`A.2` 目录树把 `ai/` 从"目标态"改成"已落地 1 个文件（`EnemyBrain.h`）"并注明**没建** `StateMachine.h`；`D.1` 的 `D-1` 行注明"S8 实际用了**按类型分容器**而不是给 `Entity` 加字段"；`G` 节的 `v0.3` 行标注进度；`H.2` 扩展池里 A\* 那行的触发条件从"S8 之后"改成"S14" |
| 2 | `doc/TECH_DEBT.md` | **C12** 触发条件改写（S8 → S10，附理由）；**新增 1 条**记 `F3` 面板 + `Key::K` 调试键（C14 家族，触发条件 = S9）；**新增 1 条**记"僵尸 `EnemyData`"（触发条件 = S9 接伤害时做真删）；**C3**（`Version` 还是 `0.1.0-dev`）的触发条件已满足 → 顺手提到 `0.2.0` |
| 3 | `doc/S8_CHECKLIST.md` | 本文件：进度板打勾 + 落地偏差追加 + 10.2 的**实测数字**回填 |
| 4 | `doc/S7_CHECKLIST.md` | 不改（历史留档） |
| 5 | `AGENTS.md` | **第 7 节的三条纪律不改**（本阶段没有新纪律）。**只加了一行**：§2 的文件表补 `doc/S8_CHECKLIST.md` 那一条 —— 那张表里 S5/S6/S7 都有，缺 S8 才是异常。⚠️ 本人若不要这一行，删掉即可（纯目录性质，无功能影响） |
| 6 | `README` / 新文档 | **不创建**（`AGENTS.md` 第 3 节） |

> ⚠️ **4 笔欠账要写进 `TECH_DEBT` 而不是只写在偏差表里**：C12（第二次推迟）· `Dead` 只有调试键入口 ·
> `F3` 临时面板 · 玩家 HP ≤ 0 无事发生。理由：C12 上一轮的教训就是"推迟记录散落 → 触发条件对不上"。

---

## 12. 与 `PLAN` 原文的差异（**必须同步**）

| # | `PLAN` 原文 | 本阶段取值 | 为什么 |
| --- | --- | --- | --- |
| 1 | `D.1`（`PLAN:971`）：S8~S9 **往 `Entity` 加 `Health`/`AiState` 字段** | 放 `game/Enemy.h` 的 `EnemyData`（`Entity` 零改动） | 分叉 ①。保住 `static_assert(sizeof(Entity)==24)` 与 A.3 的"engine 不认识 Enemy"；`D.1` 自己也给了"按类型分容器"这个技巧（`PLAN:983`） |
| 2 | `A.2` 目标树（`PLAN:171`）：`ai/ StateMachine.h` + `EnemyController.h` | 只落 `ai/EnemyBrain.h`（**不建通用模板**） | 5 个状态用 `switch` 更直白；建模板是"提前造抽象"（`PLAN:574` 同一结论） |
| 3 | `TECH_DEBT:59` C12 触发 = "S8 出现第二个会动的实体" | 改到 **S10** | 敌人重叠不影响"追人不抖"；且 `PLAN:573` 坑表本身就写"先忽略实体间碰撞解算的停止条件" |
| 4 | `PLAN:562` 目标写五态 | 五态都做，但 **`Dead` 在 S8 无游戏内触发**（靠调试键） | 分叉 ③；真入口是 S9 的子弹 |
| 5 | `A.2` 目标树有 `world/Health.h` | **不建**（玩家 HP 直接放 `Game`） | 0 调用者；与 C8（`Vec3`/`Rect` 只有契约）同族 |
| 6 | `PLAN:572` 验收写"`F3` 打开后能实时看到 …" | 做成**临时最小面板** | 与 S11 的正式 Debug Overlay 抢键 → 登记 `TECH_DEBT`（C14 家族） |
| 7 | 公共头 `KeyCode.h` 的键集（`PLAN` 未提） | 新增 `Key::K`（仅调试，S9 后删） | 分叉 ③ 的取证需要；已核不碰红既有用例 |
| 8 | `PLAN:567` "Patrol 用预置路径点**往返**" | 乒乓；若实现受阻可改**环走** + 登记偏差 | 两者都满足"巡逻"这个可运行效果；7.4 |

---

## 13. 本阶段的"学习点"备忘（给自己看的）

- **有限状态机**：`enum class` + `switch` 的**穷尽性**（加一个状态，编译器会提醒你没处理）；
  状态与数据分离 —— `Decide` 只算"下一态 + 速度 + 伤害"，**世界怎么变是调用方的事**。
- **迟滞（hysteresis）**：单阈值必然抖。`sight < lose`、`attackEnter < attackExit` 两条迟滞
  是"不抖"的**全部**机制 —— 而"抖动"恰好是 `PLAN:573` 点名的那个坑。
- **更新顺序**：AI 用"上一帧位置"还是"最新位置"不是一个风格问题 —— 它决定**能不能离线复现**。
  本阶段选"上一帧"，于是 `Decide` 成了一个可以拿两遍同样输入验伪的纯函数。
- **纯函数**的好处在这一阶段第二次兑现：`Collision` 之后，`EnemyBrain` 同样不需要地图、
  不需要 `Entity`、不需要终端 —— **测试成本几乎为零**。
- **"接线前先扫全部调用点"**（`AGENTS.md` 第 7 节第三条纪律）在本阶段的产物是坑 14 那张表：
  逐条写出"它假设了什么"，而不是"我想到了哪几条"。
- **把一个决策的代价也写下来**：分叉 ① 的"两套真相"是全阶段唯一会持续产生新 bug 的地方，
  所以它换来一条专门的用例（22 号）。**没有用例的代价等于没记。**
