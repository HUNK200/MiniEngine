// 尖括号 + `mini/` 前缀：引擎头只能从 PUBLIC include 目录找，
// 不允许用相对路径穿透到 engine/src/（S0_CHECKLIST 第 5.5 节 / R7）
#include <mini/core/Application.h>
#include <mini/platform/Input.h>
#include <mini/platform/KeyCode.h>
#include <mini/render/FrameBuffer.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <string_view>

// ============================================================
// S3 演示：引擎骨架立起来了
//
// 和 S2 的差别只在"壳"：
//   S2 → main 自己写 while、自己排帧序、自己收尾终端
//   S3 → 这些全归 Application，main 只剩"造对象 + Run"
//
// 游戏侧（DemoGame）搬的就是 S2 那套：三行 HUD 看 15 个键的四态，
// W A S D 与方向键让 @ 每次**按下**走一格，Q 退出。
//
// 这一版真正的看点：
//   1. 第 0 行是引擎的顶栏（FPS / Frame / Work / Rows）
//   2. 画面尺寸跟着终端走 —— 拖窗口就该重排，**游戏代码一行都不用改**
//   3. Rows 显示"这一帧只发了几行"，静止时是个位数甚至 0
//
// 还没做的（别在这版里找）：速度、世界、碰撞、相机 —— 那是 S5 之后。
// ============================================================

namespace {

using mini::core::FrameContext;
using mini::core::IGame;
using mini::platform::Input;
using mini::platform::Key;
using mini::platform::KeyState;
using mini::render::Color;
using mini::render::FrameBuffer;

// ------------------------------------------------------------
// 版面
//
//   第 0 行：**顶栏**，归 Application（游戏不要用这一行）
//   1      ：顶框
//   2 ~ 4  ：HUD（三行，看键的四种状态）
//   5      ：分隔线
//   6 ~ h-3：活动区（@ 在这里走）
//   h-2    ：提示
//   h-1    ：底框
//
// 在 80x25 上，这和 S2 的固定版面**逐格一致**，只是整体下移一行给顶栏让位。
// ------------------------------------------------------------
constexpr int kFrameTop  = 1;
constexpr int kHudTop    = 2;
constexpr int kSeparator = 5;
constexpr int kPlayTop   = 6;

struct Layout {
    int frameTop    = kFrameTop;
    int frameBottom = 0;
    int hudTop      = kHudTop;
    int separator   = kSeparator;
    int playTop     = kPlayTop;
    int playBottom  = 0;
    int hintRow     = 0;
};

// 按**当前画布尺寸**算版面，每帧算一次 —— resize 因此天然被跟上，
// 不需要在游戏里存"上次的尺寸"再比较（那种写法迟早会漏掉一条分支）。
Layout MakeLayout(int w, int h)
{
    (void)w;   // 版面只跟高度有关；宽度交给 FrameBuffer 自己裁剪

    Layout L;
    L.frameBottom = h - 1;
    L.hintRow     = h - 2;
    L.playBottom  = h - 3;

    // 窗口矮到装不下版面时，活动区退化成"从 kPlayTop 开始"。
    // 这时 @ 可能和提示行重叠 —— **只是难看，不会崩**：
    // FrameBuffer::Set 越界是静默忽略的（S1 定下的行为）。
    if (L.playBottom < L.playTop) {
        L.playBottom = L.playTop;
    }
    return L;
}

// ------------------------------------------------------------
// 被监视的键。屏幕上的 HUD 就是这三张表生成的 ——
// 想多看一个键，往表里加一行即可，不用改绘制代码。
// ------------------------------------------------------------
struct WatchKey {
    const char* label;
    Key         key;
};

constexpr WatchKey kHudRow0[] = {
    {"W", Key::W}, {"A", Key::A}, {"S", Key::S}, {"D", Key::D}, {"SPACE", Key::Space},
};

constexpr WatchKey kHudRow1[] = {
    {"Q", Key::Q}, {"ESC", Key::Escape}, {"UP", Key::ArrowUp},
    {"DOWN", Key::ArrowDown}, {"LEFT", Key::ArrowLeft},
};

constexpr WatchKey kHudRow2[] = {
    {"RIGHT", Key::ArrowRight}, {"F1", Key::F1}, {"F2", Key::F2},
    {"TAB", Key::Tab}, {"ENTER", Key::Enter},
};

constexpr const char* kHintText =
    "W/A/S/D or arrows: move one cell per press    Q: quit    Ctrl+C: quit";

// ------------------------------------------------------------
// 四态 → 屏幕上的文字与颜色
//
// 文字用全称（Held / Pressed / Released）不用缩写 —— 这个阶段就是在
// "看清状态机到底在干什么"，缩写反而要来回查。
// 颜色是关键：Pressed / Released 都只存在**一帧**（60fps 下约 17ms），
// 靠颜色才能一眼看出"边沿确实被抓到了"，而不是一闪而过的黑字。
// ------------------------------------------------------------
const char* StateLabel(KeyState state)
{
    switch (state) {
    case KeyState::Up:       return "-";
    case KeyState::Pressed:  return "Pressed";
    case KeyState::Held:     return "Held";
    case KeyState::Released: return "Released";
    }
    return "?";
}

Color StateColor(KeyState state)
{
    switch (state) {
    case KeyState::Up:       return Color::BrightBlack;    // 暗灰：没按
    case KeyState::Pressed:  return Color::BrightYellow;   // 黄：本帧刚按下
    case KeyState::Held:     return Color::BrightGreen;    // 绿：持续按住
    case KeyState::Released: return Color::BrightRed;      // 红：本帧刚松开
    }
    return Color::Default;
}

// FrameBuffer 只有"带颜色的单格 Set"和"默认色的整串 Draw"，
// 所以"上色的整串"要自己拼。这个 helper 将来会变成 S14 的 Renderer2D，
// 现在先放在 main 里 —— FrameBuffer 的接口与 S2 完全一致，没为这版动过。
void DrawColored(FrameBuffer& fb, int x, int y, std::string_view text, Color fg)
{
    for (std::size_t i = 0; i < text.size(); ++i) {
        fb.Set(x + static_cast<int>(i), y, text[i], fg);
    }
}

// 状态文字定宽。"Released" 是 8 个字符，其余（"-" / "Held" / "Pressed"）更短。
// 不定宽的话，每有一个键换状态，它右边所有键的位置都会左右乱抖。
constexpr int kStateField = 8;

// 一条 HUD 里，一个键占的总宽度：名字 + ": " + 定宽状态 + 间隔
int EntryWidth(const WatchKey& wk)
{
    return static_cast<int>(std::strlen(wk.label)) + 2 + kStateField + 2;
}

void DrawHudRow(FrameBuffer& fb, const Input& input, int y, const WatchKey* keys, std::size_t count)
{
    int x = 2;
    for (std::size_t i = 0; i < count; ++i) {
        const WatchKey& wk       = keys[i];
        const int       labelLen = static_cast<int>(std::strlen(wk.label));

        DrawColored(fb, x, y, wk.label, Color::BrightCyan);
        DrawColored(fb, x + labelLen, y, ":", Color::BrightBlack);

        const KeyState state = input.Get(wk.key);
        DrawColored(fb, x + labelLen + 2, y, StateLabel(state), StateColor(state));

        x += EntryWidth(wk);
    }
}

template <std::size_t N>
void DrawHudRow(FrameBuffer& fb, const Input& input, int y, const WatchKey (&keys)[N])
{
    DrawHudRow(fb, input, y, keys, N);
}

// ------------------------------------------------------------
// 玩家：S3 仍然只有一个"按一下走一格"的 @
// （速度 / 世界 / 碰撞是 S5/S7 的事，这一阶段要看的是引擎骨架）
// ------------------------------------------------------------
struct Player {
    int x = 0;
    int y = 0;
};

// 用 **Pressed** 而不是 IsDown：Pressed 每次物理按键只来一帧，所以点一下走一格。
//
// 长按要"连续走"的话得用 dt 驱动（S5 之后），**不能**靠系统的按键自动重复：
//   ① 自动重复的间隔由 Windows 的键盘设置决定（默认约 30ms 后开始、每 30ms 一次），
//      拿它当速度，换个系统设置游戏手感就变了；
//   ② 自动重复进来的事件会让 m_pulse 变 true，但 Get() 里 `was && now`（Held）
//      判定在前，所以状态**不会**被打回 Pressed —— @ 按住也只走一格，
//      这个"按住不会跑飞"的效果正是判定顺序带来的（见 InputState.h 6.3 的注释）。
void MovePlayer(Player& p, const Input& input, const Layout& L, int w)
{
    if (input.Get(Key::W) == KeyState::Pressed) --p.y;
    if (input.Get(Key::S) == KeyState::Pressed) ++p.y;
    if (input.Get(Key::A) == KeyState::Pressed) --p.x;
    if (input.Get(Key::D) == KeyState::Pressed) ++p.x;

    if (input.Get(Key::ArrowUp) == KeyState::Pressed)    --p.y;
    if (input.Get(Key::ArrowDown) == KeyState::Pressed)  ++p.y;
    if (input.Get(Key::ArrowLeft) == KeyState::Pressed)  --p.x;
    if (input.Get(Key::ArrowRight) == KeyState::Pressed) ++p.x;

    // 夹在活动区里。用 clamp 而不是"越界就当没按"：撞到边之后按反方向要能立刻走，
    // 否则得先按回来一次（S7 的真碰撞解算不会这么干，这里只求手感不别扭）。
    //
    // 宽度也要夹：拖窗口时 w 可能很小，只夹一边的话 x 会跑到框外去。
    const int left  = 1;
    const int right = w - 2;
    if (right < left) {
        p.x = left;                      // 窄到没有活动区：钉在左边界
    } else {
        if (p.x < left)  p.x = left;
        if (p.x > right) p.x = right;
    }

    if (p.y < L.playTop)    p.y = L.playTop;
    if (p.y > L.playBottom) p.y = L.playBottom;
}

// 一帧的全部绘制。只写后缓冲，**不碰终端**（那是 Application / Present 的事）。
void DrawFrame(FrameBuffer& fb, const Input& input, const Player& player, const Layout& L)
{
    const int w = fb.Width();

    // w >= 2 由 Application 保证（小于 2 的尺寸它会拒绝并跳过那次 resize），
    // 这里仍然算一次长度，免得框的长度被算成负数。
    const int barLen = (w - 2 > 0) ? (w - 2) : 0;

    const std::string horizontal(static_cast<std::size_t>(barLen), '-');
    const std::string edge = "+" + horizontal + "+";
    const std::string sep  = "|" + horizontal + "|";

    fb.Draw(0, L.frameTop, edge);
    fb.Draw(0, L.separator, sep);
    fb.Draw(0, L.frameBottom, edge);

    for (int y = L.frameTop + 1; y < L.frameBottom; ++y) {
        if (y == L.separator) {
            continue;   // 分隔线那行左右两端已经是 '|' 了
        }
        fb.Set(0, y, '|', Color::BrightBlack);
        fb.Set(w - 1, y, '|', Color::BrightBlack);
    }

    DrawHudRow(fb, input, L.hudTop + 0, kHudRow0);
    DrawHudRow(fb, input, L.hudTop + 1, kHudRow1);
    DrawHudRow(fb, input, L.hudTop + 2, kHudRow2);

    DrawColored(fb, 2, L.hintRow, kHintText, Color::BrightBlack);

    fb.Set(player.x, player.y, '@', Color::BrightYellow);
}

// ------------------------------------------------------------
// 演示游戏。它只做三件事：读 ctx 更新状态、往 fb 上画、被问时回答要不要退。
// 它**不知道** Application 存在，也不知道终端长什么样。
//
// 刻意不做的事（不然就是在 S3 里提前干 S5 的活）：
//   OnUpdate 里不碰 fb，OnRender 里不改状态。分开是为了让
//   "逻辑错了"和"画错了"能各自定位。
//
// ⚠️ 它必须自己缓存 `const Input*`：引擎只在 OnUpdate 给一帧快照，
//    而 HUD 要在 OnRender 里读按键状态。这是本版**有意的取舍**（见报告），
//    代价是 OnRender 之前必须有过一次 OnUpdate —— Run() 的帧序保证了这一点。
// ------------------------------------------------------------
class DemoGame final : public IGame {
public:
    void OnStart() override
    {
        // 初始位置要等第一帧知道尺寸才能算，所以实际摆在 OnUpdate 里（m_placed 门闩）
    }

    void OnUpdate(const FrameContext& ctx) override
    {
        m_input = &ctx.input;   // 给 OnRender 用（HUD 要读按键）

        const Layout L = MakeLayout(ctx.screenWidth, ctx.screenHeight);

        if (!m_placed) {
            m_player.x = ctx.screenWidth / 2;
            m_player.y = (L.playTop + L.playBottom) / 2;
            m_placed   = true;
        }

        if (ctx.input.Get(Key::Q) == KeyState::Pressed) {
            m_quit = true;
        }

        MovePlayer(m_player, ctx.input, L, ctx.screenWidth);
        // 注意：**不为 resize 重新摆 @ 的位置**。夹取会把它拉回可见区，
        // 而"窗口变小后实体仍留在屏幕外"那类问题属于相机与边界（S6）。
    }

    void OnRender(FrameBuffer& fb) override
    {
        // Run() 保证每帧 OnUpdate 先于 OnRender，所以这里一定有值。
        // 仍然判一次：将来有人动了 Run() 的顺序时，宁可什么都不画，也不要解空指针。
        if (m_input == nullptr) {
            return;
        }
        DrawFrame(fb, *m_input, m_player, MakeLayout(fb.Width(), fb.Height()));
    }

    [[nodiscard]] bool ShouldQuit() const override { return m_quit; }

private:
    const Input* m_input  = nullptr;
    Player       m_player;
    bool         m_placed = false;
    bool         m_quit   = false;
};

} // namespace

int main(int argc, char** argv)
{
    mini::core::AppConfig cfg;

    // 命令行第一个参数 = 目标帧率。用来验证"限帧真的在限帧，而不是工作变快了"：
    //   MiniEngine.exe 300  → FPS 明显上升，而 Work 基本不变
    if (argc > 1) {
        cfg.targetFps = std::atoi(argv[1]);
    }

    // 提示行在切码页之前打：此时码页还没切成 UTF-8，所以刻意只用 ASCII
    std::printf("[S3] W/A/S/D or arrows move @, Q quits\n");

    try {
        // 顺序有讲究：demo 先建、app 后建，析构就是反过来的 ——
        // 终端由 app 先恢复、再轮到 demo，最后那行提示才不会被备用屏吞掉。
        DemoGame                demo;
        mini::core::Application app(cfg);

        app.Run(demo);
    } catch (const std::exception& e) {
        // 只会因为一件事走到这里：stdout / stdin 不是控制台（比如你 `> out.txt`）。
        // 这时抛异常 + 人话提示，比"静默什么都不显示"好得多（见 Console.h）。
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }

    return 0;
}
