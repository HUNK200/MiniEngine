# S5_CHECKLIST —— World / Entity（`@` 可以移动）施工图

> 本文件是 S5 的**施工图 + 进度板 + 落地记录**。写代码前先读它；
> 实现过程中发现的偏差**追加**到「落地偏差」表，**不许回头改历史**。
>
> 引用格式：`PLAN:n` = `doc/MiniEngine_PLAN.md` 第 n 行；`A.x` / `D.x` / `E-x` = PLAN 的附录/小节/风险编号。

---

## 当前状态（2026-09-30：**步骤 1、2 均已验收全绿**；步骤 3 待开工）

- **S0 / S1 / S2 / S3 / S4 均已完成并验收。** 当时 **58 个 commit**，用例 **68**。
- **步骤 1 已验收（进度板 ✅）** —— 本人回报"构建运行**全绿**"，
  即达到 10.2 的期望：`TileMap` 8 / `EntityId` 2 / 全集 **78 passed** / `ctest` 1/1 /
  `MiniEngine.exe` 现象与 S3 一致（那两步只加库和测试，没碰 `main.cpp`）。
  ⚠️ **未附逐条数字** —— 按 `TECH_DEBT` A7 留下的教训，以后跑这类命令顺手把 `N passed` 抄下来。
  首次构建曾暴露 1 个编译错误（`Entity.h` 裸写 `Vec2` → C3646/C2059），已修，记在**落地偏差 4**。
- **步骤 2 已验收（进度板 ✅）** —— `engine/include/mini/world/World.h`、`engine/src/world/World.cpp`
  （含私有 `IndexOf`；`Spawn` / `MarkForRemoval` **不带** `noexcept`）、`tests/test_world.cpp`
  追加 `World` 10 个用例、`engine/CMakeLists.txt` 加一行 + 注释头。
  本人回报"**全绿通过**"（= `World` 10 passed / 全集 **88 passed** / `ctest` 1/1 / exe 现象仍与 S3 一致）。
  首次构建暴露 1 个编译错误（`test_world.cpp` **漏 include `<mini/world/World.h>`** → C2039/C2873 +
  100 条连带），已修，记在**落地偏差 7**——**而这一条正是施工图 10.4 自检里早就写下的**
  （"三个新头文件都被 `test_world.cpp` include"），我漏跑了那条自检。
- **步骤 3 已验收（✅，2026-09-30）** —— 建 `game/`（`CMakeLists.txt` + `Game.h/.cpp`）、
  `main.cpp` **359 → 47 行**（其中 40 行非空）、`tests/test_game.cpp`（假 `Input` + 3 个用例）、
  顶层 `CMakeLists.txt` 打开 `add_subdirectory(game)`、`tests/CMakeLists.txt` 加 `test_game.cpp`
  与 `MiniEngine::game`。**这是本阶段唯一改变 `MiniEngine.exe` 行为的一步。**
  本人回报"**minitest 都全绿了**" → `Game` 3 passed、全集 88 → **91 passed**。
  施工图 6.4/6.5 的 5 处修正（a~e）+ `Input::IsDown` 的注释改准，记在**落地偏差 8**。
- **步骤 3 的三轮"跑完 exe 再改"（2026-09-30）** —— 都是本人跑出来指出来的，见偏差 11、12、13：
  ① **地图布局重写**：`#` 摆得没规律 → 改成"每 10 格一根的规则柱阵 + 一个不对称地标"；
  ② **手感修正**：`kMoveSpeed` **12 → 20**（"跨一格"要按住的时长从 83ms 降到 50ms，
  轻点才看得见动）+ **速度可用 `MiniEngine.exe 60 30` 覆盖**（`Game(float)` 构造函数，
  非正/NaN 回落默认）+ HUD 加 `v` 一栏；**三条 `Game` 用例的期望值同时改成"由 kMoveSpeed 算"**
  （原来写死了格数，一调速度就误报）。⚠️ 明确**不做**"按下必 +1 格"的补丁 —— 那会破坏 dt 无关性。
  ③ **ConPTY 无真 key-up**（只记档，本人决定先跳过实现）：VS Code 终端里 `Held` 永不出现 →
  一次按键只算一帧的 `dt`。证据：60fps 轻点后 `pos.x = 3.00`、240fps ≈ `2.625`（实测 3.0 / 2.6）。
  → `TECH_DEBT` **C10**；10.2 那条自证已标注"只在提供真 key-up 的终端里成立"。
- ⚠️ **工作区仍未提交**：5 个文档改动（PLAN 的 H 节改长期路线、`AGENTS.md` 新增 §7、
  `TECH_DEBT` 第 4/5 轮状态补齐、`WORK_MODE` / `SKILL` 的交叉引用）+ 步骤 1 的 6 个文件
  （`Entity.h`、`TileMap.h/.cpp`、`test_world.cpp`、两个 `CMakeLists.txt`）+ 步骤 2 的 3 个文件
  （`World.h`、`World.cpp`、新的 `CMakeLists.txt` 那行）。
- 本阶段的**九条决策全部拍板**（第 8 节）—— 前三条是你选的，第 4~7 条是我按项目纪律定的，
  **第 8/9 条（本阶段做测试 + 假 `Input` 替身）是你在原方案之外追加的**（细节见 8.1）。
- **本阶段的性质：S4 是"没有画面的阶段"，S5 正好相反 —— 它是三个"看得见"的变化叠在一起**：
  1. **`@` 从"按一次跳一格"变成"按住连续移动"**（有速度、受 `dt` 影响）
  2. **`math` 那层第一次拿到真实调用者**：`Normalized`（斜向归一化）、`FloorToInt`
     （经 `TileMap::ToIndex` 做世界→tile，以及渲染时做世界→屏幕列 —— **同一件事**）。
     ⚠️ `RoundToInt` **不在其中**：2026-09-30 把取整统一成 floor 之后（7.2 / 落地偏差 3），
     它仍是"只有测试调用"（`TECH_DEBT` B5 的 D 类），处置留到步骤 4
  3. **引擎与游戏第一次分家**：`game/` 静态库建起来，`main.cpp` 从 359 行缩到 ~35 行
- ⚠️ **三条技术债在本阶段同时到期**（触发条件都写着 S5，不是我硬凑）：

  | 债 | 触发条件原文（`TECH_DEBT.md`） | 本阶段怎么关 |
  | --- | --- | --- |
  | **C2** `game/` 静态库未建 | "**S5**。PLAN 与顶层 CMakeLists 的注释都写死了'S5 才建'" | 建 `game/`（`CMakeLists.txt:56-58` 的注释就是"S5 之后再建"） |
  | **C1** `main.cpp` 359 行 | "**S5** 建 `game/` 时" | `PLAN:122` 原文："**等 S5 建了 `game/` 才会回到 <20 行**" |
  | **C4** `DemoGame` 自缓存 `const Input*` | "出现第 2 个需要在 `OnRender` 里读输入的游戏时" | HUD 改成世界坐标面板 → `OnRender` **不再读输入** → 缓存直接消失（决策 2） |

- ⚠️ **有一条 PLAN 里没有、但代码里已经预告的事**：`engine/include/mini/core/Application.h` 的
  `IGame::OnStart()` 注释写着"（**S5 起会建 World**）" —— 本阶段就是兑现它。

### ⚠️ 分工约定（`AGENTS.md` 1.4；本阶段沿用）

**编译与运行由本人执行，AI 只做代码编辑。** AI 不跑 `cmake` / `ctest` / `MiniEngine.exe`，
报告里只能写"等待结果"，不能写"构建通过"。一个步骤的 commit 时机由本人定。

---

## 进度板

> 状态：⬜ 未开始 · 📝 代码已写待验 · ✅ 已验并提交
> ⚠️ 截至 2026-09-30，**S5-1~S5-8 是"已验但未提交"** —— 本阶段全部改动都还在工作区里，提交时机由本人定。
> **建议在步骤 4 之前先按第 11 节提交 3 个 commit**：反向验证要"改坏 → 还原"，有提交基线才能安全用
> `git restore`（现在所有目标文件都是 untracked，`git checkout --` 会直接报错，只能手工改回）。

| # | 任务 | 对应 commit | 状态 |
| --- | --- | --- | --- |
| **S5-1** | `world/Entity.h`：`EntityId` + `Transform` + `Velocity` + `Entity` | commit 1 | ✅ |
| **S5-2** | `world/TileMap.h/.cpp`：`Tile` + `TileMap`（含 `ToIndex` 用 floor、越界 At→Wall） | commit 1 | ✅ |
| **S5-3** | `tests/test_world.cpp`：`TileMap` 8 + `EntityId` 2 个用例；`tests/CMakeLists.txt` 加一行 | commit 1 | ✅ |
| **S5-4** | `world/World.h/.cpp`：Spawn/Find/Entities/Update + 延迟删除 | commit 2 | ✅ |
| **S5-5** | `tests/test_world.cpp`：`World` 10 个用例（含 **dt 无关性**） | commit 2 | ✅ |
| **S5-6** | **建 `game/`**：`CMakeLists.txt` ×2 + `Game.h/.cpp`（搬 `DemoGame` 并改造） | commit 3 | ✅ |
| **S5-7** | `main.cpp` 缩到 ~35 行（只做 argv + 造对象 + Run） | commit 3 | ✅ 实测 **47 行**（40 行非空）：多出的是"为什么这么排"的注释，目标"只做三件事"已达成 |
| **S5-8** | **假 `Input` + `tests/test_game.cpp`（3 个用例）+ `tests/` 链接 `game`**（决策 9） | commit 3 | ✅ 全集 **91 passed** |
| **S5-9** | 反向验证 4 条实跑并**还原现场**（见 10.3.1：4 条全部实测看到红） | commit 3 | ✅ 结果：1 条 6 红 / 2 条编译期红 / 3 条 2 红 / 4 条 1 红；还原后 **90 passed** |
| **S5-10** | docs 收口：PLAN 标 ✅ / A.2 树 / 差异表 / `TECH_DEBT` 关 C1·C2·C4 + 两处修正 | commit 4 | ✅ |

> **本阶段共 10 个任务、4 个 commit、23 个新用例**（68 → 91）。
>
> ⚠️ **收口时全集变成 90**：`math::RoundToInt` 在 S5 失去生产调用者，按 `TECH_DEBT` B2 的纪律
> 于 2026-09-30 删除，连带删掉它那条用例（`MathUtils.RoundToIntHalfAwayFromZero`）→ **91 → 90**。
> 所以本文件里凡是写"**91 passed**"的地方，指的都是**删之前**的数字（保留历史，不逐个改写）。
> ⚠️ 另外：步骤 3 收口时"本人回报『minitest 都全绿了』"确实对应 **91**，不是记忆错误。
>
> ⚠️ 2026-09-30 修正：原文写"22 个新用例（68 → 90）"，**算错了** —— 10.1 的清单是
> `TileMap` 8 + `EntityId` 2 + `World` 10 + `Game` 3 = **23 个**，全集 68 + 23 = **91**。
> 全文 7 处"90"、3 处"22"已一并改准（步骤 1/2 实测：78 → 88，与 68+20 一致）。

### 落地偏差（实现时发现的问题，逐步追加）

| # | 发现在 | 偏差 | 处置 |
| --- | --- | --- | --- |
| 1 | 步骤 1 方案评审（依赖） | 施工图 6.2 的 `TileMap.h` 文件头写"只依赖 `<vector>`/`<cstdint>`（**连 math 都不用**）"，但同一个文件里 `ToIndex` 的实现**就是** `math::FloorToInt` —— 两句自相矛盾 | 本人裁定：**依赖放宽**（只要不引第三方库都行）。`TileMap.h` 改为 include `mini/math/MathUtils.h`，文件头注释改成"只用 `FloorToInt`"。理由：不在这里手写第二份 floor（两份实现会各自漂移） |
| 2 | 步骤 1 方案评审（用例 7） | 施工图 10.1 第 7 条 `IndexIsLinearized` 写"用 **200×200** 的角落与中心"验证线性索引。但方阵下 `w == h`，最容易犯的错 `ty * m_h + tx` **恰好等于**正确公式 → 方阵永远测不出"stride 写成了高" | 改用**非方阵 5×3**，并挑两对"非方阵下会撞车"的格子：`(0,1)` 与 `(3,0)` 共用 slot 3、`(3,1)` 与 `(0,2)` 共用 slot 6 → 公式写错时这两格会变 `Wall`，断言即红 |
| 3 | 步骤 1 收口（取整统一） | `PLAN` A.5 与施工图 7.2 都写"屏幕列 = `round(world)`"，但本清单 10.1 用例 23 的期望值隐含 `floor(world)` —— 两者**差整整一格**，而症状正是 `PLAN` E-8 的"画面整体偏移半格 / 走位比视觉慢一格" | 本人裁定：**统一为 floor**（"哪一格"和"画在哪一列"是同一个概念）。改动 5 处：`PLAN` A.5 的公式与规矩、本清单 7.2、用例 23（名字 `…AtRoundedPosition` → `…AtItsCell`）、`Entity.h` 的 `Transform` 注释、本清单"math 第一次拿到真实调用者"那条。⚠️ **连带后果**：`math::RoundToInt` 在 S5 失去生产调用者 → 处置留到步骤 4 按 B2"没人用就删"评估（详见 7.2 与 10.4），**不在这里私自删** |
| 4 | 步骤 1 **首次构建**（编译错误，实测） | 施工图 6.1 把 `Transform::pos` / `Velocity::perSecond` 写成裸类型名 `Vec2`，但文件在 `namespace mini::world` 里、`Vec2` 在 `mini::math` 里 → **编译不过**。MSVC 报 `C3646 "pos": 未知重写说明符` + `C2059 语法错误 "{"`，**并连带** `static_assert(sizeof(Entity) == 24)` 失败（成员根本没建起来，布局是垃圾） | 两处改成 `mini::math::Vec2`，注释里写明"别为好看简化回 `Vec2`"。依据：同阶段的 `TileMap.h` 也把 math 的东西写成全名（`mini::math::FloorToInt`）。**最值得记的一点**：报错指向 `pos` 与 `{`，**完全不提 `Vec2`** —— 下次见到这串错要先怀疑限定符，而不是去查成员声明 |
| 5 | 步骤 2 方案评审（`noexcept`） | 施工图 6.3 把 `Spawn` 与 `MarkForRemoval` 标成 `noexcept`，但两者都 `push_back` → `bad_alloc` **会抛**。标上就等于声明"分配失败直接 `terminate`" —— 一句不真的话 | 本人裁定（也是"施工图以实际为准"这条规矩的第一次应用）：**去掉这两个 `noexcept`**；`Update` / `Find` / `Entities` / `Map` / `FlushRemovals` **保留**（它们确实不分配）。依据是 B4/B5 立的"声明与注释不许说假话"。**行为不变**，验收数字仍是 88 |
| 6 | 步骤 2 方案评审（`Find` 两版怎么共用逻辑） | 施工图 6.3 只给了 `Find` 的 const / 非 const 两个签名，没定"那段线性扫描怎么共用"。三种做法各有代价：**各写一遍** → 同一段逻辑三处重复（`FlushRemovals` 也要找下标）；**`const_cast` 转调** → 全仓库目前 0 处先例，且每处都要人工证明合法；**模板 `FindImpl<Self>`** → 为一个 4 行函数引入模板，违反"不做架构漂亮的东西" | 本人裁定：抽**私有 `IndexOf(id) -> std::size_t`**，找不到（或 id 无效）返回 `m_entities.size()` 当哨兵 —— 一份扫描、三个调用点、零 `const_cast`、零模板。**不是为将来预留**：`FlushRemovals` 就是今天已有的第二个调用者。已同步改准 6.3 与 7.3 的伪代码；顺手修掉 6.3 的两处文档瑕疵（重复的 `namespace mini::world {`、缺 `<cstddef>`） |
| 7 | 步骤 2 **首次构建**（编译错误，实测） | `tests/test_world.cpp` 加了 `using mini::world::World;` 与 10 个用例，**却忘了 include `<mini/world/World.h>`**。报错是 `C2039 "World": 不是 "mini::world" 的成员` + `C2873 "World": 符号不能用在 using 声明中`，然后 100+ 条 `C2065 "world": 未声明的标识符`（`C1003 错误计数超过 100` 才停） | 加一行 include 即可。**⚠️ 这一条是施工图 10.4 早就预告过的**（"三个新头文件**都被 `test_world.cpp` include** —— 否则没编译过，**坑 #12 的伪装版**"），**我却没跑这条自检**。教训：10.4 那三条自检要在**提交前**逐条跑，不能只挑"看着像能出问题"的跑。另：报错里那条 `TileMap.h:33 参见"mini::world"的声明` 是**误导线索** —— 它只说明命名空间见过，不代表类型存在 |
| 8 | 步骤 3 落地（施工图 6.4/6.5 的 **5 处修正** + 2 处顺手改准） | 施工图 6.4 的 `Game` 只列了 `m_world`/`m_player`/`m_elapsed` 和 private 的 `kMoveSpeed`，但：<br>**(a)** 缺 `m_quit` —— `ShouldQuit()` 没有数据来源；<br>**(b)** 说 HUD 显示 `pos/tile/Σdt/fps`，可类里没有 fps、`OnRender` 也拿不到 `FrameContext`；<br>**(c)** `#include "mini/platform/Input.h"` 在"HUD 不再读输入"之后已经用不到；<br>**(d)** 用例 23 要"玩家在 `(5.5,3.5)`"，但**没有任何公开接口能设置或读取玩家位置**，而 10.4 又要求"单测只碰公开接口"；<br>**(e)** 用例 21/22 要的"位移"同样只能间接观察 | 本人裁定（"施工图以实际为准"）：<br>**(a)** 加 `bool m_quit`，`Q` 置位（照 S3 的 `DemoGame`）；<br>**(b)** **HUD 不显示 fps**（顶栏第 0 行已经有 `FPS\|Frame\|Max\|Work\|Rows`，同屏两个 FPS 是噪声）→ HUD 只显示 `pos / tile / sum dt`（用 ASCII，避开 936 码页下 `Σ` 的乱码风险）；<br>**(c)** 去掉该 include —— 它消失本身就是 **C4 已关闭**的证据；<br>**(d)** 把 `kMoveSpeed` / `kSpawnX` / `kSpawnY` 提为**公有 `constexpr`**（注释写明"改它必须同时改 `tests/test_game.cpp`"），用例据此算期望格：`floor(2.5 + 12×0.35) = 6`；<br>**(e)** 测试改成**扫 `FrameBuffer` 上的 `@`** 观察落点 —— 只依赖公开的 `FrameBuffer::At` 与 `Game` 的公开常量<br>两处顺手改准：<br>· 10.2 观察点原写"`Q` / `Esc` / `Ctrl+C` / 关窗口 正常退出"，但按 S2 决策 **Esc 不退出**（它从 S10 起要用来关菜单）→ 已改成"`Q` / `Ctrl+C` / 关窗口"；<br>· `Input::IsDown` 的注释原写"生产 0 调用 / S9 的持续射击会用到"，实际 **S5 的 WASD 就用上了**（`Game::OnUpdate` 用 `IsDown` 做"按住就走"）→ 已改准（10.4 早就列了这条；`TECH_DEBT` B5 的 A 类同一条留到步骤 4 收口一起改） |
| 9 | 步骤 3 **首次构建**（编译错误，实测） | `tests/test_game.cpp` 用了裸名 `Game`，**却没写 `using game::Game;`**（`Game` 在命名空间 `game` 里）。报错是 `C2065 "Game": 未声明的标识符`；随后 `Game game;` 声明失败 → 后面 `game.OnStart()` 里的 `game` 退回去解释成**命名空间** → 一串 `C2882 "game": 在表达式中非法使用命名空间标识符` + `C2653 "Game": 不是类或命名空间名称` | 加一行 `using game::Game;`（与 `test_world.cpp` 的 `using mini::world::World;` 同款）。<br>⚠️ **与偏差 7 同族、但机制不同**：7 是**包含**问题（漏 `#include`），这条是**名字查找**问题（头文件在、include 目录也在 —— 我从生成的 `mini_tests.vcxproj` 里确认过 `game\include` 确实在列表里）。<br>两次都发生在测试文件、都表现为"报错指向上游下游"：**下一条自检**应该是 `grep -n` 每个新测试文件里裸写的类型名，确认每个都有一行 `using` 或全限定 |
| 10 | 步骤 3 **首次测试运行**（用例逻辑错误，实测） | `Game.DiagonalMoveIsNotFaster` 里我写了 `MINI_CHECK(diagonal.dy < straight.dy)` —— 直向那趟只按 `D`，它的 `dy` **恒为 0**，而斜向 `dy ≈ 8`，于是 `8 < 0` 永远为假：**这条断言永远红**。同一次运行里其余 5 条断言**全绿** | 改成与"直向 1 秒的位移"（`kMoveSpeed = 12`）比较：`diagonal.dx < oneSecondCells` / `diagonal.dy < oneSecondCells`。<br>⚠️ **这条错的正面价值**：它反证了另外 5 条断言真的在测东西 —— `straight.dx == 12`（1 秒正好 12 格）、`straight.dy == 0`、`diagonal.dx/dy ∈ [8,9]`（12/√2 ≈ 8.49 → floor 8）**全部通过**，说明 **dt 积分、斜向 `Normalized()`、floor 落点三件事都是对的**。错的只是我拿了"另一次运行的分量"当基准。<br>教训：**跨两次运行比较时，要说清"跟谁比"** —— "没动"（0）会被误当成"更小" |
| 11 | 步骤 3 **首次跑 exe**（肉眼，实测） | demo 地图的 `#` 看着**毫无规律**（本人原话："# 是不是应该规律摆放？"）。原因不是 bug 而是设计：我把"一圈边框"画在**200×200 地图的外沿**，而开局视野只有左上角 80×24 → 边框只露左边一条；另外三根柱子（`x=10` 竖条 / `y=8` 横条 / `x=35` 竖条）是我**随手挑的坐标**，彼此不对称、也无周期，看着就是"东一块西一块" | 重写 `BuildDemoMap` 的 2)、3) 两节：<br>· **规则柱阵**：`x`、`y` 都取 10 的倍数放柱子（`for py: for px:`）→ 开局视野里是 7 列 × 2 行的整齐点阵，"一眼看出是故意的"，同时也是 **S6 相机的连续参照物**；<br>· **不对称地标**：`x=30..36, y=5..8` 一栋 7×4 的"楼"（y 取 5 而不是 6：下边界要与柱阵那一行 `y=10` **隔开一格**，否则屏幕上"楼"和正下方那颗柱子连成一串，看着像画错了） —— 因为规则点阵**平移一个周期（10 格）后会重合**，光靠它分不清"画面动了"还是"没动、只是数字在跳"，有个独一份的方块才能一眼判断位移；<br>· 外沿边框保留，并在注释里写明"只有左边那条看得见、右边要等 S6 相机"。<br>教训：**"随便放几个"和"看起来有设计"的区别要提前想到** —— 演示内容本身就是 S5 的交付物之一，不能只求"有东西可画" |
| 12 | 步骤 3 **第二次跑 exe**（手感，实测） | 本人反馈："**反应比较慢**；之前的模式按下就有反应，现在必须持续按下才会有反应"。核对后确认 **不是 bug、是设计**（10.2 的验收原文就是"`@` 连续移动（不是按一次跳一格）"）。根因可量化：S3 是 `Pressed` **边沿**触发（按下即 +1 格），按住时还有 Windows 自动重复（约 30 次/秒）兜着；S5 是 `IsDown` **电平**触发 + 速度积分 —— **"跨一格"要按住的时长 = 1/kMoveSpeed = 83ms**，而人手一次轻点只有 30~80ms → 位移 0.4~1 格，常常跨不过一格 → 看着"按了没反应" | 三处改动：<br>① `kMoveSpeed` **12 → 20**（跨一格 50ms，轻点基本都能看见走一格；穿一屏 80 列约 4 秒）；<br>② **速度做成命令行第二个参数**（`MiniEngine.exe 60 30`）—— 手感数字应该试出来而不是猜出来。`Game` 加 `explicit Game(float = kMoveSpeed) noexcept`（非正数 / NaN 回落默认值），HUD 加 `v` 一栏，好一眼核对 `pos ≈ v × sum dt`；<br>③ **测试改成"与速度无关"**：用例里的格数期望值一律由 `Game::kMoveSpeed` / `kSpawnX` 算出（原先把 `8~9`、`start.x+1`、列 `6` 写死了 —— 一调速度就误报，这次正好暴露出来）。<br>⚠️ 明确**不做**"按下必 +1 格"的补丁：那段位移与 dt 无关，会直接破坏 S5 的核心验收与反向验证 4 的语义。<br>教训：**手感也是需求** —— 换控制模型时必须把"跨一格要按住多久"这类可量化代价提前讲出来，不能只说"改成连续移动" |
| 13 | 步骤 3 **第三次跑 exe**（终端环境，实测） | 本人给出对照：`60 30` 轻点即动，`240 30` 要按住。**这次现象与帧率有关** —— 我上一轮"输入链路与帧率无关"的判断**被推翻**（那一轮我只读代码就下了结论）。追查到根因：**VS Code 内置终端是 ConPTY，它不提供真正的 key-up**，每个按键被合成为"down + 立刻 up" → `Get()` 永远是 `Pressed`、**`Held` 永不出现** → 一次轻点只被算作**一帧**的 `dt`。反算验证：预测 60fps 轻点后 `pos.x = 3.00`、240fps ≈ `2.625`，**实测 3.0 / 2.6** —— 完全吻合 | **本人决定：先跳过实现**（原话"如果太麻烦就先不考虑实现，跳过"）。已记入 `TECH_DEBT` **C10**（含证据表、两条候选修法 A/B 与成本、四个必须先实测的未知点），触发条件：想在 VS Code 里正常玩 / S9 做持续瞄准射击 / 有人再报同样的手感问题。<br>同时**必须标注验收口径**：10.2 那条自证 `pos ≈ v × sum dt` **只在提供真 key-up 的终端里成立**；在 ConPTY 里按住的位移只有 `自动重复速率 × 一帧的 dt`（60fps 约 1/2、240fps 约 1/8）。<br>⚠️ 也解释了**为什么 S3 没暴露**：S3 用 `Pressed`（边沿），而"合成对"恰好产出一个 `Pressed` —— 坏掉的输入与旧语义正好吻合，换成 `IsDown`（电平）才露出来。<br>教训：**"现象与帧率有关"是很有价值的一条线索**，我不该只凭读代码就断言"无关" |

---

## 1. 目标与**明确不做**

**目标**（`PLAN:465` 原文）：`TileMap`(200×200) + 简单 `Entity` 结构体 + `World` 容器 + 移动逻辑。

**可运行效果**（PLAN）：`@` 按 WASD 在地图上移动，速度受 `dt` 影响。

### 明确不做（防止"顺手多做一点"）

| 不做 | 留给谁 |
| --- | --- |
| **碰撞** —— `@` 能穿墙、能走出地图 | **S7**（本阶段故意留着这个缺口，它是 S7 的可见入口） |
| **相机 / 视口** —— 视口原点固定在世界的 `(0,0)`，往右走约 40 格 `@` 就移出屏幕 | **S6** |
| ECS / Component / System | **永远不做**（`PLAN:920` D.1："停在 D-1，永远不到 D-3"） |
| 地图文件格式 / 程序化生成 / 随机地图 | 不做（只有一个**确定性**的 demo 布局） |
| 空间索引（四叉树 / 网格分桶） | PLAN 明确不做（S7 用 tile 表天然达成） |
| 固定步长 / 累加器 / 插值 | `PLAN:982` 已定：纯运动学 + 可变 dt |
| `Health` / `Collider` / `AiState` 字段 | S7~S9 按需加（D-1 的演进路径） |
| 实体更新的**依赖顺序**（谁先动谁后动） | 现在只有一个会动的实体，顺序还没有观测差异；S8 加 AI 时才需要 |

## 2. 交付物清单

**新增（engine 层）**

| 文件 | 行数（估） | 内容 |
| --- | --- | --- |
| `engine/include/mini/world/Entity.h` | ~45 | `EntityId` / `Transform` / `Velocity` / `Entity` |
| `engine/include/mini/world/TileMap.h` | ~55 | `Tile` 枚举 + `TileMap` 声明 |
| `engine/src/world/TileMap.cpp` | ~45 | 构造 / `At` / `Set` / `ToIndex` |
| `engine/include/mini/world/World.h` | ~80 | `World` 声明（含延迟删除 + 私有 `IndexOf`） |
| `engine/src/world/World.cpp` | ~85 | `Spawn` / `Find` ×2 / `IndexOf` / `Entities` ×2 / `Update` / `Mark` / `Flush` |

**新增（game 层 + 构建）**

| 文件 | 行数（估） | 内容 |
| --- | --- | --- |
| `game/CMakeLists.txt` | ~20 | `add_library(game STATIC ...)` + `MiniEngine::game` ALIAS |
| `game/include/game/Game.h` | ~45 | `IGame` 的实现声明 |
| `game/src/Game.cpp` | ~200 | 从 `main.cpp` 的 `DemoGame` 搬来 + 改造成 S5 玩法 |

**修改**

| 文件 | 改动 |
| --- | --- |
| `CMakeLists.txt`（顶层） | 打开 `add_subdirectory(game)`；`MiniEngine` 改成链接 `MiniEngine::game` |
| `tests/CMakeLists.txt` | 加 `test_world.cpp`、`test_game.cpp`；并 `target_link_libraries(... MiniEngine::game)`（决策 9） |
| `main.cpp` | **359 → ~35 行**（只留 argv 解析 + 造对象 + `Run`） |
| `engine/CMakeLists.txt` | 源文件列表加 `src/world/TileMap.cpp`、`src/world/World.cpp`（**头文件不用加**，见坑 #12） |

**新增（测试与文档）**

| 文件 | 内容 |
| --- | --- |
| `tests/test_world.cpp` | 3 个 suite、**20 个用例**（`TileMap` 8 / `EntityId` 2 / `World` 10） |
| `tests/test_game.cpp` | 假 `Input`（~15 行）+ `Game` suite、**3 个用例**（决策 9） |
| `doc/S5_CHECKLIST.md` | 本文件 |

## 3. 依赖规则（本阶段**新增 3 条边**）

```text
main.cpp  →  game  →  engine  →  标准库
               ↑        ↑
            tests ──────┘        ← tests 同时链 game 与 engine（无环）
                        ↑
             engine/mini/world/   ← 新层，与 core / render 平级
```

| 新边 | 说明 |
| --- | --- |
| `main → game` | `main.cpp` 只 include `game/Game.h`（不再自己 define `DemoGame`） |
| `game → engine` | `game` 用 `mini::core::IGame` / `world::World` / `render::FrameBuffer` / `platform::Input` |
| `tests → game` | 为了让 `Game::OnUpdate` / `OnRender` 可离线测（决策 9）。已核对**无环**：`tests → game → engine`，且 `engine` 绝不反向 include `game`（PLAN A.3） |

**`world` 层的自约束（写进头注释，并用 grep 自检）**：

- `world/` **不认识 `platform`** —— 它不读输入（输入由 `game` 翻译成速度）
- `world/` **不认识 `render`** —— 它不画东西（渲染由 `game` 遍历实体来做）
- → 所以 `World`/`TileMap` **完全可离线单测**，不需要终端（这正是 `TECH_DEBT` C7 那条精神的延续）
- `math` 仍然 header-only、零依赖，可以被任何层包含（`PLAN` A.3）

## 4. 数据结构与所有权

| 问题 | 选择 | 代价 / 理由 |
| --- | --- | --- |
| 实体存在哪 | `std::vector<Entity>` 按**值**存 | 连续、cache 友好、无堆碎片；**代价**：任何 `Spawn` 都可能让 `Entity*` 失效 |
| 谁拥有 | `World` 独占 `vector`；`Game` 拥有 `World`（直接成员，不是 `unique_ptr`） | 无共享、无 `shared_ptr`、生命周期跟随 `Application::Run` 的栈帧 |
| 怎么长期引用实体 | 一律用 **`EntityId`**（值语义，4 字节） | 指针会悬空（vector 扩容 / swap-erase） |
| 怎么短期引用 | `Find()` 返回的 `Entity*` **只在本次调用内用** | 跨帧持有 = 悬空风险 |
| id 从哪来 | `m_nextId++`，从 **1** 开始，**永不复用**；`0` 保留给"无效" | 复用会让**悬空 id 静默指向新实体**（找错比找不到更难查）；32 位够 40 亿次 Spawn |
| 怎么查 | **线性扫**（`<50` 个实体） | 不引入 `unordered_map`（PLAN：不做"架构漂亮"的东西） |
| 怎么删 | **延迟删除**：`MarkForRemoval` 只记 id，`FlushRemovals` 在遍历之外做 | 见第 7.3 节与 `PLAN:967` 的 E-9 |
| 地图 | `std::vector<Tile>`（`200×200 = 40 KB`），线性化 `index = y*w + x` | 与 `FrameBuffer` 同一套路；`Tile` 是 `uint8_t` |
| 一次遍历怎么暴露 | `std::span<Entity>` | C++20；**不带所有权、不带长度信息以外的语义**；⚠️ 任何 `Spawn` 后失效 |
| `Entity` 大小 | `static_assert(sizeof(Entity) == ...)` 自检 | 照 S4 的 `Vec2`/`AABB`/`Ray` 先例（坑 #18） |
| 视口（S5 版） | **没有 Camera**，视口原点恒为世界 `(0,0)` | 这是 S6 的活；S5 的"视口偏移"只是渲染时的一个减法<br>✅ **2026-10-02 S6 已做**：`render/Camera.h` + `Game` 接入（见 `doc/S6_CHECKLIST.md`） |

## 5. 逐文件实施步骤（每步都立刻跑一次）

| 步 | 动作 | 立刻跑 | 期望 |
| --- | --- | --- | --- |
| **1** | 写 `Entity.h` + `TileMap.h/.cpp` + `tests/test_world.cpp`（前 10 个用例）+ `engine/CMakeLists.txt` 加 1 个源文件 + `tests/CMakeLists.txt` 加 1 个文件 | `mini_tests.exe TileMap` / `EntityId` / 全集 | 10 passed / 68 → **78** |
| **2** | 写 `World.h/.cpp` + 后 10 个用例 + `engine/CMakeLists.txt` 加 1 个源文件 | `mini_tests.exe World` / 全集 | 10 passed / → **88** |
| **3** | 建 `game/`（两个 CMakeLists + `Game.h/.cpp`）、`main.cpp` 缩到 ~35 行、顶层 CMakeLists 打开 `add_subdirectory(game)`、**假 `Input` + `test_game.cpp`（3 个用例）+ `tests/` 链 `game`** | `mini_tests.exe Game` / 全集 / `MiniEngine.exe` | 3 passed / 88 → **91**；**`@` 按住 WASD 连续移动**；HUD 显示 pos/tile/Σdt |
| **4** | 反向验证 4 条（10.3）+ docs 收口 | 见 10.3 | 每条都看到红、现场还原、全集回到 91 |

> ⚠️ **步骤 3 是本阶段唯一"会改变 exe 行为"的一步** —— 前两步只加库和测试，
> `MiniEngine.exe` 的现象**必须逐字符不变**（还是 S3 的按键面板）。这条要单独确认。

## 6. 接口设计（施工图）

### 6.1 `world/Entity.h`

```cpp
#pragma once

// ============================================================
// world/Entity.h —— 实体与它的两个最小组件（S5）
//
// 这一层干什么：定义"世界里有个东西"这件事的最小形态。
// 依赖谁：math/Vec2.h（以及它带的 MathUtils.h）+ <cstdint>。
// 被谁用（已核对）：game/Game.cpp 是本阶段唯一的调用者；
//   之后的 S7(碰撞) / S8(敌人) / S9(子弹) 都会加字段进来。
//
// ⚠️ **这是 PLAN D.1 的 D-0「单结构体」形态**：
//   所有实体都有全部字段，用 active 区分，**不做 ECS、不做组件指针**。
//   PLAN:920 的结论是"停在 D-1，永远不到 D-3"，本文件就是那个起点。
// ============================================================

#include "mini/math/Vec2.h"

#include <cstdint>

namespace mini::world {

// 实体的**句柄**（不是索引、不是指针）。0 = 无效。
//
// ⚠️ 为什么不用 `std::vector` 下标：`swap-erase` 之后下标会指向**另一个**实体，
//    而且是**静默**错——最难查的那一类。自增 id 不复用，于是悬空 id 只会
//    "找不到"（Find 返回 nullptr），不会"找错"。
struct EntityId {
    std::uint32_t value = 0;

    [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }
    friend constexpr bool operator==(EntityId, EntityId) noexcept = default;
};

// 位置。单位 = **格（float）**，原点 = 地图左上角，X 右 Y 下。
// ⚠️ `(0.5, 0.5)` 是**第 1 格（索引 (0,0)）的中心**（PLAN A.5）；取整一律 floor（见 7.2）。
// ⚠️ 类型名要写全 `mini::math::Vec2`：本文件在 `mini::world` 里，裸写 `Vec2` **找不到**
//    （落地偏差 4：MSVC 报 C3646「未知重写说明符」，报错指向成员名而不是类型名）。
struct Transform {
    mini::math::Vec2 pos{};
};

// 速度。单位 = **格/秒** —— 所以 `pos += perSecond * dt` 得到本帧位移。
// 用"格/秒"而不是"格/帧"是本阶段的核心：后者会让速度随帧率变。
struct Velocity {
    mini::math::Vec2 perSecond{};
};

struct Entity {
    EntityId  id{};
    Transform transform{};
    Velocity  velocity{};
    char      symbol = '?';      // 画在屏幕上用哪个字符
    bool      active = true;     // D-1 的"用标记区分"，不是"按类型分容器"
};

} // namespace mini::world
```

### 6.2 `world/TileMap.h`

```cpp
#pragma once

// ============================================================
// world/TileMap.h —— 大地图的格子表（S5）
//
// 这一层干什么：一张 W×H 的格子表，回答"第 (tx,ty) 格是什么"。
// 依赖谁：只依赖 <vector> / <cstdint>（**连 math 都不用**）。
// 被谁用：world::World 持有它；game/Game.cpp 用它画地图内容。
//
// ⚠️ 两条写死的约定：
//   1. **越界一律当 Wall**（At 越界返回 Wall、不抛异常）—— 世界外都是墙，
//      S7 的碰撞直接受益（角色永远走不出地图）。
//      代价：S6 的相机**不能靠 At() 判断边界**，得自己按 Width/Height 夹取。
//   2. 世界坐标 → tile 下标用 **floor，不是 (int) 截断**（PLAN A.5 / E-8）：
//      负数下 `(int)-0.1f == 0`，会指向**上一格**，表现为"画面整体偏移半格"。
// ============================================================

#include <cstdint>
#include <vector>

namespace mini::world {

enum class Tile : std::uint8_t {
    Empty = 0,   // 可走（S7 之后）
    Wall  = 1,   // 不可走
};

class TileMap {
public:
    // w/h 非正时抛 std::invalid_argument（与 FrameBuffer 同一规矩：
    // 0 宽的地图没有意义，且要挡住"从终端读到 0 就一路传下来"）
    TileMap(int w, int h, Tile fill = Tile::Empty);

    [[nodiscard]] int Width()  const noexcept;
    [[nodiscard]] int Height() const noexcept;
    [[nodiscard]] bool InBounds(int tx, int ty) const noexcept;

    // ⚠️ 越界 → **Wall**（见文件头约定 1）
    [[nodiscard]] Tile At(int tx, int ty) const noexcept;

    // 越界**静默忽略**（与 FrameBuffer::Set 同一条规矩：S5 之后实体天天走到边缘）
    void Set(int tx, int ty, Tile t) noexcept;

    // 世界坐标（格，float）→ tile 下标：**floor**（见文件头约定 2）
    // 内部就是 math::FloorToInt —— S4 那 8 个"只有测试调用"的工具里第一个上生产
    [[nodiscard]] static constexpr int ToIndex(float worldCoord) noexcept;

private:
    int               m_w = 0;
    int               m_h = 0;
    std::vector<Tile> m_tiles;   // 线性化：index = y * m_w + x
};

} // namespace mini::world
```

### 6.3 `world/World.h`

```cpp
#pragma once

#include "mini/world/Entity.h"
#include "mini/world/TileMap.h"

#include <cstddef>    // std::size_t（私有 IndexOf 的返回类型）
#include <cstdint>
#include <span>
#include <vector>

namespace mini::world {

// ============================================================
// world/World.h —— 实体容器 + 每帧推进（S5）
//
// 这一层干什么：装实体、按 id 找、每帧把速度积分成位移、以及延迟删除。
// **它不读输入、不画东西、不认识终端** → 可完全离线单测。
//
// ⚠️ 本阶段唯一的"物理"就是 `pos += velocity * dt`（PLAN:982：纯运动学 + AABB，
//    无重力、无阻尼、无迭代求解）。S7 的分轴碰撞会插在同一个循环里。
//
// ⚠️ **遍历中禁止 Spawn / 删除**：
//    · 删除 → 走 MarkForRemoval + FlushRemovals（遍历之外做）
//    · Spawn → 会让 Entities() 的 span 与 Find() 的指针**一起失效**（vector 可能扩容）；
//      FlushRemovals 的 swap-erase 同样让它们失效
//    这条规矩有 3 个用例钉着，不是"注释说说"（PLAN:967 的 E-9）
// ============================================================

class World {
public:
    World(int mapW, int mapH);

    // 分配一个新 id、推入、返回它。prototype 的 id 字段会被**覆盖**。
    // ⚠️ **刻意不标 noexcept**：内部 push_back 在 bad_alloc 时会抛 ——
    //    标上就等于声明"分配失败直接 terminate"（施工图原文标了，已改：落地偏差 5）
    [[nodiscard]] EntityId Spawn(const Entity& prototype);

    // 线性扫。找不到返回 nullptr（**永远不会"找到错的"**，见 EntityId 的注释）。
    // ⚠️ 返回的指针**只在本次调用内有效**：任何 Spawn / FlushRemovals 之后失效。
    //
    // 为什么两版都要：只有非 const 版 → `const World&` 上根本调不了（HUD / Debug Overlay /
    //   单测里的 const 引用全被挡住）；只有 const 版 → 改不了速度（Game::OnUpdate 没得写）。
    // 为什么两版**不各写一遍**：共用一个私有 IndexOf()，而 FlushRemovals 也要"找下标"
    //   → 一份扫描逻辑、三个调用点（落地偏差 6）
    [[nodiscard]] Entity*       Find(EntityId id) noexcept;
    [[nodiscard]] const Entity* Find(EntityId id) const noexcept;

    // ⚠️ 返回的 span 在**任何 Spawn 之后失效**（vector 可能扩容）。别在持 span 时 Spawn。
    [[nodiscard]] std::span<Entity>       Entities()       noexcept;
    [[nodiscard]] std::span<const Entity> Entities() const noexcept;

    [[nodiscard]] TileMap&       Map()       noexcept;
    [[nodiscard]] const TileMap& Map() const noexcept;

    // 本阶段唯一的"物理"：对每个 active 实体做 pos += perSecond * dt
    // 顺序：**先由调用方写好速度，再调它**（见 7.1 的帧序）
    void Update(float dt) noexcept;

    // 延迟删除：只登记，不动容器。重复登记同一个 id 是**幂等**的。
    // ⚠️ **刻意不标 noexcept**：push_back 会抛（同 Spawn，落地偏差 5）
    void MarkForRemoval(EntityId id);

    // 真正删除：swap-erase（把队尾搬到空位）。
    // ⚠️ **会打乱实体顺序** —— 任何依赖顺序的代码都是 bug（用例钉着）
    void FlushRemovals() noexcept;

private:
    // 线性扫，返回实体在 m_entities 里的下标；**找不到、或 id 无效 → 返回 m_entities.size()**
    // （索引永远 < size，所以拿 size() 当"没找到"的哨兵，不必再引入一个 npos 常量）。
    // 谁用：Find 的非 const 版、Find 的 const 版、FlushRemovals —— 一份扫描，三个调用点。
    [[nodiscard]] std::size_t IndexOf(EntityId id) const noexcept;

    TileMap               m_map;
    std::vector<Entity>   m_entities;
    std::vector<EntityId> m_pendingRemove;   // 遍历之外才消费
    std::uint32_t         m_nextId = 1;      // 0 留给"无效 id"；**只增不减**
};

} // namespace mini::world
```

### 6.4 `game/Game.h`（game 层）

```cpp
#pragma once

#include "mini/core/Application.h"
#include "mini/render/FrameBuffer.h"
#include "mini/world/World.h"

// ============================================================
// game/Game.h —— 本项目的游戏侧（S5 第一次有"游戏"这个东西）
//
// 这一层干什么：实现 engine 的 IGame（OnStart/OnUpdate/OnRender/ShouldQuit），
//   把"输入"翻译成"速度"、把"世界"画成"屏幕"。
// 依赖谁：engine 的 core / world / render / platform + math。
// **engine 绝不 include game**（PLAN A.3），所以这里是单向的。
//
// ⚠️ 与 S3 的 DemoGame（原在 main.cpp）的差别，只有三件事：
//   1. `@` 从"按一次跳一格"变成"按住连续移动"（速度 × dt）
//   2. HUD 从"15 键四态面板"换成**世界坐标面板**（pos / tile / Σdt）
//      → 于是 OnRender **不再读输入**，那条 `const Input*` 缓存（TECH_DEBT C4）直接消失
//   3. 有一条真正的开局流程（OnStart 里建地图、Spawn 玩家）
//
// ⚠️ **不 include `platform/Input.h`**（落地偏差 8c）：OnUpdate 只从 FrameContext 拿
//   `const Input&`（Application.h 已前向声明），OnRender 完全不碰输入 —— 这个 include
//   消失了本身就是"C4 已关闭"的证据。
// ============================================================

namespace game {

class Game final : public mini::core::IGame {
public:
    void OnStart() override;
    void OnUpdate(const mini::core::FrameContext& ctx) override;
    void OnRender(mini::render::FrameBuffer& fb) override;
    [[nodiscard]] bool ShouldQuit() const override;

    // —— 下面三个数**公有** constexpr，是刻意的（落地偏差 8d）——
    //    单测要用它们算期望值（"1 秒走几格" / "起点该画在哪一格"），
    //    而 10.4 要求"单测只碰公开接口"。**用例里不许写死格数**，必须由常量算出来。
    //    2026-09-30：值从 12 调到 **20**（"跨一格"要按住的时长 = 1/kMoveSpeed，12 格/秒是
    //    83ms，人手轻点 30~80ms 跨不过一格 → 看着像"按了没反应"）。见落地偏差 12。
    static constexpr float kMoveSpeed = 20.0f;   // 格/秒（**默认值**，可被构造参数覆盖）
    static constexpr float kSpawnX    = 2.5f;    // 开局位置：格中心（world 坐标）
    static constexpr float kSpawnY    = 2.5f;

    // 速度可覆盖：`MiniEngine.exe 60 30` 的第二个参数 = 格/秒（手感要试出来，不靠猜）。
    // 非正数 / NaN → 回落到 kMoveSpeed（见 Game.cpp）。
    explicit Game(float moveSpeed = kMoveSpeed) noexcept;

private:
    static constexpr int kWorldW = 200;   // PLAN：World = 200×200
    static constexpr int kWorldH = 200;

    mini::world::World    m_world{kWorldW, kWorldH};
    mini::world::EntityId m_player{};         // 0 = 还没出生（OnStart 之前）
    float                 m_elapsed = 0.0f;   // Σdt（秒）—— 验收自证用（见 10.2）
    bool                  m_quit = false;     // Q 退出（ShouldQuit 的来源，落地偏差 8a）

    // 地图布局：**确定性**写死（不随机、不读文件）。
    // 内容 = 地图外沿一圈墙 + **每 10 格一根的规则柱阵** + 一个不对称地标（"一栋楼"）。
    // ⚠️ 规则性是有要求的（2026-09-30 实测被指出"# 摆得没规律"）→ 见落地偏差 11。
    void BuildDemoMap();

    // 渲染：S5 **没有相机**，视口原点恒为世界 (0,0)；三个都是 const
    // → 用的是 `World` 的 **const 版** Find()/Entities()/Map()
    void DrawMap(mini::render::FrameBuffer& fb) const;
    void DrawEntities(mini::render::FrameBuffer& fb) const;
    void DrawHud(mini::render::FrameBuffer& fb) const;
};

} // namespace game
```

### 6.5 `main.cpp` 的最终形态

```cpp
// 359 行 → ~35 行。只剩下：解析 argv、造对象、交给 Application::Run
#include <game/Game.h>
#include <mini/core/Application.h>

#include <cstdio>
#include <cstdlib>
#include <exception>

int main(int argc, char** argv)
{
    mini::core::AppConfig cfg;          // 宽高 0 = 用终端可视区
    if (argc > 1) {
        cfg.targetFps = std::atoi(argv[1]);   // 0/负数 = 不限帧（验证限帧用）
    }

    try {
        game::Game theGame;
        mini::core::Application app(cfg);
        app.Run(theGame);               // 阻塞；返回前 theGame 不能析构
    } catch (const std::exception& e) {
        std::fprintf(stderr, "fatal: %s\n", e.what());
        return 1;
    }
    return 0;
}
```

### 6.6 文件骨架

四个新头文件都按同一形状（与 `InputState.h` / `Time.h` / `AABB.h` 的现有风格一致）：

```text
#pragma once
// 文件头注释：这一层干什么、依赖谁、被谁用、有哪些"约定死在注释里"的东西

#include <必要的标准库>

namespace mini::world {   // 或 mini::core / game

// 每个公开接口上方一行注释说明"为什么存在、谁会用"

} // namespace mini::world
```

## 7. 算法与伪代码

### 7.1 帧序（**硬约束**）

`Game::OnUpdate(ctx)` 里六步，顺序不能换：

```text
1. dir = 读 WASD / 方向键（按住，不是"按下"）
2. if (dir != {0,0})  dir = dir.Normalized()      ← 不归一化的话斜向快 √2 倍
3. player->velocity.perSecond = dir * m_moveSpeed   ← 不是 kMoveSpeed：后者只是**默认值**
4. m_elapsed += ctx.deltaSeconds
5. m_world.Update(ctx.deltaSeconds)               ← ⚠️ 必须在 3 之后
6. m_world.FlushRemovals()                        ← S5 恒为空；S9 才真用
```

**为什么 3 必须在 5 之前**：`Update` 读的是**本帧刚写进去的**速度。
反过来的现象是"按一下动一下、但慢一帧"——看着像手感问题，实际是顺序问题
（PLAN 的引擎知识条目"更新顺序的重要性"）。

**另两条来自 S3 的帧序约定**（不改，只是遵守）：

- `ctx.deltaSeconds` 已经被 `ClampDelta` 夹过（`kMaxDelta = 0.05f`）→ **不要再夹一次**
- 顶栏占**第 0 行** → 游戏的一切从**第 1 行**开始

### 7.2 世界坐标 → tile / 屏幕

```text
tile 下标      = floor(worldCoord)                          ← TileMap::ToIndex（S4 的 FloorToInt）
屏幕列         = floor(world.x - viewportOrigin.x)          ← **同一个 floor**
                 （S5 的 viewportOrigin 恒为 {0,0}；S6 换成 camera.pos）
屏幕行         = floor(world.y - viewportOrigin.y) + 1      ← 第 0 行留给引擎顶栏
```

**"落在哪一格"和"画在哪一列"是同一个问题** —— 所以**两处都用 floor**：

| 用途 | 取整 | 为什么 |
| --- | --- | --- |
| 落在哪一格 | **floor** | 格是"区间"：`[2,3)` 都属第 2 格。`(int)` 在负数下向 0 截断 → 错一格 |
| 画在哪一列 / 行 | **floor（同上）** | 实体占的是**格子**，画的就该是它占的那一格。混用 `round` 会让"HUD 说第 12 格、画面画在第 13 列" —— 同一个概念两套公式，正是 E-8 那两条症状的来源 |

> **⚠️ 2026-09-30 修正（落地偏差 3）**：本节原写"画在哪一列用 `round`"、公式
> `round(world - viewportOrigin)`。它和本清单 10.1 用例 23 的期望值
> （`(5.5, 3.5)` → `At(5, 4)`）**差整整一格** —— 7.2 的文字（"实体是格中心 → 取最近的列"）
> 是对的，是**公式漏了那个 `-0.5`**。现在统一成 floor：`floor(5.5) = 5`、`floor(3.5) + 1 = 4` ✓。
>
> 代价（明说）：终端一格只画一个字符，所以实体走到格子 70% 处时字符仍停在那一格 ——
> "平滑观感"是 `PLAN` H.2 扩展池里"半块字符"那件事，不在本阶段。
>
> 连带后果：**`math::RoundToInt` 在 S5 就没有生产调用者了**（它原本的服务对象正是这条 world→screen）。
> 处置**留到步骤 4 收口**时和 `TECH_DEBT` 一起定（按 B2"没人用就删"的纪律评估）——
> 见落地偏差 3 与 10.4。
>
> `S4` 准备的 `FloorToInt` 在本阶段经 `TileMap::ToIndex`（世界→tile）与渲染（世界→列）**两处上生产**。

### 7.3 延迟删除 + swap-erase

```text
MarkForRemoval(id):
    if id 无效 or 找不到 or 已在 m_pendingRemove:  return    # 幂等
    m_pendingRemove.push_back(id)

FlushRemovals():
    if m_pendingRemove 为空: return                          # 幂等
    for each id in m_pendingRemove:
        idx = IndexOf(id)                                    # 找不到 → m_entities.size()，跳过（可能已删）
        if idx < m_entities.size():                          # ← "找到了"的判据
            m_entities[idx] = m_entities.back()              # ← swap：把队尾搬过来
            m_entities.pop_back()                            # ← erase
    m_pendingRemove.clear()

私有 IndexOf(id):                                            # Find 两版 + FlushRemovals 共用
    if id 无效: return m_entities.size()
    for i, e in m_entities: if e.id == id: return i
    return m_entities.size()                                 # size() 就是"没找到"的哨兵
```

**四条必须理解的事**：

1. **为什么不在 `MarkForRemoval` 里直接删**：调用方很可能正在**遍历** `Entities()`
   （S9 的子弹碰撞就是这样）—— 遍历中 `erase` 会让迭代器/下标全部失效（`PLAN:967` E-9）。
   把删除推迟到遍历之外，是这个问题最便宜的解法。
   **推论：登记 ≠ 失效** —— 已登记的实体在 `FlushRemovals` 之前仍然 `Find` 得到、
   仍然参与 `Update`（2026-09-30 实现时定下的语义，用例 `MarkedEntitySurvivesUntilFlush`
   的第 ② 条断言钉着。"要不要更新"只由 `active` 决定，与"要不要删"分开）。
2. **为什么用 swap-erase 而不是 `vector::erase`**：`erase` 要把后面所有元素**前移**（O(n) 搬运），
   swap-erase 是 O(1) 搬运。代价是**顺序被打乱**。
3. **"顺序被打乱"是语义的一部分，不是实现细节**：所以用例只钉"剩下的**集合**正确"，
   **不钉顺序**；将来任何代码若依赖 `Entities()` 的顺序，那是它自己的 bug。

## 8. 接口决策表（2026-09-29 已拍板）

| # | 问题 | 决定 | 理由与代价 |
| --- | --- | --- | --- |
| 1 | 实体标识用什么 | **`EntityId`（自增 `uint32`，永不复用）** | 下标在 swap-erase 后**静默**指向别的实体；指针会悬空。代价：查找只能线性扫（`<50` 个实体，够） |
| 2 | 本阶段做不做**删除机制** | **做**：`MarkForRemoval` + `FlushRemovals`（swap-erase）+ 3 个用例 | 只留一个空壳容器的话，"遍历中禁止增删"这条规矩**既讲不清也没法测**；S9 的子弹消亡直接复用。代价：+~40 行（PLAN:967 的 E-9 点名要"先埋好概念"） |
| 3 | **HUD 显示什么** | **世界坐标面板**（pos / tile / sum dt / **v 速度**），**去掉 15 键四态面板** 和 **fps**（顶栏已有） | 于是 `OnRender` 不读输入 → **技术债 C4 关闭**。理由：四态面板是 S2 用来"证明状态机"的演示，它的任务已由 `test_input_state.cpp` 的 7 个用例接管；现在"按住 W 看 pos 连续变化"本身就是输入的展示。**`v` 是 2026-09-30 追加的**（速度可被 argv[2] 覆盖后，屏幕上必须能核 `pos ≈ v × sum dt`）。代价：S2 的调试手感没了（想找回只需把面板加回 `DrawHud`）；HUD 变长 → 提示压缩成 `WASD  Q` 才不超 80 列 |
| 4 | `world/` 文件划分 | **3 份**：`Entity.h`（含 Transform/Velocity）+ `TileMap.h` + `World.h` | `Transform`/`Velocity` 各只有一个 `Vec2` 成员（各约 6 行），单独成文件属于文件数膨胀，与 A.4"结构被复杂度逼出来"相冲。代价：**与 PLAN A.2 的 5 份不一致 → 记进差异表**（与 S4"6 份 math → 4 份"同类） |
| 5 | `TileMap` 越界语义 | **`At` 越界 → `Wall`**；`Set` 越界静默忽略 | 世界外都是墙 → S7 碰撞直接受益。代价：S6 的相机不能靠 `At()` 判边界 |
| 6 | 移动放在哪层 | `World::Update(dt)` 只做积分；**输入→速度**由 `game` 做 | `world` 因此不认识 `platform` → 可离线单测（第 3 节） |
| 7 | 地图内容从哪来 | `game/Game.cpp` 的 `BuildDemoMap()` **确定性**写死 | 引擎不认识"关卡长什么样"；不做文件格式、不做随机生成 |
| **8** | **本阶段做不做**测试（PLAN 的 S5 小节完全没提测试） | **做**：`world` 层 20 个用例 + `Game` 层 3 个用例 | `world` 纯逻辑（20 个）；`Game` 那 3 个见决策 9。代价：S5 的交付物比 PLAN 原文多两个测试文件 |
| **9** | **给 `tests/` 加一个假 `Input`**（测试替身），让 `Game` 层也能离线测 | **做**（本人在原方案之外追加；细节见下面 8.1） | 把"斜向不比直向快""帧序""`@` 画在正确的格"三条从**人眼验收**变成**机器验收**。代价：`tests` 多一条到 `game` 的链接边（无环）+ ~15 行替身 + 3 个用例 |

### 8.1 决策 9 的细节（假 `Input` 加成项）

**是什么**：`tests/test_game.cpp` 里一个 ~15 行的 `FakeInput : public mini::platform::Input` ——
`Input` 是**抽象基类**（S2 决策 1），所以测试可以自己派生一个，按键状态想怎么塞就怎么塞，
**不需要真终端**。真实现 `InputWindows` 要 `ReadConsoleInputW`，而 `Game` 只认 `const Input&` 这个抽象
—— 这就是当初"抽象基类 + 工厂"的第二个收益（第一个是"能有两个实现"，第二个是"**能被替身**"）。

**能钉住的三件事**：

| # | 用例 | 现在（不加替身时）怎么验 |
| --- | --- | --- |
| 1 | **斜向不比直向快**：按住 W+D 跑 1 秒 → 位移长度 ≈ `kMoveSpeed` 格（不是 `kMoveSpeed×√2`；默认 20 → **14 格/轴**而不是 20） | 人眼盯屏幕 —— 而人对 1.4 倍速度差不敏感 |
| 2 | **帧序**：按住 D 调一次 `OnUpdate` → **同一帧**位置就变了（不是晚一帧） | **零护栏**，且这是最容易写反的地方 |
| 3 | **画面落点**：玩家在 `(5.5, 3.5)` → `OnRender` 后 `fb.At(5,4).ch == '@'` | 人眼数格子 —— 这正是 `PLAN` E-8"画面整体偏移半格"的坑 |

> 第 3 条是**写这份文档时才想到的**：`OnRender` 只依赖 `FrameBuffer`（纯内存对象，S1 起就可离线测），
> 于是"肉眼看见 `@` 在屏幕上"也能变成机器可查 —— 这是 S4 那个手法的**二维版**：
> S4 用"字节等价"证明脏行 diff 不改画面，S5 用"像素落点"证明世界→屏幕坐标没差半格。

**代价（全部量化）**：替身 ~15 行 · 用例 3 个（20 → 23，全集 68 → **91**）· 新文件 1 个 ·
`tests/CMakeLists.txt` 多一行 `target_link_libraries` · 依赖图多一条 `tests → game`（无环）。

**⚠️ 必须写进注释的边界**（否则后人会误判覆盖面）：

> **假 `Input` 不能证明"真终端的输入是对的"。** 它只证明"**给定按键状态，`Game` 算得对**"。
> 真终端那条链（`ReadConsoleInputW` → `InputState`）是 `InputWindows` 的事，
> S2 已经用 `test_input_state.cpp` 的 7 个用例 + 手工验收覆盖了。

**不做会怎样**（留档，因为这是"选择追加"要有代价意识）：20 个用例照跑，`dt` 无关性/越界/延迟删除
都有护栏；但上面三条只能靠眼睛，且**反向验证第 4 条没有对应用例 → 只能记"未验"**。

## 9. 坑表

| # | 坑 | 怎么避 |
| --- | --- | --- |
| 1 | **半格偏移**：`(0.5,0.5)` 是第 1 格**中心**，而 tile 索引 `0` 是第 1 格 | 两套取整分开写清楚（7.2）；照 A.5 的四层约定 |
| 2 | **负数坐标下 `(int)` 向 0 截断** → 指向上一格 | `ToIndex` 用 `math::FloorToInt`；**专门一条负坐标用例**（反向验证 2 的目标） |
| 3 | **斜向速度 √2 倍** | 输入方向 `Normalized()`；用例比"直走 1 秒"与"斜走 1 秒"的位移 |
| 4 | **更新顺序**：速度晚一帧生效 | 帧序写进 `Game::OnUpdate` 注释（第 3 步在 `Update` 之前） |
| 5 | **遍历中增删** → 迭代器失效 / span 悬空 | `MarkForRemoval` + `FlushRemovals`；`Entities()` 的注释写明"任何 Spawn 后失效"；3 个用例钉住 |
| 6 | **swap-erase 打乱顺序** | 用例只钉"集合正确"、**不钉顺序**；`FlushRemovals` 注释里明写"会打乱" |
| 7 | **dt 被夹了两次** | `ctx.deltaSeconds` 已在 S3 夹过 → 不要再夹（夹两次不会错，但会掩盖"谁夹的"） |
| 8 | **id 复用** | `m_nextId` 只增不减；`EntityId{0}` 保留给"无效"；用例钉"删除后 Spawn 得到新 id" |
| 9 | **`Spawn` 覆盖调用方传的 id** | `Spawn` 的注释写明"prototype 的 id 字段会被覆盖"，免得有人以为可以指定 id |
| 10 | **HUD 与顶栏抢行** | 顶栏占第 0 行（S3 已定）→ 游戏从第 1 行开始；`AppConfig::showStats` 为假时第 0 行空着，游戏不能假设 |
| 11 | **`@` 走出地图**（本阶段没碰撞） | **故意留着**：这是 S7 的可见入口。HUD 显示 tile 坐标，让"已经出去了"看得见 |
| 12 | **忘了把新 `.cpp` 加进 `engine/CMakeLists.txt`** | 头文件不需要（PUBLIC include 目录自动暴露），**`.cpp` 必须手动加** —— 忘了会得到"未解析的外部符号"而不是"文件不存在"，容易懵 |
| 13 | **`game/` 空库会直接报错** | `add_library(game STATIC)` 没有源文件时 CMake 报 "no SOURCES given"；所以 `Game.cpp` 必须和 `CMakeLists.txt` **同一步**落地（顶层 CMakeLists:56-58 的注释就是记这件事） |
| 14 | 顶层 `MiniEngine` 忘了改成链接 `game` | 会得到"main.cpp 找不到 `game/Game.h`"；三处 CMake 要一起改（顶层 / engine / game） |
| 15 | **`std::span` 当"容器"用** | 它**不拥有**数据，也不延长生命周期；只在"立刻遍历一次"的场景用 |
| 16 | `Entity` 里有 `bool`，`sizeof` 不直观 | 加 `static_assert(sizeof(Entity) == ...)` 把布局钉住（照 S4 先例） |
| 17 | **把假 `Input` 的覆盖面想大了** | 它只证明"给定按键状态 `Game` 算得对"，**不证明真终端输入是对的** → 边界必须写进 `test_game.cpp` 的文件头注释（见 8.1 末） |

## 10. 验收

> **本阶段有"跑 exe 看现象"**（与 S4 相反）—— 但**主判据仍然是离线可证的**：
> dt 无关性做成单测，运行时的 HUD 只是**自证**。

### 10.1 用例清单（**23 个，4 个 suite**：`TileMap` / `EntityId` / `World` / `Game`）

> 前 20 个写在新文件 `tests/test_world.cpp`；最后 3 个（`Game`）写在新文件 `tests/test_game.cpp`（决策 9）。
> ⚠️ 估算在实现时从 17 → 20（决策 2 的删除机制 +3）→ **23**（决策 9 +3），**实际数以落地为准**
> （步骤 1/2 落地后实测：`test_world.cpp` 恰好 20 个，全集 88）。

**`TileMap`（8）**

| # | 用例 | 钉住什么 |
| --- | --- | --- |
| 1 | `TileMap.DefaultIsEmpty` | 构造后每格都是 `Empty` |
| 2 | `TileMap.SetAndAtRoundTrip` | `Set` 之后 `At` 读回同一个值 |
| 3 | `TileMap.OutOfBoundsAtReturnsWall` | 四边外 + 负坐标 → **`Wall`**（决策 5） |
| 4 | `TileMap.OutOfBoundsSetIsIgnored` | 越界写不崩、且不影响任何合法格（与 `FrameBuffer::Set` 同规矩） |
| 5 | `TileMap.ToIndexTruncatesTowardTile` | `2.0→2`、`2.9→2`、`3.0→3`（格是区间 `[2,3)`） |
| 6 | `TileMap.ToIndexOnNegativeFloor` | **`-0.1→-1`**、`-1.0→-1`（`(int)` 截断会给 `0`）← **反向验证 2 的目标** |
| 7 | `TileMap.IndexIsLinearized` | `(x,y)` 与线性索引一致（大小 200×200 的角落与中心都能读写） |
| 8 | `TileMap.InvalidSizeThrows` | `TileMap(0, 5)` / 负数 → 抛 `std::invalid_argument` |

**`EntityId`（2）**

| # | 用例 | 钉住什么 |
| --- | --- | --- |
| 9 | `EntityId.ZeroIsInvalid` | `EntityId{}` 的 `IsValid()` 为假；`{1}` 为真 |
| 10 | `EntityId.EqualityIsByValue` | 同值相等、异值不等（`operator==` 是默认的） |

**`World`（10）**

| # | 用例 | 钉住什么 |
| --- | --- | --- |
| 11 | `World.SpawnAssignsIncreasingIds` | 连续 `Spawn` 的 id 严格递增、互不相同、都 `IsValid()` |
| 12 | `World.FindFindsSpawnedEntity` | `Find(Spawn 返回的 id)` 拿到同一份数据（含 symbol） |
| 13 | `World.FindUnknownIdReturnsNull` | 没 Spawn 过的 id / 已删除的 id → **`nullptr`**（不是"找错实体"） |
| 14 | `World.SpawnedIdIsNeverReused` | 删除后再 `Spawn` → 得到**新的** id（不复用旧的） |
| 15 | **`World.MovementIsDtIndependent`** | `Update(0.1f)` **×1** 与 `Update(0.01f)` **×10** 的位置相同（±1e-5）← **PLAN 验收的机器形式** |
| 16 | `World.MovementScalesWithSpeed` | speed `{12,0}`、dt `0.5` → 位移 6 格；speed `{0,0}` → 纹丝不动 |
| 17 | `World.InactiveEntityDoesNotMove` | `active = false` 的实体不参与积分 |
| 18 | **`World.MarkedEntitySurvivesUntilFlush`** | `MarkForRemoval` 之后**仍然在**（遍历中不会突然消失）→ `Flush` 之后才消失 ← **"遍历中禁止删除"的可执行形式** |
| 19 | `World.FlushRemovalsKeepsSetOrderUnspecified` | 删中间一个后：**剩下的集合正确**（顺序不保证、不钉） |
| 20 | `World.FlushRemovalsIsIdempotent` | 清空后再 `Flush` 无事、再 `Mark` 同一个 id 不会重复删 |

**`Game`（3）** —— 在 `tests/test_game.cpp`，靠假 `Input`（决策 9 / 第 8.1 节）

| # | 用例 | 钉住什么 |
| --- | --- | --- |
| 21 | `Game.DiagonalMoveIsNotFaster` | 按住 D+S 跑 1 秒 → 每轴位移 ≈ `kMoveSpeed/√2`（默认 20 → 14 格），**且都小于直向的 20 格** ← **反向验证 4 的目标**（期望值全部由 `Game::kMoveSpeed` 算出，不许写死格数） |
| 22 | `Game.VelocityTakesEffectSameFrame` | 按住 D 调一次 `OnUpdate` → **本帧**位置就变了（帧序约束） |
| 23 | `Game.PlayerIsDrawnAtItsCell` | 玩家 `(5.5,3.5)` → `fb.At(5,4).ch == '@'`（世界→屏幕 = **floor**：`floor(5.5) = 5`、`floor(3.5) + 1 = 4`。取整统一之后用例名不再叫 `Rounded`，见落地偏差 3）|

### 10.2 正向验收

```powershell
cmake --build build --config Debug                      # 期望 0 warning
.\build\bin\Debug\mini_tests.exe TileMap                # 期望  8 passed
.\build\bin\Debug\mini_tests.exe EntityId               # 期望  2 passed
.\build\bin\Debug\mini_tests.exe World                  # 期望 10 passed
.\build\bin\Debug\mini_tests.exe Game                   # 期望  3 passed
.\build\bin\Debug\mini_tests.exe                        # 期望 90 passed（删 RoundToInt 后；步骤 3 当时是 91）
ctest --test-dir build -C Debug --output-on-failure     # 期望 1/1
ctest --preset asan                                     # 期望同样全绿（S3 打通、S4 复验过这条路）
cmake --build build --config Release                    # 期望同样 0 warning
```

**跑 exe 看现象（本阶段的核心交付）**：

```powershell
.\build\bin\Debug\MiniEngine.exe        # 默认 60fps
.\build\bin\Debug\MiniEngine.exe 240    # 240fps
```

| 观察点 | 期望 |
| --- | --- |
| 按住 `W/A/S/D` 或方向键 | `@` **连续**移动（不是"按一次跳一格"） |
| 斜向（如 W+D） | 与直向**速度一样**（不 √2 倍） |
| **dt 无关性（自证）** | HUD 显示 `sum dt` 与 `v`（速度），任何帧率下都满足 **`pos.x ≈ v × sum dt`**（±1 格）；`v` 默认 20，可用 `MiniEngine.exe 60 30` 改。<br>⚠️ **只在"提供真 key-up 的终端"里成立**（经典控制台窗口）。**VS Code 内置终端 / Windows Terminal 是 ConPTY，它不提供 key-up** → `Held` 永不出现 → 按住只按"自动重复速率 × 一帧的 dt"推进，这条自证会**明显偏小**（60fps 约 1/2，240fps 约 1/8）。见落地偏差 13 与 `TECH_DEBT` C10 |
| 60fps 与 240fps 对比 | 上面那条**都成立** → 这就是"100fps 和 300fps 距离相同"的现代说法 |
| 往右走约 40 格 | `@` **移出屏幕右侧** —— ✅ **当时的预期行为**（相机是 S6），不是 bug<br>⛔ **2026-10-02 已作废**：S6 落地后**走进第 41 格画面就开始滚动**，`@` 恒定在画面正中；只有**走出地图**（x ≥ 200，S7 之前能做到）才会离开画面。见 `doc/S6_CHECKLIST.md` |
| 撞墙 / 走出地图 | **不发生碰撞，能穿过去** —— ✅ 预期（碰撞是 S7） |
| 顶栏 `Rows n/n` | 仍然是个位数到几十（脏行 diff 没被破坏） |
| `Q` / `Ctrl+C` / 关窗口 | 正常退出，终端恢复。⚠️ **Esc 不退出** —— 它从 S10 起要用来关菜单（S2 决策），游戏只认 `Q` |

**"没坏"的确认**（S5 不该动 S1~S4 的行为）：

| 检查 | 期望 |
| --- | --- |
| 步骤 1/2 之后的 `MiniEngine.exe` | 与 S3 **逐字符一致**（那两步只加库和测试） |
| `mini_tests.exe Nope` | 仍返回非 0，且括号里的注册数变成 **90**（门闩 A1 有效；删 `RoundToInt` 前是 91） |

### 10.3 反向验证（4 条，**每条都必须看到红**）

> 由本人执行；也可以叫我改、你跑、我再还原。**做完必须还原现场并再跑一次确认全绿。**
> 四条影响的测试组互不相交（`World` / `TileMap` / `Game`），可以一次打完，
> 同时也验"**没有意料之外的红**"。

| # | 破坏方式 | 必须看到 | 为什么要这条 |
| --- | --- | --- | --- |
| 1 | `World::Update` 里把 `pos += perSecond * dt` 的 **`* dt` 删掉**（变成 `pos += perSecond`） | `MovementIsDtIndependent` + `MovementScalesWithSpeed` 变红 | 证明两条用例真的在测"乘了 dt"，而不是"位置变了" |
| 2 | `TileMap::ToIndex` 把 `FloorToInt` 换成 **`(int)` 截断** | **只有 `ToIndexOnNegativeFloor` 变红**，`ToIndexTruncatesTowardTile` **不变红** | 若两条都红或都不红 → 说明它们**测的是同一件事**，没钉住"负数"这个区别 |
| 3 | `World::MarkForRemoval` 改成**立即删除**（内部直接 swap-erase） | `MarkedEntitySurvivesUntilFlush` 变红 | 证明"遍历中禁止删除"这条规矩**被测到了**（不是注释说说） |
| 4 | `Game::OnUpdate` 里去掉 **`Normalized()`** | `Game.DiagonalMoveIsNotFaster` 变红 | 证明"斜向不比直向快"被钉住（这是决策 9 加替身换来的：**不做替身就没有对应用例**，只能记"未验"） |

**判据**：每条**必须**至少有一条用例标红、**不能有预料之外的红**。
**如果某条没红** —— 说明那个守卫是摆设，要按 `PLAN` 10.3 的老规矩**重写成真正能抓住的输入**
（S4 就撞过这事：`ParallelOutsideReturnsNullopt` 的两半里只有一半真会红，见 `S4_CHECKLIST` 10.3）。

### 10.3.1 实测现场（2026-09-30，**已跑完并还原**）

| # | 怎么跑的 | 实测结果 | 与预测的差异 |
| --- | --- | --- | --- |
| 1 | **单独一轮**（它级联太多，见下） | **85 passed / 6 failed**：`World.MovementIsDtIndependent`（`lhs=12.5 rhs=1200.5`）、`World.MovementScalesWithSpeed`（`12.5` vs `6.5`）、`World.InactiveEntityDoesNotMove`（同）、`World.MarkedEntitySurvivesUntilFlush`（同）、`Game.DiagonalMoveIsNotFaster`（`straight.ok` 假 + `straight.dx=0` + `diagonal.ok` 假 + 区间两条红）、`Game.PlayerIsDrawnAtItsCell`（`end.found` 假）| **数字全部命中预测**（连 `12.5 / 1200.5` 都对上了）；`Game.VelocityTakesEffectSameFrame` **如预测保持绿** ✓ |
| 2 | 与 3、4 同一轮 | **编译期就红**：`tests/test_world.cpp(33): error C2338: static_assert failed: '负数必须 floor：-0.1 属于第 -1 格'` | ⚠️ 施工图只写了"运行期哪条变红"，**没预见这一层** —— 因为 `ToIndex` 是 `constexpr`，而 `test_world.cpp` 顶部有 4 条**编译期** `static_assert`。守卫比预期**更早**生效 ✓，但也意味着运行期那条 `ToIndexOnNegativeFloor` 在破坏下**没机会跑** |
| 3 | 与 2、4 同一轮 | 红 2 条：`World.MarkedEntitySurvivesUntilFlush`、**`World.FlushRemovalsIsIdempotent`**（第 498 行也断言"登记后实体还在"） | ⚠️ **我漏算了一条** —— 施工图只点名 1 条。这是"两条用例守同一条语义"，**不是 bug**，但也说明我当时只凭 grep 摘要判断、没读函数体 |
| 4 | 与 2、3 同一轮 | 红 1 条：`Game.DiagonalMoveIsNotFaster`，且**恰好是预测的那 4 条断言**（`test_game.cpp` 189/190 区间、199/200 小于 `kMoveSpeed`） | ✅ 完美命中 —— 该守卫是"可执行的规格" |
| — | **还原后再跑** | 见 10.4 最后一条 | |

**结论**：**4 条守卫全部有效**（每条都被证明"改坏了就会红"），且**没有一条是摆设**。

### 10.4 交付后的自检（2026-09-30 结清）

- [x] 三个新头文件**都被 `test_world.cpp` include** —— 实测 `Entity.h` / `TileMap.h` / `World.h` 各 1 处 ✓
- [x] 依赖规矩自检：`world/` 头里 `windows.h` / `#ifdef` / `platform/` / `render/` **0 命中**；
      `engine/` 里**没有一处真的 include `game/`**（PLAN A.3 单向成立）✓
- [x] 所有公开接口**都有一行注释**回答"为什么存在 / 谁会用它"（决策 6 那条纪律）—— `MathUtils.h` 里
      `RoundToInt` 删除处那段注释就是这条纪律的产物
- [x] `static_assert(sizeof(Entity) == 24)` 在头文件里（**反向验证 2 的编译期守卫**也在这条线上）✓
- [x] `tests/` 里没有把 `game` 的私有实现细节写进断言 —— 三条用例只碰 `IGame` + `FrameBuffer::At` ✓
- [x] **`test_game.cpp` 的文件头写明那条边界**（坑 #17：假 `Input` 不证明真终端输入对）✓
- [x] 反向验证 4 条**都跑过**，每条都有用例标红，现场已还原（实测表见 **10.3.1**）✓
- [x] `doc/MiniEngine_PLAN.md` 的 S5 小节标 ✅（并回填实测）、A.2 目录树补上 `world/` 与 `game/`、
      差异表见本文件第 12 节 ✓
- [x] `doc/TECH_DEBT.md`：关 **C1 / C2 / C4**；B5 的 D 类改准（`FloorToInt` 上生产了、
      **`RoundToInt` 已删**、`Lerp`/`DegToRad` 保留的理由写在 `MathUtils.h`）；B5 的 A 类里
      `Input::IsDown` 那行改成"生产 1" ✓
- [x] `Input.h` 里 `IsDown` 的注释改准（步骤 3 就改了）✓
- [x] `AGENTS.md` 文档表：**S4/S5 两行本来就在**（施工图那条"漏了"已过时）—— 另把 `TECH_DEBT` 的
      描述改成"每阶段收口时更新它" ✓
- [ ] `git status` 干净 ← **只差这一条：等你按第 11 节提交**（提交时机由你定）
- [x] 额外一条（施工图没写）：**草稿期的临时改动零残留** —— 用 `grep -rn "\[反向验证"` 验过 ✓

## 11. commit 切分（每个都能构建）

| # | 信息 | 内容 | 跑完应该看到 |
| --- | --- | --- | --- |
| 1 | `feat: 加入 world/Entity.h 与 TileMap（10 个单测）` | `Entity.h`、`TileMap.h/.cpp`、`test_world.cpp` 前 10 个用例、2 处 CMakeLists | 78 passed；exe 行为**不变** |
| 2 | `feat: 加入 world/World（实体容器 + dt 积分 + 延迟删除，10 个单测）` | `World.h/.cpp`、后 10 个用例、`engine/CMakeLists.txt` | 88 passed；exe 行为**不变** |
| 3 | `feat: 建 game/ 静态库，main.cpp 缩到 35 行，@ 用速度移动（+3 个 Game 单测）` | `game/` 三件套、顶层 CMakeLists、`main.cpp`、假 `Input` + `test_game.cpp`、`tests/CMakeLists.txt` | 91 passed；`@` 连续移动、HUD 世界坐标 |
| 4 | `docs: S5 收口（PLAN 标 ✅ / TECH_DEBT 关 C1·C2·C4 / 两处注释改准）` | 三份文档 + `AGENTS.md` | `git status` 干净 |

## 12. 与 PLAN 原文的差异（**必须同步**）

| # | PLAN 写的 | 本阶段落地 | 为什么 |
| --- | --- | --- | --- |
| 1 | `impl` 栏：`struct Entity { Transform transform; Velocity velocity; char symbol; EntityId id; bool active; }` | **一致**（只把 `id` 放在第一个成员，便于聚合初始化时一眼看到） | — |
| 2 | A.2 目录树：`engine/include/mini/world/` 下 **7 个文件**（`Entity/Transform/Velocity/Health/TileMap/Scene/World`） | **3 个**（`Entity.h` 含 Transform/Velocity、`TileMap.h`、`World.h`）；`Health.h`/`Scene.h` 不建 | 决策 4；与 S4"6 份 math → 4 份"同类。等字段真加进来再拆 |
| 3 | S5 验收："100 FPS 和 300 FPS 下 `@` 走过同一段距离耗时相同" | 拆成**两条**：单测 `MovementIsDtIndependent`（主）+ HUD 显示 `Σdt` 让运行时**自证** `pos.x ≈ speed × Σdt` | "按住相同墙钟时间"有人为误差；换成"任何帧率下都满足同一条等式"，判据更硬且**不依赖手速** |
| 4 | C++ 知识栏提到 `optional` / `span` / `erase-remove` | `std::span` 用在 `Entities()`；**不用** `optional`（`Find` 返回 `Entity*`，`nullptr` 足够）；`erase-remove` 换成 **swap-erase** | `optional<Entity&>` 不存在（不能有 optional 引用）；`erase-remove` 是 O(n) 搬运，swap-erase 是 O(1) |
| 5 | 未提 `game/` 静态库 | **本阶段就建** | PLAN:122 与顶层 CMakeLists:56-58 的注释都写死了"S5 才建"；`TECH_DEBT` C2 的触发条件也是 S5 |
| 6 | 未提 HUD 内容 | 换成**世界坐标面板**（去掉 15 键四态面板） | 决策 3；这是 C4 能关闭的前提 |
| 7 | S6 相机"边界夹取" | S5 的 `TileMap::At` 越界返回 `Wall`，**但相机不能靠它判边界** | 决策 5；写在这里免得 S6 时踩<br>✅ **2026-10-02 S6 已落实**：相机用 `CameraLimits` 的 `mapW/mapH` 自己夹取（**没有碰 `At()`**），`view >= map` 时还先在 `Clamp` 之前早退（否则违反它 `lo <= hi` 的前置条件） |
| 8 | PLAN 的 S5 小节**完全没提测试**（S4 至少写了"单测 6 个"） | 加 **23 个用例**：`world` 20 + `Game` 3（假 `Input`） | 决策 8 / 9；与 S4 立下的"能离线测的就离线测"一脉相承，也让 S5 的核心现象从人眼验收变成机器验收 |

## 13. 本阶段的"学习点"备忘（给自己看的）

1. **"速度 × dt"到底解决了什么**：不是"让移动看起来顺"，而是**让移动与帧率无关**。
   写成 `pos += 1.0f`（格/帧）在 60fps 和 240fps 下速度差 4 倍 —— 这在本阶段第一次可以**亲手测出来**。
2. **实体标识三选一（id / 下标 / 指针）的判据**：**失效时是"找不到"还是"找错"**。
   找错（下标被 swap-erase 复用）比找不到难查一个数量级 —— 所以选 id。
3. **延迟删除为什么是标准解法**：它把"什么时候删"（业务时机）和"怎么删"（容器操作）**解耦**，
   于是"遍历中增删"这个类别的问题**从根上消失**，而不是靠在每个遍历里小心。
4. **为什么"顺序不保证"要写进注释而不是"以后注意"**：swap-erase 的代价是顺序，
   这是**数据结构选型的直接后果**，不是 bug。不说清，下一个人会以为是 bug 去"修"。
5. **"看得见"的阶段也要有离线判据**：S5 有画面，但 `pos.x ≈ speed × Σdt` 这条等式
   **离线就能证**（切成任意粒度都成立）—— 比"盯着屏幕看 10 秒"可靠得多。
6. **在 `main.cpp` 里养一个 250 行的 `DemoGame`，代价终于显形了**：S3 时它是"临时 demo"（`PLAN:410` 的记录），
   S5 一建 `game/` 就发现"搬走"这件事本身需要动三处 CMake。**"临时"的东西也有利息。**
7. **抽象基类的第二个收益是"能被替身"**：S2 选"抽象基类 + 工厂"时，理由是"将来会有第二个实现"。
   到 S5 才看清它真正值钱的地方 —— **测试可以派生一个假实现，于是依赖它的整条链都能离线跑**
   （`Game` 的核心三条现象全靠这个）。**"为了可测而依赖抽象"比"为了扩展"更常兑现。**
