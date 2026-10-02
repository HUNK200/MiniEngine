#include "MiniTest.h"

#include <game/Game.h>

#include <mini/platform/Input.h>
#include <mini/platform/InputState.h>
#include <mini/render/FrameBuffer.h>
#include <mini/world/TileMap.h>

#include <cmath>
#include <cstddef>
#include <initializer_list>

// ------------------------------------------------------------
// game/ 的单元测试（S5 步骤 3：3 个用例）
//
// 【为什么要假 Input】
//   `Game::OnUpdate` 只认 `const platform::Input&` 这个**抽象**（S2 决策 1 选的
//   "抽象基类 + 工厂"），所以测试可以自己派生一个 —— 真实现 `InputWindows` 要
//   `ReadConsoleInputW`，而这里一行终端代码都不用碰。
//
// 【⚠️ 假 Input 的覆盖面边界，别想大】（S5 坑 #17）
//   它只证明"**给定按键状态，Game 算得对**" —— **不证明"真终端的输入是对的"**。
//   真终端那条链（`ReadConsoleInputW` → `InputState`）是 `InputWindows` 的事，
//   S2 已用 test_input_state.cpp 的 7 个用例 + 手工验收覆盖。
//   注意：这里复用**真的** `InputState`（不是手搓四个状态），所以 Pressed/Held 的
//   判定语义是真的，替身替掉的只有"事件从哪来"这一步。
//
// 【⚠️ 只碰公开接口】（S5_CHECKLIST 10.4 的自检要求）
//   `Game` 的 `m_world` / `m_player` 都是私有的，所以位置一律**扫 FrameBuffer 上的 '@'**，
//   期望值一律由 `Game` 的**公有常量**（kMoveSpeed / kSpawnX / kSpawnY）算出来。
//   这样用例改不到内部实现，"改实现不改行为"时它不会误报。
//
// 【⚠️ S6 起：屏幕位移 ≠ 世界位移】
//   相机把玩家的屏幕位置钉在画面中央附近（未夹取时恒在第 `viewW/2` 列、`viewH/2 + 1` 行），
//   所以"位移 / 速度"这类断言只在**相机退化**（视口 ≥ 地图）时才等价于世界位移。
//   本文件里量速度的那条已改用 `kWorldCanvas*`；另外两条
//   （`PlayerIsDrawnAtItsCell` / `VelocityTakesEffectSameFrame`）仍绿只是因为位移小
//   （7 格 / 2 格）**还没走出跟随区** —— 调大 `kMoveSpeed`、或把画布改小，它们的期望值
//   就会失效。见 S6_CHECKLIST 落地偏差 3。
// ------------------------------------------------------------

namespace {

// ⚠️ `using game::Game;` 这一行**必须有**：`Game` 定义在命名空间 `game` 里，裸写 `Game`
//    是找不到的。漏了它的报错很有迷惑性 ——
//      先说 `C2065 "Game": 未声明的标识符`，
//      接着 `Game game;` 这句声明失败，于是后面 `game.OnStart()` 里的 `game` 被解释成
//      **命名空间**，刷出一串 `C2882 "game": 在表达式中非法使用命名空间标识符`。
//    ⚠️ 别去找 include：头文件在、include 目录也在（可从 build/tests/mini_tests.vcxproj 查）。
//    这是**名字查找**问题，不是包含问题。
using game::Game;
using mini::core::FrameContext;
using mini::platform::InputState;
using mini::platform::Key;
using mini::platform::KeyState;
using mini::render::FrameBuffer;

// 画布尺寸：默认用例用同一块（和 10.2 的观察口径一致：80×25）
constexpr int kScreenW = 80;
constexpr int kScreenH = 25;

// ⚠️ S6 落地偏差 3：**"量速度"的用例不能用 80×25**。
//
//    S5 的三个用例都在拿"**屏幕**位移"当"**世界**位移"用 —— 那个等价关系的前提是
//    "没有相机"。S6 之后相机把玩家的屏幕位置钉在画面中央附近（未夹取时恒在第 `viewW/2`
//    列、第 `viewH/2 + 1` 行），于是**屏幕位移在跟随区里会饱和**（2026-10-02 实测）：
//        斜向已归一化 → 屏幕 dy = 9
//        斜向没归一化 → 屏幕 dy = 9      ← 一模一样，**"归一化坏了"根本测不出来**
//    所以量世界位移的用例必须让**相机退化成空操作**：视口 ≥ 地图 → `Camera::Follow`
//    恒返回 `(0, 0)`（S6_CHECKLIST 决策 4 的那条退化语义）→ 屏幕位移重新等于世界位移。
//
//    ⚠️ 画布要 **≥ 201×201**：相机拿到的视口高是"画布高 − 1"（顶栏那一行），
//       200 行的画布只给到 199 行视口 → **y 轴仍然会跟随**。这里取 256×256 留余量。
//       256 这个数由"S5/S6 的地图是 200×200"推出，地图变大时要跟着改。
constexpr int kWorldCanvasW = 256;   // 只给"量世界位移"的用例用
constexpr int kWorldCanvasH = 256;

// —— 测试替身：复用真 InputState，只把"事件来源"换成测试自己喂 ——
class FakeInput final : public mini::platform::Input {
public:
    void Press(Key k)   { m_state.OnKeyDown(k); }   // 模拟"本帧收到按下事件"
    void Release(Key k) { m_state.OnKeyUp(k); }

    void Poll() override {}                                        // 不读终端
    void BeginFrame() override { m_state.BeginFrame(); }           // 帧尾推进（真逻辑）
    [[nodiscard]] KeyState Get(Key k) const override { return m_state.Get(k); }
    [[nodiscard]] bool     IsDown(Key k) const override { return m_state.IsDown(k); }
    [[nodiscard]] bool QuitRequested() const override { return false; }

private:
    InputState m_state;
};

// FrameContext 是聚合体（没有构造函数），最后一个成员是 `const Input&`
//
// ⚠️ 画布尺寸也在这里，且**必须与 FrameBuffer 的尺寸一致** —— `Application::Run` 就是这么
//    保证的（第 ③ 步 Resize 之后，第 ④ 步用 `m_frameBuffer.Width()/Height()` 造 ctx）。
//    S6 第一次修偏差 3 时只换了 FrameBuffer、漏了这里 → **相机拿到的视口还是 80×23**
//    （相机用的是 `ctx.screenWidth/Height`，`fb` 只管画）→ `@` 仍被钉住 → 用例仍红。
//    见 S6_CHECKLIST 落地偏差 4。
FrameContext MakeContext(const mini::platform::Input& input, float dt,
                         int canvasW = kScreenW, int canvasH = kScreenH)
{
    return FrameContext{
        dt,                                   // deltaSeconds（已 clamp 的口径）
        dt * 1000.0f,                         // frameMs
        dt * 1000.0f,                         // maxFrameMs
        0.0f,                                 // workMs
        dt > 0.0f ? 1.0f / dt : 0.0f,         // fps（dt == 0 时别除）
        canvasW,
        canvasH,
        input
    };
}

// 屏幕上某个字符的位置（"@ 画在哪"是这三个用例唯一能观察到的世界状态）
struct Pos {
    int  x     = 0;
    int  y     = 0;
    bool found = false;
};

Pos FindChar(const FrameBuffer& fb, char wanted)
{
    for (int y = 0; y < fb.Height(); ++y) {
        for (int x = 0; x < fb.Width(); ++x) {
            if (fb.At(x, y).ch == wanted) {
                return Pos{x, y, true};
            }
        }
    }
    return Pos{};
}

int CountChar(const FrameBuffer& fb, char wanted)
{
    int count = 0;
    for (int y = 0; y < fb.Height(); ++y) {
        for (int x = 0; x < fb.Width(); ++x) {
            if (fb.At(x, y).ch == wanted) {
                ++count;
            }
        }
    }
    return count;
}

// 从起点到终点的位移（单位 = 屏幕格）
struct Delta {
    int  dx = 0;
    int  dy = 0;
    bool ok = false;
};

// 按住 keys 里的键跑 seconds 秒（每步 1/60 秒），返回 '@' 相对起点的位移。
// 起点用**第 0 帧（dt = 0）**渲染一次拿到 —— 这样不依赖 kSpawn 被画在哪。
//
// ⚠️ 返回的是**屏幕**位移：只有"相机退化"（视口 ≥ 地图）时才等于世界位移，
//    所以量速度的用例必须显式传 `kWorldCanvasW/H`（见上面那段注释）。
Delta RunHolding(float seconds, std::initializer_list<Key> keys,
                 int canvasW = kScreenW, int canvasH = kScreenH)
{
    FakeInput   input;
    Game        game;
    FrameBuffer fb(canvasW, canvasH);
    game.OnStart();

    // 第 0 帧：不按键、dt = 0 → 位置不变，只为把 '@' 画出来当起点
    // ⚠️ ctx 与 fb 必须同一尺寸：相机读 ctx 的尺寸，`fb` 只管画（落地偏差 4）
    game.OnUpdate(MakeContext(input, 0.0f, canvasW, canvasH));
    fb.Clear();
    game.OnRender(fb);

    const Pos start = FindChar(fb, '@');
    if (!start.found) {
        return Delta{};
    }

    input.BeginFrame();                 // 帧尾推进（与 Application 的 ⑦ 同步）
    for (Key k : keys) {
        input.Press(k);                 // 按下之后**一直不松**（按住）
    }

    const float dt    = 1.0f / 60.0f;
    const int   steps = static_cast<int>(seconds / dt + 0.5f);
    for (int i = 0; i < steps; ++i) {
        game.OnUpdate(MakeContext(input, dt, canvasW, canvasH));   // ctx 与 fb 同尺寸（偏差 4）
        fb.Clear();
        game.OnRender(fb);
        input.BeginFrame();
    }

    const Pos end = FindChar(fb, '@');
    if (!end.found) {
        return Delta{};
    }
    return Delta{end.x - start.x, end.y - start.y, true};
}

} // namespace

// 直向 1 秒走 kMoveSpeed 格；斜向 1 秒每个轴只走 ≈ kMoveSpeed/√2 格。
// **反向验证 4 的目标**：把 OnUpdate 里的 Normalized() 去掉 → 斜向变成 kMoveSpeed 格/轴 → 这条变红。
// ⚠️ 这条效力**依赖下面两跑传的 `kWorldCanvas*`**（相机退化成 (0,0)）：用 80×25 时屏幕 dy
//    会饱和到 9，去掉 Normalized() 也还是 9 → 反向验证失效（S6 落地偏差 3 实测过，别删那两个参数）。
//
// ⚠️ 期望值**必须由 `Game::kMoveSpeed` 算出来**，不许写死格数：速度是可调的
//    （默认 20，还能用 `MiniEngine.exe 60 30` 覆盖），写死就等于"调手感必须改测试"。
MINI_TEST(Game, DiagonalMoveIsNotFaster)
{
    // ⚠️ 两跑都用"相机退化"的大画布（落地偏差 3）：本用例量的是**世界**位移。
    // 直向（只按 D）：1 秒前进 kMoveSpeed 格，y 不动
    const Delta straight = RunHolding(1.0f, {Key::D}, kWorldCanvasW, kWorldCanvasH);
    MINI_CHECK(straight.ok);
    MINI_CHECK_EQ(straight.dx, static_cast<int>(Game::kMoveSpeed));
    MINI_CHECK_EQ(straight.dy, 0);

    // 斜向（D + S）：每轴 ≈ kMoveSpeed/√2
    // ⚠️ 容差 ±1 格：屏幕上的格数是 `floor(起点 + 位移) - floor(起点)`，
    //    起点的 0.5 和小数部分会让结果比 `floor(每轴位移)` 差一格。
    const float perAxis = Game::kMoveSpeed / std::sqrt(2.0f);
    const int   lo      = static_cast<int>(perAxis) - 1;
    const int   hi      = static_cast<int>(perAxis) + 1;

    const Delta diagonal = RunHolding(1.0f, {Key::D, Key::S}, kWorldCanvasW, kWorldCanvasH);
    MINI_CHECK(diagonal.ok);
    MINI_CHECK(diagonal.dx >= lo && diagonal.dx <= hi);
    MINI_CHECK(diagonal.dy >= lo && diagonal.dy <= hi);

    // "斜向不比直向快" = **每轴**位移都小于"直向 1 秒的位移"。
    //
    // ⚠️ 2026-09-30 实测踩过的错：原先写 `diagonal.dy < straight.dy` —— 直向只往 x 走，
    //    它的 `dy` **恒为 0**，而斜向的 dy ≈ 8，于是 `8 < 0` 永远为假 → 这条断言永远红
    //    （其余五条全绿）。**比较基准必须是"1 秒的位移"（kMoveSpeed），不是另一次跑的 dy。**
    //    见 S5_CHECKLIST 落地偏差 10。
    const int oneSecondCells = static_cast<int>(Game::kMoveSpeed);
    MINI_CHECK(diagonal.dx < oneSecondCells);   // 不归一化时这里等于 kMoveSpeed → 红
    MINI_CHECK(diagonal.dy < oneSecondCells);
}

// 帧序（7.1 的可执行形式）：**本帧**按下的键，本帧就要生效。
// 若把 World::Update 挪到"写速度"之前，这一帧就会原地不动 → 这条变红。
MINI_TEST(Game, VelocityTakesEffectSameFrame)
{
    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    // 第 0 帧：没有任何键 → 只拿起点
    game.OnUpdate(MakeContext(input, 0.0f));
    fb.Clear();
    game.OnRender(fb);
    const Pos start = FindChar(fb, '@');
    MINI_CHECK(start.found);
    if (!start.found) {
        return;
    }

    // 第 1 帧：**本帧才按下** D，dt = 0.1s → 位移 1.2 格 → 必然跨过一格
    input.BeginFrame();
    input.Press(Key::D);
    game.OnUpdate(MakeContext(input, 0.1f));
    fb.Clear();
    game.OnRender(fb);

    const Pos now = FindChar(fb, '@');
    MINI_CHECK(now.found);
    if (!now.found) {
        return;
    }

    // 本帧就动了 —— 断言只钉"动了、且只动 x"，**不钉具体几格**：
    // 位移 = dt × kMoveSpeed = 0.1 × 20 = 2 格，是可调速度决定的，写死就又变成"调手感必须改测试"。
    // （"先积分后写速度"的实现这一步位移是 0 → `now.x == start.x` → 变红。）
    MINI_CHECK(now.x > start.x);
    MINI_CHECK_EQ(now.y, start.y);   // 只动了 x
}

// 世界 → 屏幕的取整：列 = floor(x)、行 = floor(y) + 1（7.2）。
// 起点由**公有常量**给出（2.5 → 列 2、行 3）；跑固定帧数后，期望列由公式算出来：
//   期望列 = floor(kSpawnX + kMoveSpeed × 帧数 × dt)
// 用 round 的实现会差一格（落在 .5 附近的位移正好卡在两个取整之间）→ 这条变红
//（那正是 2026-09-30 统一掉的那个 floor/round 混用，见落地偏差 3）。
MINI_TEST(Game, PlayerIsDrawnAtItsCell)
{
    constexpr int   kFrames = 21;              // 21 × 1/60 = 0.35 秒
    constexpr float kDt     = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    game.OnUpdate(MakeContext(input, 0.0f));
    fb.Clear();
    game.OnRender(fb);

    const int startCol = mini::world::TileMap::ToIndex(Game::kSpawnX);          // floor(2.5) = 2
    const int startRow = mini::world::TileMap::ToIndex(Game::kSpawnY) + 1;      // floor(2.5) + 1 = 3

    MINI_CHECK(fb.At(startCol, startRow).ch == '@');
    MINI_CHECK_EQ(CountChar(fb, '@'), 1);   // 屏幕上只能有一个 '@'

    input.BeginFrame();
    input.Press(Key::D);
    for (int i = 0; i < kFrames; ++i) {
        game.OnUpdate(MakeContext(input, kDt));
        fb.Clear();
        game.OnRender(fb);
        input.BeginFrame();
    }

    // 位移 = kMoveSpeed × 0.35 = 20 × 0.35 = 7.0 格 → x = 2.5 + 7.0 = 9.5
    // → 期望画在第 **9** 列（floor）。round(9.5) 会画到第 10 列。
    const float moved       = Game::kMoveSpeed * (static_cast<float>(kFrames) * kDt);
    const int   expectedCol = mini::world::TileMap::ToIndex(Game::kSpawnX + moved);

    const Pos end = FindChar(fb, '@');
    MINI_CHECK(end.found);
    if (end.found) {
        MINI_CHECK_EQ(end.x, expectedCol);
        MINI_CHECK_EQ(end.y, startRow);                                 // 没往 y 走
        MINI_CHECK(fb.At(expectedCol + 1, startRow).ch != '@');         // round 版会画到右边那一列
    }
}
