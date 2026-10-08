#include "MiniTest.h"

#include <mini/world/Entity.h>
#include <mini/world/TileMap.h>
#include <mini/world/World.h>

#include <cstddef>
#include <stdexcept>

// ------------------------------------------------------------
// world/ 的单元测试（S5 步骤 1：EntityId / Transform / Velocity / Entity / TileMap）
//
// 这个文件**不需要终端**：world 既不读输入（不认识 platform）也不画东西
// （不认识 render），所以整层都能离线验证 —— 这是 S2/S3 那条
// "能离线测的就离线测"的延续（TECH_DEBT C7）。
//
// ⚠️ 步骤 2 会往**本文件**继续追加 World 的 10 个用例（同一个 suite 文件；
//    用例是按 suite 名过滤的，拆文件只会多一行 CMake）。
// ------------------------------------------------------------

namespace {

using mini::math::Vec2;
using mini::world::Entity;
using mini::world::EntityId;
using mini::world::Tile;
using mini::world::TileMap;
using mini::world::World;

// 编译期自检（S4 立下的手法）：ToIndex 声明了 constexpr，就在**编译期**真的调它。
// 一旦有人把内部实现换成 std::floor（C++20 里不是 constexpr），这里立刻编不过 ——
// 比"运行时某条用例变红"早一步，也说明 constexpr 是承诺而不是装饰。
static_assert(TileMap::ToIndex(-0.1f) == -1, "负数必须 floor：-0.1 属于第 -1 格");
static_assert(TileMap::ToIndex(-1.0f) == -1);
static_assert(TileMap::ToIndex(0.9f) == 0);
static_assert(TileMap::ToIndex(3.0f) == 3);

// ⚠️ **会移动**的用例不能用 4×3 的小地图（S7 起会红）。
//
//    `World::Update` 现在走碰撞，而 `TileMap::At` 越界一律返回 `Wall`（TileMap.h 约定 1）
//    → 12 格/秒 跑 1 秒要 12 格，4 宽的地图会把盒子夹在 x = 3.5
//    （2026-10-08 实测：`MovementIsDtIndependent` 期望 12.5、实际 3.5）。
//
//    宽度下限：0.5 + 12 = 12.5 → 盒 [12, 13) → 宽必须 > 13；取 **16**，留 3 格余量。
//    y 方向速度为 0，但仍留 3 行（盒高 1 格 + 余量）。
//
//    ⚠️ **不移动**的用例继续用 4×3 —— 问题不是"地图小"，而是"移动撞上边界"；
//       而且 `FindFindsSpawnedEntity` 还断言了 `Map().Width() == 4`，不能跟着一起改。
constexpr int kMoveMapW = 16;
constexpr int kMoveMapH = 3;

// 项目里第一条"断言抛异常"的用例（S0~S4 的构造函数都没测过这条路径）。
// 为什么写成函数模板：MINI_CHECK 收的是**表达式**，塞不进 try/catch。
//
// ⚠️ 两个 catch 都要写：只捕获 invalid_argument 的话，抛别的异常会直接逃出
//    用例函数 → 被 runner 当成"崩溃"而不是"断言失败"，报错位置会指丢。
template <typename Fn>
bool ThrowsInvalidArgument(Fn fn)
{
    try {
        fn();
    } catch (const std::invalid_argument&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

// 非 Empty 的格子数：断言里给数字，失败时一眼看出"多了/少了哪一格"
int CountNotEmpty(const TileMap& map)
{
    int count = 0;
    for (int ty = 0; ty < map.Height(); ++ty) {
        for (int tx = 0; tx < map.Width(); ++tx) {
            if (map.At(tx, ty) != Tile::Empty) {
                ++count;
            }
        }
    }
    return count;
}

} // namespace

// 构造后每格都是 Empty：默认参数 fill 要真的生效（别把 vector 的"值初始化"当默认值）
MINI_TEST(TileMap, DefaultIsEmpty)
{
    const TileMap map(4, 3);

    MINI_CHECK_EQ(map.Width(), 4);
    MINI_CHECK_EQ(map.Height(), 3);

    for (int ty = 0; ty < map.Height(); ++ty) {
        for (int tx = 0; tx < map.Width(); ++tx) {
            MINI_CHECK(map.At(tx, ty) == Tile::Empty);
        }
    }
}

// Set 之后 At 读回同一个值，且**只动那一格**（多写了一格 = 索引算错）
MINI_TEST(TileMap, SetAndAtRoundTrip)
{
    TileMap map(4, 3);
    map.Set(2, 1, Tile::Wall);

    MINI_CHECK(map.At(2, 1) == Tile::Wall);
    MINI_CHECK_EQ(CountNotEmpty(map), 1);

    // 上下左右各一格都必须没被牵连；(2, 0) 是最关键的那个 ——
    // 行/列写反（ty/tx 互换）时，写 (2,1) 很容易连带写到它
    MINI_CHECK(map.At(2, 0) == Tile::Empty);
    MINI_CHECK(map.At(1, 1) == Tile::Empty);
    MINI_CHECK(map.At(3, 1) == Tile::Empty);
    MINI_CHECK(map.At(2, 2) == Tile::Empty);

    // 再写回 Empty 也要生效：Set 不是"只能加墙"
    map.Set(2, 1, Tile::Empty);
    MINI_CHECK_EQ(CountNotEmpty(map), 0);
}

// 越界读一律当 Wall（不是 Empty、不抛）—— 世界外都是墙，S7 的碰撞靠这条
MINI_TEST(TileMap, OutOfBoundsAtReturnsWall)
{
    const TileMap map(4, 3);

    MINI_CHECK(map.At(-1, 0) == Tile::Wall);          // 左边界外一格
    MINI_CHECK(map.At(0, -1) == Tile::Wall);          // 上边界外一格
    MINI_CHECK(map.At(4, 0) == Tile::Wall);           // 右边界外一格
    MINI_CHECK(map.At(0, 3) == Tile::Wall);           // 下边界外一格
    MINI_CHECK(map.At(9999, 9999) == Tile::Wall);     // 远到离谱也不许崩
    MINI_CHECK(map.At(-9999, -9999) == Tile::Wall);

    // 顺手把 InBounds 的边界钉住：At 内部调它，但失败时报错要能指向"边界判定"本身
    MINI_CHECK(map.InBounds(0, 0));
    MINI_CHECK(map.InBounds(3, 2));
    MINI_CHECK(!map.InBounds(-1, 0));
    MINI_CHECK(!map.InBounds(0, -1));
    MINI_CHECK(!map.InBounds(4, 2));
    MINI_CHECK(!map.InBounds(3, 3));
}

// 越界写静默忽略：不崩、且不影响任何合法格（与 FrameBuffer::Set 同规矩 ——
// S7 之前 `@` 真的会走出地图，那时"写出去一格"不该是异常）
MINI_TEST(TileMap, OutOfBoundsSetIsIgnored)
{
    TileMap map(4, 3);
    map.Set(-1, 0, Tile::Wall);
    map.Set(0, -1, Tile::Wall);
    map.Set(4, 0, Tile::Wall);
    map.Set(0, 3, Tile::Wall);
    map.Set(9999, 9999, Tile::Wall);
    map.Set(-9999, -9999, Tile::Wall);

    MINI_CHECK_EQ(CountNotEmpty(map), 0);

    // 地图仍然可用：越界写没把 m_w/m_h 或 vector 弄坏
    map.Set(3, 2, Tile::Wall);
    MINI_CHECK(map.At(3, 2) == Tile::Wall);
    MINI_CHECK_EQ(CountNotEmpty(map), 1);
}

// 落在哪一格 = floor：格是区间 [2, 3)，所以 2.9 仍属第 2 格
MINI_TEST(TileMap, ToIndexTruncatesTowardTile)
{
    MINI_CHECK_EQ(TileMap::ToIndex(0.0f), 0);
    MINI_CHECK_EQ(TileMap::ToIndex(0.9f), 0);
    MINI_CHECK_EQ(TileMap::ToIndex(1.0f), 1);
    MINI_CHECK_EQ(TileMap::ToIndex(1.99f), 1);
    MINI_CHECK_EQ(TileMap::ToIndex(2.0f), 2);
    MINI_CHECK_EQ(TileMap::ToIndex(2.9f), 2);
    MINI_CHECK_EQ(TileMap::ToIndex(3.0f), 3);
}

// 负数才是分水岭：(int) 向 0 截断会给 -0.1 → 0（错一格），floor 给 -1。
// 反向验证 2 的目标就是这条：把 FloorToInt 换成 (int)，**只有这条该变红**。
MINI_TEST(TileMap, ToIndexOnNegativeFloor)
{
    MINI_CHECK_EQ(TileMap::ToIndex(-0.1f), -1);
    MINI_CHECK_EQ(TileMap::ToIndex(-0.9f), -1);
    MINI_CHECK_EQ(TileMap::ToIndex(-1.0f), -1);
    MINI_CHECK_EQ(TileMap::ToIndex(-1.5f), -2);
    MINI_CHECK_EQ(TileMap::ToIndex(-2.0f), -2);

    // 把"我们和 (int) 截断不一样"直接写出来：将来若有人把 ToIndex 改成截断，
    // 上面那几条同时也会红，但这两行让失败信息直接指向"截断"这个原因
    MINI_CHECK(TileMap::ToIndex(-0.1f) != static_cast<int>(-0.1f));
    MINI_CHECK(TileMap::ToIndex(-1.5f) != static_cast<int>(-1.5f));
}

// index = ty * m_w + tx —— 公式写错整张表就错了。
//
// ⚠️ **必须用非方阵**：最容易犯的错是把 stride 写成"高"（ty * m_h + tx），
//    而 w == h 时它**恰好等于正确公式**，方阵下永远是绿的。
//    施工图 10.1 第 7 条原文写"用 200×200 的角落与中心"——那样测不出来，
//    改用 5×3，并选"非方阵下会撞车"的两对格子（见下面的注释）。已记入落地偏差。
MINI_TEST(TileMap, IndexIsLinearized)
{
    TileMap map(5, 3);   // 宽 5 != 高 3

    map.Set(3, 0, Tile::Wall);   // 正确 index = 0*5 + 3 = 3
    map.Set(0, 2, Tile::Wall);   // 正确 index = 2*5 + 0 = 10
    map.Set(4, 2, Tile::Wall);   // 正确 index = 2*5 + 4 = 14，正好是最后一格

    MINI_CHECK(map.At(3, 0) == Tile::Wall);
    MINI_CHECK(map.At(0, 2) == Tile::Wall);
    MINI_CHECK(map.At(4, 2) == Tile::Wall);

    // 这两条才是"公式写错"的探测器：若 stride 写成 3（= 高），
    //   · (0,1) 与 (3,0) 都会算成 slot 3
    //   · (3,1) 与 (0,2) 都会算成 slot 6
    // 于是这两格会跟着变 Wall → 断言变红。
    MINI_CHECK(map.At(0, 1) == Tile::Empty);
    MINI_CHECK(map.At(3, 1) == Tile::Empty);

    MINI_CHECK_EQ(CountNotEmpty(map), 3);
}

// 尺寸非正必须抛（与 FrameBuffer 同一规矩）：0 宽的地图没有意义
MINI_TEST(TileMap, InvalidSizeThrows)
{
    MINI_CHECK(ThrowsInvalidArgument([] { const TileMap m(0, 5);  (void)m; }));
    MINI_CHECK(ThrowsInvalidArgument([] { const TileMap m(5, 0);  (void)m; }));
    MINI_CHECK(ThrowsInvalidArgument([] { const TileMap m(-1, 5); (void)m; }));
    MINI_CHECK(ThrowsInvalidArgument([] { const TileMap m(5, -1); (void)m; }));

    // 反向：合法尺寸**不能**抛。少了这条，上面 4 条在
    // "helper 永远返回 true"这种坏实现下照样全绿
    MINI_CHECK(!ThrowsInvalidArgument([] { const TileMap m(1, 1); (void)m; }));
}

// 0 = 无效：它同时是 EntityId{} 的默认值 —— "没有实体"必须只有一种表示法
MINI_TEST(EntityId, ZeroIsInvalid)
{
    MINI_CHECK(!EntityId{}.IsValid());
    MINI_CHECK(!EntityId{0}.IsValid());
    MINI_CHECK(EntityId{1}.IsValid());
    MINI_CHECK(EntityId{42}.IsValid());

    // 运行时镜像一份布局（编译期那份在 Entity.h 的 static_assert）：
    // 它是**句柄**，必须能廉价地按值传
    MINI_CHECK_EQ(sizeof(EntityId), std::size_t{4});
}

// 相等是**按值**的：两个相同的 id 指向同一个实体，不存在"两个句柄指同一实体"
MINI_TEST(EntityId, EqualityIsByValue)
{
    MINI_CHECK(EntityId{7} == EntityId{7});
    MINI_CHECK(EntityId{7} != EntityId{8});
    MINI_CHECK(EntityId{0} == EntityId{});        // 无效 == 无效
    MINI_CHECK(!(EntityId{7} == EntityId{0}));

    // `!=` 是 C++20 由 == 自动生成的（Entity.h 里只写了 ==）。
    // 这里把它用上一次：哪天有人手写了不一致的 ==/!=，这里会响
    const EntityId a{3};
    const EntityId b{3};
    MINI_CHECK(a == b);
    MINI_CHECK(!(a != b));
}

// ------------------------------------------------------------
// World（10）—— 容器 / 积分 / 延迟删除（S5 步骤 2）
//
// ⚠️ 这个 suite 里三件事最容易写错，所以每条都踩在明处：
//   1. `Find` 返回的指针**不能跨 Spawn / FlushRemovals 使用**（扩容 / swap-erase）；
//   2. 比位置只能用 `ApproxEquals` 或 `MINI_CHECK_NEAR` —— `Vec2` 刻意没有 `==`，
//      也不能进 `MINI_CHECK_EQ`（那个宏会调 `ToString`，要求 `operator<<`，Vec2 也没有）；
//   3. `size()` 是 `std::size_t`，所以写 `std::size_t{n}`（免得有符号/无符号比较告警）。
// ------------------------------------------------------------

namespace {

// 造一个"会动的"实体原型：第 1 格中心、12 格/秒朝右、画 '@'
Entity MakePlayer()
{
    Entity e{};
    e.transform.pos = Vec2{0.5f, 0.5f};
    e.velocity.perSecond = Vec2{12.0f, 0.0f};
    e.symbol = '@';
    return e;
}

} // namespace

// id 必须**严格递增**且互不相同；prototype 里填的 id 一律被覆盖
MINI_TEST(World, SpawnAssignsIncreasingIds)
{
    World world(4, 3);

    const EntityId a = world.Spawn(MakePlayer());
    const EntityId b = world.Spawn(MakePlayer());
    const EntityId c = world.Spawn(MakePlayer());

    MINI_CHECK(a.IsValid());
    MINI_CHECK(a != b);
    MINI_CHECK(b != c);
    MINI_CHECK(a != c);

    // 不只是"互不相同"，而是**只增不减**（复用会让悬空 id 静默指错实体）
    MINI_CHECK(a.value < b.value);
    MINI_CHECK(b.value < c.value);

    MINI_CHECK_EQ(world.Entities().size(), std::size_t{3});
}

// Find 拿到的是同一份数据；顺带把 const 版与 Map() 的两版都真用上一次
MINI_TEST(World, FindFindsSpawnedEntity)
{
    World world(4, 3);

    Entity prototype = MakePlayer();
    prototype.id = EntityId{999};          // 故意乱填：Spawn 必须覆盖它
    const EntityId id = world.Spawn(prototype);
    MINI_CHECK(id != EntityId{999});

    Entity* found = world.Find(id);
    MINI_CHECK(found != nullptr);
    if (found == nullptr) {
        return;   // 后面全是解引用：空指针会让 runner 崩掉，而不是报"断言失败"
    }

    MINI_CHECK(found->id == id);
    MINI_CHECK(found->symbol == '@');
    MINI_CHECK(found->active);
    MINI_CHECK(found->transform.pos.ApproxEquals(Vec2{0.5f, 0.5f}));

    // const 版：证明"只读地看世界"这条路是通的 —— 这正是 Find 要两个版本的理由
    const World& cworld = world;
    const Entity* cfound = cworld.Find(id);
    MINI_CHECK(cfound != nullptr);
    if (cfound != nullptr) {
        MINI_CHECK(cfound->symbol == '@');
    }
    MINI_CHECK_EQ(cworld.Entities().size(), std::size_t{1});

    // Map() 两版：World 确实拥有那张地图，借出去的**就是同一张**
    world.Map().Set(1, 1, Tile::Wall);
    MINI_CHECK(cworld.Map().At(1, 1) == Tile::Wall);
    MINI_CHECK_EQ(cworld.Map().Width(), 4);
}

// 无效 id / 没分配过 / 已删除 —— 三种都给 nullptr，**绝不"找到错的"**
MINI_TEST(World, FindUnknownIdReturnsNull)
{
    World world(4, 3);
    const EntityId alive = world.Spawn(MakePlayer());

    MINI_CHECK(world.Find(EntityId{}) == nullptr);       // 无效 id
    MINI_CHECK(world.Find(EntityId{9999}) == nullptr);   // 从没分配过
    MINI_CHECK(world.Find(alive) != nullptr);            // 对照组

    world.MarkForRemoval(alive);
    world.FlushRemovals();
    MINI_CHECK(world.Find(alive) == nullptr);            // 已删除：找不到，而不是找错
}

// id 永不复用：删掉之后新 Spawn 得到的是**新的** id
MINI_TEST(World, SpawnedIdIsNeverReused)
{
    World world(4, 3);

    const EntityId first = world.Spawn(MakePlayer());
    world.MarkForRemoval(first);
    world.FlushRemovals();
    MINI_CHECK_EQ(world.Entities().size(), std::size_t{0});

    const EntityId second = world.Spawn(MakePlayer());
    MINI_CHECK(second != first);
    MINI_CHECK(second.value > first.value);
    MINI_CHECK(world.Find(first) == nullptr);
}

// **PLAN 验收的机器形式**：速度 × dt ⇒ 位移与帧率无关
MINI_TEST(World, MovementIsDtIndependent)
{
    // 同样 1.0 秒：一个大步 vs 一百个小步 → 位置必须一样
    World coarse(kMoveMapW, kMoveMapH);   // ⚠️ 不能用 4×3：S7 起角色会撞上世界边界（见 kMoveMapW）
    const EntityId cid = coarse.Spawn(MakePlayer());
    coarse.Update(1.0f);

    World fine(kMoveMapW, kMoveMapH);
    const EntityId fid = fine.Spawn(MakePlayer());
    for (int i = 0; i < 100; ++i) {
        fine.Update(0.01f);
    }

    const Entity* pc = coarse.Find(cid);
    const Entity* pf = fine.Find(fid);
    MINI_CHECK(pc != nullptr && pf != nullptr);
    if (pc == nullptr || pf == nullptr) {
        return;
    }

    // 容差 1e-5：100 次 float 加法的舍入累计在 ulp(12)≈1e-6 量级（留了一个数量级余量）
    MINI_CHECK_NEAR(pc->transform.pos.x, pf->transform.pos.x, 1e-5f);
    MINI_CHECK_NEAR(pc->transform.pos.y, pf->transform.pos.y, 1e-5f);
    MINI_CHECK_NEAR(pc->transform.pos.x, 12.5f, 1e-5f);   // 0.5 + 12 格/秒 × 1 秒
}

// 位移与速度成正比；速度为 0 就纹丝不动
MINI_TEST(World, MovementScalesWithSpeed)
{
    World world(kMoveMapW, kMoveMapH);   // ⚠️ 不能用 4×3：6 格位移会撞上边界

    const EntityId fast = world.Spawn(MakePlayer());   // {12, 0}
    world.Update(0.5f);                                // → 移动 6 格
    {
        const Entity* p = world.Find(fast);
        MINI_CHECK(p != nullptr);
        if (p != nullptr) {
            MINI_CHECK_NEAR(p->transform.pos.x, 6.5f, 1e-5f);
            MINI_CHECK_NEAR(p->transform.pos.y, 0.5f, 1e-5f);
        }
    }
    // ⚠️ 上面那个 p 已经出了作用域 —— 下面再 Spawn 一次会让它失效（本用例故意这么写）

    Entity still = MakePlayer();
    still.velocity.perSecond = Vec2{0.0f, 0.0f};
    const EntityId stopped = world.Spawn(still);
    world.Update(0.5f);

    const Entity* ps = world.Find(stopped);
    MINI_CHECK(ps != nullptr);
    if (ps != nullptr) {
        MINI_CHECK_NEAR(ps->transform.pos.x, 0.5f, 1e-5f);
        MINI_CHECK_NEAR(ps->transform.pos.y, 0.5f, 1e-5f);
    }
}

// active == false 不参与积分，但**仍在容器里**（"停更" ≠ "删除"）
MINI_TEST(World, InactiveEntityDoesNotMove)
{
    World world(kMoveMapW, kMoveMapH);   // ⚠️ 不能用 4×3：active 那条要移动 6 格

    Entity frozen = MakePlayer();
    frozen.active = false;
    const EntityId frozenId = world.Spawn(frozen);
    const EntityId liveId = world.Spawn(MakePlayer());   // 同速度，但 active

    world.Update(0.5f);

    const Entity* pFrozen = world.Find(frozenId);
    const Entity* pLive = world.Find(liveId);
    MINI_CHECK(pFrozen != nullptr && pLive != nullptr);
    if (pFrozen == nullptr || pLive == nullptr) {
        return;
    }

    MINI_CHECK_NEAR(pFrozen->transform.pos.x, 0.5f, 1e-5f);   // 没动
    MINI_CHECK_NEAR(pLive->transform.pos.x, 6.5f, 1e-5f);     // 动了
    MINI_CHECK_EQ(world.Entities().size(), std::size_t{2});
}

// "遍历中禁止删除"的可执行形式：登记之后它**还在**，Flush 之后才消失
// （反向验证 3 的目标：把 MarkForRemoval 改成立即删，这条必须变红）
MINI_TEST(World, MarkedEntitySurvivesUntilFlush)
{
    World world(kMoveMapW, kMoveMapH);   // ⚠️ 不能用 4×3：下面要 Update 0.5 秒（6 格）
    const EntityId id = world.Spawn(MakePlayer());

    world.MarkForRemoval(id);

    // ① 登记之后仍然找得到、还在容器里 —— 调用方因此能放心地"边遍历边标记"
    MINI_CHECK(world.Find(id) != nullptr);
    MINI_CHECK_EQ(world.Entities().size(), std::size_t{1});

    // ② 登记**不影响 Update**（本步定下的语义：只有 active 决定要不要更新。
    //    删除是"这一帧末"的事，不是"立刻失效"）
    world.Update(0.5f);
    {
        const Entity* p = world.Find(id);
        MINI_CHECK(p != nullptr);
        if (p != nullptr) {
            MINI_CHECK_NEAR(p->transform.pos.x, 6.5f, 1e-5f);
        }
    }

    // ③ Flush 之后才真的消失
    world.FlushRemovals();
    MINI_CHECK(world.Find(id) == nullptr);
    MINI_CHECK_EQ(world.Entities().size(), std::size_t{0});
}

// 删中间一个之后：**剩下的集合**正确 —— 顺序不保证，所以这里一条顺序断言都没有
MINI_TEST(World, FlushRemovalsKeepsSetOrderUnspecified)
{
    World world(4, 3);
    const EntityId a = world.Spawn(MakePlayer());
    const EntityId b = world.Spawn(MakePlayer());
    const EntityId c = world.Spawn(MakePlayer());

    world.MarkForRemoval(b);
    world.FlushRemovals();

    MINI_CHECK(world.Find(a) != nullptr);
    MINI_CHECK(world.Find(c) != nullptr);
    MINI_CHECK(world.Find(b) == nullptr);
    MINI_CHECK_EQ(world.Entities().size(), std::size_t{2});
}

// Flush 幂等：没标记过 / 重复标记 / 标记已删除的 id / 无效 id —— 全都不许出事
MINI_TEST(World, FlushRemovalsIsIdempotent)
{
    World world(4, 3);
    const EntityId id = world.Spawn(MakePlayer());

    world.FlushRemovals();                               // 清单本来就是空的
    MINI_CHECK(world.Find(id) != nullptr);

    world.MarkForRemoval(id);
    world.MarkForRemoval(id);                            // 重复登记 → 只算一次
    MINI_CHECK_EQ(world.Entities().size(), std::size_t{1});

    world.FlushRemovals();
    MINI_CHECK_EQ(world.Entities().size(), std::size_t{0});

    world.FlushRemovals();                               // 再 Flush：清单已空
    world.MarkForRemoval(id);                            // 标记一个已经不存在的 id
    world.MarkForRemoval(EntityId{});                    // 无效 id
    world.FlushRemovals();
    MINI_CHECK_EQ(world.Entities().size(), std::size_t{0});
}
