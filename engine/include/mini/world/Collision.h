#pragma once

#include "mini/math/Vec2.h"
#include "mini/world/TileMap.h"

#include <algorithm>   // std::max
#include <cmath>       // std::ceil / std::abs

// ============================================================
// world/Collision.h —— "你占的就是你画的那一格"（S7，header-only）
//
// ⚠️ 2026-10-08 **模型改过一次**（原因见 `S7_CHECKLIST` 偏差 9 / 10）：
//
//   旧版：实体 = **1×1 的盒子**（`pos ± 0.5`）。
//     后果：盒子占 **4** 个候选格，而屏幕只画 **1** 格 → 挡住你的可能是**看不见**的那一格。
//     实测报障：`pos=(29.5, 4.84)` 时按 D 动不了 —— 盒子 y 方向已探进第 5 行 **0.34 格**，
//     而 `(30,5)` 是地标楼的格子（屏幕上 `@` 在 `(29,4)`，两者**斜着隔一格**，看着像"没贴墙"）。
//
//   新版：实体占的**就是它渲染出来的那一格** —— `ToIndex(pos)`。
//     `game` 画 `@` 用的也是 `ToIndex(pos)`，所以**"看见的 = 占的"从结构上成立**，
//      不可能再出现"被看不见的格子挡住"。
//
// 这一层只回答一个问题：**"我往前挪一点，会不会踩进实心格？踩到了该停在哪儿？"**
//
//   · 它**知道**：格子实不实心（通过 TileMap）、格边界在哪、撞上了停哪。
//   · 它**不知道**：玩家/敌人是谁（不认识 Entity / EntityId）、输入、屏幕、终端。
//   · 它**不做**：不分配、不抛异常、不记状态 —— 公开函数都是纯函数，能完全离线单测。
//
// 依赖谁：world/TileMap.h + math/Vec2.h。**不依赖 platform / render / game**（PLAN A.3）。
// 被谁用：
//   · `world::World::Update` → `MoveAndCollide`：**唯一会改变位置的调用点**
//     （`game/` 不许自己调：否则就有两条"位置怎么变"的路径，那是 E-8 的同族问题）。
//   · `game/Bullet.h` → `SweepStepCount` / `SweepSampleAt`（S9 加）：**只读**的采样，
//     **不移动任何东西**，所以不违反上面那条"位移只有一条路径"的规矩。
//     它拿采样点去问自己的两个问题（"这一点实心吗 / 有敌人吗"）。
//
// ⚠️ 三条"写错就完蛋"的约定（每条都有用例钉着，见 S7_CHECKLIST 10.1）：
//
//   1. **占格 = `ToIndex(pos)`**：于是"**跨过格边界本身不是障碍**" ——
//      只有**目标格实心**才拦。斜向的对角缝因此能穿过去（这是新模型**有意要**的行为：
//      屏幕上看着能过，就能过）。
//   2. **停在边界前 `kContactEpsilon`**：撞上时把坐标夹到"边界 ∓ ε"，
//      使 `ToIndex(接触位置)` 仍然落在**空的那一格**里。
//      ⚠️ 这就是旧版"贴面不算重叠（开区间）"在新模型里的对应物：
//      **接触位置一定不在实心格里**（用例 `StopsAtWallMoving*` 最后都断言这一点）。
//   3. **子步进仍然是正确性的一部分，不是优化**：一步 `kMaxStepPerTick`（< 1 格）→
//      一步最多跨过**一个**格边界，于是"撞上的那一格"就是 tentative 所在的那一格，
//      不需要任何扫描；也**不可能**"跳过"一整格实心格。
//      （点模型下穿刺更隐蔽：只看终点的话，一帧跨 3 格会直接飞过中间那堵墙。）
// ============================================================

namespace mini::world {

// 单次推进的位移上限（子步进）。**必须 < 1.0**：一格实心格的厚度是 1 格，
// 步子比 1 格小，才有可能"跨不过去"某整格。
constexpr float kMaxStepPerTick = 0.5f;

// 撞上之后**停在边界前多远**。它只决定"贴得多紧"，不决定"过不过得去"。
// 取 1e-4：比 float 在 |坐标| < 1e4 时的分辨率大得多（不会和浮点误差打起来），
// 又远小于一格的视觉尺度（屏幕上完全看不出这一点点缝）。
constexpr float kContactEpsilon = 1e-4f;

static_assert(kMaxStepPerTick < 1.0f,
              "子步长必须 < 1 格（一格实心格的厚度）：否则一步就能跳过一整格");
static_assert(kContactEpsilon > 0.0f && kContactEpsilon < 0.01f,
              "contact epsilon 必须是很小的正数：<= 0 会停在实心格里，太大等于留了一条缝");

// ------------------------------------------------------------
// 谓词：实心吗？（两个重载：问"某一格" / 问"这个位置占的那一格"）
// ------------------------------------------------------------

// **越界 = 实心** —— 直接转发 TileMap::At 的既定语义（TileMap.h 文件头约定 1）。
// ⚠️ 只允许在这一处回答"实心吗"：不要再写一遍 InBounds 判断（那是第二条会各自漂移的路径）。
//    好处是"角色永远走不出地图"是**免费**的（见用例 CannotLeaveMapThroughEdge）。
[[nodiscard]] inline bool IsSolidAt(const TileMap& map, int tx, int ty) noexcept
{
    return map.At(tx, ty) == Tile::Wall;
}

// 这个位置的实体**占的那一格**是不是实心。
// ⚠️ 这就是"能不能站在这儿"的全部判据 —— 没有盒子、没有其他形状（约定 1）。
[[nodiscard]] inline bool IsSolidAt(const TileMap& map, mini::math::Vec2 center) noexcept
{
    return IsSolidAt(map, TileMap::ToIndex(center.x), TileMap::ToIndex(center.y));
}

namespace detail {

// 单轴推进一步：撞上就把这一轴**夹到边界前 ε**。
//
// ⚠️ 两轴共用这一份实现（`movingIsX` 只切"取哪个坐标"）。
//    "把 +x/−x、+y/−y 四个镜像各写一遍"是坑 17 的来源（只写对一半很难看出来）。
inline void StepAxis(const TileMap& map, mini::math::Vec2& pos, float delta, bool movingIsX) noexcept
{
    if (delta == 0.0f) {
        return;   // 这一轴本步不动
    }

    float& moving = movingIsX ? pos.x : pos.y;

    // 自愈：起点就已经在实心格里（出生点摆进墙里 / 以后关卡数据改了）→ 本轴**不拦**，
    // 让它走出去。反过来做（在墙里就不许动）会造出"永久卡死"——S7 已经因为
    // "冲出地图永久卡在界外"否掉过一个方案，不再造第二个死锁。
    if (IsSolidAt(map, pos)) {
        moving += delta;
        return;
    }

    const float before    = moving;
    const float tentative = moving + delta;

    moving = tentative;                        // 先放上去，用同一个判据问"这样行不行"
    if (!IsSolidAt(map, pos)) {
        return;                                // 目标格是空的 → 接受（**跨边界本身不拦**）
    }
    moving = before;                           // 目标格实心 → 撤回，夹到边界前 ε

    // 撞上的是哪一格？—— 就是 tentative 所在的那一格。
    // ⚠️ 这一句成立**依赖约定 3**（子步长 < 1 格）：一步最多跨一个边界，
    //    所以不存在"跳过中间某格、夹到更远那一格"的情况，不需要扫描。
    const int blockedIdx = TileMap::ToIndex(tentative);

    // 向右 / 向下 → 停在它的**左/上边界**前；向左 / 向上 → 停在它的**右/下边界**后。
    // 两种落点都满足：`ToIndex(落点)` == 那个**空**的格子（约定 2）。
    const float boundary = static_cast<float>(blockedIdx);
    moving = (delta > 0.0f) ? boundary - kContactEpsilon
                            : boundary + 1.0f + kContactEpsilon;
}

} // namespace detail

// ------------------------------------------------------------
// 推进：唯一"改变位置"的入口
// ------------------------------------------------------------

// 把 pos 沿 delta 推进；某一轴撞上实心格就把它夹在边界前 ε。
//
// ⚠️ 顺序（S7_CHECKLIST 7.4，写错就是"两条路径各自漂移"）：
//    · **外层**是子步循环（约定 3）
//    · **内层**是"先 X、再 Y"（分轴）
//    换成"先把 delta.x 整段走完、再走 delta.y"，在拐角处会差一格。
// ⚠️ 一轴被夹住后**不中止本帧**：该轴会反复夹在边界前，另一轴照常走 —— 这才是"贴墙滑行"。
//
// 返回 void：今天没有调用者需要"撞没撞"这个 bool（TECH_DEBT B2 的"0 调用者不建"纪律；
// 将来"撞墙时取消冲刺"再补 —— S7_CHECKLIST 第 8 节的升级路径）。
//
// ⚠️ 位移极大时这个循环会很长（argv[2] 能填出荒谬速度，main.cpp:35）。
//    **刻意不设上限**：截断位移会造出"真实位移 ≠ v × dt"这个第二条真相，
//    而 Game 的 HUD 自证（Game.cpp）正是靠那条等式。宁可慢，也不要一个静默削速的版本。
inline void MoveAndCollide(const TileMap& map, mini::math::Vec2& pos, mini::math::Vec2 delta) noexcept
{
    if (delta.x == 0.0f && delta.y == 0.0f) {
        return;
    }

    // 子步数按**单轴最大位移**算：分轴之后两轴各自独立，约定 3 里的"步子"就是沿轴位移
    const float axisSpan = std::max(std::abs(delta.x), std::abs(delta.y));
    const int   steps    = static_cast<int>(std::ceil(axisSpan / kMaxStepPerTick));   // ≥ 1

    const float stepX = delta.x / static_cast<float>(steps);
    const float stepY = delta.y / static_cast<float>(steps);

    for (int i = 0; i < steps; ++i) {
        detail::StepAxis(map, pos, stepX, /*movingIsX=*/true);    // 先 X
        detail::StepAxis(map, pos, stepY, /*movingIsX=*/false);   // 再 Y（顺序定死）
    }
}

// ------------------------------------------------------------
// 采样：把线段摊成一串格点（S9 步骤 1；调用方是 `game/Bullet.h`）
// ------------------------------------------------------------
//
// 干什么：把线段 [from, to] 切成若干**采样点**，让调用方逐点问自己的问题
//   （"这一点是实心吗？" / "这一点有敌人吗？"）。
// ⚠️ 它**不**回答"撞没撞" —— 两个调用方的命中条件不同（墙 / 敌人），
//    采样器只做"把线段摊成格点"这件中立的事（S9_CHECKLIST §6.1 的接口粒度决定）。
//
// ⚠️ 为什么需要它（S9_CHECKLIST 分叉 ②）：**一帧的位移可能 ≥ 1 格**
//   （`kMaxDelta = 0.05 s`（Time.h:31）× 45 格/秒 = **2.25 格**）——
//   只看积分后的那一个点，会**直接穿过**一个 1 格宽的目标（PLAN 的 S9 坑栏点名过这条）。
//   步长 ≤ 1 格就够：相邻两个采样点的 `ToIndex` 最多差 1 ⇒ **中间不存在"没被问到"的格**。
//   （1 格宽的目标只有 1 格，躲不进两个相邻采样点之间。）
//
// ⚠️ 步长口径与 `MoveAndCollide` **有意不同**（两边都 < 1 格，所以两边都跳不过一整格）：
//   · 这里按**线段长度**（欧氏距离）算步数 ⇒ 斜向线段被切得**更细**；
//   · `MoveAndCollide` 按**单轴最大位移**算（见它上面那句）⇒ 斜向时每步欧氏位移可达 0.707 格。
//   ⇒ 结论：**判定只会比位移更保守**（判定说"这一段没事"，位移一定也没事）。
//
// ⚠️ **这里刻意不加 `static_assert`**（S9_CHECKLIST 落地偏差 1）：采样要的界是"步长 ≤ 1 格"，
//    而上面 `kMaxStepPerTick < 1.0f` 那条已经更强 —— 再写一条就是它的**推论**，
//    永远不会红（`TECH_DEBT` C18 刚教过一次"空断言比没有更坏"）。
//    这条性质的真正守卫是**运行时的性质用例** `Sweep.ConsecutiveSamplesNeverSkipATile`。

// 这条线段要切成几步（**至少 1 步**）。
[[nodiscard]] inline int SweepStepCount(mini::math::Vec2 from, mini::math::Vec2 to) noexcept
{
    const float dist = (to - from).Length();

    // ⚠️ 写成 `!(dist > kMaxStepPerTick)` 而**不是** `dist <= kMaxStepPerTick`（S4 的写法）：
    //    NaN 比较恒假 → NaN 也走这一支 ⇒ 零长度线段和 NaN 输入都拿到 1 步，
    //    **不会把 NaN 传进 `ceil` / `static_cast<int>`**（那是 UB）。
    if (!(dist > kMaxStepPerTick)) {
        return 1;
    }
    return static_cast<int>(std::ceil(dist / kMaxStepPerTick));
}

// 第 i 个采样点，i ∈ [0, stepCount]（**两端点都含**）。
// ⚠️ `i == 0` / `i == stepCount` **精确返回两端点**（不做浮点乘加）：
//    端点是"差一个 ε 就落到隔壁格"最容易出问题的地方。
// ⚠️ 纯函数要**全域安全**：`stepCount <= 0` 当 1 步、`i` 夹进 [0, stepCount] ——
//    不崩、不 UB、不出现 0 除（调用方越界是它的 bug，但不该在这里变成一次崩溃）。
[[nodiscard]] inline mini::math::Vec2 SweepSampleAt(mini::math::Vec2 from, mini::math::Vec2 to,
                                                    int stepCount, int i) noexcept
{
    if (stepCount <= 0) {
        stepCount = 1;
    }
    if (i <= 0) {
        return from;
    }
    if (i >= stepCount) {
        return to;
    }

    const float t = static_cast<float>(i) / static_cast<float>(stepCount);
    return from + (to - from) * t;   // 只做线性插值，不取整（取整由调用方按 ToIndex 走）
}

} // namespace mini::world
