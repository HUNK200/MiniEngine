#pragma once

// ============================================================
// math/Vec2.h —— 二维向量
//
// 这一层干什么：位置、方向、偏移、尺寸……凡是"一对 float"都用它。
// 依赖谁：只依赖 math/MathUtils.h，以及标准库的 <cmath>（Length / Normalized 要 sqrt）。
// 被谁用：AABB / Ray（同层），以及 S5 之后的 game 层。
//
// 四条设计取向（每条都是有意的）：
//
//   • **值类型**：8 字节、trivially copyable、无堆分配、无虚函数 → 随便按值传。
//   • **不做模板**（PLAN 明确要求"别一上来就模板化 T"）：本项目只有 float 一种精度，
//     模板化会把所有运算符变成模板、报错信息变难读，换来 0 收益。
//   • **刻意不提供 `operator==`**：浮点精确相等几乎总是错的（0.1 + 0.2 != 0.3）。
//     要比较请用 `ApproxEquals()`。少了 == 之后，测试里也没法误用 MINI_CHECK_EQ
//     拿两个向量直接比 —— 这正是我们想要的：**逼所有人写"多近算相等"**。
//   • **刻意不提供 `operator<<`**：目前只有测试想打印它，而逐分量断言本来就够查。
//     真需要时再加（那时它会有明确的使用者）。
//
// 3D 的接口契约见 doc/S4_CHECKLIST.md 6.6 —— **现在刻意不建 Vec3.h**，
// 因为没被 include 的头文件编译器根本不看，半成品等于写在纸上。
// ============================================================

#include "mini/math/MathUtils.h"

#include <cmath>   // std::sqrt（Length / Normalized 用）

namespace mini::math {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    // —— 复合赋值**必须是成员**（语言规定），所以这四个在类里 ——
    constexpr Vec2& operator+=(Vec2 o) noexcept { x += o.x; y += o.y; return *this; }
    constexpr Vec2& operator-=(Vec2 o) noexcept { x -= o.x; y -= o.y; return *this; }
    constexpr Vec2& operator*=(float s) noexcept { x *= s; y *= s; return *this; }
    // s == 0 **不检查**：本项目没有"除以零"的合理场景，真除了会得到 inf/NaN，
    // 那属于调用方的 bug（约定见 MathUtils.h 顶部的第 1 条）。
    constexpr Vec2& operator/=(float s) noexcept { x /= s; y /= s; return *this; }

    // 长度的平方。**能用它就别用 Length()** —— 比大小、比范围都不需要开方，
    // 而开方是这个类型里最贵的操作（S7 的"谁更近"会大量用它）。
    [[nodiscard]] constexpr float LengthSq() const noexcept { return x * x + y * y; }

    [[nodiscard]] constexpr float Dot(Vec2 o) const noexcept { return x * o.x + y * o.y; }

    // 逆时针 90°：(x, y) → (-y, x)。
    //
    // ⚠️ **这个方向是写死的约定**：S7 的碰撞法线、S12 的墙面朝向都要跟它一致。
    // 搞反了的现象是"实体被推进墙里"，而且很难一眼看出来 —— 所以测试里有一条专门钉它。
    // 将来加 Vec3 时，`Cross()` 的方向必须和这里一致（见 doc/S4_CHECKLIST.md 6.6）。
    [[nodiscard]] constexpr Vec2 Perp() const noexcept { return Vec2{-y, x}; }

    // 逐分量取小 / 取大。
    // 手写而不是 std::min/std::max：省掉 <algorithm>，而且 NaN 参与时这里是
    // **确定性地取另一个**（std::min 遇到 NaN 的行为是实现定义的，见约定 1）。
    // 目前的使用者：AABB::FromCorners()（步骤 2 会用到）。
    [[nodiscard]] constexpr Vec2 Min(Vec2 o) const noexcept
    {
        return Vec2{x < o.x ? x : o.x, y < o.y ? y : o.y};
    }
    [[nodiscard]] constexpr Vec2 Max(Vec2 o) const noexcept
    {
        return Vec2{x > o.x ? x : o.x, y > o.y ? y : o.y};
    }

    // 逐分量近似相等。用 `mini::math::` 限定是因为成员名会把同名的自由函数**遮蔽**掉，
    // 不限定的话这里会解析到成员自己（参数对不上，直接编译不过）。
    [[nodiscard]] constexpr bool ApproxEquals(Vec2 o, float eps = 1e-5f) const noexcept
    {
        return mini::math::ApproxEquals(x, o.x, eps) && mini::math::ApproxEquals(y, o.y, eps);
    }

    // —— 下面两个**不能 constexpr**：要用 std::sqrt ——
    //
    // 证据：本机 MSVC 14.44 的 <cmath> 第 264 行是
    //     _NODISCARD _Check_return_ inline float sqrt(_In_ float _Xx) noexcept
    // 是 `inline` 而不是 `constexpr`（MSVC 的 _CONSTEXPR23 只在 C++23 下展开）。
    // PLAN 原文写的是"全 constexpr 头文件" —— 这条差异记在 doc/S4_CHECKLIST.md 差异 1，
    // C++23 之后可以无痛改回来。
    [[nodiscard]] float Length() const noexcept;
    [[nodiscard]] Vec2  Normalized() const noexcept;
};

// 二元运算符用**自由函数**而不是成员：左右对称，而且将来加 `2.0f * v`
// 只需要一个重载（成员的左操作数永远是 *this，做不到"浮点 * 向量"）。
[[nodiscard]] constexpr Vec2 operator+(Vec2 a, Vec2 b) noexcept { return Vec2{a.x + b.x, a.y + b.y}; }
[[nodiscard]] constexpr Vec2 operator-(Vec2 a, Vec2 b) noexcept { return Vec2{a.x - b.x, a.y - b.y}; }
[[nodiscard]] constexpr Vec2 operator-(Vec2 v) noexcept { return Vec2{-v.x, -v.y}; }
[[nodiscard]] constexpr Vec2 operator*(Vec2 v, float s) noexcept { return Vec2{v.x * s, v.y * s}; }
[[nodiscard]] constexpr Vec2 operator*(float s, Vec2 v) noexcept { return Vec2{v.x * s, v.y * s}; }
[[nodiscard]] constexpr Vec2 operator/(Vec2 v, float s) noexcept { return Vec2{v.x / s, v.y / s}; }

inline float Vec2::Length() const noexcept
{
    return std::sqrt(LengthSq());
}

inline Vec2 Vec2::Normalized() const noexcept
{
    const float len2 = LengthSq();

    // **一条判定同时挡住零向量与 NaN**：
    //     len2 == 0    →  !(0   > 0)  =  true
    //     len2 == NaN  →  !(NaN > 0)  =  true    ← 关键：NaN 与任何值比较都是 false
    // 写成 `len2 <= 0.0f` 就**挡不住 NaN**（比较结果是 false），于是 NaN 会一路
    // 传进物理循环，把整个世界的坐标全变成 NaN —— 那正是 PLAN S4 坑栏点名的那个坑。
    if (!(len2 > 0.0f)) { return Vec2{}; }

    // 溢出那一档不用额外判断：分量巨大到 LengthSq() 溢出成 +inf 时，
    // len 也是 +inf，而 `x / inf == 0` → 同样得到零向量。
    // （唯一漏网的是"输入分量本身就是 inf"：inf / inf = NaN。那属于**超出文档范围**的
    //   输入 —— 约定 1 不防御，见 doc/S4_CHECKLIST.md 7.1 的对照表。）
    const float len = std::sqrt(len2);
    return Vec2{x / len, y / len};
}

// 值类型的自检：8 字节说明"没有多余成员、也没有 padding 意外"。
// 放在头里 → 谁 include 谁检查；将来谁想加个成员会**立刻**炸，而不是悄悄变胖。
static_assert(sizeof(Vec2) == 8, "Vec2 must be exactly two floats, no more no less");

} // namespace mini::math
