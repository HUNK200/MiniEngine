#include "mini/render/TerminalPresenter.h"

#include "mini/render/FrameBuffer.h"

#include <windows.h>

#include <cstddef>
#include <cstdio>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <string_view>

// ============================================================
// TerminalPresenter 实现（S1）
//
// 全项目唯一 include <windows.h> 的地方。S2 会把这一层挪到 platform/（见头文件）。
//
// 三条容易踩的规矩：
//   1. Ctrl+C handler 跑在**异步信号上下文**里，只能用 WriteConsoleA 这类
//      异步安全 API，绝不能碰 std::cout / printf（会死锁）
//   2. 码页要恢复成**原来的值**，不是硬写 936（用户可能本来就在 65001）
//   3. 帧首 ESC[H 不清屏，也**不重置 SGR** —— 重置由 FrameBuffer::Serialize 负责
// ============================================================

// 老 SDK 里可能没有这个名字，兜一下（值来自 wincon.h）
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

namespace mini::render {
namespace {

HANDLE g_hOut = INVALID_HANDLE_VALUE;
UINT   g_savedOutputCp = 0;
UINT   g_savedInputCp = 0;
DWORD  g_savedOutputMode = 0;
int    g_savedStdoutMode = -1;    // _setmode 的返回值，-1 表示没改过
bool   g_modeSaved = false;
bool   g_active = false;

// handler 与 main 之间只有这一个共享量，用 Interlocked 保证可见性与原子性
volatile LONG g_quitRequested = 0;

constexpr char kRestoreSequence[] = "\x1b[?25h\x1b[?1049l";   // 显光标 + 离开备用屏

// 只走控制台 API，不做缓冲分配 —— 可以被 Ctrl+C handler 安全调用
template <std::size_t N>
void WriteRaw(const char (&text)[N])
{
    if (g_hOut == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    (void)::WriteConsoleA(g_hOut, text, static_cast<DWORD>(N - 1), &written, nullptr);
}

void RestoreConsoleAttributes()
{
    if (g_hOut != INVALID_HANDLE_VALUE && g_modeSaved) {
        (void)::SetConsoleMode(g_hOut, g_savedOutputMode);
    }
    if (g_savedOutputCp != 0) {
        (void)::SetConsoleOutputCP(g_savedOutputCp);
    }
    if (g_savedInputCp != 0) {
        (void)::SetConsoleCP(g_savedInputCp);
    }
}

BOOL WINAPI CtrlHandler(DWORD type)
{
    switch (type) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        // 第一次：恢复终端后放行给 main 正常退出（EndTerminal 会再兜一次，重复无害）
        if (::InterlockedCompareExchange(&g_quitRequested, 1, 0) == 0) {
            WriteRaw(kRestoreSequence);
            return TRUE;    // 已处理，别让系统直接杀掉进程
        }
        return FALSE;       // 第二次：放行，防止卡在等待里杀不掉
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        WriteRaw(kRestoreSequence);   // 这几类没法拦，至少把终端还回去
        return FALSE;
    default:
        return FALSE;
    }
}

} // namespace

void BeginTerminal()
{
    if (g_active) {
        return;   // 幂等：重复调用不会把"当前状态"当成"原始状态"存错
    }

    g_hOut = ::GetStdHandle(STD_OUTPUT_HANDLE);

    g_savedOutputCp = ::GetConsoleOutputCP();
    g_savedInputCp = ::GetConsoleCP();
    (void)::SetConsoleOutputCP(CP_UTF8);
    (void)::SetConsoleCP(CP_UTF8);

    if (g_hOut != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (::GetConsoleMode(g_hOut, &mode) != 0) {
            g_savedOutputMode = mode;
            g_modeSaved = true;
            (void)::SetConsoleMode(g_hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }

    (void)::SetConsoleCtrlHandler(CtrlHandler, TRUE);

    std::cout.flush();   // 先把自己缓冲里的话说完，再切屏，免得次序错乱

    // stdout 切**二进制**：CRT 在文本模式下会把 '\n' 翻成 CRLF，
    // 于是我们显式写的 "\r\n" 变成 "\r\r\n" —— 屏幕上无害（CR 两次等于一次），
    // 但帧数据不再是字节精确，而且每行白扔一个字节。
    // 我们已经自己管了 "\r\n"，所以这里要的就是"原样写出"。
    g_savedStdoutMode = ::_setmode(::_fileno(stdout), _O_BINARY);

    // 备用屏：退出后原来的 shell 历史还在（ESC[?1049h）；隐光标（ESC[?25l）
    WriteRaw("\x1b[?1049h\x1b[?25l");

    g_active = true;
}

void EndTerminal()
{
    if (!g_active) {
        return;
    }
    g_active = false;

    WriteRaw(kRestoreSequence);
    RestoreConsoleAttributes();

    if (g_savedStdoutMode != -1) {
        (void)::_setmode(::_fileno(stdout), g_savedStdoutMode);
        g_savedStdoutMode = -1;
    }
}

void Present(const FrameBuffer& fb)
{
    // 光标归位（不清屏！ESC[2J 是闪烁的根源）+ 整屏 + 刷一次
    std::cout << "\x1b[H";
    const std::string_view pixels = fb.Serialize();
    if (!pixels.empty()) {
        std::cout.write(pixels.data(), static_cast<std::streamsize>(pixels.size()));
    }
    std::cout.flush();
}

bool QuitRequested()
{
    return ::InterlockedCompareExchange(&g_quitRequested, 0, 0) != 0;
}

} // namespace mini::render
