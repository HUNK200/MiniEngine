#pragma once

// ============================================================
// math/MathUtils.h —— 标量工具（math 层的最底层）
//
// 这一层干什么：所有"只跟一个 float 打交道"的杂活。
// 依赖谁：**谁都不依赖** —— 连 <cmath> 都不需要。理由在各函数自己的注释里
//         （ApproxEquals / FloorToInt 要用的那几个标准库函数在 C++20 不是 constexpr，
//          而这里手算只要几行）。
// 被谁用：Vec2 / AABB / Ray（同层），以及 S5 之后的 game 层。
//
// ⚠️ 三条约定 —— 写在注释里是因为它们都不是笔误，是**决定**：
//
//   1. **NaN 不做防御**：NaN 进 NaN 出（Sign 例外，见它自己的注释）。
//      需要挡 NaN 的地方一律用**比较式**（`!(x > 0.0f)` 这种），
//      **不要用 std::min / std::max** —— 那两者遇到 NaN 的走向是实现定义的，
//      同一份代码换个 STL 就变，是"不可复现"的源头。
//
//   2. **浮点相等一律用 ApproxEquals**，不写 == 。本项目坐标量级 < 1e4，
//      所以只做绝对误差，不做相对比较（局限写在 ApproxEquals 的注释里）。
//
//   3. 这个头文件和它的邻居**只依赖标准库**，不许 include 任何 mini/ 下的东西。
//      （PLAN A.4：按变化频率分层，math 是最底下那一层；它一旦反向依赖上层，
//        后面所有阶段都会被拖住。）
// ============================================================

namespace mini::math {

// 圆周率在 float 下的最佳近似。
// 用 float 而不是 double：本项目内部一律 float，混用只会招来隐式转换和精度疑问。
inline constexpr float kPi = 3.14159265358979323846f;

// 夹到 [lo, hi]。**假定 lo <= hi**（调用方负责）。
// 手写而不是 std::clamp：省掉 <algorithm>，而且 std::clamp 在 lo > hi 时本来就是 UB，
// 手写版并没有比它更不安全。
[[nodiscard]] constexpr float Clamp(float v, float lo, float hi) noexcept
{
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

// 线性插值。**刻意不钳制 t**：t 超出 [0,1] 时按线性外推。
//
// 为什么不钳：钳了就再也拿不回外推能力（要外推得另写一个函数），而不钳的代价是 0 行代码。
// ⚠️ **PLAN 里目前没有任何地方需要外推** —— 我原先在这里写"S9 的子弹落点预测会需要"，
//    回头核对 PLAN 后确认那是**编的**（S4_CHECKLIST 偏差 D7）。保留只因它免费，
//    且用例 `LerpEndpointsAndExtrapolation` 用 t=2 把外推行为钉住了。
//
// 写成 `a + (b - a) * t` 而不是 `a * (1 - t) + b * t`：
// 后者在 t 接近 0 时要把两个大数相减，精度更差。
[[nodiscard]] constexpr float Lerp(float a, float b, float t) noexcept
{
    return a + (b - a) * t;
}

// 角度 → 弧度。角度**只在读配置/写调试输出时出现**，引擎内部一律用弧度。
[[nodiscard]] constexpr float DegToRad(float deg) noexcept
{
    return deg * (kPi / 180.0f);
}

// 符号：正 → +1，负 → -1，零 → 0。
//
// ⚠️ `-0.0f` 必须返回 **0**（不是 -1）—— 所以这里不能写成
// `(v >= 0.0f ? 1.0f : -1.0f)`：那个写法对 -0.0f 会返回 +1，而 -0.0f 是"零"。
// NaN 落到最后一行返回 0（约定 1：不防御，但也不至于吐出一个假的 ±1）。
[[nodiscard]] constexpr float Sign(float v) noexcept
{
    if (v > 0.0f) { return 1.0f; }
    if (v < 0.0f) { return -1.0f; }
    return 0.0f;
}

// 近似相等：|a - b| < eps。
//
// **只做绝对误差**，不做相对比较 / ULP 比较 —— 见下面的局限。
//
// 为什么不用 std::abs：那个在 C++20 **不是 constexpr**（<cmath> 里的 abs 同理），
// 而"两个方向都 < eps"这个写法既 constexpr 友好、又不引入 <cmath>。
// 副作用**正好是想要的**：NaN 与任何值比较都是 false → 这里自动返回 false
// （包括 ApproxEquals(NaN, NaN)）。这是**正确行为**，别"修"它。
//
// ⚠️ 局限：绝对 eps 只在小坐标下可靠。本项目坐标量级 < 1e4，1e-5 合适；
// 若将来到 1e7 那种量级，float 本身的分辨率就只剩 1 左右，这个 eps 会失去意义。
[[nodiscard]] constexpr bool ApproxEquals(float a, float b, float eps = 1e-5f) noexcept
{
    const float d = a - b;
    return d < eps && d > -eps;
}

// world(float) → tile/screen(int)。服务 PLAN E-8 的 S4 那一半。
//
// **不能用 `(int)` 直接截断** —— 截断是"向零取整"，而 tile 索引要的是"向下取整"：
//     (int)(-0.5f) == 0      但     floor(-0.5f) == -1
// 差一格。这正是 PLAN E-8 那两条现象（"画面整体偏移半格"、"走位比视觉慢一格"）的来源。
//
// 为什么不用 std::floor：**它和 std::sqrt 一样，在 C++20 不是 constexpr**
// （MSVC 的 `_CONSTEXPR23` 只在 C++23 下展开）。
// 这里手算只要 2 行，所以选择**保住 constexpr**。
// 对照：Vec2::Normalized() 要用 sqrt，手算代价太大 → 那边就放弃了 constexpr。
// 判据不是"能不能做到"，是"**多写的东西值不值它的复杂度**"。
//
// ⚠️ 输入约束：`v` 必须是**有限值**且 |v| < 2^31。
// NaN 或越界时 `static_cast<int>(v)` 是 **UB**（未定义行为，不是"截断成某个数"），
// 这里不防御 —— 调用方拿到的坐标本来就不该是 NaN；真出现 NaN 要去它产生的地方查。
[[nodiscard]] constexpr int FloorToInt(float v) noexcept
{
    const int i = static_cast<int>(v);                            // 向零截断
    return (v < 0.0f && static_cast<float>(i) != v) ? i - 1 : i;  // 负数且有余 → 再退一格
}

// 四舍五入，语义**对齐 std::round**：.5 一律**远离零**（+0.5 → 1、-0.5 → -1）。
//
// 为什么不写 `FloorToInt(v + 0.5f)`：那样在负半轴会变成"向 +inf 舍入"
// （-0.5 → 0），与标准库不一致。标准库已经有约定时，**跟着它走**比自己发明一套安全。
//
// 为什么不用 std::round：原因和 std::floor 一样（C++20 不是 constexpr）。
[[nodiscard]] constexpr int RoundToInt(float v) noexcept
{
    if (v >= 0.0f) { return FloorToInt(v + 0.5f); }
    return -FloorToInt(-v + 0.5f);
}

} // namespace mini::math
