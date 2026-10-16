#pragma once

#include "mini/math/Vec2.h"
#include "mini/world/Collision.h"   // IsSolidAt / SweepStepCount / SweepSampleAt

#include <cmath>     // std::abs
#include <cstddef>   // std::size_t
#include <cstdint>   // std::uint8_t
#include <span>      // std::span

// ============================================================
// game/Bullet.h —— 子弹的**规则**（S9 步骤 2，header-only）
//
// ⚠️ 这个文件**不是**"子弹的类"：
//   · 子弹本体就是一个 `mini::world::Entity`（`symbol = '*'`）—— 它连一个额外字段都不需要
//     （伤害是常量、方向就在 `velocity` 里、地图外恒为实心所以不需要 TTL）。见 S9_CHECKLIST 分叉 ①。
//   · 这里只放**规则**：朝向怎么表示、朝向 ↔ 向量、以及"这一帧这一段路上有没有东西"。
//
// 依赖谁：`math/Vec2.h` + `world/Collision.h`（采样器 + 实心谓词）。
// ⚠️ **不认识 `Entity` / `EnemyData` / `Game` / 输入 / 屏幕** —— 所以这一层能离线单测；
//    也正因为不认识"敌人是什么"，命中判定收的是**敌人位置**（`span<const Vec2>`）而不是实体指针
//    （是谁由调用方拿返回的**下标**去索引它自己那份平行数组决定）。
//
// 三条约定（写错会静默出错，都有用例钉着）：
//   1. **世界 Y 轴向下**（`Entity.h`）：`Facing8::North` = 屏幕**向上** = **−Y**。
//   2. 朝向是**8 向枚举**而不是浮点向量 —— 用例断言 `Facing8::East` **不用比浮点**，
//      HUD 也能直接查表画字形（分叉 ③）。
//   3. **墙优先于敌人**：采样器逐点先问"这一点实心吗"，实心就**立即停**（`blocked = true`）——
//      墙后面的敌人**不会**被命中。子弹不该隔着墙打死人（`S9_CHECKLIST` 决策 D8）。
// ============================================================

namespace game {

// 8 向朝向。⚠️ 顺序与 `kFacingGlyphs` **一一对应**（改顺序要同时改那张表）。
enum class Facing8 : std::uint8_t {
    East = 0, NorthEast, North, NorthWest,
    West, SouthWest, South, SouthEast,
};

inline constexpr std::size_t kFacingCount = 8;

// HUD 用的**定宽 2 字符**字形（全 ASCII —— 代码页 936 下不能写非 ASCII，S8 的坑表记过）。
// 「目测效果」不满意时**只改这张表**：用例只钉"8 项、每项 2 字符、全 ASCII"，不钉具体字符。
inline constexpr const char* kFacingGlyphs[kFacingCount] = {
    "->", "/>", "^^", "\\<",   // 东 · 东北 · 北 · 西北
    "<-", "</", "vv", "\\>",   // 西 · 西南 · 南 · 东南
};

// 越界枚举（不该发生）时的兜底：**保持 2 字符宽**，否则 HUD 后面的列会左右跳。
inline constexpr const char* kFacingGlyphFallback = "??";

struct BulletTuning {
    float speed  = 45.0f;   // 格/秒（= 2.25 × `Game::kMoveSpeed`，必须比玩家快）
    int   damage = 1;       // 单发伤害（敌人 `kEnemyMaxHp` = 3 ⇒ 三发一个）
};

inline constexpr BulletTuning kDefaultBullet{};

// 8 向的分界角：`tan(22.5°) = √2 − 1 ≈ 0.41421356`。
// 两个分量的比值超过它才判成"斜向"，否则判成"纯轴向"（详见 `DirectionToFacing`）。
inline constexpr float kOctantBoundary = 0.41421356f;

// 8 个方向对应的**单位向量**（用 1/√2 表示斜向，省一次 sqrt）。
// ⚠️ 世界 Y 向下 ⇒ 北是 (0, −1)。
inline constexpr float kInvSqrt2 = 0.70710678f;

[[nodiscard]] inline mini::math::Vec2 FacingToVec2(Facing8 f) noexcept
{
    const std::size_t i = static_cast<std::size_t>(f);
    if (i >= kFacingCount) {
        // 越界枚举（只能由强制转换造出来）：退回**东**而不是零向量 ——
        // 零向量会造出一颗"原地不动、永远赖在屏幕上"的子弹，比方向错了更糟。
        return mini::math::Vec2{1.0f, 0.0f};
    }

    // 顺序必须与 `Facing8` 一致：东 东北 北 西北 西 西南 南 东南
    constexpr mini::math::Vec2 kVectors[kFacingCount] = {
        { 1.0f,  0.0f}, { kInvSqrt2, -kInvSqrt2}, { 0.0f, -1.0f}, {-kInvSqrt2, -kInvSqrt2},
        {-1.0f,  0.0f}, {-kInvSqrt2,  kInvSqrt2}, { 0.0f,  1.0f}, { kInvSqrt2,  kInvSqrt2},
    };
    return kVectors[i];
}

// 按 45° 一档**原地转身**（S12，分叉 B = 甲′）。
//
// ⚠️ **方向约定（2026-10-16 实测纠正）**：`Facing8` 的枚举顺序在**屏幕上**是**逆时针**
//    （东 = 3 点钟 → 东北 = 1:30 ⇒ 往左转）⇒ `steps = +1` 是**逆时针**、`-1` 是**顺时针**。
//    ⚠️ 这里原写"顺序本身就是顺时针"，是**错的** —— 它让 `Game` 里 `Z`/`C` 的符号也跟着写反了
//       一次，被 `Game.TurnKeysRotateFacingInPlace` 抓出来（见 `S12_CHECKLIST` 落地偏差）。
//    不需要查表：枚举顺序本身就定义了一圈；`steps` 可以是任意整数（负数、0、超过一圈都行）。
// ⚠️ 返回值**一定是合法枚举**：轮转用"先加再对 8 取模 + 负数修正"，而不是
//    `static_cast<Facing8>(raw + steps)` —— 后者对负数或大数会造出越界枚举值
//    （那种值落到 `FacingToVec2`/`FacingGlyph` 只会走兜底，属于"能跑但语义错"）。
// ⚠️ 越界入参（只有强制转换造得出来）→ 退回 `East`，与 `FacingToVec2` 的兜底方向一致。
[[nodiscard]] inline Facing8 StepFacing(Facing8 f, int steps) noexcept
{
    const std::size_t i = static_cast<std::size_t>(f);
    if (i >= kFacingCount) {
        return Facing8::East;
    }

    constexpr int kN   = static_cast<int>(kFacingCount);   // 8
    int           next = (static_cast<int>(i) + steps) % kN;
    if (next < 0) {
        next += kN;   // ⚠️ C++ 的 `%` 对负数给负数（-1 % 8 == -1），必须自己拨回来
    }
    return static_cast<Facing8>(next);
}

// 查字形（HUD 用）。越界枚举 → `kFacingGlyphFallback`（仍是 2 字符）。
[[nodiscard]] inline const char* FacingGlyph(Facing8 f) noexcept
{
    const std::size_t i = static_cast<std::size_t>(f);
    return (i < kFacingCount) ? kFacingGlyphs[i] : kFacingGlyphFallback;
}

// 由"本帧的移动方向"推出朝向。
//
// ⚠️ **零向量返回 `false`**（并且**不写 `out`**）—— 这是分叉 ③"停下沿用上次朝向"的实现方式：
//    调用方拿到 false 就保持原朝向，而不是在这里猜一个方向。
// ⚠️ 判据是"**最近的 8 向**"（±22.5° 边界），不是"两个分量都非零就算斜向"：
//    后者对 (3,1) 这种输入会错判成东北（实际离东更近）。本游戏的输入只有
//    {−1,0,+1} 组合，两种写法结果相同 —— 但纯函数要对**任意**输入成立。
// ⚠️ NaN 也走"返回 false"这一支（比较取反的写法，同 `SweepStepCount`）。
[[nodiscard]] inline bool DirectionToFacing(mini::math::Vec2 dir, Facing8& out) noexcept
{
    const float ax = std::abs(dir.x);
    const float ay = std::abs(dir.y);

    if (!(ax > 0.0f) && !(ay > 0.0f)) {
        return false;   // 零向量 / 全 NaN：调用方保持原朝向
    }

    const bool east    = dir.x > 0.0f;
    const bool south   = dir.y > 0.0f;   // 世界 Y 向下 ⇒ 正 y 是"南"
    const bool diagonal = (ax > ay * kOctantBoundary) && (ay > ax * kOctantBoundary);

    if (diagonal) {
        out = east ? (south ? Facing8::SouthEast : Facing8::NorthEast)
                   : (south ? Facing8::SouthWest : Facing8::NorthWest);
    } else if (ax >= ay) {
        out = east ? Facing8::East : Facing8::West;      // 边界（正好 22.5°）归轴向
    } else {
        out = south ? Facing8::South : Facing8::North;
    }
    return true;
}

// 命中判定的结果。
// ⚠️ 两者**可能都"没发生"**（`enemyIndex == -1 && !blocked` = 这条线段上什么也没有）；
//    但**不会同时为真** —— 墙优先，碰上实心就立即返回（约定 3）。
struct BulletHit {
    int  enemyIndex = -1;     // 命中 `enemyPositions` 里的第几个；-1 = 没打中任何敌人
    bool blocked    = false;  // 线段被实心格挡住（子弹撞墙）
};

// 沿线段采样，返回**第一个**被命中的敌人**下标**（没有则 -1）+ 是否撞墙。
//
// ⚠️ "第一个" = **沿线段顺序的第一个**（= 离起点最近的那个），**不是**数组里的第一个。
// ⚠️ 终点那一格**也算**（端点含在内，见 `SweepSampleAt`）—— 所以"贴脸打"能命中。
// ⚠️ 同一格里站着多个敌人时，返回**数组靠前**的那个（由调用方决定自己的顺序）。
// ⚠️ 本函数不认识 `Entity`：收的是**位置**（点模型下占格 = `ToIndex(pos)`）—— 所以能离线单测。
//
// ⚠️⚠️ **已知限制：斜向"擦格角"的那一格可能漏**（`TECH_DEBT` **C21**，2026-10-10 明确"不修、只记录"）。
//    采样是**离散**的：被路径切掉薄片的那一格可能**一个采样点都没有**。边界很干净：
//      · **轴向路径不可能漏** —— 格子宽 1 格 > 采样间距 0.5 格 ⇒ 任何长度 1 的区间里必有采样点；
//      · **穿格子"边"的也不可能漏** —— 穿过一格的路程 ≥ 1 格（斜率 m 时是 `√(1+m²)`）；
//      · **只有"擦角"** —— 遍历长度可以任意小（`y = x + 0.001` 在某一格里只切出 ≈ 0.0014 格）。
//    ⚠️ 间距 ≤ 0.5 是 `SweepStepCount` 的**构造性质**，**与 dt / 弹速无关** ⇒ 调快弹速不会让它变差。
//    ⚠️ 这与 S7 的点模型**语义一致**：那套模型本来就允许"对角缝能穿"（`SlipsThroughDiagonalGap`）。
//    真要修有三条路（都在 `C21` 里写了）：步长减半（消不掉）/ DDA 逐格遍历（第二套碰撞实现）/ 给子弹加半径（改语义）。
[[nodiscard]] inline BulletHit FirstEnemyOnSegment(
    const mini::world::TileMap& map, mini::math::Vec2 from, mini::math::Vec2 to,
    std::span<const mini::math::Vec2> enemyPositions) noexcept
{
    const int steps = mini::world::SweepStepCount(from, to);

    for (int i = 0; i <= steps; ++i) {
        const mini::math::Vec2 p = mini::world::SweepSampleAt(from, to, steps, i);

        // ① 墙优先：实心就立即停 ⇒ 墙后面的敌人不会被命中（约定 3）
        if (mini::world::IsSolidAt(map, p)) {
            return BulletHit{-1, true};
        }

        // ② 有没有敌人占着这一格（点模型：占格 = ToIndex(pos)）
        const int tx = mini::world::TileMap::ToIndex(p.x);
        const int ty = mini::world::TileMap::ToIndex(p.y);
        for (std::size_t e = 0; e < enemyPositions.size(); ++e) {
            if (mini::world::TileMap::ToIndex(enemyPositions[e].x) == tx &&
                mini::world::TileMap::ToIndex(enemyPositions[e].y) == ty) {
                return BulletHit{static_cast<int>(e), false};
            }
        }
    }

    return BulletHit{};   // 什么都没碰上
}

} // namespace game
