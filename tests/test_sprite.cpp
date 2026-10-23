#include "MiniTest.h"

#include <mini/render/RayCaster.h>   // WallHeightRows（"精灵与墙同一尺度"的期望值要从它算）
#include <mini/render/Sprite.h>

// ------------------------------------------------------------
// Sprite（S13 第 1 笔）：伪 3D 精灵的**纯投影数学**。
//
// 全部离线：只依赖 `Vec2` + `RayCamera` ⇒ 不需要终端、不需要 `game/`、不需要 `FrameBuffer`。
// ⚠️ 期望值一律**按几何/代数算**（"与墙同一个 lineH"），不从实现里抄常数 ——
//    否则改实现时用例会跟着一起错，等于没测。
// ⚠️ 这一组**只测投影**：遮挡、排序、z 的脚底换算都是 `game/` 侧的事（第 2~4 笔）。
// ------------------------------------------------------------

namespace {

using mini::math::Vec2;
using mini::render::ProjectSprite;
using mini::render::RayCamera;
using mini::render::SpriteBox;
using mini::render::SpritePlace;
using mini::render::WallHeightRows;

// 视口取"100 列 × 28 行，世界从第 2 行开始"（= `kHudRow` 两行的真实形状）。
// `aspect` **按 `AspectForViewport` 的式子现算**，不写死 1.353：
//     aspect = cols / (2 · fovTan · viewH · r)
constexpr int   kCols    = 100;
constexpr int   kViewTop = 2;
constexpr int   kViewH   = 28;
constexpr float kFovTan  = 0.66f;
constexpr float kCellHW  = 2.0f;    // 字符格"高 / 宽"（`Game.cpp` 的 kCellHeightPerWidth）
constexpr float kWallH   = 2.0f;    // 分叉 C 甲：1 格 = 半条墙带
constexpr float kAspect =
    static_cast<float>(kCols) / (2.0f * kFovTan * static_cast<float>(kViewH) * kCellHW);

// 地平线（函数内部由 viewTop + viewH/2 派生）
constexpr int kHorizon = kViewTop + kViewH / 2;

// 相机 (10.5, 10.5) 朝正东
RayCamera CamEast()
{
    return RayCamera{Vec2{10.5f, 10.5f}, Vec2{1.0f, 0.0f}, kFovTan};
}

SpritePlace PlaceAt(float x, float y, float z = 0.0f, float tall = 1.0f, float wide = 1.0f)
{
    return SpritePlace{Vec2{x, y}, z, tall, wide};
}

SpriteBox Project(const SpritePlace& place)
{
    return ProjectSprite(place, CamEast(), kCols, kViewTop, kViewH, kAspect, kCellHW, kWallH);
}

} // namespace

// ------------------------------------------------------------
// 1. 正前方（cameraX == 0）⇒ 列区间**围绕 cols/2 对称**
// ------------------------------------------------------------
MINI_TEST(Sprite, CenteredSpriteIsCentered)
{
    const SpriteBox box = Project(PlaceAt(14.0f, 10.5f));   // perp = 3.5，横向偏移 0

    MINI_CHECK(box.visible);
    MINI_CHECK(box.col0 < kCols / 2);
    MINI_CHECK(kCols / 2 < box.col1);

    // 两次 floor 各可能差 1 ⇒ 中心允许偏 1 列（对称性，不是"宽度恰好等于某个数"）
    const int sum = box.col0 + box.col1;
    MINI_CHECK(sum >= kCols - 2 && sum <= kCols + 2);

    MINI_CHECK(box.row0 >= kViewTop);
    MINI_CHECK(box.row1 <= kViewTop + kViewH);
}

// ------------------------------------------------------------
// 2. 距离翻倍 ⇒ 行数与列数**都**约减半（单调 + 量级）
// ------------------------------------------------------------
MINI_TEST(Sprite, SpriteShrinksWithDistance)
{
    const SpriteBox nearBox = Project(PlaceAt(14.0f, 10.5f));   // perp = 3.5
    const SpriteBox farBox  = Project(PlaceAt(17.5f, 10.5f));   // perp = 7.0

    MINI_CHECK(nearBox.visible);
    MINI_CHECK(farBox.visible);

    const int nearRows = nearBox.row1 - nearBox.row0;
    const int farRows  = farBox.row1 - farBox.row0;
    const int nearCols = nearBox.col1 - nearBox.col0;
    const int farCols  = farBox.col1 - farBox.col0;

    MINI_CHECK(farRows < nearRows);
    MINI_CHECK(farCols < nearCols);

    // "约减半"：取整误差最多 2（远的那份 ×2 与近的那份相比）
    const int rowGap = nearRows - 2 * farRows;
    const int colGap = nearCols - 2 * farCols;
    MINI_CHECK(rowGap >= -2 && rowGap <= 2);
    MINI_CHECK(colGap >= -2 && colGap <= 2);
}

// ------------------------------------------------------------
// 3. z = 0 ⇒ 底边**就在地面行**（期望值用 `WallHeightRows` 同尺度现算，不写死行数）
// ------------------------------------------------------------
MINI_TEST(Sprite, SpriteStandsOnFloor)
{
    const SpriteBox box = Project(PlaceAt(14.0f, 10.5f, 0.0f, 1.0f, 1.0f));

    MINI_CHECK(box.visible);

    // 地面行 = 地平线 + 那面墙的半高（墙 = 一整面，脚在地面）。
    // ⚠️ 与 `DrawRaycastWorld` 用的是**同一个** `WallHeightRows` ⇒ 改了墙的尺度这条会一起动。
    const float lineH    = WallHeightRows(3.5f, kViewH, kAspect);
    const int   ground   = static_cast<int>(static_cast<float>(kHorizon) + lineH * 0.5f);

    MINI_CHECK_EQ(box.row1, ground);
    MINI_CHECK(box.row0 >= kViewTop);
    MINI_CHECK(box.row0 < box.row1);
}

// ------------------------------------------------------------
// 4. z 增大 ⇒ 行区间**整体上移**（高度基本不变）
// ------------------------------------------------------------
MINI_TEST(Sprite, HeightLiftsSprite)
{
    const SpriteBox onFloor = Project(PlaceAt(14.0f, 10.5f, 0.0f, 1.0f, 1.0f));
    const SpriteBox lifted  = Project(PlaceAt(14.0f, 10.5f, 1.0f, 1.0f, 1.0f));

    MINI_CHECK(onFloor.visible);
    MINI_CHECK(lifted.visible);

    MINI_CHECK(lifted.row0 < onFloor.row0);
    MINI_CHECK(lifted.row1 < onFloor.row1);

    // 高度不变（取整后可差 1 行）
    const int dh = (lifted.row1 - lifted.row0) - (onFloor.row1 - onFloor.row0);
    MINI_CHECK(dh >= -1 && dh <= 1);
}

// ------------------------------------------------------------
// 5. 相机背后 / 贴脸 ⇒ visible == false
// ------------------------------------------------------------
MINI_TEST(Sprite, BehindCameraIsInvisible)
{
    const SpriteBox behind = Project(PlaceAt(8.0f, 10.5f));   // 相机朝东，它在西边
    MINI_CHECK(!behind.visible);

    // 贴脸：forward = 0.005 <= kMinPerp ⇒ 直接不画（否则行数爆）
    const SpriteBox tooClose = Project(PlaceAt(10.505f, 10.5f));
    MINI_CHECK(!tooClose.visible);
}

// ------------------------------------------------------------
// 6. cellsWide 翻倍 ⇒ 列数翻倍，行**不变**
// ------------------------------------------------------------
MINI_TEST(Sprite, WideSpriteSpansMoreColumns)
{
    const SpriteBox oneCell = Project(PlaceAt(14.0f, 10.5f, 0.0f, 1.0f, 1.0f));
    const SpriteBox twoCell = Project(PlaceAt(14.0f, 10.5f, 0.0f, 1.0f, 2.0f));

    MINI_CHECK(oneCell.visible);
    MINI_CHECK(twoCell.visible);

    // 宽度只影响列：行区间一个字节都不该动
    MINI_CHECK_EQ(twoCell.row0, oneCell.row0);
    MINI_CHECK_EQ(twoCell.row1, oneCell.row1);

    const int oneCols = oneCell.col1 - oneCell.col0;
    const int twoCols = twoCell.col1 - twoCell.col0;
    MINI_CHECK(twoCols >= 2 * oneCols - 1);
    MINI_CHECK(twoCols <= 2 * oneCols + 1);
}

// ------------------------------------------------------------
// 7. 坑 15：远处的小精灵（子弹）**仍至少占 1 行**，不许被取整成 0 行
// ------------------------------------------------------------
MINI_TEST(Sprite, SpriteAtLeastOneRowVisible)
{
    // perp = 20、cellsTall = 0.2 ⇒ 0.19 行 ⇒ floor 后上下边同一行
    const SpriteBox bullet = Project(PlaceAt(30.5f, 10.5f, 0.0f, 0.2f, 1.0f));

    MINI_CHECK(bullet.visible);
    MINI_CHECK(bullet.row1 - bullet.row0 >= 1);
    MINI_CHECK(bullet.row0 >= kViewTop);
    MINI_CHECK(bullet.row1 <= kViewTop + kViewH);
}

// ------------------------------------------------------------
// 8. R4 甲的靶子：裁剪在纯函数里做完 ⇒ col ∈ [0, cols)、row ∈ [viewTop, viewTop+viewH)
//    （Game 级那条 `SpritesNeverTouchHudRows` 守不住：HUD 后画会把整行抹掉 ⇒ 永远绿）
// ------------------------------------------------------------
MINI_TEST(Sprite, SpriteStaysInsideViewport)
{
    // perp = 1、横向偏 0.594 格 ⇒ centerCol ≈ 95，宽 ≈ 18.9 列/格 × 1 格 ⇒ 右边出界
    // 同一颗精灵在地面、1 格高 ⇒ 高 ≈ 18.9 行、底边 ≈ 34.9 ⇒ 下边也出界
    const SpriteBox box = Project(PlaceAt(11.5f, 11.094f));

    MINI_CHECK(box.visible);

    // 不是"恰好落在范围内"，而是**真的被夹住**了（否则这条守不住裁剪）
    MINI_CHECK_EQ(box.col1, kCols);
    MINI_CHECK_EQ(box.row1, kViewTop + kViewH);

    MINI_CHECK(box.col0 >= 0);
    MINI_CHECK(box.col0 < box.col1);
    MINI_CHECK(box.row0 >= kViewTop);
    MINI_CHECK(box.row0 < box.row1);
}
