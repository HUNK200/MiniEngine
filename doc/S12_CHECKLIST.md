# S12 施工图 —— Ray Casting（伪 3D）· v0.5「像 3D」

> **状态**：**2026-10-16 落纸 · 五个分叉已拍板（§3）· 未开工**
> 前置：S11「看得清」✅（2026-10-15，220 用例）· S11.5「送得出去」🚧（v0.4.0 已发布，剩跨机验收）
> 本图与 `PLAN` **S12 节**的关系：`PLAN` 是需求原稿；**本图是落地口径**，两者不一致处以**本图**为准（偏差见 §12）。
> 本图**不含实现代码**（按 `AGENTS.md` §1：AI 只出方案）；动手前请先读 §3 与 §7。

---

## 0. 怎么用这张图

| 你想干什么 | 看哪节 |
| --- | --- |
| 知道"要做成什么样" | §1（目标）· §9（验收） |
| 知道"为什么不那样做" | §1（明确不做）· §12（偏差）· §3（代价） |
| 动手前核对"起点是什么" | §2（前置事实 · 每条带出处） |
| 写代码 | §4（文件）· §5（接口）· §6（算法） |
| 写用例 | §8（测试计划） |
| 出问题排查 | §7（坑表） |
| 切 commit | §10 |

---

## 1. 目标与"明确不做"

### 1.1 目标

| 项 | 内容 |
| --- | --- |
| **核心** | 同一个世界（`TileMap`）→ **第一人称走廊**：DDA 网格步进 + **每列一条射线** → 墙高 ∝ 1/垂距 → 竖直画墙 |
| **切换** | 按键在 **2D / 3D** 之间来回（**默认 2D**） |
| **v0.5 判定**（`PLAN` G 节） | ① DDA 走廊画面 ② **无鱼眼** ③ 能切换 ⇒ **两个视角下的走位一致** |
| **PLAN 原稿验收 4 条** | 墙正前方 ⇒ 中心列占满**可用视口** · 转身 180° ⇒ 看到反方向 · 直线走廊**不变弧** · 无明显鱼眼 |
| **观感** | 地板/天花板有行线；墙按距离分**字形 + 颜色**两档层次 |

### 1.2 明确不做（越界即停）

| 不做 | 归属 |
| --- | --- |
| 敌人 / 子弹 / 玩家在 3D 里出现与遮挡 | **S13**（精灵 + z-buffer） |
| 纹理映射、光照模型、俯仰角、上下看、鼠标 | 终端没有鼠标；S12 只有**水平旋转** |
| 改移动/碰撞算法（`Collision`） | 3D 复用同一套 ⇒ 否则"走位一致"这条验收自动作废 |
| 抽 `Renderer2D` / `Renderer3D` 类 | 实际项目里**从来没有这一层**（渲染就是 `DrawMap`/`DrawEntities`）；只加一个分支 |
| 改 `RowDiff` / `Present` / 顶栏 | `Renderer3D` 的唯一输出仍是 `FrameBuffer` |
| **连续转身** | **后续迭代**（本阶段只做 8 档离散；接缝见 §5.2） |

---

## 2. 前置事实（逐条核过，带出处）

| # | 事实 | 出处 | 对 S12 的影响 |
| --- | --- | --- | --- |
| 1 | `Cell` 现在是 **`char` + 2×`Color` = 恰好 3 字节**，并有 `static_assert` 钉着 | `render/Cell.h:19-28` | 分叉 A 的起点：**`█▓▒░` 现在画不出来** |
| 2 | `Cell.h` 注释把"`ch` 换 `char32_t`"排在 **S13** | `render/Cell.h:11-14` | 本次**提前**做（§3 A）⇒ 要留"重新决策"的记录 |
| 3 | **唯一的字符序列化点**：`out.push_back(cell.ch)` | `engine/src/render/FrameBuffer.cpp:169` | 扩宽后必须在这里补 UTF-8 编码；**ASCII 路径要逐字节不变** |
| 4 | `Color.h` 注释写着"底层 `uint8_t` ⇒ **`Cell` 才能保持 3 字节**" | `render/Color.h:14-17` | 扩宽后这句变假话 ⇒ 必须同步 |
| 5 | 视口定义：行 0 = **引擎顶栏**（引擎在游戏之后画 ⇒ 游戏盖不掉）· 行 1..`kHudRow` = HUD · **行 `kHudRow` 同时是世界第一逻辑行** | `Application.cpp:201` · `Game.cpp:62-92` | 【分叉 D 选甲】3D **沿用同一视口** ⇒ 相机夹取/`DrawMap`/`DrawEntities`/HUD 行号**全不动** |
| 6 | `kHudRow = 2`，且有一条 `static_assert` 挡"改回 1" | `Game.cpp:92` · `:1025-1035` | 3D 可用视口 = 画布高 − 2（100×30 ⇒ **28 行**，地平线在行 16 附近） |
| 7 | 移动是**世界轴八向**（`W` 恒为北），与朝向无关 | `Game.cpp:767-770` | 3D 移动语义 = 与 2D **完全相同**（分叉 B 甲′） |
| 8 | `m_facing` 的唯一写点：按住移动键时更新；**零向量时不写**（松开保持） | `Game.cpp:787-791` | 朝向"不移动就不变" ⇒ 没有转身键就**不能原地转头**（甲′ 的由来） |
| 9 | `Facing8` = `East, NorthEast, North, NorthWest, West, SouthWest, South, SouthEast`（**顺时针排列**） | `game/Bullet.h:35-38` | 转身 = 枚举 `±1`，**不用查表** |
| 10 | **`FacingToVec2(Facing8) -> Vec2` 已存在**；反向 `DirectionToFacing(Vec2)` 也有 | `game/Bullet.h:67-82` | 相机 `dir` **现成** ⇒ 射线器输入齐了 |
| 11 | `Facing8` 住在 **`game/`**；`engine` 绝不 include `game` | `game/Bullet.h:35` | 【分叉 C 选甲】射线器入参必须是 `Vec2`，不能收 `Facing8` |
| 12 | `KeyCode.h` 映射了 `W A S D Q E R T` + `Space/Enter/Escape/Tab` + 方向键 + `F1~F4` + `F7`；**`Z`/`C` 空着**；⚠️ **`E` 已映射但全仓 0 处使用** | `platform/KeyCode.h:83-113` | 转身键用 `Z`/`C`（字母键不在全局热键靶场） |
| 13 | `F1`/`F3` 在本机**被别的软件抢占**（拿到 `Pressed` 都做不到）⇒ 新键可达性**只能人眼验** | `KeyCode.h:30-38` · `TECH_DEBT` **C37** | ⚠️ `F5` 是高风险键（**VS Code 里 F5 = 启动调试**）⇒ 见 §7 坑 1 |
| 14 | `tests/test_game.cpp` 里 **14/39 条用例**读屏幕（`ScreenText`/`FindChar`）；读屏点还有 **6 处在文件级 helper** 里；`StepFrame` 被调 **53 次**、`WalkPlayerNextToEnemy` **18 次** | 刚扫（2026-10-16）· `test_game.cpp:157,442,586,1524` | ⇒ **3D 绝不能是默认**；且**新 3D 用例不许用 `ScreenText`**（§8） |
| 15 | `ScreenText` 按"**1 格 = 1 `char`**"拼字符串（`push_back(fb.At(x,y).ch)`） | `test_game.cpp:449-452`（+ 3 处同类取样） | 扩宽后**这 4 处编译不过** ⇒ 必须改；语义上"1 格 = 1 字节"的前提在**非 ASCII 出现时**失效 |
| 16 | `RowDiff::Compose` 靠**比较序列化字符串**判脏行；`Application::Run` 每帧先 `Clear()` | `render/RowDiff.cpp` · `Application.cpp` | ① 扩宽**不用改 RowDiff**（编码对就行）；② 3D 下每帧整屏都变 ⇒ **`Rows` 会顶满**（预期，§7 坑 14） |
| 17 | "什么是墙"的判据**已经存在**：`world/Collision.h` 的 `IsSolid()`（且"**越界 = 墙**"） | `world/Collision.h`（S7 定） | 射线器**复用它**，不许新写第二份 |
| 18 | 现状用例 **220** 条 / 23 suite（`Game` 39） | `grep -c "^MINI_TEST"` | S12 后预计 **≈237**（220 + 17；§8） |

---

## 3. 五个分叉的拍板结果（2026-10-16，本人拍板）

| 分叉 | 决定 | 选它的**代价**（照实记） |
| --- | --- | --- |
| **A 明暗怎么表达** | **甲：现在就把 `Cell` 扩宽到 `char32_t`**（把 `Cell.h` 注释里"留给 S13"的事提前） | ① 改 `Cell`/序列化/两处 `static_assert`/4 处测试取样；② `Color.h` 注释要同步；③ **必须保证 ASCII 路径逐字节不变**（否则 `Rows` 与既有判据全乱）；④ 内存 3→8 字节（3000 格 = 9→24 KB，**可忽略**） |
| **B 转向粒度** | **甲′：沿用 `Facing8` 当唯一朝向源 + 新增 `Z`/`C` 原地转 45°**（`Pressed` 边沿触发，**2D/3D 都生效**，**受暂停门控**） | 45° 一档（不连续）；多两个键位；**后续要向"连续转身"迭代**（接缝见 §5.2） |
| **C 射线器放哪** | **甲：`engine/include/mini/render/RayCaster.h`（header-only 纯函数）**，入参 `Vec2`，复用 `IsSolid` | `game` 侧要写换算（1 行）；`engine/render` 要 include `engine/world`（同层允许） |
| **D HUD/顶栏** | **甲：3D 沿用同一视口（HUD 两行照旧）** | 3D 可用 28 行（不是 30）；验收口径要写"占满**可用视口**" |
| **D 小项 1** | `T`（overlay）/ `F7`（敌人面板）在 3D 下**继续可用** | 面板会盖住 3D 画面一部分（预期） |
| **D 小项 2** | `F2`（几何层）在 3D 下**按了仍翻转标志，但 3D 不读它** | 3D 下按 `F2` "没反应"是**正常**（台账要写，免得将来当 bug） |
| **E 地板/天花板** | **乙：静态行线**（地平线以上 `-`、以下 `.`，固定颜色） | ~10 行；行线一旦弯了会被一眼看出（**这是好事**：鱼眼验收变容易） |

**"后续迭代"清单**（不在本阶段）
- **连续转身**（`Z`/`C` 按住连续转）：把 §5.2 的 `CameraDir()` 换成 `{cos(angle), sin(angle)}` + 一个 float 角度；⚠️ 要同时定"`Facing8` 与浮点角谁优先"的同步规则。
- 地板**渐变**（按行算距离 → 颜色档位，~20 行）。
- 3D 里画精灵（S13）。

---

## 4. 文件清单

| 文件 | 动作 | 内容 |
| --- | --- | --- |
| `engine/include/mini/render/RayCaster.h` | **新增** | `RayCamera` / `RayHit` / `CastColumn()` / `WallHeightRows()` / `ShadeIndex()` —— header-only 纯函数 |
| `engine/include/mini/render/Cell.h` | 改 | `char ch` → `char32_t ch`；两处 `static_assert`（`sizeof==8` / `alignof==4`）；决策注释 |
| `engine/include/mini/render/Color.h` | 改注释 | 那句"`Cell` 才能保持 3 字节"要重写 |
| `engine/include/mini/render/FrameBuffer.h` | 改 | 新增 `DrawGlyph(int,int,char32_t,Color,Color)`；`Set/DrawRect/Clear` 的 `char` 参数**保持不变**（字面量自动提升） |
| `engine/src/render/FrameBuffer.cpp` | 改 | `AppendRowTo` 里加 UTF-8 编码（`<0x80` 走**原分支**）；`kBytesPerCellGuess` 改名/改注释 |
| `engine/include/mini/platform/KeyCode.h` | 改 | `enum` 加 `Z, C, V`（+ 可选 `F5`）；`map[0x5A]=Z` / `map[0x43]=C` / `map[0x56]=V` / `map[0x74]=F5`；注释写清"为什么优先字母键"（C37） |
| `game/include/game/Game.h` | 改 | `enum class ViewMode { Mode2D, Mode3D };` + `m_viewMode = Mode2D` + `ViewMode()` 访问器 + `CameraDir()`（**唯一方向来源**）+ `DrawRaycastWorld()` 声明 |
| `game/src/Game.cpp` | 改 | `kTurnCWKey/kTurnCCWKey/kViewKey` 三个常量；`OnUpdate` 加"转身（门控内）"与"切视角（门控外）"两小段；`OnRender` 加**一个分叉**；`DrawRaycastWorld()` 实现（墙 + 地板/天花板） |
| `tests/test_raycaster.cpp` | **新增** | 新 suite `Raycaster`（~10 条，§8） |
| `tests/test_frame_buffer.cpp` | 改 | 尺寸断言 3→8；新增"ASCII 序列化逐字节不变"与"非 ASCII 编码成 UTF-8" |
| `tests/test_game.cpp` | 改 | 4 处取样改 `char32_t`（编译）；新增 4 条用例（§8）；`ScreenText`/`FindChar` 加"仅 ASCII"契约注释 |
| `tests/test_input_state.cpp` | 改 | 加 `KeyFromVirtualKey(0x5A/0x43/0x56)` 映射断言 |
| `engine/CMakeLists.txt` · `tests/CMakeLists.txt` | 改 | 登记新 `.cpp`（`RayCaster.h` 是 header-only ⇒ 只在 tests 里被 include） |
| `doc/PLAN` · `README` · `TECH_DEBT` · `AGENTS.md §2` | 改 | §11 文档同步 |

**依赖方向**（不许违反）
```
engine/render/RayCaster.h  →  engine/world/TileMap.h · engine/world/Collision.h · engine/math/Vec2.h
game/                      →  engine/...（单向）
engine/                    ✗ 绝不 include game/
```

---

## 5. 接口设计

### 5.1 `engine/include/mini/render/RayCaster.h`（纯函数 · header-only）

```cpp
namespace mini::render {

struct RayCamera {                 // 相机只由"位置 + 方向 + 视野"描述
    math::Vec2 pos{};              // 玩家中心（世界坐标，含亚格精度）
    math::Vec2 dir{1.0f, 0.0f};    // 单位方向（⚠️ 由调用方保证归一化）
    float      fovTan = 0.66f;     // 半相机平面长度（≈66° FOV）
};

struct RayHit {
    float dist  = 0.0f;            // 沿射线的**斜距**（DDA 原始结果）
    float perp  = 0.0f;            // ★ 已乘 cos 校正的**垂距** —— 画墙高度**只用它**
    int   tileX = 0, tileY = 0;    // 命中格
    bool  sideX = true;            // true = 命中 X 面（东西向墙面）
    bool  hit   = false;           // false = 一路走到地图外（"越界 = 墙"由 IsSolid 兜）
};

// 单列：col ∈ [0, cols)，内部 cameraX = 2*col/cols − 1
[[nodiscard]] RayHit CastColumn(const world::TileMap& map, const RayCamera& cam,
                                int col, int cols) noexcept;

// 投影：垂距 → 墙在屏幕上占多少行（纯数学，不认识视口/顶栏）
[[nodiscard]] float WallHeightRows(float perp, int viewH, float cellAspect) noexcept;

// 距离 → 明暗档位（0 = 最近最亮）；两档手段（字形 + 颜色）共用它
[[nodiscard]] int ShadeIndex(float perp) noexcept;   // 0..kShadeCount-1
inline constexpr int kShadeCount = 4;

} // namespace mini::render
```

**约定**
- `CastColumn` **不 clamp 输入**，但**必须在内部 clamp 输出**（贴脸/超远）⇒ 见 §7 坑 5/6。
- "什么是墙"**只走 `world::IsSolid()`**（含"越界 = 墙"），**不许**在本文件里写 `tile == Tile::Wall`。
- `WallHeightRows` **不认识** `kHudRow`/顶栏/HUD —— 行数换算与地平线定位是**调用方**的事（`game`）。
- `ShadeIndex` 的阈值是**观感参数**（`constexpr` 表，看着调），但**必须单调**（近亮远暗）⇒ 有用例钉。

### 5.2 `game` 侧接线

```cpp
// Game.h（私有）
enum class ViewMode { Mode2D, Mode3D };
ViewMode m_viewMode = ViewMode::Mode2D;    // ⚠️ 默认 2D —— 既有 14 条读屏用例的生命线

// ★ 唯一的方向来源：将来换"连续转身"只改这一个函数体
[[nodiscard]] math::Vec2 CameraDir() const noexcept;   // 现在 = FacingToVec2(m_facing)

// Game.cpp（OnUpdate）
// ③c 转身：**受门控**（与移动键同一组 policy 开关）—— 暂停时世界冻住 ⇒ 不许转身
//      if (input.Get(kTurnCCWKey) == Pressed) { m_facing = StepFacing(m_facing, -1); }
//      if (input.Get(kTurnCWKey)  == Pressed) { m_facing = StepFacing(m_facing, +1); }
// ④f 切视角：**不受门控**（视图行为，同 `T`/`F2`）—— 暂停时也能切
//      if (input.Get(kViewKey) == Pressed) { m_viewMode = (m_viewMode == Mode2D) ? Mode3D : Mode2D; }

// Game.cpp（OnRender）—— **只加一个分叉**，其余顺序（HUD/面板/banner）完全不动
if (m_state != GameState::Title) {
    if (m_viewMode == ViewMode::Mode3D) { DrawRaycastWorld(fb); }
    else                                { DrawMap(fb); DrawEntities(fb); }
    DrawHud(fb);
    ...
}
```

**键位决策（⚠️ 偏差 + 双保险）**

| 键 | 作用 | 理由 |
| --- | --- | --- |
| **`V`**（主键） | 切 2D/3D | 字母键**不在全局热键靶场**（`T` 的成功先例；`F1`/`F3` 被抢的教训 C37） |
| **`F5`**（同时映射） | 同上 | `PLAN` 原稿键；但 ⚠️ **VS Code 里 F5 = 启动调试** ⇒ 开发时按可能被 IDE 吃掉 ⇒ 只当备份 |
| **`Z` / `C`** | 逆 / 顺时针转 45°（原地） | 空闲字母键；`Z`→`C` 相邻可及 |

### 5.3 `Cell` 扩宽（分叉 A）的口径

| 项 | 定 |
| --- | --- |
| 字段 | `char32_t ch = U' '`（**一个格 = 一个码位**；不做 CJK 宽字符，不做组合符） |
| 大小 | `4 + 1 + 1` → 对齐后 **8 字节**（`static_assert` 改成 8 / 4） |
| 序列化 | `ch < 0x80` ⇒ **原样 `push_back`（字节级等价，老路径）**；否则写 UTF-8（1~4 字节） |
| 新 API | `DrawGlyph(x, y, char32_t, fg, bg)` —— **不动** `Draw(string_view)` 的 ASCII 语义 |
| 契约 | **"ASCII 画面序列化结果逐字节不变"** 要有一条用例钉住（这是唯一能防止 `Rows` 被悄悄改坏的东西） |
| 文档 | `Cell.h` 保留"S1 决策 2"原文 + 加"2026-10-16 重新决策：提前扩宽，理由与影响面见施工图 §3/§5.3" |

---

## 6. 算法步骤（伪代码）

### 6.1 主循环（每帧，3D 模式）

```text
viewH  = fb.Height() - kHudRow          // D 选甲：沿用同一视口定义（28 @100×30）
horizon = kHudRow + viewH / 2
cam = RayCamera{ player.pos, CameraDir(), kFovTan }

// ① 地板/天花板（E 选乙）：**按行填一次**，不必每列填
for row in [kHudRow, horizon)         → 整行填 '-'（天花板）
for row in [horizon, fb.Height())     → 整行填 '.'（地板）

// ② 每列一条射线
for col in [0, fb.Width()):
    hit = CastColumn(map, cam, col, fb.Width())
    lineH = WallHeightRows(hit.perp, viewH, kCellAspect)     // 行数（float）
    top   = horizon - lineH/2 ; bottom = top + lineH
    shade = ShadeIndex(hit.perp)
    在这列的 [max(top,kHudRow), min(bottom, h-1)] 范围内画：字形 = kShades[shade]，颜色 = kShadeColors[shade]
```

### 6.2 DDA（`CastColumn` 内部）

```text
cameraX = 2*col/cols - 1
rayDir  = cam.dir + plane * cameraX          // plane = 垂直于 dir、长 fovTan
mapX = floor(pos.x) ; mapY = floor(pos.y)
deltaDistX = |1 / rayDir.x| ; deltaDistY = |1 / rayDir.y|        // ⚠️ 除零：用大数兜（见坑 5）
stepX = sign(rayDir.x) ; sideDistX = (stepX>0 ? (mapX+1-pos.x) : (pos.x-mapX)) * deltaDistX   // ⚠️ 坑 3
（Y 同理）
循环（上限 = 地图宽+高，防死循环）：
    若 sideDistX < sideDistY ⇒ 前进 X 格（sideX = true, dist = sideDistX, sideDistX += deltaDistX）
    否则                    ⇒ 前进 Y 格（sideX = false, ...）
    若 IsSolid(map.At(mapX, mapY)) ⇒ 停（越界也由 IsSolid 判实心 ⇒ "越界 = 墙"天然成立）
perp = dist * cos(rayAngle - camAngle)       // ★ 分叉的核心：漏了 ⇒ 鱼眼（坑 2）
```

> **`cos` 的等价写法**：`perp = dist * |dot(rayDirNormalized, cam.dir)|`，或直接用"相机空间下的距离"（把 `dist` 拆成沿 `dir` 的分量）。三种写法等价，**选一种并在注释里写清为什么**（否则后人会以为漏了）。

### 6.3 墙高与纵横比（坑 7）

```text
WallHeightRows(perp, viewH, aspect) = viewH / max(perp, kMinPerp) * aspect
kCellAspect ≈ 2.0    // 终端字符格 ≈ 1:2（宽:高）—— 没有理论值，只能看着调
```

---

## 7. 坑表

| # | 坑 | 症状 / 判据 | 对策 |
| --- | --- | --- | --- |
| 1 | ⚠️ **键可达性**：`F5` 在 VS Code 里是"启动调试"；`F1`/`F3` 在本机已被抢 | 按下去**程序收不到**（`Pressed` 永远为假） | 主键 `V` + 备份 `F5`；**只能人眼验**（C37）；发现不通就在 `Game.cpp` 的 `kViewKey` 一处换掉 |
| 2 | **漏 `cos` 校正** | 画面中间凸成球面（`PLAN` 的"90% 判据"） | 机器用例 `PerpIsEqualAcrossColumns` 先钉住；再做反向验证 R1（§9.3） |
| 3 | **`sideDist` 初值算错** | 墙面**缝隙/错位**（`PLAN` 坑表原文） | 用例 `AxisAlignedHitDistance`（正东/正北各一条）；反向验证 R2 |
| 4 | **`rayDir` 未归一化** | 距离整体偏一个常数 ⇒ 墙高系统性不对 | `perp` 用例期望值按**几何距离**算，不按"看起来"算 |
| 5 | **除零 / 贴脸** | `perp → 0` ⇒ 行数爆 → 越界写 | `max(perp, kMinPerp)`（如 0.01f）；`rayDir.x == 0` 时 `deltaDistX` 用 `1e30f` |
| 6 | **超远 / 死循环** | DDA 一直走不到墙 | ① `IsSolid` 让越界即实心 ⇒ 必然终止；② 仍加**步数上限**兜底 |
| 7 | **字符格不是正方形** | 墙"胖/瘦得离谱" | `kCellAspect`（≈2.0）乘在墙高上；**只能看着调** ⇒ 反向验证 R3 |
| 8 | **ASCII 路径被改坏**（分叉 A 的最大风险） | `Rows` 画像/既有渲染判据**悄悄**变化 | `ch < 0x80` 走老分支 + **逐字节回归用例**（§8.2 第 1 条） |
| 9 | **多字节字符与测屏 helper 冲突** | `ScreenText` 按"1 格 = 1 字节"拼串 ⇒ 长度与格子数不再对应；非 ASCII 进屏后**一切"数一数"的判据失效** | ① 3D 默认关（既有 14 条用例的生命线）；② **新 3D 用例禁止用 `ScreenText`**，改按格取 `char32_t`；③ helper 加"仅 ASCII"契约注释 |
| 10 | **conhost 光栅字体不认 `█`** | 屏幕上出现 `?`/方框 | 人眼验收项；若中招 ⇒ 换字体（Consolas/新终端）或退回 ASCII 字形（一行常量表的事） |
| 11 | **3D 下 `F2` 无反应**被误当 bug | 按 `F2` 画面不变 | 台账写明"`F2` 改的是 2D 格底，3D 不读"（§3 D 小项 2） |
| 12 | **暂停时能转身** | `ESC` 后按 `Z`/`C` 画面还在转 ⇒ 破坏"暂停 = 世界冻住"（S11 人眼项 12） | 转身键挂在**门控内**；用例 `TurnIsGatedByPause`；反向验证 R4 |
| 13 | **两侧 `rays` 边界**：`cameraX = ±1` 时 `rayDir` 退化 | 最左/最右列画歪或越界 | `col ∈ [0, cols)` 严格半开；边界列要有用例 |
| 14 | **3D 下 `Rows` 顶满**（每帧整屏都变） | 顶栏 `Rows n/n` 分母=分子；脏行差分**失效**（不是 bug，是 3D 的必然） | 写进台账；顺带成为"3D 真的在动"的**判据** |
| 15 | **转身键与 2D 手感相互影响** | 2D 里 `dir` 字形会变（**预期**）；但若有人把转身写成"也改变位置"就错了 | 用例钉"转身**不动位置**、不改 `sumdt`" |
| 16 | **HUD 位置行被 3D 墙盖住** | 3D 画在行 `kHudRow..`，HUD 后画盖住第 2 行 ⇒ 正常 | 验收口径写"占满**可用视口**"（28 行），别按 30 判 |
| 17 | **`WallHeightRows` 的 `float→int` 截断** | 墙顶端/底端**差一行** ⇒ 远墙闪 | 统一用"半开区间 + `floor/ceil` 各一次"，并在注释里定死；用例钉住远处墙至少 1 行 |
| 18 | **`KCellAspect` 被当"物理正确"** | 有人按 2.0 精确推出别的常数 | 注释写死"这是观感参数，不是物理量；换字体/字号可能就要改" |

---

## 8. 测试计划

### 8.1 新 suite `Raycaster`（纯函数，~10 条）

| # | 用例 | 钉什么 |
| --- | --- | --- |
| 1 | `AxisAlignedHitDistance` | 正东/正北看一堵墙 ⇒ `perp` = 到墙面的**垂直距离** |
| 2 | ★ `PerpIsEqualAcrossColumns` | **同一面墙：`col = 0` / 中间 / `cols-1` 的 `perp` 相等**（**鱼眼校正的机器判据**） |
| 3 | `DiagonalHitFindsCorrectTile` | 45° 朝向下命中格正确 |
| 4 | `SideFlagIsCorrect` | `sideX` 正确（东西向 vs 南北向墙面） |
| 5 | `WallHeightShrinksWithDistance` | 单调递减 + 远处至少 1 行 |
| 6 | `AspectScalesLinearly` | `kCellAspect` 翻倍 ⇒ 行数翻倍 |
| 7 | `TinyDistanceIsClamped` | 贴脸不溢出、不除零 |
| 8 | `OutOfBoundsCountsAsWall` | 越界 = 墙（与 S7 口径一致） |
| 9 | `ShadeIndexIsMonotonic` | 近亮远暗 + 边界值落在 `[0, kShadeCount)` |
| 10 | `EdgeColumnsDoNotCrash` | `col = 0` 与 `col = cols-1` 安全 |

### 8.2 `Cell` 扩宽的回归（分叉 A 的护栏，2~3 条）

| # | 用例 | 钉什么 |
| --- | --- | --- |
| 1 | ★ `AsciiSerializationIsByteIdentical` | 一段已知 ASCII 画面 ⇒ 序列化结果**逐字节**等于手写期望（**唯一能防"扩宽悄悄改行为"的东西**） |
| 2 | `CellIsEightBytes` | `sizeof==8` / `alignof==4`（原断言改值） |
| 3 | `NonAsciiGlyphEncodesAsUtf8` | `U'█'` ⇒ 3 字节 `E2 96 88` |

### 8.3 `Game` 侧新用例（4 条）

| # | 用例 | 钉什么 |
| --- | --- | --- |
| 1 | `ViewKeyTogglesRenderMode` | `V` 切换；切回 2D 后**位置/朝向/sumdt 不变**；3D 屏与 2D 屏**不同**（证明真画了） |
| 2 | `TurnKeysRotateFacingInPlace` | 4 次 `C` ⇒ 180°；8 次 ⇒ 回原点；`Z`+`C` ⇒ 回原点；**位置与 `sumdt` 不变** |
| 3 | `TurnIsGatedByPause` | `ESC` 后按 `C` ⇒ 朝向不变（与"暂停 = 世界冻住"一致） |
| 4 | `Mode2DIsDefault` | 不按键时 `ViewMode() == Mode2D`（**守住既有 14 条读屏用例的前提**） |

⚠️ 3D 相关断言**不许用 `ScreenText`**（坑 9）—— 用 `fb.At(x,y).ch == U'█'` 这类按格判据。

### 8.4 不动的部分（必须保持绿）

- `Game` suite 既有 **39 条**（含 14 条读屏用例 + `StepFrame`/`WalkPlayerNextToEnemy` 两个重度 helper）
- `FrameBuffer` / `RowDiff` 既有用例（除两条尺寸断言按 §8.2 改值）
- 其余 17 个 suite

---

## 9. 验收

### 9.1 一个命令

```powershell
cmake --build build --config Debug ; .\build\bin\Debug\mini_tests.exe ; ctest --test-dir build -C Debug --output-on-failure
```

### 9.2 一个数字
**220 → ≈237 条全绿**（新增 Raycaster ~10 + Cell 回归 3 + Game 4 = **17**；suite 数 23 → **24**）· `ctest` 1/1

### 9.3 人眼验收合并清单（**阶段末一次走完**，不按步碎片化 —— 沿用 S10/S11 纪律）

| # | 项 | 判据 | ★ |
| --- | --- | --- | --- |
| 1 | `V`（及 `F5`）能切视角 | 屏幕明显变化 | ★ 键可达性只能人眼验 |
| 2 | 3D 走廊有纵深 | 墙按距离变窄变暗 | |
| 3 | **不鱼眼** | 站在长走廊中间，两侧墙**笔直**；地板/天花板行线是**水平直线** | ★ |
| 4 | 墙正前方 ⇒ 中心列占满**可用视口** | 约 28 行（不是 30） | |
| 5 | 转身 180° 看到反方向 | 按 4 次 `C` | |
| 6 | 直线走廊不变弧 | 沿走廊走一个来回 | |
| 7 | `Z`/`C` 原地转身（3D） | 位置不动、画面转 | |
| 8 | `Z`/`C` 在 2D 也生效 | HUD 的 `dir` 字形立刻变 | ★ 最快的判据 |
| 9 | `ESC` 暂停后转身无反应 | 世界真的冻住 | ★ |
| 10 | 3D 里撞墙停住 | 与 2D 手感一致 | |
| 11 | `V` 切回 2D：位置/朝向不丢 | "两种视角走位一致" | ★ v0.5 判定 |
| 12 | 3D 下 `F2` 无反应（正常）· 切回 2D 后生效 | 见坑 11 | |
| 13 | `T` overlay 在 3D 下仍可见 | 右上角面板（部分被盖） | |
| 14 | 不出现 `?` / 方框 | 若有 ⇒ 字体问题（坑 10） | ★ |
| 15 | `Rows` 在 3D 下顶满 | 预期（坑 14），**不是 bug** | |

### 9.4 反向验证（4 场：我改坏 → 你判红/绿/崩 → 逐条比对 → 还原）

| 场 | 破坏 | **预测** |
| --- | --- | --- |
| **R1** | 删掉 `perp` 的 `cos` 校正 | **人眼**看到鱼眼 + **机器** `PerpIsEqualAcrossColumns` 应红 |
| **R2** | `sideDistX/Y` 初值算错（少减 1 格） | 机器红（距离/命中格错）· 人眼**可能**看到缝隙 |
| **R3** | `kCellAspect = 1.0` | ⚠️ **预计机器全绿**（它是观感参数）⇒ **只有人眼能发现**（墙被压扁）—— 这一场是"**绿场也有信息量**"的诚实预告 |
| **R4** | 把转身键挪到门控外（暂停也能转） | 机器 `TurnIsGatedByPause` 应红 |

---

## 10. commit 切分（6 笔，每笔都能构建 + 能跑）

| # | 前缀 | 内容 | 为什么这样切 |
| --- | --- | --- | --- |
| 1 | `feat:` | **`Cell` 扩宽到 `char32_t`** + 序列化 UTF-8（ASCII 路径不变）+ 两处 `static_assert` + 3 条回归用例 + `Cell.h`/`Color.h` 注释 | A 是**结构改动**，必须**独立**先落 ⇒ 出问题能立刻分清是"扩宽"还是"3D" |
| 2 | `feat:` | **`Z`/`C` 原地离散转身**（`KeyCode` + `Game::OnUpdate` 门控内 + 3 条用例） | 与 3D **无关**、在 2D 里就能验（HUD `dir`），可独立收口 |
| 3 | `feat:` | **`render/RayCaster.h` + `tests/test_raycaster.cpp`**（纯函数，10 条） | 引擎能力先立住，**不碰 game** |
| 4 | `feat:` | **3D 接线**：`V`/`F5` + `ViewMode` + `CameraDir()` + `DrawRaycastWorld()`（**只画墙**）+ 4 条 Game 用例 | 到这一笔"能看到走廊"（D 选甲的视口复用） |
| 5 | `feat:` | **地板/天花板**（E 选乙）+ `kCellAspect` 调参 | 观感独立一笔，便于回退 |
| 6 | `docs:` | 收口：`PLAN`（S12 行 + D.2 偏差 + **S13 改写**）· `TECH_DEBT` · `README`/Release 键位 · `AGENTS.md §2` · v0.5 判定回填 | 与代码同步 |

---

## 11. 与既有模块的接口约定 + 文档同步

**复用（不许新造第二份）**

| 能力 | 用哪个 | 不许做 |
| --- | --- | --- |
| "什么是墙"（含越界） | `world/Collision.h` 的 `IsSolid()` | ✗ 在 RayCaster 里写 `tile == Wall` |
| 玩家位置/朝向 | `Game::Facing()` · `FacingToVec2()` · 玩家的 `transform.pos`（含 `frac`） | ✗ 新加第二套位置/朝向 |
| 视口行数 | `kHudRow`（D 选甲：**不改**） | ✗ 让 `kHudRow` 按模式变 |
| 脏行/提交 | `RowDiff` / `Present`（**完全不动**） | ✗ 为 3D 加第二条提交路径 |

**文档同步（commit 6）**

| 文件 | 改什么 |
| --- | --- |
| `doc/MiniEngine_PLAN.md` | S12 行标 ✅ + 落地偏差 · **§D.2 的 `AppConfig::renderMode` 改成"模式在 `game` 侧"** · **S13 行改写**（`Cell` 扩宽已提前 ⇒ S13 = 精灵 + z-buffer）· G 节 v0.5 判定 ✅ |
| `doc/TECH_DEBT.md` | ① "提前扩宽 `Cell`"的记录（理由 + 影响面）② "连续转身"列为**后续项**（触发条件：想要顺滑转身）③ 3D 下 `Rows` 顶满（观感/性能说明） |
| `README.md` | 键位表加 `V`（视角）、`Z`/`C`（转身）；"状态"行加 3D |
| `doc/AGENTS.md §2` | 加本图一行（`doc/S12_CHECKLIST.md`） |
| Release 正文 | 下一版（`v0.5.0`）时加"伪 3D"一节（本阶段不发布） |

---

## 12. 偏差预告（相对 `PLAN` 原稿，落地时按实际再补）

| # | `PLAN` 原稿 | 本图口径 | 为什么 |
| --- | --- | --- | --- |
| 1 | `AppConfig::renderMode { Mode2D, Mode3D }` | 模式放 **`Game` 私有成员** | `AppConfig` 是引擎的；"这个游戏有几种视角"不该让引擎知道 |
| 2 | `F5` 切换 | **`V` 主键 + `F5` 备份** | `F5` 在 VS Code 里是"启动调试"；`F1`/`F3` 的教训（C37） |
| 3 | "按距离选字符 `█ ▓ ▒ ░`" | **需要先扩宽 `Cell`**（原稿漏了这一环） | `Cell.ch` 是单字节，画不了 U+2588 |
| 4 | （没写）地板/天花板 | **加静态行线** | 观感 + 让鱼眼验收更容易 |
| 5 | （没写）纵横比 | **`kCellAspect ≈ 2.0`** | 终端字符格是 1:2，不校正墙会胖/瘦 |
| 6 | 提到 `Renderer2D`/`Renderer3D` | **不建这两个类型** | 实际项目从来没有这一层；只加一个分支 |
| 7 | （没写）转身 | **加 `Z`/`C` 8 档原地转身** | 没有它就不能原地转头（3D 演示价值打折） |
| 8 | 规模 ~350 行 | 本图估 **~500~600 行**（含扩宽 + 用例） | 多了 A 的结构改动与 ~17 条用例 |

---

## 13. 风险与待确认

| # | 项 | 状态 |
| --- | --- | --- |
| **T1** | `V` 在经典控制台/VS Code 终端都能收到？ | 待 30 秒实测（`F1`/`F3` 栽过两次）；`Game.cpp` 里 `kViewKey` 一处可换 |
| **T2** | `kCellAspect` 最优值（与字体/字号有关） | 待看着调；**没有理论值** |
| **T3** | `█` 在 conhost 默认字体下会不会变 `?` | 待人眼（坑 10） |
| **T4** | 3D 下整屏重绘对帧时间的影响 | 待实测（`T` overlay 看 `Game.Render`）—— 预期 <0.5 ms（100 列 × ~20 格） |
| **T5** | 3D 里"看不见自己/敌人"是否影响可玩性 | 见 §1.2：本阶段**有意不做**（S13） |
| **T6** | 转身 45° 一档是否"太跳" | 若跳得难受 ⇒ 触发"连续转身"后续项（§3 末尾） |

---

## 14. 完成的定义（本阶段收口时逐条打勾）

- [ ] `cmake --build build --config Debug` **0 warning**
- [ ] `mini_tests.exe` ≈ **237 passed, 0 failed**（suite 24）
- [ ] `ctest --test-dir build -C Debug` **1/1**
- [ ] §9.3 人眼 **15 条**通过
- [ ] §9.4 反向验证 **4 场**跑完并逐条比对（含 R3 的"预计绿场"如实记录）
- [ ] §11 的文档同步全部落地（含 **S13 行改写**）
- [ ] `v0.5` 判定（`PLAN` G 节）回填 ✅
- [ ] 每个 commit 单独 checkout 都能构建 + 能跑（`AGENTS.md` §7 第二条）

---

## 15. 排查记录：一次"人眼报告 ≠ bug"（2026-10-16，第 2 笔人眼验收期间）

**报告（原话）**："子弹碰到底部时会持续在最后一行显示，然后向右移动，直到尽头，其他方向（上面、左边）没有这个问题，只有底部有"；补充："那一排 `#` 会闪现，短暂时间后消失"。

### 15.1 结论：**不是 bug**（四条实测事实）

| # | 事实 | 依据 |
| --- | --- | --- |
| 1 | 那排 `#` 是**柱子阵**的一行（每 10 格一根、每 10 行重复）—— 这正是"`#` 的数量与位置和中间的**完全一致**"这条观察的来源 | `Game::OnStart` 第 2 段（`kPillarStep = 10`） |
| 2 | 子弹**一直在飞**：世界 **200×200**、速度 0.75 格/帧 ⇒ 从出生点 `(2.5,2.5)` 朝南要 **≈262 帧（4.4 秒）**才撞到下边界；中途"消失"只是**飞出了视口**（相机只跟玩家、不跟子弹） | `Game::kWorldW/H = 200` · `kDefaultBullet.speed ÷ 60` |
| 3 | 那排 `#`"闪"是因为**子弹正在那一行里移动** ⇒ `RowDiff` 只发变化行 ⇒ **该行每帧整行重发**；实拍顶栏 `Rows 4/39`（顶栏 + 2 行 HUD + 子弹那行）四个数正好对上 | `RowDiff::Compose` · 本人截图 |
| 4 | 只在 **VS Code 内置终端（ConPTY）** 出现；**独立 `conhost` 没有**（本人实测：换独立命令行 ⇒ 现象消失）。机制：ConPTY 报的窗口高度可能 > 实际可见行数 ⇒ 画面写到可见区之下 ⇒ 终端滚动 | 实测 `canvas=116x30` / `116x42`（跟着面板变 ⇒ 不是过时值） |

### 15.2 走到过的两条弯路（留档，别再走）

1. **按错误诊断改过一版代码**：加过"`Update` 之后再扫一遍子弹、实心就删"（编号 ⑥b）—— **已撤掉**。理由：`MoveAndCollide` 的分轴夹紧 + ⑤d 的线段采样**已经**保证"子弹进不了实心格"（判定比位移更保守）⇒ 那条兜底防的失败**不可能发生**（`AGENTS.md` §1 的"不写会怎样"纪律）。经过留在 `Game.cpp` 文件头的帧序注释里。
2. **两次预测失误**：① 把"终端滚动"列为候选甲/乙之一，又用"顶栏是否还在第一行"把它排除 —— 那个判据**不锐利**（顶栏每帧都被重写，滚动之后它会被"修好"；暴露错位的是**静止的地图行**）；② 我给的第一版用例只跑 **40 帧**就断言"场上没有子弹了"，而正确预算是 **≈262 帧**（事实 2）⇒ 那条断言本身是错的，被本人跑红抓出来。

### 15.3 落地物

| # | 落点 |
| --- | --- |
| 1 | 用例 **`Game.BulletFliesThenDiesAtTheMapBorder`**：逐帧断言"`*` 不许落在实心格上" + 400 帧后必须已经消失（钉"子弹飞行/收尾"这条**真契约**；与本次误会无关，但值得钉） |
| 2 | `S11_5_DEMO_CHECKLIST` **坑 16**：ConPTY 报高 > 实际可见 ⇒ 溢出滚动；**纪律 = 人眼验收/试玩一律走 `开始试玩.cmd`（conhost）**，VS Code 终端只用来构建 + 跑用例 |
| 3 | `TECH_DEBT` **C39**：同一条的环境说明 + **不做修**的理由 + 触发条件 |
| 4 | `Game.cpp` 文件头帧序注释里的留档段（"别再走一遍"） |
