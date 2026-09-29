#include "MiniTest.h"

#include <mini/math/AABB.h>
#include <mini/math/MathUtils.h>
#include <mini/math/Ray.h>
#include <mini/math/Vec2.h>

#include <limits>
#include <optional>

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

using mini::math::AABB;
using mini::math::ApproxEquals;
using mini::math::Clamp;
using mini::math::DegToRad;
using mini::math::FloorToInt;
using mini::math::kPi;
using mini::math::Lerp;
using mini::math::Ray;
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

// ============================================================
// 第 3 组：AABB（7 个）
//
// 这一组里有两件事**故意反直觉**，各自都有专门用例：
//   · Contains 是**闭**区间（边界算在内）
//   · Intersects 是**开**区间（贴边不算相交）—— 反向验证 2 就钉它
// ============================================================

MINI_TEST(AABB, FromCenterSizeAndAccessors)
{
    // size 是**全宽全高**（不是半宽）：中心 {5,5} + 尺寸 {2,4} → min{4,3} max{6,7}
    const AABB box = AABB::FromCenterSize(Vec2{5.0f, 5.0f}, Vec2{2.0f, 4.0f});
    MINI_CHECK_NEAR(box.min.x, 4.0f, kEps);
    MINI_CHECK_NEAR(box.min.y, 3.0f, kEps);
    MINI_CHECK_NEAR(box.max.x, 6.0f, kEps);
    MINI_CHECK_NEAR(box.max.y, 7.0f, kEps);

    // Center / Size 与构造互逆（往返一致）
    MINI_CHECK_NEAR(box.Center().x, 5.0f, kEps);
    MINI_CHECK_NEAR(box.Center().y, 5.0f, kEps);
    MINI_CHECK_NEAR(box.Size().x, 2.0f, kEps);
    MINI_CHECK_NEAR(box.Size().y, 4.0f, kEps);

    MINI_CHECK(box.IsValid());
}

MINI_TEST(AABB, FromCornersSortsAutomatically)
{
    // **反序**传进去（先给"右下"再给"左上"）也必须得到正确的 min/max。
    // 不排序会造出反盒（min > max），而反盒上所有判定都**静默**失效 ——
    // 不崩不报错，只是结果没有意义，属于最难查的一类。
    const AABB box = AABB::FromCorners(Vec2{6.0f, 7.0f}, Vec2{4.0f, 3.0f});
    MINI_CHECK_NEAR(box.min.x, 4.0f, kEps);
    MINI_CHECK_NEAR(box.min.y, 3.0f, kEps);
    MINI_CHECK_NEAR(box.max.x, 6.0f, kEps);
    MINI_CHECK_NEAR(box.max.y, 7.0f, kEps);
    MINI_CHECK(box.IsValid());

    // 顺序传进去结果必须**完全一样**（排序是唯一的差别，不该有别的副作用）
    const AABB same = AABB::FromCorners(Vec2{4.0f, 3.0f}, Vec2{6.0f, 7.0f});
    MINI_CHECK_NEAR(same.min.x, box.min.x, kEps);
    MINI_CHECK_NEAR(same.max.y, box.max.y, kEps);
}

MINI_TEST(AABB, ContainsIncludesEdges)
{
    const AABB box = AABB::FromCorners(Vec2{2.0f, 2.0f}, Vec2{5.0f, 6.0f});

    MINI_CHECK(box.Contains(Vec2{3.0f, 4.0f}));    // 内部

    // **四条边**上的点都算在内（闭区间）
    MINI_CHECK(box.Contains(Vec2{2.0f, 4.0f}));    // 左边
    MINI_CHECK(box.Contains(Vec2{5.0f, 4.0f}));    // 右边
    MINI_CHECK(box.Contains(Vec2{3.0f, 2.0f}));    // 上边
    MINI_CHECK(box.Contains(Vec2{3.0f, 6.0f}));    // 下边

    // 四个角当然也算（"点在盒子里"的直觉）
    MINI_CHECK(box.Contains(Vec2{2.0f, 2.0f}));
    MINI_CHECK(box.Contains(Vec2{5.0f, 6.0f}));

    // 出去一点点就不算
    MINI_CHECK(!box.Contains(Vec2{1.99f, 4.0f}));
    MINI_CHECK(!box.Contains(Vec2{5.01f, 4.0f}));
    MINI_CHECK(!box.Contains(Vec2{3.0f, 1.99f}));
    MINI_CHECK(!box.Contains(Vec2{3.0f, 6.01f}));
}

MINI_TEST(AABB, TouchingIsNotIntersecting)
{
    const AABB a = AABB::FromCorners(Vec2{0.0f, 0.0f}, Vec2{2.0f, 2.0f});

    // ⚠️ 这条是**反向验证 2 的目标**：把 Intersects 的 `<` 改成 `<=`，它就会红。
    //
    // 右边那个盒子正好贴着 a 的右边界（2.0）：它们只是**相邻**，不是碰撞。
    // 用 `<=` 的后果不是"多算一点"这么轻 —— S7 里紧贴墙壁的实体每帧都会被判碰撞、
    // 被推开一格，表现是"怎么走都贴不住墙"。
    const AABB touchingRight = AABB::FromCorners(Vec2{2.0f, 0.0f}, Vec2{4.0f, 2.0f});
    MINI_CHECK(!a.Intersects(touchingRight));
    MINI_CHECK(!touchingRight.Intersects(a));   // 必须对称

    // 另外三条边也各来一次（免得只钉住了 x 轴）
    MINI_CHECK(!a.Intersects(AABB::FromCorners(Vec2{-2.0f, 0.0f}, Vec2{0.0f, 2.0f})));
    MINI_CHECK(!a.Intersects(AABB::FromCorners(Vec2{0.0f, -2.0f}, Vec2{2.0f, 0.0f})));
    MINI_CHECK(!a.Intersects(AABB::FromCorners(Vec2{0.0f, 2.0f}, Vec2{2.0f, 4.0f})));
}

MINI_TEST(AABB, OverlapAndContainmentIntersects)
{
    const AABB a = AABB::FromCorners(Vec2{0.0f, 0.0f}, Vec2{4.0f, 4.0f});

    // 真重叠（各进去一点）
    MINI_CHECK(a.Intersects(AABB::FromCorners(Vec2{3.0f, 3.0f}, Vec2{7.0f, 7.0f})));

    // 一边完全包含另一边 —— 双向都要成立
    const AABB inner = AABB::FromCorners(Vec2{1.0f, 1.0f}, Vec2{2.0f, 2.0f});
    MINI_CHECK(a.Intersects(inner));
    MINI_CHECK(inner.Intersects(a));

    // 与自己相交（非退化时成立）—— 开区间下 `min < max` 为真
    MINI_CHECK(a.Intersects(a));

    // 十字交叉：x 和 y 上都重叠，但谁都不含谁
    MINI_CHECK(a.Intersects(AABB::FromCorners(Vec2{1.0f, -1.0f}, Vec2{3.0f, 5.0f})));
}

MINI_TEST(AABB, ValidityAndDegenerate)
{
    // 反盒（min > max）：IsValid 报 false。
    //
    // ⚠️ 这里**故意不测 Intersects 的结果** —— 反盒上它没有意义：
    //    公式照样会算出一个 true/false，但那个值不对应任何几何事实。
    //    反盒应该在**构造阶段**就避免；走 FromCorners() 永远造不出反盒，
    //    只有直接写 {min, max} 两个成员才可能。
    const AABB reversed{Vec2{5.0f, 0.0f}, Vec2{1.0f, 10.0f}};
    MINI_CHECK(!reversed.IsValid());

    // 只在**一个轴**上反了也算无效（IsValid 是逐分量判的）
    MINI_CHECK(!AABB{Vec2{0.0f, 5.0f}, Vec2{5.0f, 1.0f}}.IsValid());

    // 合法盒
    MINI_CHECK(AABB{Vec2{}, Vec2{1.0f, 1.0f}}.IsValid());

    // 退化成一个点（min == max）：**合法**，但**不与自己相交**（开区间下 min < max 为假）
    const AABB point{Vec2{2.0f, 2.0f}, Vec2{2.0f, 2.0f}};
    MINI_CHECK(point.IsValid());
    MINI_CHECK(!point.Intersects(point));

    // 退化成一条线（一个轴相等）也不与自己相交
    const AABB line{Vec2{0.0f, 3.0f}, Vec2{4.0f, 3.0f}};
    MINI_CHECK(line.IsValid());
    MINI_CHECK(!line.Intersects(line));
}

MINI_TEST(AABB, DegeneratePointBoxSemantics)
{
    // 这条把"两处边界语义故意不一致"钉成**可执行的事实**，
    // 同时钉住**退化盒上 Intersects 的真实行为**。
    //
    // ⚠️ 第一版写错过：我原以为"退化点盒不与任何东西相交" —— 只对了**一半**。
    //    正确说法是"**退化盒上的结果是开区间公式作用于零宽区间的自然产物**"，
    //    分三种情况，下面逐个断言。教训记在 S4_CHECKLIST 偏差 D6。
    const AABB point{Vec2{1.0f, 1.0f}, Vec2{1.0f, 1.0f}};

    // ——— Contains 是闭区间：那个点在"盒内" ———
    MINI_CHECK(point.Contains(Vec2{1.0f, 1.0f}));
    MINI_CHECK(point.Contains(point.min));

    // ——— Intersects 是开区间；退化时**看对方在哪** ———

    // ① 点盒 vs 它自己 → **false**（`1 < 1` 不成立）
    MINI_CHECK(!point.Intersects(point));

    // ② 点盒在另一个盒子的**内部** → **true**
    //    `1 < 2 && 0 < 1` 两条都成立（重叠长度是 0，但公式不看长度）
    //    ⚠️ 所以**别把退化盒当"闭区间的点"用**：点与盒的查询请用 Contains()。
    MINI_CHECK(point.Intersects(AABB::FromCorners(Vec2{0.0f, 0.0f}, Vec2{2.0f, 2.0f})));

    // ③ 点盒落在另一个盒子的**边界**上 → false（`2 < 2` 不成立）。
    //    这一条恰好和"贴边不算相交"同源，是自洽的。
    const AABB onEdge{Vec2{2.0f, 2.0f}, Vec2{2.0f, 2.0f}};
    MINI_CHECK(!onEdge.Intersects(AABB::FromCorners(Vec2{0.0f, 0.0f}, Vec2{2.0f, 2.0f})));

    // 同一次判断里两个答案不同 —— 不是矛盾，是两处语义各服务不同用途
    //（**查询用 Contains，碰撞用 Intersects**），见 AABB.h 文件头那张表与 Intersects 的注释。
    MINI_CHECK(point.Contains(Vec2{1.0f, 1.0f}) != point.Intersects(point));
}

// ============================================================
// 第 4 组：Ray（9 个）
//
// 这一组重点不是"算得对"，而是**几个边界语义有没有被钉住**：
//   · 返回的 t 是 `dir` 的倍数，**不是**欧氏距离
//   · 起点在盒内 / 在边界上 → t = 0
//   · 擦角（tNear == tFar）→ **算命中**
//   · `d == 0` 的显式分支（反向验证 3 的目标，见 29 的两半）
// ============================================================

// 测试帮手：安全地从 optional 里取 t。
// 先断言"确实有值"，避免测试自己踩空 optional 的 UB；
// 取不到时返回一个显眼的哨兵，让紧随其后的 MINI_CHECK_NEAR 也一起红。
[[nodiscard]] float HitT(const std::optional<float>& hit) noexcept
{
    MINI_CHECK(hit.has_value());
    return hit.value_or(-9999.0f);
}

MINI_TEST(Ray, HitsFromFront)
{
    // 盒子 x∈[2,4] y∈[-1,1]；从原点沿 +x 出发 → 在 x=2 处进入 → t = 2
    // （这条同时覆盖了"y 轴 d == 0 且在内 → 该轴不限制 t"）
    const Ray ray{Vec2{0.0f, 0.0f}, Vec2{1.0f, 0.0f}};
    const AABB box = AABB::FromCorners(Vec2{2.0f, -1.0f}, Vec2{4.0f, 1.0f});

    const float t = HitT(ray.IntersectsAABB(box));
    MINI_CHECK_NEAR(t, 2.0f, kEps);

    // 命中点 = origin + dir * t（dir 是单位向量，所以 t 在数值上等于距离）
    const Vec2 hit = ray.origin + ray.dir * t;
    MINI_CHECK_NEAR(hit.x, 2.0f, kEps);
    MINI_CHECK_NEAR(hit.y, 0.0f, kEps);
}

MINI_TEST(Ray, MissesWhenBehind)
{
    // 同一个盒子，但射线朝反方向 → 盒子整个在背后 → nullopt
    const Ray ray{Vec2{0.0f, 0.0f}, Vec2{-1.0f, 0.0f}};
    const AABB box = AABB::FromCorners(Vec2{2.0f, -1.0f}, Vec2{4.0f, 1.0f});

    MINI_CHECK(!ray.IntersectsAABB(box).has_value());

    // 边界情况：起点在盒子**前方**、朝**远离**盒子的方向 —— 也是背后
    MINI_CHECK(!Ray{Vec2{10.0f, 0.0f}, Vec2{1.0f, 0.0f}}.IntersectsAABB(box).has_value());
}

MINI_TEST(Ray, OriginInsideReturnsZero)
{
    // 起点已经在盒内 → 第一次"进入"就是现在 → t = 0（PLAN S4 验收明文要求）
    const AABB box = AABB::FromCorners(Vec2{2.0f, -1.0f}, Vec2{4.0f, 1.0f});

    MINI_CHECK_NEAR(HitT(Ray{Vec2{3.0f, 0.0f}, Vec2{1.0f, 0.0f}}.IntersectsAABB(box)), 0.0f, kEps);

    // 朝任何一个方向出发都一样是 0（四个方向都试，免得只有 +x 那条路是对的）
    MINI_CHECK_NEAR(HitT(Ray{Vec2{3.0f, 0.0f}, Vec2{-1.0f, 0.0f}}.IntersectsAABB(box)), 0.0f, kEps);
    MINI_CHECK_NEAR(HitT(Ray{Vec2{3.0f, 0.0f}, Vec2{0.0f, 1.0f}}.IntersectsAABB(box)), 0.0f, kEps);
    MINI_CHECK_NEAR(HitT(Ray{Vec2{3.0f, 0.0f}, Vec2{0.0f, -1.0f}}.IntersectsAABB(box)), 0.0f, kEps);
}

MINI_TEST(Ray, OriginOnBoundaryReturnsZero)
{
    // 起点**恰在盒子的面上**：`o > box.max` / `o < box.min` 都是严格判断，所以边界算"在内"
    const AABB box = AABB::FromCorners(Vec2{2.0f, -1.0f}, Vec2{4.0f, 1.0f});

    // 在左面上、朝外（-x）出发：t = 0（"这一刻就贴着它"）
    MINI_CHECK_NEAR(HitT(Ray{Vec2{2.0f, 0.0f}, Vec2{-1.0f, 0.0f}}.IntersectsAABB(box)), 0.0f, kEps);

    // 在右面上、朝外（+x）出发：同样是 t = 0
    MINI_CHECK_NEAR(HitT(Ray{Vec2{4.0f, 0.0f}, Vec2{1.0f, 0.0f}}.IntersectsAABB(box)), 0.0f, kEps);

    // 在左面上、朝**里**出发：进入参数仍然从 0 起算
    MINI_CHECK_NEAR(HitT(Ray{Vec2{2.0f, 0.0f}, Vec2{1.0f, 0.0f}}.IntersectsAABB(box)), 0.0f, kEps);
}

MINI_TEST(Ray, ParallelOutsideReturnsNullopt)
{
    // ⚠️ **这条是反向验证 3 的目标**。它的两半难度差很多，别只看第一半：
    const AABB box = AABB::FromCorners(Vec2{2.0f, -1.0f}, Vec2{4.0f, 1.0f});

    // ① 平行且在**上侧**（o.y > max.y → o.y = 5）→ nullopt
    //    ⚠️ 这一半**抓不住** `1/d` 写法：IEEE 恰好也给 nullopt
    //       （tFar 被推成 -inf，被"盒子在背后"那条规则兜住）。
    //       只写这一半的话，这个用例就是**摆设** —— 文档 10.3 早就警告过这件事。
    MINI_CHECK(!Ray{Vec2{0.0f, 5.0f}, Vec2{1.0f, 0.0f}}.IntersectsAABB(box).has_value());

    // ② 平行且在**下侧**（o.y < min.y → o.y = -5）→ nullopt
    //    ✅ **这一半才是真正的守卫**：`1/d` 写法会把两个 t 都算成 +inf，
    //       于是 tNear = +inf，最后返回 **t = +inf**（"在无穷远处打中了"）——
    //       既不 nullopt 也不崩。删掉 `d == 0` 分支时，红的就是这一条。
    MINI_CHECK(!Ray{Vec2{0.0f, -5.0f}, Vec2{1.0f, 0.0f}}.IntersectsAABB(box).has_value());

    // 左右两侧同理（射线平行于 y 轴）
    MINI_CHECK(!Ray{Vec2{5.0f, 0.0f}, Vec2{0.0f, 1.0f}}.IntersectsAABB(box).has_value());
    MINI_CHECK(!Ray{Vec2{-5.0f, 0.0f}, Vec2{0.0f, 1.0f}}.IntersectsAABB(box).has_value());
}

MINI_TEST(Ray, ParallelInsideHits)
{
    // 盒子 x∈[2,4] y∈[-1,1]
    const AABB box = AABB::FromCorners(Vec2{2.0f, -1.0f}, Vec2{4.0f, 1.0f});

    // —— 平行且在 slab **内部**：那条轴不限制 t，由另一条轴决定 ——
    // 起点 x = -5（盒外），方向 {10, 0}（**故意不归一化**）：
    //   x 轴：t1 = (2-(-5))/10 = 0.7、t2 = (4+5)/10 = 0.9 → tNear = 0.7
    //   y 轴：d == 0 且 0 ∈ [-1,1] → 不限制
    // ⚠️ 0.7 是 **dir 的倍数**，不是距离（真实距离是 0.7 × 10 = 7）
    const Ray alongX{Vec2{-5.0f, 0.0f}, Vec2{10.0f, 0.0f}};
    MINI_CHECK_NEAR(HitT(alongX.IntersectsAABB(box)), 0.7f, kEps);

    // 命中点确实在盒子的左面上
    const float t = HitT(alongX.IntersectsAABB(box));
    const Vec2 hit = alongX.origin + alongX.dir * t;
    MINI_CHECK_NEAR(hit.x, 2.0f, kEps);

    // —— 平行且**恰在 slab 边界上**：边界算"在内"，贴着上表面滑过去的射线**算打中** ——
    //    ⚠️ "擦边 + 平行"正是会算出 `0 * inf = NaN` 的输入。
    //    在这条上：MSVC 的 std::min 会把 NaN 悄悄丢掉、结果恰好和显式分支一致，
    //    所以它**不保证**会红；真正会红的是 29 的 ②。
    const Ray onTop{Vec2{0.0f, 1.0f}, Vec2{1.0f, 0.0f}};   // o.y 恰等于 box.max.y
    MINI_CHECK_NEAR(HitT(onTop.IntersectsAABB(box)), 2.0f, kEps);
}

MINI_TEST(Ray, ZeroDirectionDegeneratesToPointTest)
{
    // `dir == {0,0}` 是**调用方违约**（文件头 ② 写明必须非零），但代码不崩：
    // 两条轴都走"平行"分支 → 都不限制 t → 最后返回 max(-inf, 0) = 0。
    // 于是它**退化成一次"点在不在盒内"的测试**。
    const AABB box = AABB::FromCorners(Vec2{2.0f, -1.0f}, Vec2{4.0f, 1.0f});

    // 起点在盒内 → 0（和"点在盒内"的直觉一致）
    MINI_CHECK_NEAR(HitT(Ray{Vec2{3.0f, 0.0f}, Vec2{0.0f, 0.0f}}.IntersectsAABB(box)), 0.0f, kEps);

    // 起点在盒外 → nullopt（平行且在外）
    MINI_CHECK(!Ray{Vec2{5.0f, 0.0f}, Vec2{0.0f, 0.0f}}.IntersectsAABB(box).has_value());
    MINI_CHECK(!Ray{Vec2{0.0f, -5.0f}, Vec2{0.0f, 0.0f}}.IntersectsAABB(box).has_value());
}

MINI_TEST(Ray, DiagonalHitDistanceMatchesManualValue)
{
    // 盒子 x∈[2,4] y∈[1,4]；起点原点、方向 {1,1} → 手算：
    //   x 轴：t1 = 2、t2 = 4
    //   y 轴：t1 = 1、t2 = 4
    //   tNear = max(2, 1) = 2、tFar = min(4, 4) = 4 → 命中 t = 2
    //   命中点 = {0,0} + {1,1}*2 = {2,2} → 正好在**左面**上（不是角）
    const Ray ray{Vec2{0.0f, 0.0f}, Vec2{1.0f, 1.0f}};
    const AABB box = AABB::FromCorners(Vec2{2.0f, 1.0f}, Vec2{4.0f, 4.0f});

    const float t = HitT(ray.IntersectsAABB(box));
    MINI_CHECK_NEAR(t, 2.0f, kEps);

    const Vec2 hit = ray.origin + ray.dir * t;
    MINI_CHECK_NEAR(hit.x, 2.0f, kEps);   // 在左面上
    MINI_CHECK_NEAR(hit.y, 2.0f, kEps);

    // ⚠️ 顺手钉住"t 不是欧氏距离"：欧氏距离 = |dir| * t = sqrt(2) * 2 ≈ 2.828，
    //    和 t = 2 差了 41% —— 别在别处把它们当同一个数用。
    MINI_CHECK_NEAR(ray.dir.Length() * t, 2.8284271f, 1e-4f);
}

MINI_TEST(Ray, GrazesCorner)
{
    // 盒子 x∈[2,4] y∈[2,4]；起点 {0,2}、方向 {1,1} → 手算：
    //   x 轴：t1 = (2-0)/1 = 2、t2 = 4
    //   y 轴：t1 = (2-2)/1 = 0、t2 = (4-2)/1 = 2
    //   tNear = max(2, 0) = 2、tFar = min(4, 2) = 2 → **tNear == tFar == 2**
    //   命中点 = {0,2} + {1,1}*2 = {2,4} = 盒子的角（min.x, max.y）
    // 这正是"擦角"：射线只在那**一个点**上碰到盒子，前后都立刻离开。
    const Ray ray{Vec2{0.0f, 2.0f}, Vec2{1.0f, 1.0f}};
    const AABB box = AABB::FromCorners(Vec2{2.0f, 2.0f}, Vec2{4.0f, 4.0f});

    // ⚠️ **算命中**（分离判定用 `tNear > tFar`，严格大于）—— 坑 #14
    const float t = HitT(ray.IntersectsAABB(box));
    MINI_CHECK_NEAR(t, 2.0f, kEps);

    const Vec2 hit = ray.origin + ray.dir * t;
    MINI_CHECK_NEAR(hit.x, 2.0f, kEps);
    MINI_CHECK_NEAR(hit.y, 4.0f, kEps);

    // 擦角是**单点**接触：t 往前/往后挪一点点就已经出盒了
    MINI_CHECK(!box.Contains(ray.origin + ray.dir * (t - 0.001f)));
    MINI_CHECK(!box.Contains(ray.origin + ray.dir * (t + 0.001f)));

    // 对照：真切进一个面的射线，往前走一点**还在**盒内 ——
    // 说明上面那两条不是"随便写都成立"的空话。
    const Ray throughFace{Vec2{0.0f, 0.0f}, Vec2{1.0f, 1.0f}};
    const AABB faceBox = AABB::FromCorners(Vec2{2.0f, 1.0f}, Vec2{4.0f, 4.0f});
    const float tFace = HitT(throughFace.IntersectsAABB(faceBox));
    MINI_CHECK(faceBox.Contains(throughFace.origin + throughFace.dir * (tFace + 0.001f)));
}

