#include "MiniTest.h"

#include <mini/math/MathUtils.h>
#include <mini/render/Camera.h>

// ------------------------------------------------------------
// render/Camera 的单元测试（S6 步骤 1）
//
// 这个文件**不需要终端**：Camera 不读输入、不画东西、不认识 TileMap —— 整层都能离线验证
//（S2/S3 那条"能离线测的就离线测"的延续，TECH_DEBT C7）。
//
// 尺寸取 game 的真实数字：地图 200×200，画布 80×24，其中第 0 行留给引擎顶栏
//（`Game::kHudRow = 1`）→ **可用视口 80×23**；玩家速度 20 格/秒（不需要，这里只测静态夹取）。
//
// ⚠️ 两条写用例时的纪律（都是前几个阶段踩出来的）：
//   · `Vec2` **没有 `operator<<`**（Vec2.h 刻意不提供）→ 不能把它塞进 `MINI_CHECK_EQ`，
//     逐分量用 `MINI_CHECK_NEAR` 比（`MINI_CHECK_EQ` 失败时要 ToString 两侧的值）。
//   · `MINI_CHECK_EQ / _NEAR` 的参数里若含**顶层逗号**要自己多包一层圆括号
//     （TECH_DEBT C9；花括号不保护逗号）。本文件里的 `Vec2{...}` 都落在函数调用圆括号内，
//     或者先存进局部变量，所以不需要那层括号。
// ------------------------------------------------------------

namespace {

using mini::math::FloorToInt;
using mini::math::Vec2;
using mini::render::Camera;
using mini::render::CameraLimits;

constexpr int kMapW  = 200;   // Game::kWorldW
constexpr int kMapH  = 200;   // Game::kWorldH
constexpr int kViewW = 80;    // 画布 80 列
constexpr int kViewH = 23;    // 画布 24 行 − 顶栏 1 行（Game::kHudRow）

CameraLimits Limits80x23()
{
    return CameraLimits{kViewW, kViewH, kMapW, kMapH};
}

// 逐分量近似相等。存在的理由：`Vec2` 刻意没有 `operator==`（浮点精确相等几乎总是错的）
// 也没有 `operator<<`，所以这里替它做一次"多近算相等"。
bool SameVec(Vec2 a, Vec2 b, float eps = 1e-5f)
{
    return mini::math::ApproxEquals(a.x, b.x, eps) && mini::math::ApproxEquals(a.y, b.y, eps);
}

// 地图的绘制规则（game 侧）：世界第 t 格画在屏幕第 (t − OriginCol()) 列。
// 写成函数是为了让用例里"实体列"和"tile 列"能用**同一条规则的两种写法**对照。
int ColOfTile(const Camera& cam, int tile)
{
    return tile - cam.OriginCol();
}

int RowOfTile(const Camera& cam, int tile)
{
    return tile - cam.OriginRow();
}

} // namespace

// ------------------------------------------------------------
// 1. 居中：地图中间时，视口原点 = 玩家 − 视口/2
// ------------------------------------------------------------
MINI_TEST(Camera, FollowCentersPlayerInOpenArea)
{
    Camera cam;
    cam.Follow(Vec2{100.5f, 100.5f}, Limits80x23());

    const Vec2 p = cam.Pos();
    MINI_CHECK_NEAR(p.x, 60.5f, 1e-5f);   // 100.5 − 80/2
    MINI_CHECK_NEAR(p.y, 89.5f, 1e-5f);   // 100.5 − 23/2（整数除法 → 11，不是 11.5）

    MINI_CHECK_EQ(cam.OriginCol(), 60);
    MINI_CHECK_EQ(cam.OriginRow(), 89);
}

// ------------------------------------------------------------
// 2. 左下角夹取：开局 (2.5, 2.5) 时视口贴左上，动不了
// ------------------------------------------------------------
MINI_TEST(Camera, ClampAtLeftTopCorner)
{
    Camera cam;
    cam.Follow(Vec2{2.5f, 2.5f}, Limits80x23());   // Game::kSpawnX / kSpawnY

    MINI_CHECK(SameVec(cam.Pos(), Vec2{0.0f, 0.0f}));
    MINI_CHECK_EQ(cam.OriginCol(), 0);
    MINI_CHECK_EQ(cam.OriginRow(), 0);

    // 玩家仍然看得见：格 2 落在第 2 列（视口未被夹取时它会在第 40 列）
    MINI_CHECK_EQ(ColOfTile(cam, FloorToInt(2.5f)), 2);
}

// ------------------------------------------------------------
// 3. 右下角夹取：走到地图尽头时视口停住，玩家贴到画面最后一列/行
// ------------------------------------------------------------
MINI_TEST(Camera, ClampAtRightBottomCorner)
{
    Camera cam;
    cam.Follow(Vec2{199.5f, 199.5f}, Limits80x23());

    MINI_CHECK_NEAR(cam.Pos().x, 120.0f, 1e-5f);   // 200 − 80
    MINI_CHECK_NEAR(cam.Pos().y, 177.0f, 1e-5f);   // 200 − 23

    const int tile = FloorToInt(199.5f);
    MINI_CHECK_EQ(ColOfTile(cam, tile), kViewW - 1);   // 79：画面最后一列
    MINI_CHECK_EQ(RowOfTile(cam, tile), kViewH - 1);   // 22：画面最后一行
}

// ------------------------------------------------------------
// 4. 两轴独立：一轴被夹住不影响另一轴居中
// ------------------------------------------------------------
MINI_TEST(Camera, AxesClampIndependently)
{
    Camera cam;
    cam.Follow(Vec2{199.5f, 100.5f}, Limits80x23());

    MINI_CHECK_NEAR(cam.Pos().x, 120.0f, 1e-5f);   // x 被右上界夹住
    MINI_CHECK_NEAR(cam.Pos().y, 89.5f, 1e-5f);    // y 仍然居中（100.5 − 11）
}

// ------------------------------------------------------------
// 5 / 6. 退化：视口不小于地图 → 该轴贴左上（不能靠 Clamp 的越界行为兜）
// ------------------------------------------------------------
MINI_TEST(Camera, DegenerateWhenViewEqualsMap)
{
    Camera cam;
    cam.Follow(Vec2{50.0f, 50.0f}, CameraLimits{80, 23, 80, 23});

    MINI_CHECK(SameVec(cam.Pos(), Vec2{0.0f, 0.0f}));
}

MINI_TEST(Camera, DegenerateWhenViewLargerThanMap)
{
    Camera cam;
    cam.Follow(Vec2{50.0f, 50.0f}, CameraLimits{100, 30, 80, 23});

    MINI_CHECK(SameVec(cam.Pos(), Vec2{0.0f, 0.0f}));
}

// ------------------------------------------------------------
// 7. 极小 / 非法视口不崩、退回 (0,0)
//    为什么必须有这条：Clamp 的前置条件是 lo <= hi，而 view <= 0 时区间是空的
//    （MathUtils.h:33-35）。Application 虽然在 width/height < 2 时跳过 resize，
//    但"拖窗口到极窄"会连续触发，这里是第二道闸。
// ------------------------------------------------------------
MINI_TEST(Camera, ZeroViewportIsSafe)
{
    Camera cam;
    cam.Follow(Vec2{100.5f, 100.5f}, Limits80x23());   // 先放一个正常状态

    cam.Follow(Vec2{100.5f, 100.5f}, CameraLimits{0, 23, 200, 200});
    MINI_CHECK(SameVec(cam.Pos(), Vec2{0.0f, 0.0f}));

    cam.Follow(Vec2{100.5f, 100.5f}, CameraLimits{80, 0, 200, 200});
    MINI_CHECK(SameVec(cam.Pos(), Vec2{0.0f, 0.0f}));

    cam.Follow(Vec2{100.5f, 100.5f}, CameraLimits{-5, -5, 200, 200});
    MINI_CHECK(SameVec(cam.Pos(), Vec2{0.0f, 0.0f}));
}

// ------------------------------------------------------------
// 8. 取整是 **floor**，不是四舍五入
//    ⚠️ 落地偏差 1：施工图原本写"含负数 pos（把相机推到地图外）"，但 Follow 的
//    pos 恒落在 [0, map-view] → **负数分支不可达** → 用例改成钉"小数部分是向下取整"
//    （负数那一半由 test_world.cpp 的 TileMap::ToIndex 编译期断言覆盖）。
// ------------------------------------------------------------
MINI_TEST(Camera, OriginIsFloorOfFractionalPos)
{
    Camera cam;

    cam.Follow(Vec2{50.7f, 100.5f}, Limits80x23());   // pos.x = 10.7
    MINI_CHECK_NEAR(cam.Pos().x, 10.7f, 1e-5f);
    MINI_CHECK_EQ(cam.OriginCol(), 10);               // 不是 11

    cam.Follow(Vec2{50.999f, 100.5f}, Limits80x23()); // pos.x = 10.999
    MINI_CHECK_EQ(cam.OriginCol(), 10);               // 不越过整数边界

    cam.Follow(Vec2{50.0f, 100.5f}, Limits80x23());   // pos.x = 10.0（整数时不该退一）
    MINI_CHECK_EQ(cam.OriginCol(), 10);

    cam.Follow(Vec2{100.5f, 89.5f}, Limits80x23());   // pos.y = 78.5 → 小数部分是 .5
    MINI_CHECK_EQ(cam.OriginRow(), 78);
}

// ------------------------------------------------------------
// 9. 奇数视口宽：居中偏移必须用整数除法，否则 @ 会在相邻两列之间跳
//    这里用 81 列把差别逼出来：81/2 = 40（整数除法），若写成 81*0.5f = 40.5，
//    `@` 的屏幕列就会在 40 与 41 之间来回变。
// ------------------------------------------------------------
MINI_TEST(Camera, OddViewportUsesIntegerHalf)
{
    Camera             cam;
    const CameraLimits limits{81, kViewH, kMapW, kMapH};

    // 未夹取区间：x − 40 ∈ [0, 200−81] → x ∈ [40, 159]
    const float xs[] = {40.0f, 40.5f, 77.3f, 120.0f, 159.0f};
    for (const float x : xs) {
        cam.Follow(Vec2{x, 100.5f}, limits);
        MINI_CHECK_EQ(FloorToInt(x) - cam.OriginCol(), 40);
    }
}

// ------------------------------------------------------------
// 10. 未夹取时 @ 恒在画面正中（snap 的可观测特征）
// ------------------------------------------------------------
MINI_TEST(Camera, PlayerColumnStaysAtViewCenter)
{
    Camera cam;

    const float xs[] = {40.0f, 40.5f, 100.25f, 150.7f, 160.0f};   // 整段都在未夹取区间内
    for (const float x : xs) {
        cam.Follow(Vec2{x, 100.5f}, Limits80x23());
        MINI_CHECK_EQ(FloorToInt(x) - cam.OriginCol(), kViewW / 2);   // 40
    }
}

// ------------------------------------------------------------
// 11. E-8 守卫：实体与它脚下的 tile 必须落在**同一列**
//     取一个"小数部分与玩家不同"的实体 —— 这时两条取整写法会分道扬镳：
//       本阶段（整数原点）：ToIndex(63.2) − OriginCol()      = 63 − 10 = 53
//       PLAN A.5 原式     ：floor(63.2 − cam.pos.x=10.7)     = 52   ← 差一格
//     ⚠️ 诚实说明：这条用例只能钉住"相机提供的原点 + 地图规则的**组合**是自洽的"。
//        真正防止 game 侧写错的是 S6_CHECKLIST 10.4 的自检（grep 只允许出现 Origin*）
//        与反向验证 R2（那时候这条用例会红）。
// ------------------------------------------------------------
MINI_TEST(Camera, PlayerColumnMatchesItsTileColumn)
{
    Camera cam;
    cam.Follow(Vec2{50.7f, 100.5f}, Limits80x23());   // pos = (10.7, 89.5)，OriginCol = 10
    MINI_CHECK_EQ(cam.OriginCol(), 10);

    const int entityTile = FloorToInt(63.2f);          // 实体站在第 63 格
    const int col        = ColOfTile(cam, entityTile); // = 该格被画到的列

    MINI_CHECK_EQ(col, 53);
    MINI_CHECK_EQ(cam.OriginCol() + col, entityTile);  // 回算得到同一个格（平移自洽）
}

// ------------------------------------------------------------
// 12. "在地图范围内 @ 永远看得见"的机器形式：扫描整张地图
//     y 也要扫：垂直视口只有 23 格，是更紧的那一侧（施工图第 3 节的分析）。
//
//     ⚠️ 循环上界必须是 `< 200.0f`（停在 199.5）：地图是 [0, 200)，**恰好走到 200.0
//        就是走出地图**（S7 之前没有碰撞）。那一刻 `@` 会离开画面 —— 这是坑 6 的**预期**，
//        所以不写进"必须看得见"的断言里。实测边界值（用算式复核过）：
//          x = 199.5 → 第 79 列（画面最后一列）· x = 200.0 → 第 80 列（出屏）
//          y = 199.5 → 第 22 行（画面最后一行）· y = 200.0 → 第 23 行（出屏）
// ------------------------------------------------------------
MINI_TEST(Camera, PlayerStaysOnScreenInsideMap)
{
    Camera cam;

    for (float x = 0.0f; x < 200.0f; x += 0.5f) {
        cam.Follow(Vec2{x, 100.5f}, Limits80x23());
        const int col = FloorToInt(x) - cam.OriginCol();
        MINI_CHECK(col >= 0 && col < kViewW);
    }

    for (float y = 0.0f; y < 200.0f; y += 0.5f) {
        cam.Follow(Vec2{100.5f, y}, Limits80x23());
        const int row = FloorToInt(y) - cam.OriginRow();
        MINI_CHECK(row >= 0 && row < kViewH);
    }
}

// ------------------------------------------------------------
// 13. resize 之后必须按**新**视口夹取（E-12 留给 S6 的那一半）
//     不需要额外的 resize 钩子：尺寸每帧现给，Follow 自然跟上。
// ------------------------------------------------------------
MINI_TEST(Camera, ResizeThenFollowUsesNewViewport)
{
    Camera cam;

    cam.Follow(Vec2{199.5f, 199.5f}, Limits80x23());
    MINI_CHECK_EQ(cam.OriginCol(), 120);   // 200 − 80
    MINI_CHECK_EQ(cam.OriginRow(), 177);   // 200 − 23

    // 窗口缩小到 60×15：同一位置下夹取上界变小 → 原点必须跟着变大
    cam.Follow(Vec2{199.5f, 199.5f}, CameraLimits{60, 15, kMapW, kMapH});
    MINI_CHECK_EQ(cam.OriginCol(), 140);   // 200 − 60
    MINI_CHECK_EQ(cam.OriginRow(), 185);   // 200 − 15
}

// ------------------------------------------------------------
// 14. 纯函数：没有隐藏状态（中间穿插一次完全不同的输入，结果不受影响）
//     这条是"本阶段没有平滑"的机器形式 —— 将来加死区/橡皮筋时，本条会红，
//     那时要把它换成"收敛到目标 + dt 无关"那类用例（S6_CHECKLIST 第 8 节）。
// ------------------------------------------------------------
MINI_TEST(Camera, FollowIsPureFunctionOfInputs)
{
    Camera a;
    Camera b;

    a.Follow(Vec2{100.5f, 100.5f}, Limits80x23());
    b.Follow(Vec2{2.5f, 2.5f}, Limits80x23());        // 先走到另一个极端
    b.Follow(Vec2{100.5f, 100.5f}, Limits80x23());    // 再回到同一输入

    MINI_CHECK(SameVec(a.Pos(), b.Pos()));
    MINI_CHECK_EQ(a.OriginCol(), b.OriginCol());
    MINI_CHECK_EQ(a.OriginRow(), b.OriginRow());
}
