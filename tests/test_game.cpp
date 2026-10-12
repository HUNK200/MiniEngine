#include "MiniTest.h"

#include <game/Game.h>

#include <mini/ai/EnemyBrain.h>   // S8：用例要拿 kDefaultTuning::sightRange 算"该不该被追"
#include <mini/core/Time.h>       // S9：`kMaxDelta`（dt 的上限，G7 的防穿靶用例要用它）
#include <mini/platform/Input.h>
#include <mini/platform/InputState.h>
#include <mini/render/FrameBuffer.h>
#include <mini/world/TileMap.h>

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <sstream>  // S9 步骤 3 收尾：`SummarizeEnemyPanel` 按"词"解析面板行（不按列）
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
using game::Facing8;
using game::Game;
using game::GameState;   // S10 步骤 2：用例断言 State() 要用它
using mini::core::FrameContext;
using mini::platform::InputState;
using mini::platform::Key;
using mini::platform::KeyState;
using mini::render::FrameBuffer;

// 画布尺寸：默认用例用同一块（和 10.2 的观察口径一致：80×25）
constexpr int kScreenW = 80;
constexpr int kScreenH = 25;

// ⚠️ **`Game::kHudRow` 的镜像**（S9 步骤 5 起 = **2**）：HUD 占两行 ⇒ 世界视口的第一个屏幕行 = 2。
//    `Game` 里那个常量是**文件内的**（`Game.cpp` 顶部），并且刻意不开公开接口 ⇒ 用例只能照抄。
//    ⚠️ 照抄的代价是"**安全方向**"的：一旦它与 `Game.cpp` 不同步，用例会**红**
//       （`PlayerIsDrawnAtItsCell` 的 `startRow` 就是拿它算的），而不是静默错位。
//    ⚠️ 凡是"拿屏幕行号当判据"的地方一律用它、**别写字面量**：
//       · `CollectEnemies` / `MinEnemyDistanceSq` 的扫描起点（要跳过顶栏 + HUD）
//       · `PlayerIsDrawnAtItsCell` 的 `startRow`
constexpr int kTestHudRow = 2;

// 世界视口**第一个可见**的屏幕行 = 跳过引擎顶栏（行 0）与 HUD（行 1..kTestHudRow）。
// ⚠️ 第 `kTestHudRow` 行是"最后一行 HUD"，它**同时**是世界的第一个逻辑行 → 被 HUD 盖住，
//    所以扫描要从它**下面**那行开始。
constexpr int kTestFirstWorldRow = kTestHudRow + 1;   // = 3

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
//    ⚠️ 画布要 **≥ 202×202**：相机拿到的视口高是"画布高 − `kHudRow`"（HUD 占的行数；
//       S9 步骤 5 起 = **2**，此前是 1），200 行的画布只给到 198 行视口 → **y 轴仍然会跟随**。
//       这里取 256×256 留余量。
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
// ⚠️ **前置声明**：`StartPlaying` 在下面定义（它要用 `StepFrame`），而 `RunHolding` 比它先写
//    —— C++ 要求先见到声明。S10 步骤 2 起，**任何"要推帧看世界动"的地方都得先过它一次**。
void StartPlaying(Game& game, FakeInput& input, FrameBuffer& fb);

Delta RunHolding(float seconds, std::initializer_list<Key> keys,
                 int canvasW = kScreenW, int canvasH = kScreenH)
{
    FakeInput   input;
    Game        game;
    FrameBuffer fb(canvasW, canvasH);
    game.OnStart();
    StartPlaying(game, input, fb);   // ★ S10：先进 `Playing`，否则世界不会动（见它的注释）

    // 第 0 帧：不按键、dt = 0 → 位置不变，只为把 '@' 画出来当起点
    // ⚠️ ctx 与 fb 必须同一尺寸：相机读 ctx 的尺寸，`fb` 只管画（落地偏差 4）
    // ⚠️ S10 起它前面多了 `StartPlaying` 那两帧（只按 Enter、不按方向键）⇒ 玩家仍在出生点，
    //    `kSpawnX/kSpawnY` 这条基线不受影响。
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

// ── S10 步骤 2：把游戏推进到 `Playing`（按一次 Enter）──────────────────────
//
// 【为什么每个"要推帧看世界动"的用例都得先调它】
//   S10 起状态机初值是 `Title`（`PLAN` 的"开局提示 → 按 Enter"），而 `Title` 的
//   `StatePolicy` **五项全是 false** ⇒ 不按 Enter 的话：不读 WASD、不写速度、不积分、
//   连 `sumdt` 都不加 —— **世界一个像素都不会动**，所有既有用例都会因为"@ 不动"而红。
//
// ⚠️ 这是 S10 施工图**漏算的一条涟漪**（`S10_CHECKLIST` 落地偏差 3）：施工图只列了
//    "'R' 不重开是已知中间态"，没提"18 条既有 `Game` 用例全都假设世界从第 0 帧就在动"。
//
// ⚠️ 刻意**不藏进 `StepFrame` 里**（那能让 18 个调用点零改动）：因为新加的
//    "`Title` 下推 60 帧什么都不许变"那几条用例**恰恰需要"没按过 Enter"的状态**
//    —— 藏进去它们就没法推帧了。摆在调用点上，谁需要谁调。
void StartPlaying(Game& game, FakeInput& input, FrameBuffer& fb)
{
    constexpr float kDt = 1.0f / 60.0f;

    input.Press(Key::Enter);
    StepFrame(game, input, fb, kDt);     // 这一帧的 ⓪ 看到 Pressed → 转 `Playing`
    input.Release(Key::Enter);
    StepFrame(game, input, fb, kDt);     // 消化掉 Released（它不产生事件，只是走完状态机）

    // ⚠️ 这两帧玩家一个方向键都没按 ⇒ **位置没动**，所以"第 0 帧基线"仍然是出生点，
    //    以 `kSpawnX/kSpawnY` 算期望值的用例不受影响。
}

// 屏幕上所有 'E' 的位置。行优先扫描 → 顺序天然按 (y, x) 排好，可以直接逐项比较。
// 返回写进 out 的个数（最多 cap 个）。
//
// ⚠️ 从 `kTestFirstWorldRow` 起扫：**顶栏（行 0）与 HUD（行 1..kTestHudRow）不许算进来**
//    —— HUD 上印着 `hp/kills/dir` 这些字母，把它们当"画面内容"数会静默漏计/多计。
//    S9 步骤 5 之前这里是写死的 `y = 2`（HUD 只有一行时的值）。
int CollectEnemies(const FrameBuffer& fb, Pos* out, int cap)
{
    int count = 0;
    for (int y = kTestFirstWorldRow; y < fb.Height() && count < cap; ++y) {
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
    for (int y = kTestFirstWorldRow; y < fb.Height(); ++y) {   // 同 `CollectEnemies`：跳过顶栏 + HUD
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

    StartPlaying(game, input, fb);       // ★ S10：先按 Enter 进 `Playing`，否则 D 键读不到

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

// ── S10 步骤 2：让玩家**死掉** ─────────────────────────────────────────────
//
// 走到敌人跟前 → 站着被打到 0 血 → 再推几帧等 ⓪ 判出 `PlayerDied`。
// 返回 true = 确实停在 `GameOver`（否则说明"走到 0 血"这条链本身有问题，别把红算到状态机上）。
//
// ⚠️ 血是 10 点、敌人 0.8 秒一击、可能好几个一起打 ⇒ 12 秒（720 帧）绰绰有余
//    （`PlayerHpStopsAtZero` 从 S8 起就用这个数）。
// ⚠️ 循环条件写成 `PlayerHp() > 0` 而不是"跑满 720 帧"：一归零就停，
//    这样"死亡那一帧"与"下一帧才转态"这两个时刻不会被多余的帧混过去。
bool KillPlayer(Game& game, FakeInput& input, FrameBuffer& fb)
{
    constexpr float kDt = 1.0f / 60.0f;

    if (WalkPlayerNextToEnemy(game, input, fb) <= 0) {
        return false;   // 没能走到敌人跟前（-1 = 走满上限还没靠近）
    }

    for (int i = 0; i < 720 && game.PlayerHp() > 0; ++i) {
        StepFrame(game, input, fb, kDt);
    }
    if (game.PlayerHp() != 0) {
        return false;   // 12 秒都没打死 —— 那是敌人/AI 的问题，不是状态机的
    }

    // 归零之后**最多再推 3 帧**：状态转移只发生在下一帧的 ⓪（见 `S10_CHECKLIST` §7.1 理由 1）
    for (int i = 0; i < 3 && game.State() != GameState::GameOver; ++i) {
        StepFrame(game, input, fb, kDt);
    }
    return game.State() == GameState::GameOver;
}

// ── S9 的两个小辅助 ────────────────────────────────────────────────────
//
// 开一枪：按下 → 推一帧（这一帧的 ⑤c 看到 `Pressed` 并生成子弹）→ 松开。
//
// ⚠️ **连续开火时，两次调用之间必须至少推一帧**（让状态机看到"抬起过"）。
//    原因在 `InputState::Get`：`Pressed` 要求 `was == false`，而 `BeginFrame` 是在**帧尾**
//    把"本帧末的按下状态"折成 `was` 的 —— 按完立刻再按，`was` 还是 true ⇒ 第二次拿到的
//    是 `Held`，**子弹会少一发**（用例里表现为"三发只打中两发"这种很难查的现象）。
//    所以下面这些用例都是"开一枪 → 再推若干帧"，天然满足。
//
// ⚠️ 开完枪**当帧就能看到子弹**（`FireBullet` 在 ⑤c = 积分之前）—— 想看"刚出膛"的样子
//    就在调用后立刻查屏幕；想等它飞过去就再 `StepFrames`。
void FireOnce(Game& game, FakeInput& input, FrameBuffer& fb, float dt)
{
    input.Press(Key::Space);
    StepFrame(game, input, fb, dt);
    input.Release(Key::Space);
}

// 不按键地推 n 帧（等子弹飞 / 等状态机收敛）。
void StepFrames(Game& game, FakeInput& input, FrameBuffer& fb, float dt, int n)
{
    for (int i = 0; i < n; ++i) {
        StepFrame(game, input, fb, dt);
    }
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

// ── S9 步骤 3 收尾：面板文本的解析（四条"打敌人"的用例共用）─────────────────
//
// 【为什么要有它】"屏幕上一个 `'E'` = 一个敌人"这个前提**是假的**：
//   `DrawEntities` 一格只写一个字符，而敌人之间**没有推挤**（`C12` → S10）
//   ⇒ 两只敌人可以站在同一格上、只画得出一个 `'E'`。
//   2026-10-10 实测：`Game.BulletHitsEnemyEvenAtMaxDelta` 里 `Kills() == 1`（真死 1 个）
//   而屏幕少了 **2** 个 `'E'` —— 就是两只追兵汇到了同一格（跑 exe + `F7` 人眼确认过）。
//   同一个前提在 `G4`/`G5` 上只是**碰巧**没被踩到（它们的检查点比 `G7` 早 0.1 秒）。
//
// 【换成的判据】像素计数改成"**从游戏自己的真值算期望值**"：
//       屏幕上的 `'E'` 个数  ==  面板里各行所占的**不同 tile** 数
//   重叠会被自然算进去；而"渲染漏画了一只"照样会红（两边不等）。
//
// ⚠️ 代价（`ScreenText` 的注释里早就写过）：**面板格式一改，这里就失效**。
//    所以"面板长什么样"这套知识只写在这一处 —— 改 `DrawAiPanel` 的格式串时，
//    必须同时改这里，否则这几条用例会一起红。
// ⚠️ **2026-10-10（S9 步骤 4）真删落地时又改过一次**：面板里不再有 `state=Dead` 行
//    （敌人一死就**当场**从 `m_enemies` 里被 erase），所以"死了几个"改由**表头的 `n=`**
//    说话 —— 它更准（`n=` 就是 `m_enemies.size()`）；同时多出 `hasIdGone`：
//    `<id gone>` 行是**悬空 id 唯一能被看见的地方**，也就成了新用例的判据。
struct PanelSummary {
    int  enemyCount = 0;     // 表头 `n=`（= `m_enemies.size()`；真删之后就是"还活着几个"）
    int  aliveTiles = 0;     // 各行占的**不同** tile 数（= 屏幕上该有几个 `'E'`）
    bool hasIdGone  = false; // 出现过 `<id gone>` ⇔ 有 `EnemyData` 找不到它的 `Entity`
};

// 逐行解析。三种行**互斥**，靠行内特征区分（一律**不按列取** —— `ScreenText` 的注释
// 要求"别断言第几列第几个字"，所以按"词"和"子串"认，改列宽不影响）：
//   · 含 `t=(`        → 敌人行（表头与 `<id gone>` 行都不含）
//   · 含 `n=`         → 表头
//   · 含 `<id gone>`  → 悬空 id 行
//
// ⚠️ 隐含前提：**所有还活着的敌人都在视口内**（`nBefore == kWaveBase` 已经在每条用例里钉过了）。
//    不在视口里的敌人也占一格、但屏幕上看不见 ⇒ 两边不等。当前的场景满足：敌人都在
//    `x = 44.5..72.5 / y = 2.5..6.5`，而相机的 80 列覆盖了整片交战区。
PanelSummary SummarizeEnemyPanel(const std::string& panelText)
{
    // 面板行数上限（表头 1 + 每敌 1）。当前 5 个敌人 = 6 行，够用。
    // ⚠️ 真到了上限也只是 `aliveTiles` 数不全 ⇒ `nAfter == aliveTiles` 会**红**（安全方向），
    //    不会静默变绿。
    constexpr int kMaxTiles = 32;
    int  tileCount  = 0;
    int  enemyCount = 0;
    bool hasIdGone  = false;
    int  tilesXPts[kMaxTiles] = {};   // 收集到的 tile（去重用）
    int  tilesYPts[kMaxTiles] = {};

    std::size_t begin = 0;
    for (;;) {
        const std::size_t end = panelText.find('\n', begin);
        const std::string line = (end == std::string::npos)
                                     ? panelText.substr(begin)
                                     : panelText.substr(begin, end - begin);

        const std::size_t tileAt = line.find("t=(");
        if (tileAt != std::string::npos) {
            // ── 敌人行 ──
            // 第 1 个词是下标（这里不用，但它是状态名**之前**的词，得先吃掉），
            // 第 2 个词是状态名。⚠️ `Dead` 在真删之后**不该再出现**（一死就当场被 erase），
            // 但保留这个过滤器 —— 万一将来有路径又把僵尸留回来，这里会把它排除在
            // `aliveTiles` 之外，而不是算成一个"该画的 `'E'`"。
            std::istringstream head(line);
            int                idx = 0;
            std::string        state;
            if ((head >> idx >> state) && state != "Dead") {
                std::istringstream tail(line.substr(tileAt + 3));   // "t=(" 之后
                int  tx    = 0;
                int  ty    = 0;
                char comma = 0;
                if ((tail >> tx >> comma >> ty) && comma == ',') {
                    bool seen = false;
                    for (int i = 0; i < tileCount; ++i) {
                        if (tilesXPts[i] == tx && tilesYPts[i] == ty) {
                            seen = true;
                            break;
                        }
                    }
                    if (!seen && tileCount < kMaxTiles) {
                        tilesXPts[tileCount] = tx;
                        tilesYPts[tileCount] = ty;
                        ++tileCount;
                    }
                }
            }
        } else if (const std::size_t nAt = line.find("n="); nAt != std::string::npos) {
            // ── 表头（`F7 AI  n=%d`）──
            std::istringstream tail(line.substr(nAt + 2));
            tail >> enemyCount;
        } else if (line.find("<id gone>") != std::string::npos) {
            // ── 悬空 id 行 ──（`EnemyData` 还在，但它的 `Entity` 已经不在世界里）
            hasIdGone = true;
        }

        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }

    return PanelSummary{enemyCount, tileCount, hasIdGone};
}

// 取证姿势：开 `F7` 面板 → 推一帧 → 返回**这一帧**的面板摘要。
// ⚠️ 面板与 `'E'` 计数**必须取同一帧**：两次 `StepFrame` 之间敌人会走动，
//    分开取就会出现"面板说 4 只、屏幕还画着上一帧的 5 个"这种假红。
PanelSummary OpenPanelAndSummarize(Game& game, FakeInput& input, FrameBuffer& fb, float dt)
{
    input.Press(Key::F7);        // `Pressed` 是边沿 ⇒ 按一下就开，不会来回切
    StepFrame(game, input, fb, dt);
    input.Release(Key::F7);
    return SummarizeEnemyPanel(ScreenText(fb));
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
    StartPlaying(game, input, fb);   // ★ S10：先进 `Playing`（否则不读键、不积分）

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
    StartPlaying(game, input, fb);   // ★ S10：先进 `Playing`（否则不读键、不积分）

    game.OnUpdate(MakeContext(input, 0.0f));
    fb.Clear();
    game.OnRender(fb);

    const int startCol = mini::world::TileMap::ToIndex(Game::kSpawnX);              // floor(2.5) = 2
    // ⚠️ `+ kTestHudRow` 就是 `Game.cpp` 里那句 `row = ToIndex(y) − originRow + kHudRow`
    //    （相机夹在 (0,0)，所以 originCol/originRow 都是 0）—— **S9 步骤 5 之前这里是 `+ 1`**。
    //    下面两条断言都从 `startRow` 派生，所以只改这一个表达式就够。
    const int startRow = mini::world::TileMap::ToIndex(Game::kSpawnY) + kTestHudRow;   // floor(2.5) + 2 = 4

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
    StartPlaying(game, input, fb);   // ★ S10：先进 `Playing`（否则敌人也不动）

    StepFrame(game, input, fb, kDt);   // 第 0 帧：先看一眼

    constexpr int kMaxEnemies = 8;     // = `kWaveMax`：封顶那一波也放得下
    Pos       before[kMaxEnemies] = {};
    const int nBefore  = CollectEnemies(fb, before, kMaxEnemies);
    const Pos player0  = FindChar(fb, '@');
    const int dist0Sq  = MinEnemyDistanceSq(fb);

    MINI_CHECK(player0.found);
    // ⚠️ 期望值 = **第 1 波有几个**（S10 步骤 4 起由 `kWaveBase` 决定，不再是写死的 5）。
    //    它由公开常量算出来 ⇒ 调波次数值不会让这条红（`S5_CHECKLIST` 10.4 的纪律）。
    MINI_CHECK_EQ(nBefore, Game::kWaveBase);
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
//   ✅ **屏幕上的 `'E'` 与面板真值对得上** ← 只有这个能守住 `active = false`
//      （`nAfter == panel.aliveTiles`：死掉的那个若**还在画**，屏幕上就会多一个字符 ⇒ 红）
//   ⚠️ 面板里出现 `Dead` **不能单独替代它**：状态来自**另一套真相**（`EnemyData.state`），
//      `active` 没置 false 时它照样成立（这正是 R6 全绿的直接原因）。
//      所以下面两条断言**各守一件事**，缺一不可 —— 面板那条先取、像素那条从同一个 `fb` 数。
//
//   ⚠️ **2026-10-10（S9 步骤 3 收尾）改过一次判据**：原来屏幕那条是
//      `nAfter == nBefore - 1`，它多隐含了一个**假前提**"一个敌人 = 屏幕上一个字符"
//      —— 而敌人**会叠格**（没有推挤，`C12` → S10）。改成"与面板真值比"之后，
//      重叠被算进期望值，守卫的力气没丢、假红没了（详见 `SummarizeEnemyPanel`）。
//
// 【它顺便补上了施工图里写不出来的那半条】
//   §10.1 用例 22 `RemovingEnemyLeavesNoDanglingId`（S8 拟的名）在 S8 无法实现（没有删除路径，
//   会是空用例，见偏差 8）。这条用例覆盖了"死亡 → 从屏幕上消失"的**端到端**；
//   ✅ **"不留悬空 id"那半边已由 `Game.KilledEnemyIsRemovedAndLeavesNoDanglingId` 补上**
//   （S9 步骤 4，2026-10-10 落地，文件末尾 G8）—— 两条都留着，各守一件事（分工见 G8 的注释）。
//
// 【观察面全是公开的】`F7` 只是 `Key` 枚举（`FakeInput` 照样能按）· 屏幕上的 `'@'`/`'E'`
//   · 面板文本（`ScreenText`）。
// ------------------------------------------------------------
MINI_TEST(Game, EnemyDiesAndDisappears)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    // 走到敌人身边：① 让**所有敌人都落在相机视口里**（这样"屏幕上少一个"才数得准）；
    //              ② 让玩家**面朝东** —— `WalkPlayerNextToEnemy` 一路按 `D` 走过来，
    //                 而分叉 ③ 是"移动方向即朝向"，所以这时开火就是朝那个方向打。
    MINI_CHECK(WalkPlayerNextToEnemy(game, input, fb) > 0);

    constexpr int kMaxEnemies = 8;
    Pos       before[kMaxEnemies] = {};
    const int nBefore = CollectEnemies(fb, before, kMaxEnemies);
    MINI_CHECK_EQ(nBefore, Game::kWaveBase);   // 第 1 波几个；同 `EnemiesPatrolUntilPlayerIsNear`

    // ⚠️ **2026-10-10（S9 步骤 3）改写过**：原来这里是"按 `F7` 开面板 + 按 `K` 杀敌"。
    //    `K` 连同它的枚举都删了（`TECH_DEBT` C15 到期）—— 真入口是子弹，所以改成**真的开枪**。
    // ⚠️ 打几发**由两个公开常量算**（`kEnemyMaxHp / kDefaultBullet.damage`），不写死 3。
    constexpr int kShotsToKill = Game::kEnemyMaxHp / game::kDefaultBullet.damage;
    for (int i = 0; i < kShotsToKill; ++i) {
        FireOnce(game, input, fb, kDt);          // ⑤c 生成一发
        StepFrames(game, input, fb, kDt, 30);    // 等它飞过去并结算（≤8 格 ÷ 45 格/秒 ≈ 11 帧）
    }

    // (1) 取证：开 `F7` 面板，拿到"面板真值 + 屏幕像素"**同一帧**的两个数。
    //     ⚠️ **2026-10-10 改过判据**：原来是 `nAfter == nBefore - 1`，它隐含
    //        "一个敌人 = 屏幕上一个字符" —— 而敌人**会叠格**（没有推挤，`C12` → S10）。
    //        现在换成"屏幕字符数 == 存活敌人所占的不同 tile 数"（见 `SummarizeEnemyPanel`）。
    const PanelSummary panel = OpenPanelAndSummarize(game, input, fb, kDt);

    Pos       after[kMaxEnemies] = {};
    const int nAfter = CollectEnemies(fb, after, kMaxEnemies);

    // 守 `active = false` 的那一条：屏幕上**确实少了一个**（`nBefore` 是 5，真值该是 4）
    MINI_CHECK_EQ(nAfter, panel.aliveTiles);

    // (2) 击杀计数（S9 新加的公开接口）：**打死了一个**（不是"打中了一次"）
    MINI_CHECK_EQ(game.Kills(), 1);

    // (3) 真删落地（S9 步骤 4）：`EnemyData` 也**当场**被移除了 ⇒ 表头 `n=5` 变 `n=4`。
    //     ⚠️ 判据从"面板里那一行还是 `Dead`"改成了"**行数少了一行**" —— 真删之后没有
    //        `state=Dead` 行可看了（S8 的取证手段到此为止，C16 到期；面板的行数本身就是结果）。
    MINI_CHECK_EQ(panel.enemyCount, nBefore - 1);
    MINI_CHECK(!panel.hasIdGone);   // 只删 Entity 不删 EnemyData 会留下 `<id gone>`
}

// ============================================================
// S9 的接线用例（步骤 3 加；原来那条 `EnemyDiesAndDisappears` 在上面已改写）
//
// ⚠️ 判据全部走**公开面**：屏幕上的字符（`'*'` / `'E'`）、`game.Facing()`、`game.Kills()`、
//    以及 `F7` 面板的文本。私有成员（`m_bullets` / `m_enemies` / `m_facing`）用例一概不碰。
// ⚠️ "打几发"一律由 `Game::kEnemyMaxHp / game::kDefaultBullet.damage` **算**出来，不写死 3。
// ============================================================

// ------------------------------------------------------------
// G1. 开火方向 = **当前朝向**；而朝向 = "移动方向，停下沿用"（分叉 ③ 的两半都钉）
// ------------------------------------------------------------
MINI_TEST(Game, FiringSpawnsBulletAlongFacing)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();
    StartPlaying(game, input, fb);   // ★ S10：先进 `Playing`

    StepFrame(game, input, fb, kDt);
    MINI_CHECK(game.Facing() == Facing8::East);   // 初值是确定性的：东
    MINI_CHECK_EQ(CountChar(fb, '*'), 0);         // 还没开过枪

    // 往北走一小步（只按一帧，别走到贴墙上）→ 朝向变北
    input.Press(Key::W);
    StepFrame(game, input, fb, kDt);
    input.Release(Key::W);
    MINI_CHECK(game.Facing() == Facing8::North);

    // **松开之后沿用**（分叉 ③ 的另一半）：不按键再推一帧，朝向不许变
    StepFrame(game, input, fb, kDt);
    MINI_CHECK(game.Facing() == Facing8::North);

    // 站着不动开火 ⇒ 子弹朝**北**飞
    FireOnce(game, input, fb, kDt);

    const Pos player = FindChar(fb, '@');
    const Pos bullet = FindChar(fb, '*');
    MINI_CHECK(player.found);
    MINI_CHECK(bullet.found);
    if (player.found && bullet.found) {
        MINI_CHECK_EQ(bullet.x, player.x);   // 同一列
        MINI_CHECK(bullet.y < player.y);     // 而且在**上面**：世界 Y 向下 ⇒ 北 = −Y
    }
}

// ------------------------------------------------------------
// G2. 边沿触发：**按住不放也只有一发**（不是"按住连射"）
//     ⚠️ 反向验证 R4 就是把 `kFireKey` 的判定从 `Pressed` 改成 `IsDown` —— 那时这里会红。
// ------------------------------------------------------------
MINI_TEST(Game, HoldingSpaceFiresOnlyOnce)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();
    StartPlaying(game, input, fb);   // ★ S10：先进 `Playing`（否则 `SPACE` 读不到）
    StepFrame(game, input, fb, kDt);

    // `FakeInput::Press` = "按下并保持"（直到 `Release`），所以这就是"按住不放"
    input.Press(Key::Space);
    StepFrames(game, input, fb, kDt, 3);

    // 3 帧里只该有 1 发。若改成"按住连射"，第 n 帧就已经有 n 发了（3 发布在相邻的格子里）
    MINI_CHECK_EQ(CountChar(fb, '*'), 1);
}

// ------------------------------------------------------------
// G3. 撞墙的子弹**会消失**（不是永远贴在墙面上）
//     ⚠️ 这条挡两种错：(a) 判定看到了墙但**忘了标删** —— 子弹会被 `MoveAndCollide`
//        夹在上墙面上、**永远画着**；(b) 判定根本没看到墙（那更明显）。
//     所以打完之后要**等它飞完**再查屏幕：此时一个 `'*'` 都不该剩。
// ------------------------------------------------------------
MINI_TEST(Game, BulletDiesOnWall)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();
    StartPlaying(game, input, fb);   // ★ S10：先进 `Playing`
    StepFrame(game, input, fb, kDt);

    // 面朝北（世界 Y 向下 ⇒ 北就是地图上边缘那堵墙的方向），只走一帧别贴到墙上
    input.Press(Key::W);
    StepFrame(game, input, fb, kDt);
    input.Release(Key::W);
    MINI_CHECK(game.Facing() == Facing8::North);

    FireOnce(game, input, fb, kDt);
    StepFrames(game, input, fb, kDt, 20);   // 2 格多一点的飞行，20 帧绰绰有余

    MINI_CHECK_EQ(CountChar(fb, '*'), 0);
}

// ------------------------------------------------------------
// G4. 一发**打不死**（`kEnemyMaxHp - 1` 发也不行）—— 从"不够"那一侧钉住伤害
//     ⚠️ 与 G5 是一对：G4 挡住"伤害太大（一击必杀）"，G5 挡住"伤害太小/没生效"。
// ------------------------------------------------------------
MINI_TEST(Game, BulletDamagesEnemy)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    MINI_CHECK(WalkPlayerNextToEnemy(game, input, fb) > 0);   // 走过去 = 朝东 = 正对着最东边那个敌人

    constexpr int kMaxEnemies = 8;   // = `kWaveMax`（封顶那一波的个数；够放下任何一波）
    Pos       before[kMaxEnemies] = {};
    const int nBefore = CollectEnemies(fb, before, kMaxEnemies);
    MINI_CHECK_EQ(nBefore, Game::kWaveBase);   // ★ 第 1 波几个（S10 步骤 4 起由波次决定）

    constexpr int kShotsToKill = Game::kEnemyMaxHp / game::kDefaultBullet.damage;
    for (int i = 0; i < kShotsToKill - 1; ++i) {   // 差一发
        FireOnce(game, input, fb, kDt);
        StepFrames(game, input, fb, kDt, 30);
    }

    // 取证：开 `F7` 面板（与像素取同一帧）。
    // ⚠️ 原来是 `nAfter == nBefore`（"一个都没少"）—— 同一个假前提（敌人会叠格），
    //    换成下面三条：真死 0 个 + 屏幕与真值一致 + `Kills()`。
    const PanelSummary panel = OpenPanelAndSummarize(game, input, fb, kDt);

    Pos       after[kMaxEnemies] = {};
    const int nAfter = CollectEnemies(fb, after, kMaxEnemies);

    MINI_CHECK_EQ(panel.enemyCount, nBefore);  // 一个都没死（面板表头 = `m_enemies.size()`）
    MINI_CHECK_EQ(nAfter, panel.aliveTiles);   // 屏幕画出来的 == 存活敌人占的不同格数
    MINI_CHECK_EQ(game.Kills(), 0);            // "打中"不等于"击杀"
}

// ------------------------------------------------------------
// G5. `kEnemyMaxHp / kDefaultBullet.damage` 发把它打死，`Kills()` 恰好 +1
// ------------------------------------------------------------
MINI_TEST(Game, ThreeHitsKillAnEnemy)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    MINI_CHECK(WalkPlayerNextToEnemy(game, input, fb) > 0);

    constexpr int kMaxEnemies = 8;   // = `kWaveMax`（封顶那一波的个数；够放下任何一波）
    Pos       before[kMaxEnemies] = {};
    const int nBefore = CollectEnemies(fb, before, kMaxEnemies);
    MINI_CHECK_EQ(nBefore, Game::kWaveBase);   // ★ 第 1 波几个（S10 步骤 4 起由波次决定）

    // ⚠️ 发数由公开常量算（不写死 3）：`kEnemyMaxHp` 3 ÷ `kDefaultBullet.damage` 1 = 3
    constexpr int kShotsToKill = Game::kEnemyMaxHp / game::kDefaultBullet.damage;
    for (int i = 0; i < kShotsToKill; ++i) {
        FireOnce(game, input, fb, kDt);
        StepFrames(game, input, fb, kDt, 30);
    }

    // 取证：开 `F7` 面板（与像素取同一帧）
    const PanelSummary panel = OpenPanelAndSummarize(game, input, fb, kDt);

    Pos       after[kMaxEnemies] = {};
    const int nAfter = CollectEnemies(fb, after, kMaxEnemies);

    MINI_CHECK_EQ(panel.enemyCount, nBefore - 1);   // 死了一个（面板表头少了 1）
    MINI_CHECK(!panel.hasIdGone);                   // 且没留下悬空 id
    MINI_CHECK_EQ(nAfter, panel.aliveTiles);        // 屏幕画出来的与真值一致
    MINI_CHECK_EQ(game.Kills(), 1);                 // 而且只记了一次（不是"每发打中 +1"）
}

// ------------------------------------------------------------
// G6. 数量有界：开完枪、**等它们全部消失**之后，屏幕上不该还剩任何 `'*'`
//
//     ⚠️ 判据写成"== 0"而不是"变少了"（`PLAN` A.6 第 ② 条）：子弹**会重叠**
//        —— 5 发叠在同一格里也只数得出 1 个 ⇒ "变少了"那种写法在漏删时照样绿。
//     ⚠️ 等待时间按"地图对角线 ÷ 弹速"给足（> 6.3 秒 = 378 帧），开 400 帧。
// ------------------------------------------------------------
MINI_TEST(Game, BulletsDoNotAccumulateForever)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();
    StartPlaying(game, input, fb);   // ★ S10：先进 `Playing`
    StepFrame(game, input, fb, kDt);

    // 面朝北（上墙只有 2 格远 ⇒ 每发都在几帧内撞墙消失）
    input.Press(Key::W);
    StepFrame(game, input, fb, kDt);
    input.Release(Key::W);

    for (int shot = 0; shot < 5; ++shot) {
        FireOnce(game, input, fb, kDt);
        StepFrames(game, input, fb, kDt, 10);
    }

    StepFrames(game, input, fb, kDt, 400);   // > 6.3 秒

    MINI_CHECK_EQ(CountChar(fb, '*'), 0);
}

// ------------------------------------------------------------
// G7. ★ 用 dt 的**上限**打一发 —— 一帧飞 2.25 格，照样命中
//
//     ⚠️ 这是分叉 ②（"防穿靶"）的**接线级证明**：`kMaxDelta = 0.05 s` ⇒
//        45 格/秒 × 0.05 = **2.25 格/帧**，而敌人只占 1 格。用"只看终点"的实现，
//        这几发子弹会**从敌人身上飞过去** ⇒ 敌人打不死 ⇒ 下面的 `Kills() == 1` 红。
//     ⚠️ 实测口径：它与 `Bullet.HitsWhenBothEndsOutside` 是同一件事的两个层次
//        （那条测纯函数，这条测"`Game` 真的把预测线段算对了"）。
// ------------------------------------------------------------
MINI_TEST(Game, BulletHitsEnemyEvenAtMaxDelta)
{
    constexpr float kDt = mini::core::kMaxDelta;   // 0.05 s —— dt 的**上限**

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    MINI_CHECK(WalkPlayerNextToEnemy(game, input, fb) > 0);

    constexpr int kMaxEnemies = 8;   // = `kWaveMax`（封顶那一波的个数；够放下任何一波）
    Pos       before[kMaxEnemies] = {};
    const int nBefore = CollectEnemies(fb, before, kMaxEnemies);
    MINI_CHECK_EQ(nBefore, Game::kWaveBase);   // ★ 第 1 波几个（S10 步骤 4 起由波次决定）

    constexpr int kShotsToKill = Game::kEnemyMaxHp / game::kDefaultBullet.damage;
    for (int i = 0; i < kShotsToKill; ++i) {
        FireOnce(game, input, fb, kDt);
        StepFrames(game, input, fb, kDt, 10);   // 2.25 格/帧 ⇒ 10 帧足够飞到（≤8 格）
    }

    // 取证：开 `F7` 面板（与像素取同一帧）
    const PanelSummary panel = OpenPanelAndSummarize(game, input, fb, kDt);

    Pos       after[kMaxEnemies] = {};
    const int nAfter = CollectEnemies(fb, after, kMaxEnemies);

    MINI_CHECK_EQ(panel.enemyCount, nBefore - 1);   // 真死了一个（表头少了 1）
    MINI_CHECK_EQ(nAfter, panel.aliveTiles);        // 屏幕与真值一致
    // ★ **判据是下面这一行**：穿靶的实现在这里必红（`Kills() == 0`）。
    // ⚠️ 上面那条屏幕断言 2026-10-10 从 `nAfter == nBefore - 1` 改成了"与面板真值比" ——
    //    本用例的检查点恰好比 G5 晚 0.1 秒，只有它踩到了"两只追兵汇进同一格"那个假红
    //    （理由与实测数据写在 `SummarizeEnemyPanel` 的注释里）。
    MINI_CHECK_EQ(game.Kills(), 1);
}

// ------------------------------------------------------------
// G8. ★ **真删**（`TECH_DEBT` **C16** 到期）：敌人死后**两边一起消失**、不留悬空 id
//
//     ⚠️ 它与 `EnemyDiesAndDisappears` 的分工（两条都要留着）：
//        · `EnemyDiesAndDisappears`（S8 加）守"**死亡 → 屏幕上消失**"（`active`/绘制的下游）；
//        · 这条守"**`m_enemies` 与 `m_world` 两边都真的删掉了**" —— 也就是 S8 分叉 ①
//          （两套真相）留下的那笔账：**只删一边都是 bug，而两种 bug 的症状不同**：
//            ① 只 `MarkForRemoval(Entity)` → `EnemyData` 是**悬空 id**（面板印 `<id gone>`）；
//            ② 只 erase `EnemyData`        → 实体还在，但**再没人驱动它**（原地当活靶子）。
//          反向验证 **R5** 打①、**R5′** 打②，各有多条断言会红。
//
//     ⚠️ 判据一律"**重叠免疫**"：面板表头的 `n=` / `<id gone>` / "不同 tile 数" ——
//        屏幕上数 `'E'` 不能当敌人数用（敌人会叠格，见 `SummarizeEnemyPanel`）。
//     ⚠️ **用例清单里它是 `Game` suite 的第 6 条，文件里排在最后**：不改既有 `G4`/`G5`/`G7`
//        的编号，免得 `S9_CHECKLIST` 偏差 9 那些引用指错人。
// ------------------------------------------------------------
MINI_TEST(Game, KilledEnemyIsRemovedAndLeavesNoDanglingId)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    MINI_CHECK(WalkPlayerNextToEnemy(game, input, fb) > 0);

    constexpr int kMaxEnemies = 8;   // = `kWaveMax`（封顶那一波的个数；够放下任何一波）
    Pos       before[kMaxEnemies] = {};
    const int nBefore = CollectEnemies(fb, before, kMaxEnemies);
    MINI_CHECK_EQ(nBefore, Game::kWaveBase);   // ★ 第 1 波几个（S10 步骤 4 起由波次决定）

    constexpr int kShotsToKill = Game::kEnemyMaxHp / game::kDefaultBullet.damage;
    for (int i = 0; i < kShotsToKill; ++i) {
        FireOnce(game, input, fb, kDt);
        StepFrames(game, input, fb, kDt, 30);
    }

    // 再推 30 帧才取证：① 让真删（⑤d 标删 → ⑨ `FlushRemovals`）彻底落定；
    // ② 顺便当"真删之后，每个系统仍能对每个 id 走一遍 `Find`"的冒烟。
    // ⚠️ `F7` 是**开关**（按一次开、再按一次关）⇒ 整条用例只按一次，取证只取一次。
    StepFrames(game, input, fb, kDt, 30);

    const PanelSummary panel = OpenPanelAndSummarize(game, input, fb, kDt);

    Pos       after[kMaxEnemies] = {};
    const int nAfter = CollectEnemies(fb, after, kMaxEnemies);

    // ① `m_enemies` 那边：表头 `n=` 真的少了一个（`EnemyData` 被 erase 了）
    MINI_CHECK_EQ(panel.enemyCount, nBefore - 1);
    // ② 没有悬空 id：面板**不该**出现 `<id gone>`（症状①，反向验证 R5）
    MINI_CHECK(!panel.hasIdGone);
    // ③ `m_world` 那边：屏幕上的 `'E'` == 面板各行占的**不同 tile** 数
    //    （实体若还留在世界里，就多画一个"没人驱动的活靶子" ⇒ 红。症状②，反向验证 R5′）
    MINI_CHECK_EQ(nAfter, panel.aliveTiles);
    // ④ 真删不该把击杀数再动一次
    MINI_CHECK_EQ(game.Kills(), 1);
}

// ------------------------------------------------------------
// G9. HUD **两行**（S9 步骤 5）：两行都不溢出、第二行有 `kills` 与 `dir`
//
//     ⚠️ 它把"人眼验收第 4 条（两行都不溢出、不折行）"变成机器守卫。溢出在终端里的
//        表现是**折行**（会把下面一行世界顶掉），肉眼可见，但机器看不见"折行"本身。
//     ⚠️ 所以**必须用一块比终端更宽的画布**来画：在 80 列画布上，溢出的部分会被
//        `FrameBuffer::Set` **静默裁掉** ⇒ 量出来永远是"没溢出"（一条空断言）。
//        宽画布量出的是**真实文本长度**，再拿它和真实终端宽（`kScreenW`）比。
//
//     ⚠️ 断"含子串"而不是"第几列是什么"（`ScreenText` 的注释里的纪律）。
//     ⚠️ 顺带钉住 `sumdt` / `v` 没被挤掉 —— 它们是 10.2 的自证项（`pos ≈ v × sumdt`），
//        不许为了让新栏目进来而砍掉。
// ------------------------------------------------------------
MINI_TEST(Game, HudFitsInTwoLinesWithoutOverflow)
{
    constexpr float kDt = 1.0f / 60.0f;

    constexpr int kProbeW = 200;   // 比终端宽得多：**为了让 HUD 文本不被裁掉**
    constexpr int kProbeH = 40;    // 高也要够（`kHudRow` 是行号，不改也行，留余量）

    FakeInput   input;
    Game        game;
    FrameBuffer probe(kProbeW, kProbeH);
    game.OnStart();
    StartPlaying(game, input, probe);   // ★ S10：先进 `Playing`，否则 `Title` 下根本不画 HUD
    StepFrame(game, input, probe, kDt);   // `StepFrame` 用 fb 的尺寸造 ctx ⇒ 游戏真的画进这块

    const int posRow   = kTestHudRow - 1;   // 行 1：位置类
    const int stateRow = kTestHudRow;       // 行 2：状态类（**同时**是世界视口的第一个逻辑行）

    std::string posLine;
    std::string stateLine;
    for (int x = 0; x < kProbeW; ++x) {
        posLine.push_back(probe.At(x, posRow).ch);
        stateLine.push_back(probe.At(x, stateRow).ch);
    }

    // ① 两行都真的画了，而且**真实长度都在终端宽度之内**（80 列 ⇒ 不折行）
    const std::size_t posEnd   = posLine.find_last_not_of(' ');
    const std::size_t stateEnd = stateLine.find_last_not_of(' ');
    MINI_CHECK(posEnd != std::string::npos);        // 行 1 不是空的（HUD 真的画了）
    MINI_CHECK(stateEnd != std::string::npos);      // 行 2 不是空的
    MINI_CHECK(posEnd != std::string::npos && posEnd < static_cast<std::size_t>(kScreenW));
    MINI_CHECK(stateEnd != std::string::npos && stateEnd < static_cast<std::size_t>(kScreenW));

    // ② 行 1（位置类）四栏都在 —— 特别是 `sumdt` / `v`（10.2 的自证项）
    MINI_CHECK(posLine.find("pos") != std::string::npos);
    MINI_CHECK(posLine.find("tile") != std::string::npos);
    MINI_CHECK(posLine.find("sumdt") != std::string::npos);
    MINI_CHECK(posLine.find("v ") != std::string::npos);

    // ③ 行 2（状态类）各栏都在。`dir ->` 是**确定性**的：`m_facing` 初值就是东
    //    ⚠️ 这条同时钉住分叉 ③ 的"必要配套"：朝向必须**看得见**，否则玩家不知道该往哪打。
    //    ⚠️ S10 补的两栏：`wave`（步骤 4 加的）与 `ammo`（步骤 5 加）—— 加它们的原因是
    //      **当时没有任何机器守卫**：`Ammo()` / `Wave()` 的用例读的是**访问器**，
    //      格式串里漏写一栏它们照样全绿（只有人眼能看出来）⇒ 这两行把那次缺口堵上。
    MINI_CHECK(stateLine.find("hp ") != std::string::npos);
    MINI_CHECK(stateLine.find("ammo") != std::string::npos);
    MINI_CHECK(stateLine.find("kills") != std::string::npos);
    MINI_CHECK(stateLine.find("wave") != std::string::npos);
    MINI_CHECK(stateLine.find("dir ->") != std::string::npos);
    // ④ `F7` 提示终于进了提示行（S8 偏差 6：单行时 78/80 列，塞不下第三个键）
    MINI_CHECK(stateLine.find("F7") != std::string::npos);

    // ⑤ ⚠️ HUD 文本里**不能**出现 `@` / `E` / `*`：`FindChar` / `CountChar` 是**全屏扫描**的，
    //    HUD 上一旦出现同名字符，那些"数画面上有几个"的用例会**静默失真**（不是红，是算错）。
    for (const std::string* line : {&posLine, &stateLine}) {
        MINI_CHECK(line->find('@') == std::string::npos);
        MINI_CHECK(line->find('E') == std::string::npos);
        MINI_CHECK(line->find('*') == std::string::npos);
    }
}

// ══════════════════════════════════════════════════════════════════════════
// S10 步骤 2：状态机接线（`Title` / `Playing` / `Paused` / `GameOver`）
//
// 【这 7 条在测什么】
//   那张决策表（`NextState` + `kStatePolicies`）在 `GameState` suite 里已经**逐条钉死**了；
//   这 7 条测的是**接线**：表真的被 `Game::OnUpdate` 用上了吗？
//   —— "表是对的、但忘了查表"是这类改动最典型的失败（写完 `PolicyOf` 却忘了包住那几步，
//      那样 `GameState` suite 全绿、游戏却照常在暂停里动）。
//
// 【观察面：两个】
//   ① `State()`（S10 新加的公开接口）
//   ② **屏幕文本** `ScreenText`（逐字节比较）+ HUD 的 `pos` 栏（截图/取行的老套路，见 G28）
//
//   ⚠️ "整屏逐字节不变"是这里的主力判据，理由是它**一次盖住**玩家、敌人、子弹，以及
//      HUD 里的 `sumdt` —— 而 `sumdt` 正是 10.2 自证链（`pos ≈ v × sumdt`）的输入，
//      冻结时它必须**一点也不涨**。代价是它很脆：任何"冻结时还该动的东西"冒出来都会红
//      （这正是想要的效果）。
//   ⚠️ 反向：**光有"不变"不够** —— 一条永远不动的空屏幕也能过。所以每条"冻结"用例都配了一个
//      "正向对照"（`Playing` 下真的会动 / 横幅真的画出来了 / 敌人真的在追）。
//
// 【与既有 18 条的接缝】
//   `m_state` 初值是 `Title` ⇒ `OnStart()` 之后世界**不会自己动**，所以既有用例都得先按一次
//   `Enter`（`StartPlaying`）。⚠️ 这条涟漪是 S10 施工图**漏算**的，见 `S10_CHECKLIST` 落地偏差 3。
// ══════════════════════════════════════════════════════════════════════════

// G1（施工图 §10.1 用例 1）：开局停在 `Title`，按 `Enter` 才进 `Playing`；进去之后**真的会动**。
MINI_TEST(Game, StartsInTitleAndEnterBeginsPlay)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    // ① `OnStart()` 之后**世界是停着的** —— 这就是 `PLAN` 要的"开局提示"
    MINI_CHECK(game.State() == GameState::Title);
    StepFrames(game, input, fb, kDt, 30);
    MINI_CHECK(game.State() == GameState::Title);   // 不按 Enter 就一直是 Title

    // ② 开局提示那一屏里**没有世界**：连 `@` 都不该有
    MINI_CHECK_EQ(CountChar(fb, '@'), 0);

    // ③ 按 `Enter` → `Playing`
    input.Press(Key::Enter);
    StepFrame(game, input, fb, kDt);
    input.Release(Key::Enter);
    MINI_CHECK(game.State() == GameState::Playing);

    // ③b ★ **第 1 波已经刷出来了**（`S10_CHECKLIST` 落地偏差 5：这条断言原来是"留给步骤 4"的）
    //     ⚠️ 期望值由公开常量算（`kWaveBase`），不写死数字。
    //     ⚠️ 判据用**面板的 `n=`**（= `m_enemies.size()` 真值）而不是屏幕上的 `'E'` 个数：
    //        敌人**会叠格**（没有推挤，`TECH_DEBT` C12）⇒ 数屏幕字符会少（`SummarizeEnemyPanel`）。
    MINI_CHECK_EQ(game.Wave(), 1);
    input.Press(Key::F7);                                  // 开面板（这一步之后一直开着）
    StepFrame(game, input, fb, kDt);
    input.Release(Key::F7);
    const PanelSummary panel = SummarizeEnemyPanel(ScreenText(fb));
    MINI_CHECK_EQ(panel.enemyCount, Game::kWaveBase);
    MINI_CHECK(!panel.hasIdGone);

    // ④ **正向对照**：`Playing` 下世界真的会动
    //    （少了这条，"接上了但五个开关一个都没打开"也会全绿）
    StepFrame(game, input, fb, kDt);
    const Pos before = FindChar(fb, '@');
    MINI_CHECK(before.found);

    input.Press(Key::D);
    StepFrames(game, input, fb, kDt, 30);   // 30 帧 × 20 格/秒 = 10 格（相机还没开始跟随）
    input.Release(Key::D);

    const Pos after = FindChar(fb, '@');
    MINI_CHECK(after.found);
    if (before.found && after.found) {
        MINI_CHECK(after.x > before.x);     // 往东走了（只比大小，不假设走几格）
        MINI_CHECK_EQ(after.y, before.y);
    }
}

// G2（§10.1 用例 2）：`Title` 下按住键推 60 帧，**整屏逐字节不变**。
MINI_TEST(Game, TitleDoesNotSimulate)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();
    StepFrame(game, input, fb, kDt);           // 第 0 帧：拿基线

    const std::string frozen = ScreenText(fb);
    // ⚠️ 先证明"这一屏真的是开局提示"：否则下面那条"不变"可能是在测一块空屏
    MINI_CHECK(frozen.find("PRESS ENTER TO START") != std::string::npos);

    // 按住 D + 按住 SPACE 再推 60 帧 —— **什么都不许变**：
    //   位置不动（`readMoveKeys` 关）、不出子弹（`readFireKey` 关）、
    //   `sumdt` 不涨（`advanceClock` 关；它没画在 Title 上，靠 `Playing` 那几条兜底）。
    input.Press(Key::D);
    input.Press(Key::Space);
    StepFrames(game, input, fb, kDt, 60);
    input.Release(Key::D);
    input.Release(Key::Space);

    MINI_CHECK(game.State() == GameState::Title);
    MINI_CHECK(ScreenText(fb) == frozen);      // ★ 整屏逐字节
}

// G3（§10.1 用例 3）：`Esc` 暂停 → 冻结；再按 `Esc` 恢复 → 又能动。
MINI_TEST(Game, EscapePausesAndResumes)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();
    StartPlaying(game, input, fb);
    StepFrame(game, input, fb, kDt);           // 让 `sumdt` 涨一点，后面"不许再涨"才有的谈

    // ① 按 `Esc` → `Paused`
    input.Press(Key::Escape);
    StepFrame(game, input, fb, kDt);
    input.Release(Key::Escape);
    MINI_CHECK(game.State() == GameState::Paused);

    StepFrame(game, input, fb, kDt);           // 消化 `Released`，拿干净基线
    const std::string frozen = ScreenText(fb);

    // ② 按住 D + SPACE 推 60 帧：**整屏逐字节不变**
    //    （含 HUD 上的 `sumdt`：暂停 1 秒它不许涨 1 秒 —— 涨了这条自证链就废了）
    input.Press(Key::D);
    input.Press(Key::Space);
    StepFrames(game, input, fb, kDt, 60);
    MINI_CHECK(game.State() == GameState::Paused);
    MINI_CHECK(ScreenText(fb) == frozen);
    //    ⚠️ 这一条其实被上一条盖住了（子弹会改变屏幕）；**分开写是为了红了能一眼看出是哪个开关**。
    MINI_CHECK_EQ(CountChar(fb, '*'), 0);      // 暂停时按 SPACE 不出子弹（`readFireKey` 关）

    // ③ 再按 `Esc` → `Playing`，而且**真的能动了**（"恢复"的正向对照）
    const Pos before = FindChar(fb, '@');
    MINI_CHECK(before.found);

    input.Release(Key::D);
    input.Release(Key::Space);
    input.Press(Key::Escape);
    StepFrame(game, input, fb, kDt);
    input.Release(Key::Escape);
    MINI_CHECK(game.State() == GameState::Playing);

    input.Press(Key::D);
    StepFrames(game, input, fb, kDt, 30);
    input.Release(Key::D);

    const Pos after = FindChar(fb, '@');
    MINI_CHECK(after.found);
    if (before.found && after.found) {
        MINI_CHECK(after.x > before.x);
    }
}

// G4（§10.1 用例 4）：**敌人在追人的状态下**暂停也不动。
//   ⚠️ 与 G3 的分工：G3 的场景里敌人还在巡逻（Patrol），本条的敌人已经进战（Chase）
//      —— Chase 每帧都会写一个"朝玩家"的速度，是"没关 `simulate`"最容易暴露的场景。
MINI_TEST(Game, PausedEnemyDoesNotMove)
{
    constexpr float kDt = 1.0f / 60.0f;
    constexpr int   kMaxEnemies = 8;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    // 走到敌人跟前 ⇒ 它们已经进战，每帧都在朝玩家写速度
    MINI_CHECK(WalkPlayerNextToEnemy(game, input, fb) > 0);

    input.Press(Key::Escape);
    StepFrame(game, input, fb, kDt);
    input.Release(Key::Escape);
    MINI_CHECK(game.State() == GameState::Paused);
    StepFrame(game, input, fb, kDt);           // 消化 `Released`

    Pos       before[kMaxEnemies] = {};
    const int nBefore = CollectEnemies(fb, before, kMaxEnemies);
    MINI_CHECK(nBefore > 0);

    StepFrames(game, input, fb, kDt, 60);      // 1 秒：平时它们能贴上来好几格

    Pos       after[kMaxEnemies] = {};
    const int nAfter = CollectEnemies(fb, after, kMaxEnemies);

    MINI_CHECK_EQ(nAfter, nBefore);
    // 屏幕上的 `E` 位置**逐个**对得上（行优先扫描 ⇒ 顺序确定，可以直接按位比）
    for (int i = 0; i < nBefore && i < nAfter; ++i) {
        MINI_CHECK_EQ(after[i].x, before[i].x);
        MINI_CHECK_EQ(after[i].y, before[i].y);
    }
}

// G5（§10.1 用例 5）：HP 归 0 → **下一帧**才 `GameOver`。
//   ⚠️ 这条钉的是施工图 §7.1 理由 1 的那个取舍：转移只在 ⓪（读方向键之前）⇒ 死亡响应
//      **最早等下一帧**。把 ⓪ 挪到读键之后，这条会红（归零那一帧就转态了）。
MINI_TEST(Game, PlayerHpReachesZeroThenGameOver)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    MINI_CHECK(WalkPlayerNextToEnemy(game, input, fb) > 0);

    // 站着被打到 0。⚠️ **一归零就停**：多推一帧就会把状态转掉，下面那条断言就失效了
    for (int i = 0; i < 720 && game.PlayerHp() > 0; ++i) {
        StepFrame(game, input, fb, kDt);
    }
    MINI_CHECK_EQ(game.PlayerHp(), 0);

    // ★ 归零的**那一帧**：状态还是 `Playing`（伤害在 ⑤b，而转移只在下一帧的 ⓪）
    MINI_CHECK(game.State() == GameState::Playing);

    StepFrame(game, input, fb, kDt);
    MINI_CHECK(game.State() == GameState::GameOver);
}

// G6（§10.1 用例 6 · `PLAN` 的验收原话："HP 归零后不再响应移动"）
MINI_TEST(Game, GameOverIgnoresMoveInput)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    MINI_CHECK(KillPlayer(game, input, fb));   // 走过去被打死，停在 `GameOver`
    MINI_CHECK(game.State() == GameState::GameOver);

    StepFrame(game, input, fb, kDt);           // 消化输入状态，拿干净基线

    // 判据取 **HUD 第 1 行的 `pos` 栏**（而不是屏幕上的 `@`）：
    //   `@` 可能被同格的敌人**盖住**（敌人之间没有推挤，`TECH_DEBT` C12）⇒ 会假红；
    //   而 HUD 是**最后画**的，`pos` 就是玩家自己的世界坐标 ⇒ 既不会被盖住，又比"动没动"精确
    //   （位移 0.01 都会让它变）。⚠️ 取行的套路见 `HudFitsInTwoLinesWithoutOverflow`。
    const int posRow = kTestHudRow - 1;
    std::string posBefore;
    for (int x = 0; x < fb.Width(); ++x) {
        posBefore.push_back(fb.At(x, posRow).ch);
    }
    MINI_CHECK(posBefore.find("pos") != std::string::npos);   // 这一行真的是 pos 栏

    input.Press(Key::D);
    StepFrames(game, input, fb, kDt, 60);
    input.Release(Key::D);

    std::string posAfter;
    for (int x = 0; x < fb.Width(); ++x) {
        posAfter.push_back(fb.At(x, posRow).ch);
    }

    // ★ 按住 D 推 1 秒，`pos` **一个字符都不许变**（`readMoveKeys` 关 ⇒ 连速度都不重写）
    MINI_CHECK(posAfter == posBefore);
    MINI_CHECK(game.State() == GameState::GameOver);   // 顺便：按键也不会把状态顶出去
}

// G7（§10.1 用例 7）：`GameOver` 下**整个世界**冻住（敌人 + 子弹 + HUD 的 `sumdt`）。
MINI_TEST(Game, GameOverFreezesWorld)
{
    constexpr float kDt = 1.0f / 60.0f;
    constexpr int   kMaxEnemies = 8;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    MINI_CHECK(KillPlayer(game, input, fb));
    MINI_CHECK(game.State() == GameState::GameOver);

    StepFrame(game, input, fb, kDt);           // 消化输入状态，拿干净基线
    const std::string frozen = ScreenText(fb);
    // ⚠️ 先证明结算横幅真的画出来了（否则"整屏不变"可能是在测一块空屏）
    MINI_CHECK(frozen.find("GAME OVER") != std::string::npos);

    Pos       before[kMaxEnemies] = {};
    const int nBefore = CollectEnemies(fb, before, kMaxEnemies);

    input.Press(Key::D);
    input.Press(Key::Space);
    StepFrames(game, input, fb, kDt, 120);     // 2 秒：平时敌人能贴上来、子弹能飞出去
    input.Release(Key::D);
    input.Release(Key::Space);

    MINI_CHECK(game.State() == GameState::GameOver);
    MINI_CHECK(ScreenText(fb) == frozen);      // ★ 整屏逐字节（含 HUD 的 `sumdt`）
    MINI_CHECK_EQ(CountChar(fb, '*'), 0);      // 结算后按 SPACE 也不出子弹

    // 敌人位置**逐个**对得上（这一条与"整屏不变"重复，但红了能直接指出是敌人没冻）
    Pos       after[kMaxEnemies] = {};
    const int nAfter = CollectEnemies(fb, after, kMaxEnemies);
    MINI_CHECK_EQ(nAfter, nBefore);
    for (int i = 0; i < nBefore && i < nAfter; ++i) {
        MINI_CHECK_EQ(after[i].x, before[i].x);
        MINI_CHECK_EQ(after[i].y, before[i].y);
    }
}

// ══════════════════════════════════════════════════════════════════════════
// S10 步骤 3：`StartNewRun()` —— 一局的唯一入口（开局与重开同一条路）
//
// 【这 2 条在测什么】
//   `StartNewRun` 的要害是**一份 7 步清单**（清实体 → 清两张 id 表 → 标量归位 → 重铺地图
//   → Spawn 玩家 → 铺敌人）。清单这种东西**漏一行不会报错**，只会让上一局的东西活到下一局
//   —— 所以必须由用例把"清干净了"变成**可断言**的。
//
// 【观察面】
//   ① 公开接口：`Kills()` / `PlayerHp()` / `State()`
//   ② 屏幕：`CountChar(fb,'*')`（上一局的子弹）、`CountChar(fb,'E')`（敌人画出来了吗）、
//      `CountChar(fb,'@')`（玩家在新 run 里是**新 id**、还被画得出来）
//   ③ **F7 面板**：`n=`（= `m_enemies.size()` 的真值）+ `<id gone>`（悬空 id 唯一能被看见的地方）
//   ⚠️ ②与③要**一起**用（坑表 14）：屏幕上少画一个 `E` 可能是"真少了"也可能是"两只叠在一格"（C12），
//      只有面板真值能区分。反过来面板也会说谎（它读的是 `m_enemies`，不看渲染）—— 两边都得对。
// ══════════════════════════════════════════════════════════════════════════

// G8（§10.1 用例 8）：重开之后**不留上一局的任何东西**。
//   ⚠️ 每一条断言对应重开清单里的一步 —— 红了就能直接指出**哪一步漏了**（见断言后的括注）。
MINI_TEST(Game, RestartLeavesNoStaleState)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();

    // ── ① 先把这一局**弄脏**：每一处脏都对应清单里的一条 ────────────────────
    //   · 开枪      → `m_bullets` 进过东西（→ 第 3 步）
    //   · 打死一个  → `Kills() > 0`、`m_enemies` 少一个、**那个 id 永久消失**（→ 第 3 步）
    //   · 一路往东走 → `m_facing` 过程里变过（→ 第 4 步）
    //   · 被打 + 推帧 → `m_playerHp < kPlayerMaxHp`、`m_elapsed > 0`（→ 第 4 步）
    MINI_CHECK(WalkPlayerNextToEnemy(game, input, fb) > 0);

    // ⚠️ 发数由公开常量算（不写死 3）：`kEnemyMaxHp` 3 ÷ `kDefaultBullet.damage` 1 = 3
    constexpr int kShotsToKill = Game::kEnemyMaxHp / game::kDefaultBullet.damage;
    for (int i = 0; i < kShotsToKill; ++i) {
        FireOnce(game, input, fb, kDt);
        StepFrames(game, input, fb, kDt, 30);
    }
    MINI_CHECK_EQ(game.Kills(), 1);          // ← "弄脏"成功；下面要看到它回 0

    // ── ② 站着被打到死 → `GameOver`（**重开的前提**：`R` 只在 `GameOver` 有效）──────
    // ⚠️ 这里**不用 `KillPlayer`**：它内部会再走一次 `WalkPlayerNextToEnemy`，而此刻敌人已经贴上来了，
    //    "最近距离平方"可能正好是 0（`WalkPlayerNextToEnemy` 把 0 当失败返回）⇒ 会假红。
    for (int i = 0; i < 720 && game.PlayerHp() > 0; ++i) {
        StepFrame(game, input, fb, kDt);
    }
    MINI_CHECK_EQ(game.PlayerHp(), 0);
    for (int i = 0; i < 3 && game.State() != GameState::GameOver; ++i) {
        StepFrame(game, input, fb, kDt);
    }
    MINI_CHECK(game.State() == GameState::GameOver);

    // ── ③ 按 `R` 重开（与 `Title` 按 `Enter` 走的是**同一条路**）────────────────
    input.Press(Key::R);
    StepFrame(game, input, fb, kDt);
    input.Release(Key::R);
    StepFrame(game, input, fb, kDt);
    MINI_CHECK(game.State() == GameState::Playing);   // R 之后**没有再闪回** GameOver

    // ── ④ 断言"清干净了"（逐条对 §10.1 用例 8）
    MINI_CHECK_EQ(game.Kills(), 0);                            // ← 第 4 步：击杀数归 0
    MINI_CHECK_EQ(game.PlayerHp(), Game::kPlayerMaxHp);        // ← 第 4 步：HP 回满（期望值由公开常量算）
    // ← S10 步骤 4/5 补的两条（用例 8 的清单里写着 `Wave == 1` / `Ammo == kStartAmmo`，
    //    但那时这两个成员还不存在 ⇒ 到本步才补上）
    MINI_CHECK_EQ(game.Wave(), 1);                             // 第 7 步 `BeginWave(1)` 设的
    MINI_CHECK_EQ(game.Ammo(), Game::kStartAmmo);              // `BeginWave` 的"每波回满"（含重开那一波）
    MINI_CHECK_EQ(CountChar(fb, '*'), 0);                      // ← 第 3 步：上一局的子弹一发不剩

    // ← 第 1/2/3 步：实体清空后重铺 + `m_enemies` 清空 ⇒ 个数对、且**没有悬空 id**
    const PanelSummary panel = OpenPanelAndSummarize(game, input, fb, kDt);
    // ⚠️ 第 1 波几个（S10 步骤 4 起由波次决定）；同 `EnemiesPatrolUntilPlayerIsNear` 那条注释。
    MINI_CHECK_EQ(panel.enemyCount, Game::kWaveBase);
    // ★ "上一局的 id 一个都查不到"在公开面上的直接体现：若 `m_enemies` 没清，
    //   那些残留 `EnemyData` 会指向**已经删掉的**实体 ⇒ 面板立刻印出 `<id gone>`
    MINI_CHECK(!panel.hasIdGone);
    MINI_CHECK_EQ(panel.aliveTiles, panel.enemyCount);   // 屏幕画出来的与面板真值一致（渲染没漏）

    // ★ 另一半：`m_player` 必须指向**新**那个实体。旧 id 已经删了，若 `m_player` 没跟着换，
    //   `Find(m_player)` 就是 nullptr ⇒ 玩家不画、`DrawHud` 早退 ⇒ 屏幕上根本没有 '@'。
    //   （⚠️ "碰巧对"最危险：若重开**重建**了 `World`，旧的 id 1 会被新玩家重新用上，
    //    这一条照样绿 —— 所以它只守"玩家换了"，不守"id 不复用"；后者由**构造**保证，见分叉 ②）
    MINI_CHECK_EQ(CountChar(fb, '@'), 1);
}

// G9（§10.1 用例 9 · `PLAN` 的验收原话"**循环 3 次**"）：重开 3 次不累积。
MINI_TEST(Game, RestartThreeTimesDoesNotAccumulate)
{
    constexpr float kDt      = 1.0f / 60.0f;
    constexpr int   kRestarts = 3;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();
    StartPlaying(game, input, fb);

    // 面板**一次打开到底**：`StartNewRun` 刻意不清 `m_showAiPanel`（坑表 9）
    // ⇒ 之后每轮直接读，不必来回按 F7（`OpenPanelAndSummarize` 每次都会**翻转**，按第二次就关了，
    //   那时读到的 `n=0` 会变成假红）。
    input.Press(Key::F7);
    StepFrame(game, input, fb, kDt);
    input.Release(Key::F7);

    int enemiesAfterFirstRun = -1;

    for (int run = 0; run < kRestarts; ++run) {
        MINI_CHECK(KillPlayer(game, input, fb));      // 打死 → 停在 GameOver

        input.Press(Key::R);
        StepFrame(game, input, fb, kDt);
        input.Release(Key::R);
        StepFrame(game, input, fb, kDt);
        MINI_CHECK(game.State() == GameState::Playing);

        // 判据成对出现（坑表 14）：屏幕"画了几个 E" + 面板"真值几个 / 有没有悬空 id"
        const int           drawn = CountChar(fb, 'E');
        const PanelSummary  panel = SummarizeEnemyPanel(ScreenText(fb));

        MINI_CHECK(!panel.hasIdGone);
        MINI_CHECK_EQ(drawn, panel.enemyCount);
        MINI_CHECK_EQ(panel.aliveTiles, panel.enemyCount);

        if (run == 0) {
            enemiesAfterFirstRun = panel.enemyCount;
            // 第 1 波几个（同上；S10 步骤 4 起由波次决定）
            MINI_CHECK_EQ(enemiesAfterFirstRun, Game::kWaveBase);
        } else {
            // ★ 核心：第 2、3 次重开之后，场上敌人个数与**第 1 次完全相同**（不累积）
            MINI_CHECK_EQ(panel.enemyCount, enemiesAfterFirstRun);
        }
    }

    // 三次重开之后，数值状态仍是"一局刚开始"的样子
    MINI_CHECK_EQ(game.Kills(), 0);
    MINI_CHECK_EQ(game.PlayerHp(), Game::kPlayerMaxHp);
    MINI_CHECK_EQ(CountChar(fb, '*'), 0);
}

// ══════════════════════════════════════════════════════════════════════════
// S10 步骤 4：波次（分叉 ③ 取甲：固定模板 + 数量递增 + 封顶）
//
// 【两条用例的分工（**这是本步最重要的设计决定**）】
//   · `WaveEnemyCountGrowsAndIsCapped` —— 管**公式**：逐 n 核"每波 +growth、到 max 封顶"，
//     覆盖到远超封顶（n = 1..20）。⚠️ 它是**纯函数测试**：`WaveEnemyCount` 是
//     `static constexpr` ⇒ 不用造 `Game`、不用推帧。
//   · `WaveAdvancesWhenAllEnemiesDie` —— 管**接线**：真的清完一波、真的过了喘息、
//     真的刷出下一波（第 1 波 3 个 → 第 2 波 4 个）。
//
//   ⚠️ **为什么不把"封顶"也交给接线那条测**：那要连打 6 波、杀掉 3+4+5+6+7 = 25 个敌人
//      （约 75 发子弹、5000+ 帧），而且第 3 波起玩家几乎必死（最多 8 个敌人贴脸，
//      而且**没有推挤**，`TECH_DEBT` C12）⇒ 那种用例**又慢又脆**，红了还分不清是
//      "公式错了"还是"玩家死了"。⇒ 把可判定的那半（公式）拆成纯函数，是本项目
//      "决策与接线分家"的第 5 次实践。
// ══════════════════════════════════════════════════════════════════════════

// G10（§10.1 用例 10）：清完一波 → 喘息 → 下一波。
MINI_TEST(Game, WaveAdvancesWhenAllEnemiesDie)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();
    StartPlaying(game, input, fb);

    // 面板开一次**一直开着**（`StartNewRun`/`BeginWave` 都不清 `m_showAiPanel`，坑表 9）
    // ⇒ 全程用 `n=` 读真值。⚠️ 不数屏幕上的 `'E'`：敌人会叠格（C12）。
    input.Press(Key::F7);
    StepFrame(game, input, fb, kDt);
    input.Release(Key::F7);

    MINI_CHECK_EQ(game.Wave(), 1);
    MINI_CHECK_EQ(SummarizeEnemyPanel(ScreenText(fb)).enemyCount, Game::kWaveBase);

    // ── ① 站到"最近敌人 8 格"处 ─────────────────────────────────────────────
    //    停下来的那一行 = 前两个敌人的巡逻线（都是 y = 2.5）⇒ 向东开火必中。
    MINI_CHECK(WalkPlayerNextToEnemy(game, input, fb) > 0);

    // ── ② 一直向东开枪，直到面板说敌人全没了 ─────────────────────────────────
    //   ⚠️ 为什么"向东开火"能清完（三条几何事实，落地时逐条核过）：
    //     · 第 1 波 3 个出生在 `kPatrolPaths` 第 0/1/2 行 = (44.5,2.5) (60.5,2.5) (44.5,4.5)；
    //     · 前两个的 y 与玩家相同 ⇒ 占**同一格行** ⇒ 向东的子弹必中
    //       （命中判定是**按格**的：采样点所在格 == 敌人所在格，见 `Bullet.h`）；
    //     · 第三个从 8 格以外沿直线追过来、停在离玩家 1.5 格处 ⇒ 纵向只偏
    //       2 × (1.5/8.25) ≈ 0.36 格 ⇒ **格行不变**（还是同一行）⇒ 照样中。
    //   ⚠️ 逐帧推进而不是"固定推 40 帧"：**倒计时的起点要尽量准**（下面 ③ 要用它）。
    //   ⚠️ 上限 = `kStartAmmo`（S10 步骤 5 起"开火要花弹药"）：3 个敌人要 9 发、
    //      每次击杀返还 1 ⇒ 预算 **18 + 3 = 21** 发 ⇒ 上限取 18 与弹药**同源**
    //      （避免把"清不完"和"打光了"两种失败混成同一个症状）。
    constexpr int kMaxShots = Game::kStartAmmo;
    int shots = 0;
    while (shots < kMaxShots && SummarizeEnemyPanel(ScreenText(fb)).enemyCount > 0) {
        FireOnce(game, input, fb, kDt);
        ++shots;
        // 最多等 45 帧（够子弹飞到最远那个：24 格 ÷ 45 格/秒 ≈ 32 帧），一清空就立刻停
        for (int f = 0; f < 45 && SummarizeEnemyPanel(ScreenText(fb)).enemyCount > 0; ++f) {
            StepFrame(game, input, fb, kDt);
        }
    }
    MINI_CHECK(shots < kMaxShots);                                     // 没打光上限就打完了
    MINI_CHECK_EQ(SummarizeEnemyPanel(ScreenText(fb)).enemyCount, 0);
    MINI_CHECK_EQ(game.Kills(), Game::kWaveBase);                      // 全清 ⇒ 击杀数 = 第 1 波个数
    MINI_CHECK(game.State() == GameState::Playing);                    // ⚠️ 玩家还活着（不然下面没意义）

    // ── ③ 喘息期：**还没到点之前不许刷下一波** ────────────────────────────────
    //   ⚠️ 波次只在"新一波真的刷出来"时才 +1 ⇒ 喘息期间 `Wave()` 仍是 1（这是可观察的"喘息"）。
    //   ⚠️ 倒计时起点 = **清完那一帧**（`TickWave` 在 ⑤x，而真删在 ⑤d 当帧就完成了）。
    const int breakFrames = static_cast<int>(Game::kWaveBreakSeconds * 60.0f);
    MINI_CHECK_EQ(game.Wave(), 1);
    StepFrames(game, input, fb, kDt, breakFrames - 6);      // 留 6 帧余量（浮点累减 + 帧边界）
    MINI_CHECK_EQ(game.Wave(), 1);                          // ★ 喘息中：还没刷
    MINI_CHECK_EQ(SummarizeEnemyPanel(ScreenText(fb)).enemyCount, 0);

    // ── ④ 到点 → 第 2 波：`Wave()` +1 **且**敌人重新出现（数量 = 第 2 波该有的）────────
    StepFrames(game, input, fb, kDt, 12);
    MINI_CHECK_EQ(game.Wave(), 2);
    MINI_CHECK_EQ(SummarizeEnemyPanel(ScreenText(fb)).enemyCount,
                  Game::kWaveBase + Game::kWaveGrowth);
    MINI_CHECK(!SummarizeEnemyPanel(ScreenText(fb)).hasIdGone);        // 新一波没有悬空 id
}

// G11（§10.1 用例 11）：第 n 波数量 = 每波 +`kWaveGrowth`、到 `kWaveMax` 封顶。
//   ⚠️ **纯函数测试**（`WaveEnemyCount` 是 `static constexpr`，不用造 `Game`、不用推帧）——
//      理由见上面那段"两条用例的分工"。
MINI_TEST(Game, WaveEnemyCountGrowsAndIsCapped)
{
    // ⚠️ 不拿 `WaveEnemyCount(n)` 自己算期望值（那是空断言）。这里核的是**性质**：
    //    相邻波的差 == `kWaveGrowth`（未封顶时），封顶后恒 == `kWaveMax`。
    constexpr int kLastWave = 20;   // 远超封顶该出现的那一波（覆盖"涨上去 + 停住"两段）

    int prev = Game::WaveEnemyCount(1);
    MINI_CHECK_EQ(prev, Game::kWaveBase);          // 第 1 波 = 基数

    for (int n = 2; n <= kLastWave; ++n) {
        const int now = Game::WaveEnemyCount(n);

        if (prev < Game::kWaveMax) {
            MINI_CHECK_EQ(now, prev + Game::kWaveGrowth);   // 还没封顶：每波涨固定值
        } else {
            MINI_CHECK_EQ(now, Game::kWaveMax);             // 已封顶：不再涨
        }

        MINI_CHECK(now <= Game::kWaveMax);   // ★ 任何一波都不许超过封顶（越界 = 读表越界）
        MINI_CHECK(now >= 1);                // 也不许变成 0 / 负数（否则波次会自激）

        prev = now;
    }

    // 封顶**确实发生**（若有人把 `kWaveGrowth` 改成 0，上面每条都会"遵守不变式"却永远不封顶
    // ⇒ 这条把它钉住：到第 20 波必须已经贴着封顶了）
    MINI_CHECK_EQ(Game::WaveEnemyCount(kLastWave), Game::kWaveMax);

    // 巡逻表长必须 ≥ 封顶值 —— ⚠️ **这条耦合不在这里核**：它是**编译期**的
    // （`Game.h` 里那句 `static_assert(kWaveMax <= kPatrolPathRows)`），而 `kPatrolPathRows`
    // 是 private ⇒ 用例只能拿字面量去比，那是**比 `static_assert` 更弱**的重复守卫。
}

// ══════════════════════════════════════════════════════════════════════════
// S10 步骤 5：弹药（分叉 ④ 定稿 = 乙+丁：**每波回满** 且 **每击杀 +kAmmoPerKill**，cap = 初值）
//
// 【这四条的分工】
//   · `AmmoDropsByOnePerShot`   扣减：**对墙**打 —— 没有击杀 ⇒ 与"返还"完全解耦；
//   · `AmmoEmptyBlocksFiring`   打光：`== 0` 之后**屏幕上不再出新 `*`**，且**不为负**；
//   · `AmmoRefundHappensOnlyOnKill` 返还：★ 差**恰好**是 `kAmmoPerKill`（不是"打中一次 +1"）；
//   · `AmmoIsCappedAtMax`       夹取：任何时刻 `Ammo() <= kMaxAmmo` 且 `>= 0`。
//
//   ⚠️ **为什么扣减那条一定要"对墙"**：拿敌人测就分不清"扣了 3 发"和"扣了 3 发又还了 1 发"。
//      对墙开火是**唯一**能把两条通路拆开的动线（墙上没有敌人 ⇒ 不可能有击杀）。
// ══════════════════════════════════════════════════════════════════════════

// G12（§10.1 用例 12）：每开一枪扣 1 发（对墙 ⇒ 不掺返还）。
MINI_TEST(Game, AmmoDropsByOnePerShot)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();
    StartPlaying(game, input, fb);

    MINI_CHECK_EQ(game.Ammo(), Game::kStartAmmo);   // 开局满弹（`BeginWave(1)` 里回的）
    MINI_CHECK_EQ(game.Kills(), 0);

    // 面朝北：上墙只有 2 格远 ⇒ 每发都在几帧内撞墙消失（屏幕上不会越积越多）
    input.Press(Key::W);
    StepFrame(game, input, fb, kDt);
    input.Release(Key::W);

    // 打 `kShots` 发（不发敌人 ⇒ **一定**没有击杀返还）
    constexpr int kShots = 5;
    for (int i = 0; i < kShots; ++i) {
        FireOnce(game, input, fb, kDt);
        StepFrames(game, input, fb, kDt, 10);
    }

    MINI_CHECK_EQ(game.Kills(), 0);                             // 确认"没有返还"这个前提
    MINI_CHECK_EQ(game.Ammo(), Game::kStartAmmo - kShots);      // ★ 期望值由公开常量算
}

// G13（§10.1 用例 13）：打光之后**开不了火**（屏幕不出新 `*`），且弹药不为负。
MINI_TEST(Game, AmmoEmptyBlocksFiring)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();
    StartPlaying(game, input, fb);

    // 面朝北（同 G12）
    input.Press(Key::W);
    StepFrame(game, input, fb, kDt);
    input.Release(Key::W);

    // ① 把弹药**打到正好 0**（打 `kStartAmmo` 发，一发都不打敌人 ⇒ 无返还）
    for (int i = 0; i < Game::kStartAmmo; ++i) {
        FireOnce(game, input, fb, kDt);
        StepFrames(game, input, fb, kDt, 10);
    }
    MINI_CHECK_EQ(game.Ammo(), 0);
    MINI_CHECK_EQ(game.Kills(), 0);

    // 等所有子弹撞墙消失（上墙 2 格远 ⇒ 10 帧足够；留足余量）
    StepFrames(game, input, fb, kDt, 30);
    MINI_CHECK_EQ(CountChar(fb, '*'), 0);   // ← 清干净了，下面"不出新 `*`"才有意义

    // ② 再按一次 `SPACE`：**什么都不该发生**
    FireOnce(game, input, fb, kDt);
    MINI_CHECK_EQ(CountChar(fb, '*'), 0);   // ★ 打光了就是"屏幕上不出新子弹"
    MINI_CHECK_EQ(game.Ammo(), 0);          // ★ 也不许变成负数
    MINI_CHECK_EQ(game.Kills(), 0);

    // ③ 再多按两下也一样（不是"只在第 1 次挡住"）
    FireOnce(game, input, fb, kDt);
    FireOnce(game, input, fb, kDt);
    StepFrames(game, input, fb, kDt, 5);
    MINI_CHECK_EQ(CountChar(fb, '*'), 0);
    MINI_CHECK_EQ(game.Ammo(), 0);
}

// G14（§10.1 用例 14）：★ 返还在"**击杀**"那一次，**不是**"打中一次"。
//     即：打死一个（3 发）⇒ 净扣 3、返还 `kAmmoPerKill` **1** 发（不是 3 发）。
MINI_TEST(Game, AmmoRefundHappensOnlyOnKill)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();
    StartPlaying(game, input, fb);

    MINI_CHECK(WalkPlayerNextToEnemy(game, input, fb) > 0);

    constexpr int kShotsToKill = Game::kEnemyMaxHp / game::kDefaultBullet.damage;
    for (int i = 0; i < kShotsToKill; ++i) {
        FireOnce(game, input, fb, kDt);
        StepFrames(game, input, fb, kDt, 30);
    }

    MINI_CHECK_EQ(game.Kills(), 1);   // 确认"恰好发生了一次击杀"

    // 没击杀时该剩 `kStartAmmo - kShotsToKill`（同 G12 的算式）；实际多出来的就是返还量。
    constexpr int kNoKillAmmo = Game::kStartAmmo - kShotsToKill;
    MINI_CHECK_EQ(game.Ammo() - kNoKillAmmo, Game::kAmmoPerKill);

    // ★ **这才是这条用例的靶子**：实现若写成"打中一次 +1"，上面的差值会 = `kShotsToKill`（3）
    //   ⇒ 红。⚠️ 所以判据必须是"差值 == `kAmmoPerKill`"，而不是"Ammo() 变大了"。
    MINI_CHECK(Game::kAmmoPerKill < kShotsToKill);   // 前提：两种实现的差值本来就不相等
    MINI_CHECK_EQ(game.Ammo(), kNoKillAmmo + Game::kAmmoPerKill);
}

// G15（§10.1 用例 15）：夹取守卫 —— **任何时刻 `0 <= Ammo() <= kMaxAmmo`**。
//
// ⚠️ **诚实说明（别把它写成"刚好顶到 cap"）**：在**本步的常量**下
//    （`kMaxAmmo == kStartAmmo == 18`、`kAmmoPerKill == 1`、打死一个至少要 3 发），
//    "刚好把 cap 顶满"这个**边界打不到**：击杀之前必然已经打掉 ≥ 3 发 ⇒ `m_ammo ≤ 15`
//    ⇒ `+1` 之后 ≤ 16 < 18。⇒ 所以这条核的是**不变量**（"永远不超上限、不为负"），
//    而不是"在某一点被夹住"。
//    ⚠️ 一旦有人把 `kAmmoPerKill` 调到 ≥ 3（或让 `kMaxAmmo` 小于起手值），
//      这条会立刻变成真正的边界用例 —— 而它守的还是同一件事。
MINI_TEST(Game, AmmoIsCappedAtMax)
{
    constexpr float kDt = 1.0f / 60.0f;

    FakeInput   input;
    Game        game;
    FrameBuffer fb(kScreenW, kScreenH);
    game.OnStart();
    StartPlaying(game, input, fb);

    // ① 开局：满弹，但**不超上限**
    MINI_CHECK_EQ(game.Ammo(), Game::kStartAmmo);
    MINI_CHECK(game.Ammo() <= Game::kMaxAmmo);
    MINI_CHECK(game.Ammo() >= 0);

    // ② 走过去打死一个：**每一发之后**都核一遍（不变量要在每个中间状态成立）
    MINI_CHECK(WalkPlayerNextToEnemy(game, input, fb) > 0);

    constexpr int kShotsToKill = Game::kEnemyMaxHp / game::kDefaultBullet.damage;
    for (int i = 0; i < kShotsToKill; ++i) {
        FireOnce(game, input, fb, kDt);
        StepFrames(game, input, fb, kDt, 30);
        MINI_CHECK(game.Ammo() >= 0);
        MINI_CHECK(game.Ammo() <= Game::kMaxAmmo);
    }
    MINI_CHECK_EQ(game.Kills(), 1);

    // ③ 击杀返还**之后**那一瞬间（最接近上限的时刻）再核一次
    MINI_CHECK(game.Ammo() <= Game::kMaxAmmo);

    // ④ "每波回满"那个写入点也不可能超上限 —— 这是编译期成立的
    //    （`Game.h` 里 `static_assert(kMaxAmmo >= kStartAmmo)`）；
    //    这里用运行时再说一遍语义，免得以后有人把静态断言删了还不知道少了什么。
    MINI_CHECK(Game::kStartAmmo <= Game::kMaxAmmo);
}
