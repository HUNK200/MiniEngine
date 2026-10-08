#include "MiniTest.h"

#include <mini/math/MathUtils.h>
#include <mini/world/Collision.h>

// ------------------------------------------------------------
// world/Collision 的单元测试（S7 步骤 1，17 个用例）
//
// 这个文件**不需要终端**：Collision 不读输入、不画东西、不认识 Entity —— 整层都能离线验证
// （S2/S3 起"能离线测的就离线测"的延续）。
//
// ⚠️ 地图只有 32×32，而且是**全空**的：用例自己用 `Set` 铺那几格墙。
//   · 这样每个用例的"墙在哪"在用例里看得见，不用去猜 demo 地图的布局。
//   · 边界用例（14）**故意不铺外沿墙**：越界本身就是墙（TileMap.h 约定 1）——
//     而且 `TileMap::Set` **越界是静默忽略的**（TileMap.h:59），想在界外铺墙是写不进去的（坑 9）。
//
// ⚠️ 三条写用例时的纪律（前几个阶段踩出来的）：
//   · `Vec2` **没有 `operator<<`**（Vec2.h 刻意不提供）→ 不能塞进 `MINI_CHECK_EQ`，
//     逐分量用 `MINI_CHECK_NEAR` / 本文件的 `NearPos`。
//   · 浮点比较**不写 `==`**（MathUtils.h 的规矩）→ 一律 `ApproxEquals`（默认 eps = 1e-5）。
//   · 接触面是"整格 ∓ 0.5"这种**精确半数**，所以期望值可以**写死**（如 9.5）；
//     这与"不许写死速度算出来的格数"（test_game.cpp 的纪律）不冲突：那个数依赖速度，这个数不依赖。
// ------------------------------------------------------------

namespace {

using mini::math::ApproxEquals;
using mini::math::Vec2;
using mini::world::IsSolidAt;
using mini::world::kEntityHalfSize;
using mini::world::kMaxStepPerTick;
using mini::world::MoveAndCollide;
using mini::world::OverlapsSolid;
using mini::world::Tile;
using mini::world::TileMap;

constexpr int kMapW = 32;
constexpr int kMapH = 32;

// 用例里反复出现的那几格：墙格、以及"十字墙"的列与行
constexpr int kWallX = 10;
constexpr int kWallY = 5;
constexpr int kCrossRow = 10;

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
// 3~6. 四个方向撞墙 → 停在接触面（**精确半数**，可以写死）
//      四条一起钉"镜像公式只写对一半"这个坑（施工图坑 17）
// ------------------------------------------------------------
MINI_TEST(Collision, StopsAtWallMovingRight)
{
    const TileMap map = MapWithWall(kWallX, kWallY);
    Vec2          pos{9.0f, 5.5f};          // 盒 [8.5, 9.5)，墙格 [10, 11)

    MoveAndCollide(map, pos, Vec2{5.0f, 0.0f});   // 够跨过墙 3 格

    MINI_CHECK(ApproxEquals(pos.x, 9.5f));        // 10 − kEntityHalfSize
    MINI_CHECK(ApproxEquals(pos.y, 5.5f));        // y 完全没动
    MINI_CHECK(!OverlapsSolid(map, pos));         // 停在接触面上不算重叠（约定 2）
}

MINI_TEST(Collision, StopsAtWallMovingLeft)
{
    const TileMap map = MapWithWall(kWallX, kWallY);
    Vec2          pos{12.0f, 5.5f};

    MoveAndCollide(map, pos, Vec2{-5.0f, 0.0f});

    MINI_CHECK(ApproxEquals(pos.x, 11.5f));       // 10 + 1 + kEntityHalfSize
    MINI_CHECK(!OverlapsSolid(map, pos));
}

MINI_TEST(Collision, StopsAtWallMovingDown)
{
    const TileMap map = MapWithWall(5, kWallX);   // 墙格换成"行"上的那一格
    Vec2          pos{5.5f, 9.0f};

    MoveAndCollide(map, pos, Vec2{0.0f, 5.0f});

    MINI_CHECK(ApproxEquals(pos.y, 9.5f));
    MINI_CHECK(!OverlapsSolid(map, pos));
}

MINI_TEST(Collision, StopsAtWallMovingUp)
{
    const TileMap map = MapWithWall(5, kWallX);
    Vec2          pos{5.5f, 12.0f};

    MoveAndCollide(map, pos, Vec2{0.0f, -5.0f});

    MINI_CHECK(ApproxEquals(pos.y, 11.5f));
    MINI_CHECK(!OverlapsSolid(map, pos));
}

// ------------------------------------------------------------
// 7~9. 重叠判据的边界语义（开区间）+ 单轴到底压几格
//      ⚠️ 反向验证 R4（把 `<` 改成 `<=`）的目标就是第 7 条
// ------------------------------------------------------------
MINI_TEST(Collision, AlignedTouchIsNotOverlap)
{
    const TileMap map = MapWithWall(kWallX, kWallY);

    // 盒 [9, 10) 与墙格 [10, 11) 只贴边 → **不算重叠**（停在这里是合法的）
    MINI_CHECK(!OverlapsSolid(map, Vec2{9.5f, 5.5f}));

    // 站在墙格上（中心 10.5 → 盒 [10, 11)）→ 就是重叠
    MINI_CHECK(OverlapsSolid(map, Vec2{10.5f, 5.5f}));
}

MINI_TEST(Collision, TileRangeAtTileCenter)
{
    // 中心 9.5 → 盒恰好 [9, 10) → 只压**一格**（tx = 9）。
    // 观察方式：墙铺在哪一格会挡住我。
    MINI_CHECK(OverlapsSolid(MapWithWall(9, kWallY), Vec2{9.5f, 5.5f}));    // 同格 → 挡
    MINI_CHECK(!OverlapsSolid(MapWithWall(8, kWallY), Vec2{9.5f, 5.5f}));   // 左边一格只贴边 → 不挡
}

MINI_TEST(Collision, TileRangeAtFractionalCenter)
{
    // 中心 9.0 → 盒 [8.5, 9.5) → 压**两格**（8 和 9）：两格里的墙都会挡住
    MINI_CHECK(OverlapsSolid(MapWithWall(8, kWallY), Vec2{9.0f, 5.5f}));
    MINI_CHECK(OverlapsSolid(MapWithWall(9, kWallY), Vec2{9.0f, 5.5f}));
    MINI_CHECK(!OverlapsSolid(MapWithWall(10, kWallY), Vec2{9.0f, 5.5f}));

    // 两个轴都非整格中心 → **常态是 4 格**（施工图 6.7）：对角上的墙也要挡住
    MINI_CHECK(OverlapsSolid(MapWithWall(8, 8), Vec2{9.0f, 9.0f}));
    MINI_CHECK(OverlapsSolid(MapWithWall(9, 9), Vec2{9.0f, 9.0f}));
}

// ------------------------------------------------------------
// 10~11. 子步进：高速不穿墙 + 步长边界值
//        ⚠️ 反向验证 R1（kMaxStepPerTick 改成 1e9f）的目标就是第 10 条
// ------------------------------------------------------------
MINI_TEST(Collision, DoesNotTunnelThroughWallAtHighSpeed)
{
    const TileMap map = MapWithWall(kWallX, kWallY);
    Vec2          pos{8.0f, 5.5f};

    // 一帧推 +5 格：不分子步的话，"移动前"和"移动后"两个采样点都干净 → 直接穿过墙（在 13.0 停下）
    MoveAndCollide(map, pos, Vec2{5.0f, 0.0f});

    MINI_CHECK(ApproxEquals(pos.x, 9.5f));
    MINI_CHECK(pos.x < static_cast<float>(kWallX));   // 连"越过去再被推回来"都不许出现
}

MINI_TEST(Collision, DeltaAtMaxStepBoundary)
{
    const TileMap map = MapWithWall(kWallX, kWallY);

    // 略小于一个子步：还没碰到墙 → 不该被夹紧（位置 = 起点 + delta）
    Vec2 justUnder{9.0f, 5.5f};
    MoveAndCollide(map, justUnder, Vec2{kMaxStepPerTick * 0.99f, 0.0f});
    MINI_CHECK(ApproxEquals(justUnder.x, 9.0f + kMaxStepPerTick * 0.99f, 1e-4f));
    MINI_CHECK(justUnder.x <= 9.5f);

    // 恰好一个子步：正好落在接触面上
    Vec2 exactly{9.0f, 5.5f};
    MoveAndCollide(map, exactly, Vec2{kMaxStepPerTick, 0.0f});
    MINI_CHECK(ApproxEquals(exactly.x, 9.5f));

    // 略大于一个子步（会被切成 2 段）：必须**夹紧**，不能越过墙
    Vec2 justOver{9.0f, 5.5f};
    MoveAndCollide(map, justOver, Vec2{kMaxStepPerTick * 1.01f, 0.0f});
    MINI_CHECK(ApproxEquals(justOver.x, 9.5f));
    MINI_CHECK(justOver.x < static_cast<float>(kWallX));
}

// ------------------------------------------------------------
// 12~13. 分轴：一轴被夹住，另一轴照走；顶角两轴一起夹住
//        ⚠️ 反向验证 R3（换成"整体移动后检测并回退"）的目标就是第 12 条
// ------------------------------------------------------------
MINI_TEST(Collision, SlidesAlongWallOnOtherAxis)
{
    const TileMap map = MapWithWall(kWallX, kWallY);
    Vec2          pos{9.0f, 5.5f};

    MoveAndCollide(map, pos, Vec2{5.0f, 0.8f});

    MINI_CHECK(ApproxEquals(pos.x, 9.5f));            // x 被墙夹住
    MINI_CHECK(ApproxEquals(pos.y, 6.3f, 1e-4f));     // y 照走满 0.8（这就是"贴墙滑行"）
    MINI_CHECK(!OverlapsSolid(map, pos));
}

MINI_TEST(Collision, CornerClampsBothAxes)
{
    const TileMap map = MapWithCross(kWallX, kCrossRow);   // 竖墙 x=10、横墙 y=10
    Vec2          pos{9.0f, 9.0f};

    MoveAndCollide(map, pos, Vec2{5.0f, 5.0f});

    MINI_CHECK(ApproxEquals(pos.x, 9.5f));
    MINI_CHECK(ApproxEquals(pos.y, 9.5f));

    // 顶在角上是**稳定状态**：同样再来一次，位置不动（不卡进墙，也不会被"弹"开）
    MoveAndCollide(map, pos, Vec2{5.0f, 5.0f});
    MINI_CHECK(ApproxEquals(pos.x, 9.5f));
    MINI_CHECK(ApproxEquals(pos.y, 9.5f));
    MINI_CHECK(!OverlapsSolid(map, pos));
}

// ------------------------------------------------------------
// 14. 走不出地图（**免费**得到的性质，判据是"越界 = 实心"）
// ------------------------------------------------------------
MINI_TEST(Collision, CannotLeaveMapThroughEdge)
{
    const TileMap map = EmptyMap();   // 没有外沿墙：越界本身就是墙

    Vec2 left{5.5f, 5.5f};
    MoveAndCollide(map, left, Vec2{-100.0f, 0.0f});
    MINI_CHECK(ApproxEquals(left.x, 0.5f));       // 盒 [0, 1) → 左边那格（tx = −1）是墙

    Vec2 right{5.5f, 5.5f};
    MoveAndCollide(map, right, Vec2{100.0f, 0.0f});
    MINI_CHECK(ApproxEquals(right.x, static_cast<float>(kMapW) - kEntityHalfSize));

    Vec2 up{5.5f, 5.5f};
    MoveAndCollide(map, up, Vec2{0.0f, -100.0f});
    MINI_CHECK(ApproxEquals(up.y, 0.5f));

    Vec2 down{5.5f, 5.5f};
    MoveAndCollide(map, down, Vec2{0.0f, 100.0f});
    MINI_CHECK(ApproxEquals(down.y, static_cast<float>(kMapH) - kEntityHalfSize));
}

// ------------------------------------------------------------
// 15. 起点就嵌在墙里 → **允许移动**（自愈，别冻死）
// ------------------------------------------------------------
MINI_TEST(Collision, AlreadyOverlappingDoesNotFreeze)
{
    const TileMap map = MapWithWall(5, 5);
    Vec2          pos{5.5f, 5.5f};

    MINI_CHECK(OverlapsSolid(map, pos));   // 前提：起点确实嵌着

    MoveAndCollide(map, pos, Vec2{1.0f, 0.0f});

    MINI_CHECK(pos.x > 5.5f);              // 没被冻住
    MINI_CHECK(ApproxEquals(pos.x, 6.5f));
}

// ------------------------------------------------------------
// 16. 谓词与 TileMap::At 同源（含越界 = 实心）
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
