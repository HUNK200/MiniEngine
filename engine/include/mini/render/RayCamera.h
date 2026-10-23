#pragma once

#include "mini/math/Vec2.h"

// ============================================================
// RayCamera.h —— 伪 3D 的**相机与贴脸兜底**（S13 第 1 笔，分叉 A-2：从 `RayCaster.h` 搬来）
//
// 为什么单独一个头（而不是继续留在 `RayCaster.h` 里）：
//   `raycaster` 与 `sprite` 是**两件事**，但要用**同一个深度口径**。
//   `Sprite.h` 只需要"相机 + kMinPerp"，**不需要** `TileMap` / `Collision`
//   （那是 `CastColumn` 的依赖）⇒ 把这两样搬出来，`Sprite.h` 的 include 图里
//   就没有地图与碰撞：纯数学、离线可测、将来别的渲染器也能直接复用。
//
// ⚠️ 硬约定（违反的任何一条，出的都是"只有人眼看得见"的错）：
//   1. `RayCamera::dir` **由调用方保证是单位向量** —— `perp` 的定义
//      （"沿 dir 的垂距"）依赖 `|dir| == 1`；本文件不替调用方归一化
//      （那会掩盖调用方的错，而且每列多一次 sqrt）。
//   2. `kMinPerp` 是**贴脸兜底**：深度不许是 0，否则墙高/精灵尺寸算成无穷 ⇒ 行数爆 ⇒ 越界写。
//      `raycaster` 与 `sprite` **必须用同一个值**，否则"墙 vs 精灵"的比较会差一截 ——
//      这正是它俩共用本文件的原因。
//   3. 这里**不认识** `FrameBuffer` / HUD / 顶栏 / 终端，也不 include 任何 `game/` 的东西。
// ============================================================

namespace mini::render {

// 相机：三个字段就够。`pos` 是**亚格精度**的世界坐标（= 玩家中心的浮点位置）。
struct RayCamera {
    mini::math::Vec2 pos{};              // 世界坐标（格，float）
    mini::math::Vec2 dir{1.0f, 0.0f};    // ⚠️ 单位向量（调用方保证）—— `perp` 的推导依赖它
    float            fovTan = 0.66f;     // 相机平面半长 ⇒ 0.66 约等于 66° FOV
};

// 贴脸兜底：`perp` 不许是 0（否则墙高算成无穷 ⇒ 行数爆 ⇒ 越界写，`RayCaster` 坑 5）。
// ⚠️ 精灵侧同样用它（见本文件顶部约定 2）：`forward <= kMinPerp` ⇒ 直接不画。
inline constexpr float kMinPerp = 0.01f;

} // namespace mini::render
