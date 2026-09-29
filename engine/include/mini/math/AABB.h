#pragma once

// ============================================================
// math/AABB.h —— 轴对齐包围盒（Axis-Aligned Bounding Box）
//
// 这一层干什么：碰撞检测、裁剪、拾取、空间划分的**基础形状**。
// 依赖谁：只依赖 math/Vec2.h（以及它带的 MathUtils.h）。
// 被谁用：S7 的实体碰撞、S9 的子弹命中、S11 的 Debug 绘制、S12 的 DDA 步进。
//
// ⚠️⚠️ **本文件最需要记住的一件事：两处边界语义是故意不一致的。**
//
//   | 函数 | 区间 | 边界相接时 |
//   | --- | --- | --- |
//   | `Contains(p)`   | **闭** `min <= p <= max` | **算**在里面 |
//   | `Intersects(o)` | **开** `min < max`       | **不算**相交 |
//
//   为什么不能统一？两边各自有**非它不可**的理由：
//     · 统一成闭区间 → 紧贴墙壁的实体**每帧都被判碰撞并被推开一格**，永远贴不住墙（S7 的现象）。
//     · 统一成开区间 → 点落在盒子边界上会被判"不在盒内"，拾取和裁剪在边缘会**漏掉一格**。
//   所以这不是笔误，是取舍；两处都有注释说明，也各有一条用例钉着。
//
// ⚠️ 还有第三条要知道的：**退化盒（`min == max`，零尺寸）不是"闭区间的点"**。
//   `Intersects` 在退化输入上给的是开区间公式的**自然结果**：
//     点盒 vs 它自己 = false ｜ 点在盒**内部** = true ｜ 点在盒**边界**上 = false
//   所以：**查询用 `Contains`，碰撞用 `Intersects`** —— 别拿退化盒当"点坐标"去查询。
// ============================================================

#include "mini/math/Vec2.h"

namespace mini::math {

struct AABB {
    Vec2 min{};
    Vec2 max{};

    // 由**中心 + 全宽全高**构造。注意 size 是"直径"不是"半径"：
    //     FromCenterSize({5,5}, {2,2})  →  min={4,4}, max={6,6}
    [[nodiscard]] static constexpr AABB FromCenterSize(Vec2 c, Vec2 size) noexcept
    {
        const Vec2 half = size * 0.5f;
        return AABB{c - half, c + half};
    }

    // 由任意两个角点构造 —— **自动排序**，谁大谁小都行。
    //
    // 少了这个排序会造出"反盒"（min > max），而反盒上**所有判定都静默失效**
    // （不崩、不报错，只是结果没有意义），是最难查的一类 bug。
    // 所以这里强制排序：只要走这个入口，就不可能造出反盒。
    [[nodiscard]] static constexpr AABB FromCorners(Vec2 a, Vec2 b) noexcept
    {
        return AABB{a.Min(b), a.Max(b)};
    }

    [[nodiscard]] constexpr Vec2 Center() const noexcept { return (min + max) * 0.5f; }
    [[nodiscard]] constexpr Vec2 Size()   const noexcept { return max - min; }

    // **闭区间**：min <= p <= max（边界算在里面）。
    // "点在盒子里"符合直觉 —— 屏幕上点中的那个格子当然算在盒子内。
    [[nodiscard]] constexpr bool Contains(Vec2 p) const noexcept
    {
        return p.x >= min.x && p.x <= max.x
            && p.y >= min.y && p.y <= max.y;
    }

    // **开区间**：逐轴都用严格小于。
    //
    // 为什么必须是 `<` 而不是 `<=`：两个盒子刚好贴边（一个的 max 等于另一个的 min）时，
    // 它们只是**相邻**，不是**碰撞**。用 `<=` 的话，紧贴墙壁的实体每帧都会被判碰撞
    // 并被推开一格 —— 表现是"怎么走都贴不住墙"，而且看起来像是移动逻辑的毛病。
    // 这条有专门的用例（`TouchingIsNotIntersecting`），别顺手改成 `<=`。
    //
    // ⚠️ **退化盒（`min == max`，零尺寸）上的行为是"公式的自然结果"，没有特判**：
    //     · 点盒 vs 它自己           → **false**（`1 < 1` 不成立）
    //     · 点盒落在盒子的**边界**上 → **false**（同上，与"贴边不算相交"一致）
    //     · 点盒落在盒子的**内部**   → **true**（`1 < 2 && 0 < 1` 都成立，虽然重叠长度为 0）
    //   所以：**退化盒 ≠ 闭区间的点**。要在"点与盒"之间做查询，请用 `Contains()`
    //   （闭区间、边界算在内）—— 那才是查询该有的语义。
    //   用途上很好区分：**查询用 `Contains`，碰撞用 `Intersects`。**
    [[nodiscard]] constexpr bool Intersects(const AABB& o) const noexcept
    {
        return min.x < o.max.x && o.min.x < max.x
            && min.y < o.max.y && o.min.y < max.y;
    }

    // min <= max（逐分量）。`min > max` 的"反盒"上所有判定都没有意义，
    // 所以留一个**显式的检查**给断言和测试用。
    //
    // **本类不做自动纠正**：反盒应该在**构造阶段**就避免 ——
    // 走 `FromCorners()` 永远不会得到反盒；只有直接写 `{min, max}` 两个成员才可能。
    [[nodiscard]] constexpr bool IsValid() const noexcept
    {
        return min.x <= max.x && min.y <= max.y;
    }
};

// 值类型的自检：16 字节 = 两个 Vec2，不多不少
static_assert(sizeof(AABB) == 16, "AABB must be exactly two Vec2, no more no less");

} // namespace mini::math
