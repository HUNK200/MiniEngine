#include "MiniTest.h"

#include <mini/math/MathUtils.h>
#include <mini/world/Collision.h>

#include <limits>   // std::numeric_limits<float>::quiet_NaN()（Sweep 的退化输入用例）

// ------------------------------------------------------------
// world/Collision 的单元测试（S7 步骤 1 建立；2026-10-08 按**点模型**重算了期望值）
//
// ⚠️ **2026-10-08 模型改过**（S7_CHECKLIST 偏差 9/10）：实体从"1×1 的盒子"改成
//    "**它渲染出来的那一格**"（`ToIndex(pos)`）。所以：
//      · 判据从 `OverlapsSolid`（正面积重叠）换成 `IsSolidAt(map, pos)`（中心所在格实心吗）；
//      · 接触位置从 `整格 ∓ 0.5`（精确半数）换成 `格边界 ∓ kContactEpsilon`（ε = 1e-4）；
//      · **跨格边界本身不再是障碍** —— 只有目标格实心才拦（对角线上的缝因此能穿）。
//    用例条数**仍然是 17**：四条描述旧概念的直接改写（名字也换了），其余只重算期望值。
//
// 这个文件**不需要终端**：Collision 不读输入、不画东西、不认识 Entity —— 整层都能离线验证
// （S2/S3 起"能离线测的就离线测"的延续）。
//
// ⚠️ 地图只有 32×32，而且是**全空**的：用例自己用 `Set` 铺那几格墙。
//   · 这样每个用例的"墙在哪"在用例里看得见，不用去猜 demo 地图的布局。
//   · 边界用例（14）**故意不铺外沿墙**：越界本身就是墙（TileMap.h 约定 1）——
//     而且 `TileMap::Set` **越界是静默忽略的**（TileMap.h:59），想在界外铺墙是写不进去的（坑 9）。
//
// ⚠️ 四条写用例时的纪律（前几个阶段踩出来的）：
//   · `Vec2` **没有 `operator<<`**（Vec2.h 刻意不提供）→ 不能塞进 `MINI_CHECK_EQ`，
//     逐分量用 `MINI_CHECK_NEAR` / 本文件的 `NearPos`。
//   · 浮点比较**不写 `==`**（MathUtils.h 的规矩）→ 一律 `ApproxEquals`（默认 eps = 1e-5）。
//   · 期望值里的 `kContactEpsilon` **从常量取**、不从实现里抄数：这样"改 ε"不会让用例谎报通过。
//   · **不许**为了"让用例过"去放宽容差 —— 要改就改**测量口径**（S7 偏差 5 的教训）。
// ------------------------------------------------------------

namespace {

using mini::math::ApproxEquals;
using mini::math::Vec2;
using mini::world::IsSolidAt;
using mini::world::kContactEpsilon;
using mini::world::kMaxStepPerTick;
using mini::world::MoveAndCollide;
using mini::world::SweepSampleAt;
using mini::world::SweepStepCount;
using mini::world::Tile;
using mini::world::TileMap;

constexpr int kMapW = 32;
constexpr int kMapH = 32;

// 用例里反复出现的那几格
constexpr int kWallX = 10;     // 竖着看：墙在 x = 10 这一列
constexpr int kWallY = 5;      // 横着看：墙在 y = 5 这一行
constexpr int kCrossRow = 10;  // 十字墙的横墙所在行

TileMap EmptyMap()
{
    return TileMap{kMapW, kMapH};
}

TileMap MapWithWall(int tx, int ty)
{
    TileMap map{kMapW, kMapH};
    map.Set(tx, ty, Tile::Wall);
    return map;
}

// 一整列竖墙（x = tx，所有行）—— 用来测"一轴永远被挡、另一轴照走"（贴墙滑行）
TileMap MapWithColumn(int tx)
{
    TileMap map{kMapW, kMapH};
    for (int y = 0; y < kMapH; ++y) {
        map.Set(tx, y, Tile::Wall);
    }
    return map;
}

// 一整列竖墙 + 一整行横墙 → 用来造"顶墙角"（两个轴都被挡住）
TileMap MapWithCross(int col, int row)
{
    TileMap map{kMapW, kMapH};
    for (int x = 0; x < kMapW; ++x) {
        map.Set(x, row, Tile::Wall);
    }
    for (int y = 0; y < kMapH; ++y) {
        map.Set(col, y, Tile::Wall);
    }
    return map;
}

bool NearPos(Vec2 v, float ex, float ey, float eps = 1e-5f)
{
    return ApproxEquals(v.x, ex, eps) && ApproxEquals(v.y, ey, eps);
}

} // namespace

// ------------------------------------------------------------
// 1~2. 没墙时正常推进；零位移是空操作
//     ⚠️ 这两条**不涉及墙**，所以模型改动不影响它们（连期望值都没动）—— 这是"改模型时
//        先看哪几条不用动"的好信号：不动的那几条说明它们测的是别的东西。
// ------------------------------------------------------------
MINI_TEST(Collision, MovesFreelyWhenNoWall)
{
    const TileMap map = EmptyMap();
    Vec2          pos{5.5f, 5.5f};

    MoveAndCollide(map, pos, Vec2{2.0f, 1.0f});

    MINI_CHECK(NearPos(pos, 7.5f, 6.5f));
}

MINI_TEST(Collision, ZeroDeltaIsNoOp)
{
    const TileMap map = EmptyMap();
    Vec2          pos{5.5f, 5.5f};

    MoveAndCollide(map, pos, Vec2{0.0f, 0.0f});

    MINI_CHECK(NearPos(pos, 5.5f, 5.5f));
}

// ------------------------------------------------------------
// 3~6. 四个方向撞墙 → 停在**格边界前 ε**
//      接触位置 = `格边界 ∓ kContactEpsilon`（旧版是"整格 ∓ 0.5"）
//      四条一起钉"镜像公式只写对一半"这个坑（施工图坑 17）
//      ⚠️ 反向验证 R2（夹紧改成"这一轴不动"）的目标就是这四条
// ------------------------------------------------------------
MINI_TEST(Collision, StopsAtWallMovingRight)
{
    const TileMap map = MapWithWall(kWallX, kWallY);
    Vec2          pos{9.0f, 5.5f};                 // 占格 (9,5)；墙格 (10,5) 在右边

    MoveAndCollide(map, pos, Vec2{5.0f, 0.0f});    // 够跨过墙 3 格

    MINI_CHECK(ApproxEquals(pos.x, 10.0f - kContactEpsilon));   // 停在墙格左边界前 ε
    MINI_CHECK(ApproxEquals(pos.y, 5.5f));                      // y 完全没动
    MINI_CHECK(!IsSolidAt(map, pos));                           // **接触位置一定不在实心格里**（约定 2）
}

MINI_TEST(Collision, StopsAtWallMovingLeft)
{
    const TileMap map = MapWithWall(kWallX, kWallY);
    Vec2          pos{12.0f, 5.5f};

    MoveAndCollide(map, pos, Vec2{-5.0f, 0.0f});

    MINI_CHECK(ApproxEquals(pos.x, 11.0f + kContactEpsilon));   // 停在墙格右边界后 ε
    MINI_CHECK(!IsSolidAt(map, pos));
}

MINI_TEST(Collision, StopsAtWallMovingDown)
{
    const TileMap map = MapWithWall(5, kWallX);    // 墙在 (5,10) —— 在下方
    Vec2          pos{5.5f, 9.0f};

    MoveAndCollide(map, pos, Vec2{0.0f, 5.0f});

    MINI_CHECK(ApproxEquals(pos.y, 10.0f - kContactEpsilon));
    MINI_CHECK(!IsSolidAt(map, pos));
}

MINI_TEST(Collision, StopsAtWallMovingUp)
{
    const TileMap map = MapWithWall(5, kWallX);    // 墙在 (5,10) —— 在上方
    Vec2          pos{5.5f, 12.0f};

    MoveAndCollide(map, pos, Vec2{0.0f, -5.0f});

    MINI_CHECK(ApproxEquals(pos.y, 11.0f + kContactEpsilon));
    MINI_CHECK(!IsSolidAt(map, pos));
}

// ------------------------------------------------------------
// 7~9. 新模型的**占格语义**（旧版三条"盒子压几格"的用例改写而来）
//      ⚠️ 反向验证 R4 的目标就是第 7 条（把 ε 去掉 / 改成"边界处也算实心"）
// ------------------------------------------------------------
MINI_TEST(Collision, OccupiesOnlyItsOwnTile)
{
    const TileMap map = MapWithWall(kWallX, kWallY);

    // 中心 9.5 → 占格 (9,5)：**只有这一格**说了算
    MINI_CHECK(!IsSolidAt(map, Vec2{9.5f, 5.5f}));
    MINI_CHECK(IsSolidAt(map, Vec2{10.5f, 5.5f}));    // 中心进了墙格 → 实心

    // ⚠️ **边界点属于实心那一格**（`ToIndex(10.0)` == 10）——
    //    这正是"停在边界前 ε"必须存在的原因：停在 10.0 就已经站在实心格里了。
    MINI_CHECK(IsSolidAt(map, Vec2{10.0f, 5.5f}));

    // 而 ε 之后的位置是安全的（用例 3 停在的就是这里）
    MINI_CHECK(!IsSolidAt(map, Vec2{10.0f - kContactEpsilon, 5.5f}));
}

MINI_TEST(Collision, BoundaryItselfIsNotAnObstacle)
{
    // ① 边界右边**是空**的 → 跨过去不受任何影响（旧模型里"盒子的边"会扫到墙）
    const TileMap farWall = MapWithWall(kWallX + 1, kWallY);   // 墙在 (11,5)，(10,5) 是空的
    Vec2          crossing{9.5f, 5.5f};
    MoveAndCollide(farWall, crossing, Vec2{5.0f, 0.0f});
    MINI_CHECK(ApproxEquals(crossing.x, 11.0f - kContactEpsilon));   // 自由穿过 x=10，停在下一堵墙前

    // ② 边界右边**有墙** → 就停在边界前 ε（挡的是**目标格**，不是"边界"本身）
    const TileMap nearWall = MapWithWall(kWallX, kWallY);      // 墙在 (10,5)
    Vec2          blocked{9.5f, 5.5f};
    MoveAndCollide(nearWall, blocked, Vec2{5.0f, 0.0f});
    MINI_CHECK(ApproxEquals(blocked.x, 10.0f - kContactEpsilon));
}

MINI_TEST(Collision, SlipsThroughDiagonalGap)
{
    // 一颗孤立的墙格 (10,5)：**站在它上面一行**时能直接从它上方走过去
    //（屏幕上看到的就是"从角边擦着过去"）。旧模型会被"盒子的角"挡住 —— 这是新模型
    // **有意要**的行为：看着能过，就能过。
    const TileMap map = MapWithWall(kWallX, kWallY);

    Vec2 above{9.5f, 4.5f};                    // 占格 (9,4) —— 与墙格 (10,5) 只是**对角**
    MoveAndCollide(map, above, Vec2{2.0f, 0.0f});
    MINI_CHECK(ApproxEquals(above.x, 11.5f));  // 一路走过去，没被拦

    // 对照组：**站进墙所在那一行**时，同一堵墙就挡得住
    Vec2 sameRow{9.5f, 5.5f};                  // 占格 (9,5)
    MoveAndCollide(map, sameRow, Vec2{2.0f, 0.0f});
    MINI_CHECK(ApproxEquals(sameRow.x, 10.0f - kContactEpsilon));
}

// ------------------------------------------------------------
// 10~11. 子步进：高速不穿墙 + 步长边界值
//        ⚠️ 反向验证 R1（kMaxStepPerTick 改成 1e9f）的目标就是第 10 条
// ------------------------------------------------------------
MINI_TEST(Collision, DoesNotTunnelThroughWallAtHighSpeed)
{
    const TileMap map = MapWithWall(kWallX, kWallY);
    Vec2          pos{8.0f, 5.5f};

    // 一帧推 +5 格：不分子步的话，只测终点（tile 13 是空的）→ 直接穿过这堵墙
    MoveAndCollide(map, pos, Vec2{5.0f, 0.0f});

    MINI_CHECK(ApproxEquals(pos.x, 10.0f - kContactEpsilon, 1e-4f));
    MINI_CHECK(pos.x < static_cast<float>(kWallX));   // 连"越过去再被推回来"都不许出现
}

MINI_TEST(Collision, DeltaAtMaxStepBoundary)
{
    const TileMap map = MapWithWall(kWallX, kWallY);

    // 略小于一个子步：还没碰到墙 → 原样前进（**切子步不能吃掉位移**）
    Vec2 justUnder{9.0f, 5.5f};
    MoveAndCollide(map, justUnder, Vec2{kMaxStepPerTick * 0.99f, 0.0f});
    MINI_CHECK(ApproxEquals(justUnder.x, 9.0f + kMaxStepPerTick * 0.99f, 1e-4f));
    MINI_CHECK(!IsSolidAt(map, justUnder));

    // 恰好一个子步
    Vec2 exactly{9.0f, 5.5f};
    MoveAndCollide(map, exactly, Vec2{kMaxStepPerTick, 0.0f});
    MINI_CHECK(ApproxEquals(exactly.x, 9.5f));
    MINI_CHECK(!IsSolidAt(map, exactly));

    // 略大于一个子步（被切成 2 段）：结果必须**一模一样**（还是走满 0.505，不是 0.5）
    Vec2 justOver{9.0f, 5.5f};
    MoveAndCollide(map, justOver, Vec2{kMaxStepPerTick * 1.01f, 0.0f});
    MINI_CHECK(ApproxEquals(justOver.x, 9.505f, 1e-4f));
    MINI_CHECK(justOver.x > 9.5f);                    // ⚠️ 这条钉的就是"子步进不许削位移"
    MINI_CHECK(!IsSolidAt(map, justOver));
}

// ------------------------------------------------------------
// 12~13. 分轴：一轴被夹住，另一轴照走；顶角两轴一起夹住
//        ⚠️ 反向验证 R3（换成"整体移动后检测并回退"）的目标就是第 12 条
// ------------------------------------------------------------
MINI_TEST(Collision, SlidesAlongWallOnOtherAxis)
{
    const TileMap map = MapWithColumn(kWallX);   // x=10 整列都是墙
    Vec2          pos{9.0f, 5.5f};

    MoveAndCollide(map, pos, Vec2{5.0f, 0.8f});

    MINI_CHECK(ApproxEquals(pos.x, 10.0f - kContactEpsilon));   // x 被墙夹住
    MINI_CHECK(ApproxEquals(pos.y, 6.3f, 1e-4f));               // y 照走满 0.8（这就是"贴墙滑行"）
    MINI_CHECK(!IsSolidAt(map, pos));

    // 再推一次同样的 delta：x 仍然不动、y 继续走 —— 贴墙滑行是**稳定**状态
    MoveAndCollide(map, pos, Vec2{5.0f, 0.8f});
    MINI_CHECK(ApproxEquals(pos.x, 10.0f - kContactEpsilon));
    MINI_CHECK(ApproxEquals(pos.y, 7.1f, 1e-4f));
}

MINI_TEST(Collision, CornerClampsBothAxes)
{
    const TileMap map = MapWithCross(kWallX, kCrossRow);   // 竖墙 x=10、横墙 y=10
    Vec2          pos{9.0f, 9.0f};

    MoveAndCollide(map, pos, Vec2{5.0f, 5.0f});

    MINI_CHECK(ApproxEquals(pos.x, 10.0f - kContactEpsilon));
    MINI_CHECK(ApproxEquals(pos.y, 10.0f - kContactEpsilon));

    // 顶在角上是**稳定状态**：同样再来一次，位置不动（不卡进墙，也不会被"弹"开）
    MoveAndCollide(map, pos, Vec2{5.0f, 5.0f});
    MINI_CHECK(ApproxEquals(pos.x, 10.0f - kContactEpsilon));
    MINI_CHECK(ApproxEquals(pos.y, 10.0f - kContactEpsilon));
    MINI_CHECK(!IsSolidAt(map, pos));
}

// ------------------------------------------------------------
// 14. 走不出地图（**免费**得到的性质，判据是"越界 = 实心"）
//     ⚠️ 期望值从"0.5 / W−0.5"变成"ε / W−ε"：新模型下能站到**最边上那一格**里
//        （tile 0 / tile W−1 都是真格子），只是进不去界外那格。
// ------------------------------------------------------------
MINI_TEST(Collision, CannotLeaveMapThroughEdge)
{
    const TileMap map = EmptyMap();   // 没有外沿墙：越界本身就是墙

    Vec2 left{5.5f, 5.5f};
    MoveAndCollide(map, left, Vec2{-100.0f, 0.0f});
    MINI_CHECK(ApproxEquals(left.x, kContactEpsilon));              // 占格 0，再往左是 (−1) = 墙
    MINI_CHECK(!IsSolidAt(map, left));

    Vec2 right{5.5f, 5.5f};
    MoveAndCollide(map, right, Vec2{100.0f, 0.0f});
    MINI_CHECK(ApproxEquals(right.x, static_cast<float>(kMapW) - kContactEpsilon));
    MINI_CHECK(!IsSolidAt(map, right));

    Vec2 up{5.5f, 5.5f};
    MoveAndCollide(map, up, Vec2{0.0f, -100.0f});
    MINI_CHECK(ApproxEquals(up.y, kContactEpsilon));
    MINI_CHECK(!IsSolidAt(map, up));

    Vec2 down{5.5f, 5.5f};
    MoveAndCollide(map, down, Vec2{0.0f, 100.0f});
    MINI_CHECK(ApproxEquals(down.y, static_cast<float>(kMapH) - kContactEpsilon));
    MINI_CHECK(!IsSolidAt(map, down));
}

// ------------------------------------------------------------
// 15. 起点就在实心格里 → **允许移动**（自愈，别冻死）
// ------------------------------------------------------------
MINI_TEST(Collision, AlreadyInsideSolidDoesNotFreeze)
{
    const TileMap map = MapWithWall(5, 5);
    Vec2          pos{5.5f, 5.5f};

    MINI_CHECK(IsSolidAt(map, pos));       // 前提：起点确实在墙格里

    MoveAndCollide(map, pos, Vec2{1.0f, 0.0f});

    MINI_CHECK(pos.x > 5.5f);              // 没被冻住
    MINI_CHECK(ApproxEquals(pos.x, 6.5f)); // 走出去了，而且走到的是"正常推进"的结果
    MINI_CHECK(!IsSolidAt(map, pos));
}

// ------------------------------------------------------------
// 16. 谓词与 TileMap::At 同源（含越界 = 实心）+ 位置重载与格子重载一致
// ------------------------------------------------------------
MINI_TEST(Collision, IsSolidAtMirrorsTileAt)
{
    const TileMap map = MapWithWall(3, 4);

    MINI_CHECK(IsSolidAt(map, 3, 4));
    MINI_CHECK(!IsSolidAt(map, 2, 4));
    MINI_CHECK(map.At(3, 4) == Tile::Wall);   // ⚠️ 枚举比较，不是浮点

    MINI_CHECK(IsSolidAt(map, -1, 0));        // 越界一律当墙
    MINI_CHECK(IsSolidAt(map, kMapW, 0));
    MINI_CHECK(IsSolidAt(map, 0, -1));
    MINI_CHECK(IsSolidAt(map, 0, kMapH));

    // 位置重载 == 它占的那一格的格子重载（两条路径不许漂移）
    MINI_CHECK(IsSolidAt(map, Vec2{3.5f, 4.5f}));
    MINI_CHECK(!IsSolidAt(map, Vec2{2.5f, 4.5f}));
}

// ------------------------------------------------------------
// 17. 纯函数：同输入两遍 → 结果相同（没有任何隐藏状态）
//     （同 Camera.FollowIsPureFunctionOfInputs 的套路）
// ------------------------------------------------------------
MINI_TEST(Collision, MoveAndCollideIsPureFunctionOfInputs)
{
    const TileMap map = MapWithCross(kWallX, kCrossRow);

    Vec2 first{9.0f, 9.0f};
    MoveAndCollide(map, first, Vec2{5.0f, 5.0f});
    Vec2 second{9.0f, 9.0f};
    MoveAndCollide(map, second, Vec2{5.0f, 5.0f});

    MINI_CHECK(NearPos(first, second.x, second.y));
}

// ============================================================
// Suite `Sweep` —— 线段采样（S9 步骤 1 加）
//
// ⚠️ 为什么**另开一个 suite**（而不是并进 `Collision`）：
//   · suite 名是 runner 的过滤单位 —— `mini_tests.exe Sweep` 只跑这 6 条；
//   · 这 6 条测的是**只读**的采样器，和 `MoveAndCollide`（唯一会改位置的函数）是两件事。
//
// ⚠️ 这里**刻意不测"撞没撞"**：采样器不回答那个问题（接口粒度，`S9_CHECKLIST` §6.1）——
//    它只保证"把线段摊成格点"这件事本身对。命中判定在 `tests/test_bullet.cpp` 里测。
// ============================================================

namespace {

// 沿线段的**每个**采样点问一句"这一格实心吗"（正是 `game/Bullet.h` 里墙判定要做的事，
// 只是这里用 Collision 自己的谓词）。
bool AnySampleSolid(const TileMap& map, Vec2 from, Vec2 to)
{
    const int steps = SweepStepCount(from, to);
    for (int i = 0; i <= steps; ++i) {
        if (IsSolidAt(map, SweepSampleAt(from, to, steps, i))) {
            return true;
        }
    }
    return false;
}

} // namespace

// ------------------------------------------------------------
// S1. 两端点都在采样点里（i = 0 与 i = steps）
//     ⚠️ 不是凑数：子弹的起点就是玩家那一格、终点是"这一帧要飞到的地方"，
//        端点算不算在内，直接决定"贴脸打"与"最后一步"算不算命中。
// ------------------------------------------------------------
MINI_TEST(Sweep, BothEndpointsAreSampled)
{
    const Vec2 from{2.5f, 3.5f};
    const Vec2 to{9.75f, 3.5f};
    const int  steps = SweepStepCount(from, to);

    MINI_CHECK(steps >= 1);

    const Vec2 first = SweepSampleAt(from, to, steps, 0);
    const Vec2 last  = SweepSampleAt(from, to, steps, steps);

    MINI_CHECK(NearPos(first, from.x, from.y));   // 起点**精确**是 from
    MINI_CHECK(NearPos(last, to.x, to.y));        // 终点**精确**是 to
}

// ------------------------------------------------------------
// S2. 退化输入：零长度、NaN、越界的 i、stepCount <= 0
//     ⚠️ 这些分支是**特意在实现里挡掉**的（`!(dist > k)` 那个写法 + 夹取）——
//        实现了却没人测的分支 = 没测的分支（S8 的教训：空断言比没有更坏）。
// ------------------------------------------------------------
MINI_TEST(Sweep, DegenerateInputsStillGiveOneStep)
{
    const Vec2 p{4.5f, 4.5f};

    // 零长度线段：1 步（**不是 0** —— 否则调用方要么 0 除、要么一个点都不问）
    MINI_CHECK_EQ(SweepStepCount(p, p), 1);

    // NaN：`ceil(NaN)` / `static_cast<int>(NaN)` 是 UB，必须在进那两行之前挡住
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const Vec2  nanFrom{nan, nan};
    const Vec2  nanTo{nan, 1.0f};
    MINI_CHECK_EQ(SweepStepCount(nanFrom, p), 1);
    MINI_CHECK_EQ(SweepStepCount(p, nanTo), 1);

    // 零长度线段的两个采样点都是它自己
    const Vec2 only = SweepSampleAt(p, p, 1, 0);
    MINI_CHECK(NearPos(only, p.x, p.y));
    const Vec2 onlyToo = SweepSampleAt(p, p, 1, 1);
    MINI_CHECK(NearPos(onlyToo, p.x, p.y));

    // 越界的 i 被夹进 [0, steps]：不崩、不指到线段之外
    const Vec2 from{0.5f, 0.5f};
    const Vec2 to{9.5f, 0.5f};
    MINI_CHECK(NearPos(SweepSampleAt(from, to, 18, -5), from.x, from.y));
    MINI_CHECK(NearPos(SweepSampleAt(from, to, 18, 999), to.x, to.y));

    // stepCount <= 0 → 当 1 步（防 0 除）
    MINI_CHECK(NearPos(SweepSampleAt(from, to, 0, 0), from.x, from.y));
}

// ------------------------------------------------------------
// S3. ★★ **性质**：相邻两个采样点的**格坐标**每轴最多差 1 ⇒ 跳不过任何 1 格宽的目标
//
//     ⚠️ 这是分叉 ②（"子弹不会穿靶"）的**唯一机器守卫** ——
//        采样器里**刻意没有 `static_assert`**（那会是 `kMaxStepPerTick < 1.0f` 的推论，
//        永远不会红），理由写在 `Collision.h` 采样段的注释里。
//     ⚠️ 期望值全部**从 `kMaxStepPerTick` 算**出来，不抄实现里的数：
//        把步长常量改大（比如 2 格）时，这条必须**跟着红**（反向验证 R1）。
//     ⚠️ 它**不**保证"线段擦过的每一格都被问到"：斜向线段从 (0,0) 直穿到 (1,1) 时，
//        (1,0)/(0,1) 只在**角点**上被碰到（测度为零）。这与点模型"对角缝能穿"是**同一个**
//        既定行为（`Collision.h` 约定 1 / `SlipsThroughDiagonalGap`），不是漏洞。
// ------------------------------------------------------------
MINI_TEST(Sweep, ConsecutiveSamplesNeverSkipATile)
{
    // 行 / 列 / 斜向 / 地图对角线（最后一条是最长可能线段：30√2 ≈ 42.4 格）
    const Vec2 kSegments[][2] = {
        {{0.25f, 0.25f}, {10.25f, 0.25f}},
        {{0.25f, 0.25f}, {0.25f, 10.25f}},
        {{0.25f, 0.25f}, {7.25f, 7.25f}},
        {{1.50f, 1.50f}, {31.50f, 31.50f}},
    };

    for (const auto& seg : kSegments) {
        const Vec2 from  = seg[0];
        const Vec2 to    = seg[1];
        const int  steps = SweepStepCount(from, to);

        MINI_CHECK(steps >= 1);

        // 步数够：每步的欧氏位移 ≤ kMaxStepPerTick（⇔ dist / steps ≤ kMaxStepPerTick）
        const float dist = (to - from).Length();
        MINI_CHECK(static_cast<float>(steps) * kMaxStepPerTick >= dist);

        int prevX = TileMap::ToIndex(from.x);
        int prevY = TileMap::ToIndex(from.y);

        for (int i = 1; i <= steps; ++i) {
            const Vec2 p = SweepSampleAt(from, to, steps, i);
            const int  x = TileMap::ToIndex(p.x);
            const int  y = TileMap::ToIndex(p.y);

            const int dx = x - prevX;
            const int dy = y - prevY;

            // ← 把步长换成 2 格时，这里会红（相邻采样点会跨 2 格）
            MINI_CHECK(dx >= -1 && dx <= 1);
            MINI_CHECK(dy >= -1 && dy <= 1);

            prevX = x;
            prevY = y;
        }

        // 最后一个采样点必须落在**终点那一格**（不能"停在终点前一格"）
        MINI_CHECK_EQ(prevX, TileMap::ToIndex(to.x));
        MINI_CHECK_EQ(prevY, TileMap::ToIndex(to.y));
    }
}

// ------------------------------------------------------------
// S4. 线段穿过实心格 → 采样点能发现（这就是"子弹撞墙"的那半个判据）
// ------------------------------------------------------------
MINI_TEST(Sweep, SegmentThroughBlockedTileIsDetected)
{
    const TileMap map = MapWithWall(kWallX, kWallY);   // 只有 (10,5) 一格是墙

    // 沿 y = 5 这一行从 x = 2.5 走到 18.5 → 必然经过 (10,5)
    MINI_CHECK(AnySampleSolid(map, Vec2{2.5f, 5.5f}, Vec2{18.5f, 5.5f}));

    // 起点自己就在实心格里（出生点摆错的情况）：也要能发现
    MINI_CHECK(AnySampleSolid(map, Vec2{10.5f, 5.5f}, Vec2{10.5f, 5.5f}));

    // 反向走（起点在墙后面、终点在墙前面）：也必须发现，不能"只看终点"
    MINI_CHECK(AnySampleSolid(map, Vec2{18.5f, 5.5f}, Vec2{2.5f, 5.5f}));
}

// ------------------------------------------------------------
// S5. 线段**贴着**实心格旁边走 → 不算命中
//     ⚠️ 防的是"用包围盒 / 把格子膨胀一圈"这种糊弄实现：那种实现会把
//        "紧贴墙飞过去"误判成撞墙（屏幕上看起来像"子弹凭空消失了"）。
// ------------------------------------------------------------
MINI_TEST(Sweep, SegmentBesideABlockedTileIsNotDetected)
{
    const TileMap map = MapWithWall(kWallX, kWallY);   // (10,5) 实心

    // 紧贴上面一行（y = 4.5 ⇒ 格行 4）与下面一行（y = 6.5 ⇒ 格行 6）
    MINI_CHECK(!AnySampleSolid(map, Vec2{2.5f, 4.5f}, Vec2{18.5f, 4.5f}));
    MINI_CHECK(!AnySampleSolid(map, Vec2{2.5f, 6.5f}, Vec2{18.5f, 6.5f}));

    // 走到墙**前面**就停：终点 x = 9.9 ⇒ 格列 9，还没进 (10,5)
    MINI_CHECK(!AnySampleSolid(map, Vec2{2.5f, 5.5f}, Vec2{9.9f, 5.5f}));
}

// ------------------------------------------------------------
// S6. 纯函数：同输入两遍 → 同结果
//     ⚠️ 对一个无状态函数来说这**天然成立**，所以它挡不住任何现在的 bug ——
//        它的作用是把"这一层不许有隐藏状态"写成机器可查的形式
//        （同 `Collision.MoveAndCollideIsPureFunctionOfInputs` 的定位）。
// ------------------------------------------------------------
MINI_TEST(Sweep, SweepIsPureFunctionOfInputs)
{
    const Vec2 from{3.25f, 7.75f};
    const Vec2 to{13.5f, 2.25f};

    const int firstSteps  = SweepStepCount(from, to);
    const int secondSteps = SweepStepCount(from, to);
    MINI_CHECK_EQ(firstSteps, secondSteps);

    // 逐个采样点：连着取两遍必须一样
    for (int i = 0; i <= firstSteps; ++i) {
        const Vec2 a = SweepSampleAt(from, to, firstSteps, i);
        const Vec2 b = SweepSampleAt(from, to, firstSteps, i);
        MINI_CHECK(NearPos(a, b.x, b.y));
    }

    // **倒着**再取一遍：取用顺序不影响任何一点的取值
    for (int i = firstSteps; i >= 0; --i) {
        const Vec2 backward = SweepSampleAt(from, to, firstSteps, i);
        const Vec2 forward  = SweepSampleAt(from, to, firstSteps, i);
        MINI_CHECK(NearPos(backward, forward.x, forward.y));
    }
}
