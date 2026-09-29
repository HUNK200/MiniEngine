# S5_CHECKLIST —— World / Entity（`@` 可以移动）施工图

> 本文件是 S5 的**施工图 + 进度板 + 落地记录**。写代码前先读它；
> 实现过程中发现的偏差**追加**到「落地偏差」表，**不许回头改历史**。
>
> 引用格式：`PLAN:n` = `doc/MiniEngine_PLAN.md` 第 n 行；`A.x` / `D.x` / `E-x` = PLAN 的附录/小节/风险编号。

---

## 当前状态（2026-09-29：**方案已定，未开工**）

- **S0 / S1 / S2 / S3 / S4 均已完成并验收。** 当前 **56 个 commit**，用例 **68**，工作区干净。
- 本阶段的三条接口决策**已由本人拍板**（第 8 节），另有 **1 条加成项待定**（第 8 节末）。
- **本阶段的性质：S4 是"没有画面的阶段"，S5 正好相反 —— 它是三个"看得见"的变化叠在一起**：
  1. **`@` 从"按一次跳一格"变成"按住连续移动"**（有速度、受 `dt` 影响）
  2. **`math` 那层第一次拿到真实调用者**：`Normalized`（斜向归一化）、`FloorToInt`（世界→tile）、
     `RoundToInt`（世界→屏幕）—— 这三条在 S4 是"只有测试调用"（TECH_DEBT B5 的 D 类）
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

| # | 任务 | 对应 commit | 状态 |
| --- | --- | --- | --- |
| **S5-1** | `world/Entity.h`：`EntityId` + `Transform` + `Velocity` + `Entity` | commit 1 | ⬜ |
| **S5-2** | `world/TileMap.h/.cpp`：`Tile` + `TileMap`（含 `ToIndex` 用 floor、越界 At→Wall） | commit 1 | ⬜ |
| **S5-3** | `tests/test_world.cpp`：`TileMap` 8 + `EntityId` 2 个用例；`tests/CMakeLists.txt` 加一行 | commit 1 | ⬜ |
| **S5-4** | `world/World.h/.cpp`：Spawn/Find/Entities/Update + 延迟删除 | commit 2 | ⬜ |
| **S5-5** | `tests/test_world.cpp`：`World` 10 个用例（含 **dt 无关性**） | commit 2 | ⬜ |
| **S5-6** | **建 `game/`**：`CMakeLists.txt` ×2 + `Game.h/.cpp`（搬 `DemoGame` 并改造） | commit 3 | ⬜ |
| **S5-7** | `main.cpp` 缩到 ~35 行（只做 argv + 造对象 + Run） | commit 3 | ⬜ |
| **S5-8** | 反向验证 4 条实跑并**还原现场**（见 10.3） | commit 3 | ⬜ |
| **S5-9** | docs 收口：PLAN 标 ✅ / A.2 树 / 差异表 / `TECH_DEBT` 关 C1·C2·C4 + 两处修正 | commit 4 | ⬜ |

**待定（第 8 节末）**：`tests/` 加一个假 `Input`，把 `Game` 层也变成可离线测（+2~3 个用例）。

### 落地偏差（实现时发现的问题，逐步追加）

| # | 发现在 | 偏差 | 处置 |
| --- | --- | --- | --- |
| — | — | （开工后追加） | — |

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
| `engine/include/mini/world/World.h` | ~70 | `World` 声明（含延迟删除） |
| `engine/src/world/World.cpp` | ~75 | Spawn / Find / Update / Mark / Flush |

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
| `tests/CMakeLists.txt` | 加 `test_world.cpp`；（若做加成项）链接 `MiniEngine::game` |
| `main.cpp` | **359 → ~35 行**（只留 argv 解析 + 造对象 + `Run`） |
| `engine/CMakeLists.txt` | 源文件列表加 `src/world/TileMap.cpp`、`src/world/World.cpp`（**头文件不用加**，见坑 #12） |

**新增（测试与文档）**

| 文件 | 内容 |
| --- | --- |
| `tests/test_world.cpp` | 3 个 suite、**20 个用例**（`TileMap` 8 / `EntityId` 2 / `World` 10） |
| `doc/S5_CHECKLIST.md` | 本文件 |

## 3. 依赖规则（本阶段**新增 2 条边**）

```text
main.cpp  →  game  →  engine  →  标准库
                        ↑
             engine/mini/world/   ← 新层，与 core / render 平级
```

| 新边 | 说明 |
| --- | --- |
| `main → game` | `main.cpp` 只 include `game/Game.h`（不再自己 define `DemoGame`） |
| `game → engine` | `game` 用 `IMini::core::IGame` / `world::World` / `render::FrameBuffer` / `platform::Input` |
| （加成项）`tests → game` | 只有要测 `Game` 时才需要；无环（`tests → game → engine`） |

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
| 视口（S5 版） | **没有 Camera**，视口原点恒为世界 `(0,0)` | 这是 S6 的活；S5 的"视口偏移"只是渲染时的一个减法 |

## 5. 逐文件实施步骤（每步都立刻跑一次）

| 步 | 动作 | 立刻跑 | 期望 |
| --- | --- | --- | --- |
| **1** | 写 `Entity.h` + `TileMap.h/.cpp` + `tests/test_world.cpp`（前 10 个用例）+ `engine/CMakeLists.txt` 加 1 个源文件 + `tests/CMakeLists.txt` 加 1 个文件 | `mini_tests.exe TileMap` / `EntityId` / 全集 | 10 passed / 68 → **78** |
| **2** | 写 `World.h/.cpp` + 后 10 个用例 + `engine/CMakeLists.txt` 加 1 个源文件 | `mini_tests.exe World` / 全集 | 10 passed / → **88** |
| **3** | 建 `game/`（两个 CMakeLists + `Game.h/.cpp`）、`main.cpp` 缩到 ~35 行、顶层 CMakeLists 打开 `add_subdirectory(game)` | `MiniEngine.exe` | **`@` 按住 WASD 连续移动**；HUD 显示 pos/tile/Σdt/fps |
| **4** | 反向验证 4 条（10.3）+ docs 收口 | 见 10.3 | 每条都看到红、现场还原、全集回到 88 |

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
// ⚠️ `(0.5, 0.5)` 才是**第 1 格的中心**（PLAN A.5）；别和格索引混着用。
struct Transform {
    Vec2 pos{};
};

// 速度。单位 = **格/秒** —— 所以 `pos += perSecond * dt` 得到本帧位移。
// 用"格/秒"而不是"格/帧"是本阶段的核心：后者会让速度随帧率变。
struct Velocity {
    Vec2 perSecond{};
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
//    · Spawn → 会让 Entities() 返回的 span 失效（vector 可能扩容）
//    这条规矩有 3 个用例钉着，不是"注释说说"（PLAN:967 的 E-9）
// ============================================================

namespace mini::world {

class World {
public:
    World(int mapW, int mapH);

    // 分配一个新 id、推入、返回它。prototype 的 id 字段会被**覆盖**
    [[nodiscard]] EntityId Spawn(const Entity& prototype) noexcept;

    // 线性扫。找不到返回 nullptr（**永远不会"找到错的"**，见 EntityId 的注释）
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

    // 延迟删除：只登记，不动容器。重复登记同一个 id 是**幂等**的
    void MarkForRemoval(EntityId id) noexcept;

    // 真正删除：swap-erase（把队尾搬到空位）。
    // ⚠️ **会打乱实体顺序** —— 任何依赖顺序的代码都是 bug（用例钉着）
    void FlushRemovals() noexcept;

private:
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
//   2. HUD 从"15 键四态面板"换成**世界坐标面板**（pos / tile / Σdt / fps）
//      → 于是 OnRender **不再读输入**，那条 `const Input*` 缓存（TECH_DEBT C4）直接消失
//   3. 有一条真正的开局流程（OnStart 里建地图、Spawn 玩家）
// ============================================================

#include "mini/core/Application.h"
#include "mini/platform/Input.h"
#include "mini/render/FrameBuffer.h"
#include "mini/world/World.h"

namespace game {

class Game final : public mini::core::IGame {
public:
    void OnStart() override;
    void OnUpdate(const mini::core::FrameContext& ctx) override;
    void OnRender(mini::render::FrameBuffer& fb) override;
    [[nodiscard]] bool ShouldQuit() const override;

private:
    // 12 格/秒：约 7 秒穿过一屏（终端 80 列）—— 手感数字，不是技术常数
    static constexpr float kMoveSpeed = 12.0f;

    static constexpr int kWorldW = 200;   // PLAN：World = 200×200
    static constexpr int kWorldH = 200;

    mini::world::World    m_world{kWorldW, kWorldH};
    mini::world::EntityId m_player{};
    float                 m_elapsed = 0.0f;   // Σdt（秒）—— 验收自证用（见 10.2）

    // 地图布局：**确定性**写死（不随机、不读文件）。
    // 内容是"一圈边框 + 几根柱子"，够 S6 的相机看出滚动效果即可。
    void BuildDemoMap();

    // 渲染：S5 **没有相机**，视口原点恒为世界 (0,0)
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
3. player->velocity.perSecond = dir * kMoveSpeed
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
tile 下标      = floor(worldCoord)                    ← TileMap::ToIndex（S4 的 FloorToInt）
屏幕列/行      = round(world - viewportOrigin)        ← S4 的 RoundToInt
                 （S5 的 viewportOrigin 恒为 {0,0}；S6 换成 camera.pos）
屏幕行还要 +1  ← 第 0 行留给引擎顶栏
```

两种情况**故意用不同的取整**：

| 用途 | 取整 | 为什么 |
| --- | --- | --- |
| 落在哪一格 | **floor** | 格是"区间"：`[2,3)` 都属第 2 格。`(int)` 在负数下向 0 截断 → 错一格 |
| 画在哪一列 | **round** | 实体位置是**格中心**（`(0.5,0.5)` = 第 1 格中心）→ 最近的列才是对的 |

> 这正是 `PLAN` E-8 说的"画面整体偏移半格、走位比视觉慢一格"的两个来源。
> S4 已经为它准备了 `FloorToInt` / `RoundToInt` 并各带用例 —— 本阶段第一次真的用上。

### 7.3 延迟删除 + swap-erase

```text
MarkForRemoval(id):
    if id 无效 or 找不到 or 已在 m_pendingRemove:  return    # 幂等
    m_pendingRemove.push_back(id)

FlushRemovals():
    if m_pendingRemove 为空: return                          # 幂等
    for each id in m_pendingRemove:
        idx = 在 m_entities 里线性找                         # 找不到就跳过（可能已删）
        if 找到:
            m_entities[idx] = m_entities.back()              # ← swap：把队尾搬过来
            m_entities.pop_back()                            # ← erase
    m_pendingRemove.clear()
```

**三条必须理解的事**：

1. **为什么不在 `MarkForRemoval` 里直接删**：调用方很可能正在**遍历** `Entities()`
   （S9 的子弹碰撞就是这样）—— 遍历中 `erase` 会让迭代器/下标全部失效（`PLAN:967` E-9）。
   把删除推迟到遍历之外，是这个问题最便宜的解法。
2. **为什么用 swap-erase 而不是 `vector::erase`**：`erase` 要把后面所有元素**前移**（O(n) 搬运），
   swap-erase 是 O(1) 搬运。代价是**顺序被打乱**。
3. **"顺序被打乱"是语义的一部分，不是实现细节**：所以用例只钉"剩下的**集合**正确"，
   **不钉顺序**；将来任何代码若依赖 `Entities()` 的顺序，那是它自己的 bug。

## 8. 接口决策表（2026-09-29 已拍板）

| # | 问题 | 决定 | 理由与代价 |
| --- | --- | --- | --- |
| 1 | 实体标识用什么 | **`EntityId`（自增 `uint32`，永不复用）** | 下标在 swap-erase 后**静默**指向别的实体；指针会悬空。代价：查找只能线性扫（`<50` 个实体，够） |
| 2 | 本阶段做不做**删除机制** | **做**：`MarkForRemoval` + `FlushRemovals`（swap-erase）+ 3 个用例 | 只留一个空壳容器的话，"遍历中禁止增删"这条规矩**既讲不清也没法测**；S9 的子弹消亡直接复用。代价：+~40 行（PLAN:967 的 E-9 点名要"先埋好概念"） |
| 3 | **HUD 显示什么** | **世界坐标面板**（pos / tile / Σdt / fps），**去掉 15 键四态面板** | 于是 `OnRender` 不读输入 → **技术债 C4 关闭**。理由：四态面板是 S2 用来"证明状态机"的演示，它的任务已由 `test_input_state.cpp` 的 7 个用例接管；现在"按住 W 看 pos 连续变化"本身就是输入的展示。代价：S2 的调试手感没了（想找回只需把面板加回 `DrawHud`） |
| 4 | `world/` 文件划分 | **3 份**：`Entity.h`（含 Transform/Velocity）+ `TileMap.h` + `World.h` | `Transform`/`Velocity` 各只有一个 `Vec2` 成员（各约 6 行），单独成文件属于文件数膨胀，与 A.4"结构被复杂度逼出来"相冲。代价：**与 PLAN A.2 的 5 份不一致 → 记进差异表**（与 S4"6 份 math → 4 份"同类） |
| 5 | `TileMap` 越界语义 | **`At` 越界 → `Wall`**；`Set` 越界静默忽略 | 世界外都是墙 → S7 碰撞直接受益。代价：S6 的相机不能靠 `At()` 判边界 |
| 6 | 移动放在哪层 | `World::Update(dt)` 只做积分；**输入→速度**由 `game` 做 | `world` 因此不认识 `platform` → 可离线单测（第 3 节） |
| 7 | 地图内容从哪来 | `game/Game.cpp` 的 `BuildDemoMap()` **确定性**写死 | 引擎不认识"关卡长什么样"；不做文件格式、不做随机生成 |

**待定（加成项）**：`tests/` 里加一个 **~15 行的假 `Input`**（`Input` 是抽象类，塞假按键即可）
→ `Game::OnUpdate` 也能离线测，**"斜向不比直向快"从"眼睛看"变成"机器查"**。
- 好处：符合 `TECH_DEBT` C7 的精神（能离线测的就离线测）
- 代价：`tests` 多一条到 `game` 的链接边（无环）
- **状态：等本人点头**。不做也能跑，只是坑 #3 那条只能靠运行观察。

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

## 10. 验收

> **本阶段有"跑 exe 看现象"**（与 S4 相反）—— 但**主判据仍然是离线可证的**：
> dt 无关性做成单测，运行时的 HUD 只是**自证**。

### 10.1 用例清单（20 个，3 个 suite：`TileMap` / `EntityId` / `World`）

> 全部写在新文件 `tests/test_world.cpp`。
> ⚠️ 估算在实现时从 17 涨到 20（决策 2 的删除机制 +3），**实际数以落地为准**。

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

### 10.2 正向验收

```powershell
cmake --build build --config Debug                      # 期望 0 warning
.\build\bin\Debug\mini_tests.exe TileMap                # 期望  8 passed
.\build\bin\Debug\mini_tests.exe EntityId               # 期望  2 passed
.\build\bin\Debug\mini_tests.exe World                  # 期望 10 passed
.\build\bin\Debug\mini_tests.exe                        # 期望 68 → 88 passed
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
| **dt 无关性（自证）** | HUD 显示 `Σdt`，任何帧率下都满足 **`pos.x ≈ kMoveSpeed × Σdt`**（±1 格） |
| 60fps 与 240fps 对比 | 上面那条**都成立** → 这就是"100fps 和 300fps 距离相同"的现代说法 |
| 往右走约 40 格 | `@` **移出屏幕右侧** —— ✅ **预期行为**（相机是 S6），不是 bug |
| 撞墙 / 走出地图 | **不发生碰撞，能穿过去** —— ✅ 预期（碰撞是 S7） |
| 顶栏 `Rows n/n` | 仍然是个位数到几十（脏行 diff 没被破坏） |
| `Q` / `Esc` / `Ctrl+C` / 关窗口 | 正常退出，终端恢复 |

**"没坏"的确认**（S5 不该动 S1~S4 的行为）：

| 检查 | 期望 |
| --- | --- |
| 步骤 1/2 之后的 `MiniEngine.exe` | 与 S3 **逐字符一致**（那两步只加库和测试） |
| `mini_tests.exe Nope` | 仍返回非 0，且括号里的注册数变成 88（门闩 A1 有效） |

### 10.3 反向验证（4 条，**每条都必须看到红**）

> 由本人执行；也可以叫我改、你跑、我再还原。**做完必须还原现场并再跑一次确认全绿。**
> 四条影响的测试组互不相交（`World` / `TileMap` / `Game`），可以一次打完，
> 同时也验"**没有意料之外的红**"。

| # | 破坏方式 | 必须看到 | 为什么要这条 |
| --- | --- | --- | --- |
| 1 | `World::Update` 里把 `pos += perSecond * dt` 的 **`* dt` 删掉**（变成 `pos += perSecond`） | `MovementIsDtIndependent` + `MovementScalesWithSpeed` 变红 | 证明两条用例真的在测"乘了 dt"，而不是"位置变了" |
| 2 | `TileMap::ToIndex` 把 `FloorToInt` 换成 **`(int)` 截断** | **只有 `ToIndexOnNegativeFloor` 变红**，`ToIndexTruncatesTowardTile` **不变红** | 若两条都红或都不红 → 说明它们**测的是同一件事**，没钉住"负数"这个区别 |
| 3 | `World::MarkForRemoval` 改成**立即删除**（内部直接 swap-erase） | `MarkedEntitySurvivesUntilFlush` 变红 | 证明"遍历中禁止删除"这条规矩**被测到了**（不是注释说说） |
| 4 | `Game::OnUpdate` 里去掉 **`Normalized()`** | 斜向速度用例变红 | 证明"斜向不比直向快"被钉住。⚠️ **这条依赖第 8 节的加成项**：不做假 `Input` 的话它没有对应用例 → 那就只能靠运行观察，**届时记为"未验"而不是"通过"** |

**预测（实现后填实测）**：3 条或 4 条会红；每条**必须**至少有一条用例标红。
**如果某条没红** —— 说明那个守卫是摆设，要按 `PLAN` 10.3 的老规矩**重写成真正能抓住的输入**
（S4 就撞过这事：`ParallelOutsideReturnsNullopt` 的两半里只有一半真会红，见 `S4_CHECKLIST` 10.3）。

### 10.4 交付后的自检

- [ ] 三个新头文件**都被 `test_world.cpp` include**（否则没编译过，坑 #12 的伪装版）
- [ ] 依赖规矩自检（PowerShell，本机没有 ripgrep）：
      ```powershell
      Select-String -Path engine\include\mini\world\*.h -Pattern 'windows\.h|#ifdef'   # 期望无输出
      Select-String -Path engine\include\mini\world\*.h -Pattern 'platform/|render/'   # 期望无输出（world 不认识这两层）
      Select-String -Path engine\include\mini\*.h -Pattern 'include <mini/world/'      # 只应 engine 内部 + game 出现
      ```
- [ ] 所有公开接口**都有一行注释**回答"为什么存在 / 谁会用它"（决策 6 那条纪律）
- [ ] `static_assert(sizeof(Entity) == ...)` 在头文件里
- [ ] `tests/` 里没有把 `game` 的私有实现细节写进断言（只测公开接口）
- [ ] 反向验证 4 条**都跑过**，每条都有用例标红（或明确记为"未验"），现场已还原
- [ ] `doc/MiniEngine_PLAN.md` 的 S5 小节标 ✅、A.2 目录树改对、差异表写完
- [ ] `doc/TECH_DEBT.md`：关 **C1 / C2 / C4**；改 **B5 的 D 类**（`FloorToInt`/`RoundToInt`/`Normalized` 有生产调用者了；补上漏掉的 `Vec2` 查询方法）
- [ ] `Input.h` 里 `IsDown` 的注释改准（原写"S9 的持续射击会用到" → 其实 **S5 的 WASD 就用上了**）
- [ ] `AGENTS.md` 文档表补 `doc/S4_CHECKLIST.md`（漏了）与 `doc/S5_CHECKLIST.md`
- [ ] `git status` 干净

## 11. commit 切分（每个都能构建）

| # | 信息 | 内容 | 跑完应该看到 |
| --- | --- | --- | --- |
| 1 | `feat: 加入 world/Entity.h 与 TileMap（10 个单测）` | `Entity.h`、`TileMap.h/.cpp`、`test_world.cpp` 前 10 个用例、2 处 CMakeLists | 78 passed；exe 行为**不变** |
| 2 | `feat: 加入 world/World（实体容器 + dt 积分 + 延迟删除，10 个单测）` | `World.h/.cpp`、后 10 个用例、`engine/CMakeLists.txt` | 88 passed；exe 行为**不变** |
| 3 | `feat: 建 game/ 静态库，main.cpp 缩到 35 行，@ 用速度移动` | `game/` 三件套、顶层 CMakeLists、`main.cpp` | `@` 连续移动、HUD 世界坐标 |
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
| 7 | S6 相机"边界夹取" | S5 的 `TileMap::At` 越界返回 `Wall`，**但相机不能靠它判边界** | 决策 5；写在这里免得 S6 时踩 |

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
