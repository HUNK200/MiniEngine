# S9 施工图 —— Weapon / Projectile（可以射击）

> 本文件是 S9 的**施工图 + 进度板 + 落地记录**，格式对齐 `doc/S8_CHECKLIST.md`。
> ⚠️ 与 `AGENTS.md` 冲突时**以 `AGENTS.md` 为准**。

---

## 当前状态

| 项 | 状态 |
| --- | --- |
| 本文件 | ✅ **2026-10-10 定稿**（4 个分叉已由本人拍板） |
| 代码 | **步骤 1 / 2 / 3 / 4 已验证**（152 / 160 / **167** / **168** 绿；R1、R3′ 已做）；**步骤 5（HUD 两行）已写完、待构建** → 期望 **169 绿**。⚠️ 工作区**尚未提交**（步骤 3 收尾 + 步骤 4 + 步骤 5 都在里面） |
| 上一阶段 | S8 已全部收口并推送（`ORIGIN/main` = `76eb583`），用例 **146** |

## 分工约定（沿用 `AGENTS.md` 1.4，S8 已跑通一遍）

| 谁 | 做什么 |
| --- | --- |
| **AI** | **只编辑 / 新建源文件**。不跑 `cmake`、不跑 `ctest`、不跑 `MiniEngine.exe` |
| **本人** | 构建、跑用例、跑 exe、人眼观察，把结果回报 |

每步报告必须含：**改了哪些文件** · **原样可粘贴的 PowerShell 命令** · **期望看到什么（数字 / 现象）** · **当前状态**（代码已写完，待构建 / 待运行 / 待肉眼验） · **⑤ 本次改动"有什么用"** · **⑥ 目前整体进度**（阶段 / 用例数 / 工作区状态）。
⚠️ **不许写"构建通过 / 已实测"**，除非那个结果确实由本人回报（`AGENTS.md` 1.4）。

⚠️ **⑤⑥ 是本人 2026-10-10 追加的默认项**（原话："报告添加需求，需要总结一下目前做了什么，有什么用"）——
   目的：每份报告都能一眼看出"现在到哪了、这一步值不值"，不用回头翻 diff。
   ⚠️ **"有什么用"不许写成卖点**：要写**它挡住了哪个具体的失败**（或挡住了哪个还没发生的失败），
   写不出具体失败就照实说"这一步只是把某件事写进结构，没有直接可见的效果"。

---

## 分叉定稿（2026-10-10，本人拍板）

### 分叉 ① 子弹的数据放哪 → **中 1：纯 `Entity` + id 成员表**（叠加**中 3**：纯函数进 `game/Bullet.h`）

**关键发现**：一颗子弹除 `Entity` 已有的字段外，**额外字段是 0 个**（伤害是常量、没有 owner 逻辑、方向就在 `velocity` 里、不需要 TTL）。

| | |
| --- | --- |
| **好处** | ① 位置/速度**只有一份真相**（不复制数据）；② 积分、绘制、删除**全部白拿**（`World::Update` / `DrawEntities` / `MarkForRemoval` + `FlushRemovals`）；③ "这是子弹"的身份是**显式的**（在表里），不靠 `symbol` 兼职类型标记；④ ⚠️ **"遍历 `m_entities` 时 `Spawn` → 迭代器失效"这条 PLAN 点名的坑在架构上消失** —— `ResolveBullets` 遍历的是 `m_bullets`，不是 `m_entities` |
| **坏处（代价）** | ① 多一个容器 → "成员关系"可能漂移（表里有过期 id）；**但它自愈**（帧末 `erase_if(Find(id) == nullptr)`），且失败表现是"多发一次 `Find`"，**不是"指错实体"**；② 比"纯 Entity"多 4 行；③ **没有**解决"将来子弹要带属性"（那时把 `EntityId` 换成 `BulletData` 即可，**生长点现成**） |
| **为什么不选 (b) 独立的 `BulletData` 容器** | (b) 是"**两份必须同步的真相**"（位置/速度各一份），而 S9 恰好是**真删第一次发生**的地方 → 要写同步删除 + 专门用例，**只为了让一个空结构体活着**。中 1 是"**一份可自愈的缓存**"，不是两份真相 |
| **触发条件（什么时候该升级）** | **S10 加第二种子弹 / 多武器**（每发要携带自己的伤害）→ `std::vector<EntityId>` 换成 `std::vector<BulletData>`（同形状，一次机械替换） |

### 分叉 ② 命中判定 → **(b) 沿线段等距采样**，住在 **`engine/include/mini/world/Collision.h`**，暴露**采样格点**

**决定性事实**：`kMaxDelta = 0.05f`（`Time.h:31`）⇒ 45 格/秒的子弹**一帧最多飞 2.25 格** ⇒ 点判定要成立就得让**子弹比玩家（20 格/秒）还慢**。

| | |
| --- | --- |
| **好处** | ① 步长 0.5 格 ⇒ 相邻采样点的 `floor` 最多差 1 ⇒ **任何 1 格宽的目标都躲不掉**；② **纯函数、可离线单测**（同 `Collision.h` 的三兄弟）；③ 撞墙与敌人命中**共用一个采样器**；④ 与 `MoveAndCollide` **共用同一个 `kMaxStepPerTick`** ⇒ "位移"和"判定"走在**同一套格点**上 |
| **坏处（代价）** | engine +约 25 行（2 个纯函数 + 1 条 `static_assert`）；每发子弹每帧多几次 `IsSolidAt`（飞 0.75 格 → 2 个采样点，可忽略） |
| **不取"中 X：采样器放 `game/Bullet.h`"** | 那要用一个**跨层约定**（步长必须一致）去换"engine 零改动"这个**审美收益**；放一起才能用 `static_assert` 钉住。**S9 反正要动 `KeyCode.h`（删 `Key::K`）**，engine 不是零改动也没关系 |
| **不取 DDA（Amanatides-Woo）** | 它往"更精确"那一轴走（不是中间项）：约 40 行 + 一堆退化分支，而且**是"第二套碰撞实现"** —— 项目从 S7 起守着"**位移只有一条路径**"。收益（省几次 `IsSolidAt`）在 5 发子弹的量级下看不见 |
| **接口粒度** | 暴露**采样格点**（`SweepStepCount` / `SweepSampleAt`），**不**暴露"是否撞墙"这种成品判断 —— 两个调用方（墙 / 敌人）的命中条件不同，采样器只负责"把线段摊成格点"这件中立的事 |

### 分叉 ③ 朝向 / 瞄准 → **(a) 移动即朝向（停下沿用）**，朝向用 **8 向枚举 `Facing8`**，**`SPACE` 开火**

**两条前置事实**：① `Game.cpp:367-370` 里**方向键已经是移动的别名** ⇒ "箭头键独立瞄准"这条选项**失效**（改它等于撤掉 S7 为键盘 ghosting 加的替代操作）；② 本机 **2 键同按不可靠**（`S7_CHECKLIST` 记录"D+S 同按只出一个字母"）。

| | |
| --- | --- |
| **好处** | ① **0 个新键**；② 0 个新模式（不像"锁定/旋转"要额外交互）；③ 8 向免费（斜向移动 → 斜向射击） |
| **坏处（代价）** | ① ⚠️ **不能边退边打** —— 想打右边就必须往右走（贴近敌人）；② "停下沿用朝向"这条规则**必须被看见**，否则玩家不知道站着会朝哪打 ⇒ **HUD 必须有朝向指示（`dir ->`）** ← 这是本分叉的**必要配套**，不是可选项；③ 多一个状态（`m_facing`） |
| **⚠️ 关于"2 键同按"** | 这条约束**也打在 (a) 身上**："边跑边打"本身就是 2 键（WASD + `SPACE`）⇒ 实际动线是"**跑 → 松手 → 打**"。本机验不了"移动中开火" |
| **为什么朝向用枚举** | ① 用例断言 `Facing() == Facing8::East` **不用比浮点**；② HUD 可以直接查表画字形；③ 与 S8 用 `enum class EnemyState` 同一套路（**状态用枚举表达更难写错**） |
| **触发条件（什么时候买"边退边打"）** | **换键盘之后**，或 S10 做波次时真的需要 → 加一个 `E` 键做"锁定/解锁朝向"（`E` 是**空闲键**，0 调用者；默认行为与 (a) 完全一致，可逆） |

### 分叉 ④ HUD → **占行 + 两行 HUD**（`kHudRow` 1 → 2）；朝向指示用 **2 字符 ASCII 表**

**实测数字**：现状 **78/80 列**；只加 `kills` 会到 **89 列（溢出 9）**；只砍 `sumdt` 后**加朝向指示又溢出到 86**。

| | |
| --- | --- |
| **好处** | ① 两行**共 160 列**预算 → 现有字段**一个不用砍**（`pos`/`tile`/`sumdt`/`v`/`hp` 全留）；② 可以按用途分组（行 1 位置类 / 行 2 状态类）；③ 顺手解掉 **S8 偏差 6**（`F7` 一直进不了提示行）；④ **S10 的 `HP/ammo/kills/wave` 实测只要 64 列** ⇒ 一行绰绰有余，**将来不用再砍任何东西** |
| **坏处（代价）** | ① 世界**可见区少 1 行**（这 1 行本来就被文字盖着，等于是"少一行可玩区"）；② `Game.PlayerIsDrawnAtItsCell` 的 **1 个表达式**（`startRow`，另两条断言都从它派生）；③ `CollectEnemies` / `MinEnemyDistanceSq` 的扫描起点 `y >= 2` → `>= 3`（**2 处**） |
| **⚠️ 为什么必须"占行"而不是"叠加"** | 叠加（视口不变、多画一行盖住）虽然 **0 处用例改动**，但会让**玩家消失**：`PlayerStopsAtTopWall` 证明玩家最小 `y = 1.5`（tile 行 1，行 0 是墙），叠加时那一格落在**屏幕第 2 行 = 正好被第二行 HUD 盖住**。占行时相机知道"多出来的那行不可用"，玩家落在第 3 行 ⇒ 安全 |
| **朝向指示字形（方案 A，2 字符定宽）** | 东 `->` · 西 `<-` · 北 `^^` · 南 `vv` · 东北 `/>` · 西北 `\<` · 东南 `\>` · 西南 `</`<br>⚠️ **全 ASCII**（代码页 936，S8 的坑表记过 `→` 这类会乱码）；**定宽 2 列** ⇒ 后面的列不会左右跳。它是一张 `const char* [8]` 表，**人眼验收时可现场换** |
| **HUD 行布局** | 行 1（位置类，34 列）：`pos   2.50,  2.50 \| tile    2,   2`<br>行 2（状态类，44 列）：`hp 10 \| kills  3 \| dir -> \| WASD SPC Q F4 F7` |

### 附带定稿（不在四个分叉里，但一起定了）

| 项 | 取法 | 理由 |
| --- | --- | --- |
| **开火节奏** | `KeyState::Pressed` **边沿触发**（按一下一发），**不做**连发/冷却 | 0 状态、0 计时器；天然"每帧最多 1 发" → 顺手消灭"遍历中 `Spawn`"的另一半风险；PLAN 的验收正是"连按 `SPACE` 100 次不崩"。连发是 S10 的手感活（那时有 `Ammo` 兜着） |
| **帧序落点** | `FireBullet()` 与 `ResolveBullets()` **都在 `World::Update` 之前**，用**预测线段** `[pos, pos + velocity × dt]` | ⚠️ 若把判定挪到 `Update` **之后**，就需要"上一帧位置"，而"用 `pos_after − v·dt` 反推起点"在**子弹被墙夹住时是错的**（反推的起点会在墙里 ⇒ 线段穿墙 ⇒ **打到墙后的敌人 = 假命中**）。放前面就只需要预测线段，且与 S8 的"AI 读本帧积分前的位置"是同一个口径 |
| **子弹速度** | `kBulletSpeed = 45.0f`（2.25 × 玩家），**公开 `constexpr`** | 用例要用它算期望值（S8 的纪律：不写死数字） |
| **子弹伤害** | `kBulletDamage = 1`，敌人 `kEnemyMaxHp = 3` ⇒ **三发打死一个** | 用例可以"用两个公开常量算出三发" |

---

## 进度板

| 步骤 | 内容 | 状态 |
| --- | --- | --- |
| 0 | 需求 + 4 个分叉 + 本施工图 | ✅ **2026-10-10 完成** |
| 1 | `world/Collision.h` 加采样器（2 个纯函数）+ `Sweep` suite（6 用例） | ✅ **2026-10-10 完成**：本人回报 `152 passed, 0 failed`（146 → +6）；**R1 反向验证已做，红集与预判逐行吻合**（见 §10.3.1）；2 条落地偏差见上表 |
| 2 | `game/Bullet.h`（`Facing8` + 命中判定纯函数 + 常量）+ `Bullet` suite（8 用例） | ✅ **2026-10-10 完成**：本人回报 `160 passed, 0 failed`（152 → +8）；偏差 3/4 见上表；⚠️ R3 原方案被推演否掉并改写（见 §10.3.2），顺带发现用例缺口 **B9**（⬜ 待补） |
| 3 | 接线：`FireBullet` / `ResolveBullets` / `m_bullets` / **`Key::K` 退休** / 改写 `EnemyDiesAndDisappears` | ✅ **2026-10-10 完成**：`Key::K` 从枚举+映射+用例删净；新增 7 条 `Game` 用例（160 → **167**）。<br>过程：第一次实测 **166 passed, 1 failed**（`Game.BulletHitsEnemyEvenAtMaxDelta`，`lhs=3 rhs=4`）→ 定案为**用例判据的假前提**（偏差 9，本人跑 exe + `F7` 人眼确认了"两只敌人叠在同一格"）→ 判据改写成"与面板真值比"；同时修掉 `Key::K` 漏扫的第三处（偏差 10）。<br>**本人回报最终 `167 passed, 0 failed`** |
| 4 | **真删**（`TECH_DEBT` C16 到期）+ `KilledEnemyIsRemovedAndLeavesNoDanglingId` | ✅ **2026-10-10 完成**：`ResolveBullets` 收"待删 id 名单"（循环内）→ MarkForRemoval + `erase_if(m_enemies)`（循环外，两边一起删）；`PanelSummary` 的 `deadCount` 换成表头的 `enemyCount` + 新增 `hasIdGone`；新用例 `Game.KilledEnemyIsRemovedAndLeavesNoDanglingId`（167 → **168**）。**本人回报 `168 passed, 0 failed`**。<br>⚠️ 反向验证 **R5 / R5′** 与人眼 1 条**还没做**（排进步骤 7）|
| 5 | HUD 两行（`kHudRow` 1→2 + `kills` + `dir` + 提示加 `F7`）+ 修 `PlayerIsDrawnAtItsCell` | ⚠️ **代码已写完，未构建/未运行**（2026-10-10 家机）：`kHudRow` 1→2 并**顺带把语义在注释里写准**（它是"HUD 占的行数"= "世界视口第一行"）；`DrawHud` 拆两行（行 1 位置类 / 行 2 状态类）；涟漪三处（用例的 `kTestHudRow` 镜像 + 两个扫描起点）+ 新用例 `Game.HudFitsInTwoLinesWithoutOverflow`（168 → **169**）<br>👉 **待构建**：`cmake --build build --config Debug` + `mini_tests.exe`（期望 **169 绿**）|
| 6 | 收口：同步第 11 节的文档 + `TECH_DEBT`（C11/C15/C16 到期 + 新增项） | ⬜ 未开始 |
| 7 | 验收：正向（10.2）+ 反向验证（10.3）+ 人眼 | ⬜ 未开始 |

## 落地偏差（实现时发现的问题，逐步追加）

| # | 施工图写的 | 实际落地 | 为什么 |
| --- | --- | --- | --- |
| **1** | §2/§6.1 写"`Collision.h`（+2 个纯函数 + **1 条 `static_assert`**）" | **不加 `static_assert`** —— 采样段里只有 2 个函数 | 采样要的界是"步长 ≤ 1 格"，而现有那条 `kMaxStepPerTick < 1.0f` **已经更强** ⇒ 再写一条就是它的**推论**，**永远不会红**（`TECH_DEBT` C18 刚教过一次"空断言比没有更坏"）。这条性质的守卫改由**用例** `Sweep.ConsecutiveSamplesNeverSkipATile` 承担，理由写在 `Collision.h` 采样段注释里 |
| **2** | §6.1 的注释写"步长与 `MoveAndCollide` 的**同一个常量** ⇒ 位移与判定走在**同一套格点**上" | 用的是同一个**常量** ✓，但**步数口径不同**：采样器按**线段长度（欧氏距离）**算步数，`MoveAndCollide` 按**单轴最大位移**算 | 欧氏口径把斜向线段切得**更细**（采样每步欧氏位移 ≤ 0.5 格；位移那边按主轴算，斜向每步可达 0.707 格）⇒ **判定只会比位移更保守**，"两边都 < 1 格 ⇒ 都跳不过一整格"这条**关键性质仍然成立**。⚠️ 原措辞"同一套格点"**说过了头**，已在 `Collision.h` 的注释里改准（写明两个口径的差别与它为什么仍然安全） |
| **3** | §6.2 只写"`DirectionToFacing`：零向量返回 false"，**没写多分量输入的判据** | 实现成"**最近的 8 向**"（±22.5° 边界，常量 `kOctantBoundary = √2−1`），**不是**"两个分量都非零就算斜向" | 后者对 `(3,1)` 会判成东北（实际离东只有 18.4°）。本游戏的输入只有 {−1,0,+1} 组合，**两种写法结果相同** —— 但纯函数要对**任意**输入成立，而且"最近 8 向"能直接用例钉住（`B1` 里有 `(3,1)→East`、`(1,3)→South` 两条）。代价：多 1 个常量 + 3 行分支 |
| **4** | §6.2 的接口里**没有**越界枚举的兜底 | `FacingToVec2` 越界 → 退回**东**（**不是**零向量）；`FacingGlyph` 越界 → `"??"`（**保持 2 字符宽**） | 越界只能由强制转换造出来（正常路径不存在），但纯函数要全域安全。⚠️ 两个兜底各有理由：**零向量**会造出一颗"原地不动、永远赖在屏幕上"的子弹；**1 字符**会让 HUD 后面的列左右跳（分叉 ④ 刚定的"定宽 2"）。两条都有用例（`B3`/`B4` 各一条） |
| **5** | §6.3 的私有新增里列了 `mini::math::Vec2 m_dir{}`（"本帧归一化后的移动方向"）；§6.3 还列了 `static constexpr char kBulletSymbol = '*';` | **都不需要**：`m_dir` 删掉（`FireBullet` 从 `FacingToVec2(m_facing)` 拿方向，不读移动方向）；子弹字符改用**匿名 namespace 里的** `constexpr char kBulletGlyph`（不是类成员） | 朝向是**枚举** ⇒ 方向可以随时由它算出来，再存一份 `m_dir` 就是"第二份真相"（而且两份可能不一致）。字符同理：`'*'` 在 `Game.cpp` 只有 `FireBullet` 一处落点，放文件内常量就够；`'@'`/`'E'` 也一直是字面量，保持同一风格 |
| **6** | §7.2 的 `FireBullet()` 内部判 `SPACE` 是不是 `Pressed` | 键的判定放在 **`OnUpdate` 的 ⑤c**，`FireBullet()` 只管"造一发"（**不带参数**） | 与其余按键（`Q` / `F4` / `F7`）**同一处一视同仁**：按键的读全在 `OnUpdate`，私有函数只做一件事。副作用是 `FireBullet` 没有任何输入依赖，将来更好测 |
| **7** | `kEnemyMaxHp` 在**私有**区（S8 放的） | **挪到 public** | 用例的"几发打死"必须由 `Game::kEnemyMaxHp / kDefaultBullet.damage` **算**出来；留在私有区就只能写死 3（违反 S5_CHECKLIST 10.4：用例不许写死数字） |
| **8** | §7.3 的伪代码是"**整帧收一次**敌人位置表"，然后逐发子弹用它 | **每发子弹重新收一次**（5 个元素的栈上定长数组，不分配） | ⚠️ 整帧收一次会造出**两个真 bug**：(a) 同一帧里第一发把敌人打死后，**后面的子弹还在用旧表** ⇒ 同一具尸体被重复结算 ⇒ `kills` 多计；(b) 后面的子弹会被一具**看不见的尸体**挡住而消失。代价只是每发重收 5 个元素（可忽略） |
| **9** | §10.1 的 `Game` 用例判据写成"**屏幕上的 `'E'` 少一个**"（`nAfter == nBefore - 1`） | 改成"**屏幕字符数 == 面板里还活着的敌人所占的不同 tile 数**"（新 helper `SummarizeEnemyPanel` / `PanelSummary`，`tests/test_game.cpp`）；四条"打敌人"的用例（`G4`/`G5`/`G7`/`EnemyDiesAndDisappears`）一起改 | ⚠️⚠️ **这是 2026-10-10 实测踩出来的**：`Game.BulletHitsEnemyEvenAtMaxDelta` 报 `lhs=3 rhs=4`，而同一个用例里 `Kills() == 1`（**真死 1 个**）⇒ 屏幕少了 **2** 个 `'E'`。<br>**机制**：`DrawEntities` 一格只写一个字符，而敌人之间**没有推挤**（`C12` → S10 才做）⇒ **两只敌人可以站在同一格、只画出一个 `'E'`**。跑 exe + `F7` 面板人眼确认过（两行 `t=(x,y)` 相同）。<br>**为什么只有 `G7` 红**：四条用例的初始状态**完全相同**（都走 `WalkPlayerNextToEnemy`，内部固定 1/60），差别只有检查点时刻 —— `G5`/`EnemyDiesAndDisappears` 在停住后 **1.55 s** 检查、`G7` 在 **1.65 s** 检查，而 `dt = 0.05` 时敌人每帧走 **0.3 格** ⇒ 0.1 s 足够让两只追兵跨进同一格。**`G4`/`G5` 是"碰巧绿"，它们踩的是同一个假前提。**<br>**为什么新判据是对的**：期望值改由**游戏自己的真值**（`F7` 面板 = `m_enemies`）算出来，重叠被自然算进去；而"渲染真漏画一只"时两边不等 ⇒ **照样红**（守卫的力气没丢）。<br>⚠️ **这不是"放宽断言"**：`nAfter == nBefore - 1` 无法在重叠时成立（会假红），换成"与真值相等"反而**更严**。<br>⚠️ **到期日 = 步骤 4 —— ✅ 已于 2026-10-10 当天兑现**：真删落地后 `EnemyData` 一死就被 erase、面板不再有 `Dead` 行，于是 `PanelSummary.deadCount` 换成**解析表头的 `enemyCount`（`n=`）**，另加 `hasIdGone`（`<id gone>` 行 = 悬空 id 的唯一观察面）。四条用例同步换完 |
| **10** | §9 坑 6/7 与 `TECH_DEBT` C15 都把 `Key::K` 的落点列全了（枚举 / VK 映射 / `test_input_state` 断言 / `kKillKey` / `KillNearestEnemy` / 那条用例） | ⚠️ **漏了第三处**：`DrawAiPanel` 的表头字符串 **`"F7 AI  n=%d   K=kill nearest"`**（`Game.cpp:681`）—— 一直教人去按一个已经不存在的键 | **假提示比缺提示更坏**（用户按了没反应，会以为是输入坏了）。C15 原话就写着"删的时候**三处一起**：`kAiPanelKey`/`kKillKey` 常量、**面板表头里那个字母**、`KeyCode.h` 的两条映射"。<br>⚠️ **教训（我的扫描方式也有同一个盲区）**：`grep 'Key::K\|kKillKey\|KillNearestEnemy'` 扫不到它 —— 那里写的是**提示文本里的一个字母** `K=`。删键/改键时除了 grep **标识符**，还要 grep **提示文本里的那个字母**（`grep -rn 'K=\|kill'`）。已写进 `Game.cpp` 那一处的注释 |
| **11** | 分叉 ④ 给的 HUD 行布局示例：行 1 = `pos` + `tile`（34 列）· 行 2 = `hp` + `kills` + `dir` + 提示（44 列） | **`sumdt` 与 `v` 留在行 1**（行 1 实际 **58 列**、行 2 **44 列**，合计仍远低于 160 列预算） | ⚠️ 示例布局**漏了两个栏目**，而同一段话又写着"现有字段**一个不用砍**（`pos`/`tile`/`sumdt`/`v`/`hp` 全留）"—— 两句冲突时按后者。<br>**为什么 `sumdt` 必须和 `pos` 同行**：10.2 的运行时自证是 `pos ≈ v × sumdt`，**一行之内**才能一眼核对；拆到第二行也能看，但跨行对照更费眼。<br>代价：行 1 从示例的 34 列变 58 列 —— 仍然 < 80，**没有溢出风险**（新用例 `Game.HudFitsInTwoLinesWithoutOverflow` 钉着） |
| **12** | 施工图 §坑 11 只列了涟漪两处：`PlayerIsDrawnAtItsCell` 的 `startRow` + 两个辅助函数的扫描起点 | 还多出**两处**：① 用例侧新增 **`kTestHudRow = 2` 镜像常量**（`Game::kHudRow` 是 `Game.cpp` 的**文件内**常量、刻意没开公开接口 ⇒ 测试只能照抄）；② `tests/test_game.cpp` 顶部那句"画布要 ≥ **201**×201（视口高 = 画布高 − 1）"要改成 **202**（视口高 = 画布高 − **2**）；③ `Camera.h` 与 `test_camera.cpp` 里两处把它说成"Game::kHudRow"的注释要改准 | ⚠️ ①的失效方向是**安全**的（不同步 ⇒ `PlayerIsDrawnAtItsCell` 直接红，不是静默错位）。<br>②的取值 256 仍然满足 202 ⇒ **代码不用改，只有注释要改**。<br>③`tests/test_camera.cpp` 的 `kViewH = 23` **不受影响**（那是 Camera 单测自己的入参；数值 23 与 game 的 `25 − 2` 相同纯属巧合，已写进注释免得后人误判耦合）。<br>⚠️ 这次的教训同偏差 9：**凡是"拿屏幕行号/列号当期望值"的地方都要过一遍**，不能只 grep 常量名 |

---

## 0. 前置事实（写方案前逐条核过的，不是推测）

| # | 事实 | 位置 | 对 S9 的意义 |
| --- | --- | --- | --- |
| 1 | 帧序 = ①方向 ②归一化 ③写玩家速度 ④Q ④b 面板 ④c 杀敌 ⑤Σdt ⑤b `DriveEnemyAi` ⑥`m_world.Update` ⑦相机 ⑧F4 ⑨`FlushRemovals` | `Game.cpp:367,386,392,405,410,413,420,430,435` | S9 要插 **⑤c `FireBullet`** 与 **⑤d `ResolveBullets`**（都在 ⑥ 之前） |
| 2 | 子步长 = **0.5 格**；撞上夹到格边界 ∓ ε | `Collision.h:52,57,146` | 采样器**复用这个常量** ⇒ 位移与判定同一套格点 |
| 3 | **dt 上限 = 0.05 秒** | `Time.h:31` | 45 格/秒 ⇒ 一帧最多 2.25 格 ⇒ **点判定必穿靶** |
| 4 | `IsSolidAt` **越界 = 实心** | `Collision.h:71,78` | 子弹不可能飞丢 ⇒ **不需要 TTL**（这是分叉 ①"0 字段"的一半理由） |
| 5 | `World::Spawn` 内部 `m_entities.push_back` | `World.cpp:Spawn` | 可能 realloc ⇒ 遍历 `m_entities` 时不许 `Spawn` |
| 6 | `World::MarkForRemoval` **只碰 `m_pendingRemove`** | `World.cpp:MarkForRemoval` | ⇒ **遍历中标记删除是安全的**（只有 `Spawn` 不安全） |
| 7 | `World::FlushRemovals` 是 **swap-erase** | `World.cpp:FlushRemovals` | 实体顺序会乱 ⇒ **用例不许依赖顺序**；`m_bullets` 存 id 天然免疫 |
| 8 | `Entity` = id + pos + vel + symbol + active，**24 字节**（`static_assert` 钉着，实际 22 + 2 padding） | `Entity.h` | 分叉 ① 不加字段 ⇒ 这个 `static_assert` **不会响** |
| 9 | `DrawEntities`：`!active` **跳过绘制**；颜色 = 玩家黄 / `IsEnemyId` 红 / **其余白** | `Game.cpp:475-477` + 颜色段 | 子弹 `'*'` **自动白色**，绘制零改动 |
| 10 | 敌人 5 个、`kEnemyMaxHp = 3`、巡逻点在 `x=44..72 / y=2,4,6` | `Game.h` 的 `kEnemyMaxHp` / `kPatrolPaths` | 三发打死一个；测试的移动区（`x≤23`）与敌人区不相交 |
| 11 | 玩家最小 `y = 1.5`（tile 行 1；行 0 是墙） | `test_game.cpp:PlayerStopsAtTopWall` | 决定分叉 ④ **必须"占行"**（叠加会让玩家被 HUD 盖住） |
| 12 | HUD 现状 **78/80 列**；第 0 行是引擎顶栏（FPS/Frame/Work/Rows） | `Game.cpp:531` / `Application.cpp:167` | 分叉 ④ 的起点 |
| 13 | 相机 `viewH = screenHeight − kHudRow` | `Game.cpp:420-425` | `kHudRow` 1→2 ⇒ 视口 24→23 |
| 14 | **方向键是移动的别名**；`Space` 空闲（只有测试当样本）；`E`/`R`/`Tab`/`Esc` 都是 0 调用者 | `Game.cpp:367-370` | 分叉 ③ 的"箭头瞄准"失效；`SPACE` 可用 |
| 15 | `Key::K` 的**全部落点**：枚举（在 `F7` **之前**）· VK 映射 · `test_input_state.cpp` 的断言 · `Game.cpp` 的 `kKillKey` · `KillNearestEnemy` · **`Game.EnemyDiesAndDisappears` 用例（按 `F7`+`K`）** | 见 §9 坑 6 | ⚠️ **删 `K` 会让那条用例变红** —— 必须同一步改写，不能留给"跑起来发现红了" |
| 16 | `test_game.cpp` 已有可复用辅助：`StepFrame` / `CollectEnemies` / `MinEnemyDistanceSq` / `WalkPlayerNextToEnemy` / `ScreenText` | `test_game.cpp` 匿名 namespace | S9 的接线用例直接用，不必重造 |

---

## 1. 目标 / 明确不做

| | 内容 |
| --- | --- |
| **目标** | `SPACE` 朝朝向发射子弹 → 子弹飞 → **撞墙消失** → **命中敌人扣血** → **敌人 hp 归 0 即真删** → `Kills` 计数上 HUD |
| **PLAN 原话** | "`@ ---> * ---> E`，敌人掉血、死亡、`Kills` +1"；验收"连按 SPACE 100 次不崩溃、不重复占用内存；子弹不会穿墙" |
| **明确不做** | 玩家死亡 / 重开（S10）· Ammo / 换弹 / 连发冷却（S10）· **鼠标瞄准**（`Input` 是纯键盘抽象）· 子弹互撞 · 穿透 / 散射 / 多武器 · 敌人远程攻击 · A\*（S14）· 推挤（C12 → S10）· **`Camera::ScreenToWorld`（C11——键盘瞄准不需要它）** |

---

## 2. 交付物

| 类别 | 文件 | 预计规模 |
| --- | --- | --- |
| **引擎（唯一改动）** | `engine/include/mini/world/Collision.h`（+2 个纯函数 + 1 条 `static_assert`） | ~25 行 |
| **引擎（删）** | `engine/include/mini/platform/KeyCode.h`（`Key::K` 枚举 + VK 映射） | −3 行 |
| **新增（game）** | `game/include/game/Bullet.h`（`Facing8` + 字形表 + 命中判定纯函数 + 常量） | ~90 行 |
| **改（game）** | `game/include/game/Game.h` / `game/src/Game.cpp`（`FireBullet` / `ResolveBullets` / `m_bullets` / `m_facing` / `m_kills` / 两行 HUD / 删 `KillNearestEnemy`） | ~150 行 |
| **测试** | `tests/test_collision.cpp`（新 suite `Sweep`，6 条）· **新文件** `tests/test_bullet.cpp`（suite `Bullet`，8 条）· `tests/test_game.cpp`（+8 条、**改写 1 条**）· `tests/test_input_state.cpp`（删 `K` 的断言）· `tests/CMakeLists.txt`（+1 行） | ~350 行 |
| **文档** | 本文件 · `PLAN`（S9 小节 + G 节）· `TECH_DEBT`（C11/C15/C16 到期 + 新增）· `AGENTS.md` §2 | — |

**预计用例总数：146 → 约 168**（6 + 8 + 8，另 1 条是改写而非新增）。

---

## 3. 依赖规则

| 层 | 允许 include | 备注 |
| --- | --- | --- |
| `game/Bullet.h` | `mini/math/Vec2.h` · `mini/world/TileMap.h` · `mini/world/Collision.h` · 标准库 | **纯函数 + 常量**；⚠️ **连 `Entity` 都不 include**（命中判定收的是"敌人位置" `span<const Vec2>`，不是 `Entity*`），也不认识 `Game` / 输入 / 屏幕（同 `ai/EnemyBrain.h` 的路子） |
| `Game.h/.cpp` | `game/Bullet.h` + 引擎公共头 | 只做**接线**：生成、遍历、标删、画 |
| `engine/` | **不许 include `game/`** | `PLAN` A.3 单向；本阶段只动 `Collision.h` 与 `KeyCode.h` |

⚠️ `Bullet.h` **不是**"子弹的类" —— 它是一个**无状态模块**：没有 `struct Bullet`，子弹就是 `Entity`（分叉 ①）。

---

## 4. 数据结构与所有权

| 数据 | 在哪 | 所有权 / 生命周期 |
| --- | --- | --- |
| 子弹的**位置/速度/字符/active** | `mini::world::Entity`（`m_world` 里的 `std::vector<Entity>`） | `World` 拥有；`Spawn` 分配 id，`MarkForRemoval` + `FlushRemovals` 回收 |
| **哪些实体是子弹** | `Game::m_bullets`（`std::vector<mini::world::EntityId>`） | `Game` 拥有；**只存 id**（4 字节/发）⇒ 不存在"两份数据不一致" |
| 朝向 | `Game::m_facing`（`Facing8`） | `Game` 拥有；8 向枚举，**没有无效值**（初值 = 东） |
| 击杀数 | `Game::m_kills`（`int`） | `Game` 拥有；只增 |
| 敌人血量 | `game/Enemy.h` 的 `EnemyData::hp`（`m_enemies`） | `Game` 拥有；S9 **第一次真的删它**（→ `TECH_DEBT` C16 到期） |

**`m_bullets` 的维护规则（只有一处）**：

```text
FireBullet()      : m_world.Spawn(...) → m_bullets.push_back(id)     // 只增
每帧末（⑨ 之后）  : std::erase_if(m_bullets, Find(id) == nullptr)     // 自愈压缩
```

⚠️ **只增 + 一处压缩** ⇒ 不可能出现"两边必须同步"的双向责任（那是 (b) 的病）。

---

## 4.1 容量与内存上限（**结论已上升为全局约定：`PLAN` A.6**）

> 起因：本人问"子弹也是实体，以后会不会实体过多？" —— 结论是"现在很小，但**上界算法与直觉不同**"，
> 而且**第一个撞到的墙不是内存**。完整推导在 `PLAN` **A.6「容量与生命周期约定」**，这里只留 S9 相关的数。

| 项 | 结论 |
| --- | --- |
| **代码里有没有上限** | **没有**：`m_entities` / `m_pendingRemove` / `m_bullets` 全无 cap、无 reserve、无 assert |
| **屏幕是不是上界** | ⚠️ **不是** —— 子弹之间**没有碰撞**（C12 未做），**可以重叠** ⇒ "80×24 格"只是**可见位置**的上界 |
| **真正的上界公式** | **存量 = 到达率 × 平均停留时间**（Little 定律） |
| **到达率（S9）** | ≤ **帧率** —— `InputState::m_pulse` 是**每键一个 `bool`** ⇒ 每键每帧最多一次 `Pressed`；⚠️ **键盘自动重复不会产生 `Pressed`**（`Get()` 里 `was && now → Held`） |
| **停留时间（S9）** | ≤ **地图对角线 ÷ 弹速** = 200√2 ÷ 45 ≈ **6.3 秒**（撞实心格或命中即死；地图外恒为实心） |
| **60fps 下的上界** | **≈ 378 发** ⇒ 实体侧最坏 **≈ 16 KB**（含 MSVC `vector` 的 1.5 倍容量余量） |
| 对照：整个程序内存 | **≈ 100 KB**，大头是**固定的两块**（ANSI 序列化缓冲 38.4 KB + `TileMap` 40 KB）⇒ 实体侧约 **15%** |

## 4.2 ⚠️ 这条上界**什么时候会失效**（设计武器前必须算）

| 武器形态 | 上界公式 | 60fps 下 B | 内存 | **先撞到哪面墙** |
| --- | --- | --- | --- | --- |
| 单发（S9） | `fps × (对角线 ÷ 速度)` | 378 | 16 KB | 没有 |
| **霰弹**（一发 8 颗） | `8 × fps × 寿命` | ≈ 3,000 | 126 KB | ⚠️ **CPU** |
| **分裂**（每 0.3s 裂成 2） | `fps × 寿命 × 2^(寿命/0.3)` | ≈ 200 万 | 56 MB | ⚠️⚠️ **CPU 早就死了** |

**为什么是 CPU**：`ResolveBullets` 对**每发**子弹调一次 `m_world.Find`，而它是**线性扫** ⇒ **O(B²)**（帧末自愈压缩再来一遍）：

| B | 每帧比较 ≈ 2B² | 量级 | 60fps 预算 16.7 ms |
| --- | --- | --- | --- |
| 378（S9 最坏） | 286 k | **≈ 0.3 ms** | ✅ 占 2% |
| 3,000（霰弹） | 18 M | **≈ 18 ms** | ❌ 一帧就超预算 |
| 20,000 | 800 M | ≈ 0.8 s | ❌ 卡死 |

⇒ **所以"加不加 cap"应该由 CPU 决定，而不是内存**；而且**内存到 56 MB 都不是问题的时候，CPU 早就不可用了**。

**将来真要上量的两条出路**（登记在 `TECH_DEBT` **C19 / C20**）：

| 路 | 做法 | 代价 |
| --- | --- | --- |
| **(a) 加硬 cap** | 到上限**拒绝生成**（或**回收最老的**）+ 分裂要加"代数上限" | 最简单；B 有界 ⇒ O(B²) 自动有界。但要定义"按了没反应"的体验 |
| **(b) 把 O(B²) 降成 O(n)** | 一次遍历 `m_entities` 挑出子弹（而不是每发 `Find`） | 需要"一眼认出哪个是子弹" ⇒ 给 `Entity` 加**吃 2 字节 padding 的 `kind` 字段**（尺寸仍是 24）。⚠️ 那就是分叉 ① 的**中 2**，子弹上千时它会重新变成正解 |

**⛔ 无论走哪条，加 cap 时绝不许用"回收 id 再复用"的对象池** —— `EntityId` 的"**永不复用**"契约保证悬空 id 只表现为"**找不到**"而不是"**找错**"（`Entity.h` 的注释），复用 id 会把这个最难查的 bug 请回来。

**S9 的取法**：**不加 cap、也不加 `reserve`**（见决策表 D13/D14）。⚠️ 我一度建议加 `reserve(32)`，**重新算过之后撤掉**：378 发的量级下 `vector` 只增长约 10 次、每次拷贝 ≤ 12 KB —— 那属于"提前优化 + 一个要维护的魔法数字"。

---

## 5. 逐文件实施步骤

| 步 | 改什么 | 不做什么 |
| --- | --- | --- |
| **1** | `Collision.h`：`SweepStepCount` + `SweepSampleAt` + `static_assert`（步长 < 1 格）；`test_collision.cpp` 加 `Sweep` suite | 不动 `MoveAndCollide` / `IsSolidAt` |
| **2** | 新建 `game/Bullet.h`（`Facing8` + `kFacingGlyphs[8]` + `DirectionToFacing` / `FacingToVec2` / `FirstEnemyOnSegment` / 常量）；新建 `tests/test_bullet.cpp`；`tests/CMakeLists.txt` +1 行 | 不碰 `Game.*`（本步只加"能离线跑"的东西） |
| **3** | `Game.h/.cpp`：`m_bullets` / `m_facing` / `FireBullet` / `ResolveBullets` / 帧序插 ⑤c⑤d；**删 `Key::K`**（枚举 + 映射 + `kKillKey` + `KillNearestEnemy` + `test_input_state` 断言）；**改写 `Game.EnemyDiesAndDisappears`**（改用真子弹打死） | 不做真删（本步先只 `active=false`，第 4 步再删） |
| **4** | 真删：`ResolveBullets` 里 hp≤0 → `MarkForRemoval(敌人的 Entity)` **且** `std::erase_if(m_enemies, ...)`；新增 `KilledEnemyIsRemovedAndLeavesNoDanglingId` | —— |
| **5** | HUD：`kHudRow` 1→2；`DrawHud` 改两行（行 1 位置类 / 行 2 状态类）；加 `kills` 与 `dir`；提示加 `SPC`/`F7`；修 `PlayerIsDrawnAtItsCell` 的 `startRow`；两个辅助函数扫描起点 | 不动相机的 `viewH` 公式（它自动跟着 `kHudRow`） |
| **6** | 收口：`PLAN` 的 S9 小节 / A.2（`game/Bullet.h`）/ G 节 / H 节；`TECH_DEBT` 关 `C11`/`C15`/`C16` + 新增项；`AGENTS.md` §2 加本文件 | 不改历史留档 |
| **7** | 验收（§10） | —— |

⚠️ **每一步都必须是"能构建、能跑"的状态**（`AGENTS.md` 第 7 节第二条）。

---

## 6. 接口设计（**只列声明，不写实现**）

### 6.1 `engine/include/mini/world/Collision.h`（+2 个纯函数）

```cpp
// 把线段 [from, to] 切成 ceil(距离 / kMaxStepPerTick) 步（**至少 1 步**）。
// ⚠️ 步长与 MoveAndCollide 的子步长是**同一个常量** —— 位移与判定必须走在同一套格点上。
[[nodiscard]] inline int SweepStepCount(mini::math::Vec2 from, mini::math::Vec2 to) noexcept;

// 第 i 个采样点，i ∈ [0, steps]（**两端点都含**）。
[[nodiscard]] inline mini::math::Vec2 SweepSampleAt(mini::math::Vec2 from,
                                                    mini::math::Vec2 to,
                                                    int stepCount, int i) noexcept;
```

### 6.2 `game/include/game/Bullet.h`（新建）

```cpp
namespace game {

// 8 向朝向。⚠️ 用枚举而不是 Vec2：用例不比浮点（同 S8 用 enum class EnemyState 的理由）。
enum class Facing8 : std::uint8_t { East = 0, NorthEast, North, NorthWest,
                                    West, SouthWest, South, SouthEast };

// HUD 用的**定宽 2 字符**字形（全 ASCII —— 代码页 936 下不能写非 ASCII）。
// 顺序与 Facing8 一一对应；「目测效果」不满意时**只改这张表**。
inline constexpr const char* kFacingGlyphs[8] = { "->", "/>", "^^", "\\<",
                                                  "<-", "</", "vv", "\\>" };

struct BulletTuning {
    float speed  = 45.0f;   // 格/秒（2.25 × Game::kMoveSpeed）
    int   damage = 1;       // 单发伤害
};
inline constexpr BulletTuning kDefaultBullet{};

[[nodiscard]] inline const char* FacingGlyph(Facing8 f) noexcept;          // 查表，不做边界假设
[[nodiscard]] inline mini::math::Vec2 FacingToVec2(Facing8 f) noexcept;    // 单位向量
// 由"本帧的移动方向"推出朝向；**零向量返回 false**（调用方保持原朝向 —— 分叉 ③ 的"停下沿用"）。
[[nodiscard]] inline bool DirectionToFacing(mini::math::Vec2 dir, Facing8& out) noexcept;

// 命中判定的结果：命中谁 + 是否被墙挡住（两者可能同时为真时**墙优先**，见下）。
struct BulletHit {
    int  enemyIndex = -1;     // 命中 enemies 里的第几个；-1 = 没打中任何敌人
    bool blocked    = false;  // 线段被实心格挡住（子弹撞墙）
};

// 命中判定：沿线段采样，返回**第一个**被命中的敌人**下标**（没有则 -1）+ 是否撞墙。
// · 线段上的实心格 = 阻挡 ⇒ **墙后的敌人不会被命中**（墙优先，命中即停）
// · 参数是**敌人的位置**（点模型：占格 = `ToIndex(pos)`）—— 本函数**不认识 `Entity`**，
//   也不认识 `EnemyData`，所以它可以离线单测（同 `ai/EnemyBrain.h` 的路子）
// · 返回**下标**而不是指针：调用方拿它去索引自己那份平行数组（是谁由调用方决定）
[[nodiscard]] inline BulletHit FirstEnemyOnSegment(
    const mini::world::TileMap& map, mini::math::Vec2 from, mini::math::Vec2 to,
    std::span<const mini::math::Vec2> enemyPositions) noexcept;

} // namespace game
```

### 6.3 `game/include/game/Game.h`（公开面）

```cpp
[[nodiscard]] int            Kills()  const noexcept;   // 给 HUD 与用例（同 PlayerHp 的先例）
[[nodiscard]] mini::ai::…    // （无）
[[nodiscard]] game::Facing8  Facing() const noexcept;   // 给用例断朝向（只读）
```

**私有新增**：

```cpp
void FireBullet();                                        // ⑤c
void ResolveBullets(const mini::core::FrameContext& ctx);  // ⑤d
std::vector<mini::world::EntityId> m_bullets;              // 只存 id（自愈）
mini::math::Vec2                   m_dir{};                // 本帧归一化后的移动方向（③ 那里已经在算）
Facing8                            m_facing = Facing8::East;
int                                m_kills  = 0;
```

**删除**：`KillNearestEnemy()` · `kKillKey` · 帧序 ④c。

---

## 7. 算法与伪代码

### 7.1 帧序（S9 版）

```text
①方向(读 WASD/箭头)  ②归一化  ③写玩家速度 + 更新 m_facing（零向量则沿用）
④ Q  ④b F7 面板
⑤ Σdt
⑤b DriveEnemyAi                       ← S8 的敌人决策（写敌人速度）
⑤c FireBullet()                       ← ★ 生成：必须在 ⑥ 之前（子弹当帧就飞）
⑤d ResolveBullets(ctx)                ← ★ 判定：用**预测线段**，也在 ⑥ 之前
⑥ m_world.Update(dt)                  ← 玩家/敌人/子弹同一个循环被积分（撞墙白拿）
⑦ 相机 Follow   ⑧ F4 快照   ⑨ FlushRemovals + m_bullets 自愈压缩
```

### 7.2 `FireBullet`（生成）

```text
若 `SPACE` 不是 Pressed          → return                    // 边沿触发：按一下一发
若 `m_dir` 长度平方 == 0         → return                    // 不因为在移动里；朝向由 m_facing 提供
在遍历之外 Spawn 一个 Entity：
    transform.pos = 玩家的 transform.pos
    velocity      = FacingToVec2(m_facing) * kDefaultBullet.speed
    symbol        = '*'
m_bullets.push_back(返回的 id)
```

### 7.3 `ResolveBullets`（判定 + 标删）

```text
先把"活着且 id 有效的敌人位置"收进一个可复用的暂存表（`std::span` 交给纯函数）：
    for enemy in m_enemies: if enemy.hp > 0 且 m_world.Find(enemy.id) != nullptr → 记下它的 pos
    （同时记下"第 i 项对应 m_enemies 的哪一个"，因为真删会打乱下标）

for id in m_bullets:                                   // ① 遍历 id 表（不是 m_entities）
    e = m_world.Find(id);  若 e == nullptr → continue   // 自愈：过期 id 直接跳过
    from = e.pos
    to   = e.pos + e.velocity * dt                      // 预测线段（见 7.1 的落点理由）

    hit = FirstEnemyOnSegment(map, from, to, 敌人位置表)
    若 hit.blocked 或 hit.enemyIndex >= 0:
        m_world.MarkForRemoval(id)                      // ⚠️ 安全：它只碰 m_pendingRemove
        若 hit.enemyIndex >= 0:
            把那个敌人的 hp -= kDefaultBullet.damage
            若 hp <= 0 → 记进"待真删"名单（**存 id，不存下标** —— 真删会 swap-erase）
在遍历之外：对名单里的敌人在 m_enemies 里 erase_if(按 id 找)，并 MarkForRemoval(它的 Entity)
```

### 7.4 `FirstEnemyOnSegment`（`Bullet.h` 里的纯函数）

```text
result = { enemyIndex = -1, blocked = false }
stepCount = SweepStepCount(from, to)                        // ≥ 1（两端点都含）
for i in [0, stepCount]:
    p = SweepSampleAt(from, to, stepCount, i)
    若 IsSolidAt(map, p):                                   // ⚠️ **墙优先**：先判实心
        result.blocked = true
        return result                                       // 命中即停 ⇒ 墙后的敌人**不会**被命中
    若敌人位置表里第一个 `ToIndex(pos) == ToIndex(p)` 的项存在:
        result.enemyIndex = 那个项的下标                     // **第一个**（沿线段顺序），不是最近的
        return result
return result                                               // 什么都没碰上
```

⚠️ **墙优先是刻意的**：子弹不应该"穿过墙打死墙后的敌人"。这条要有专门的用例钉住。

---

## 8. 接口决策表

| # | 决定 | 取法 | 代价 / 触发条件 |
| --- | --- | --- | --- |
| D1 | 子弹的数据 | 纯 `Entity` + `m_bullets`（id 表，自愈压缩） | 升级触发：S10 多武器 |
| D2 | 命中判定 | 沿线段等距采样（`kMaxStepPerTick`） | engine +25 行 |
| D3 | 采样器住哪 | `engine/include/mini/world/Collision.h` | engine 不再"零改动"（本来也要删 `Key::K`） |
| D4 | 采样器接口 | 暴露格点（`SweepStepCount` / `SweepSampleAt`） | 两个调用方各自判 |
| D5 | 朝向 | `Facing8` 枚举 + 移动即朝向 + 停下沿用 | 不能边退边打（换键盘后再加 `E` 锁定） |
| D6 | 开火键 / 节奏 | `SPACE` / `Pressed` 边沿 | 按住不连发（S10 再说） |
| D7 | 帧序落点 | `FireBullet` 与 `ResolveBullets` **都在 ⑥ 之前**（预测线段） | 若改到 ⑥ 之后必须额外存"上一帧位置" |
| D8 | 墙 vs 敌人 | **墙优先**（先判实心，命中即停） | 要有用例钉住"墙后不中" |
| D9 | HUD | **占行 + 两行**（`kHudRow` 1→2） | 世界可见区 −1 行；1 个表达式 + 2 处扫描起点 |
| D10 | HUD 朝向字形 | 2 字符 ASCII 表（方案 A） | 不满意只改 `kFacingGlyphs` |
| D11 | 敌人死亡 | hp ≤ 0 → `active = false`（同帧可见 `Dead`）→ **真删**（⑤d 里） | `TECH_DEBT` C16 到期 |
| D12 | `Key::K` | **删除**（枚举 + 映射 + 常量 + 函数 + 测试断言） | `TECH_DEBT` C15 到期；⚠️ 用例必须同步改写 |
| D13 | **数量上限** | **S9 不加 cap，也不加 `reserve`** | 上界是**结构性**的（≤378 发，见 §4.1/§4.2）⇒ 加 cap 是"给不存在的病吃药"。⚠️ **触发条件**：出现"一次 `Pressed` 多发生成"的武器（霰弹/分裂）**或** S10 刷怪 → `TECH_DEBT` **C19**（**含"不许用 id 复用池"的禁令**） |
| D14 | **`ResolveBullets` 的 O(B²)** | 照现状（每发一次 `Find`）**不优化** | B ≤ 378 时 ≈ **0.3 ms/帧**（60fps 预算的 2%）。⚠️ **触发条件**：B 要**上千**时（那时内存才 126 KB，CPU 已经 ≈18 ms/帧）→ 出路见 §4.2 与 `TECH_DEBT` **C20** |

---

## 9. 坑表

| # | 坑 | 处理 |
| --- | --- | --- |
| 1 | **遍历 `m_entities` 时 `Spawn` → 迭代器失效**（PLAN 点名） | 架构上消除：`ResolveBullets` 遍历 `m_bullets`；且 `FireBullet`（会 `Spawn`）在它**之前**、当时无任何遍历 |
| 2 | **判定用积分后位置 ⇒ 需要"上一帧位置"**；用 `pos_after − v·dt` 反推在**被墙夹住时错**（起点落进墙里 ⇒ 线段穿墙 ⇒ **假命中墙后的敌人**） | 判定放 ⑥ **之前**，用**预测线段**（D7） |
| 3 | **子弹穿靶**（一帧飞 2.25 格） | 分叉 ②（沿线段采样，步长 0.5） |
| 4 | **方向为 `{0,0}`** | `FireBullet` 里不判方向（朝向是枚举，永远有效）；但 **③ 更新朝向时**零向量要"沿用上次" |
| 5 | **子弹与玩家同格** | 命中判定**只对敌人**做 ⇒ 玩家格天然排除 |
| 6 | ⚠️ **删 `Key::K` 会让 `Game.EnemyDiesAndDisappears` 变红**（它按 `F7`+`K`） | **同一步内改写**成"朝敌人开火打死它"（用真入口）。这正是 `AGENTS.md` 第 7 节第三条：**接线前先扫全部调用点** |
| 7 | ⚠️ **`Key::K` 在枚举里位于 `F7` 之前** | 删它会**重编号 `F7`**；`test_input_state.cpp` 的 `kVkK = 0x4B` 断言要一起删（否则编译不过） |
| 8 | **`FlushRemovals` 是 swap-erase** ⇒ 实体顺序会乱 | 用例不许依赖顺序；`m_bullets` 存 id 免疫 |
| 9 | **`m_bullets` 里的过期 id** | 帧末一处 `erase_if(Find(id) == nullptr)`；`ResolveBullets` 里也要 `continue`（同一帧内可能已被标删） |
| 10 | **墙后的敌人被误伤** | 采样器"先判实心、命中即停"（D8）+ 专门用例 |
| 11 | **`kHudRow` 1→2 的涟漪**：视口高 −1、行号公式变 | 只影响 `PlayerIsDrawnAtItsCell` 的 `startRow` 与两个辅助函数的扫描起点（§0 事实 11/13） |
| 12 | **面板（`F7`）与两行 HUD 的行距** | `DrawAiPanel` 的 `firstRow` 夹取用的是 `kHudRow` ⇒ 自动适配；但**人眼要确认两行 HUD 没盖住面板表头** |
| 13 | **子弹永远不消失**（若某条路径漏了标删） | 用例 `BulletsDoNotAccumulateForever`（连发 N 次后场上子弹数回落） |
| 14 | **`kEnemyMaxHp`(3) 与 `kBulletDamage`(1) 的关系不能写死** | 用例用两个公开常量算出"三发" |
| 15 | ⚠️ **斜向"擦格角"的那一格可能一个采样点都没有** ⇒ 该格里的敌人不会被命中（**方法的固有限制，不是 bug**） | **明确不修，只记录**（本人 2026-10-10 决定）→ `TECH_DEBT` **C21** + `Bullet.h` 的 `FirstEnemyOnSegment` 注释。边界：轴向**不可能漏**（格宽 1 > 间距 0.5）、穿格"边"**不可能漏**（路程 ≥ 1 格）、**只有擦角**会漏（切入可以任意小）。⚠️ 与 S7 点模型"对角缝能穿"**语义一致**；现状**没有用例覆盖擦角** |

---

## 10. 验收

### 10.1 用例清单（实测：S8 收口 146 → 步骤 1 **152** → 步骤 2 **160** → 步骤 3 **167** → 步骤 4 **168** → 步骤 5 **169**（⬜ 步骤 5 待实测））

**`Sweep` suite（`tests/test_collision.cpp`，离线纯函数，6 条）**

| # | 用例 | 钉什么 |
| --- | --- | --- |
| 1 | `BothEndpointsAreSampled` | 两端点都在采样点里（否则"贴着目标起点"会漏） |
| 2 | `ZeroLengthSegmentStillHasOneStep` | 返回 ≥ 1（防调用方除零 / 空循环） |
| 3 | `ConsecutiveSamplesNeverSkipATile` | 相邻采样点的 `ToIndex` 差 ≤ 1（**"不跳格"的性质**，用 `kMaxStepPerTick` 算） |
| 4 | `SegmentThroughBlockedTileIsDetected` | 用 `IsSolidAt` 当谓词，穿过实心格能检出 |
| 5 | `SegmentBesideABlockedTileIsNotDetected` | 贴着墙走**不算**命中（防"用包围盒糊弄"） |
| 6 | `SweepIsPureFunctionOfInputs` | 同输入同输出 |

**`Bullet` suite（新文件 `tests/test_bullet.cpp`，8 条）**

| # | 用例 | 钉什么 |
| --- | --- | --- |
| 1 | `DirectionToFacingMapsAllEightWays` | 8 个方向 → 8 个枚举 |
| 2 | `ZeroDirectionReportsFalse` | 零向量 ⇒ `false`（调用方沿用旧朝向） |
| 3 | `FacingToVec2IsUnitLengthAndPointsRight` | 8 个向量都是单位长度、方向正确 |
| 4 | `FacingGlyphsAreTwoCharsAndAscii` | **字形表全 ASCII、定宽 2**（钉住代码页 936 的约束） |
| 5 | `FirstEnemyOnSegmentHitsWhenBothEndsOutside` | ★**防穿靶的核心**：起终点都在敌人格之外，但线段穿过 → 命中 |
| 6 | `FirstEnemyOnSegmentIgnoresEnemiesOffTheLine` | 线段外的敌人不中（防"拿距离糊弄"） |
| 7 | `WallBlocksEnemyBehindIt` | ★墙优先：墙后的敌人**不中**（D8） |
| 8 | `ReturnsTheFirstEnemyAlongTheSegment` | 多个敌人时返回**线路上第一个**（不是最近的） |
| **9** | `EnemyStandingInsideWallIsBlockedNotHit` | ⬜ **待补**（2026-10-10 推演 R3 时发现）：**同一个采样点既实心又站着敌人**时，必须报 `blocked` 而不是命中 ⇒ 这才能把"循环内墙优先"这条**顺序**钉住（见 §10.3.2）。⚠️ 游戏里这个状态**不可达**（敌人只出生在空地、且撞不进墙），所以它是**防御分支** —— 但按"实现了的分支必须有用例"这条纪律，它得补 |

**`Game` suite（+8 条，其中 1 条是改写）**

| # | 用例 | 钉什么 |
| --- | --- | --- |
| 1 | `FiringSpawnsBulletAlongFacing` | 按下 `SPACE` → 屏幕上多一个 `'*'`，且**在朝向上**（用 `Facing()` 算期望值） |
| 2 | `HoldingSpaceFiresOnlyOnce` | 边沿触发：按住 1 秒只有一发 |
| 3 | `BulletDiesOnWall` | 贴墙打 → 子弹没出墙（屏幕上墙外无 `'*'`） |
| 4 | `BulletDamagesEnemy` | 命中后敌人 `hp` 下降（用面板文本或"再打两发就死"间接测）。⚠️ 落地后判据 = `面板 n= 不变` + `屏幕 'E' 数 == 面板不同 tile 数` + `Kills()==0` |
| 5 | `ThreeHitsKillAnEnemy` | `kEnemyMaxHp / kBulletDamage` 发即死（**用公开常量算**）。同上三条判据（`n=` 少 1） |
| 6 | `KilledEnemyIsRemovedAndLeavesNoDanglingId` | ★**`TECH_DEBT` C16 到期**：真删之后 `m_enemies` 与 `m_world` 两边都不留悬空 id。判据 = `面板 n=少1` + `无 <id gone>` + `屏幕 'E' 数 == 面板不同 tile 数` + `Kills()==1`（见 §10.3 的 R5/R5′） |
| 7 | `BulletsDoNotAccumulateForever` | ⚠️ **判据必须对"重叠"免疫**（`PLAN` A.6 第 ② 条）：朝同一面墙连开 N 发 → 等 **> 6.3 秒**（= 地图对角线 ÷ 弹速）→ **屏幕上的 `'*'` 必须 == 0**。⚠️ **不能**写成"数屏幕上的 `*` 有没有减少" —— 子弹**会重叠**（没有子弹间碰撞），50 发叠在一格里只数出 1 个 ⇒ **漏删 49 发也照样绿**（又一个"空断言"的雏形） |
| 8 | `BulletHitsEnemyEvenAtMaxDelta` | ★用 `dt = kMaxDelta`（0.05）打一发 —— **分叉 ② 的正向证明**（点判定在这里必红）。⚠️ 真正的判据是 `Kills() == 1`；屏幕那条只与面板真值比（偏差 9） |
| — | **`EnemyDiesAndDisappears`（改写）** | 不再按 `K`，改用"朝敌人开火打死它"；判据 = **屏幕 `'E'` 与面板真值一致** + `面板 n= 少 1`（真删后不再有 `Dead` 行可看，见偏差 9/偏差 10 后的落地） |
| **9** | `HudFitsInTwoLinesWithoutOverflow` | ⚠️ **施工图没列，步骤 5 落地时加的**：把"人眼第 4 条（两行 HUD 不溢出、不折行）"变成机器守卫。判据 = 用**比终端宽的画布**（200×40，否则溢出会被 `FrameBuffer::Set` 静默裁掉 ⇒ 空断言）量出两行文本的真实长度 < 80 + 行 1 含 `pos/tile/sumdt/v` + 行 2 含 `hp/kills/dir ->/F7` + **两行里不许出现 `@`/`E`/`*`**（`FindChar`/`CountChar` 是全屏扫描的，HUD 上出现同名字符会让那些用例**静默算错**） |

### 10.2 正向验收 + 人眼

```powershell
cmake --build build --config Debug
.\build\bin\Debug\mini_tests.exe
ctest --test-dir build -C Debug --output-on-failure
.\build\bin\Debug\MiniEngine.exe
```

| # | 人眼看什么 |
| --- | --- |
| 1 | 按 `SPACE` → 前方出现 `*` 并**沿朝向飞** |
| 2 | 对着墙打 → `*` **在墙前消失**，不出墙 |
| 3 | 对着敌人打 → `*` 到敌人身上消失，**连打三发它消失** |
| 4 | HUD **两行**都不溢出、不折行；第二行有 `kills` 与 `dir ->` |
| 5 | **转身再看 `dir`**：往八个方向各走一下，字形跟着变（这是分叉 ③ 的必要配套） |
| 6 | **连按 `SPACE` 100 次**不崩；任务管理器看**内存稳定**（PLAN 的原始验收） |
| 7 | 敌人从屏幕消失后，按 `F7` 面板里**也少一行**（`n=5` → `n=4`） |
| 8 | `F4` 快照仍工作（回归） |

⚠️ 本机键盘 ghosting：**"边跑边打"是 2 键**，验不了 → 动线是"跑 → 松手 → 打"。

### 10.3 反向验证（**每条都必须看到红，跑完必须还原 + 重建**）

| # | 破坏方式 | 应变红的用例 |
| --- | --- | --- |
| R1 | 采样步长改成 **2 格**：把 `SweepStepCount` 里那**两处** `kMaxStepPerTick` 临时换成**字面量 `2.0f`** | ⚠️ **步骤 1 后就能做**：`Sweep.ConsecutiveSamplesNeverSkipATile`（**只这一条**：S4/S5 的线段长度是 2 的倍数，2 格步长照样落在墙上 ⇒ 仍绿）。⚠️ **不许**改常量 `kMaxStepPerTick` 本身 —— 它有 `static_assert(< 1.0f)`，改成 2.0 **直接编译不过** |
| R2 | `ResolveBullets` 改用**积分后的位置**做一个点判定（不做采样） | `Game.BulletHitsEnemyEvenAtMaxDelta` |
| R3 | **把 `FirstEnemyOnSegment` 里的墙判定整段删掉**（⚠️ **不是**原来的"对调判定顺序"——理由见下） | `Bullet.WallBlocksEnemyBehindIt`（**2 条断言**：`hit.blocked` 与 `inside.blocked`）；其余 7 条仍绿（`B6` 的线段不碰墙） |
| R4 | `FireBullet` 改用 `Held` 而不是 `Pressed` | `Game.HoldingSpaceFiresOnlyOnce` |
| R5 | 真删**只 `MarkForRemoval(Entity)`、不 erase `m_enemies`** | `Game.KilledEnemyIsRemovedAndLeavesNoDanglingId`：面板会印 `<id gone>` ⇒ `!hasIdGone` **红**，且 `enemyCount == nBefore-1` 也红（两个方向都钉住） |
| R5′ | **反向的那一半**（2026-10-10 步骤 4 加）：只 erase `m_enemies`、**不 `MarkForRemoval(Entity)`** | 同一条用例：实体还留在世界里、**没人驱动它** ⇒ 面板 `n=` 是 4 但屏幕上多画一个 `'E'` ⇒ `nAfter == panel.aliveTiles` **红** |
| R6 | 去掉 `m_bullets` 的帧末自愈压缩 | `Game.BulletsDoNotAccumulateForever`（可能不红 → 需如实登记） |
| R7 | 加**第二行 HUD** 但不改 `kHudRow`（叠加式） | 人眼：贴地图上边缘时玩家被文字盖住（**这条只有人眼能验**） |

> ⚠️ **R6 我预判可能不红**（过期 id 只是"多发一次 `Find`"）→ 若真不红，**如实登记**为"自愈压缩目前没有机器守卫"，别写成"通过"。

#### 10.3.1 实测记录 —— R1（2026-10-10；**AI 改破坏码，本人跑**）

**破坏方式**：`SweepStepCount` 内部两处 `kMaxStepPerTick` → 字面量 `2.0f`（不动常量本身 —— 那条有 `static_assert`，改常量会直接编译不过）。

**结果**：`[FAIL] Sweep.ConsecutiveSamplesNeverSkipATile` —— **整次运行只有这一个 `[FAIL]` 行**。

**红集（断言行级，与动手前的预判逐条对照）**：

| 线段 | 实际红线 | 实际条数 | 预测 | 一致 |
| --- | --- | --- | --- | --- |
| seg1 横向 10 格 | `test_collision.cpp:501`（步数够）+ `:515`（`dx`） | 1 + 5 | 1 + 5 | ✅ |
| seg2 纵向 10 格 | `:501` + `:516`（`dy`） | 1 + 5 | 1 + 5 | ✅ |
| seg3 斜向 (7,7) | `:501` + `:515`/`:516` 各 2 | 1 + 4 | 1 + 4 | ✅ |
| seg4 对角线 (30,30) | `:501` + `:515`/`:516` 若干 | 1 + ~14 | 1 + ~14 | ✅ |
| **其余 5 条用例** | 无 | 0 | 0 | ✅ |

**结论**：这条守卫**真的会红** —— 它不是空断言（对照 `TECH_DEBT` C18 那条恒真的版本断言）。
⚠️ 顺带证实了一件事：用例里那条"**步数够**"断言（`:501`）与 `dx/dy` 那条是**两条独立守卫**，而且**先红的是步数那条** —— 即使以后有人放宽 `dx/dy`，步数那条仍然拦得住。

**还原**：破坏码已改回 `kMaxStepPerTick`；`git diff --stat` 与破坏前**逐字节相同**（`Collision.h` 仍是 `+67/−2`）；`kR1BrokenStep` 与 🚨 标记各 **0 处**。
⚠️ **还原后没有第三次运行**：本步本人的两次回报是「破坏前 **152 passed, 0 failed**」与「破坏后 **1 红**」；还原后的代码与第一次运行的那份**完全相同**（不是推断，是 diff 比对），所以第三次没跑 —— 要跑也行，只是重复。

#### 10.3.2 动手前的推演：**R3 的原方案必须改**（2026-10-10，步骤 2 完成后）

原方案写的是"把 `FirstEnemyOnSegment` 里**先判实心、后判敌人**的顺序**对调**"（墙不再优先）。推演之后发现：**那样一条都不会红。**

**为什么**：`FirstEnemyOnSegment` 是**沿线段逐点**走的。对 `B7` 的主场景（墙在格 7、敌人在格 9），采样点先到达墙那一格（x≈7.5）再到达敌人那一格（x≈9.5）——
所以**无论循环里两条判定的先后如何**，"先碰到的都是墙"⇒ 结果一样。**顺序只在"同一个采样点既实心、又站着敌人"时才可观察**，
而那要求**敌人卡在墙里** —— 游戏里不可达（敌人只出生在空地，且撞不进墙）。

⇒ 这正是 S8 学到的第 1 条发现的复发：**参数/顺序级的破坏在期望值不覆盖它时是空转**。

**处置**：
1. **R3 改成"把墙判定整段删掉"** —— 这测的是"墙规则**存不存在**"（安全相关的那一半），一条也不会空转：`B7` 的两条 `blocked` 断言会红；
2. **发现一个用例缺口** → `§10.1` 补记 **B9**（`EnemyStandingInsideWallIsBlockedNotHit`，⬜ 待补）：把"循环内墙优先"这条顺序钉住。
   ⚠️ 它测的是**不可达状态下的防御分支**，属于"实现了的分支必须有用例"那条纪律，**不是**功能需求；
3. **诚实登记**：B9 补齐之前，"循环内两条判定的先后"**没有机器守卫** —— 靠的是这条"墙在敌人前面"的推理 + `B7` 的两条断言（它们守住的是**规则本身**，不是**顺序**）。

#### 10.3.3 实测记录 —— R3′（2026-10-10；AI 改破坏码，本人跑）

**破坏方式**：把 `FirstEnemyOnSegment` 里的**墙判定整段删掉**（`IsSolidAt` 那个 `if` 连同 `return BulletHit{-1, true}`）；为免 /W4 报"未使用形参"，临时留一句 `(void)map;`。

**结果**：`[FAIL] Bullet.WallBlocksEnemyBehindIt` —— 整次运行只有这一条红。

**红集（断言行级，与动手前的预判逐条对照）**：

| 红线 | 实际输出 | 预测 | 一致 |
| --- | --- | --- | --- |
| `test_bullet.cpp:265` `MINI_CHECK(hit.blocked)` | 红 | 红 | ✅ |
| `test_bullet.cpp:266` `MINI_CHECK_EQ(hit.enemyIndex, -1)` | 红，`lhs=0 rhs=-1`（**返回了下标 0** = 命中了墙后的敌人） | 红 | ✅ |
| `test_bullet.cpp:275` `MINI_CHECK(inside.blocked)` | 红 | 红 | ✅ |
| `test_bullet.cpp:276` `MINI_CHECK_EQ(inside.enemyIndex, -1)` | 红，`lhs=0 rhs=-1` | 红 | ✅ |
| `:270` / `:271`（`back`，从墙后朝墙打） | 绿 | 绿 | ✅ |
| `B1`~`B6`、`B8`（7 条用例） | 绿 | 绿 | ✅ |

⇒ **4/4 行逐条吻合**，而且失败信息本身说明了性质：`lhs=0 rhs=-1` = "墙后的敌人**被命中了**" —— 正是墙判定存在的理由。

**还原**：`git checkout -- game/include/game/Bullet.h` 一行回到提交状态（🚨 标记 0 处、`(void)map` 0 处、`IsSolidAt` 回到 2 处）。**未再运行**（还原后与 `2d364ac` 逐字节相同）。

### 10.4 交付后的自检

| # | 自检项 | 怎么查 |
| --- | --- | --- |
| 1 | `engine/` 只动了 `Collision.h` + `KeyCode.h` | `git diff --stat` |
| 2 | `Bullet.h` 不认识 `Game` / 输入 / 屏幕 | `grep #include` |
| 3 | 用例总数 | `mini_tests.exe Nope` 括号里的数 = 预期值 |
| 4 | **既有 `Game` 用例只改了 `startRow` 那一处** | `git diff tests/test_game.cpp` 逐条看 |
| 5 | 两行 HUD 都 < 80 列 | 数格式串（行 1 = 34、行 2 = 44） |
| 6 | **收尾抄录** | 干净构建跑一次，把尾行**照抄**进本表下方（S8 的 A7 教训复现过三次） |

> **收尾实测（本人抄录）**：⬜ 待填

---

## 11. 需要同步的文档

| # | 文档 | 改什么 |
| --- | --- | --- |
| 1 | `doc/MiniEngine_PLAN.md` | S9 小节标 ✅ + 按实际落地重写；`A.2` 目录树加 `game/Bullet.h`；`G` 节 v0.3 行的"② 能射击、能击杀"标 ✅。⚠️ **`A.6「容量与生命周期约定」已在 2026-10-10 提前加好**（S9 只是引用它，收口时不用再动） |
| 2 | `doc/TECH_DEBT.md` | **关 `C11`**（键盘瞄准不需要 `ScreenToWorld` → 触发条件改挂"真要做鼠标/拾取时"）· **关 `C15`**（`Key::K` 已删；`F7` 面板留到 S11）· **关 `C16`**（真删已做 + 用例已补）· 新增：**"HUD 加行的涟漪"若再发生**、**`m_bullets` 自愈压缩无守卫**（R6 若成立）· S9 进度段 |
| 3 | `doc/S9_CHECKLIST.md` | 本文件：进度板打勾 + 落地偏差追加 + 10.2/10.3/10.4 回填 |
| 4 | `doc/S8_CHECKLIST.md` | **不改**（历史留档）；但它的"§10.1 用例 19/22"里那条**留给 S9 的**要在本文件里交代清楚 |
| 5 | `AGENTS.md` | §2 文件表补一行 `doc/S9_CHECKLIST.md`（§7 的三条纪律不动） |
| 6 | `README` / 新文档 | **不创建** |

---

## 12. 与 `PLAN` 原文的差异（写方案时就已知，**不是**落地偏差）

| # | `PLAN` S9 原文 | 本阶段实际 | 为什么 |
| --- | --- | --- | --- |
| 1 | "`Projectile` **组件** + 简单对象池（`std::vector<Projectile>` + `active` 标记 + 定期压缩）" | **子弹 = 纯 `Entity`** + `m_bullets`（id 表，帧末自愈压缩） | 子弹的额外字段是 **0 个**（分叉 ①）；"对象池"由引擎的 `MarkForRemoval`/`FlushRemovals` **已经提供**，不需要另建 |
| 2 | "命中检测用 **AABB**" | **沿线段等距采样** | AABB 是"某一时刻的位置判据"，解决不了"一帧飞 2.25 格"（分叉 ②）；而且敌人是**点模型**（1 格），没有可用的盒 |
| 3 | 坑："遍历中 `push_back` 导致迭代器失效" | **架构上消除** | `ResolveBullets` 遍历 `m_bullets`，不遍历 `m_entities` |
| 4 | 坑："子弹速度过快在低帧率下跳过敌人（如出现 → 子步进）" | 用**采样器**解决，**不动 `MoveAndCollide`** | 子步进解决的是"撞墙"；"命中敌人"是游戏侧判定，两者共用步长常量即可 |
| 5 | （没写）朝向从哪来 | 新增 `Facing8` + "移动即朝向、停下沿用" | `SPACE` 只说了"发射"，没说方向（分叉 ③） |
| 6 | （没写）HUD 加 `kills` 怎么放 | **HUD 从 1 行变 2 行**（`kHudRow` 1→2） | 实测 78/80 无余量，只加 `kills` 会到 89 列（分叉 ④） |
| 7 | "`Kills` +1" | ✅ 做（HUD + 用例） | 一致 |
| 8 | "连按 SPACE 100 次不崩溃、不重复占用内存" | ✅ 作为人眼验收 + `BulletsDoNotAccumulateForever` 用例 | 一致 |

---

## 13. 学习点（写给自己）

- **连续检测 vs 离散采样**：这在项目里是**第二次**出现（S7 的子步进 → S9 的采样器）。两次的结论一样：**"默认参数下不出错"是巧合，正确性要写进结构**（步长 < 目标宽度）。
- **`dt` 的上限如何决定"正确性需要什么"**：`kMaxDelta = 0.05` 这一个数字，直接把"点判定"从"够用"变成"必错"。**先算清楚"一帧最多走多远"，再选算法。**
- **"身份"的三种表达**：字段（`kind`）/ 容器成员（`m_bullets`）/ 约定符号（`symbol == '*'`）。三者的失败模式完全不同 —— 字段会涨结构体、符号会撞车、容器成员会漂移（但可自愈）。
- **空断言比没有更坏**（C18 的延续）：`SweepConsecutiveSamplesNeverSkipATile` 这类"性质用例"要能真的红 —— 写完必须反向验证一次。
- **帧序的落点由"读什么"决定**：判定要读"这一帧子弹实际走了多远"就只能放积分后，而"实际起点"在撞墙时反推是错的 ⇒ **放积分前、用预测线段**。这与 S8"AI 读上一帧位置"是同一种取舍。
