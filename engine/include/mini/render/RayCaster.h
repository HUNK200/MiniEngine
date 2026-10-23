#pragma once

#include "mini/math/Vec2.h"
#include "mini/render/RayCamera.h"  // RayCamera + kMinPerp（S13 第 1 笔搬来；见 A-2）
#include "mini/world/Collision.h"   // IsSolidAt —— "什么是墙"的**唯一**判据
#include "mini/world/TileMap.h"

#include <array>
#include <cmath>     // std::abs / std::sqrt
#include <cstddef>   // std::size_t

// ============================================================
// RayCaster.h —— 伪 3D（射线投射）的**纯数学**部分（S12 第 3 笔，分叉 C = 甲）
//
// 四样东西，全都**不认识** `FrameBuffer` / HUD / 顶栏 / 终端：
//   RayCamera      相机 = 位置 + 方向 + 视野（相机平面半长）★ 已搬到 `render/RayCamera.h`（S13 A-2）
//   CastColumn     一条列射线 → 命中信息（斜距 / 垂距 / 命中格 / 命中哪个面）
//   WallHeightRows 垂距 → 墙在屏幕上占**几行**（纯数学）
//   ShadeIndex     垂距 → 明暗档位（0 = 最近最亮）
//
// 为什么单独拆出来（而不是直接塞进 `game/`）：
//   · 它只依赖 `math/Vec2.h` + `render/RayCamera.h` + `world/TileMap.h` + `world/Collision.h`
//     ⇒ **可以离线单测**（`tests/test_raycaster.cpp`，10 条；S13 起 `RayCamera.h` 被单独搬出）；
//   · 它是**引擎能力**（将来第二个游戏 / 别的关卡沿用同一套），不是这一个游戏的私货；
//   · 反过来：`engine/` 绝不 include `game/`（`PLAN` A.3 的单向依赖）。
//
// ⚠️ 三条硬约定（违反任何一条，出的都是**只有人眼看得见**的错）：
//   1. **"什么是墙"只走 `world::IsSolidAt`** —— 它把"越界"也判成实心（S7 决策）
//      ⇒ "越界 = 墙"在这里**天然成立**。别在本文件里另写一份 `tile == Tile::Wall`。
//   2. 输入**不 clamp**（`col` 越出 `[0, cols)` 也照样算），但**输出必须有限**：
//      贴脸 ⇒ `perp` 夹到 `kMinPerp`；一路没撞到 ⇒ 给大数并把 `hit` 置 false。
//   3. `RayCamera::dir` **由调用方保证是单位向量**（`plane ⊥ dir` + `|dir| == 1` 是下面
//      "垂距 = DDA 参数 t"那条推导的前提）。本文件**不**替调用方归一化：
//      那会掩盖调用方的错，而且每列多一次 sqrt。
// ============================================================

namespace mini::render {

// ⚠️ `RayCamera` 与 `kMinPerp` **不在这里** —— S13 第 1 笔搬到了 `render/RayCamera.h`
//    （分叉 A-2：让 `Sprite.h` 只依赖相机、不依赖 `TileMap`/`Collision`）。
//    本文件仍 include 它 ⇒ `#include <mini/render/RayCaster.h>` 的老调用点一行都不用改。

// 一条列射线的结果。
//
// ⚠️ `dist` 与 `perp` **不是一回事** —— 这是本文件最容易被写错的地方：
//    · `perp` = 沿 `cam.dir` 的**垂距** ⇒ **画墙高度只用它**（用 `dist` 就是鱼眼，坑 2）；
//    · `dist` = 欧氏**斜距**（≥ `perp`）⇒ 留给"按真实距离做衰减"（火把/雾）那一类用途。
struct RayHit {
    float dist  = 0.0f;
    float perp  = 0.0f;
    int   tileX = 0;
    int   tileY = 0;
    bool  sideX = true;    // true = 跨过一条**竖**格线（打到东西向的墙面）
    bool  hit   = false;   // false = 走到步数上限都没撞到（病态输入；调用方按"没墙"处理）
};

// 内部哨兵值。刻意**不用** `inf`：`inf` 参与减法/乘法会变成 NaN（坑 5）。
inline constexpr float kRayHuge = 1.0e30f;
inline constexpr float kRayFar  = 1.0e6f;

// 明暗档数 + 距离阈值。
// ⚠️ 阈值是**观感参数**，不是物理量（换字体/字号/窗口都可能要调）；但**必须单调**（近亮远暗）
//    ⇒ 有用例钉（`ShadeIndexIsMonotonic`）。
inline constexpr int                  kShadeCount = 4;
inline constexpr std::array<float, 3> kShadeThresholds{2.0f, 5.0f, 9.0f};
static_assert(kShadeThresholds.size() == static_cast<std::size_t>(kShadeCount) - 1,
              "明暗阈值个数必须是档数 − 1（否则 ShadeIndex 的档位会与 kShadeCount 对不上）");

// ------------------------------------------------------------
// 单列射线：DDA 走格（Amanatides & Woo 的那套，Lode 的教程里也叫 ray casting）
// ------------------------------------------------------------
// `col ∈ [0, cols)`，内部 `cameraX = 2*col/cols − 1` ∈ [−1, 1]。
// ⚠️ 边界列（`cameraX = ±1`）是**合法**的：那是视锥的最左/最右一列，不是错误输入。
[[nodiscard]] inline RayHit CastColumn(const world::TileMap& map, const RayCamera& cam,
                                       int col, int cols) noexcept
{
    RayHit hit{};
    if (cols <= 0) {
        return hit;   // 防御：列数非法 ⇒ 返回"没命中"（调用方越界是它的 bug，但不该在这里 UB）
    }

    // 相机平面 = `dir` 逆时针转 90°（|plane| = 1 因为它是由单位向量转出来的）
    // ★ 这一点是下面"垂距 = 参数 t"的**全部依据**：别把它换成一个带 aspect 的 plane。
    const mini::math::Vec2 plane{-cam.dir.y, cam.dir.x};
    const float            cameraX = 2.0f * static_cast<float>(col) / static_cast<float>(cols) - 1.0f;
    const mini::math::Vec2 rayDir{cam.dir.x + plane.x * cam.fovTan * cameraX,
                                  cam.dir.y + plane.y * cam.fovTan * cameraX};

    int mapX = world::TileMap::ToIndex(cam.pos.x);
    int mapY = world::TileMap::ToIndex(cam.pos.y);

    // 在该轴上"跨一格"要走的参数距离。
    // ⚠️ 分量恰为 0（正东/正南）时用 `kRayHuge`，**不要**用 `1/0 = inf`：
    //    `inf * 0` 会得到 NaN，后面的比较就全乱了。
    const float deltaDistX = (rayDir.x == 0.0f) ? kRayHuge : std::abs(1.0f / rayDir.x);
    const float deltaDistY = (rayDir.y == 0.0f) ? kRayHuge : std::abs(1.0f / rayDir.y);

    const int stepX = (rayDir.x < 0.0f) ? -1 : 1;
    const int stepY = (rayDir.y < 0.0f) ? -1 : 1;

    // ⚠️ 初值 = 到**下一条格线**的参数距离（写错 ⇒ 墙面有缝/错位，坑 3）：
    //    往 + 方向走 ⇒ 目标是 `mapX + 1`；往 − 方向走 ⇒ 目标是 `mapX`。
    float sideDistX = (stepX > 0) ? (static_cast<float>(mapX + 1) - cam.pos.x) * deltaDistX
                                  : (cam.pos.x - static_cast<float>(mapX)) * deltaDistX;
    float sideDistY = (stepY > 0) ? (static_cast<float>(mapY + 1) - cam.pos.y) * deltaDistY
                                  : (cam.pos.y - static_cast<float>(mapY)) * deltaDistY;

    // 步数上限 = 宽 + 高：一条射线在地图上最多跨这么多格 ⇒ 必然终止（坑 6 的兜底）
    const int maxSteps = map.Width() + map.Height();

    float t = 0.0f;   // 走到这一步的**参数距离**（也 = 垂距，见下面的推导）
    for (int i = 0; i < maxSteps; ++i) {
        if (sideDistX < sideDistY) {
            t         = sideDistX;
            sideDistX += deltaDistX;
            mapX      += stepX;
            hit.sideX  = true;
        } else {
            t         = sideDistY;
            sideDistY += deltaDistY;
            mapY      += stepY;
            hit.sideX  = false;
        }

        if (world::IsSolidAt(map, mapX, mapY)) {   // "越界 = 墙"天然含在这条判据里（约定 1）
            hit.hit   = true;
            hit.tileX = mapX;
            hit.tileY = mapY;
            break;
        }
    }

    if (!hit.hit) {
        hit.dist = kRayFar;
        hit.perp = kRayFar;
        return hit;   // 病态输入：调用方按"没墙"处理（别拿这个距离去画墙）
    }

    // ★★ 鱼眼校正就发生在这两行（分叉 C 的核心，坑 2）：
    //   因为 `plane ⊥ dir` 且 `|dir| = 1` ⇒ `rayDir · dir == 1`（**恒等**）
    //   ⇒ "沿 dir 的垂距"**恰好等于**参数 `t`（它同时也 = 欧氏斜距 × cos(射线与 dir 的夹角)）。
    //   ⇒ 所以：`perp = t`（**不要再乘一次 cos**：再乘一次是过校正，画面会凹进去）；
    //           `dist = t × |rayDir|`（欧氏斜距，留给将来的距离衰减用）。
    //   ⚠️ 这一行就是 `tests/test_raycaster.cpp` 里 `PerpIsEqualAcrossColumns` 钉的东西：
    //      同一面墙在不同列上的 `perp` **必须相等**（不相等 = 鱼眼）。
    const float lenRay = std::sqrt(rayDir.x * rayDir.x + rayDir.y * rayDir.y);

    // ⚠️ 用 `(x > kMinPerp) ? x : kMinPerp` 而**不是** `std::max`：NaN 走 else 分支 ⇒ 结果确定
    //    （与 `SweepStepCount` / `DirectionToFacing` 同一写法）
    hit.perp = (t > kMinPerp) ? t : kMinPerp;
    hit.dist = (t * lenRay > kMinPerp) ? t * lenRay : kMinPerp;
    return hit;
}

// ------------------------------------------------------------
// 墙高：垂距 → 屏幕**行数**（纯数学）
// ------------------------------------------------------------
// ⚠️ 它**不认识** `kHudRow` / 顶栏 / HUD：地平线放在哪、超出视口怎么裁，都是调用方（`game`）的事。
// ⚠️ `cellAspect` 是**观感参数**（终端字符格 ≈ 1:2 的宽高比），**不是物理量**：
//    换字体/字号/窗口都可能要调它（坑 18）。
// ⚠️ 返回值**至少 1 行**：远墙算出来常是 0.x 行，直接 floor 会变成 0 ⇒ 那面墙在屏幕上"消失"，
//    看起来像空洞（坑 17）。
[[nodiscard]] inline float WallHeightRows(float perp, int viewH, float cellAspect) noexcept
{
    if (viewH <= 0) {
        return 1.0f;
    }

    const float p    = (perp > kMinPerp) ? perp : kMinPerp;
    const float rows = static_cast<float>(viewH) / p * cellAspect;
    return (rows > 1.0f) ? rows : 1.0f;
}

// ------------------------------------------------------------
// 明暗档位：垂距 → `[0, kShadeCount)`
// ------------------------------------------------------------
// ⚠️ **单调（近亮远暗）是契约**：调用方可能用"字形 + 颜色"两套手段表达同一档，
//    两套必须落在同一档上，否则会出现"字形说近、颜色说远"这种自相矛盾的画面。
[[nodiscard]] inline int ShadeIndex(float perp) noexcept
{
    const float p = (perp > kMinPerp) ? perp : kMinPerp;

    int idx = 0;
    for (const float threshold : kShadeThresholds) {
        if (!(p >= threshold)) {   // 写成取反：NaN 走 break ⇒ 落在第 0 档（确定、且不会越界）
            break;
        }
        ++idx;
    }
    return idx;
}

} // namespace mini::render
