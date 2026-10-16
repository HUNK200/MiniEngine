#include "MiniTest.h"

#include <mini/render/RayCaster.h>
#include <mini/world/TileMap.h>

#include <cmath>   // std::isfinite / std::sqrt

// ------------------------------------------------------------
// Raycaster（S12 第 3 笔）：伪 3D 的**纯数学**部分。
//
// 全部离线：只依赖 `TileMap` + `Vec2` ⇒ 不需要终端、不需要 `game/`、不需要 `FrameBuffer`。
// ⚠️ 期望值一律**按几何算**（"相机到墙面的垂距"），不从实现里抄常数 ——
//    否则改实现时用例会跟着一起错，等于没测。
// ------------------------------------------------------------

namespace {

using mini::math::Vec2;
using mini::render::CastColumn;
using mini::render::kMinPerp;
using mini::render::kShadeCount;
using mini::render::RayCamera;
using mini::render::RayHit;
using mini::render::ShadeIndex;
using mini::render::WallHeightRows;
using mini::world::Tile;
using mini::world::TileMap;

// ⚠️ **必须是偶数**：`cameraX = 2*col/cols − 1` 的零点只落在 `col = cols/2` 上，
//    而 `cols` 为**奇数**时 `cols/2` 不是整数列 ⇒ **根本取不到 `cameraX == 0`**。
//    第一版这里写的是 21（奇数）⇒ `CastCenter` 其实偏了 1/21 格 ⇒ 三条用例同时红
//    （`dist != perp`、斜向的命中格差 1、垂距差 0.24）。
//    ⚠️ 那三条红**是用例的错，不是实现的错**：鱼眼那条"三列 `perp` 相等"**是绿的**。
constexpr int kCols = 20;
static_assert(kCols % 2 == 0, "kCols 必须是偶数，否则 CastCenter 取不到 cameraX == 0");

// 一条**竖墙**（整列都是墙）⇒ 它给的是"东西向的墙面"，命中时 sideX == true
void AddVerticalWall(TileMap& map, int wallX)
{
    for (int y = 0; y < map.Height(); ++y) {
        map.Set(wallX, y, Tile::Wall);
    }
}

// 一条**横墙**（整行都是墙）⇒ 命中时 sideX == false
void AddHorizontalWall(TileMap& map, int wallY)
{
    for (int x = 0; x < map.Width(); ++x) {
        map.Set(x, wallY, Tile::Wall);
    }
}

// 打**正中间**那一列：cameraX == 0 ⇒ rayDir == cam.dir（角度相关的期望值最好算）
RayHit CastCenter(const TileMap& map, const RayCamera& cam)
{
    return CastColumn(map, cam, kCols / 2, kCols);
}

} // namespace

// ------------------------------------------------------------
// 1. 轴向：正东 / 正北看一堵墙 ⇒ `perp` = 到那面墙的**垂直距离**
// ------------------------------------------------------------
MINI_TEST(Raycaster, AxisAlignedHitDistance)
{
    // 正东：相机 (2.5, 6.5)，竖墙在 x = 8 ⇒ 墙的**西面**在 x = 8 ⇒ 垂距 = 8 − 2.5 = 5.5
    TileMap map{20, 12};
    AddVerticalWall(map, 8);

    const RayHit east = CastCenter(map, RayCamera{Vec2{2.5f, 6.5f}, Vec2{1.0f, 0.0f}, 0.66f});
    MINI_CHECK(east.hit);
    MINI_CHECK_EQ(east.tileX, 8);
    MINI_CHECK_EQ(east.tileY, 6);
    MINI_CHECK_NEAR(east.perp, 5.5f, 1e-4f);
    MINI_CHECK(east.sideX);                       // 跨竖格线 ⇒ 东西向的墙面
    MINI_CHECK_NEAR(east.dist, east.perp, 1e-4f); // 正东 ⇒ 斜距 == 垂距

    // 正北：相机 (6.5, 9.5)，横墙在 y = 4 ⇒ 墙的**南面**在 y = 5 ⇒ 垂距 = 9.5 − 5 = 4.5
    TileMap map2{20, 12};
    AddHorizontalWall(map2, 4);

    const RayHit north = CastCenter(map2, RayCamera{Vec2{6.5f, 9.5f}, Vec2{0.0f, -1.0f}, 0.66f});
    MINI_CHECK(north.hit);
    MINI_CHECK_EQ(north.tileX, 6);
    MINI_CHECK_EQ(north.tileY, 4);
    MINI_CHECK_NEAR(north.perp, 4.5f, 1e-4f);
    MINI_CHECK(!north.sideX);                     // 跨横格线 ⇒ 南北向的墙面
}

// ------------------------------------------------------------
// 2. ★ **鱼眼校正的机器判据**：同一面墙在 col = 0 / 中间 / cols−1 上的 `perp` **必须相等**
//
// ⚠️ 这就是反向验证 R1 的靶子：把 `perp` 改成直接用欧氏斜距（= 漏掉 cos 校正）⇒ 两侧列偏大 ⇒ 红。
// ⚠️ 同时反着钉一条：**斜距必须不同**（两侧更长）—— 否则说明"斜距/垂距"这两个概念根本没分开，
//    用例会在"两个都写成一样"的实现下**假绿**。
// ------------------------------------------------------------
MINI_TEST(Raycaster, PerpIsEqualAcrossColumns)
{
    TileMap map{24, 20};
    AddVerticalWall(map, 12);            // 一面竖墙，上下都够开阔（两侧列不会先撞到别的墙）

    const RayCamera cam{Vec2{2.5f, 10.5f}, Vec2{1.0f, 0.0f}, 0.66f};

    const RayHit left   = CastColumn(map, cam, 0, kCols);
    const RayHit middle = CastColumn(map, cam, kCols / 2, kCols);
    const RayHit right  = CastColumn(map, cam, kCols - 1, kCols);

    MINI_CHECK(left.hit);
    MINI_CHECK(middle.hit);
    MINI_CHECK(right.hit);

    // 三列都打在**同一面墙**（x = 12）上 ⇒ 垂距恒等于 12 − 2.5 = 9.5（与列号无关！）
    MINI_CHECK_NEAR(left.perp, 9.5f, 1e-3f);
    MINI_CHECK_NEAR(middle.perp, left.perp, 1e-3f);
    MINI_CHECK_NEAR(right.perp, left.perp, 1e-3f);

    // 中间那一列是正东 ⇒ 斜距 == 垂距
    MINI_CHECK_NEAR(middle.dist, middle.perp, 1e-3f);

    // 两侧列是斜着打过去的 ⇒ **斜距**必须明显更大（否则这两个字段就没分开）
    MINI_CHECK(left.dist > left.perp + 0.5f);
    MINI_CHECK(right.dist > right.perp + 0.5f);
}

// ------------------------------------------------------------
// 3. 斜向 45°：命中格正确（DDA 走格没走错）
// ------------------------------------------------------------
MINI_TEST(Raycaster, DiagonalHitFindsCorrectTile)
{
    TileMap map{24, 24};
    AddVerticalWall(map, 8);

    // 相机 (2.5, 3.7)，方向 = 东南 45°（单位向量）
    //   ⇒ 走到 x = 8 时 Δx = 5.5（y 同步走 5.5）⇒ y = 9.2 ⇒ 落在第 9 行
    constexpr float kInvSqrt2 = 0.70710678f;
    const RayHit    hit = CastCenter(map, RayCamera{Vec2{2.5f, 3.7f}, Vec2{kInvSqrt2, kInvSqrt2}, 0.66f});

    MINI_CHECK(hit.hit);
    MINI_CHECK_EQ(hit.tileX, 8);
    MINI_CHECK_EQ(hit.tileY, 9);
    MINI_CHECK(hit.sideX);                          // 跨的是竖格线（尽管是斜着过去的）
    MINI_CHECK_NEAR(hit.perp, 5.5f / kInvSqrt2, 1e-2f);   // 垂距 = Δx ÷ dir.x
}

// ------------------------------------------------------------
// 4. `sideX`：跨竖格线（东西向墙面） vs 跨横格线（南北向墙面）
// ------------------------------------------------------------
MINI_TEST(Raycaster, SideFlagIsCorrect)
{
    TileMap map{20, 12};
    AddVerticalWall(map, 8);
    AddHorizontalWall(map, 10);          // 两条墙不挡彼此的直线视线

    const RayCamera cam{Vec2{2.5f, 2.5f}, Vec2{1.0f, 0.0f}, 0.66f};

    const RayHit east = CastCenter(map, cam);
    MINI_CHECK(east.hit);
    MINI_CHECK(east.sideX);
    MINI_CHECK_EQ(east.tileX, 8);

    const RayCamera south{Vec2{2.5f, 2.5f}, Vec2{0.0f, 1.0f}, 0.66f};
    const RayHit     down = CastCenter(map, south);
    MINI_CHECK(down.hit);
    MINI_CHECK(!down.sideX);
    MINI_CHECK_EQ(down.tileY, 10);
}

// ------------------------------------------------------------
// 5. 墙高：随距离**单调递减**，且**远墙至少 1 行**（算成 0 会让远处的墙在屏幕上"消失"）
// ------------------------------------------------------------
MINI_TEST(Raycaster, WallHeightShrinksWithDistance)
{
    constexpr int   kViewH  = 28;
    constexpr float kAspect = 2.0f;

    const float hNear = WallHeightRows(2.0f, kViewH, kAspect);
    const float hMid  = WallHeightRows(4.0f, kViewH, kAspect);
    const float hFar  = WallHeightRows(100.0f, kViewH, kAspect);

    MINI_CHECK(hNear > hMid);
    MINI_CHECK(hMid > hFar);
    MINI_CHECK(hFar >= 1.0f);                                    // ★ 远墙至少一行
    MINI_CHECK_NEAR(hNear, static_cast<float>(kViewH) / 2.0f * kAspect, 1e-3f);   // 28 ÷ 2 × 2 = 28
}

// ------------------------------------------------------------
// 6. 纵横比：`cellAspect` 翻倍 ⇒ 行数精确翻倍（终端字符格不是正方形）
// ------------------------------------------------------------
MINI_TEST(Raycaster, AspectScalesLinearly)
{
    const float h1 = WallHeightRows(2.0f, 28, 1.0f);
    const float h2 = WallHeightRows(2.0f, 28, 2.0f);

    MINI_CHECK_NEAR(h2, h1 * 2.0f, 1e-3f);
}

// ------------------------------------------------------------
// 7. 贴脸：`perp` 被夹到 `kMinPerp` ⇒ 不除零、墙高有限（不会炸成 inf/NaN）
// ------------------------------------------------------------
MINI_TEST(Raycaster, TinyDistanceIsClamped)
{
    TileMap map{20, 12};
    AddVerticalWall(map, 8);

    // 相机离墙只有 0.001 格
    const RayHit hit = CastCenter(map, RayCamera{Vec2{7.999f, 6.5f}, Vec2{1.0f, 0.0f}, 0.66f});

    MINI_CHECK(hit.hit);
    MINI_CHECK_EQ(hit.tileX, 8);
    MINI_CHECK(hit.perp >= kMinPerp);
    MINI_CHECK_NEAR(hit.perp, kMinPerp, 1e-6f);

    // 贴脸时墙高会很大（28 ÷ 0.01 × 2 = 5600 行）—— 但必须是**有限**的，
    // 超出视口的那部分由调用方裁剪（`FrameBuffer` 本来就越界忽略）
    const float rows = WallHeightRows(hit.perp, 28, 2.0f);
    MINI_CHECK(std::isfinite(rows));
    MINI_CHECK(rows < 1.0e6f);
}

// ------------------------------------------------------------
// 8. **越界 = 墙**（与 S7 的口径一致）：往西走出地图 ⇒ 仍然 `hit`，命中格是 −1
// ------------------------------------------------------------
MINI_TEST(Raycaster, OutOfBoundsCountsAsWall)
{
    TileMap map{20, 12};                  // 空地图：边界外靠 `IsSolidAt` 兜
    const RayHit hit = CastCenter(map, RayCamera{Vec2{2.5f, 6.5f}, Vec2{-1.0f, 0.0f}, 0.66f});

    MINI_CHECK(hit.hit);
    MINI_CHECK_EQ(hit.tileX, -1);
    MINI_CHECK_EQ(hit.tileY, 6);
    MINI_CHECK_NEAR(hit.perp, 2.5f, 1e-4f);   // 从 x = 2.5 到第 0 格的**西面**（x = 0）
}

// ------------------------------------------------------------
// 9. 明暗档位：**单调**（近亮远暗）+ 恒落在 `[0, kShadeCount)`
// ------------------------------------------------------------
MINI_TEST(Raycaster, ShadeIndexIsMonotonic)
{
    const float kProbes[] = {0.1f, 1.0f, 2.0f, 3.0f, 5.0f, 9.0f, 50.0f, 1.0e5f};

    int previous = 0;
    for (const float p : kProbes) {
        const int idx = ShadeIndex(p);
        MINI_CHECK(idx >= 0);
        MINI_CHECK(idx < kShadeCount);
        MINI_CHECK(idx >= previous);        // 单调不减（近亮远暗）
        previous = idx;
    }

    MINI_CHECK_EQ(ShadeIndex(0.1f), 0);                  // 最近 ⇒ 最亮那一档
    MINI_CHECK_EQ(ShadeIndex(1.0e5f), kShadeCount - 1);  // 最远 ⇒ 最暗那一档
}

// ------------------------------------------------------------
// 10. 边界列 / 病态列数：不许崩、不许出 NaN / ±inf
// ------------------------------------------------------------
MINI_TEST(Raycaster, EdgeColumnsDoNotCrash)
{
    TileMap map{20, 12};
    AddVerticalWall(map, 14);

    const RayCamera cam{Vec2{2.5f, 6.5f}, Vec2{1.0f, 0.0f}, 0.66f};
    const int       kColsToProbe[] = {0, 1, 40, 79};     // 含最左列与最右列（cols = 80）

    for (const int col : kColsToProbe) {
        const RayHit hit = CastColumn(map, cam, col, 80);
        MINI_CHECK(std::isfinite(hit.perp));
        MINI_CHECK(std::isfinite(hit.dist));
        MINI_CHECK(hit.perp >= kMinPerp);
    }

    // 病态输入：`cols <= 0` ⇒ 返回"没命中"，但仍必须是有限值
    const RayHit bad = CastColumn(map, cam, 0, 0);
    MINI_CHECK(!bad.hit);
    MINI_CHECK(std::isfinite(bad.perp));

    // 单列（cols = 1）⇒ cameraX = −1（视锥最左），照样有限
    const RayHit single = CastColumn(map, cam, 0, 1);
    MINI_CHECK(std::isfinite(single.perp));
    MINI_CHECK(single.perp >= kMinPerp);
}
