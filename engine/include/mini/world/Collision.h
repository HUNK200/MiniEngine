#pragma once

#include "mini/math/Vec2.h"
#include "mini/world/TileMap.h"

#include <algorithm>   // std::max
#include <cmath>       // std::ceil / std::abs

// ============================================================
// world/Collision.h —— 1×1 盒与格子地图的碰撞（S7，header-only）
//
// 这一层只回答一个问题：**"一个 1×1 的盒子从 A 走到 B，中途撞到墙了吗？最终停在哪？"**
//
//   · 它**知道**：格子是不是实心（通过 TileMap）、格子占哪片区域、盒子怎么夹紧。
//   · 它**不知道**：玩家/敌人是谁（不认识 Entity / EntityId）、输入、屏幕、终端。
//   · 它**不做**：不分配、不抛异常、不记录历史 —— 三个公开函数都是纯函数，能完全离线单测。
//
// 依赖谁：world/TileMap.h（格子是什么）+ math/Vec2.h（位置/位移）。
//   **不依赖 platform / render / game**（PLAN A.3 的分层）。
// 被谁用：world::World::Update —— **唯一调用点**。
//   game/ 不许自己调 MoveAndCollide：否则就有了两条"位置怎么变"的路径（S6 的 E-8 教训同族）。
//
// ⚠️ 四条"写错就完蛋"的约定（每一条都有用例钉着，见 S7_CHECKLIST 10.1）：
//
//   1. **盒子是 1×1、中心在 pos**（Entity.h 的 0.5 语义：pos 是格中心）
//      → 盒的边界是 `pos ± 0.5`，**不是** `[pos, pos + 1)`。
//   2. **贴面不算重叠**（"正面积重叠" / 开区间）：停在接触面上是合法状态，不会自己挡住自己。
//      因此**不需要 epsilon** —— 接触面就是 `tx − 0.5` 这种**精确半数**（用例能把期望值写死）。
//   3. **子步进是正确性的一部分，不是优化**：每步 ≤ `kMaxStepPerTick`（< 1 格）→
//      "移动前 / 移动后"这两个采样点之间**塞不下一整格厚的墙**。
//      没有它，速度稍高（`argv[2]` 可改）就会一帧穿过一根柱子。
//   4. **候选格只取 tentative 盒子压到的那 1~2 格** —— 这一条**依赖子步长 ≤ 0.5**：
//      若没有子步进，从 x = 5.5 一帧推 −100 会让"候选格"落在 −95 附近，
//      夹到完全错误的格上（这正是约定 3 与这里的耦合）。
//
// ⚠️ 本文件里**定义的先后不是随意的**：`IsSolidAt` → `detail::AxisTileBlocks` → `OverlapsSolid`
//    → `detail::StepAxis` → `MoveAndCollide`，每一个都用到前一个（所以 `detail` 被分成两块）。
//    写反了 MSVC 会报"未声明的标识符"，而那种报错很容易被误当成"忘了 include"。
//
// 与 PLAN 的差异（见 S7_CHECKLIST 第 12 节）：PLAN 原案是 `TileMap::IsSolidAt(Vec2)`（挂在 TileMap 上）、
// 且把"子步进"留给 S9 的子弹；本阶段改成这里的自由函数 + S7 就做子步进。
// ============================================================

namespace mini::world {

// 实体 = 1×1 格的方形盒，中心在 pos（约定 1）。
constexpr float kEntityHalfSize = 0.5f;

// 单次推进的位移上限（子步进）。**必须 < 1.0**：
// 盲区长度 = 子步长 − 1 格，只有 < 0 时才在几何上不可能漏格（约定 3）。
constexpr float kMaxStepPerTick = 0.5f;

static_assert(kEntityHalfSize == 0.5f,
              "盒子的半宽变了：接触面就不再是“整格 ∓ 0.5”这种精确半数，用例里的期望值要跟着重算");
static_assert(kMaxStepPerTick < 1.0f,
              "子步长必须 < 1 格（墙厚 1 格）：否则“移动前/移动后”之间能藏下一整格墙");

// 盒子在**某一个轴**上最多压到 2 格：跨度恰好 1.0 格。
// 中心不在格中心时两格都有正面积重叠；**恰好落在格中心时只有 1 格**（另一半的重叠为 0）。
constexpr int kMaxTilesPerAxis = 2;

// ------------------------------------------------------------
// 谓词 1：这一格是不是实心
// ------------------------------------------------------------

// **越界 = 实心** —— 直接转发 TileMap::At 的既定语义（TileMap.h 的文件头约定 1）。
//
// ⚠️ 只允许在这一处回答"实心吗"：不要再写一遍 InBounds 判断（那就是第二条会各自漂移的路径）。
//    好处是"角色永远走不出地图"是免费的（见用例 CannotLeaveMapThroughEdge）。
[[nodiscard]] inline bool IsSolidAt(const TileMap& map, int tx, int ty) noexcept
{
    return map.At(tx, ty) == Tile::Wall;
}

namespace detail {

// 格子的开区间 [t, t+1)
[[nodiscard]] constexpr float TileMin(int tile) noexcept { return static_cast<float>(tile); }
[[nodiscard]] constexpr float TileMax(int tile) noexcept { return static_cast<float>(tile) + 1.0f; }

// 区间 [a0, a1) 与 [b0, b1) 的重叠长度；不重叠（含只贴边）返回 0。
// ⚠️ "有没有重叠"必须用 **> 0** 判 —— 这就是约定 2 的落地处。
[[nodiscard]] constexpr float OverlapLength(float a0, float a1, float b0, float b1) noexcept
{
    const float lo = a0 > b0 ? a0 : b0;
    const float hi = a1 < b1 ? a1 : b1;
    return hi > lo ? hi - lo : 0.0f;
}

// 移动轴上的某一格会不会挡住"中心在 (moving, other)"的这个盒子。
//
// ⚠️ 检测（OverlapsSolid）与夹紧（StepAxis）**共用这一个判据** —— 它们各写一份的话，
//    就会出现"检测说有墙、夹紧却找不到墙"（或反过来）的静默不一致。
//
// movingIsX 是两轴唯一的差别：它只决定**查地图时的下标顺序**
//   （X 轴查 (moving, other)，Y 轴查 (other, moving)）。
[[nodiscard]] inline bool AxisTileBlocks(const TileMap& map, float moving, float other,
                                        int movingIdx, bool movingIsX) noexcept
{
    if (OverlapLength(moving - kEntityHalfSize, moving + kEntityHalfSize,
                      TileMin(movingIdx), TileMax(movingIdx)) <= 0.0f) {
        return false;   // 移动轴上"只是贴着"，不算重叠（约定 2）
    }

    const int otherIdx0 = TileMap::ToIndex(other - kEntityHalfSize);   // 取整只走 ToIndex
    for (int i = 0; i < kMaxTilesPerAxis; ++i) {
        const int otherIdx = otherIdx0 + i;
        if (OverlapLength(other - kEntityHalfSize, other + kEntityHalfSize,
                          TileMin(otherIdx), TileMax(otherIdx)) <= 0.0f) {
            continue;   // 这一格在另一轴上只贴边
        }
        if (movingIsX ? IsSolidAt(map, movingIdx, otherIdx)
                      : IsSolidAt(map, otherIdx, movingIdx)) {
            return true;
        }
    }
    return false;
}

} // namespace detail

// ------------------------------------------------------------
// 谓词 2：盒（1×1，中心在 center）有没有压到实心格
// ------------------------------------------------------------

// **开区间语义**：贴面（重叠长度为 0）不算压到（约定 2）。
[[nodiscard]] inline bool OverlapsSolid(const TileMap& map, mini::math::Vec2 center) noexcept
{
    const int tx0 = TileMap::ToIndex(center.x - kEntityHalfSize);
    for (int i = 0; i < kMaxTilesPerAxis; ++i) {
        // 复用 detail::AxisTileBlocks：与夹紧用同一个判据（否则两者会静默不一致）
        if (detail::AxisTileBlocks(map, center.x, center.y, tx0 + i, /*movingIsX=*/true)) {
            return true;
        }
    }
    return false;
}

namespace detail {

// 单轴推进：撞到就**夹紧到接触面**（决策 1）。
//
// ⚠️ 两轴共用这一份实现（movingIsX 只在下标顺序上分岔）。
//    "把 +x/−x、+y/−y 四个镜像各写一遍"是坑 17 的来源（只写对一半很难看出来）。
//
// ⚠️ 定义在 `OverlapsSolid` **之后**：它要用那个判据（见文件头的顺序说明）。
inline void StepAxis(const TileMap& map, mini::math::Vec2& pos, float delta, bool movingIsX) noexcept
{
    if (delta == 0.0f) {
        return;   // 这一轴本段不动（顺带让下面的扫描不必处理"零位移"）
    }

    float&      moving = movingIsX ? pos.x : pos.y;
    const float other  = movingIsX ? pos.y : pos.x;   // 本段不变，只用来算另一轴的格范围

    // 自愈：起点已经嵌在墙里 → 本轴**不夹紧**，直接接受位移（7.4 的约定）。
    // 反过来做（重叠就不许动）会把"已经嵌进去"变成永久卡死 ——
    // S7 已经因为"冲出地图永久卡在界外"否掉过一个方案，不再造第二个死锁。
    if (OverlapsSolid(map, pos)) {
        moving += delta;
        return;
    }

    const float before    = moving;
    const float tentative = moving + delta;

    moving = tentative;                        // 先放上去，用**同一个判据**问"这样行不行"
    if (!OverlapsSolid(map, pos)) {
        return;                                // 没撞到 → 接受
    }
    moving = before;                           // 撞到 → 撤回，再按移动方向找最近的接触面

    // 候选格：tentative 盒子在移动轴上压到的那 1~2 格（约定 4：**依赖子步长 ≤ 0.5**）
    const int movingIdx0 = TileMap::ToIndex(tentative - kEntityHalfSize);

    if (delta > 0.0f) {
        // 向右 / 向下：**最小**的挡路格就是接触面（从近到远扫）
        for (int i = 0; i < kMaxTilesPerAxis; ++i) {
            const int movingIdx = movingIdx0 + i;
            if (!AxisTileBlocks(map, tentative, other, movingIdx, movingIsX)) {
                continue;
            }
            moving = TileMin(movingIdx) - kEntityHalfSize;   // = tx − 0.5（精确半数）
            return;
        }
    } else {
        // 向左 / 向上：**最大**的挡路格就是接触面（扫掠方向反过来）
        for (int i = kMaxTilesPerAxis - 1; i >= 0; --i) {
            const int movingIdx = movingIdx0 + i;
            if (!AxisTileBlocks(map, tentative, other, movingIdx, movingIsX)) {
                continue;
            }
            moving = TileMax(movingIdx) + kEntityHalfSize;   // = tx + 1 + 0.5
            return;
        }
    }

    // 走到这里 = "OverlapsSolid 说撞了、但按方向找不到挡路的格"。逻辑上不可达
    // （两个判据是同一个函数、候选集也相同），保守停在原位：**宁可不动，也绝不放行**。
}

} // namespace detail

// ------------------------------------------------------------
// 推进：唯一"改变位置"的入口
// ------------------------------------------------------------

// 把 pos 沿 delta 推进；撞到墙就把**那一轴**夹在接触面上。
//
// ⚠️ 顺序（7.4，写错就是"两条路径各自漂移"）：
//    · **外层**是子步循环（约定 3）
//    · **内层**是"先 X、再 Y"（分轴）
//    换成"先把 delta.x 整段走完、再走 delta.y"，在拐角处会差一格。
// ⚠️ 一轴被夹紧后**不中止本帧**：该轴会反复夹在接触面上，另一轴照常走 —— 这才是"贴墙滑行"。
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

    // 子步数按**单轴最大位移**算：分轴之后两轴各自独立，盲区公式里的 D 就是"沿轴位移"
    const float axisSpan = std::max(std::abs(delta.x), std::abs(delta.y));
    const int   steps    = static_cast<int>(std::ceil(axisSpan / kMaxStepPerTick));   // ≥ 1

    const float stepX = delta.x / static_cast<float>(steps);
    const float stepY = delta.y / static_cast<float>(steps);

    for (int i = 0; i < steps; ++i) {
        detail::StepAxis(map, pos, stepX, /*movingIsX=*/true);    // 先 X
        detail::StepAxis(map, pos, stepY, /*movingIsX=*/false);   // 再 Y（顺序定死）
    }
}

} // namespace mini::world
