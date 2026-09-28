#include "mini/platform/Console.h"

#include "mini/render/FrameBuffer.h"
#include "mini/render/RowDiff.h"

#include <windows.h>

#include <cstddef>
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <stdexcept>
#include <string_view>

// ============================================================
// platform::Console 的 Windows 实现（S2）
//
// 这个文件是全项目**唯一** include <windows.h> 的地方（PLAN A.3 硬规矩）。
// S1 时它叫 render/TerminalPresenter.cpp，S2 归位到 platform/。
//
// 四条容易踩的规矩：
//   1. Ctrl+C handler 跑在**异步信号上下文**里，只能用 WriteConsoleA 这类
//      异步安全 API，绝不能碰 std::cout / printf（会死锁）
//   2. 码页要恢复成**原来的值**，不是硬写 936（用户可能本来就在 65001）
//   3. 帧首用 ESC[H 归位，**不清屏**，也不重置 SGR —— 重置由
//      FrameBuffer::Serialize 负责
//   4. QUICK_EDIT_MODE 必须与 EXTENDED_FLAGS **同时**置位才生效：
//      只清 QUICK_EDIT 而不开 EXTENDED_FLAGS，Windows 会忽略整次修改
// ============================================================

// 老 SDK 里可能没有这个名字，兜一下（值来自 wincon.h）
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

namespace mini::platform {
namespace {

HANDLE g_hOut = INVALID_HANDLE_VALUE;
HANDLE g_hIn  = INVALID_HANDLE_VALUE;

UINT g_savedOutputCp = 0;
UINT g_savedInputCp  = 0;

DWORD g_savedOutputMode = 0;
DWORD g_savedInputMode  = 0;
bool  g_outputModeSaved = false;
bool  g_inputModeSaved  = false;

int  g_savedStdoutMode = -1;   // _setmode 的返回值，-1 表示没改过
bool g_active = false;

// handler 与主线程之间只有这一个共享量，用 Interlocked 保证可见性与原子性
volatile LONG g_quitRequested = 0;

constexpr char kRestoreSequence[] = "\x1b[?25h\x1b[?1049l";   // 显光标 + 离开备用屏

// 只走控制台 API、不做缓冲分配 —— 可以被 Ctrl+C handler 安全调用
template <std::size_t N>
void WriteRaw(const char (&text)[N])
{
    if (g_hOut == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    (void)::WriteConsoleA(g_hOut, text, static_cast<DWORD>(N - 1), &written, nullptr);
}

// 把"我们改过的东西"还原回原值。幂等：可以安全地被调用多次
void RestoreConsoleState()
{
    if (g_hOut != INVALID_HANDLE_VALUE && g_outputModeSaved) {
        (void)::SetConsoleMode(g_hOut, g_savedOutputMode);
        g_outputModeSaved = false;
    }
    if (g_hIn != INVALID_HANDLE_VALUE && g_inputModeSaved) {
        (void)::SetConsoleMode(g_hIn, g_savedInputMode);
        g_inputModeSaved = false;
    }
    if (g_savedOutputCp != 0) {
        (void)::SetConsoleOutputCP(g_savedOutputCp);
        g_savedOutputCp = 0;
    }
    if (g_savedInputCp != 0) {
        (void)::SetConsoleCP(g_savedInputCp);
        g_savedInputCp = 0;
    }
}

BOOL WINAPI CtrlHandler(DWORD type)
{
    switch (type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        // 第一次：恢复终端后放行给主线程正常退出
        // （析构会再兜一次，重复无害）
        if (::InterlockedCompareExchange(&g_quitRequested, 1, 0) == 0) {
            WriteRaw(kRestoreSequence);
            return TRUE;    // 已处理，别让系统直接杀掉进程
        }
        return FALSE;       // 第二次：放行，防止卡在等待里杀不掉
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        WriteRaw(kRestoreSequence);   // 这几类拦不住，至少把终端还回去
        return FALSE;
    default:
        return FALSE;
    }
}

class ConsoleWindows final : public Console {
public:
    ConsoleWindows();
    ~ConsoleWindows() override;

    bool        Present(const render::FrameBuffer& fb) override;
    ConsoleSize Size() const override;
    int         LastFrameRowsSent() const override;

private:
    // 脏行 diff：只把内容变了的行交给终端。它是"防闪烁/少写"那条线的落点，
    // 所以放在**唯一的终端写入方**（Console）里，而不是让调用方自己拼字节。
    render::RowDiff m_rowDiff;

    // 上一次 Present 的画布尺寸。变了 → RowDiff 快照可能失效 + 需要清一次屏。
    int  m_lastWidth  = 0;
    int  m_lastHeight = 0;
    // 首帧不发作 ESC[2J：刚进备用屏，本来就是空的，清它纯属浪费（也就不是"闪烁"）。
    bool m_hasPresented = false;
};

ConsoleWindows::ConsoleWindows()
{
    if (g_active) {
        return;   // 幂等：重复创建不会把"当前状态"当成"原始状态"存错
    }

    g_hOut = ::GetStdHandle(STD_OUTPUT_HANDLE);
    g_hIn  = ::GetStdHandle(STD_INPUT_HANDLE);

    // ---- 校验阶段：先把"这根本不是终端"挡掉。此时**还没改过任何东西**，
    //      所以直接抛异常不会把终端留在半改状态 ----
    DWORD outMode = 0;
    DWORD inMode  = 0;
    if (g_hOut == INVALID_HANDLE_VALUE || ::GetConsoleMode(g_hOut, &outMode) == 0) {
        throw std::runtime_error(
            "MiniEngine: 标准输出不是控制台（stdout 被重定向到文件或管道了？）。\n"
            "            请在 Windows Terminal 里直接运行 .\\build\\bin\\Debug\\MiniEngine.exe");
    }
    if (g_hIn == INVALID_HANDLE_VALUE || ::GetConsoleMode(g_hIn, &inMode) == 0) {
        throw std::runtime_error(
            "MiniEngine: 标准输入不是控制台（stdin 被重定向了？）。\n"
            "            请在 Windows Terminal 里直接运行 .\\build\\bin\\Debug\\MiniEngine.exe");
    }

    // ---- 变更阶段：从这里开始出错必须回滚 ----
    try {
        g_savedOutputCp = ::GetConsoleOutputCP();
        g_savedInputCp  = ::GetConsoleCP();
        (void)::SetConsoleOutputCP(CP_UTF8);
        (void)::SetConsoleCP(CP_UTF8);

        // 输出侧：打开 VT 处理，否则画面里出现的是字面的 "←[H" 而不是光标归位
        g_savedOutputMode = outMode;
        g_outputModeSaved = true;
        (void)::SetConsoleMode(g_hOut, outMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

        // 输入侧：
        //   ~ENABLE_QUICK_EDIT_MODE  否则在终端里点一下鼠标就进"选择模式"，输入卡死
        //   ENABLE_EXTENDED_FLAGS    与上面那条**必须成对**，见本文件头注释 4
        //   ENABLE_WINDOW_INPUT      才收得到 FOCUS_EVENT —— 切走窗口时靠它放掉按住的键
        g_savedInputMode = inMode;
        g_inputModeSaved = true;
        const DWORD wantedInputMode =
            (inMode | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT) & ~ENABLE_QUICK_EDIT_MODE;
        (void)::SetConsoleMode(g_hIn, wantedInputMode);

        // 先把自己缓冲里的话说完，再切备用屏，免得次序错乱
        std::cout.flush();

        // stdout 切**二进制**：CRT 在文本模式下会把 '\n' 翻成 CRLF，
        // 于是我们显式写的 "\r\n" 变成 "\r\r\n" —— 屏幕上无害（CR 两次等于一次），
        // 但帧数据不再是字节精确，而且每行白扔一个字节。
        g_savedStdoutMode = ::_setmode(::_fileno(stdout), _O_BINARY);

        (void)::SetConsoleCtrlHandler(CtrlHandler, TRUE);

        // 备用屏：退出后原来的 shell 历史还在（ESC[?1049h）；隐光标（ESC[?25l）
        WriteRaw("\x1b[?1049h\x1b[?25l");

        g_active = true;
    } catch (...) {
        (void)::SetConsoleCtrlHandler(CtrlHandler, FALSE);
        RestoreConsoleState();
        throw;
    }
}

ConsoleWindows::~ConsoleWindows()
{
    if (!g_active) {
        return;
    }
    g_active = false;

    WriteRaw(kRestoreSequence);
    RestoreConsoleState();

    if (g_savedStdoutMode != -1) {
        (void)::_setmode(::_fileno(stdout), g_savedStdoutMode);
        g_savedStdoutMode = -1;
    }

    // 卸掉 handler：终端已经还回去了，再让 handler 往屏幕上写转义序列没有意义
    (void)::SetConsoleCtrlHandler(CtrlHandler, FALSE);
}

bool ConsoleWindows::Present(const render::FrameBuffer& fb)
{
    // ---- ① 终端还在不在？----
    //
    // 为什么单独查一次、而不是只靠"写完看 std::cout 的 failbit"：
    //   脏行 diff 生效之后，**内容没变的帧一个字节都不发** —— 而"没写"就没有失败信号。
    //   比如画面完全静止时（S2 的演示、或以后暂停的游戏），只靠写失败检测根本发现不了
    //   终端已经死了。所以这一层是主力，写失败检测是兜底。
    //   代价是一次 GetConsoleMode，约 1μs，和 16ms 的帧时间比可以忽略。
    DWORD outMode = 0;
    if (g_hOut == INVALID_HANDLE_VALUE || ::GetConsoleMode(g_hOut, &outMode) == 0) {
        return false;   // 控制台已被销毁（关窗口留下的孤儿进程就死在这条上）
    }

    // ---- ② 尺寸变了？----
    const int w = fb.Width();
    const int h = fb.Height();
    if (w != m_lastWidth || h != m_lastHeight) {
        // **唯一允许清屏的地方**：尺寸变化。清屏 + 重画的那一瞬就是闪烁的来源，
        // 所以每帧 ESC[2J 是禁忌，只有这里例外（PLAN E-3 的原始教训）。
        // 首帧除外：刚进备用屏，本来就是空的。
        if (m_hasPresented) {
            std::cout << "\x1b[2J";
            m_rowDiff.Invalidate();   // 清完屏必须全量重画，否则屏幕上会缺一块
        }
        m_lastWidth  = w;
        m_lastHeight = h;
    }

    // ---- ③ 拼帧（只含变化的行）并一次写出去 ----
    const std::string_view frame = m_rowDiff.Compose(fb);
    if (!frame.empty()) {
        std::cout.write(frame.data(), static_cast<std::streamsize>(frame.size()));
        std::cout.flush();

        // 兜底：真写失败了（句柄被别的路径弄坏、重定向目标满了……）也算终端不可用。
        // 注意 std::cout 的 failbit 一旦置上就**不会自己清**，所以这里直接返回 false，
        // 让上层退出 —— 不要把同一个 cout 留着复用（见 S3 检查单坑 #25）。
        if (!std::cout) {
            return false;
        }
    }

    m_hasPresented = true;
    return true;
}

ConsoleSize ConsoleWindows::Size() const
{
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (g_hOut == INVALID_HANDLE_VALUE || ::GetConsoleScreenBufferInfo(g_hOut, &info) == 0) {
        return {};
    }
    // srWindow 是**可视区**（相对缓冲区原点），不是缓冲区大小：
    // 缓冲区通常比窗口高得多（滚动条那部分不算"屏幕"）
    return ConsoleSize{ static_cast<int>(info.srWindow.Right  - info.srWindow.Left + 1),
                        static_cast<int>(info.srWindow.Bottom - info.srWindow.Top  + 1) };
}

int ConsoleWindows::LastFrameRowsSent() const
{
    // 上一帧的统计（本帧的要等 Present 之后才有）。顶栏差一帧无妨，
    // 换来的是"画顶栏"和"提交帧"之间没有循环依赖。
    return m_rowDiff.LastStats().rowsSent;
}

} // namespace

std::unique_ptr<Console> MakeConsole()
{
    return std::make_unique<ConsoleWindows>();
}

bool QuitRequested()
{
    return ::InterlockedCompareExchange(&g_quitRequested, 0, 0) != 0;
}

} // namespace mini::platform
