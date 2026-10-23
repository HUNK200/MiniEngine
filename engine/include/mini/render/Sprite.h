#pragma once

#include "mini/math/Vec2.h"
#include "mini/render/RayCamera.h"   // RayCamera + kMinPerp（S13 A-2：从 RayCaster.h 搬来）

#include <cmath>   // std::floor

// ============================================================
// Sprite.h —— 精灵（"贴在世界上某处、有高度"的东西）→ 屏幕矩形的**纯数学**（S13 第 1 笔）
//
// 三样东西，全都**不认识** `FrameBuffer` / `Entity` / `game` / HUD：
//   SpritePlace     输入：世界坐标 + **脚底**高度 z + 世界宽高（格）
//   SpriteBox       输出：屏幕矩形（**半开区间**）+ 深度 perp + visible
//   ProjectSprite   唯一入口：世界 → 屏幕（**不画**，也不做遮挡判断）
//
// 为什么拆成纯函数（分叉 A = 甲，与 S12 的 `RayCaster.h` 同一套路数）：
//   · 只依赖 `math/Vec2.h` + `render/RayCamera.h` ⇒ **离线可单测**（`tests/test_sprite.cpp`，8 条）；
//   · 精灵与墙必须共用**同一套尺度/同一个深度口径**，纯函数是唯一能把这件事钉住的地方；
//   · 反过来：`engine/` 绝不 include `game/`（PLAN A.3 的单向依赖）。
//
// ⚠️ 五条硬约定（违反任何一条，出的都是"只有人眼看得见"的错）：
//   1. **深度只用 `perp`**（沿 `cam.dir` 的垂距，与 `RayHit::perp` **同口径**）。
//      用斜距 `dist` 会让屏幕两侧的精灵误判遮挡（坑 2）⇒ 本文件只算 `forward`，不算斜距。
//   2. **尺寸从墙的尺度推**：`lineH = viewH / perp * aspect` 与 `WallHeightRows` 是**同一个式子**；
//      横向 = 行数 × `cellHeightPerWidth`（物理上"方"）。**不许**用几何公式
//      `cols/(2·fovTan·perp)` 算宽度 —— 那会让精灵横向胖 2.0×（坑 4b）。几何公式**只**用于定位（centerCol）。
//   3. **`z` 是脚底**（不是中心）：`row1`（底边）= 地面行 − `z × rowsPerCell`。
//      调用方要让"中心"在某个高度时，自己减去 `cellsTall / 2`（分叉 D-2 甲）。
//   4. **裁剪在这里做完**（分叉 R4 甲）：输出保证 `col ∈ [0, cols)`、`row ∈ [viewTop, viewTop+viewH)`。
//      ⚠️ 别指望调用方补裁 —— `FrameBuffer::Set` 会**静默忽略**越界，画到 HUD 上也只是"被后画的 HUD 抹掉"，
//      两种都是**看不见的错**。
//   5. **行数至少 1**（坑 15）：远处的子弹 `cellsTall ≈ 0.2` ⇒ 取整后 0 行 ⇒ 整颗消失。
//      ⚠️ **列数不兜底**：宽度不足 1 列的精灵确实看不见（比一个字符还小）—— 这是有意的，
//      补它会让"很远的敌人"变成一个与距离无关的亮点。
// ============================================================

namespace mini::render {

// 输入：一个"贴在世界上某处、有高度"的精灵（**只读**，不认识 Entity/game）
struct SpritePlace {
    mini::math::Vec2 pos{};        // 世界坐标（格，含亚格精度）
    float            z = 0.0f;     // ★ **脚底**离地高度（格）；0 = 站在地上（分叉 D-2 甲）
    float            cellsTall = 1.0f;   // 世界高度（格）；1 = 一格高（分叉 C 甲：= 半条墙带）
    float            cellsWide = 1.0f;   // 世界宽度（格）
};

// 输出：落到屏幕上的矩形（**半开区间**：列 `[col0, col1)`、行 `[row0, row1)`）+ 深度
// ★ 出来时**已经在视口内**（约定 4）、且**至少 1 行**（约定 5）
struct SpriteBox {
    int   col0 = 0, col1 = 0;
    int   row0 = 0, row1 = 0;
    float perp = 0.0f;             // ★ 与墙**同一个**深度口径（垂距），用于比较
    bool  visible = false;         // false = 相机背后 / 贴脸 / 裁完为空 / 输入非法
};

namespace detail {

// 正数 ⇒ 截断即 floor；这里显式写 floor，是因为**负数也要向下**（贴脸时 col0 会为负）
[[nodiscard]] inline int FloorToInt(float v) noexcept
{
    return static_cast<int>(std::floor(v));
}

} // namespace detail

// 唯一入口：世界 → 屏幕（**不认识** FrameBuffer，也不画）
// ⚠️ 观感参数必须**从外面喂**（`aspect` / `cellHeightPerWidth`），引擎不认识 Game.cpp 的策略
//    （与 `RayCaster::CastColumn/WallHeightRows` 收 `cellAspect` 是同一条纪律）
[[nodiscard]] inline SpriteBox ProjectSprite(const SpritePlace& place, const RayCamera& cam,
                                             int cols, int viewTop, int viewH,
                                             float aspect, float cellHeightPerWidth,
                                             float wallHeightCells) noexcept
{
    SpriteBox box{};

    // ── 0. 输入防御：非法参数一律"不可见"，而不是算出一个巨大的矩形 ──────────
    if (cols <= 0 || viewH <= 0) {
        return box;
    }
    if (!(aspect > 0.0f) || !(cellHeightPerWidth > 0.0f) || !(wallHeightCells > 0.0f)) {
        return box;
    }
    if (!(cam.fovTan > 0.0f)) {
        return box;   // 视野为 0 ⇒ colScale 是 inf ⇒ 后面的 floor 会 UB
    }

    // ── 1. 深度：沿 dir 的**垂距**（约定 1）──────────────────────────────────
    // ⚠️ 写成 `!(forward > kMinPerp)`：NaN 走 false ⇒ 返回不可见（确定行为，同 RayCaster 的写法）
    const math::Vec2 toSprite = place.pos - cam.pos;
    const float      forward  = toSprite.Dot(cam.dir);
    if (!(forward > kMinPerp)) {
        return box;   // 相机背后 / 贴脸 / NaN
    }

    // ── 2. 垂直方向：与墙**同一尺度**（约定 2）──────────────────────────────
    const float horizon    = static_cast<float>(viewTop) + static_cast<float>(viewH) * 0.5f;
    const float lineH      = static_cast<float>(viewH) / forward * aspect;   // 该距离下一整面墙的行数
    const float rowsPerCell = lineH / wallHeightCells;                       // 1 个世界格 = 多少行

    const float row1F = horizon + lineH * 0.5f - place.z * rowsPerCell;       // 底边 = 地面行 − z
    const float row0F = row1F - place.cellsTall * rowsPerCell;                // 顶边 = 底边 − 身高

    // ── 3. 横向：**尺寸**从墙的尺度推（约定 2），**位置**才用几何 ───────────────
    const math::Vec2 plane{-cam.dir.y, cam.dir.x};
    const float colsWide = place.cellsWide * rowsPerCell * cellHeightPerWidth;

    // 只有横向位置用 fovTan：否则精灵与墙在屏幕上对不齐（墙的列从 cameraX 定义来）
    const float centerCol = static_cast<float>(cols) * 0.5f +
                            (toSprite.Dot(plane) / forward) *
                                (static_cast<float>(cols) / (2.0f * cam.fovTan));
    const float col0F = centerCol - colsWide * 0.5f;
    const float col1F = col0F + colsWide;

    // ── 4. 裁剪到视口（约定 4）：**先夹 float、再取整** ─────────────────────
    // 为什么先夹：夹 int 会把"完全在视口外"的东西也压成 1 行/1 列，假可见。
    const float viewTopF    = static_cast<float>(viewTop);
    const float viewBottomF = static_cast<float>(viewTop + viewH);

    const float topF    = (row0F < viewTopF) ? viewTopF : row0F;
    const float bottomF = (row1F > viewBottomF) ? viewBottomF : row1F;
    if (!(bottomF > topF)) {
        return box;   // 完全在视口上方/下方
    }

    int row0 = detail::FloorToInt(topF);
    int row1 = detail::FloorToInt(bottomF);

    // 约定 5：至少 1 行（投影高度不足 1 行时 floor 会把上下边压成同一行）
    if (row1 - row0 < 1) {
        row1 = row0 + 1;
        if (row1 > viewTop + viewH) {   // 恰好顶在视口最底 ⇒ 往回让一行（不越界）
            row1 = viewTop + viewH;
            row0 = row1 - 1;
        }
    }

    const float leftF  = (col0F < 0.0f) ? 0.0f : col0F;
    const float rightF = (col1F > static_cast<float>(cols)) ? static_cast<float>(cols) : col1F;
    if (!(rightF > leftF)) {
        return box;   // 完全在视口左/右之外（或宽度不足 1 列 —— 见约定 5 的注）
    }

    const int col0 = detail::FloorToInt(leftF);
    const int col1 = detail::FloorToInt(rightF);
    if (col1 <= col0) {
        return box;   // 宽度不足 1 列：确实看不见（刻意不兜底）
    }

    // ── 5. 出口：所有量都已有限且在视口内 ─────────────────────────────────
    box.col0  = col0;
    box.col1  = col1;
    box.row0  = row0;
    box.row1  = row1;
    box.perp  = forward;
    box.visible = true;
    return box;
}

} // namespace mini::render
