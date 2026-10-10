#include "MiniTest.h"

#include <game/Bullet.h>

#include <mini/math/MathUtils.h>
#include <mini/world/TileMap.h>

#include <cstring>   // std::strlen / std::strcmp（字形表那两条）
#include <limits>    // std::numeric_limits<float>::quiet_NaN()
#include <span>      // 空敌人表那个边界

// ------------------------------------------------------------
// game/Bullet 的单元测试（S9 步骤 2）
//
// ⚠️ 这一层**完全离线**：它不认识 `Entity` / `EnemyData` / `Game` / 输入 / 屏幕 ——
//    地图自己铺、敌人用"一串位置"（`span<const Vec2>`）表示。这是分叉 ① 的接口决定换来的：
//    `FirstEnemyOnSegment` 收位置而不是实体指针，所以不需要世界、不需要窗口。
//
// ⚠️ 四条写用例时的纪律（沿用 `test_collision.cpp` 的）：
//   · `Vec2` **没有 `operator<<`** ⇒ 不能塞进 `MINI_CHECK_EQ`；
//   · 枚举**也不能**塞 `MINI_CHECK_EQ`（要 `operator<<`）⇒ 一律 `MINI_CHECK(a == b)`；
//   · 浮点不写 `==` ⇒ `ApproxEquals`（默认 eps = 1e-5）；
//   · 期望值**从被钉住的表/常量算**（如 `kFacingGlyphs`、`Vec2::Normalized()`），不抄实现里的字面量。
// ------------------------------------------------------------

namespace {

using game::BulletHit;
using game::DirectionToFacing;
using game::Facing8;
using game::FacingGlyph;
using game::FacingToVec2;
using game::FirstEnemyOnSegment;
using game::kFacingCount;
using game::kFacingGlyphs;
using mini::math::ApproxEquals;
using mini::math::Vec2;
using mini::world::Tile;
using mini::world::TileMap;

constexpr int kMapW = 24;
constexpr int kMapH = 24;

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

bool Near(Vec2 v, float ex, float ey, float eps = 1e-5f)
{
    return ApproxEquals(v.x, ex, eps) && ApproxEquals(v.y, ey, eps);
}

// 8 向的**标准**输入与它该推出的朝向。
// ⚠️ 世界 Y 轴向下（`Entity.h`）⇒ "北" = 屏幕上 = **−Y**。
struct FacingCase {
    float   dx;
    float   dy;
    Facing8 expected;
};

constexpr FacingCase kAllEight[] = {
    { 1.0f,  0.0f, Facing8::East},
    { 1.0f, -1.0f, Facing8::NorthEast},
    { 0.0f, -1.0f, Facing8::North},
    {-1.0f, -1.0f, Facing8::NorthWest},
    {-1.0f,  0.0f, Facing8::West},
    {-1.0f,  1.0f, Facing8::SouthWest},
    { 0.0f,  1.0f, Facing8::South},
    { 1.0f,  1.0f, Facing8::SouthEast},
};

} // namespace

// ------------------------------------------------------------
// B1. 8 个方向都映射对，且**归一化前后一样**（`Game` 的 ③ 会先归一化）
// ------------------------------------------------------------
MINI_TEST(Bullet, DirectionToFacingMapsAllEightWays)
{
    for (const FacingCase& c : kAllEight) {
        Facing8 out = Facing8::South;   // 先塞一个**错的**值，确认函数真的写了它
        const bool ok = DirectionToFacing(Vec2{c.dx, c.dy}, out);
        MINI_CHECK(ok);
        MINI_CHECK(out == c.expected);
    }

    // 归一化过再传（Game 传给这个函数的就是归一化后的方向）
    for (const FacingCase& c : kAllEight) {
        Facing8 out = Facing8::South;
        const Vec2 normalized = Vec2{c.dx, c.dy}.Normalized();
        MINI_CHECK(DirectionToFacing(normalized, out));
        MINI_CHECK(out == c.expected);
    }

    // ⚠️ 判据是"**最近的** 8 向"（±22.5° 边界），不是"两个分量都非零就算斜向"：
    //    (3,1) 离东只有 18.4° ⇒ 必须是东（"两分量都非零 ⇒ 斜向"那种写法会给东北，错）
    Facing8 out = Facing8::South;
    MINI_CHECK(DirectionToFacing(Vec2{3.0f, 1.0f}, out));
    MINI_CHECK(out == Facing8::East);

    MINI_CHECK(DirectionToFacing(Vec2{1.0f, 3.0f}, out));
    MINI_CHECK(out == Facing8::South);

    MINI_CHECK(DirectionToFacing(Vec2{-4.0f, 1.0f}, out));
    MINI_CHECK(out == Facing8::West);

    MINI_CHECK(DirectionToFacing(Vec2{1.0f, -4.0f}, out));
    MINI_CHECK(out == Facing8::North);

    // 正好 45°：必须是斜向（两个分量等长）
    MINI_CHECK(DirectionToFacing(Vec2{2.0f, 2.0f}, out));
    MINI_CHECK(out == Facing8::SouthEast);
}

// ------------------------------------------------------------
// B2. 零向量 / NaN ⇒ `false`，而且**不许改写调用方的朝向**
//     ⚠️ 这就是分叉 ③ "停下沿用上次朝向"的全部实现 —— 如果这里偷偷写了个默认方向，
//        玩家站着不动开火时会朝一个他自己没选的方向打。
// ------------------------------------------------------------
MINI_TEST(Bullet, ZeroDirectionKeepsPreviousFacing)
{
    Facing8 facing = Facing8::NorthWest;   // 调用方"上一次的朝向"

    MINI_CHECK(!DirectionToFacing(Vec2{0.0f, 0.0f}, facing));
    MINI_CHECK(facing == Facing8::NorthWest);   // 没被改写

    const float nan = std::numeric_limits<float>::quiet_NaN();
    MINI_CHECK(!DirectionToFacing(Vec2{nan, nan}, facing));
    MINI_CHECK(facing == Facing8::NorthWest);

    // 只有一个分量是 NaN：也不许猜出某个方向
    MINI_CHECK(!DirectionToFacing(Vec2{nan, 0.0f}, facing));
    MINI_CHECK(facing == Facing8::NorthWest);
    MINI_CHECK(!DirectionToFacing(Vec2{0.0f, nan}, facing));
    MINI_CHECK(facing == Facing8::NorthWest);
}

// ------------------------------------------------------------
// B3. `FacingToVec2` 是单位向量，且指向与枚举名一致
// ------------------------------------------------------------
MINI_TEST(Bullet, FacingToVec2IsUnitLengthAndPointsRight)
{
    for (const FacingCase& c : kAllEight) {
        const Vec2 v = FacingToVec2(c.expected);

        MINI_CHECK_NEAR(v.Length(), 1.0f, 1e-5f);   // 单位长度（斜向不能是 √2 长度）

        // 指向：与"把用例表里那个向量归一化"的结果一致（期望值来自表，不抄实现）
        const Vec2 want = Vec2{c.dx, c.dy}.Normalized();
        MINI_CHECK(Near(v, want.x, want.y));
    }

    // 读起来最直观的两条：北在屏幕上是 −Y，东是 +X
    MINI_CHECK(FacingToVec2(Facing8::North).y < 0.0f);
    MINI_CHECK(FacingToVec2(Facing8::East).x > 0.0f);

    // 越界枚举（只能由强制转换造出来）：兜底是**东**，不是零向量 ——
    // 零向量会造出一颗"原地不动、永远赖在屏幕上"的子弹，比方向猜错更糟。
    const Vec2 fallback = FacingToVec2(static_cast<Facing8>(200));
    MINI_CHECK(Near(fallback, 1.0f, 0.0f));
}

// ------------------------------------------------------------
// B4. HUD 字形：8 项、每项 **2 字符**、**全 ASCII**
//     ⚠️ 定宽 2 是为了 HUD 后面的列不左右跳；全 ASCII 是因为传统控制台是代码页 936
//        （S8 的坑表记过：`→` 这类会乱码）。用例**只钉宽度与字符集**，不钉具体字形 ——
//        "目测效果"不满意时只改 `kFacingGlyphs`，不用动用例。
// ------------------------------------------------------------
MINI_TEST(Bullet, FacingGlyphsAreTwoCharsAndAscii)
{
    for (std::size_t i = 0; i < kFacingCount; ++i) {
        const char* g = FacingGlyph(static_cast<Facing8>(i));

        MINI_CHECK_EQ(static_cast<int>(std::strlen(g)), 2);
        for (int k = 0; k < 2; ++k) {
            const unsigned char ch = static_cast<unsigned char>(g[k]);
            MINI_CHECK(ch >= 0x20u && ch <= 0x7Eu);   // 可打印 ASCII
        }

        // 函数与表必须一致（防止有人只改一处）
        MINI_CHECK(std::strcmp(g, kFacingGlyphs[i]) == 0);
    }

    // 兜底也必须是 2 字符，否则越界时 HUD 会跳一格
    MINI_CHECK_EQ(static_cast<int>(std::strlen(FacingGlyph(static_cast<Facing8>(200)))), 2);
}

// ------------------------------------------------------------
// B5. ★★ **防穿靶**：起终点都在敌人格**之外**，但线段穿过它 ⇒ 必须命中
//     ⚠️ 这是分叉 ② 要挡的那个失败：一帧位移 ≥ 1 格（dt 上限 0.05s × 45 格/秒 ≈ 2.25 格），
//        只看终点会整条丢掉这次命中。
// ------------------------------------------------------------
MINI_TEST(Bullet, FirstEnemyOnSegmentHitsWhenBothEndsOutside)
{
    const TileMap map = EmptyMap();
    const Vec2    enemies[] = {Vec2{5.5f, 5.5f}};   // 占 (5,5) 这一格

    // 横向：一帧飞 2.2 格。起点在格 4、终点在格 6 —— 两端都不是敌人那一格。
    const BulletHit hit = FirstEnemyOnSegment(map, Vec2{4.2f, 5.5f}, Vec2{6.4f, 5.5f}, enemies);
    MINI_CHECK(!hit.blocked);
    MINI_CHECK_EQ(hit.enemyIndex, 0);

    // 反向（从右往左）：采样不许依赖方向符号
    const BulletHit back = FirstEnemyOnSegment(map, Vec2{6.4f, 5.5f}, Vec2{4.2f, 5.5f}, enemies);
    MINI_CHECK(!back.blocked);
    MINI_CHECK_EQ(back.enemyIndex, 0);

    // 斜向穿过同一格
    const BulletHit diagonal = FirstEnemyOnSegment(map, Vec2{2.5f, 2.5f}, Vec2{8.5f, 8.5f}, enemies);
    MINI_CHECK_EQ(diagonal.enemyIndex, 0);

    // 终点**正好落在**敌人那一格（贴脸打）：也要算命中（端点含在内）
    const BulletHit pointblank = FirstEnemyOnSegment(map, Vec2{5.5f, 5.5f}, Vec2{5.5f, 5.5f}, enemies);
    MINI_CHECK_EQ(pointblank.enemyIndex, 0);
}

// ------------------------------------------------------------
// B6. 线段**旁边**的敌人不算命中（防"用距离/包围盒糊弄"）
// ------------------------------------------------------------
MINI_TEST(Bullet, FirstEnemyOnSegmentIgnoresEnemiesOffTheLine)
{
    const TileMap map = EmptyMap();

    // 紧贴下面一格 / 上面一格：中心距只有 1.0 格 ⇒ 用"距离 < 1 格也算"的实现会误命中
    const Vec2 below[] = {Vec2{5.5f, 6.5f}};
    const Vec2 above[] = {Vec2{5.5f, 4.5f}};

    const BulletHit h1 = FirstEnemyOnSegment(map, Vec2{2.5f, 5.5f}, Vec2{9.5f, 5.5f}, below);
    MINI_CHECK(!h1.blocked);
    MINI_CHECK_EQ(h1.enemyIndex, -1);

    const BulletHit h2 = FirstEnemyOnSegment(map, Vec2{2.5f, 5.5f}, Vec2{9.5f, 5.5f}, above);
    MINI_CHECK_EQ(h2.enemyIndex, -1);

    // 在**延长线**上、但不在线段里 ⇒ 不算（不许拿"整条直线"或"直线距离"当判据）
    const Vec2 beyond[] = {Vec2{20.5f, 5.5f}};
    const BulletHit h3 = FirstEnemyOnSegment(map, Vec2{2.5f, 5.5f}, Vec2{9.5f, 5.5f}, beyond);
    MINI_CHECK_EQ(h3.enemyIndex, -1);

    // 空敌人表：0 个敌人也不能越界读（分叉 ① 的"自愈表"以后真的会瞬时为空）
    const BulletHit h4 = FirstEnemyOnSegment(map, Vec2{2.5f, 5.5f}, Vec2{9.5f, 5.5f},
                                             std::span<const Vec2>{});
    MINI_CHECK_EQ(h4.enemyIndex, -1);
    MINI_CHECK(!h4.blocked);
}

// ------------------------------------------------------------
// B7. ★ **墙优先**：墙后面的敌人不许被命中；而"从墙后面朝墙打"时，
//        先碰到的若真是敌人，就该报命中（同一条线段，方向不同 ⇒ 结果不同）
// ------------------------------------------------------------
MINI_TEST(Bullet, WallBlocksEnemyBehindIt)
{
    const TileMap map     = MapWithWall(7, 5);        // 墙在 (7,5)
    const Vec2    enemies[] = {Vec2{9.5f, 5.5f}};     // 敌人在墙**后面**（格 9）

    // 从左边打：先撞墙 ⇒ blocked，而且**不报命中**
    const BulletHit hit = FirstEnemyOnSegment(map, Vec2{2.5f, 5.5f}, Vec2{12.5f, 5.5f}, enemies);
    MINI_CHECK(hit.blocked);
    MINI_CHECK_EQ(hit.enemyIndex, -1);

    // 从右边（墙后面）朝墙打：先碰到的其实是**敌人**（它在墙之前）⇒ 报命中，不是撞墙
    const BulletHit back = FirstEnemyOnSegment(map, Vec2{12.5f, 5.5f}, Vec2{2.5f, 5.5f}, enemies);
    MINI_CHECK(!back.blocked);
    MINI_CHECK_EQ(back.enemyIndex, 0);

    // 起点就在墙里（出生点摆错）：起点那一点就实心 ⇒ 立即报 blocked
    const BulletHit inside = FirstEnemyOnSegment(map, Vec2{7.5f, 5.5f}, Vec2{12.5f, 5.5f}, enemies);
    MINI_CHECK(inside.blocked);
    MINI_CHECK_EQ(inside.enemyIndex, -1);
}

// ------------------------------------------------------------
// B8. 多个敌人时返回**沿线段先碰到的**那个，而不是"数组里排前面的"那个
// ------------------------------------------------------------
MINI_TEST(Bullet, ReturnsTheFirstEnemyAlongTheSegment)
{
    const TileMap map = EmptyMap();

    // ⚠️ 故意把**远的**放在下标 0：返回"数组里第一个命中的"实现会给 0（错），
    //    正确结果是沿线段先碰到的那个 = 下标 1。
    const Vec2 enemies[] = {
        Vec2{8.5f, 5.5f},   // 远（下标 0）
        Vec2{4.5f, 5.5f},   // 近（下标 1）
    };

    const BulletHit hit = FirstEnemyOnSegment(map, Vec2{2.5f, 5.5f}, Vec2{10.5f, 5.5f}, enemies);
    MINI_CHECK(!hit.blocked);
    MINI_CHECK_EQ(hit.enemyIndex, 1);

    // 从另一边打过来 ⇒ 先碰到的是"远"的那个（下标 0）
    const BulletHit back = FirstEnemyOnSegment(map, Vec2{10.5f, 5.5f}, Vec2{2.5f, 5.5f}, enemies);
    MINI_CHECK(!back.blocked);
    MINI_CHECK_EQ(back.enemyIndex, 0);
}
