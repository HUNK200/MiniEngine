#include "MiniTest.h"

#include <mini/math/MathUtils.h>
#include <mini/math/Vec2.h>

#include <limits>

// ------------------------------------------------------------
// math 层的单元测试（S4）。
//
// 这个文件**不需要时钟、不需要终端、不需要窗口**：
// math 是纯函数 + 纯值类型，所以每个用例都是"给输入、断输出"，
// 没有一丝"跑一会儿看数字像不像"的成分 —— 这是本项目里最好测的一层。
//
// 用例分三组（与 doc/S4_CHECKLIST.md 10.1 的清单一一对应）：
//   MathUtils —— 标量工具（8 个）
//   Vec2      —— 向量（9 个）
//   AABB/Ray  —— 步骤 2 / 3 再加（各 7 / 9 个）
//
// ⚠️ 比向量**不用** MINI_CHECK_EQ：Vec2 刻意没有 operator==，
// 而且 MINI_CHECK_EQ 拼失败消息时要 operator<<。比向量一律用 ApproxEquals；
// 比单个 float 用 MINI_CHECK_NEAR（**禁写 ==**，这是本项目的浮点约定）。
//
// ⚠️ **宏参数里的顶层逗号会被当成参数分隔符** —— 这是本文件第一次编译时踩的坑
//      （C4002「参数过多」外加一串看不懂的下游语法错误）：
//      **花括号不保护逗号，只有圆括号保护。**
//
//      这次的处置是**从根上修 `MiniTest.h`**：`MINI_CHECK` 改成可变参数 `(...)`，
//      于是下面这种写法**现在直接就能用**，不需要任何括号：
//          MINI_CHECK(Vec2{1.0f, 2.0f}.LengthSq() > 0.0f)
//
//      但 `MINI_CHECK_EQ(lhs, rhs)` / `MINI_CHECK_NEAR(lhs, rhs, eps)` 要**区分参数**，
//      可变参数拆不开 → 它们的参数里若含顶层逗号，**必须自己包一层圆括号**：
//          MINI_CHECK_NEAR((Vec2{3.0f, 4.0f}.Perp().Length()), 5.0f, kEps)   ← 括号必需
//      而本来就落在函数调用圆括号里的花括号**不用管**：
//          MINI_CHECK(v.ApproxEquals(Vec2{1.0f, 2.0f}))   ← 逗号已在第 2 层，安全
// ------------------------------------------------------------

namespace {

using mini::math::ApproxEquals;
using mini::math::Clamp;
using mini::math::DegToRad;
using mini::math::FloorToInt;
using mini::math::kPi;
using mini::math::Lerp;
using mini::math::RoundToInt;
using mini::math::Sign;
using mini::math::Vec2;

constexpr float kEps = 1e-5f;

// NaN 用标准库拿，别手写 0.0f/0.0f（那是 UB，而且编译器可能把它折叠掉）
const float kNaN = std::numeric_limits<float>::quiet_NaN();

} // namespace

// ============================================================
// 第 1 组：MathUtils（8 个）
// ============================================================

MINI_TEST(MathUtils, ClampBasic)
{
    MINI_CHECK_NEAR(Clamp(5.0f, 0.0f, 10.0f), 5.0f, kEps);
    MINI_CHECK_NEAR(Clamp(-3.0f, 0.0f, 10.0f), 0.0f, kEps);   // 下溢夹到 lo
    MINI_CHECK_NEAR(Clamp(42.0f, 0.0f, 10.0f), 10.0f, kEps);  // 上溢夹到 hi
    // 边界值本身算"在区间内"，不该被夹动
    MINI_CHECK_NEAR(Clamp(0.0f, 0.0f, 10.0f), 0.0f, kEps);
    MINI_CHECK_NEAR(Clamp(10.0f, 0.0f, 10.0f), 10.0f, kEps);
}

MINI_TEST(MathUtils, LerpEndpointsAndExtrapolation)
{
    MINI_CHECK_NEAR(Lerp(10.0f, 20.0f, 0.0f), 10.0f, kEps);
    MINI_CHECK_NEAR(Lerp(10.0f, 20.0f, 1.0f), 20.0f, kEps);
    MINI_CHECK_NEAR(Lerp(10.0f, 20.0f, 0.5f), 15.0f, kEps);

    // **刻意不钳 t**：超界时按线性外推。这两条是"设计决定"的守卫 ——
    // 谁哪天给 Lerp 顺手加了 clamp，这里就红。
    MINI_CHECK_NEAR(Lerp(10.0f, 20.0f, 2.0f), 30.0f, kEps);
    MINI_CHECK_NEAR(Lerp(10.0f, 20.0f, -1.0f), 0.0f, kEps);
}

MINI_TEST(MathUtils, DegToRadKnownValues)
{
    MINI_CHECK_NEAR(DegToRad(0.0f), 0.0f, kEps);
    MINI_CHECK_NEAR(DegToRad(90.0f), kPi / 2.0f, kEps);
    MINI_CHECK_NEAR(DegToRad(180.0f), kPi, kEps);
    MINI_CHECK_NEAR(DegToRad(360.0f), 2.0f * kPi, kEps);
    MINI_CHECK_NEAR(DegToRad(-90.0f), -kPi / 2.0f, kEps);   // 负角度
}

MINI_TEST(MathUtils, SignThreeWay)
{
    MINI_CHECK_NEAR(Sign(3.5f), 1.0f, kEps);
    MINI_CHECK_NEAR(Sign(-0.001f), -1.0f, kEps);
    MINI_CHECK_NEAR(Sign(0.0f), 0.0f, kEps);

    // ⚠️ 负零也是零。写成 (v >= 0 ? 1 : -1) 的实现会在这里红 ——
    // 而那个实现会让 Sign(-0.0f) == +1，进而在 S7 里造出一个方向相反的零步位移。
    MINI_CHECK_NEAR(Sign(-0.0f), 0.0f, kEps);
}

MINI_TEST(MathUtils, ApproxEqualsEps)
{
    MINI_CHECK(ApproxEquals(1.0f, 1.0f));
    MINI_CHECK(ApproxEquals(1.0f, 1.0f + 1e-6f));    // 差 1e-6 < 默认 eps 1e-5
    MINI_CHECK(!ApproxEquals(1.0f, 1.0f + 1e-3f));   // 差 1e-3 > 默认 eps
    MINI_CHECK(ApproxEquals(1.0f, 1.001f, 0.01f));   // 放大 eps 就相等
    MINI_CHECK(!ApproxEquals(1.0f, 1.001f, 1e-6f));  // 缩小 eps 就不相等

    // 正零与负零相等（|0 - (-0)| == 0）
    MINI_CHECK(ApproxEquals(0.0f, -0.0f));
}

MINI_TEST(MathUtils, ApproxEqualsNanIsFalse)
{
    // **正确行为，别"修"它**：NaN 与任何值比较都是 false，
    // 所以这个函数不需要任何额外分支就自动返回 false。
    // 换来的好处是：比较里一旦混进 NaN，不会"碰巧相等"。
    MINI_CHECK(!ApproxEquals(kNaN, kNaN));   // 连它自己和自己都不相等
    MINI_CHECK(!ApproxEquals(kNaN, 0.0f));
    MINI_CHECK(!ApproxEquals(0.0f, kNaN));
}

MINI_TEST(MathUtils, FloorToIntNegative)
{
    MINI_CHECK_EQ(FloorToInt(0.0f), 0);
    MINI_CHECK_EQ(FloorToInt(3.0f), 3);
    MINI_CHECK_EQ(FloorToInt(3.7f), 3);

    // ⚠️ 这三条是**整个函数存在的理由**（PLAN E-8）：
    //     (int)(-0.5f) ==  0   而   FloorToInt(-0.5f) == -1     ← 差一格
    //     (int)(-3.0f) == -3   而   FloorToInt(-3.0f) == -3     ← 整数值两者一致
    //     (int)(-3.7f) == -3   而   FloorToInt(-3.7f) == -4     ← 差一格
    // 拿截断去算 tile 索引，负坐标区就会整体偏一格（E-8 的现象）。
    MINI_CHECK_EQ(FloorToInt(-0.5f), -1);
    MINI_CHECK_EQ(FloorToInt(-3.0f), -3);
    MINI_CHECK_EQ(FloorToInt(-3.7f), -4);

    // 负零向下取整还是零（不该退成 -1）
    MINI_CHECK_EQ(FloorToInt(-0.0f), 0);
}

MINI_TEST(MathUtils, RoundToIntHalfAwayFromZero)
{
    MINI_CHECK_EQ(RoundToInt(0.0f), 0);
    MINI_CHECK_EQ(RoundToInt(0.4f), 0);
    MINI_CHECK_EQ(RoundToInt(0.5f), 1);
    MINI_CHECK_EQ(RoundToInt(2.4f), 2);
    MINI_CHECK_EQ(RoundToInt(2.5f), 3);

    // ⚠️ 负半轴：**.5 远离零**（对齐 std::round），不是"向 +inf 舍入"。
    // 写成 FloorToInt(v + 0.5f) 的实现会让 -0.5 得到 0 —— 这里会红。
    MINI_CHECK_EQ(RoundToInt(-0.4f), 0);
    MINI_CHECK_EQ(RoundToInt(-0.5f), -1);
    MINI_CHECK_EQ(RoundToInt(-2.5f), -3);
    MINI_CHECK_EQ(RoundToInt(-2.6f), -3);
}

// ============================================================
// 第 2 组：Vec2（9 个）
// ============================================================

MINI_TEST(Vec2, DefaultIsZero)
{
    const Vec2 v;
    MINI_CHECK_NEAR(v.x, 0.0f, kEps);
    MINI_CHECK_NEAR(v.y, 0.0f, kEps);

    // 聚合初始化也能用（Vec2 没有构造函数，{1,2} 直接就是两个成员）
    const Vec2 w{1.0f, 2.0f};
    MINI_CHECK_NEAR(w.x, 1.0f, kEps);
    MINI_CHECK_NEAR(w.y, 2.0f, kEps);
}

MINI_TEST(Vec2, Arithmetic)
{
    const Vec2 a{1.0f, 2.0f};
    const Vec2 b{10.0f, 20.0f};

    MINI_CHECK((a + b).ApproxEquals(Vec2{11.0f, 22.0f}));
    MINI_CHECK((b - a).ApproxEquals(Vec2{9.0f, 18.0f}));
    MINI_CHECK((-a).ApproxEquals(Vec2{-1.0f, -2.0f}));
    MINI_CHECK((a * 2.0f).ApproxEquals(Vec2{2.0f, 4.0f}));
    MINI_CHECK((2.0f * a).ApproxEquals(Vec2{2.0f, 4.0f}));   // 左乘也要能用
    MINI_CHECK((b / 2.0f).ApproxEquals(Vec2{5.0f, 10.0f}));

    // 复合赋值：改成自身**并且**返回引用（链式写法靠返回值）
    Vec2 c{1.0f, 2.0f};
    MINI_CHECK((c += b).ApproxEquals(Vec2{11.0f, 22.0f}));
    MINI_CHECK((c -= b).ApproxEquals(Vec2{1.0f, 2.0f}));
    MINI_CHECK((c *= 3.0f).ApproxEquals(Vec2{3.0f, 6.0f}));
    MINI_CHECK((c /= 3.0f).ApproxEquals(Vec2{1.0f, 2.0f}));
}

MINI_TEST(Vec2, LengthSqAndLength)
{
    const Vec2 v{3.0f, 4.0f};
    MINI_CHECK_NEAR(v.LengthSq(), 25.0f, kEps);
    MINI_CHECK_NEAR(v.Length(), 5.0f, kEps);

    MINI_CHECK_NEAR(Vec2{}.Length(), 0.0f, kEps);

    // LengthSq 存在的理由：比"谁更远"不需要开方
    MINI_CHECK(Vec2{10.0f, 0.0f}.LengthSq() > Vec2{0.0f, 3.0f}.LengthSq());
}

MINI_TEST(Vec2, NormalizedIsUnit)
{
    const Vec2 n = Vec2{3.0f, 4.0f}.Normalized();

    // MINI_CHECK_NEAR 对 NaN 会失败（NaN 与任何值比较都是 false），
    // 所以这两条同时钉住了"不是 NaN"和"等于期望值"。
    MINI_CHECK_NEAR(n.Length(), 1.0f, kEps);
    MINI_CHECK_NEAR(n.x, 0.6f, kEps);
    MINI_CHECK_NEAR(n.y, 0.8f, kEps);

    // 已经单位化的向量再归一化应该几乎不动
    MINI_CHECK(n.Normalized().ApproxEquals(n));
}

MINI_TEST(Vec2, NormalizedZeroReturnsZero)
{
    const Vec2 n = Vec2{}.Normalized();
    MINI_CHECK_NEAR(n.x, 0.0f, kEps);
    MINI_CHECK_NEAR(n.y, 0.0f, kEps);

    // 没有这条保护的话：0/0 = NaN，而 NaN 会顺着 `pos += v * dt` 传遍整个世界
    // —— 屏幕上一片空白、位置全变成 nan，且**不会崩**，最难查的那种。
    // 单位向量之外的向量（比如只有 y 分量为 0 的）也要稳住
    MINI_CHECK(Vec2{0.0f, -7.0f}.Normalized().ApproxEquals(Vec2{0.0f, -1.0f}));
}

MINI_TEST(Vec2, NormalizedNanReturnsZero)
{
    const Vec2 n = Vec2{kNaN, 1.0f}.Normalized();

    // LengthSq() 是 NaN → `!(NaN > 0.0f)` 为真 → 走零向量分支。
    // ⚠️ 这条是**反向验证 1 的目标**：把保护改成 `len2 <= 0.0f` 之后，
    // NaN 会漏过判定、最终得到 NaN —— 这条会红，而"零向量"那条**不会红**。
    // 两条用例必须"只有一条红"，否则说明它们没区分开、守卫是假的。
    MINI_CHECK_NEAR(n.x, 0.0f, kEps);
    MINI_CHECK_NEAR(n.y, 0.0f, kEps);
}

MINI_TEST(Vec2, NormalizedHugeReturnsZero)
{
    // 1e30 的平方 = 1e60，float 装不下 → LengthSq() 溢出成 +inf。
    // 于是 len = sqrt(inf) = +inf，而 1e30 / inf == 0 → 得到零向量（不是 NaN）。
    // 也就是说"溢出"这一档不需要额外判断，同一个分支就接住了。
    const Vec2 n = Vec2{1e30f, 0.0f}.Normalized();
    MINI_CHECK_NEAR(n.x, 0.0f, kEps);
    MINI_CHECK_NEAR(n.y, 0.0f, kEps);
}

MINI_TEST(Vec2, DotAndPerp)
{
    const Vec2 a{1.0f, 0.0f};

    MINI_CHECK_NEAR(a.Dot(Vec2{0.0f, 1.0f}), 0.0f, kEps);  // 垂直 → 点积为 0
    MINI_CHECK_NEAR(a.Dot(a), 1.0f, kEps);                 // 平行 → 点积 = 长度积

    // ⚠️ Perp 的方向是**写死的约定**：逆时针 90°，(x, y) → (-y, x)。
    // 搞反了的话 S7 的法线会朝里、实体被推进墙里 —— 这两条就是防那个的。
    MINI_CHECK(a.Perp().ApproxEquals(Vec2{0.0f, 1.0f}));                      // 右 → 上
    MINI_CHECK(Vec2{0.0f, 1.0f}.Perp().ApproxEquals(Vec2{-1.0f, 0.0f}));      // 上 → 左

    // ⚠️ 这对圆括号**不能省**：MINI_CHECK_NEAR 要区分 3 个参数，
    //    所以它的参数里带顶层逗号时只能靠调用方包圆括号（见 file 头与 MiniTest.h）
    MINI_CHECK_NEAR((Vec2{3.0f, 4.0f}.Perp().Length()), 5.0f, kEps);          // 长度不变
    MINI_CHECK_NEAR(a.Dot(a.Perp()), 0.0f, kEps);                             // 与原向量垂直
}

MINI_TEST(Vec2, ApproxEqualsAndConstexpr)
{
    MINI_CHECK(Vec2{1.0f, 2.0f}.ApproxEquals(Vec2{1.0f + 1e-6f, 2.0f}));
    MINI_CHECK(!Vec2{1.0f, 2.0f}.ApproxEquals(Vec2{1.0f, 2.0f + 1e-3f}));

    // **逐分量**比较，不是"长度差不多"：方向相反但长度相同的两条向量不能被判等
    MINI_CHECK(!Vec2{1.0f, 1.0f}.ApproxEquals(Vec2{-1.0f, -1.0f}));

    // ——— 编译期自检：证明 constexpr 那部分是**真的** constexpr ———
    // 下面这些会在**编译期**红，而 MINI_TEST 只在运行期红 —— 两套机制互补。
    // 注意 Length / Normalized **不敢**这么写：它们要用 std::sqrt，
    // 而 C++20 的 sqrt 不是 constexpr（S4_CHECKLIST 差异 1）。
    constexpr Vec2 kThreeFour{3.0f, 4.0f};
    static_assert(kThreeFour.LengthSq() == 25.0f, "LengthSq 应该能在编译期算出来");
    static_assert(kThreeFour.Dot(kThreeFour) == 25.0f, "Dot 应该能在编译期算出来");
    static_assert(Vec2{1.0f, 0.0f}.Perp().y == 1.0f, "Perp 应该能在编译期算出来");
    static_assert((Vec2{1.0f, 2.0f} + Vec2{3.0f, 4.0f}).x == 4.0f, "operator+ 应该能在编译期算出来");
    static_assert(kThreeFour.ApproxEquals(Vec2{3.0f, 4.0f}), "ApproxEquals 应该能在编译期算出来");
}
