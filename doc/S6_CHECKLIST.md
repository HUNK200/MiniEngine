# S6_CHECKLIST —— Camera / Viewport（世界大于屏幕）施工图

> 本文件是 S6 的**施工图 + 进度板 + 落地记录**。写代码前先读它；
> 实现过程中发现的偏差**追加**到「落地偏差」表，**不许回头改历史**。
>
> 引用格式：`PLAN:n` = `doc/MiniEngine_PLAN.md` 第 n 行；`A.x` / `D.x` / `E-x` = PLAN 的附录/小节/风险编号。

---

## 当前状态（2026-10-08：**S6 完成并本机复验**；原始收口记录见下方 2026-10-02 条目）

- **S0 ~ S5 均已完成并验收**。S5 收口在 `651689a`；本机（新机）2026-10-02 复验全绿：
  构建 0 warning、`ctest` 1/1、`mini_tests` **90 passed**。
- **S6 全部写完并提交**（`1ed45b6` / `7a414b1` / `c4bdbd7` 三个 commit）：
  `render/Camera.h` + 14 条用例 → `game/` 接入 → 文档收口。
  本人回报 **测试全绿**：用例总数 90 → **104**（`Camera` 14 / `Game` 3，其中
  `DiagonalMoveIsNotFaster` 因"相机作废了它的测量口径"被重做 —— 见落地偏差 3、4）。
  ⚠️ **未逐条抄录数字**（按 `TECH_DEBT` A7 的教训，以后跑这类命令顺手抄 `N passed`）。
  **→ 2026-10-08 已补录，见下一条。**
- **2026-10-08 本机复验**：构建 **0 warning**、`ctest` **1/1**、`mini_tests` **105 passed / 0 failed**
  （与源码静态计数 105 一致；此前 C7 那半步只标到"代码已写完"）。
  exe 人眼复核：本人 2026-10-08 回报"exe 执行逻辑正确"。本次复验只跑命令，**未改任何源文件**。
- **验收口径**：
  ✅ 机器验收（104 条用例）与 ✅ **人眼验收**（10.2 那 7 条，本人 2026-10-02 确认"可以了"：
  走进第 41 格画面开始滚动、`@` 在中正、右下角停住、走出去才出屏）；
  ➖ **反向验证 4 条按本人决定跳过**（理由、代价与触发条件写在 10.3 的开头）。
- **本阶段方案 2026-10-02 定稿**：跟随语义走 **snap（无平滑）**，7 条决策见第 8 节，
  **6 处**与 `PLAN` 原文的偏离已登记（见第 12 节）。
- **落地偏差 4 条**（见下表）—— 第 3、4 条是**跑起来之后**才发现的（S5 的用例口径被相机作废）。
- **C7 已在 S6 收口过程中办完并收窄**（本人 2026-10-02 裁定"现在抽"）：`core/Application.h`
  加 `kMinUsableCanvasSide` + `IsUsableCanvasSide/IsUsableCanvasSize` 两个谓词，
  `Application.cpp` 两条路径各用各的（**行为逐字不变**，见 `TECH_DEBT` 4.1），
  用例 `Core.CanvasSizeAcceptanceIsBoundarySafe`（用例总数 104 → **105**）。
  C7 剩"`ConsoleWindows` 的真终端 IO"继续挂在 C 组。

### ⚠️ 分工约定（`AGENTS.md` 1.4；本阶段沿用）

| 谁 | 做什么 |
| --- | --- |
| AI | **只改/新建源文件**。不跑 `cmake`、不跑 `ctest`、不跑 `MiniEngine.exe` |
| 本人 | 构建、跑测试、跑 exe、看现象，把结果回报 |

每步报告必须包含：改了哪些文件 / 我该执行什么（PowerShell 原样可粘贴）/ 期望看到什么 /
以及状态标注（**代码已写完，待构建 · 待运行 · 待肉眼验**）。

---

## 进度板

| 步骤 | 内容 | 状态 |
| --- | --- | --- |
| 1 | `render/Camera.h` + `tests/test_camera.cpp`（14 个用例）+ `tests/CMakeLists.txt` 一行 | ✅ **本人实测全绿**（2026-10-02） |
| 2 | `game/Game.h` / `game/Game.cpp` 接入相机（**本阶段唯一改变 exe 行为的一步**） | ✅ **本人回报测试全绿**（2026-10-02；含 `test_game.cpp` 的口径重做 —— 落地偏差 3、4） |
| 3 | 收口：同步 5 份文档 + `TECH_DEBT` | ✅ **本人已复核**（2026-10-08，与本次复验及过期引用修正同批） |
| 4 | 验收：**人眼 7 条**（10.2）+ **反向验证 4 条**（10.3） | **人眼 ✅ 本人确认"可以了"（2026-10-02）** · **反向验证 ➖ 本人决定跳过**（见 10.3 的说明） |
| 5 | **C7 收窄**（S6 收口时顺手办完）：画布尺寸谓词 + 1 条边界用例 | ✅ **本人实测全绿**（2026-10-08；0 warning / 105 passed / `ctest` 1/1） |

### 落地偏差（实现时发现的问题，逐步追加）

| # | 施工图写的 | 实际落地 | 为什么 |
| --- | --- | --- | --- |
| 1 | 10.1 用例 8 叫 `OriginIsFloorOfPos`，内容是"含**负数** pos（把相机推到地图外）" | 改名 `OriginIsFloorOfFractionalPos`，改成钉**小数部分的向下取整**（10.7 → 10、10.999 → 10、78.5 → 78）；负数那一半由 `test_world.cpp` 的 `TileMap::ToIndex(-0.1f)` 编译期断言覆盖 | **Follow 的 pos 恒落在 `[0, map-view]`**（负数分支不可达）→ 按原写明"负数"就成了一句不真的话（B4 纪律）。连带影响：10.3 的 **R3 "把 `FloorToInt` 换成 `(int)` 截断"在 pos ≥ 0 时不可观测**（截断与 floor 对非负数结果相同）→ R3 的破坏方式要改成"**先加 0.5 再截断**（模拟 round）"，那时用例 10/11/12 会红。<br>⚠️ 本行**不改** 10.1/10.3 的原文（偏差只追加），读到那里时以本行为准 |
| 2 | 6.6 的表格写 "`Game::OnUpdate` 末尾…（**在 `m_world.Update` 与 `FlushRemovals` 之后**）"，**但 7.1 的帧序图写的是 "Update 之后、FlushRemovals 之前"** —— 施工图自己前后矛盾 | 按 **7.1 的帧序图**落地：`Follow` 放在 `Update` 之后、`FlushRemovals` 之前（原 ⑦ 号顺延为 ⑧） | 两者都不影响正确性（S6 里玩家不会被标记删除），但帧序图那条更自然：相机读的是"本帧更新后的位置"，而 `FlushRemovals` 只动被标记的实体、与相机无关。**6.6 那句话以本行为准**（它同时把 `kHudRow` 那条注释误标成 "`Game.h` 注释"，实际在 `Game.cpp:34-36`，行号对、文件名错） |
| 3 | （施工图**没写**：S5 的 3 个 `Game` 用例是否会被相机影响） | `tests/test_game.cpp`：新增 `kWorldCanvasW/H = 256`（**视口 ≥ 地图 → 相机退化成 (0,0)**），`RunHolding` 加两个画布参数（默认仍是 80×25），**只有量速度的 `DiagonalMoveIsNotFaster` 换成大画布**；另两条保持 80×25；文件头与两处注释加了警告 | **S5 的用例拿"屏幕位移"当"世界位移"用，前提是"没有相机"**。实测（2026-10-02）：斜向跑 1 秒后玩家 y 从 2.5 涨到 16.6 → **越过垂直跟随起点（y > 11.5）** → 相机开始跟 → `@` 被钉在第 12 行 → `dy` 只测出 **9**（真实 14）→ `dy ∈ [13,15]` 变红。<br>⚠️ 关键：相机跟随区里屏幕位移会**饱和**，所以这不是"期望值要放宽"而是**测量口径失效** —— 实测"归一化正常"与"删掉 Normalized()"两种情况下 `dy` **都是 9**，反向验证 4 会失效。<br>修法：让量世界位移的用例把相机变成空操作（视口 ≥ 地图，即决策 4 的退化语义）。**画布必须 ≥ 201×201**：相机拿到的视口高 = 画布高 − 1，200 行的画布只给 199 行视口 → y 轴仍会跟随。这个修法与速度无关（不需要为"速度可调"再改测试）。<br>⚠️ 另两条用例**仍绿是运气**（位移 7 格 / 2 格，还没走出跟随区），它们含同一隐含假设 → 已在文件头写明"调大 `kMoveSpeed` 会让它们失效" |
| 4 | （偏差 3 的**第一次修法不完整**） | 偏差 3 只换了 `FrameBuffer` 的尺寸 → 用例**仍然红**（同样断在 `dy`）。修法：`MakeContext` 也接画布尺寸参数，`RunHolding` 的两处 `OnUpdate` 传 `canvasW/canvasH` | **相机读的是 `ctx.screenWidth/Height`，不是 `FrameBuffer` 的尺寸**：`fb` 只管"画到哪"，相机只管"从哪看"。第一次修法让两者不一致（ctx 仍是 80×25 → 视口 80×23 → 相机照旧跟随）→ 用例照旧红。<br>教训：**`ctx` 与 `fb` 的尺寸是同一个事实的两条路**，`Application::Run` 第 ③/④ 步保证它们一致（先 `Resize` 再造 `ctx`）；测试里手搓 `FrameContext` 时也必须一起改 —— 这条已写进 `MakeContext` 的注释。**用例第二次变红反而是好事**：它把"相机的权威尺寸是 `ctx`"这件事钉了出来 |

---

## 0. 前置事实（写方案前逐条核过的，不是推测）

| 事实 | 位置 | 对 S6 的意义 |
| --- | --- | --- |
| 全仓 `grep -i "camera\|viewport\|scroll"` → **0 命中** | `engine/` `game/` `main.cpp` `tests/` | 相机是从零加，不是改 |
| 渲染已经在**遍历屏幕格**：`for row, col` → `map.At(col, row - 1)` | `game/src/Game.cpp:178-185` | "只遍历可见 tile"**已经满足**（约 1840 格），S6 只是把屏幕格换成"整数原点 + 偏移" |
| 游戏画布从第 **1** 行开始，第 0 行是引擎顶栏 | `game/src/Game.cpp:37`（`kHudRow = 1`） | **可用视口 = `fb.Width() × (fb.Height() - 1)`**，垂直方向少一行 |
| 地图 200×200、速度 20 格/秒、出生 (2.5, 2.5) | `game/include/game/Game.h:46-58` | 夹取区间与验收路径的计算基数 |
| **resize 与帧内一致性已经解决** | `engine/src/core/Application.cpp:89-118`：第 ③ 步先 `Resize`，第 ④ 步才用 `fb.Width()/Height()` 造 `ctx` | **不存在"ctx 与 fb 差一帧"的坑**（原本怀疑有，核过是对的）；相机可以直接用 `ctx` 的尺寸 |
| HUD 格式串约 **71** 字符 | `game/src/Game.cpp:230-231` | 想加 `cam` 一栏会超 80 列 → 决策 5 定"不加" |
| `Clamp` **假定 `lo <= hi`，由调用方负责** | `engine/include/mini/math/MathUtils.h:33-35` | `视图 >= 地图` 时夹取区间为空 → **必须在 `Clamp` 之前显式分支**，不能靠它兜 |
| `TileMap::ToIndex` 就是 `math::FloorToInt` 的转发 | `engine/include/mini/world/TileMap.h:72-75` | 相机直接用 `math::FloorToInt`，**不必依赖 `world/`**（否则多一条 render→world 的反向边） |
| S5 特意把 demo 地图改成"每 10 格规则柱阵 + 一栋不对称地标楼" | `doc/S5_CHECKLIST.md:116` | **这是 S6 相机的连续参照物**：柱阵证明"滚动是整格平移"，地标楼证明"确实动了" |
| S5 的验收写"往右走约 40 格 `@` 移出屏幕是**预期行为**" | `doc/S5_CHECKLIST.md:793` | S6 完成后这条**作废**（收口时标注） |

---

## 1. 目标与**明确不做**

**目标**（`PLAN:501-512`）：让视口跟着 `@` 在地图上滚动；`@` 在地图范围内**永远在画面内**；
走到地图边角时视口被顶住、**不出现错位的墙**。本阶段的学习点是
"**坐标变换 + 视口 + 只画看得见的**"，不是"跟随算法"。

### 明确不做（防止"顺手多做一点"）

| 不做 | 留给谁 / 触发条件 |
| --- | --- |
| **平滑跟随**（橡皮筋 / 死区） | 本人 2026-10-02 决定。触发条件：玩过之后觉得"@ 钉在正中太机械"或"想看清地图中间"。升级只改 `Follow` 内部 + 补用例（接口形状不变） |
| `ScreenToWorld` 的**实现** | **S9**（瞄准 / 拾取）才有调用者；本阶段只在头注释留契约（决策 3） |
| 新建 `render/Viewport.h` | `PLAN` A.2 目标态有它，但 S6 只需要"四个 int"；按 `TECH_DEBT` B2 的"0 调用者不建"纪律，不建 |
| 相机旋转 / 缩放 / 倾斜 | `PLAN:961`：本阶段**只有平移** |
| 亚格渲染（`▀▄` 半块字符，纵向分辨率翻倍） | `PLAN:1150` 扩展池；1 格 = 1 字符下"真平滑"不存在 |
| 距离雾 / 变暗 | `PLAN:1149` 扩展池 |
| 改 `world/`（含 `TileMap::At` 的越界语义） | 永远不动（S5 决策 5）。`PLAN:969` 也写了"S6→S12 世界数据完全不用改" |
| 碰撞（`@` 仍能穿墙、能走出地图） | **S7**；本阶段故意留着这个缺口 |
| HUD 加 `cam` 一栏 | 决策 5；将来想加必须先砍掉别的栏目腾出列宽 |
| `Renderer2D` 抽象 / 相机朝向 / look-ahead | 没有需求；`PLAN:968` 只要求"将来的 Renderer2D 不接收 Player"，不要求现在建 |

---

## 2. 交付物清单

| # | 文件 | 动作 | 说明 |
| --- | --- | --- | --- |
| 1 | `engine/include/mini/render/Camera.h` | **新增** | header-only；类 `Camera` + POD `CameraLimits` |
| 2 | `tests/test_camera.cpp` | **新增** | suite `Camera`，14 个用例 |
| 3 | `tests/CMakeLists.txt` | 改 1 行 | 手动加 `test_camera.cpp`（S0 起禁用 `file(GLOB)`） |
| 4 | `game/include/game/Game.h` | 改 | 加 `mini::render::Camera m_camera;` + 更新绘制注释 |
| 5 | `game/src/Game.cpp` | 改 | `OnUpdate` 末尾 `Follow`；`DrawMap`/`DrawEntities` 改用整数原点；改写 HUD 覆盖注释 |
| — | `engine/CMakeLists.txt` / 顶层 `CMakeLists.txt` / `main.cpp` / `world/` / `render/` 其余文件 | **不动** | header-only，不需要新 .cpp |

---

## 3. 依赖规则（本阶段**新增 1 条边**）

```text
main → game → render/Camera.h → math
                    ↑ 新增这一条（render → math，向下，符合 PLAN A.4 的分层）

⛔ 禁止出现的边（本阶段要自检）：
   render/Camera.h → world/      （TileMap::ToIndex 只是 FloorToInt 的转发，不必绕道）
   render/Camera.h → platform/   （相机不认识终端）
   world/ → render/ 或 game/     （PLAN A.3 单向，S5 已立）
```

`render/` 里此前没有任何文件 include `math/`，**这是 render → math 的第一条边**——向下依赖，
`math` 是 `PLAN` A.4 里最底层、变化最慢的一层，所以这条边安全。

---

## 4. 数据结构与所有权

| 项 | 决定 | 理由 |
| --- | --- | --- |
| `Camera` 的成员 | **只有 `mini::math::Vec2 m_pos{}`** | 尺寸不进成员（见 6.3）；`PLAN` 写的 `target` 字段本阶段**不建**（snap 下它是临时量，0 调用者） |
| `Camera` 的大小 | `static_assert(sizeof(Camera) == 8)` 写在头里 | 与 S5 的 `sizeof(Entity) == 24` 同一套路：编译期钉住"没有偷偷长胖" |
| 谁拥有 `Camera` | `game::Game` 持有一个**值成员** | 相机是游戏侧状态；引擎不认识"相机"这个概念 |
| 尺寸的来源 | 视口取 `ctx.screenWidth/Height`；地图取 `m_world.Map()` | **不复制、不缓存**，避免与 `FrameBuffer` / `TileMap` 形成第二份真相 |
| `CameraLimits` | 每帧就地构造的 POD（4 个 int = 16 字节，无堆分配） | 便宜到不必缓存；`static_assert(sizeof(CameraLimits) == 16)` |
| 生命周期 | 与 `Game` 同寿；不需要 `OnStart` 特殊初始化 | `Follow` 第一帧就会把它放对位置（无滞后状态 → 它同时也是 `SnapTo`） |

---

## 5. 逐文件实施步骤（每步都立刻跑一次）

| 步 | 动作 | 跑完应该看到 |
| --- | --- | --- |
| 1 | 新建 `render/Camera.h`（第 6 节）→ 新建 `tests/test_camera.cpp`（第 10.1 节）→ `tests/CMakeLists.txt` 加一行 | `mini_tests.exe Camera` → `14 passed`；全集 → **104 passed**；`MiniEngine.exe` 现象**与 S5 完全一致**（Game 还没用它） |
| 2 | 改 `game/Game.h` / `Game.cpp`（第 6.6 节） | 地图开始滚动、`@` 在画面正中、角上不露错位墙（第 10.2 节） |
| 3 | 同步第 11 节列的 5 份文档 + `TECH_DEBT` | `git status` 干净 |

---

## 6. 接口设计（施工图）

### 6.1 为什么要有 `Camera` 这个类 —— 职责边界

`Camera` 只回答**一个问题**：

> "现在这一屏，是从世界的第几格开始看的？"

它**不做**这些事（每一条都有代替者）：

| 不做 | 谁做 |
| --- | --- |
| 不认识"玩家"、不读输入 | `game::Game`（它才知道谁是玩家） |
| 不认识 `FrameBuffer`、不画任何东西 | `game` 的 `DrawMap` / `DrawEntities` |
| 不认识 `TileMap`、不知道哪格是墙 | `world::TileMap` |
| 不知道"顶栏占了第几行" | `game`（`kHudRow` 是游戏侧概念） |
| 不认识终端 / 代码页 / ANSI | `platform::Console` |

**为什么值得单独一个类**（而不是在 `Game::DrawMap` 里写两行减法）：

1. 它是 S6 唯一的**新概念**，也是唯一有**边界逻辑**（夹取 + 退化）的东西 →
   单独成类才能离线单测（`PLAN:989` E-8 明确要求"写一个坐标转换的单元测试"）；
2. `Game` 里已经有地图 / 实体 / HUD 三件事，把夹取逻辑塞进 `DrawMap` 会让"画"和"算坐标"混在一起；
3. 将来加平滑/死区只动这个类（第 8 节的升级路径）。

### 6.2 `render/Camera.h` 的骨架（**只有声明，实现由本人敲**）

```cpp
#pragma once

#include "mini/math/Vec2.h"        // Vec2
#include "mini/math/MathUtils.h"   // FloorToInt / Clamp

// ============================================================
// render/Camera.h —— 视口在世界里的位置（S6）
//
// 这一层干什么：记住"现在从世界的哪个格子开始看"，并把世界坐标换算成屏幕格。
//   **它不读输入、不画东西、不认识终端、不认识 TileMap** → 可以完全离线单测。
// 依赖谁：math（Vec2 / FloorToInt / Clamp）+ 标准库。
//   ⛔ 不 include world/：`TileMap::ToIndex` 本来就是 `math::FloorToInt` 的转发，
//      绕道 world 只会多一条 render → world 的跨层边（PLAN A.4）。
// 被谁用：game::Game（每帧跟随 + 渲染）；tests/test_camera.cpp。
//
// ⚠️ 本阶段**没有平滑**（2026-10-02 决定走 snap）：`Follow` 是纯函数，
//    `pos` 直接等于夹取后的目标。因此它同时也是 SnapTo —— S10 的玩家瞬移天生正确。
//    升级路径写在 S6_CHECKLIST 第 8 节：只改 `Follow` 内部，接口不变。
// ============================================================

namespace mini::render {

// 视口与地图的尺寸（单位：格）。**由调用方每帧提供，Camera 不持有**（见 S6_CHECKLIST 6.3）。
struct CameraLimits {
    int viewW = 0;   // 可用视口宽（列）
    int viewH = 0;   // 可用视口高（行，**不含引擎顶栏那一行**）
    int mapW  = 0;   // 世界宽（格）
    int mapH  = 0;   // 世界高（格）
};

class Camera {
public:
    // snap 跟随：逐轴独立地令 pos = clamp(world - view/2, 0, map - view)。
    //   · `view / 2` 用**整数除法**：奇数宽下不让 `@` 在相邻两列之间跳（坑 4）
    //   · `view <= 0` 或 `view >= map` 的轴 → pos 取 0（退化，贴左上；坑 1、2）
    //   · 不抛、不分配、无隐藏状态（noexcept，与 world::World::Update 同类）
    void Follow(mini::math::Vec2 worldPos, const CameraLimits& limits) noexcept;

    // 渲染用的**整数原点** = floor(pos)（负数也正确，坑 5）。
    // 地图与实体**都用它**（第 7.2 节的硬规矩）。生产调用者：Game::DrawMap / DrawEntities。
    [[nodiscard]] int OriginCol() const noexcept;
    [[nodiscard]] int OriginRow() const noexcept;

    // 相机的浮点位置。为什么存在：① 单测要断言"居中/夹取"的精确值；
    //   ② `ScreenToWorld` 与将来的平滑都要从它出发。
    //   ⚠️ 生产代码目前**不直接调它**（渲染走 Origin*）—— 别因为"没人调"就删，
    //   它是"相机到底在哪"的唯一可读出口（TECH_DEBT B5 的同类处理）。
    [[nodiscard]] mini::math::Vec2 Pos() const noexcept;

    // ── 契约（**S6 不实现**，S9 需要时再加，加的时候必须带用例）────────────
    //   ScreenToWorld(col, row) = pos + {col + 0.5, row + 0.5}   ← 那格的**中心**
    //   它不是 floor 的精确逆运算：半格偏移由 +0.5 表达，反算回去只能拿到格中心
    //   （PLAN A.5 原文已写明这一点）。
    // [[nodiscard]] mini::math::Vec2 ScreenToWorld(int col, int row) const noexcept;

    // ⛔ 刻意**不提供**：
    //   IsDegenerate() —— 退化行为已在 Follow 里定死（该轴取 0），界面上不需要查询
    //   Reset()        —— Follow 就是 snap，没有需要复位的状态
    //   target 字段    —— snap 下它是临时量（0 调用者）；加平滑时再加

private:
    mini::math::Vec2 m_pos{};
};

static_assert(sizeof(CameraLimits) == 16, "CameraLimits 是 4 个 int 的 POD");
static_assert(sizeof(Camera) == 8, "Camera 只应有一个 Vec2");

} // namespace mini::render
```

### 6.3 为什么尺寸（`CameraLimits`）是"调用方给"而不是"`Camera` 持有"

| 方案 | 后果 |
| --- | --- |
| **A. `Camera` 持有 view/map 尺寸**（`SetViewport` / `SetMapSize` 各一个 setter） | 调用点最短（`cam.Follow(playerPos)`），但**同一份事实出现三处**：`FrameBuffer` 的宽高、`TileMap` 的宽高、`Camera` 里抄的一份。任何一处不同步（比如忘了在 resize 时调 `SetViewport`）症状都是"画面错位"，且**很难查**（三处都"看起来对"） |
| **B. 每帧由调用方传入（本阶段选择）** | 没有第二份真相（尺寸永远来自 `FrameBuffer` / `TileMap` 本身）；代价是调用点要写 4 个 int，且 `Follow` 的签名长一点 |

选 B 的理由与 S5 选"`Find` 返回 `Entity*` 而不是持有引用"是同一类：
**宁可在调用点多写一点，也不留一个会跟真实来源漂移的缓存**。

`CameraLimits` 是 POD 聚合体，`{a, b, c, d}` 就地构造，没有堆分配、没有生命周期问题。

### 6.4 为什么公开的是"整数原点"`OriginCol/OriginRow`，而不是 `ToScreenCol(world)`

第 7.2 节的硬规矩是：**地图与实体必须用同一个整数原点**。
如果 `Camera` 提供 `ToScreenCol(worldX)`，那么：

- 地图那一边（遍历屏幕格）**用不上**它 —— `DrawMap` 需要的是"从哪一格开始"，不是"某个坐标画在哪"；
- 于是代码里会同时存在**两条取整路径**（`Camera::ToScreenCol` 与 `DrawMap` 自己算的偏移），
  正是 `PLAN:989` E-8 那类"两条路径各自漂移"的温床。

只公开整数原点之后：**取整只发生在 `OriginCol()` 内部那一次 `FloorToInt`**，
`game` 那边全是整数加减（不会再有第二次取整的机会）。用例 11 专门钉这条。

### 6.5 调用方契约（`game` 侧）

| 契约 | 内容 | 违反后的现象 |
| --- | --- | --- |
| **帧序** | `Follow` 必须在 `m_world.Update(dt)` **之后**、`OnRender` **之前** | 用上一帧的 @ 位置 → `@` 与地图相位差一帧（表现为"`@` 偶尔不在正中"）；这是 S5 帧序坑（`S5_CHECKLIST` 7.1）的同族 |
| **尺寸来源** | `CameraLimits{ ctx.screenWidth, ctx.screenHeight - kHudRow, map.Width(), map.Height() }` | 减错 `kHudRow` → 底部多露一行 / HUD 压错行（坑 8）。`Application` 已保证 `ctx` 与 `fb` 同帧一致，不需要额外同步 |
| **HUD 行归属** | "第 1 行是 HUD"这件事**只属于 `game`**，`Camera` 只认"格" | 把 `kHudRow` 塞进 `Camera` 会让渲染层知道游戏 HUD 的存在（跨层语义污染） |
| **调用次数** | 每帧一次（`OnUpdate` 里）；resize 也在同一次调用里自动跟上 | 不需要 resize 钩子：尺寸每帧现取 |

### 6.6 `game` 侧的改动点（第 2 步交付物）

| 位置 | 改动 |
| --- | --- |
| `Game.h` | 加 `#include "mini/render/Camera.h"` + 成员 `mini::render::Camera m_camera;` |
| `Game.h` 注释（`Game.cpp:34-36` 那条） | "HUD 压在世界的第 0 行上" → 改成"**HUD 压在视口第 0 行**（即 `OriginRow()` 那一行）对应的世界行上" |
| `Game::OnUpdate` 末尾 | 取 `const Entity* p = m_world.Find(m_player)` → `m_camera.Follow(p->transform.pos, limits)`（在 `m_world.Update` 与 `FlushRemovals` 之后） |
| `Game::DrawMap` | 循环体内世界格从 `(col, row - kHudRow)` 改成 `(OriginCol() + col, OriginRow() + row - kHudRow)` |
| `Game::DrawEntities` | 屏幕列 `ToIndex(pos.x) - OriginCol()`、屏幕行 `ToIndex(pos.y) - OriginRow() + kHudRow` |
| `Game::DrawHud` | **不改**（`pos` / `tile` / `Σdt` 继续是世界坐标；决策 5 说不加 `cam` 栏） |

---

## 7. 算法与伪代码

### 7.1 帧序（硬约束，顺序不能换）

```text
① 读输入 → ② 归一化方向 → ③ 写速度 → ④ m_world.Update(dt)
                                            ↓
                        ⑤ m_camera.Follow(玩家位置, limits)   ← **必须在这之后**
                                            ↓
                        ⑥ m_world.FlushRemovals()
        ─────────── 帧内分割线（下面是渲染）───────────
                        ⑦ OnRender：DrawMap → DrawEntities → DrawHud
```

### 7.2 世界坐标 → 屏幕格（**同一个整数原点**）

```text
OriginCol = FloorToInt(cam.pos.x)        ← 相机**唯一**一次取整（floor）
OriginRow = FloorToInt(cam.pos.y)

地图:  屏幕格 (col, row) 显示世界格 (OriginCol + col, OriginRow + row - kHudRow)
实体:  屏幕列 = ToIndex(entity.pos.x) - OriginCol
       屏幕行 = ToIndex(entity.pos.y) - OriginRow + kHudRow
```

**为什么这不等于 `PLAN` A.5 原式 `screen = floor(world - camera.pos)`**：
两者在 `camera.pos` **非整数**时**差一格**。A.5 原式是一条**混合约定**
（画 tile 时用屏幕空间遍历、画实体时用世界空间减法，两条路径的取整方向不一致），
在相机恒为 0 的 S5 里看不出来，S6 一引入相机就会暴露成"`@` 与它脚下的格子错位"。

统一成整数原点之后有一个可以直接拿来做验收的推论：

```text
未夹取时：@ 的屏幕列 = ToIndex(px) - FloorToInt(px - viewW/2) = viewW/2  ← 恒定
         地图整屏平移一格 ⇔ @ 跨进新的一格        （两者严格同步，不会各走各的）
```

**收口时必须同步改 `PLAN` A.5 的措辞**（见第 11 节）。

### 7.3 `Follow` 伪代码

```text
Follow(worldPos, limits):
    if limits.viewW <= 0 || limits.viewH <= 0:      // 极小窗口 / 未初始化
        m_pos = {0, 0}
        return                                      // 别把 lo > hi 喂给 Clamp（坑 1）

    对 x、y 各轴独立地:
        view = 该轴视口尺寸
        map  = 该轴地图尺寸

        if view >= map:                             // 退化：贴左上
            pos.轴 = 0
        else:
            target = worldPos.轴 - float(view / 2)  // 整数除法（坑 4）
            pos.轴 = Clamp(target, 0.0f, float(map - view))
            //        ↑ 这个分支保证 lo <= hi，满足 Clamp 的前置条件（MathUtils.h:33）

    m_pos = pos
```

---

## 8. 接口决策表（2026-10-02 已拍板）

| # | 决策 | 取值 | 为什么不选另一个 |
| --- | --- | --- | --- |
| 1 | 跟随语义 | **snap**（无平滑、无死区） | S6 的核心风险是 E-8（坐标错位），snap 把相机压成纯函数，坐标问题一眼可查；平滑/死区各带 1~3 个参数与新失效模式，且本阶段 80×23 的视口里手感收益小 |
| 2 | 落地位置 | `engine/include/mini/render/Camera.h` | `PLAN` A.2 目标态就在 `render/`；header-only 不需要新 .cpp |
| 3 | `ScreenToWorld` | **只留契约，不实现** | S6 无调用者（S9 瞄准才有）；按 `TECH_DEBT` B2 / S4 差异 2 的"0 调用者不建"纪律 |
| 4 | 视图 ≥ 地图 | 该轴 **pos = 0**，在 `Clamp` **之前**显式分支 | `Clamp` 假定 `lo <= hi`（`MathUtils.h:33`），喂进去就是违约；"贴左上"是可解释的语义（地图真的比屏幕小） |
| 5 | HUD | **不加 `cam` 栏** | 现格式串约 71/80 列，加 19 列必超宽；判相机对错靠柱阵 + `tile` 数字 |
| 6 | 跟随/夹取顺序 | snap 下合一；只守**帧序**（`World::Update` 之后） | 没有滞后状态，顺序只剩"读的是不是本帧位置"这一个问题 |
| 7 | 相机取整 | `FloorToInt`，且"屏幕格 ↔ 世界格"共用**同一个整数原点** | 见 7.2：混用两条取整路径就是 E-8 本身 |

### 升级路径（现在不做，触发条件写在这里）

| 触发条件 | 加什么 | 改动面 |
| --- | --- | --- |
| 觉得"`@` 钉在正中太机械" / 想看清地图中间 | **死区**（本轮分析里最省的一个：不加 `k`、天然与帧率无关） | 只改 `Follow` 内部 + 加 2~4 个用例；渲染、夹取、验收判据不动 |
| 想要"镜头有质感"（接受调参 + 尾巴） | **橡皮筋**（`α = -expm1(-k·dt)`，`k >= 4`） | 同上，另加：`k` 常量、`expm1` 精度用例、`PLAN` A.5 不用改 |

---

## 9. 坑表

| # | 情况 | 期望行为 / 坑 |
| --- | --- | --- |
| 1 | **`view >= map`**（终端比地图大） | 该轴 pos = 0。**必须在 `Clamp` 之前分支**：`MathUtils.h:33` 写明 `Clamp` 假定 `lo <= hi` |
| 2 | `view <= 0`（拖窗口到极窄 / 未初始化） | 立即返回并把 pos 置 0。`Application` 已拒绝 `width/height < 2`（`Application.cpp:100`），这里是第二道闸 |
| 3 | **帧序** | `Follow` 必须在 `World::Update(dt)` **之后**（7.1）：放前面用上一帧位置 → `@` 与地图相位差一帧 |
| 4 | **奇数视口宽** | 居中偏移必须 `viewW / 2`（整数除法）。用 `viewW * 0.5f` 会让 `@` 在相邻两列之间反复跳 |
| 5 | 取整方向 | 统一 `FloorToInt`：不是 `(int)` 截断（负数下指向上一格），不是 `round` |
| 6 | 玩家走出地图（S7 之前会发生） | 相机夹在 `[0, map - view]`，但 `@` 能走到地图外 → **此时 `@` 会离开画面，这是预期**（S5 那条"往右走 40 格出屏"要改写成"走出地图才出屏"） |
| 7 | 地图边框墙 + 夹取墙 | 两者在角上都会出现。判对错靠**柱阵每 10 格一棵 + 地标楼 `x=30..36, y=5..8`**；夹取写错的现象是"屏幕边缘出现**错位**的一整片墙"，不是空白（因为 `At` 越界返回 `Wall`，`TileMap.h:22-26`） |
| 8 | HUD 覆盖 | 视口高必须 `H - kHudRow`；写成 `H` 会在底部多露一行、并让 HUD 压错行。`Game.cpp:34-36` 那条注释要同步改写 |
| 9 | 滚动时的脏行 | 滚动期间整屏都变 → 顶栏 `Rows` 接近满行；**松手立刻掉回 1~2**（snap 无尾巴，这是它与橡皮筋的可观测区别） |
| 10 | `@` 的 HUD 自证 | `pos ≈ v × Σdt` 不受影响（相机与世界坐标无关） |
| 11 | 视口尺寸取整带来的"半格" | 相机 pos 是浮点，但渲染只用 `FloorToInt(pos)` → 画面**只在整数格上移动**，天然不会出现"半格抖动"（`PLAN:512` 的坑 2 由这个设计消掉，而不是靠额外的 floor 补丁） |

---

## 10. 验收

### 10.1 用例清单（**14 个**，suite `Camera`，新文件 `tests/test_camera.cpp`）

| # | 用例 | 内容 / 期望 |
| --- | --- | --- |
| 1 | `FollowCentersPlayerInOpenArea` | 地图中间：`Pos() == worldPos - view/2`（逐分量 `ApproxEquals`） |
| 2 | `ClampAtLeftTopCorner` | `worldPos = (2.5, 2.5)`（开局）→ `Pos() == (0,0)` |
| 3 | `ClampAtRightBottomCorner` | `worldPos = (199.5, 199.5)` → `Pos() == (map - view)` |
| 4 | `AxesClampIndependently` | 一轴被夹、另一轴居中 |
| 5 | `DegenerateWhenViewEqualsMap` | 该轴 `view == map` → pos = 0 |
| 6 | `DegenerateWhenViewLargerThanMap` | 该轴 `view > map` → pos = 0（且不依赖 `Clamp` 的越界行为） |
| 7 | `ZeroViewportIsSafe` | `view <= 0` → 不崩、pos = 0 |
| 8 | `OriginIsFloorOfPos` | 含**负数** pos（把相机推到地图外）→ `OriginCol() == FloorToInt(pos.x)` |
| 9 | `OddViewportUsesIntegerHalf` | `viewW = 81` → 连续移动玩家时 `@` 的屏幕列**恒定**（钉坑 4） |
| 10 | `PlayerColumnStaysAtViewCenter` | 未夹取时屏幕列恒为 `viewW / 2` |
| 11 | **`PlayerColumnMatchesItsTileColumn`** | **E-8 守卫**：`ToIndex(px) - OriginCol()` == 该 tile 被画到的列（两条路径必须同源） |
| 12 | **`PlayerStaysOnScreenInsideMap`** | 扫描 `x ∈ [0, 200)`（步长 0.5）→ `ToIndex(x) - OriginCol() ∈ [0, viewW)`；`y` 同理（"`@` 始终在画面内"的机器形式） |
| 13 | `ResizeThenFollowUsesNewViewport` | 换视口尺寸后再 `Follow` → 新尺寸下夹取（E-12 的 S6 那一半） |
| 14 | `FollowIsPureFunctionOfInputs` | 同输入调两遍 → 结果相同（无隐藏状态） |
| — | （编译期）`static_assert(sizeof(Camera) == 8)` | 与 S5 的 `sizeof(Entity) == 24` 同一套路 |

### 10.2 正向验收

```powershell
cmake --build build --config Debug                      # 期望 0 warning
.\build\bin\Debug\mini_tests.exe Camera                 # 期望 14 passed
.\build\bin\Debug\mini_tests.exe                        # 期望 104 passed, 0 failed
.\build\bin\Debug\mini_tests.exe Nope                   # 期望 (104 case(s) registered)、退出码非 0
ctest --test-dir build -C Debug --output-on-failure     # 期望 1/1
cmake --build build --config Release                    # 期望同样 0 warning
.\build\bin\Debug\MiniEngine.exe                        # 人眼验收（下表）
```

**人眼验收**（**必须在经典控制台窗口**里跑，不要用 VS Code 内置终端 / Windows Terminal —— 见 `TECH_DEBT` C10）：

| 观察点 | 期望 |
| --- | --- |
| 往右直走 **超过 40 格** | 地图开始滚动。⚠️ 40 格之前三者（含 snap）看不出差别，**这一步必须走够** |
| 滚动中 | 柱阵**每 10 格一棵保持对齐**；地标楼（`x=30..36, y=5..8`）是唯一参照（柱阵平移 10 格会重合） |
| `@` 的位置 | **恒定在画面正中**（未夹取时） |
| 走到地图右下角 | 相机停住、地图不动，`@` 自己走到画面右侧/底部；**不出现错位的墙** |
| 继续走出地图（`x >= 200`） | `@` 离开画面 —— **预期**（碰撞是 S7） |
| 顶栏 `Rows` | 滚动时接近满行；**松手立刻掉回 1~2** |
| HUD | `pos` / `tile` / `Σdt` 继续自证；`tile` 显示世界格（**不随相机变化**） |
| `W/A/S/D` + `Q` | 与 S5 一致（相机不影响输入） |

### 10.3 反向验证（4 条，**每条都必须看到红**，跑完必须还原）

> ➖ **2026-10-02 本人决定：跳过不做**（不是"没来得及"，是明确不做 —— 与 S5 的 A6 同类处置）。
> 记录代价与补救：这 4 条原本要证明"用例真的在盯着这四件事"；跳过之后，**效力未经现场确认**。
> **触发条件（任一出现就该回来补跑，至少补 R1 / R2）**：
> ① 有人改 `Camera::Follow` 的夹取 / 退化分支，或改 `OriginCol/OriginRow` 的取整；
> ② 有人改 `DrawMap` / `DrawEntities` 的坐标换算（R2 是 E-8 的直接守卫）；
> ③ 出现"画面错位 / `@` 与格子不同列 / 边界闪"这类现象时，先跑 R1、R2 定位。
> ⚠️ 另：**R3 的破坏方式要按落地偏差 1 改**——原来的"`(int)` 截断"在 `pos ≥ 0` 时不可观测，
> 应改成"`static_cast<int>(m_pos.x + 0.5f)`（模拟 round）"。

| # | 怎么破坏 | 哪条用例应该变红 | 说明 |
| --- | --- | --- | --- |
| R1 | 去掉 `Follow` 里的两处 `Clamp` | 3 / 12 | 相机越界 → 屏幕边缘出现错位墙 |
| R2 | 实体列改成 `ToIndex(pos.x - cam.Pos().x)`（A.5 原式） | **11** | `@` 与它脚下的 tile 差一格（E-8 的原始症状） |
| R3 | `OriginCol()` 改成 `static_cast<int>(m_pos.x + 0.5f)`（模拟 round；**不是 `(int)` 截断**） | 10 / 11 / 12 | 取整方向错了 → `@` 不再恒在正中 |
| R4 | 居中偏移改成 `viewW * 0.5f` | 9 | 奇数宽下 `@` 在两列之间跳 |

（可选第五条，与 S5 的反向验证 4 相关：把 `Game::OnUpdate` 里的 `Normalized()` 注释掉 →
`Game.DiagonalMoveIsNotFaster` 应变红 —— 它证明"大画布修法"让那条用例重新有了效力。）

### 10.4 交付后的自检

- [ ] 新头文件**被 `test_camera.cpp` include**（S5 落地偏差 7 的教训：漏 include 会连报 100 条）
- [ ] 依赖自检：`render/Camera.h` 里 `windows.h` / `#ifdef` / `platform/` / `world/` **0 命中**
- [ ] 生产 0 调用的公开函数（`Pos()`）**有一行注释回答"为什么存在"**（`TECH_DEBT` B5 纪律）
- [ ] `game/` 里没有第二处 `FloorToInt(cam...)`（取整只发生在 `OriginCol/Row` 内）
- [ ] `tests/` 里没有把 `game` 的私有细节写进断言（只碰 `Camera` 的公开接口）
- [ ] 反向验证 4 条**都跑过**，每条都有用例标红，**现场已还原**
- [ ] 第 11 节的 5 份文档 + `TECH_DEBT` 全部同步
- [ ] 草稿期临时改动零残留（`grep -rn "\[S6"` 之类）

---

## 11. 需要同步的文档

| 文档 | 改什么 |
| --- | --- |
| `doc/MiniEngine_PLAN.md` **A.5** | `screen = floor(world - camera.pos)` → 改成**整数原点**写法（`ToIndex(world) - FloorToInt(camera.pos)`），并注明"两者在 `camera.pos` 非整数时**差一格**，S6 起统一为整数原点" |
| `doc/MiniEngine_PLAN.md` **A.2 目录树** | `render/Camera.h` 标为**已落地**；注明 `Viewport.h` 本阶段不建（见第 1 节"明确不做"与第 12 节差异 4） |
| `doc/MiniEngine_PLAN.md` **S6 小节** | 收口时标 ✅ + 回填实测数字（用例数、命令结果） |
| `doc/S5_CHECKLIST.md` **:793** | "往右走约 40 格 `@` 移出屏幕 —— 预期行为" → 标注**已作废**（S6 已修） |
| `doc/TECH_DEBT.md` | E-8 相关关闭；新增 C 组一条"`ScreenToWorld` 只有契约没有实现（S9 触发）"；`A2`（顶栏量化基线）仍缓做 |
| 本文件 | 落地偏差表、进度板、实测数字（**追加，不改历史**） |

---

## 12. 与 `PLAN` 原文的差异（**必须同步**）

| # | PLAN 写的 | 本阶段落地 | 为什么 |
| --- | --- | --- | --- |
| 1 | 目标含"**平滑跟随**"，实现含 "`lerp` 跟随" | **不做平滑**，`Follow` 是纯 snap | 本人 2026-10-02 决定（决策 1）：本阶段风险是 E-8，不是手感；平滑的升级路径与触发条件写在第 8 节 |
| 2 | `Camera{ Vec2 pos; Vec2 target; }` | **只有 `pos`**，不建 `target` | snap 下 `target` 是临时量、0 调用者（`TECH_DEBT` B2）；加平滑时再补 |
| 3 | `Camera` 要实现 `WorldToScreen` / `ScreenToWorld` | 只提供**整数原点** `OriginCol/OriginRow`；`ScreenToWorld` 只有契约 | 两条取整路径正是 E-8 的成因（6.4 / 7.2） |
| 4 | A.2 目标态含 `render/Viewport.h` | **不建** | S6 只需要"四个 int"；0 调用者（`TECH_DEBT` B2） |
| 5 | "只遍历可见 tile" | **已经满足**（S5 的 `DrawMap` 就是屏幕空间遍历） | S6 不需要改遍历结构，只换索引来源 |
| 6 | 坑 2："相机移动导致整屏抖动 → 应对相机做 `floor`" | 由"**整数原点**"设计从根上消掉（坑 11） | 相机 pos 是浮点，但渲染只用 `FloorToInt(pos)` |

---

## 13. 本阶段的"学习点"备忘（给自己看的）

1. **"相机"在 1 格 = 1 字符的终端里到底能做什么**：它不可能提供连续的像素移动
   （那要等 `▀▄` 半块字符）。它能做的是决定"`@` 停在画面哪个位置"和"地图什么时候整格平移"。
   把这层看穿之后，"平滑"就不再是审美问题，而是"相位差 + 参数"。
2. **同一个事实只能有一个来源**：相机不持有尺寸（6.3）与"取整只发生一次"（6.4 / 7.2）
   是同一条纪律的两次应用 —— 目标都是"消灭第二条能各自漂移的路径"。
3. **夹取区间的下界可能大于上界**：`Clamp` 的前置条件（`lo <= hi`）是**调用方的责任**，
   而不是函数内部该兜的。这条在 `MathUtils.h:33` 早就写了，S6 是它第一次真的被违反。
4. **验收判据要和底层语义对齐**：`At()` 越界返回 `Wall`，所以"相机越界"的现象是
   **一片错位的墙**而不是"空白"。`PLAN` 那句"不出现世界之外的空白"在本项目的语义下是错的，
   真正能判的是"柱阵与地标楼有没有错位"。
5. **"预期行为"也会过期**：S5 写的"往右走 40 格出屏是预期"到了 S6 就变成 bug 的判定线。
   阶段收口时把这类"当时的预期"标出来，比事后猜"这算不算回归"便宜得多。
