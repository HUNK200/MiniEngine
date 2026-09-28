// 尖括号 + `mini/` 前缀：引擎头只能从 PUBLIC include 目录找，
// 不允许用相对路径穿透到 engine/src/（S0_CHECKLIST 第 5.5 节 / R7）
#include <mini/platform/Console.h>
#include <mini/platform/Input.h>
#include <mini/render/FrameBuffer.h>

#include <chrono>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <string_view>
#include <thread>

// ============================================================
// S2 演示：把"输入通路通了"这件事变成屏幕上看得见的东西。
//
//   1. 三行 HUD 实时显示 15 个键的四态（Up / Pressed / Held / Released）
//   2. W A S D 与方向键让 @ 每次"按下"走一格（**瞬时移动**，没有速度、
//      没有世界、没有碰撞 —— 那三样分别是 S3 / S5 / S7 的事）
//   3. Q 退出，Ctrl+C 也能退出
//
// 帧序（与 PLAN C.4 的定稿一致，**顺序不能动**）：
//     Poll()  →  读状态/更新逻辑  →  画  →  Present()  →  BeginFrame()
//   BeginFrame 必须在"读状态"之后：它把本帧的按下折成"上一帧"，
//   调早了 Pressed 永远看不到，调晚了 Released 会被吃掉。
//
// 仍然没有游戏循环（PLAN C.4 的 Application 是 S3 的事），
// 这里的 while 只是"够用就好"的临时壳子。
// ============================================================

namespace {

using mini::platform::Input;
using mini::platform::Key;
using mini::platform::KeyState;
using mini::render::Color;
using mini::render::FrameBuffer;

constexpr int kWidth  = 80;
constexpr int kHeight = 25;

// 版面：0 顶框 / 1~3 HUD / 4 分隔线 / 5~22 活动区 / 23 提示 / 24 底框
constexpr int kHudTop     = 1;
constexpr int kSeparator  = 4;
constexpr int kPlayTop    = 5;
constexpr int kPlayBottom = 22;

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

// ------------------------------------------------------------
// 四态 → 屏幕上的文字与颜色
//
// 文字用和 PLAN S2 一样的全称（Held / Pressed / Released），不用缩写 ——
// 这个阶段就是在"看清状态机到底在干什么"，缩写反而要来回查。
// 颜色是关键：Pressed / Released 都只存在**一帧**（约 20ms），
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
// 现在先放在 main 里 —— S2 不动 FrameBuffer 的接口。
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
        const WatchKey& wk = keys[i];

        DrawColored(fb, x, y, wk.label, Color::BrightCyan);
        DrawColored(fb, x + static_cast<int>(std::strlen(wk.label)), y, ":", Color::BrightBlack);

        const int stateX = x + static_cast<int>(std::strlen(wk.label)) + 2;
        const KeyState state = input.Get(wk.key);
        DrawColored(fb, stateX, y, StateLabel(state), StateColor(state));

        x += EntryWidth(wk);
    }
}

template <std::size_t N>
void DrawHudRow(FrameBuffer& fb, const Input& input, int y, const WatchKey (&keys)[N])
{
    DrawHudRow(fb, input, y, keys, N);
}

// ------------------------------------------------------------
// 玩家：S2 只有一个"按一下走一格"的 @
// ------------------------------------------------------------
struct Player {
    int x = kWidth / 2;
    int y = (kPlayTop + kPlayBottom) / 2;
};

void MovePlayer(Player& p, const Input& input)
{
    // 用 **Pressed** 而不是 IsDown：Pressed 每次物理按键只来一帧，所以点一下走一格。
    //
    // 长按要"连续走"的话得用 dt 驱动（S3 的 Time），**不能**靠系统的按键自动重复：
    //   ① 自动重复的间隔由 Windows 的键盘设置决定（默认约 30ms 后开始、每 30ms 一次），
    //      拿它当速度，换个系统设置游戏手感就变了；
    //   ② 自动重复进来的事件会让 m_pulse 变 true，但 Get() 里 `was && now`（Held）
    //      判定在前，所以状态**不会**被打回 Pressed —— @ 按住也只走一格，
    //      这个"按住不会跑飞"的效果正是判定顺序带来的（见 InputState.h 6.3 的注释）。
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
    if (p.x < 1)             p.x = 1;
    if (p.x > kWidth - 2)    p.x = kWidth - 2;
    if (p.y < kPlayTop)      p.y = kPlayTop;
    if (p.y > kPlayBottom)   p.y = kPlayBottom;
}

// 一帧的全部绘制。只写后缓冲，**不碰终端**（那是 Present 的事）。
void DrawFrame(FrameBuffer& fb, const Input& input, const Player& player)
{
    fb.Clear();

    const std::string horizontal(static_cast<std::size_t>(kWidth - 2), '-');
    const std::string edge = "+" + horizontal + "+";
    const std::string sep  = "|" + horizontal + "|";

    fb.Draw(0, 0, edge);
    fb.Draw(0, kSeparator, sep);
    fb.Draw(0, kHeight - 1, edge);
    for (int y = 1; y < kHeight - 1; ++y) {
        if (y == kSeparator) {
            continue;   // 分隔线那行左右两端已经是 '|' 了
        }
        fb.Set(0, y, '|', Color::BrightBlack);
        fb.Set(kWidth - 1, y, '|', Color::BrightBlack);
    }

    DrawHudRow(fb, input, kHudTop + 0, kHudRow0);
    DrawHudRow(fb, input, kHudTop + 1, kHudRow1);
    DrawHudRow(fb, input, kHudTop + 2, kHudRow2);

    DrawColored(fb, 2, kHeight - 2,
                "W/A/S/D or arrows: move one cell per press    Q: quit    Ctrl+C: quit",
                Color::BrightBlack);

    fb.Set(player.x, player.y, '@', Color::BrightYellow);
}

} // namespace

int main()
{
    using namespace std::chrono_literals;

    FrameBuffer fb(kWidth, kHeight);

    // 提示行在切码页之前打：此时码页还没切成 UTF-8，所以刻意只用 ASCII
    std::printf("[S2] W/A/S/D or arrows move @, Q quits\n");

    try {
        // 顺序不能反：输入模式（关 QUICK_EDIT、开 WINDOW_INPUT）是 Console 构造时设的，
        // Input 只负责读事件。反过来会在 Input 构造时抛异常。
        auto console = mini::platform::MakeConsole();
        auto input   = mini::platform::MakeInput();

        Player player;
        bool   quit = false;

        while (!quit && !input->QuitRequested()) {
            input->Poll();                                    // 1. 事件 → 状态

            if (input->Get(Key::Q) == KeyState::Pressed) {    // 2. 逻辑
                quit = true;
            }
            MovePlayer(player, *input);

            DrawFrame(fb, *input, player);                    // 3. 全部画进后缓冲

            // 4. 提交（脏行 diff 之后是"一帧最多写一次"：内容没变时一个字节都不发）
            //    返回 false = 终端已经不在了 → 立刻退出。
            //    这是 S2 那个坑的收口处：关掉控制台窗口后进程不再变成锁住 exe 的孤儿。
            if (!console->Present(fb)) {
                break;
            }

            input->BeginFrame();                              // 5. 帧尾推进
            std::this_thread::sleep_for(16ms);
        }

        // 离开作用域 = 原 EndTerminal()：显光标 → 离开备用屏 → 还原模式与码页
    } catch (const std::exception& e) {
        // 只会因为一件事走到这里：stdout / stdin 不是控制台（比如你 `> out.txt`）。
        // 这时抛异常 + 人话提示，比"静默什么都不显示"好得多（见 Console.h）。
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }

    return 0;
}
