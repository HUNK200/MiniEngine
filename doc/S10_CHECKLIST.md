# S10 施工图 —— GameState / HUD / Wave（一个可玩的完整循环）

> 本文件是 S10 的**施工图 + 进度板 + 落地记录**，格式对齐 `doc/S9_CHECKLIST.md`。
> ⚠️ 与 `AGENTS.md` 冲突时**以 `AGENTS.md` 为准**。

---

## 当前状态

| 项 | 状态 |
| --- | --- |
| 本文件 | ✅ **2026-10-12 定稿**（**4 个分叉 + 附加项已由本人拍板**，见下表；§到期复核 5 条也已有结论） |
| **分叉拍板结果** | ① **乙**（`game/GameState.h` 纯函数 + 策略表）· ② **丙**（用 `MarkForRemoval` + `FlushRemovals` 清空 ⇒ **`engine/` 零改动且 id 不复用**）· ③ **甲**（固定模板 + 数量递增 + 封顶）· ④ **乙+丁合并**（每波回满 **且** 每击杀 +1，`cap = 初值`，意图 A）· 附加 **加**（`GameOver` 的 `Esc → Title`） |
| 代码 | **步骤 1 ✅**（本人 2026-10-12 回报 **`182 passed, 0 failed`**，169 → +13）· **步骤 2 ✅**（本人 2026-10-12 回报**全绿 + 人眼验证通过**，182 → **189**，`Game` 18 → 25）· 步骤 3~8 ⬜ 未开始 |
| 上一阶段 | S9 已全部收口并推送（`ORIGIN/main` = `a56d923`），用例 **169**、`16` 条 `Game` 用例全绿 |
| 本阶段在路线上的位置 | v0.3「玩得起来」的**最后一块**（`PLAN` G 节：① 敌人会追会打 ✅ S8 · ② 能射击能击杀 ✅ S9 · **③ HP/Ammo/Kills/Wave + 死亡重开 = 本节**）。⚠️ 它达成之后 **v0.3 就交付了**，`H.1` 的"保底路径"到此为止 —— 后面的 S11~S14 都属于加分项 |

## 分工约定（沿用 `AGENTS.md` 1.4，S8/S9 各跑通一遍）

| 谁 | 做什么 |
| --- | --- |
| **AI** | **只编辑 / 新建源文件**。不跑 `cmake`、不跑 `ctest`、不跑 `MiniEngine.exe` |
| **本人** | 构建、跑用例、跑 exe、人眼观察，把结果回报 |

每步报告必须含：**改了哪些文件** · **原样可粘贴的 PowerShell 命令** · **期望看到什么（数字 / 现象）** ·
**当前状态**（代码已写完，待构建 / 待运行 / 待肉眼验）· **⑤ 本次改动"有什么用"** · **⑥ 目前整体进度**。

⚠️ **2026-10-12 起报告的格式统一到 7 节**（本人拍板）：格式正文 = **`.agents/skills/step-report/SKILL.md`**，
`AGENTS.md` §1.4 是它的绑定规则。上面那六项是**骨架**，7 节里的**第 2 节（逐部分说明"一处改动 = 一个作用"）**
是新增的重点 —— 光写"改了哪个文件"**不算**报告完整（本人原话："改了什么有了，具体改的作用也说明一下"）。

⚠️ **不许写"构建通过 / 已实测"**，除非那个结果确实由本人回报（`AGENTS.md` 1.4）。
⚠️ **"有什么用"不许写成卖点**：要写**它挡住了哪个具体的失败**；写不出具体失败就照实说"这一步只是把某件事写进结构"。

---

## 分叉定稿（✅ **2026-10-12 本人拍板**）

> 4 个分叉 + 附加项**已定稿**（结果见 §当前状态 那张表）；本节保留**完整的取舍理由**，
> 因为"为什么不选另一条"才是下次改主意时真正要读的东西。§到期复核 5 条也已有结论。

### 分叉 ① 状态机住哪 → **✅ 取乙：纯函数表 `game/GameState.h`**（甲：嵌在 `Game` 里，未取）

**关键发现**：这个状态机的**真内容不是"4 个状态"**，而是**"哪些系统在每个状态下跑"**（4 状态 × ~11 个帧步）。
转移表只有 5~6 条边、错不到哪去；**漏停一个系统**（例如在 `GameOver` 里还在跑 AI）才是真实故障。

| | 甲：`enum class GameState` 埋在 `Game.h` + `OnUpdate` 顶部 `switch` | **乙：`game/GameState.h` 纯函数 + 策略表** |
| --- | --- | --- |
| 形态 | 1 个枚举 + 1 个成员 + 1 段 `switch`，约 +40 行，**0 个新文件** | 新增头文件（枚举 + `StateEvent` + `NextState` 纯函数 + `kStatePolicies[4]` 表），约 70 行；配 `GameState` suite |
| 好处 | 文件最少；`switch` 不写 `default` ⇒ 加第五态编译器会提醒（穷尽性） | ① 转移边**可离线单测**（"`R` 在 `Playing` 下无效"这种非法边一测就钉住）；② **策略表**把"漏停一个系统"变成**可离线断言的性质**，而不是靠读 `switch` 时眼睛不漏行；③ 与 `Bullet.h`/`EnemyBrain.h` 同一套路（**"决策与接线分家"的第 4 次兑现**）；④ `Game.cpp` 里只剩接线 |
| 坏处（代价） | 转移/矩阵**只能靠集成用例**测（要构造真 `Game` + 推帧），用例更慢也更绕 | 多 1 个文件 + 1 个 suite（约 +10 条用例）；**存在一个诱惑**：把本该是 `game` 的接线也塞进那张家 → 边界就是"表里只能说'跑不跑'，不能说'怎么跑'" |
| 依赖 | — | **只 include `<cstdint>`**：不认识 `Game` / 输入 / 屏幕 / engine（同 `Bullet.h` 的纪律，可离线测） |

**推荐乙的理由（一句话）**：它把"暂停时世界真的停了"从**读代码确认**变成**一条断言**。
✅ **本人 2026-10-12 拍板取乙**。⚠️ 留档：如果将来觉得"多一个文件不值"，**甲是可接受的** —— 本分叉**不影响任何别的接口**（退甲 = 把枚举挪回 `Game.h`、策略表换成 `switch`，`GameState.h` 整个删掉）。

### 分叉 ② 「一键重开」怎么清空世界 → **✅ 取丙：用既有原语清空（`MarkForRemoval` + `FlushRemovals`）**

**⚠️ 这条是拍板过程中改过主意的一处**：起草时的推荐是"甲（`m_world = World{w, h}`）"，
本人要求"先分析两种方式的优缺点" ⇒ 复核 `World.cpp` 时**发现了第三条路（丙）**，它**同时**拿到甲的
成本与乙的安全性，于是改取丙。三种都留档在这里。

**三条前置事实（逐行核过 `World.h` / `World.cpp`）**：
① `World` **没有**用户声明的拷贝/移动 ⇒ **隐式拷贝赋值存在**（甲能编过）；`TileMap` 内部是 `std::vector<Tile>` ⇒ 可赋值。
② `World::m_nextId` 是**成员**（初始 1、只增不减）；**`FlushRemovals()` 不碰它**，并且会把 `m_pendingRemove` 清空（`World.cpp:131`）。
③ **`MarkForRemoval` 只 push `m_pendingRemove`**（`World.cpp:101-116`）⇒ **遍历 `Entities()` 时调它是安全的**
   （S9 前置事实 6；这条有既有用例守着）—— 这正是丙可行的技术前提。

| | 甲：整体替换 `m_world` | 乙：`engine` 加 `World::Clear()` | **✅ 丙：用既有原语清空** |
| --- | --- | --- | --- |
| 做法 | `m_world = mini::world::World{w, h}` | 清 `m_entities` / `m_pendingRemove`，**保留 `m_nextId`** | `for (e : m_world.Entities()) m_world.MarkForRemoval(e.id);` → `m_world.FlushRemovals();` |
| 引擎改动 | 0 行 | +~8 行 + World suite 用例 | **0 行**（用的全是既有公开接口） |
| `m_nextId` | ★ **回到 1 ⇒ 跨 run id 复用** | 单调递增 ⇒ 不复用 | 单调递增 ⇒ **不复用** |
| **失败模式** | ★ **静默指错** | 响亮卡住 | **响亮卡住** |
| 依赖的隐含假设 | ① `StartNewRun` 是唯一入口 ② 5 样一个都不能漏 ③ `World` 永远保持可拷贝赋值 | ① 手写清理清单不能漏字段 ② 要写清"清不清 `m_map`" | ① 地图在整局之间不变（重开不需要重建地图）② 依赖"遍历中 `MarkForRemoval` 安全"这条既有契约 |
| 顺带 | 每次重开 2 次 40 KB 分配 + 拷贝（可忽略） | — | `m_entities` 清空后**顺序天然恢复**（玩家先、敌人后）⇒ 渲染顺序与 `OnStart` 时一致 |

**★ 决定性的那一条差别 = "漏掉一行时会怎样"**（9 步重置清单靠的是**纪律**，而纪律会衰减）：

| 漏了什么 | 甲（id 复用） | ✅ 丙／乙（id 单调） |
| --- | --- | --- |
| `m_enemies` | ★★ **静默、而且会"劫持子弹"**。上一局打到第 2 波以上时敌人 id 是 `2..10`；重开后新玩家 = 1、新敌人 = `2..6`、**第一发新子弹的 id 恰好 = 7** ⇒ 那条残留的 `EnemyData{id=7}` 会：① 把**子弹当敌人驱动**（每帧写它的速度）；② `IsEnemyId(7)` 让它**画成红色**；③ 同一批残留还会给 id `2..6` 的敌人**双重驱动**（同帧写两次速度、`attackTimer` 双倍递减 ⇒ 攻击冷却快一倍） | `Find` 返回 `nullptr` → 敌人面板印 `<id gone>`（**看得见**）+ `m_enemies.empty()` 永假 ⇒ **波次卡住不动**（**看得见**） |
| `m_bullets` | 侥幸自愈（`Find` 撞空 → ⑨b 清掉）—— 但"侥幸"本身就是隐患 | 同样自愈 |
| `m_player` | ⚠️ **碰巧是对的**（新玩家也是 id 1）—— 最危险的一种"对" | `Find` 空 → 玩家不画、`DrawHud` 早退（**看得见**） |

**为什么"响亮"比"静默"重要**：甲把 `Entity.h` 那句"**id 永不复用 ⇒ 最坏只是找不到，不会找错**"的
契约作用域，从"一个进程"悄悄缩成了"一个 `World` 实例"—— 而那条契约是项目从 S5 起最看重的一条
（S9 的 `m_bullets` 存 id 不存下标、`ResolveBullets` 的待删名单存 id，全都是冲着它去的）。
**丙 = 甲的成本（引擎 0 改动）+ 乙的安全性（id 不复用）** ⇒ 三选一里严格最优。

**⚠️ 丙的两条使用条件（写进代码注释）**：
1. **必须在遍历之外调用**，且清空两步要**连贯**（先全部 `MarkForRemoval`、再**一次** `FlushRemovals`）；
   ⛔ 不许在 `Entities()` 的循环里调 `FlushRemovals`（那才是真的会失效）。
2. **假设地图在整局之间不变**：丙不重建 `m_map`。当前 `BuildDemoMap` 是确定性的、且整局中无人改地图 ⇒ 成立。
   ⚠️ 触发条件：**将来地图可变时**（打到"可破坏的墙" / 从文件加载关卡）⇒ 那时重开必须**重建地图**，
   要么回到乙（给 `Clear()` 一个明确的"清不清地图"语义），要么在 `StartNewRun` 里重新 `BuildDemoMap`。

### 分叉 ②附 「重置清单」（✅ 取丙之后的版本）

```text
StartNewRun()：                      // ★ 唯一入口
1. for (e : m_world.Entities()) m_world.MarkForRemoval(e.id);
2. m_world.FlushRemovals();          // ← 此时 m_entities 为空、m_nextId 保持不变
3. m_enemies.clear();  m_bullets.clear();
4. m_playerHp = kPlayerMaxHp;  m_kills = 0;  m_elapsed = 0;
   m_facing = Facing8::East;   m_wave = 0;   m_ammo = kStartAmmo;
5. m_player = m_world.Spawn(玩家原型);     // 新 id（单调递增，与上一局不撞车）
6. BeginWave(1);                          // 内含 SpawnEnemies 那一套
   // m_map 不动（条件 2）；m_camera 不显式重置 —— 本帧 ⑦ 的 Follow 会按新位置夹出来
```
守卫用例：**`RestartLeavesNoStaleState`**（重开后场上实体数 == 该有的数；**上一局的 id 一个都查不到**；
`m_bullets` 为空）—— ⚠️ 即使丙让 id 不会撞车，这条用例**仍然值得留**：它守的是"清空真的清了"。

### 分叉 ③ 波次（Wave）的形态 → **✅ 取甲：固定模板 + 数量递增 + 封顶**

**硬约束（核过代码）**：`kEnemyCount = 5` 是**编译期常量**，`kPatrolPaths[5][2]` 只有 **5 行**
⇒ "比 5 个多"这件事**必须先扩表**（这是本分叉的主要成本，不是难点）。

| | 甲：固定模板 + 数量递增（封顶） | 乙：每波刷同样的 5 个，难度靠敌人数值 | 丙：计时刷（每 T 秒补一个） | 丁：固定种子 PRNG 随机位置 |
| --- | --- | --- | --- | --- |
| 做法 | 第 n 波刷 `min(kBase + (n-1)*kGrowth, kWaveMax)` 个；位置取巡逻表第 `i` 行 | 每波都刷 5 个；`EnemyTuning` 或 `hp` 随波次提高 | 与"是否全灭"无关，场上敌人**持续累积** | 用自写 LCG（固定种子）算出生点 |
| 好处 | 确定性、可测；难度增长**看得见**；用例能算出"第 n 波该有几个" | 0 行扩表；难度增长在手感上更明显 | 压力连续上升、体感最"塔防" | 每局不同，重玩价值高 |
| 坏处（代价） | 要扩 `kPatrolPaths` 到 `kWaveMax` 行（人工核对落在空格上，同 S8 的老规矩） | 5 个来源不变 ⇒ 波次这件事**没什么可玩的**（等于只换了数值） | ⛔ **打破上界**：敌人总数**无界** ⇒ 直接触发 `TECH_DEBT` **C19**，必须加 cap | ⛔ 引入**随机**，与 `BuildDemoMap` 那句"**确定性**写死（不随机、不读文件）"冲突；且要用例能复现就得把种子暴露出来 |
| 触发条件 | — | — | 想做"塔防式"压力时（那时**先定 cap**） | 真觉得"每局一样"无聊时 |

✅ **本人 2026-10-12 拍板取甲**：它让"第几波"这个数**同时**是"HUD 上显示的数"与"用例能算出来的数"
（S5_CHECKLIST 10.4 的纪律：用例不许写死数字）。⚠️ 顺带解决 `C19` 的到期问题（见 §到期复核）。

**✅ 取甲之后的实测常量（本人 2026-10-12 定，都可调）**：`kWaveBase = 3` · `kWaveGrowth = 1` ·
`kWaveMax = 8` ⇒ 第 n 波 `min(3 + (n-1), 8)` 个：`3, 4, 5, 6, 7, 8, 8, 8, …`
⚠️ `kPatrolPaths` 要扩到 **8 行**（前 5 行沿用 S8 的，后 3 行新加、位置**人工核对落在空格上**）。
⚠️ `kWaveMax = 8` **同时**是弹药张力旋钮（分叉 ④ 的意图 A 用的就是它）。

### 分叉 ④ 弹药（Ammo）怎么取 → **✅ 取乙+丁合并：每波回满 AND 每击杀 +N，`cap = 初值`，意图 A**

**前置事实**：`PLAN` S10 的目标里写了 `Ammo`，`input_plan.md` 的 HUD 样例也写着 `Ammo: 12`；
但 `PLAN` 对"怎么补充"**一个字都没写** —— 而"有限弹药 + 无处补充"= **死局**（打光之后玩家只能
看着敌人贴脸，没有任何翻盘手段），所以**必须**同时定补充方式。

**⚠️ 这条也改过主意**：起草时给的是"乙（每波回满）vs 丁（每击杀 +N）二选一"，本人追问
"**乙和丁是否可行**" ⇒ 结论是**可行，而且合并后最好**，但**必须加 cap**，并且要先算清"净耗"。

**净耗公式（本分叉的全部数学）**：打死一个敌人要 `C = kEnemyMaxHp / kBulletDamage = 3` 发，
每击杀返还 `N` 发 ⇒ **净耗 = (C − N) 发/敌人**：

| N | 净耗/敌人 | 后果 |
| --- | --- | --- |
| 0 | 3 | 纯乙：波内必弹尽（张力最强，也可能死局） |
| **1** | **2** | ✅ **取这个**：打得准就够用，**失误才缺**（打空的子弹只出不回） |
| 2 | 1 | 很宽松 |
| 3 | 0 | 只要一直在杀，永不缺弹（等于无限） |
| ≥4 | **负** | ⛔ **弹药无界增长** ⇒ 没有 cap 就是又一个 `C19` |

**必须有 cap 的第二个理由**：**"每波回满"只发生在波与波之间**，而"每击杀 +N"在一波内就能累积 ——
没有 cap 时，前期弱波可以**慢慢磨着囤弹**，把后面的难度设计整个废掉。
⇒ 规则：`m_ammo = min(m_ammo + kAmmoPerKill, kMaxAmmo)`，且 **`kMaxAmmo = kStartAmmo`**（不许囤过初值）。

**两种设计意图（都成立，取 A）**：

| | 判据 | 体验 |
| --- | --- | --- |
| **✅ 意图 A（取这个）** | `(C − N) × kWaveMax < kStartAmmo` | 全中时一路够用，**只有失误才紧**（友好，贴合 v0.3 的"外人能玩 5 分钟"） |
| 意图 B | `(C − N) × kWaveMax > kStartAmmo` | 后期**必然弹尽**（更硬核） |

**✅ 取定的一组常量（本人 2026-10-12 定，**都可调**，建议都公开）**：

| 常量 | 值 | 说明 |
| --- | --- | --- |
| `kStartAmmo` | **18** | 每波起手弹药 |
| `kMaxAmmo` | **18** | = `kStartAmmo`（不许囤过初值） |
| `kAmmoPerKill` | **1** | N=1 ⇒ 净耗 2/敌人 |
| `kWaveBase` / `kWaveGrowth` / `kWaveMax` | 3 / 1 / **8** | 来自分叉 ③ |

代入验算：`(3 − 1) × 8 = 16 < 18` ✅ **意图 A 成立**（全中时够用，约 1 次失误才会紧）。
⚠️ **与分叉 ③ 耦合**：`kWaveMax` 同时是"难度旋钮"和"弹药张力旋钮"，**两个意图不能同时满足** ——
改数值前先代一遍上面那个不等式。

**未取的对照项**：

| | 甲：不做 Ammo | 纯乙（N=0） | 纯丁（无每波回满） |
| --- | --- | --- | --- |
| 为什么没取 | HUD 少一项、与 `input_plan.md` 样例不符 | 若打光时本波敌人还活着 ⇒ 玩家只能跑着等死（**死局**） | 弹药变成"击杀数的函数" ⇒ 几乎不会弹尽，Ammo 退化成装饰 |

**⚠️ 用例怎么测（不依赖"真的弹尽"，否则是利用例碰运气）**：
1. **打光**：**对着墙**连开 `kStartAmmo` 发（不击杀 ⇒ 无返还）⇒ `Ammo() == 0`，且**第 `kStartAmmo + 1` 发
   屏幕上不出新 `*`**（`CountChar(fb, '*')` 不变）。⚠️ 关键在"对墙"—— 对敌人打会因为击杀返还而不确定。
2. **返还**：打死一个敌人后 `Ammo()` 比"没有返还时"多 `kAmmoPerKill`（用公开常量算差值）。
3. **cap**：在 `Ammo()` 已接近 `kMaxAmmo` 时击杀，`Ammo()` **不超过** `kMaxAmmo`。

---

## 进度板

> ⚠️ 每一步都必须停在**能构建、能跑**的状态（`AGENTS.md` 第 7 节第二条）。做完一步就在这儿打勾，
> 并把"本人回报的数字"抄进来 —— **不许写"构建通过 / 已实测"**，除非那个结果确实是本人回报的。

| 步骤 | 内容 | 状态 |
| --- | --- | --- |
| **0** | 需求 + 4 个分叉 + 附加项 + 本施工图 + §到期复核 | ✅ **2026-10-12 定稿**（分叉已拍板） |
| **1** | `game/GameState.h`（枚举 + `StateEvent` + `NextState` + `kStatePolicies`）+ `tests/test_gamestate.cpp`（13 条）+ `tests/CMakeLists.txt` +1 行 | ✅ **2026-10-12 完成**：本人回报 **`182 passed, 0 failed`**（169 → +13）· `engine/` 零改动 ✓ · 2 条落地偏差见下表 |
| **2** | 状态机接线：`m_state` + `OnUpdate` ⓪（读 `Enter`/`Esc`/`R` → 事件 → `NextState`）+ 按 `PolicyOf` 分派 + `DrawTitle` / `DrawGameOver` / 暂停横幅 | ✅ **2026-10-12 完成**：本人回报**全绿 + 人眼验证通过**（182 → **189**，`Game` 18 → 25）· `engine/` 零改动 ✓ · 新增 `Game::State()` 公开接口 · **6 条落地偏差见下表（偏差 3 最重要：18 条既有用例全受影响）** · ⚠️ 本步结束时"按 `R` 不重开"是**已知中间态**（步骤 3 的 `StartNewRun()` 修它；`DrawGameOver` 里也留了"补 `R restart`"的注释） |
| **3** | `StartNewRun()`（§4 的 6 步清空）+ `OnStart` 瘦身 + 重开两条用例 | ⬜ 未开始（189 → **191**） |
| **4** | 波次：`kPatrolPaths` 5 → 8 行 + `BeginWave` / `TickWave` + HUD 显示 `wave` | ⬜ 未开始（191 → **193**） |
| **5** | Ammo：扣减 / 每波回满 / 每击杀 +1 / `min(…, kMaxAmmo)` + HUD 加 `ammo` 列 | ⬜ 未开始（193 → **197**） |
| **6** | HUD/画面收尾：行 2 挤进 `ammo`/`wave` + 键位提示 + 各状态横幅 → **重新数 80 列** + 改写 `HudFitsInTwoLinesWithoutOverflow` | ⬜ 未开始（**197**，本步只改写、不新增） |
| **7** | 收口：§11 的文档逐条同步 + `TECH_DEBT`（§到期复核 5 条 + 新增 4 项） | ⬜ 未开始 |
| **8** | 验收：正向（§10.2）+ 反向验证（§10.3，**11 场**）+ 人眼（§10.2 的 12 条） | ⬜ 未开始 |

⚠️ **步骤 2 与 3 的顺序不要换**（§5 末尾那句）：先有"谁决定状态"，再有"重开怎么清"。
反过来做会让"谁能触发重开"这件事**没有守卫**。

---

## 落地偏差（实现时发现的问题，逐步追加）

| # | 施工图写的 | 实际落地 | 为什么 |
| --- | --- | --- | --- |
| **1** | §6.1 的接口清单是 **6 项**（`GameState` / `StateEvent` / `StatePolicy` / `kStatePolicies` / `PolicyOf` / `NextState`） | **多加了 7 行**：`detail::SimulatingStateCount()`（6 行）+ **3 条 `static_assert`** | 分叉 ① 说这张表存在的理由是"把矩阵从读代码变成**断言**" ⇒ 那就让最要命的那条性质（**只有 `Playing` 会模拟**）**在编译期**成立。"会模拟的状态恰好 1 个" + "那一个必须是 `Playing`" **两条合起来才等价**（只写 `== 1` 时，"关掉 `Playing`、同时打开 `Paused`"也能过）—— 这正是 `TECH_DEBT` C18"恒真断言比没有更坏"的反面案例。⚠️ 代价：改坏这张表会**编译不过**（见偏差 2） |
| **2** | §10.3 的 **R1** 写"把 `Paused` 的 `simulate` 改成 `true` ⇒ 应红：`GameState.OnlyPlayingSimulates`" | **实际会先在偏差 1 那条 `static_assert` 上编译不过**，根本跑不到用例 | 反向验证的"红"有很多种，**编译期红比用例红更早、也更省事**。⇒ R1 的考察方式要重定：**(a)** 接受"编译期红"并照实记录；或 **(b)** 临时把那条 `static_assert` 一起去掉，才能看到用例红。**待做反向验证时由本人定**（施工图 §10.3 的 R1 那一行按此修正） |
| **3** | §5 步骤 2 只写了"'按 `R` 不重开'是已知的中间态"，**没提既有用例** | **18 条既有 `Game` 用例全部受影响**：`m_state` 初值改成 `Title` 后，`OnStart()` 之后**世界不会自己动**（`Title` 的策略五项全 false）⇒ 不先按一次 `Enter`，`@` 一格都不会动、`WalkPlayerNextToEnemy` 会走满上限返回 -1、`HudFitsInTwoLinesWithoutOverflow` 量到的会是标题屏 | ⚠️ **这正是 `AGENTS.md` 第 7 节第三条要抓的东西**（"接线前按'谁假设了它不存在'扫一遍全部调用点"）：那 18 条的**隐含前提**是"游戏一开局就在 `Playing`"。做法：加 `StartPlaying`（按一次 `Enter`）辅助，**摆在调用点上**（不藏进 `StepFrame` —— 藏着的话"`Title` 下什么都不许变"那几条新用例就没法推帧了）。覆盖：`RunHolding` 内 2 条 · `WalkPlayerNextToEnemy` 内 8 条 · 显式 8 条 = **18/18** |
| **4** | §7.2 的 `DrawGameOver` 提示行 | 只写 `ESC back to title`，**暂时不写 `R restart`** | `R` 的世界重建是**步骤 3** 的 `StartNewRun()`；本步按 `R` 只会让状态闪一帧。**写了就是假提示**（S9 偏差 10 的教训：面板一直在教人按一个已经不存在的键）。⇒ **步骤 3 落地 `StartNewRun` 之后必须回来补上 `R restart`**（`Game.cpp` 的 `DrawGameOver` 里留了 TODO 注释） |
| **5** | §10.1 用例 1 写"按 `Enter` → `Playing` **且第 1 波已刷**（敌人 = `kWaveBase`）" | 前半条（`Title` → `Playing`）已实现；**"第 1 波已刷"那半条要等步骤 4** | 本步还没有波次：敌人仍是 `OnStart()` 里铺的 5 个，`kWaveBase = 3` 是步骤 4 的事。⇒ 用例 1 的波次断言**在步骤 4 补**（现在先用"`Playing` 下 `@` 真的会动"当正向对照） |
| **6** | §10.1 用例 6 的判据写"`GameOver` 下按住 `D` 推 60 帧，**`'@'` 不动**" | 改判据为 **HUD 第 1 行的 `pos` 栏逐字符不变**（外加 `State()` 仍是 `GameOver`） | ⚠️ `'@'` **会被同格的敌人盖住**：敌人之间没有推挤（`TECH_DEBT` C12），一个敌人可能正好站在玩家那一格上、后画 ⇒ `FindChar('@')` 找不到 ⇒ **假红**（不是状态机的问题）。HUD 是**最后画**的、且 `pos` 就是玩家自己的世界坐标 ⇒ 既不会被盖住，还比"格子动没动"精确（位移 0.01 都会变）。用例 7 里那条"整屏逐字节"用的是同一条思路 |

---

## 到期复核（`AGENTS.md` 第 7 节第三条：接线前按"谁假设了它不存在"扫一遍）

> 这几条 `TECH_DEBT` 的**触发条件**恰好写着 S10（刷怪 / HUD 扩展），按纪律**必须**在这里给结论，
> 不能装作没看见。**5 条结论已于 2026-10-12 定稿：全部维持现状**，逐条理由如下。

| # | 事项 | 触发条件原话 | 本阶段的结论 |
| --- | --- | --- | --- |
| **C12** | 实体间 **AABB 推挤**没做 | "→ **S10** 刷怪后同屏敌人变多、重叠到影响可玩性时" | **仍不做，触发条件顺延到"S11 之后 / 真的觉得叠成一坨"**。三条理由：① 推挤要么进 `World::Update`、要么在 `game` 侧挪玩家，**两条都会打破"位移只有一条路径"**（S7 起守着的原则，`Collision.h` 是唯一的位移解算）；② 会动既有 `Game` 用例的期望值（S8 已经算过一次这笔账）；③ 刷怪**不改变**一个事实 —— 波次里敌人是**分散出生**的（巡逻表各行错开），重叠概率不比 S8 高。⚠️ **代价照实记**：屏幕上仍可能出现"两只敌人叠在一格、只画得出一个 `E`"，所以**用例一律不许数屏幕字符**当敌人数，要读 `F7` 面板的 `n=`（S9 偏差 9 的教训 + `SummarizeEnemyPanel` 已备好） |
| **C19** | 实体 / 子弹 / 敌人**没有数量上限** | "**S10 刷怪**（第一个真正会累积的地方）" | **不需要加 cap**，理由是**结构性的**（`PLAN` A.6 ②）：分叉 ③ 取"甲" ⇒ **只有全灭才刷下一波** ⇒ 场上敌人 ≤ `kWaveMax`（由表长封顶）；子弹上界仍是 ≤378（A.6 ⑤，S10 没改武器形态）。⚠️ **触发条件改挂**："出现**与'是否全灭'脱钩的生成**（计时刷怪 / 一波未清完就刷下一波 / 霰弹 / 分裂）时" |
| **C22** | `DriveEnemyAi` 里 `hp <= 0 → active = false` **不可达** | "**第二条伤害路径落地时**复核" | **仍不可达**：S10 的波次只**生成**敌人，**没有新的伤害来源**（玩家仍只有 `DRIVE` 那条攻击）。⇒ 触发条件不变，**继续留着当防御** |
| **C23** | `m_bullets` 自愈压缩**没有机器守卫** | "子弹数量真的变大时（S10 的多武器 / 刷怪）" | **触发条件不成立**：S10 不做多武器、不做霰弹，子弹上界不变 ⇒ **建议等 S11 的 Debug Overlay 落地时一起做**（`C23` 自己就推荐了这条路：在面板表头加 `bullets=N` 当观察面） |
| **C24** | HUD **行数**没有单一真相 | "下一次增 / 减 HUD **行**时" | ⚠️ **本阶段是"加列"不是"加行"**（§5 步骤 5 实测两行够用）⇒ **不触发**。但要在 §坑表 里写清楚"加列"与"加行"的风险差别，免得下次混为一谈 |

---

## 0. 前置事实（写方案前逐条核过的，不是推测）

| # | 事实 | 位置 | 对 S10 的意义 |
| --- | --- | --- | --- |
| 1 | `GameState` **不存在**；`Game` 里唯一的"状态"是 `bool m_quit` | `Game.h:203` | 分叉 ① 的起点 |
| 2 | **玩家 HP 归 0 目前什么都不发生**（不会死、不会重开），唯一后果是敌人脱战（`targetAlive = m_playerHp > 0`） | `Game.cpp:309` / `Game.h:209-212` | 注释里已经写明"真正的死亡/重开是 S10 的 `GameState`" ⇒ 本阶段**兑现这句承诺** |
| 3 | 帧序 = ①方向 ②归一化 ③写玩家速度 ③b 朝向 ④Q ④b面板 ⑤Σdt ⑤b`DriveEnemyAi` ⑤c`FireBullet` ⑤d`ResolveBullets` ⑥`m_world.Update` ⑦相机 ⑧F4 ⑨`FlushRemovals` ⑨b子弹表压缩 | `Game.cpp:491-600` | S10 要在**最前面**插 ⓪状态转移，并把中间的步骤**按状态分派**（§7.1） |
| 4 | `World` **无用户声明的拷贝/移动** ⇒ 隐式拷贝赋值存在；`TileMap` 内部是 `std::vector<Tile>` | `World.h:34` / `TileMap.h:85` | 分叉 ② 的**甲**方案靠它可行 —— ✅ 但**没取甲**（取丙），这条留档备查 |
| 5 | `World::m_nextId` 是**成员**，初值 1、只增不减；**`FlushRemovals()` 不碰它**，且会 `m_pendingRemove.clear()` | `World.h:92` / `World.cpp:131` | ★ 分叉 ② 的核心：甲会把它**重置**（⇒ 跨 run id 复用）；**丙/乙不会** |
| 6 | `Enter` / `Escape` / `R` **早就在 `KeyCode.h` 里映射好了**（S2 起），且**0 调用者** | `KeyCode.h:28-31` | ⇒ **分叉 ② 取丙（引擎零改动）之后，S10 完全不动 `engine/`**。`Escape` 是**刻意**留给 S10 的（`Game.cpp:529` 那句"Esc 刻意不退出"） |
| 7 | `Application::Run` 每帧无条件调 `OnUpdate` + `OnRender`，**没有暂停通路**；`IGame` 只有 4 个虚函数 | `Application.cpp:81-149` | "暂停"**必须**在 `game` 侧实现（不调模拟那几步），**不新增 `IGame::OnPause`** |
| 8 | `Time` **不累积任何时间**：每帧 `dt` 独立、已 `ClampDelta`（上限 0.05） | `Time.h:31-40` | ⇒ `PLAN` 那条坑"**暂停时累积 dt、恢复瞬间瞬移**"在这套结构里**只可能由 `game` 侧自造**（谁去攒一个"游戏时钟"谁就踩）。S10 的规矩：**暂停期间 `m_elapsed` 一点也不加**，并且**不引入任何"积压 dt"通路** |
| 9 | HUD 两行：行 1 = **58 列**、行 2 = **44 列** | `Game.cpp:704-724` | 加 `ammo`/`wave`/新键位提示后**实测约 67 列** ⇒ **两行够用，不需要加第三行**（`S9_CHECKLIST` 分叉 ④ 的预测成立） |
| 10 | `kEnemyCount = 5` 是**编译期常量**；`kPatrolPaths[5][2]` **只有 5 行**；`SpawnEnemies` 用 `i` 当 `patrolPath` | `Game.h:106,127` / `Game.cpp:250` | 分叉 ③ 的硬约束：想刷多于 5 个 ⇒ **先扩表**（且 `patrolPath` 是 `std::uint8_t`，表长上限 255，**不是**瓶颈） |
| 11 | 敌人真删已落地：`ResolveBullets`（⑤d）收 id 名单 → 循环外 `MarkForRemoval` + `erase_if(m_enemies)` | `Game.cpp:460-474` | "全灭"判据 `m_enemies.empty()` 可用了 ⇒ 波次推进有了现成的信号 |
| 12 | `m_enemies` / `m_bullets` 的**遍历中不许增删**（`erase_if` 是 swap-erase、`Spawn` 可能扩容）；但 **`MarkForRemoval` 只 push 另一个 vector ⇒ 遍历 `Entities()` 时调它安全** | `Enemy.h` 文件头 / `Game.h:214-231` / `World.cpp:101-116` | ★ 分叉 ② 取丙的技术前提。`StartNewRun()` 内含**完整的清空 + 重新 `Spawn`** ⇒ 它**必须**在"没有任何别的遍历"的时刻调（帧首 ⓪ 恰好满足） |
| 13 | 开局在 `OnStart` 里：`BuildDemoMap` → `Spawn` 玩家 → `SpawnEnemies` | `Game.cpp:219-235` | S10 要把这一串**搬进 `StartNewRun()`**（分叉 ①/② 的结果），`OnStart` 只剩"建个空世界"或彻底空着 |
| 14 | 用例侧辅助已齐备：`StepFrame` / `StepFrames` / `FireOnce` / `CollectEnemies` / `CountChar` / `MinEnemyDistanceSq` / `WalkPlayerNextToEnemy` / `ScreenText` / `SummarizeEnemyPanel` / `OpenPanelAndSummarize` / `kTestHudRow` / `kTestFirstWorldRow` | `tests/test_game.cpp` 匿名 namespace | 波次/重开/暂停的用例**直接复用**，不重造 |
| 15 | `input_plan.md` 的 HUD 目标是 `HP: 80  Ammo: 12  Kills: 5  Wave: 2` | `input_plan.md:336-339` | HUD 四个字段的**原始需求**；S9 已落 `hp`/`kills`，本阶段补 `ammo`/`wave` |
| 16 | `kPlayerMaxHp = 10` 在 **private**（注释：用例断的是"HP 下降"而不是"HP == 某数"） | `Game.h:114` | ⚠️ S10 的"重开后 HP 回满"需要一个**期望值** ⇒ 大概率要把 `kPlayerMaxHp` 挪到 public（同 S9 偏差 7 挪 `kEnemyMaxHp` 的**同一条理由**：用例不许写死数字） |
| 17 | `m_world` 是**值成员**、`m_camera` 也是值成员；`m_player` 是 id | `Game.h:195-203` | 重开时"清到哪一层"就是分叉 ② 的题目 |

---

## 1. 目标 / 明确不做

| | 内容 |
| --- | --- |
| **目标** | 一个**完整的单向循环**：开局提示 → `Enter` 开始 → 刷第一波 → 射击/被追打 → `Esc` 暂停 → 死亡 → `GameOver` → `R` 重开 → 再打一轮。HUD 四字段 `hp / ammo / kills / wave` 全在 |
| **PLAN 原话** | "`HP/Ammo/Kills/Wave` 面板 + `Title/Playing/Paused/GameOver` 状态机 + 刷怪波次"；可运行效果"开局提示 → 按 Enter → 刷怪 → 射击 → 掉血 → 死亡 → 按 R 重开"；验收"从启动到死亡再重开，**循环 3 次**不泄漏、不卡死；**HP 归零后不再响应移动**" |
| **明确不做** | 拾取 / `Pickup`（真要有"掉在地上的弹药/血包"时再做）· 多武器 / 换武器 / 霰弹（`C19` 的触发）· 推挤 `C12`（见 §到期复核）· 粒子与死亡特效（`PLAN` H.2 扩展池，已可挑）· 存档 / 最高分 / 持久化 · 鼠标瞄准 · A\*（S14）· **引擎侧的暂停接口**（不加 `IGame::OnPause`）· **`World` 的通用"重置" / `Clear()` 接口**（分叉 ② 取丙 ⇒ **`engine/` 零改动**）· 计时刷怪 / PRNG 随机出生点（分叉 ③ 已否） |
| **⚠️ 不做的边界** | "不泄漏"**不等于**要做内存检测工具。本阶段的"不泄漏"判据 = **重开 N 次后场上实体数 / 子弹表长度不随次数增长**（可在用例里数出来）+ 人眼看任务管理器内存稳定（S9 已经这么验过一次） |

---

## 2. 交付物

> ✅ 这张表按**已定稿的组合（①乙 + ②丙 + ③甲 + ④乙+丁）**写。

| 类别 | 文件 | 预计规模 |
| --- | --- | --- |
| **新增（game）** | `game/include/game/GameState.h` = `GameState` 四态 + `StateEvent` + `NextState` 纯函数 + `kStatePolicies[4]` 策略表 | ~70 行 |
| **改（game）** | `game/include/game/Game.h`：公开面（`State()` / `Wave()` / `Ammo()` + `kPlayerMaxHp` 挪 public + 弹药三常量 + 波次三常量）+ 私有新增（`StartNewRun` / `BeginWave` / `TickWave`）+ 新成员 + **`kPatrolPaths` 扩到 8 行** | ~70 行 |
| **改（game）** | `game/src/Game.cpp`：⓪状态转移 + 分派、`StartNewRun`（丙的清空）、波次、Ammo（扣减/返还/cap）、`DrawTitle`/`DrawGameOver`/暂停横幅、HUD 加列 | ~220 行 |
| **改（engine）** | ✅ **零改动**（分叉 ① 取乙只加 `game/` 侧的头文件；分叉 ② 取丙走既有公开接口；键位早就映射好了） | **0 行** |
| **测试** | `tests/test_gamestate.cpp`（新，suite `GameState`，~12 条）· `tests/test_game.cpp`（+~12 条、**改写 1~2 条**）· `tests/CMakeLists.txt`（+1 行） | ~450 行 |
| **文档** | 本文件 · `PLAN`（S10 小节标 ✅ + 按实际落地重写、`A.2` 目录树加 `GameState.h`、`G` 节 v0.3 ③ 标 ✅、`H.1` 保底路径标注达成）· `TECH_DEBT`（§到期复核的 5 条 + 新增项）· `AGENTS.md` §2 加一行 | — |

**预计用例总数：169 → 约 197**（`GameState` +13、`Game` +15；另有 1 条是**改写**而非新增）。

---

## 3. 依赖规则

| 层 | 允许 include | 备注 |
| --- | --- | --- |
| `game/GameState.h`（分叉 ① 取乙 ⇒ 本阶段有这个文件） | **只有 `<cstdint>`** | **纯枚举 + 纯常量表 + `constexpr` 纯函数**。⚠️ 连 `mini/` 的任何头都不 include（不认识 `Vec2`、不认识 `Entity`、不认识输入）—— 与 `Bullet.h`/`EnemyBrain.h` 同一条纪律，这样它能被**完整离线覆盖**。⚠️ 代价（要说清楚）：**它不知道"按住 `Esc` 会怎样"** —— 键是从 `Input` 读的，"键 → `StateEvent`"这一步**留在 `Game.cpp`**（那是接线） |
| `Game.h/.cpp` | `game/GameState.h` + `game/Bullet.h` + `game/Enemy.h` + 引擎公共头 | 只做**接线**：读键 → 造 `StateEvent` → 问 `NextState` → 按 `PolicyOf(state)` 分派 |
| `engine/` | **不许 include `game/`** | `PLAN` A.3 单向。本阶段预计**零改动** |

⚠️ `GameState.h` **不是**"游戏对象的类" —— 它和 `Bullet.h` 一样是**无状态模块**：没有 `struct GameStateData`，状态就是 `Game` 里一个枚举成员。

---

## 4. 数据结构与所有权

| 数据 | 在哪 | 所有权 / 生命周期 |
| --- | --- | --- |
| **游戏状态** | `Game::m_state`（`GameState`） | `Game` 拥有；四态，**没有无效值**（初值 = `Title`）。⚠️ 它**不属于"一局"** ⇒ 重开**不清**它（重开只是"从 `Title`/`GameOver` 走到 `Playing`"的一次转移） |
| **波次** | `Game::m_wave`（`int`）+ `m_waveTimer`（`float`） | `Game` 拥有；`m_wave` 只增（**从 1 开始**，0 = 还没开始）；`m_waveTimer` 是"本波清完到下一波的喘息倒计时" |
| **弹药** | `Game::m_ammo`（`int`） | `Game` 拥有；开火 −1、**每击杀 +`kAmmoPerKill`**、**每波开始回满**，且用 `min(…, kMaxAmmo)` 夹取（**永不为负、永不超过 `kMaxAmmo`**） |
| 玩家 HP | `Game::m_playerHp`（已存在） | `Game` 拥有；`DriveEnemyAi` 扣它（既有路径），**S10 第一次给它后果**（归 0 → `GameOver`） |
| 击杀数 | `Game::m_kills`（已存在） | `Game` 拥有；只增；⚠️ **跨波次不清零**，只在**重开**时归 0 |
| 世界（实体表） / 玩家 id / 敌人表 / 子弹表 | `m_world` / `m_player` / `m_enemies` / `m_bullets`（都已存在） | **重开时：实体表清空重建、敌人表清空重铺、子弹表清空**（分叉 ② 取丙）；⚠️ `m_world` 本体与 `m_map` 都**不重建** |
| 面板开关 / 日志计数 / 退出标志 | `m_showAiPanel` / `m_dumpCount` / `m_quit` | ⚠️ 它们**不属于"一局"** ⇒ **重开不清**（`m_dumpCount` 是"本次进程按了几次 F4"，清了反而让日志编号重复）。见 §坑表 9 |

**`StartNewRun()` 的重置清单（**唯一**入口，顺序即依赖）**：

```text
1. for (e : m_world.Entities()) m_world.MarkForRemoval(e.id);   // ★ 只登记，不动 m_entities
2. m_world.FlushRemovals();                                     // ★ 一次做完；m_nextId 保持不变
3. m_enemies.clear();   m_bullets.clear();
4. m_playerHp = kPlayerMaxHp;  m_kills = 0;  m_elapsed = 0;
   m_facing = Facing8::East;   m_wave = 0;   m_ammo = kStartAmmo;
5. m_player = m_world.Spawn(玩家原型);                           // 新 id（单调递增 ⇒ 与上一局不撞车）
6. BeginWave(1);                                                // 内含"按巡逻表铺 N 个敌人"
   // m_map 不动（分叉 ② 的条件 2）；m_camera 不显式重置 —— 本帧 ⑦ 的 Follow 会按新位置夹出来
```

⚠️ **1/2 必须在 3~6 之前**，且 1 与 2 要**连贯**（先全登记、再一次 flush）：
⛔ **不许在 `Entities()` 的循环里调 `FlushRemovals`** —— 那会当场让手里的 span 失效（1 之所以安全，正是因为
`MarkForRemoval` 只碰 `m_pendingRemove`）。
⚠️ **5 必须在 2 之后**：`Spawn` 往刚清空的 `m_entities` 里推 —— 顺序反了会连新实体一起删掉。
⚠️ **`m_nextId` 全程不变** ⇒ 本局的 id 与上一局**永不撞车**（这是分叉 ② 取丙的全部收益）。

---

## 5. 逐文件实施步骤

| 步 | 改什么 | 不做什么 |
| --- | --- | --- |
| **1** | （分叉 ① 取乙 ⇒ 这一步一定做）新建 `game/include/game/GameState.h`（枚举 + `StateEvent` + `NextState` + `kStatePolicies`）+ 新建 `tests/test_gamestate.cpp`（suite `GameState`）+ `tests/CMakeLists.txt` +1 行 | 不碰 `Game.*`（本步只加"能离线跑"的东西 —— 与 S9 步骤 2 同一个切法） |
| **2** | 状态机接线：`Game::m_state` + `OnUpdate` 顶部 ⓪（读 `Enter`/`Esc`/`R` → 造 `StateEvent` → `NextState`）+ 按 `PolicyOf(state)` 分派既有帧步（含 `hp<=0` 那条死亡边）+ `DrawTitle`/`DrawGameOver`/暂停横幅 + `Title` 下不画 HUD | 不动波次与 Ammo；⚠️ **不做重开**（本步先只切状态，"按 `R` 什么也不发生"是已知的中间态 ⇒ 见下面那句） |
| **3** | `StartNewRun()`（§4 的 6 步清单，**丙的清空**）+ 把 `OnStart` 里的"铺地图 + Spawn 玩家 + 铺敌人"搬进去；`R`（`GameOver`）与 `Enter`（`Title`）都走它；新用例 `RestartLeavesNoStaleState` | 不动波次数量（第 1 波仍是 5 个） |
| **4** | 波次：扩 `kPatrolPaths` 到 **8 行** + `BeginWave(n)` / `TickWave(dt)`（全灭 → 倒计时 → 下一波）+ HUD 显示 `wave` | 不做随机、不做计时刷怪（分叉 ③ 已排除） |
| **5** | Ammo：`m_ammo` + `FireBullet` 里的"0 不开火"与扣减 + **每波回满** + **每击杀 +`kAmmoPerKill`（带 `kMaxAmmo` 夹取）** + HUD 加 `ammo` 列 | 不做 Pickup |
| **6** | HUD/画面收尾：行 1 留原地、行 2 加 `ammo`/`wave` 与键位提示（`ENTER`/`ESC`/`R`）+ 各状态横幅 → **重新数 80 列** | ⚠️ **不加第三行**（前置事实 9 已实测 67 列够用）。若真要加行 → 必须按 `C24` 清单扫 5 处 |
| **7** | 收口：§11 的文档逐条同步 + `TECH_DEBT`（§到期复核的 5 条 + 新增） | 不改历史留档（S8/S9 的 CHECKLIST 不动） |
| **8** | 验收（§10） | — |

⚠️ **每一步都必须是"能构建、能跑"的状态**（`AGENTS.md` 第 7 节第二条）。
⚠️ 步骤 2 结束时"状态机能切、但按 `R` 不重开"是**一个能跑的中间态**（重开在步骤 3）；反过来做（先写重开、再写状态机）
会让"谁能触发重开"这件事没有守卫 —— **顺序不要换**。

---

## 6. 接口设计（**只列声明，不写实现**）

### 6.1 `game/include/game/GameState.h`（新建；分叉 ① 取乙 ⇒ 本阶段有这个文件）

```cpp
namespace game {

// 四态。⚠️ 与 `ai::EnemyState` 一样用 enum class：用例断言 `State() == GameState::Paused`
// 不必比浮点/字符串，面板也能直接查表画名字（同 S8 的理由）。
enum class GameState : std::uint8_t { Title = 0, Playing, Paused, GameOver };

// 触发转移的"事件"。⚠️ 它是**这一帧发生过什么**的抽象，不是按键 ——
// "哪个键算哪个事件"由 `Game.cpp` 决定（那是接线；本模块不认识输入）。
enum class StateEvent : std::uint8_t { None = 0, StartPressed, PauseToggled, RestartPressed, PlayerDied };

// 状态 → "本帧跑哪些系统"。**这是本模块存在的主要理由**：
// 把"暂停时世界真的停了"从"读 switch 时眼睛别漏行"变成一条**可离线断言的性质**。
// ⚠️ 表里只能说"跑不跑"，**不许**放"怎么跑"（那是 `game` 的接线）。
struct StatePolicy {
    bool simulate        = false;   // ⑤b AI / ⑤d 判定 / ⑥ 积分
    bool readMoveKeys    = false;   // ① 方向 / ② 归一化 / ③ 写速度 / ③b 朝向
    bool readFireKey     = false;   // ⑤c `FireBullet`
    bool advanceClock    = false;   // ⑤ `m_elapsed += dt`（HUD 的 sumdt 自证）
    bool advanceWave     = false;   // 全灭 → 倒计时 → 下一波
};
inline constexpr StatePolicy kStatePolicies[4] = { /* Title / Playing / Paused / GameOver */ };

// 越界（只可能由强制转换造出来）→ 返回**全 false**（最保守：什么都不跑，至少不会误伤世界）。
[[nodiscard]] constexpr StatePolicy PolicyOf(GameState s) noexcept;

// 转移：**纯函数，同输入同输出**。`None` 一律返回 `cur`（"这一帧不转移"）。
// ⚠️ 非法边必须返回 `cur`（例：`RestartPressed` 只在 `GameOver` 有效；`PlayerDied` 只在 `Playing` 有效）
//    —— 这条**要有用例逐边钉住**（同 `EnemyBrain` 的 18 条用例那个路子）。
[[nodiscard]] constexpr GameState NextState(GameState cur, StateEvent ev) noexcept;

} // namespace game
```

**转移表（✅ 2026-10-12 定稿；含附加项那条边；`—` = 非法边 → 返回原状态）**：

| 当前 \ 事件 | `StartPressed` | `PauseToggled` | `RestartPressed` | `PlayerDied` |
| --- | --- | --- | --- | --- |
| `Title` | → `Playing` | — | — | — |
| `Playing` | — | → `Paused` | — | → `GameOver` |
| `Paused` | — | → `Playing` | — | — ⚠️ 见下 |
| `GameOver` | — | **✅ → `Title`**（附加项，本人 2026-10-12 决定加） | → `Playing` | — |

⚠️ **`Paused` 下收到 `PlayerDied` 走 `—`**：暂停期间不该有伤害结算（`simulate = false` ⇒ 敌人在 ⑤b 就不跑），
所以这个事件在 `Paused` 下**不该被造出来**；返回 `cur` 是**兜底**（防御分支，用完例钉住，同 `B9` 那种"实现了的分支必须有用例"）。
✅ **`GameOver --PauseToggled--> Title` 已采纳**（附加项）：`Title` 不再只在进程启动时看得到一次，
且"重开"有了两种口味 —— `R` **立刻重来**（→ `Playing`）/ `Esc` **回开局提示**（→ `Title`，再 `Enter` 才开始）。
⚠️ 它复用同一个 `PauseToggled` 事件（`Esc` 在两个状态下的去向不同），所以这条边**必须**有一条用例单独钉住，
否则很容易被后来人"顺手统一成 `Paused ↔ Playing`"。

### 6.2 `game/include/game/Game.h`（公开面）

```cpp
// 当前状态（给 HUD、给用例）。⚠️ 与 PlayerHp/Kills 一样是**真接口**：`DrawTitle`/`DrawGameOver`
// 真的按它分支，不是"为测试而加"。
[[nodiscard]] GameState State() const noexcept;

// 当前波次（从 1 开始；0 = 还没开始）。HUD 会显示它。
[[nodiscard]] int Wave() const noexcept;

// 剩余弹药。HUD 会显示它。
[[nodiscard]] int Ammo() const noexcept;

// ⚠️ 从 **private 挪到 public**（同 S9 偏差 7 挪 kEnemyMaxHp 的同一条理由）：
//    用例要断"重开后 HP 回满"，那需要一个**期望值**；留在 private 就只能写死 10
//    —— 那就变成"调手感要改测试"（S5_CHECKLIST 10.4 的纪律）。
static constexpr int kPlayerMaxHp = 10;

// ── 弹药三常量（分叉 ④ 定稿：18 / 18 / 1）────────────────────────────────
// ⚠️ **三个都公开**：用例要拿它们算"打几发该空""击杀该返还几发""cap 在哪"，
//    写在 private 就只能写死数字（同 `kEnemyMaxHp` / `kMoveSpeed` 的纪律）。
// ⚠️ 手感关系式（改数值前先代一遍）：打死一个敌人要 kEnemyMaxHp/kDefaultBullet.damage 发，
//    每击杀返还 kAmmoPerKill ⇒ **净耗 = 3 − 1 = 2 发/敌人**；
//    意图 A（全中够用、失误才紧）要求 (3 − kAmmoPerKill) * kWaveMax < kStartAmmo，即 2*8=16 < 18 ✓。
static constexpr int kStartAmmo    = 18;   // 每波起手弹药
static constexpr int kMaxAmmo      = 18;   // = kStartAmmo（不许囤过初值）；配 static_assert(kMaxAmmo >= kStartAmmo)
static constexpr int kAmmoPerKill  = 1;    // 每击杀返还（N=1 ⇒ 净耗 2/敌人）

// ── 波次三常量（分叉 ③ 定稿：3 / 1 / 8）──────────────────────────────────
static constexpr int kWaveBase   = 3;   // 第 1 波的敌人数
static constexpr int kWaveGrowth = 1;   // 每波递增
static constexpr int kWaveMax    = 8;   // 封顶（**同时**是弹药张力旋钮）；配 static_assert(kWaveMax <= 巡逻表行数)
```

### 6.3 `game/include/game/Game.h`（私有新增 / 改动）

```cpp
// 重开（唯一入口）。⚠️ 它**清空实体表并重新 Spawn** ⇒ **必须在没有任何别的遍历的时刻调**（帧首 ⓪ 满足）。
// ⚠️ 用的是**既有公开接口**（遍历 Entities() + MarkForRemoval → 一次 FlushRemovals），**不重建 m_world**
//    ⇒ `m_nextId` 保持不变、id 跨局不复用（分叉 ② 取丙）。重置清单 = §4 那 6 步，顺序即依赖。
void StartNewRun();

// 刷第 n 波：数量 = min(kWaveBase + (n-1) * kWaveGrowth, kWaveMax)；位置取 kPatrolPaths 第 i 行。
// ⚠️ 内含"分叉 ④ 的每波回满"（m_ammo = kStartAmmo）。
void BeginWave(int wave);

// 波次推进（帧序 ⑤ 附近，**只在 `advanceWave` 时调**）：全灭 → 起倒计时 → 到点 `BeginWave(m_wave + 1)`。
// ⛔ 判据**不能**只有 `m_enemies.empty()`（它在"还没刷第一波"时也为真）—— 见 §7.3。
void TickWave(float dt);

// 渲染（三项新增；都是 const，与既有三个绘制函数同一个口径）
void DrawTitle(mini::render::FrameBuffer& fb) const;      // 开局提示
void DrawGameOver(mini::render::FrameBuffer& fb) const;   // 结算 + "R 重开 / Esc 回标题"
void DrawPausedBanner(mini::render::FrameBuffer& fb) const;
```

**新成员**：

```cpp
GameState m_state = GameState::Title;   // 初值 = 开局提示（PLAN 的"开局提示 → 按 Enter"）
int       m_wave  = 0;                  // 0 = 还没开始；Playing 时从 1 起
float     m_waveTimer = 0.0f;           // 本波清完之后的喘息倒计时（秒）；**0 = 未起倒计时**（哨兵）
int       m_ammo  = kStartAmmo;         // 分叉 ④：初值 18；每波回满、每击杀 +1、cap = kMaxAmmo
```

**改动**：`OnStart`（→ 只留"什么都不做"或建空世界，见前置事实 13）· `OnUpdate`（插 ⓪ + 按策略分派）·
`FireBullet`（加 Ammo 判定）· `DrawHud`（加列）· `OnRender`（按状态分支）。

**删除**：无（本阶段不退休任何键 —— 与 S9 删 `Key::K` 那步不同）。

---

## 7. 算法与伪代码

### 7.1 帧序（S10 版）

```text
⓪ 读"状态相关"的键：Enter / Escape / R（Q 仍在原处，属模拟期）
⓪b 造事件（优先级：**死亡 > 其他** —— 死亡那一帧的按键不算数）
    if m_playerHp <= 0 && m_state == Playing        → PlayerDied
    elif Enter == Pressed && m_state == Title       → StartPressed
    elif Escape == Pressed                          → PauseToggled（⚠️ 一个事件、三种去向：
                                                       Playing→Paused / Paused→Playing / GameOver→Title）
    elif R == Pressed && m_state == GameOver        → RestartPressed
    else                                            → None
① m_state = NextState(m_state, ev)
   if ev 是 StartPressed / RestartPressed → StartNewRun()      ← ★ 唯一入口
② p = PolicyOf(m_state)
③ 分派（**只有这几步是新的，其余原样**）：
   if p.readMoveKeys → ①方向 ②归一化 ③写玩家速度 ③b写朝向
   if p.advanceClock → ⑤ m_elapsed += dt
   if p.simulate     → ⑤b DriveEnemyAi
   if p.readFireKey  → ⑤c FireBullet（内含 Ammo 判定：0 不发、发则 −1）
   if p.simulate     → ⑤d ResolveBullets（预测线段，dt 仍在；**击杀处 +kAmmoPerKill 并夹到 kMaxAmmo**）
   if p.simulate     → ⑥ m_world.Update(dt)
   if p.advanceWave  → ⑤x TickWave(dt)                        ← 全灭判定在这里
   总是执行：④b F7 面板开关 · ④ Q · ⑦ 相机 Follow · ⑧ F4 快照 · ⑨ FlushRemovals · ⑨b 子弹表压缩
```

**⚠️ 三条顺序理由（别换）**：
1. **⓪ 在一切之前**（含 ①方向）："HP 归零后不再响应移动"这条验收**要求**死亡那一帧之后玩家不动 ——
   所以 `PlayerDied` 必须在"读方向键"之前生效。⇒ 代价是**响应最早也要等下一帧**（伤害在上一帧的 ⑤b 发生），
   这是可接受的（也是唯一的正确时机：本帧开始时他还活着）。
2. **`StartNewRun()` 必须在 ⓪① 之后、任何遍历之前**：它会清空实体表并重新 `Spawn`（前置事实 12）。
   帧首恰好没有任何遍历，**只有这里安全**。⚠️ 它**不是**"重建世界"—— `m_world` 本体与 `m_map` 都不动（分叉 ② 取丙）。
3. **④b/④/⑦/⑧/⑨/⑨b 在冻结状态下也跑**：
   · ⑨/⑨b 是"清理端"，停了会让上一帧标删的东西永远留着（虽然冻结时没新增，但**改动量是 0，风险也是 0**）；
   · ⑧ F4 是调试出口，暂停时**恰恰最需要**它；
   · ⑦ 相机用同一个位置 Follow 结果不变 ⇒ 无害；
   · ④b 面板开关同理（暂停时看敌人状态是正常需求）。

### 7.2 `StartNewRun`（重开）

见 §4 的 **6 步**清单。⚠️ 它是**幂等**的（连续调两次结果一样）。⚠️ **别在别的路径上调它**
（例如别在 `BeginWave` 里调）—— 它是 `m_nextId` 不变这条性质的**唯一**守卫点（分叉 ② 取丙）。
⚠️ 与分叉 ② 的措辞差别：**清空是它的第一步**（丙 = 用 `MarkForRemoval` + `FlushRemovals`），不是"另找一个手段"。

### 7.3 `BeginWave` / `TickWave`（分叉 ③ 取甲）

```text
BeginWave(n):
    m_wave = n
    count = min(kWaveBase + (n - 1) * kWaveGrowth, kWaveMax)
    for i in [0, count):
        在 kPatrolPaths[i][0] 处 Spawn 一个敌人（symbol 'E'，hp = kEnemyMaxHp）+ 一条 EnemyData
        （patrolPath = i，waypointIdx = 0，idleTimer = kDefaultTuning.idleSeconds）
    m_waveTimer = 0        // 本波没在倒计时
    m_ammo = kStartAmmo    // ★ 分叉 ④ 的"每波回满"（写在 BeginWave 里 ⇒ 重开那一波也会回满）

TickWave(dt):
    if m_enemies 非空           → m_waveTimer = 0; return      // 本波没清完
    if m_waveTimer == 0         → m_waveTimer = kWaveBreakSeconds; return   // 刚清完，起倒计时
    m_waveTimer -= dt
    if m_waveTimer <= 0         → BeginWave(m_wave + 1)
```

**⚠️ 边界（必须有用例）**：
· ⛔ **不能用 `m_enemies.empty()` 单独当"本波清完"的判据** —— 它在"**还没刷第一波**"时也为真 ⇒
  会立刻再刷一次（表现为"第一波刷两遍"）。**必须**配 `m_wave >= 1` 或用一个显式的"本波已刷"标志。
  **这是本阶段最容易写错的一处。**
· **`m_waveTimer` 用 `== 0` 当"未起倒计时"的哨兵**：⇒ `kWaveBreakSeconds` 必须是正数（配 `static_assert`），否则"起倒计时"和"到点"会撞在一起。
· 喘息期内玩家可以自由走动（`simulate = true`）—— 这是刻意的：喘息用来找位置，不是硬暂停。
· ✅ 取甲的常量（`kWaveBase/kWaveGrowth/kWaveMax = 3/1/8`）⇒ 第 n 波 `min(3 + (n-1), 8)`：`3,4,5,6,7,8,8,…`
  ⇒ 用例的期望值**必须**用这三个常量算（`WaveEnemyCountGrowsAndIsCapped`），不许写死 3 或 8。

### 7.4 Ammo：扣减 / 返还 / cap（分叉 ④ 定稿：乙+丁，N=1，cap=初值）

**生成端（`FireBullet`）**：

```text
if m_ammo <= 0        → return（不开火，不给"负弹药"）      ← ★ 判定在**最前面**
… 既有生成逻辑（位置 = 玩家位置、速度 = 朝向 × 弹速、symbol '*'）…
m_bullets.push_back(id)                                     // ← 先确认 Spawn 成功
m_ammo -= 1                                                 // ← 扣减放在**最后**
```

**返还端（`ResolveBullets` 里 `++m_kills` 的旁边）**：

```text
target.hp 跨过 0：
    ++m_kills
    m_ammo = min(m_ammo + kAmmoPerKill, kMaxAmmo)      // ★ 必须有 min(...) 夹取
    把 target.id 记进待删名单（既有逻辑）
```

· ⚠️ **顺序**：扣弹药必须**在 `Spawn` 成功之后**（否则 `Spawn` 抛异常时弹药白扣 —— 与 `World::Spawn` 刻意不标 `noexcept` 同一条道理）。
· ⚠️ **`min(..., kMaxAmmo)` 不是可选项**：`kMaxAmmo = kStartAmmo = 18`，而"每击杀 +1"在一波内会累积 ——
  没有夹取时前期弱波可以**慢慢磨着囤弹**，把后面的难度设计整个废掉（分叉 ④ 的第二个理由）。
· ⚠️ 返还点**必须**落在"跨过 0 那一次"（与 `++m_kills` 同一处），**不是**"打中一次 +1" ——
  否则一个敌人被打 3 次就返还 3 发，净耗变成 0。⇒ 这一条要有用例（`AmmoRefundHappensOnlyOnKill`）。
· ✅ 返还与"每波回满"并存：**波内靠击杀返还续命，波间靠回满重置** —— 这就是合取两项的全部意义。
· ⚠️ 在遍历里改 `m_ammo` 是安全的（它不是容器，没有迭代器失效问题）；把它放在 `++m_kills` 旁边只是为了"两件事同源"。

---

## 8. 接口决策表

| # | 决定 | 取法 | 代价 / 触发条件 |
| --- | --- | --- | --- |
| D1 | 状态机的住所 | ✅ 乙：`game/GameState.h` 纯函数 + 策略表 | 多 1 文件；退甲的条件见分叉 ① |
| D2 | 状态的表达 | `enum class GameState : uint8_t`，四态，初值 `Title` | 与 `EnemyState` 同一套路 |
| D3 | 状态转移的入口 | **只有一处**：`OnUpdate` 的 ⓪，且只经 `NextState` | 别在伤害代码里直接改 `m_state`（那会造出第二个真相） |
| D4 | 死亡事件的时机 | 帧首判 `m_playerHp <= 0` | 响应晚一帧；换来"死那一帧起就不动"+ 转移只有一处 |
| D5 | 重开的手段 | ✅ **丙**：`for(Entities()) MarkForRemoval` → **一次** `FlushRemovals`（不重建 `m_world`） | 引擎零改动 + `m_nextId` 不变；⚠️ 两个使用条件见分叉 ② |
| D6 | 重开的入口 | `StartNewRun()`，**唯一调用点** = ⓪① 的 StartPressed/RestartPressed | 加第二处调用点 = 把"id 不复用"的守卫点弄丢 |
| D7 | `Enter`/`Esc`/`R` | 复用 S2 已有的映射，**不动 `engine/`** | 若发现某键在本机被占（F3/F7 的前车之鉴），改的是 `Game.cpp` 顶部常量 |
| D8 | 暂停的实现 | game 侧不跑模拟那几步，**不加 `IGame::OnPause`** | 引擎仍每帧调 `OnUpdate`（"跳过 Update"在 game 内部完成） |
| D9 | 暂停期的时钟 | `m_elapsed` **一点也不加**；**不引入"积压 dt"通路** | 触发条件：若将来要做"暂停时也走动画"，那时**单独定**一个不受暂停影响的时钟 |
| D10 | 波次形态 | ✅ 甲：固定模板 + 数量递增 + **封顶**（`3/1/8`） | 要扩 `kPatrolPaths` 到 8 行；触发条件见分叉 ③ |
| D11 | 波次推进 | 全灭 + 喘息倒计时 → 下一波 | ⛔ 见 §7.3 的 `m_enemies.empty()` 坑 |
| D12 | Ammo | ✅ **乙+丁合并**：每波回满 **且** 每击杀 +`kAmmoPerKill`，`min(…, kMaxAmmo)` 夹取 | `18/18/1` ⇒ 净耗 2/敌人、意图 A（§分叉 ④） |
| D13 | 数量上限 | **S10 仍不加 cap** | 理由：只有全灭才刷下一波 ⇒ 敌人上界 = `kWaveMax`（`PLAN` A.6 ②）。⚠️ 触发条件改挂"与全灭脱钩的生成" |
| D14 | HUD 行数 | **仍是 2 行**（只加列） | 实测 ~67/80 列。⚠️ 若真要加行 → 按 `C24` 清单扫 5 处 |
| D15 | 推挤 C12 | **仍不做**（§到期复核） | 触发条件顺延；代价是"叠格"仍在 |
| D16 | `kPlayerMaxHp` | **private → public** | 同 S9 偏差 7 |
| D17 | **`GameOver --Esc--> Title`** | ✅ **加**（附加项） | 多 1 条边 + 1 条用例；⚠️ 与 `Paused ↔ Playing` 共用 `PauseToggled` 事件，必须单独钉住（§6.1） |
| D18 | 弹药三常量 / 波次三常量的可见性 | **全部 public `constexpr`** | 用例要拿它们算期望值（同 `kEnemyMaxHp` / `kMoveSpeed` / `kSpawnX` 的纪律） |

---

## 9. 坑表

| # | 坑 | 处理 |
| --- | --- | --- |
| 1 | ★ **跨 run 的 id 复用** —— ✅ **本阶段已用分叉 ② 取丙消除**：甲（`m_world = World{w,h}`）会把 `m_nextId` 重置回 1 ⇒ 上一局的 id 数字重新出现在新实体上 ⇒ 残留的 `m_player`/`m_bullets`/`m_enemies` 会**静默命中别人**（`Entity.h` 那句"永不复用"的契约只在**一个 World 的生命周期内**成立）。丙走"清空实体"⇒ `m_nextId` 单调递增 ⇒ **永不撞车** | 留档理由：**这是"契约的作用域"这条教训的现场**。⚠️ 若将来有人把 `StartNewRun` 改回"重建 `World`"，这个坑立刻回来 ⇒ 代码注释里要写明"**不许重建 `m_world`，见 `S10_CHECKLIST` 分叉 ②**" |
| 2 | **`Title` 只在进程启动时看得到一次**（若 `R` 直接回 `Playing`） | ✅ **已处置**：加了 `GameOver --Esc--> Title`（附加项 D17）⇒ `Title` 可重复到达。⚠️ 但这条边与 `Paused ↔ Playing` **共用 `PauseToggled` 事件** ⇒ 必须有用例单独钉住它（否则会被"顺手统一"掉） |
| 3 | **`StartNewRun()` 的调用时机**：它会**清空实体表并重新 `Spawn`** ⇒ `Entities()` 的 span 与 `Find()` 的指针都会失效；且它**自己**要遍历 `Entities()` 去登记删除 | 只允许在**帧首 ⓪**（此时无任何其他遍历）。⚠️ **不许**在 `DrawEntities` / `DriveEnemyAi` / `ResolveBullets` 里或它们之间调。⛔ 它内部的"登记"与"flush"必须**连贯**（先全部 `MarkForRemoval`、再**一次** `FlushRemovals`）—— 在循环里 flush 才是真的会失效 |
| 3b | ⚠️ **丙的前提：地图在整局之间不变**（丙不重建 `m_map`） | 当前成立（`BuildDemoMap` 确定性、整局中无人改地图）。⚠️ **触发条件**：将来地图可变时（可破坏的墙 / 从文件加载关卡）⇒ 重开必须重建地图，那时要么回到"乙 + 明确的 `Clear()` 语义"，要么在 `StartNewRun` 里重新 `BuildDemoMap`。**写进代码注释** |
| 4 | **`m_enemies.empty()` 在"还没刷第一波"时也为真** ⇒ 会立刻刷第二遍 | §7.3：配 `m_wave >= 1` 或显式标志 + 专门用例 |
| 5 | ⚠️ **"暂停时累积 dt"这条 PLAN 坑在本项目结构里不成立**（`Time` 每帧给独立的 `dt`，引擎不攒时间）—— 但**很容易被后人造出来**：谁去加一个"游戏时钟"（把 dt 累起来再一次性应用）谁就踩 | 写死规矩（D9）：暂停期间 `m_elapsed` 不加，**且不引入任何积压 dt 通路**。⚠️ 恢复那一帧用的就是**本帧的正常 dt**，没有任何"补帧" |
| 6 | **HP 归 0 那一帧敌人会立刻脱战**（`targetAlive = m_playerHp > 0` 是既有逻辑） | 这是**既有行为，不是 bug**：死亡帧起敌人不再贴脸打（否则玩家死后还在掉血）。⚠️ 用例别把它当异常 |
| 7 | **`kPatrolPaths` 表长与 `kWaveMax` 耦合**：`BeginWave` 按 `i` 索引它 ⇒ 数量封顶值**必须 ≤ 表长** | 配 `static_assert(kWaveMax <= kPatrolPathRows)`；扩表时位置要**人工核对落在空格上**（S8 的老规矩：柱阵只在 x/y 同为 10 的倍数处，地标楼在 x=30..36/y=5..8） |
| 8 | **扩展 `kPatrolPaths` 会碰既有用例的隐含前提**（★ `AGENTS.md` 第 7 节第三条，S7 因此真红过三次） | 动手前**先扫**：把"拿坐标 / 尺寸 / 帧数 / 敌人位置做期望值"的用例列出来（`grep -rn "kPatrol\|MinEnemyDistanceSq\|CollectEnemies\|EnemiesPatrol\|EnemyClosesIn\|WalkPlayerNextToEnemy" tests/`），逐条问"**它假设了几个敌人 / 它们在哪**"。⚠️ 已知的敏感项：`EnemiesPatrolUntilPlayerIsNear`、`EnemyClosesInWhenPlayerIsNear`、`AttackReducesPlayerHp`、`PlayerHpStopsAtZero`、`WalkPlayerNextToEnemy`（它按"最近的敌人"停） |
| 9 | **重开时"清到哪一层"**：`m_showAiPanel` / `m_dumpCount` / `m_quit` **不属于一局** | 建议**不清**（面板开关是玩家的观看偏好；`m_dumpCount` 是"本进程按了几次 F4"，清了会让日志编号重复）。⚠️ 但要在代码里**写清为什么不清** |
| 10 | **`Title` 下 `m_player` 是 0**（还没出生）⇒ 所有 `Find(m_player)` 都返回 `nullptr` | 既有代码**处处都判了 nullptr**（`DriveEnemyAi`/`FireBullet`/`DrawHud`/`DrawAiPanel`/相机/`DumpSnapshot`）⇒ 结构上安全；⚠️ 但 `Title` **不该画 HUD**（`DrawHud` 的 `player == nullptr` 早退恰好挡住，属于"巧合正确" —— 建议在 `OnRender` 里**显式**按状态分支，别依赖它） |
| 11 | **`Paused` 下按键仍被读**（`Input`/`InputState` 照常推进）⇒ `Q`（退出）、`F4`（快照）、`F7`（面板）在暂停时仍然生效 | 这是**刻意的**（暂停时更需要这些）。⚠️ 但"按住方向键然后按 `Esc` 恢复"会让玩家**立刻开始移动** —— 这是正常行为，不是 bug；用例别断"恢复后位置不变"（那要松开键再恢复） |
| 12 | **`Enter` / `R` 的边沿性**：必须用 `KeyState::Pressed`，用 `Held` 会每帧触发一次转移（`R` 会变成"每帧重开"，画面疯狂闪） | 与 `kDumpKey`/`kFireKey` 同一条纪律（`Pressed` 触发）。反向验证里要有这一条（同 S9 的 R4） |
| 13 | **HUD 加列的溢出**：行 2 从 44 → ~67 列 | 用例 `HudFitsInTwoLinesWithoutOverflow`（S9 已有）**必须同步扩到新字段**，否则它守的是旧的布局。⚠️ 这正是 `C24` 说的"安全地红"那条性质 |
| 14 | **"不泄漏"的判据要能被证伪** | 断言用"**重开 N 次后场上实体数 == 该有的数**"（可用 `F7` 面板的 `n=` + `CountChar(fb,'E')` 组合，⚠️ 但叠格时屏幕字符会少 —— 见 `C12`），**不要**用"内存没涨"当唯一判据（那个在用例里没法观察） |
| 15 | **`Ammo` 为 0 时的开火路径** | §7.4：判定在最前面 + 扣减在 `Spawn` 之后 ⇒ 不会出现"负弹药"、也不会"扣了弹药没出子弹"。要有一条用例直接钉住（打光之后再按 `SPACE`，屏幕上 `*` 数量不变） |
| 16 | **波次倒计时的哨兵值 `m_waveTimer == 0`** | 配 `static_assert(kWaveBreakSeconds > 0)`（同 `EnemyTuning` 那 6 条 `static_assert` 的路子）。⚠️ 别写成"`m_waveTimer < 0` 表示未开始" —— 那和"到点"共用同一个区间 |

---

## 10. 验收

### 10.1 用例清单（预计 +28 条 → 合计约 197）

**`GameState` suite（新文件 `tests/test_gamestate.cpp`，离线纯函数，13 条）**

| # | 用例 | 钉什么 |
| --- | --- | --- |
| 1 | `NextStateWalksEveryLegalEdge` | 表里每一个 `→` 都对（逐边，**共 6 条**：Title→Playing · Playing→Paused · Playing→GameOver · Paused→Playing · **GameOver→Title** · GameOver→Playing） |
| 2 | `IllegalEdgesAreNoOps` | `—` 的边全部返回原状态（**共 10 条**组合逐条，**包括 `Paused` 收到 `PlayerDied`**） |
| 3 | `NoneEventNeverTransitions` | 4 个状态 × `None` |
| 4 | `RestartOnlyFromGameOver` | `R` 只在 `GameOver` 有效 |
| 5 | `PlayerDiedOnlyFromPlaying` | 死亡事件不能从 `Title`/`Paused`/`GameOver` 触发 |
| 6 | `PauseTogglesBothWays` | `Playing ↔ Paused` 是一个自反事件 |
| 6b | ★ `EscapeFromGameOverReturnsToTitle` | **附加项那条边**：`GameOver` 收到 `PauseToggled` → `Title`（⚠️ 与 #6 共用同一个事件，所以必须**单独**钉住 —— 否则后来人很容易把它"顺手统一成 `Paused ↔ Playing`"） |
| 7 | `PolicyOfEveryStateIsTotal` | 4 个状态都能取到策略（无空洞） |
| 8 | `OnlyPlayingSimulates` | ★ `simulate` 只在 `Playing` 为真（"暂停世界真的停了"的**机器守卫**） |
| 9 | `FrozenStatesDoNotAdvanceClock` | `advanceClock` 只在 `Playing` 为真（`sumdt` 不会在暂停时偷涨） |
| 10 | `FrozenStatesDoNotReadMoveOrFireKeys` | 同上 |
| 11 | `OutOfRangeStateIsInert` | 越界 `GameState`（强转造出）→ 全 `false` |
| 12 | `NextStateIsPureFunctionOfInputs` | 同输入同输出 |

**`Game` suite（`tests/test_game.cpp`，+15 条；其中 1 条是**改写**）**

| # | 用例 | 钉什么 |
| --- | --- | --- |
| 1 | `StartsInTitleAndEnterBeginsPlay` | 开局是 `Title`；按 `Enter` → `Playing` 且第 1 波已刷（敌人 = `kWaveBase`） |
| 2 | `TitleDoesNotSimulate` | `Title` 下推 60 帧：世界不动、`sumdt` 不变（`State()` 也还是 `Title`） |
| 3 | `EscapePausesAndResumes` | `Esc` → `Paused`；再 `Esc` → `Playing`；★ **暂停期间推 60 帧 '@' 位置与 `sumdt` 都不变** |
| 4 | `PausedEnemyDoesNotMove` | 暂停期间敌人的屏幕位置不变（AI 真的停了，不只是玩家） |
| 5 | `PlayerHpReachesZeroThenGameOver` | HP 归 0 → 下一帧 `State() == GameOver` |
| 6 | `GameOverIgnoresMoveInput` | ★ **PLAN 的验收原话**：`GameOver` 下按住 `D` 推 60 帧，'@' 不动 |
| 7 | `GameOverFreezesWorld` | 同帧起敌人也不动 |
| 8 | ★ `RestartLeavesNoStaleState` | 重开后：`PlayerHp == kPlayerMaxHp`、`Kills == 0`、`Wave == 1`、`Ammo == kStartAmmo`、场上敌人 == `kWaveBase`、屏幕上 `*` == 0、**上一局的 id 全查不到** |
| 9 | `RestartThreeTimesDoesNotAccumulate` | ★ **PLAN 的验收原话**："循环 3 次" ⇒ 重开 3 次后实体数/子弹表长度与第 1 次**相同** |
| 10 | `WaveAdvancesWhenAllEnemiesDie` | 清完一波 → 喘息 → `Wave()` +1 且敌人重新出现（数量 = `kWaveBase + 1`） |
| 11 | `WaveEnemyCountGrowsAndIsCapped` | 第 n 波数量 = 用 `kWaveBase/kWaveGrowth/kWaveMax` **算出来**的值；到 `kWaveMax` 不再涨 |
| 12 | `AmmoDropsByOnePerShot` | ★ **对墙**连开 N 发：`Ammo() == kStartAmmo − N`（不依赖击杀返还） |
| 13 | `AmmoEmptyBlocksFiring` | ★ **对墙**打满 `kStartAmmo` 发后：`Ammo() == 0`，**第 `kStartAmmo+1` 发屏幕上不出新 `*`**；`Ammo()` 不为负 |
| 14 | `AmmoRefundHappensOnlyOnKill` | 打死**一个**敌人 ⇒ `Ammo()` 比"相同发数但没打死"多 `kAmmoPerKill`（⚠️ **不是**"打中一次 +1"：打 3 发打死一个只返还 1） |
| 15 | `AmmoIsCappedAtMax` | 弹药接近 `kMaxAmmo` 时击杀 ⇒ `Ammo() <= kMaxAmmo`（夹取守卫） |
| — | **改写**：`HudFitsInTwoLinesWithoutOverflow` | 扩到新字段（`ammo` / `wave` / 新键位提示）—— ⚠️ 不改它的话，它守的还是**旧布局** |
| — | **可能受 `kPatrolPaths` 扩容（5→8 行）与首波数量（5→3）影响的既有用例**（§坑表 8 那张清单） | 逐条核对隐含前提，**不红就别动**；⚠️ 首波**从 5 变 3** 这一点最可能碰既有期望值 |

### 10.2 正向验收 + 人眼

```powershell
cmake --version                       # 每台机器第一步：核版本（三台机三版本）
cmake --build build --config Debug
.\build\bin\Debug\mini_tests.exe      # 期望括号里 ~197、全绿
ctest --test-dir build -C Debug --output-on-failure
.\build\bin\Debug\MiniEngine.exe
```

| # | 人眼看什么 |
| --- | --- |
| 1 | 开局是**提示画面**（不是直接进游戏）；按 `Enter` 才开始 |
| 2 | 开局 **3 个** `E` 在巡逻（`kWaveBase`）；HUD 第 2 行有 `hp / ammo / kills / wave / dir`，**不折行不溢出** |
| 3 | 按 `Esc` → 出现暂停横幅；**画面完全冻住**（敌人不动、`sumdt` 不变）；再按 `Esc` 恢复 |
| 4 | 站着不动让敌人贴脸打 → `hp` 数字往下掉；掉到 0 → 出现 `GameOver` 画面 |
| 5 | `GameOver` 下按 `D`：`@` **不动**（PLAN 的验收原话） |
| 6 | 按 `R` → **立刻从头开始**：`hp` 回满、`kills` 归 0、`wave` 回 1、敌人重新出现在初始位置 |
| 7 | **对墙**打光弹药 → 打不出 `*`；打中敌人（打空枪不算）时 `ammo` **会 +1 回来** |
| 8 | 清完一波 → 下一波敌人**变多**（3 → 4 → 5 …封顶 8），且 `ammo` **回满到 18** |
| 9 | **连续重开 3 次**：不崩、不卡；任务管理器里内存**稳定**（对照 S9 的第 6 条） |
| 10 | `GameOver` 下按 `Esc` → **回到开局提示**；再按 `Enter` 又能开始（附加项那条边） |
| 11 | `F4` 快照仍工作（回归）；暂停时按 `F4` 也写得进去 |
| 12 | `F7` 敌人面板仍工作（回归）；面板的 `n=` 与新波次的敌人数一致 |

⚠️ 本机键盘 ghosting：**"边跑边打"是 2 键**，验不了 → 动线仍是"跑 → 松手 → 打"（S9 已记录）。

### 10.3 反向验证（**每条都必须看到红，跑完必须还原 + 重建**）

| # | 破坏方式 | 应变红的用例 |
| --- | --- | --- |
| R1 | `kStatePolicies` 里把 `Paused` 的 `simulate` 改成 `true` | `GameState.OnlyPlayingSimulates` + `Game.EscapePausesAndResumes` + `Game.PausedEnemyDoesNotMove`（★ 两条层次不同：一条纯函数、一条集成） |
| R2 | 把 `NextState` 的 `RestartPressed` 分支放行到 `Playing`/`Paused` | `GameState.RestartOnlyFromGameOver` |
| R2b | 把 `GameOver` 的 `PauseToggled` 分支**删掉**（把 `Esc` 统一成 `Paused ↔ Playing` 那一条） | `GameState.EscapeFromGameOverReturnsToTitle`（★ 这正是附加项那条边"最容易被顺手改掉"的形态） |
| R3 | `StartNewRun` 里**漏清 `m_bullets`**（第 3 步只做一半） | `Game.RestartLeavesNoStaleState`（★ 这条 R 是**专门为"清空清单"设计的**：丙让 id 不撞车，所以漏清**不会**立刻出乱子 —— 正好用来说明"为什么清单再简单也要有用例守"） |
| R4 | `StartNewRun` 里**漏重置 `m_wave`**（或漏 `m_kills` / `m_ammo`） | `Game.RestartThreeTimesDoesNotAccumulate` / `RestartLeavesNoStaleState`（后者的 `Ammo == kStartAmmo` 那条会抓 `m_ammo`） |
| R5 | `TickWave` 去掉 `m_wave >= 1` 那个守卫（只留 `m_enemies.empty()`） | `Game.WaveAdvancesWhenAllEnemiesDie`（表现为"第一波刷两遍"）—— ⚠️ **若这条不红，说明用例的起点假设了"敌人已经存在"**，需如实登记 |
| R6 | `FireBullet` 里把 Ammo 判定挪到 `Spawn` **之后** | `Game.AmmoDropsByOnePerShot` / `AmmoEmptyBlocksFiring`（打光后仍出子弹、且弹药会变负） |
| R6b | `ResolveBullets` 里去掉 `min(..., kMaxAmmo)`（退回裸 `+=`） | `Game.AmmoIsCappedAtMax` —— ⚠️ **预判可能不红**：要红得先让弹药涨到 cap（需要"连杀到满"的场景）⇒ 若真不红，**如实登记**，并把用例改成"先把弹药打到接近满再击杀"的构造 |
| R6c | 把返还点从"跨过 0 那一次"改成"打中一次 +1" | `Game.AmmoRefundHappensOnlyOnKill` |
| R7 | 把 `Enter`/`R` 的 `Pressed` 改成 `Held` | `Game.StartsInTitleAndEnterBeginsPlay`（+ 人眼：画面疯狂闪） |
| R8 | `OnUpdate` 里把 ⓪ 挪到"读方向键"**之后** | `Game.GameOverIgnoresMoveInput`（死亡后还会动一帧）—— ⚠️ **预判可能不红**（只差一帧，若用例断的是"60 帧后不动"就看不出来）⇒ 若真不红，**如实登记**并考虑把用例改成"死亡那一帧就不动" |
| R9 | 把 `StartNewRun` 的清空拆坏：**在 `Entities()` 循环里调 `FlushRemovals`**（而不是循环外一次） | ⚠️ **预判会崩或行为诡异**（span 失效 = UB），**不一定会红**。⇒ 若实测"能跑且全绿"，如实登记为"**这条只有注释 + 契约在守**"（同 S9 的 R6 那种"绿出来的欠账"） |

> ⚠️ **R8 / R9 / R6b 三条我都预判可能不红** —— 记录在此，跑完按实际结果回填（同 S9 的 R6 那种"绿出来的欠账"）。
> ⚠️ R9 属于**UB**（不是逻辑错），跑之前先想清楚"它崩了也不算意外"；**别把它当功能验证用**。

### 10.4 交付后的自检

| # | 自检项 | 怎么查 |
| --- | --- | --- |
| 1 | `engine/` **零改动**（①②③④ 都已定稿 ⇒ 这是**预期**，不是"如果"） | `git diff --stat -- engine/` 应为空。⚠️ 唯一例外：某键在本机被别的软件占用 —— 那时也**只改 `Game.cpp` 顶部的常量**（F3/F7 的前车之鉴） |
| 2 | `GameState.h` 不认识 game/engine | `grep #include game/include/game/GameState.h` 只有 `<cstdint>` |
| 3 | 用例总数 | `mini_tests.exe Nope` 括号里的数 = 预期值（~197） |
| 4 | **既有 `Game` 用例只因"巡逻表扩容 + 首波 5→3"而改的那几处** | `git diff tests/test_game.cpp` 逐条看（§坑表 8 的清单） |
| 5 | HUD 两行都 < 80 列 | 数格式串（行 1 = 58、行 2 ≈ 67） |
| 6 | **`StartNewRun` 里没有"重建 `m_world`"** | `grep -n "m_world *=" game/src/Game.cpp` 应为 **0 处**（1 号坑的守卫；改回重建等于把静默指错请回来） |
| 7 | **收尾抄录** | 干净构建跑一次，把尾行**照抄**进本表下方（S8/S9 的教训复现过三次） |

> **收尾实测（本人抄录）**：⬜ 待填

---

## 11. 需要同步的文档

| # | 文档 | 改什么 |
| --- | --- | --- |
| 1 | `doc/MiniEngine_PLAN.md` | S10 小节**整节重写并标 ✅**（对齐 S8/S9 那种"实际落地"格式：实现 / C++ 知识 / 引擎知识 / 规模 / 可运行效果 / 验收 / 坑 / 设计结论）· `A.2` 目录树加 `game/GameState.h` · `G` 节 v0.3 的 **③ 标 ✅** · **`H.1` 保底路径标注"已达成"**（v0.3 交付）· `A.6` 的"待办去向"里 `C19` 那行按本阶段的结论改写 |
| 2 | `doc/TECH_DEBT.md` | 按 §到期复核 逐条落：`C12` 触发条件顺延 · `C19` 触发条件改挂"与全灭脱钩的生成" · `C22`/`C23` 触发条件不变（写明"S10 复核过"）· `C24` 写明"本阶段是加列不是加行" · **新增 4 项**：① **丙的前提 —— "地图在整局之间不变"**（触发条件：地图可变时 = 可破坏的墙 / 从文件加载关卡）② **"不许重建 `m_world`"目前只有注释 + 一条 `grep` 自检在守**（若反向验证 R9 不红 ⇒ 如实登记"**没有机器守卫**"，同 S9 的 C23）③ **弹药 / 波次两组数值是手感参数**（`18/18/1` 与 `3/1/8`，附那条 `(C−N)×kWaveMax ⋚ kStartAmmo` 的不等式；触发条件：真上手玩过之后觉得不对时重调）④ **`GameOver --Esc--> Title` 与 `Paused ↔ Playing` 共用一个事件**（若 R2b 不红 ⇒ 登记"这条边可能被顺手改掉"，同 C17 的味）· 加 **S10 进度段**（用例数 169 → ~197、验收状态、"`N passed` 逐条抄录"） |
| 3 | `doc/S10_CHECKLIST.md` | 本文件：分叉拍板结果回填 + 进度板打勾 + 落地偏差追加 + §10.2/10.3/10.4 回填 |
| 4 | `doc/S9_CHECKLIST.md` / `S8_CHECKLIST.md` | **不改**（历史留档）。⚠️ S9 §10.1 里"留给 S10 的"那两条（偏差 13 的 `G3`/`G6` 断言、`B9` 的墙优先顺序）**仍不在本阶段范围内** —— 要在本文件里**明确交代"不做，仍带着走"**，别让它悄悄消失 |
| 5 | `AGENTS.md` | §2 文件表补一行 `doc/S10_CHECKLIST.md`（§7 三条纪律不动） |
| 6 | `README` / 新文档 | **不创建**（`S14` 才做。⚠️ 但 v0.3 达成后 `H.1` 说"标准/完整路径"是加分项 —— 那是路线图的结论，不是本阶段要写文档） |

⚠️ 另外两笔**既有欠账**（收口时若仍存在，按 `AGENTS.md` 1.3"只改点到的那部分"决定要不要顺手改）：
1. `PLAN` `A.2` 的 `tests/` 清单缺 `test_enemy.cpp`（S8）与 `test_bullet.cpp`（S9）—— 本阶段会**再加** `test_gamestate.cpp`；
2. `S9_CHECKLIST` §10.4 的"收尾实测"表**归本人填**。

---

## 12. 与 `PLAN` 原文的差异（写方案时就已知，**不是**落地偏差）

| # | `PLAN` S10 原文 | 本阶段实际 | 为什么 |
| --- | --- | --- | --- |
| 1 | "`GameState` 枚举状态机，`switch` 分派 Update/Render" | 分叉 ① **取乙**：纯函数 + 策略表（`switch` 退化成"按表分派"） | 真内容是"4 状态 × 11 帧步"的矩阵，不是 4 个状态本身；把矩阵做成表才能**离线断言**"暂停时世界真的停了" |
| 2 | "`WaveManager` 按波次刷怪并递增难度" | **不建 `WaveManager` 类型**，落在 `Game` 的 `BeginWave`/`TickWave` 两个私有函数 + `m_wave`/`m_waveTimer` 两个成员 | 与 `PLAN` 里 `Projectile` 被 S9 砍掉的**同一条理由**（结构被复杂度逼出来，而不是提前建）：波次现在只有"数量 + 位置表"两件事，一个类包不住任何东西 |
| 3 | "HUD 文本布局（左对齐 + 数值右对齐）" | 沿用现有的 `std::snprintf` + 定宽说明符（`%2d` / `%6.2f`） | 与 S3 起就用的手法一致（`S3_CHECKLIST` 的"定宽字段让数字跳动时位置不抖"）；`std::format` 是 C++20，本项目 C++ 标准未升级（不引入） |
| 4 | "常量配置抽到 `Config`" | **不建 `Config`**：新增的调参常量留在 `Game.h` 的 public 区（与 `kMoveSpeed`/`kSpawnX` 同处）或 `GameState.h` | 同上面第 2 条；而且"调参常量集中在一个地方"这件事现在已经由 `Game.h` 的 public 区承担了（用例要靠它们算期望值） |
| 5 | "暂停即'跳过 Update，只做 Render'" | 一致，但**粒度更细**：不是整块跳过 `OnUpdate`，而是按 `StatePolicy` 逐项分派（清理/调试那几步仍然跑） | 整块跳过会让 ⑨ `FlushRemovals` 与 ⑧ `F4` 快照一起停 ⇒ 前者留下未清理的标记、后者让"暂停时更需要调试"这个场景失灵 |
| 6 | 坑："暂停时仍累积 `dt`，恢复瞬间角色瞬移" | **本项目的结构让这条坑不成立**（`Time` 每帧独立给 `dt`，引擎不攒时间）⇒ 改成一条**防御性规矩**（D9）写进代码注释 | 见 §0 事实 8 与 §坑表 5；不成立不等于不用管 —— 它很容易被"将来加个游戏时钟"重新造出来 |
| 7 | `Ammo` 只出现在 HUD 与目标里，"怎么补充"没写 | 分叉 ④ **合并了两种补充方式**（每波回满 **且** 每击杀 +1，带 `cap`） | "有限 + 无处补充" = 死局（与 v0.3 的锚点冲突）；而单用"每击杀 +N"在 N≥3 时**弹药无界增长**（又一个 `C19`）⇒ 合取 + cap 才是自洽的一组 |
| 8 | "验收：循环 3 次不泄漏" | 一致；把"不泄漏"落成**可证伪的判据**（重开 N 次后实体数/子弹表长度不涨） | "不泄漏"如果只靠"内存没涨"，用例里观察不到 ⇒ 会变成一句没人能验的话 |
| 9 | （没写）重开时"世界怎么清" | **用既有原语清空**（`MarkForRemoval` + `FlushRemovals`），**不重建 `World`** | 重建 `World` 会重置 id 计数器 ⇒ 跨局 id 复用（`PLAN` A.6 ⑧ 的禁令只说了"不许回收 id 再复用"，**没说"重置整个容器也不行"** —— 这是同一件事的另一种形态，本阶段补上） |

---

## 13. 学习点（写给自己）

- **状态机的价值在"矩阵"不在"转移"**：4 个状态里真正会出错的不是"`Esc` 是不是从 `Playing` 出发"，
  而是"我在 `GameOver` 里漏停了哪一步"。把矩阵做成表 ⇒ 它从"读代码"变成"断言"。
- **"清空"是有代价的动作**：`m_world = World{}` 看起来最干净，却**悄悄把 id 计数器也清了** ——
  一个"重置"动作顺带破坏了一条你以为与它无关的契约（"id 永不复用"只在**一个 World 的生命周期内**成立）。
  这是**第二次**遇到"契约的作用域比想象的小"（第一次是 S7 点模型下"对角缝能穿"）。
  ✅ **这次的处置值得记下来**：没有在"便宜但有坑（甲）"和"安全但要改引擎（乙）"之间硬选，
  而是回去把 `World` 的实现读了一遍，**找到了第三条路（丙）—— 用既有原语达成目的**。
  ⇒ 教训：**"二选一"往往是"还没读完代码"**。
- **失败模式的"响亮程度"是选型标准**：甲漏一行 ⇒ 子弹被当成敌人驱动（**静默**）；丙漏一行 ⇒ 波次卡住、面板印 `<id gone>`（**看得见**）。
  同样都要靠纪律，选那个"错了会喊"的。
- **"不泄漏"必须能被证伪**：说得出"什么数字不该涨"，才是一条验收；说不出来就只是愿望。
- **不做 ammo 补充 = 造死局**：HUD 上多一个字段**同时**是一份设计承诺 —— 字段存在就有人会打光它。
  加字段之前先问"它到 0 之后发生什么"。
- **一条设计承诺要同时给出"上界"**："每击杀返还弹药"听起来只是手感，但 `N ≥ C` 时它让弹药**无界**（`C19` 的又一个入口）。
  ⇒ 任何"会增长的量"都要配一句"它凭什么有界"（这次靠 `min(..., kMaxAmmo)`）。
- **数值手感可以用一条不等式表达**：`(C − N) × kWaveMax ⋚ kStartAmmo` 就是"友好 / 硬核"的分界。
  把意图写成不等式 ⇒ 调参时**知道自己在调哪个方向**，而不是凭感觉试。
- **触发条件必须当真**：`C12` 的触发条件写着"S10 刷怪"，所以 S10 **必须给结论**（哪怕是"仍不做 + 顺延"），
  不能让它在文档里过期。
