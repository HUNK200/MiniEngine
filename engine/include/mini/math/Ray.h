#pragma once

// ============================================================
// math/Ray.h —— 射线（起点 + 方向）与 AABB 求交（slab 法）
//
// 这一层干什么：回答两个问题 ——
//   ① 从 `origin` 沿 `dir` 出发，会不会打到这个盒子？
//   ② 打中的话，是**哪个参数 t**（第一次进入盒子的位置）？
//
// 依赖谁：math/AABB.h（以及它带的 Vec2.h / MathUtils.h）。
// 被谁用（**已核对 PLAN，不是猜的**）：
//   · PLAN:455 把 `Ray::IntersectsAABB`（slab 法）列为 **S4 的交付物**
//   · PLAN:457 把"AABB 的 slab 法"列为 S4 要学的引擎知识
//   ⚠️ **S4 阶段（现在）它没有任何生产调用者** —— 只有测试在调（与 AABB 一样）。
//      真要用它的地方（拾取 / 视线 / 命中判定）都还没写到，见 6.6 的触发条件。
//
// ------------------------------------------------------------
// ⚠️ 三条"约定死在注释里"的东西
//
// ① **返回的 t 是 `dir` 的倍数，不是欧氏距离**（除非 `dir` 恰好是单位向量）：
//        命中点 = origin + dir * t
//    好处是**不需要先归一化**（S9 的子弹速度自带长度，直接拿它当 dir 就行）。
//    想换成世界距离，乘一次 `dir.Length()` 即可 —— 别在这里做。
//
// ② **`dir` 必须非零**（调用方保证）。`dir == {0,0}` 不崩，会退化成"点测试"：
//    起点在盒内 → t = 0；在外 → nullopt。这是**故意的**，有用例钉着。
//
// ③ **擦角算命中**：分离判定用 `tNear > tFar`（严格大于），
//    所以射线恰好擦过盒子角（`tNear == tFar`）**算打中**。见用例 `GrazesCorner`。
//    另一头：**起点恰在盒子边界上 → t = 0**（边界算"在内"）。
//
// ------------------------------------------------------------
// ⚠️ 为什么**必须显式写 `d == 0` 分支**，而不是靠 IEEE 的 `1/0 = ±inf`
//
// 教科书写法是 `t1 = (lo - o) * (1/d)`，让 `±inf` 去参与 max/min 自然收敛。
// **绝大多数输入下它是对的** —— 但有两类输入会坏，而且坏得很安静：
//
//   情况 A：`o` 恰好落在 slab 边界上（`lo - o == 0` 或 `hi - o == 0`）且 `d == 0`
//           → 算出 `0 * inf = NaN`。
//           而 `std::min` / `std::max` 的实现是 `a < b ? b : a` ——
//           `NaN < x` 恒为假，于是 **NaN 被悄悄丢掉**，算出来一个"看着正常"的值。
//           也就是说：**别指望 NaN 会传染出去被你发现**。
//
//   情况 B（更严重）：`d == 0` 且 `o < lo`（平行、在层的**下侧**）
//           → 两个 t 都是 `+inf`，`tNear` 被推成 `+inf`，最后一行的钳制给出
//           `t = +inf`，也就是**"在无穷远处打中了"** —— 既不返回 nullopt，也不崩。
//           （另一侧 `o > hi` 恰好给出 `-inf`，会被"盒子在背后"那条规则兜住，
//             所以**只有下侧这一半会错**。用例 29 专门盯这一半。）
//
// 显式分流之后：这两类输入走的是两条**平凡的**分支（"平行且在外 → nullopt"、
// "平行且在内 → 这条轴不限制 t"），除法根本不发生，NaN 无从产生。
// 附带好处：两条 nullopt 的路都能单独构造用例，于是"这段逻辑被测到了"是可证的。
// ============================================================

#include "mini/math/AABB.h"
#include "mini/math/MathUtils.h"

#include <algorithm>   // std::max / std::min
#include <limits>      // std::numeric_limits（用 ±infinity 当初始的"无约束"区间）
#include <optional>

namespace mini::math {
namespace detail {

// 单轴 slab 裁剪：把 t 的可行区间 [tNear, tFar] 收缩到"这条轴允许的那一段"。
//
// 返回 false = 可以早退（平行且在外，或收缩后交集为空）。
// 注意这里判"在外"用的是**严格**不等号，所以 `o` 恰好落在边界上算"在内"。
[[nodiscard]] constexpr bool ClipSlab(float o, float d, float lo, float hi,
                                     float& tNear, float& tFar) noexcept
{
    if (d == 0.0f) {
        // ⚠️ 显式分支 —— 别删掉改成 (1/d)，见文件头"为什么"
        if (o < lo || o > hi) {
            return false;   // 平行且在外侧 → 这条射线永远打不中这个盒子
        }
        return true;        // 平行且在内 → 这条轴不限制 t，什么都不做
    }

    const float inv = 1.0f / d;
    float t1 = (lo - o) * inv;
    float t2 = (hi - o) * inv;
    if (t1 > t2) {
        // `d < 0` 时两个端点的先后会反过来。手写交换而不用 std::swap：
        // std::swap 到 C++20 才是 constexpr，手写两行没有这个依赖。
        const float tmp = t1;
        t1 = t2;
        t2 = tmp;
    }

    tNear = std::max(tNear, t1);
    tFar  = std::min(tFar,  t2);

    // **等于也算**：`tNear == tFar` 就是"擦角"，算命中（见文件头 ③）
    return tNear <= tFar;
}

} // namespace detail

struct Ray {
    Vec2 origin{};
    Vec2 dir{1.0f, 0.0f};   // ⚠️ 不必是单位向量，但**必须非零**（调用方保证，见文件头 ②）

    // 求与盒子的第一次相交。
    //   返回 nullopt = 打不中（含"平行且在外"和"盒子整个在背后"）
    //   返回 t       = 进入盒子的参数；**起点在盒内 → t = 0**
    //
    // 用 `std::optional<float>` 而不是 `bool` + 出参：命中信息就在返回值里，
    // 调用点读得出来（`bool` + `float&` 出参容易忘传、也看不出 t 是入还是出），
    // 而且 `Ray` 保持**无状态**（TECH_DEBT B5：新接口要能回答"谁在用"，
    // 现在是测试在用，触发条件是"第一次做拾取/视线判定"）。
    [[nodiscard]] constexpr std::optional<float> IntersectsAABB(const AABB& box) const noexcept
    {
        float tNear = -std::numeric_limits<float>::infinity();
        float tFar  =  std::numeric_limits<float>::infinity();

        // 逐轴收缩 —— 和 AABB::Intersects 是**同一个套路**（把 2D 拆成两个 1D），
        // 只不过这里的 1D 区间长在**参数轴 t** 上，而不是坐标轴上。
        if (!detail::ClipSlab(origin.x, dir.x, box.min.x, box.max.x, tNear, tFar)) {
            return std::nullopt;
        }
        if (!detail::ClipSlab(origin.y, dir.y, box.min.y, box.max.y, tNear, tFar)) {
            return std::nullopt;
        }

        // 走到这里说明两条轴都"有交集"，但盒子可能**整个在射线背后**
        if (tFar < 0.0f) {
            return std::nullopt;
        }

        // 起点在盒内时 tNear < 0，钳成 0 —— "已经在盒子里，进入参数就是 0"
        return std::max(tNear, 0.0f);
    }
};

// 值类型的自检：16 字节 = 两个 Vec2，不多不少
static_assert(sizeof(Ray) == 16, "Ray must be exactly two Vec2, no more no less");

// 编译期自检：证明 IntersectsAABB 是**真的** constexpr（不是"写了 constexpr 但编不过"）
// 盒子 x∈[2,4] y∈[-1,1]，从原点沿 +x 出发 → 在 x=2 处进入，t = 2
// 用 `operator*` 取值而不是 `value()`：前者 C++17 起就保证 constexpr，后者要 C++20
constexpr Ray kSelfCheckRay{Vec2{0.0f, 0.0f}, Vec2{1.0f, 0.0f}};
constexpr auto kSelfCheckHit =
    kSelfCheckRay.IntersectsAABB(AABB{Vec2{2.0f, -1.0f}, Vec2{4.0f, 1.0f}});
static_assert(kSelfCheckHit.has_value(), "从原点沿 +x 应该打中右侧的盒子");
static_assert(*kSelfCheckHit == 2.0f, "命中参数应该是 2（slab 的除法在这里恰好是精确的）");

} // namespace mini::math
