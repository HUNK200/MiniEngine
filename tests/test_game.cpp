#include "MiniTest.h"

#include <game/Game.h>

#include <mini/ai/EnemyBrain.h>   // S8：用例要拿 kDefaultTuning::sightRange 算"该不该被追"
#include <mini/platform/Input.h>
#include <mini/platform/InputState.h>
#include <mini/render/FrameBuffer.h>
#include <mini/world/TileMap.h>

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <string>   // S8：`ScreenText` 用（在面板文本里找状态名）

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
//
// 【⚠️ S7 起：斜向跑会擦到柱子】
//   碰撞接上之后，"直向 1 秒"与"斜向 1 秒"不再对称：斜向从出生点 (2.5, 2.5) 出发朝右下，
//   会去撞 (10, 10) 那根柱子 —— y 在 `x ∈ (9.5, 11.5)` 这段里被柱子的**上表面夹住**
//   （接触面 y = 9.5），于是少了约 2 格的 y 位移。那是**碰撞几何**，不是归一化失效。
//   所以 `DiagonalMoveIsNotFaster` 的斜向只跑 **0.45 秒**（终点 ≈ (8.86, 8.86)，与柱子不接触），
//   而直向仍然跑满 1 秒（那一行 y=2.5 上没有柱子）。见 S7_CHECKLIST 落地偏差 5。
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

// ------------------------------------------------------------
// S8 的观察手段：**只看屏幕**（`Game` 的世界是私有的，用例不许去戳它）
//
// ⚠️ 为什么"屏幕格差"能当"世界格差"用：相机给地图和实体的是**同一个整数原点**
//    （S6 的纪律），所以两者的屏幕列/行之差 == 世界格之差，**与相机滚到哪无关**。
//    这就是 S8_CHECKLIST 10.1 里"只断言相对量、不依赖相机"那条要求。
//
// ⚠️ 为什么从第 2 行开始扫：第 0 行是引擎顶栏、第 1 行是游戏 HUD，它们的文本里可能有字母。
//    实体永远画在第 2 行及以下。
// ------------------------------------------------------------

// 与 `Application` 的帧序一致地推一帧：OnUpdate → Clear → OnRender → 输入帧尾。
void StepFrame(Game& game, FakeInput& input, FrameBuffer& fb, float dt)
{
    game.OnUpdate(MakeContext(input, dt, fb.Width(), fb.Height()));
    fb.Clear();
    game.OnRender(fb);
    input.BeginFrame();
}

// 屏幕上所有 'E' 的位置。行优先扫描 → 顺序天然按 (y, x) 排好，可以直接逐项比较。
// 返回写进 out 的个数（最多 cap 个）。
int CollectEnemies(const FrameBuffer& fb, Pos* out, int cap)
{
    int count = 0;
    for (int y = 2; y < fb.Height() && count < cap; ++y) {
        for (int x = 0; x < fb.Width() && count < cap; ++x) {
            if (fb.At(x, y).ch == 'E') {
                out[count++] = Pos{x, y, true};
            }
        }
    }
    return count;
}

// '@' 到**最近** 'E' 的格距**平方**（屏幕上没有 'E'、或没有 '@' 时返回 -1）。
// 用平方是为了不引入 sqrt —— 比大小不需要开方（`Vec2::LengthSq` 同一条理由）。
int MinEnemyDistanceSq(const FrameBuffer& fb)
{
    const Pos player = FindChar(fb, '@');
    if (!player.found) {
        return -1;
    }

    int best = -1;
    for (int y = 2; y < fb.Height(); ++y) {
        for (int x = 0; x < fb.Width(); ++x) {
            if (fb.At(x, y).ch != 'E') {
                continue;
            }
            const int dx = x - player.x;
            const int dy = y - player.y;
            const int d2 = dx * dx + dy * dy;
            if (best < 0 || d2 < best) {
                best = d2;
            }
        }
    }
    return best;
}

// 让玩家一路向右走，直到最近的敌人进到 `kStopDist` 格以内，然后**站住**（松开 D）。
// 返回站住那一刻的格距**平方**；走满上限还没靠近就返回 -1。
//
// ⚠️ `kStopDist = 8` **必须小于 `EnemyTuning::sightRange`(10)** —— 不然敌人根本不会
//    进战，"接下来距离会变小"这个断言就不成立（会变成一条永远红的用例）。
// ⚠️ 走多久**不写死**：敌人出生点在哪是 `Game` 的私有安排，用例只认"够近了就停"。
int WalkPlayerNextToEnemy(Game& game, FakeInput& input, FrameBuffer& fb)
{
    constexpr float kDt         = 1.0f / 60.0f;
    constexpr int   kStopDist   = 8;
    constexpr int   kStopDistSq = kStopDist * kStopDist;
    constexpr int   kMaxWalk    = 400;   // 5 秒上限（约 100 格），够走到地图上任何一个敌人

    StepFrame(game, input, fb, kDt);     // 第 0 帧：拿起点（不按键）
    input.Press(Key::D);

    int walked = 0;
    int distSq = -1;
    while (walked < kMaxWalk) {
        StepFrame(game, input, fb, kDt);
        ++walked;
        distSq = MinEnemyDistanceSq(fb);
        if (distSq >= 0 && distSq <= kStopDistSq) {
            break;
        }
    }

    input.Release(Key::D);               // 站住：下一帧 `IsDown(D)` 就是 false 了
    return (distSq >= 0 && distSq <= kStopDistSq) ? distSq : -1;
}

// 把整个屏幕拼成一个字符串（行优先，每行末尾补 '\n'）。
//
// 用途：在**面板文本**里找状态名（`Dead` / `Chase` ……）。
// ⚠️ 这是"从公开面取证"的最后一招 —— 面板是 `Game` 唯一**主动把内部状态写出来**的地方
//    （`EnemyData.state` 本身是私有的）。代价是它很脆：**面板的列宽/格式一改，用例就会红**。
//    所以只在"没有更窄的观察面"时才用它，而且要断言的是**内容包含**（`find`），
//    不是"第几列第几行是什么字"。
std::string ScreenText(const FrameBuffer& fb)
{
    const std::size_t width = static_cast<std::size_t>(fb.Width());
    std::string text;
    text.reserve((width + 1) * static_cast<std::size_t>(fb.Height()));

    for (int y = 0; y < fb.Height(); ++y) {
        for (int x = 0; x < fb.Width(); ++x) {
            text.push_back(fb.At(x, y).ch);
        }
        text.push_back('\n');
    }
    return text;
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
    //
    // ⚠️ **S7 起斜向只跑 0.45 秒**（理由见文件头那段"斜向会擦到柱子"）：
    //    跑满 1 秒时 y 会被 (10,10) 柱子的上表面夹住约 2 格 → 那时测到的是"碰撞几何"，
    //    不再是"归一化"。**这不是"期望值该放宽"，是测量口径被碰撞污染了**
    //    （同 S6 落地偏差 3：先判口径，再决定改哪里）。
    //    0.45 秒 × 每轴 kMoveSpeed/√2 = 6.36 格 → 终点 (8.86, 8.86)，
    //    盒 [8.36, 9.36) 与柱子那一格 [10, 11) 不接触（余量 0.64 格）。
    // ⚠️ 容差 ±1 格：屏幕上的格数是 `floor(起点 + 位移) - floor(起点)`，
    //    起点的 0.5 和小数部分会让结果比 `floor(每轴位移)` 差一格。
    constexpr float kDiagonalSeconds = 0.45f;

    const float perAxis = Game::kMoveSpeed / std::sqrt(2.0f) * kDiagonalSeconds;
    const int   lo      = static_cast<int>(perAxis) - 1;
    const int   hi      = static_cast<int>(perAxis) + 1;

    const Delta diagonal = RunHolding(kDiagonalSeconds, {Key::D, Key::S}, kWorldCanvasW, kWorldCanvasH);
    MINI_CHECK(diagonal.ok);
    MINI_CHECK(diagonal.dx >= lo && diagonal.dx <= hi);
    MINI_CHECK(diagonal.dy >= lo && diagonal.dy <= hi);

    // "斜向不比直向快"的判别力在这两条上界里：参考值 = **同时长**下"忘了归一化"会走多远
    //   = kMoveSpeed × 0.45 ≈ 9 格。归一化正常 → 6 < 9 通过；忘了归一化 → 9 → 红。
    //
    // ⚠️ 2026-09-30 实测踩过的错（S5_CHECKLIST 落地偏差 10）：原先写 `diagonal.dy < straight.dy`
    //    —— 直向只往 x 走、它的 `dy` **恒为 0**，于是 `8 < 0` 永远为假 → 断言永远红。
    //    **比较基准必须由"同一口径下的位移"算出来，不能拿另一次跑的另一个分量当基准。**
    const int unnormalizedPerAxis = static_cast<int>(Game::kMoveSpeed * kDiagonalSeconds + 0.5f);
    MINI_CHECK(diagonal.dx < unnormalizedPerAxis);
    MINI_CHECK(diagonal.dy < unnormalizedPerAxis);
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

// ------------------------------------------------------------
// S7（用例 18）：碰撞接上之后 —— **走不出地图**，而且撞得停
//
// 按 W 从出生点 (2.5, 2.5) 往上：外沿墙在第 0 行（`Game::BuildDemoMap`），
// 盒 `pos ± 0.5` 贴到 y = 1.5 就停住 → 只走 **1 格**（不是 kMoveSpeed 格）。
//
// ⚠️ 期望值 -1 **可以写死**：它由"外沿墙在第 0 行"这条**地图几何**决定，与速度无关
//    （与"不许写死速度算出来的格数"不冲突 —— 那个数依赖 kMoveSpeed，这个数不依赖）。
// ⚠️ 反向验证：把 `World::Update` 里那行改回 `pos += v * dt` → 1 秒会走 kMoveSpeed 格 → 这条变红。
// ------------------------------------------------------------
MINI_TEST(Game, PlayerStopsAtTopWall)
{
    // 不夹紧的话 1 秒会走 kMoveSpeed(20) 格（早就走出地图了）；夹紧后只走 1 格就贴住顶墙
    const Delta up = RunHolding(1.0f, {Key::W});

    MINI_CHECK(up.ok);
    MINI_CHECK_EQ(up.dy, -1);   // (2.5, 2.5) → (2.5, 1.5)：tile 行 2 → 1
    MINI_CHECK_EQ(up.dx, 0);    // 只动了 y
}

// ------------------------------------------------------------
// S8（敌人）的 4 条集成用例
//
// 【它们和 `Enemy` suite 的分工】
//   `Enemy`（test_enemy.cpp）测的是**纯函数** `Decide`：状态机、迟滞、冷却、边界。
//   这里测的是**接线**：敌人真的被 spawn 出来、真的在动、真的会追、玩家的 HP 真的会掉。
//   两层都不省 —— 纯函数全绿只说明"给定输入算得对"，不说明"速度被写回了世界"。
//
// 【⚠️ 只碰公开接口】（S5_CHECKLIST 10.4）
//   `Game` 的世界、敌人集合、巡逻表**全是私有的**，所以这里的观察手段只有两样：
//     · 屏幕上的 `'@'` / `'E'`（`CollectEnemies` / `MinEnemyDistanceSq`）
//     · 公开只读的 `Game::PlayerHp()`
//   期望值一律由**公开常量/公开 tuning**算出，不写死"第几格"（相机一滚就失真的那类数）。
//
// 【⚠️ 这两条是**替代**了施工图里的用例，见 S8_CHECKLIST 偏差 7 / 8】
//   · `EnemyStopsAtWall` —— 删了：S8 的敌人从来不会走到墙边（巡逻段与追击路径都在空地上），
//     没有可复现的机器判据。它由"全项目只有一条积分路径"结构保证，人眼验收第 5 条负责看。
//   · `RemovingEnemyLeavesNoDanglingId` —— 换成 `PlayerHpStopsAtZero`：
//     S8 **根本没有删除敌人的路径**（`Dead` 只把 `active` 置 false），
//     所以"不留悬空 id"这条在 S8 是**空用例**（永远绿、什么也没测）。它该跟 S9 的真删一起做。
// ------------------------------------------------------------

// S8（用例 19）：玩家待在出生点不动 → 敌人**只管巡逻，不追人**。
//
// 这条同时钉两件事，缺一不可：
//   · 巡逻**真的在动** —— 否则"不追"可以靠"敌人根本不更新"作弊，那是最容易假绿的一种写法；
//   · 远距离**没被拉进战斗** —— 判据用公开的 `sightRange`，不是屏幕上某个写死的格数。
MINI_TEST(Game, EnemiesPatrolUntilPlayerIsNear)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    StepFrame(game, input, fb, kDt);   // 第 0 帧：先看一眼

    constexpr int kMaxEnemies = 8;     // 比 kEnemyCount(5) 宽裕，够放下所有敌人
    Pos       before[kMaxEnemies] = {};
    const int nBefore  = CollectEnemies(fb, before, kMaxEnemies);
    const Pos player0  = FindChar(fb, '@');
    const int dist0Sq  = MinEnemyDistanceSq(fb);

    MINI_CHECK(player0.found);
    // ⚠️ 这个 5 是**写死**的：`kEnemyCount` 是私有的，而且它由 `PLAN:572` 的验收
    //    （"5 个敌人同时追"）钉着 —— 改敌人数量本来就该让这条红。
    MINI_CHECK_EQ(nBefore, 5);
    MINI_CHECK(dist0Sq > 0);

    // 跑 1 秒。玩家一个键都不按 → 他一步都不会动，所有位移都只能来自敌人。
    constexpr int kFrames = 60;
    for (int i = 0; i < kFrames; ++i) {
        StepFrame(game, input, fb, kDt);
    }

    Pos       after[kMaxEnemies] = {};
    const int nAfter = CollectEnemies(fb, after, kMaxEnemies);
    MINI_CHECK_EQ(nAfter, nBefore);

    // 巡逻真的在动：逐个比较（`CollectEnemies` 行优先扫描 → 顺序稳定）
    bool moved = false;
    for (int i = 0; i < nBefore; ++i) {
        if (after[i].x != before[i].x || after[i].y != before[i].y) {
            moved = true;
        }
    }
    MINI_CHECK(moved);

    // 玩家没动 —— 这条让"距离没变小"归因清楚（是他的问题还是敌人的问题）
    const Pos player1 = FindChar(fb, '@');
    MINI_CHECK(player1.found);
    MINI_CHECK_EQ(player1.x, player0.x);
    MINI_CHECK_EQ(player1.y, player0.y);

    // 远距离不追：最近的那个敌人仍在视距**之外**（比较的是平方，所以右边也要平方）
    const int dist1Sq = MinEnemyDistanceSq(fb);
    const int sightSq = static_cast<int>(mini::ai::kDefaultTuning.sightRange *
                                         mini::ai::kDefaultTuning.sightRange);
    MINI_CHECK(dist1Sq > sightSq);
}

// S8（用例 20）：玩家走进视距 → 敌人**真的会靠近**（`Chase` 生效，且速度真的写回了世界）。
// 这是反向验证 R5 的目标：把 `DriveEnemyAi` 挪到 `World::Update` 之后 → 这条变红。
MINI_TEST(Game, EnemyClosesInWhenPlayerIsNear)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    // 走到"最近敌人 8 格以内"（< 视距 10）再站住
    const int dist0Sq = WalkPlayerNextToEnemy(game, input, fb);
    MINI_CHECK(dist0Sq > 0);   // -1 = 走满上限还没靠近（那说明 spawn 或移动坏了）

    // 站住不动 0.4 秒：敌人应该**主动把距离拉近**
    constexpr int kObserveFrames = 24;
    for (int i = 0; i < kObserveFrames; ++i) {
        StepFrame(game, input, fb, kDt);
    }

    const int dist1Sq = MinEnemyDistanceSq(fb);
    MINI_CHECK(dist1Sq > 0);
    MINI_CHECK(dist1Sq < dist0Sq);   // ← "会追"的判据（只比大小，不假设追到哪）
}

// S8（用例 21）：贴上去之后，玩家的 HP 会掉（`Attack` + 伤害结算真的接上了）。
MINI_TEST(Game, AttackReducesPlayerHp)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    MINI_CHECK(WalkPlayerNextToEnemy(game, input, fb) > 0);

    const int hpBefore = game.PlayerHp();
    MINI_CHECK(hpBefore > 0);

    // 站住 2 秒：够敌人贴到 attackEnter(1.5) 并打出至少一击（冷却 0.8s）
    constexpr int kFrames = 120;
    for (int i = 0; i < kFrames; ++i) {
        StepFrame(game, input, fb, kDt);
    }

    const int hpAfter = game.PlayerHp();
    MINI_CHECK(hpAfter < hpBefore);
    MINI_CHECK(hpAfter >= 0);
}

// S8（用例 22 的**替代品**，见上面那段说明与 S8_CHECKLIST 偏差 8）：
// HP 掉到 0 就**停在那里**，永远不变负。
// 钉的是 `DriveEnemyAi` 里那句 `std::max(0, ...)`，以及"HP 归 0 → 敌人脱战"这条连锁。
MINI_TEST(Game, PlayerHpStopsAtZero)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    MINI_CHECK(WalkPlayerNextToEnemy(game, input, fb) > 0);

    // 一直站着被打：10 点血、0.8s 一击，而且可能好几个敌人一起打 → 12 秒绰绰有余
    constexpr int kFrames = 720;
    for (int i = 0; i < kFrames; ++i) {
        StepFrame(game, input, fb, kDt);
    }

    MINI_CHECK_EQ(game.PlayerHp(), 0);

    // 再站 1 秒：仍然**不许**变负（这就是 `std::max(0, ...)` 那条守卫）
    constexpr int kExtraFrames = 60;
    for (int i = 0; i < kExtraFrames; ++i) {
        StepFrame(game, input, fb, kDt);
    }
    MINI_CHECK_EQ(game.PlayerHp(), 0);
}

// ------------------------------------------------------------
// S8（补的用例，2026-10-09 反向验证 R6 逼出来的）：击杀 → 敌人**从屏幕上消失**。
//
// 【为什么补这一条】
//   反向验证 R6 = 把 `DriveEnemyAi` 里那句 `entity->active = false;` 注掉。
//   结果：**测试仍然 145 passed / 0 failed 全绿** —— 但 exe 里现象明显不对：
//   "按 F7 → 按 K 杀掉最近的敌人，面板显示 Dead，可那个 `E` **还留在屏幕上**"。
//   也就是说"死亡三件套"里最后一件（**不再绘制**）当时**零覆盖**。
//
// 【这条用例的判据必须落在哪】
//   ✅ **屏幕上的 `'E'` 少一个** ← 只有这个能守住 `active = false`
//   ⚠️ 面板里出现 `Dead` **不能**替代它：状态来自**另一套真相**（`EnemyData.state`），
//      `active` 没置 false 时它照样成立（这正是 R6 全绿的直接原因）。
//      所以下面两条断言**各守一件事**，缺一不可 —— 顺序也不能反（先数数、再看文本）。
//
// 【它顺便补上了施工图里写不出来的那半条】
//   §10.1 用例 22 `RemovingEnemyLeavesNoDanglingId` 在 S8 无法实现（没有删除路径，会是空用例，
//   见偏差 8）。这条用例覆盖了"死亡后停更 → 不绘制"的**端到端**；真正的"不留悬空 id"
//   仍留到 S9 做真删时再补。
//
// 【观察面全是公开的】`F7`/`K` 只是 `Key` 枚举（`FakeInput` 照样能按）· 屏幕上的 `'@'`/`'E'`
//   · 面板文本（`ScreenText`）。
// ------------------------------------------------------------
MINI_TEST(Game, EnemyDiesAndDisappears)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    // 走到敌人身边：不是为了"够得着"（`K` 杀的是**离玩家最近**的那个，多远都能杀），
    // 而是为了让**所有敌人都落在相机视口里**，这样"屏幕上少一个"才数得准。
    MINI_CHECK(WalkPlayerNextToEnemy(game, input, fb) > 0);

    constexpr int kMaxEnemies = 8;
    Pos       before[kMaxEnemies] = {};
    const int nBefore = CollectEnemies(fb, before, kMaxEnemies);
    MINI_CHECK_EQ(nBefore, 5);   // 5 个（PLAN:572）；同 `EnemiesPatrolUntilPlayerIsNear`

    // ④b 开面板 —— `K` **只在面板开着时才生效**（`Game.cpp` 的 ④c），所以这一步不能省
    input.Press(Key::F7);
    StepFrame(game, input, fb, kDt);

    // ④c 杀敌（在 ⑤b 之前，所以"同帧就能看到"它是 S8 刻意的帧序安排）
    input.Press(Key::K);
    StepFrame(game, input, fb, kDt);

    // (1) **守 `active = false` 的那一条**：屏幕上少一个 `'E'`
    Pos       after[kMaxEnemies] = {};
    const int nAfter = CollectEnemies(fb, after, kMaxEnemies);
    MINI_CHECK_EQ(nAfter, nBefore - 1);

    // (2) 另一套真相还在：面板里仍然看得见 `Dead`（不删 `EnemyData` 的理由，见 §6.6 / C16）
    const std::string text = ScreenText(fb);
    MINI_CHECK(text.find("Dead") != std::string::npos);
}
