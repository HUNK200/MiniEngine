#pragma once

#include "mini/math/MathUtils.h"   // FloorToInt / Clamp（只写真正用到的那两个）
#include "mini/math/Vec2.h"        // Vec2

// ============================================================
// render/Camera.h —— 视口在世界里的位置（S6）
//
// 这一层干什么：回答**一个问题** ——"现在这一屏，是从世界的第几格开始看的？"
//   **它不读输入、不画东西、不认识终端、不认识 TileMap** → 可以完全离线单测
//   （施工图 6.1；PLAN:989 的 E-8 明确要求"写一个坐标转换的单元测试"）。
// 依赖谁：math（Vec2 / FloorToInt / Clamp）+ 标准库。
//   ⛔ **不 include `world/`**：`TileMap::ToIndex` 本来就是 `math::FloorToInt` 的转发
//      （TileMap.h:72-75），绕道 world 只会多一条 render → world 的跨层边（PLAN A.4）。
//      这条是本阶段**唯一的新边**：render → math（向下，安全）。
// 被谁用：game::Game（每帧跟随 + 渲染取整原点）；tests/test_camera.cpp。
//
// ⚠️ 本阶段**没有平滑**（2026-10-02 决定走 snap，见 S6_CHECKLIST 第 8 节）：
//    `Follow` 是纯函数，`pos` 直接等于夹取后的目标 —— 无隐藏状态，
//    因此它同时也是 SnapTo：S10 的玩家瞬移天生正确。
//    升级路径（死区 / 橡皮筋）只改 `Follow` 内部，接口不变。
//
// ⚠️ 坐标约定（PLAN A.5 + S6_CHECKLIST 7.2）：**屏幕格 ↔ 世界格共用同一个整数原点**。
//    相机 pos 是浮点，但渲染只用 `OriginCol()/OriginRow()`（= floor(pos)）——
//    于是取整在整个 S6 里**只发生一次**，地图与实体不会各用一条取整路径（那就是 E-8）。
//
// ⚠️ 输入约束（沿用 math 的既定约定，这里**不重复防御**）：
//    `worldPos` 必须是**有限值**，`limits` 的尺寸应来自真实的地图 / 画布。
//    真出现 NaN 要去它产生的地方查（MathUtils.h 顶部约定 1），不要在这里兜。
// ============================================================

namespace mini::render {

// 视口与地图的尺寸（单位：格）。**由调用方每帧提供，Camera 不持有**（S6_CHECKLIST 6.3）。
//
// 为什么不持有：尺寸的真实来源是 `FrameBuffer`（画布宽高）与 `TileMap`（地图宽高）。
// 再抄一份进 Camera 就等于同一份事实有三个来源，任何一处忘了同步（比如 resize 时
// 漏调一次 setter）都会变成"画面错位"，而三处单看都像是对的 —— 很难查。
struct CameraLimits {
    int viewW = 0;   // 可用视口宽（列）
    int viewH = 0;   // 可用视口高（行）—— **由调用方减掉自己那些 HUD/顶栏占的行**，Camera 不猜
                     //（`game` 传的是 `画布高 − Game::kHudRow`；S9 步骤 5 起 kHudRow = 2）
                     // ⚠️ S6 起这句话原本写的是"不含引擎顶栏那一行"，S9 步骤 5 改成两行 HUD 之后
                     //    已经不准：Camera 只管"多少行"，具体被谁占了是**调用方的事**。
    int mapW  = 0;   // 世界宽（格）
    int mapH  = 0;   // 世界高（格）
};

class Camera {
public:
    // snap 跟随：逐轴独立地令 `pos = clamp(world - view/2, 0, map - view)`。
    //
    //   · `view / 2` 用**整数除法**：奇数宽下若用 `view * 0.5f`，`@` 会在相邻两列之间
    //     反复跳（施工图坑 4）—— 80 宽看不出差别，81 宽就会露出来，所以用例 9 专门钉它。
    //   · `view <= 0` 或 `view >= map` 的轴 → pos 取 **0**（退化：贴左上）。
    //     必须**在这里分支**，不能把空区间喂给 `Clamp`：它明确"假定 lo <= hi，调用方负责"
    //     （MathUtils.h:33-35），`view >= map` 时 lo = 0 > hi = map - view 就是违约
    //     （施工图坑 1、2；这是 Clamp 那条前置条件第一次真的会被踩到）。
    //   · 不抛、不分配、无副作用（noexcept）。**没有"上一帧"这个概念** → 同输入必同输出
    //     （用例 14 钉这条；将来加平滑时这条纪律会变，那时用例也要跟着改）。
    void Follow(mini::math::Vec2 worldPos, const CameraLimits& limits) noexcept
    {
        if (limits.viewW <= 0 || limits.viewH <= 0) {
            m_pos = mini::math::Vec2{};   // 极小窗口 / 未初始化：别让 Clamp 收空区间
            return;
        }

        m_pos.x = FollowAxis(worldPos.x, limits.viewW, limits.mapW);
        m_pos.y = FollowAxis(worldPos.y, limits.viewH, limits.mapH);
    }

    // 渲染用的**整数原点** = floor(pos)。地图与实体**都必须用它**（施工图 7.2 的硬规矩）：
    //   地图：屏幕第 c 列显示世界第 (OriginCol() + c) 格
    //   实体：屏幕列 = ToIndex(entity.pos.x) - OriginCol()   ← 同一条规则的另一种写法
    // 生产调用者：Game::DrawMap / Game::DrawEntities。
    //
    // 为什么**只公开这个**、不公开 `ToScreenCol(world)`：地图那边遍历的是屏幕格，
    // "某个世界坐标画在哪一列"对它没用；两边各写一条取整路径正是 E-8 的成因（施工图 6.4）。
    //
    // ⚠️ `Follow` 之后 pos 必然落在 [0, map-view]（或退化的 0），所以这里传给 FloorToInt
    //    的值一定有限且有界 —— 它那条"|v| < 2^31 且非 NaN"的输入约束自动满足。
    [[nodiscard]] int OriginCol() const noexcept { return mini::math::FloorToInt(m_pos.x); }
    [[nodiscard]] int OriginRow() const noexcept { return mini::math::FloorToInt(m_pos.y); }

    // 相机的浮点位置。**为什么存在**（TECH_DEBT B5 的纪律：只有测试调用的接口也要能回答这句）：
    //   ① 单测要断言"居中 / 夹取"的精确值（Origin* 只有整数，看不出夹取边界差半格）；
    //   ② 将来加死区/橡皮筋、以及 S9 的 ScreenToWorld 都要从它出发。
    // ⚠️ 生产代码目前**不直接调它**（渲染走 Origin*）—— 别因为"没人调"就删。
    [[nodiscard]] mini::math::Vec2 Pos() const noexcept { return m_pos; }

    // ── 契约（**S6 不实现**；S9 需要瞄准/拾取时再加，加时必须带用例）─────────────
    //   ScreenToWorld(col, row) = pos + {col + 0.5, row + 0.5}   ← 那一格的**中心**
    //   它不是 floor 的精确逆运算：半格偏移由 +0.5 表达，反算回去只能拿到格中心
    //   （PLAN A.5 原文已写明这一点）。
    // [[nodiscard]] mini::math::Vec2 ScreenToWorld(int col, int row) const noexcept;

    // ⛔ 刻意**不提供**（S6_CHECKLIST 6.2）：
    //   IsDegenerate() —— 退化行为已在 Follow 里定死（该轴取 0），界面上不需要查询
    //   Reset()        —— Follow 就是 snap，没有需要复位的状态
    //   target 字段    —— snap 下它是临时量（0 调用者）；加平滑时再加

private:
    // 单轴跟随。抽出来是因为 x / y 的规则完全一样（"逐轴独立夹取"是用例 4 钉的性质）。
    [[nodiscard]] static float FollowAxis(float worldCoord, int view, int map) noexcept
    {
        if (view >= map) {
            return 0.0f;   // 退化：视口不小于地图 → 贴左上（见 Follow 的注释）
        }

        const float target = worldCoord - static_cast<float>(view / 2);   // 整数除法
        return mini::math::Clamp(target, 0.0f, static_cast<float>(map - view));
    }

    mini::math::Vec2 m_pos{};
};

// 编译期钉住"没有偷偷长胖"（与 S5 的 sizeof(Entity) == 24 同一套路）。
static_assert(sizeof(CameraLimits) == 16, "CameraLimits 应恰好是 4 个 int 的 POD");
static_assert(sizeof(Camera) == 8, "Camera 只应有一个 Vec2（8 字节）");

} // namespace mini::render
