#include "mini/platform/Input.h"

#include "mini/platform/Console.h"   // platform::QuitRequested()
#include "mini/platform/InputState.h"

#include <windows.h>

#include <cstddef>
#include <stdexcept>

// ============================================================
// platform::Input 的 Windows 实现（S2）
//
// 本文件**只做一件有含量的事**：把 Win32 的 INPUT_RECORD 翻译成
// InputState::OnKeyDown / OnKeyUp / OnFocusLost。三态/四态逻辑一行都不在这里，
// 全部在 InputState.h —— 那边的代码不碰 windows.h，所以可以被单元测试完整覆盖。
//
// 它不设置输入模式：关 QUICK_EDIT、开 WINDOW_INPUT 都是 Console 构造时做的
// （终端模式是一件事，拆成两处保存/恢复必然漏）。所以调用顺序是
// **MakeConsole() 先，MakeInput() 后**。
//
// 用 ReadConsoleInputW 而不是 ...A：UTF-8 码页下 A 版本的 AsciiChar 只给单字节。
// 我们只取 wVirtualKeyCode（与 A/W 无关），但用 W 更干净、不用想这件事。
// ============================================================

namespace mini::platform {
namespace {

// 只用来读事件。输入模式的设置/恢复全在 ConsoleWindows.cpp。
HANDLE g_hIn = INVALID_HANDLE_VALUE;

class InputWindows final : public Input {
public:
    InputWindows();

    void     Poll() override;
    void     BeginFrame() override;
    KeyState Get(Key k) const override;
    bool     IsDown(Key k) const override;
    bool     QuitRequested() const override;

private:
    InputState m_state;
};

InputWindows::InputWindows()
{
    g_hIn = ::GetStdHandle(STD_INPUT_HANDLE);

    // 与 Console 一样，宁可抛异常也不静默降级：
    // 一个"按键没反应"的程序比一个"起不来"的程序难查得多。
    DWORD mode = 0;
    if (g_hIn == INVALID_HANDLE_VALUE || ::GetConsoleMode(g_hIn, &mode) == 0) {
        throw std::runtime_error(
            "MiniEngine: 标准输入不是控制台（stdin 被重定向了？），拿不到键盘事件。\n"
            "            请在 Windows Terminal 里直接运行 .\\build\\bin\\Debug\\MiniEngine.exe");
    }
    // 这里**不校验 makeConsole 是否先调过**：模式没设好也能读事件，
    // 只是 QUICK_EDIT / FOCUS_EVENT 的行为会退化。顺序要求写在 Input.h 的注释里。
}

void InputWindows::Poll()
{
    if (g_hIn == INVALID_HANDLE_VALUE) {
        return;
    }

    // 一直读到队列为空。**不要**一帧只读固定条数：积压会让输入越来越滞后，
    // 表象是"按了键过一会儿才动"。
    DWORD available = 0;
    while (::GetNumberOfConsoleInputEvents(g_hIn, &available) != 0 && available > 0) {
        INPUT_RECORD record{};
        DWORD        read = 0;
        if (::ReadConsoleInputW(g_hIn, &record, 1, &read) == 0 || read == 0) {
            break;   // 句柄出了问题就别空转，下一帧再说
        }

        switch (record.EventType) {
        case KEY_EVENT: {
            const KEY_EVENT_RECORD& key = record.Event.KeyEvent;
            // 长按的自动重复 = 连续多个 bKeyDown = TRUE 且没有对应的 KEY_UP，
            // 所以这里不需要为"重复"做任何特判（见 InputState::OnKeyDown 的注释）
            if (key.bKeyDown != 0) {
                m_state.OnKeyDown(KeyFromVirtualKey(key.wVirtualKeyCode));
            } else {
                m_state.OnKeyUp(KeyFromVirtualKey(key.wVirtualKeyCode));
            }
            break;
        }

        case FOCUS_EVENT:
            // 切走窗口时**收不到 KEY_UP**，按住的键会永远卡在 Held
            // （切回来 @ 会自己一直走）。靠这个事件在失焦时把全部键放掉。
            // 能收到它，是因为 Console 开了 ENABLE_WINDOW_INPUT。
            if (record.Event.FocusEvent.bSetFocus == 0) {
                m_state.OnFocusLost();
            }
            break;

        case WINDOW_BUFFER_SIZE_EVENT:
            // S2 **只安全丢弃**：不崩，也不会被误当成按键。
            // 画面自适应（FrameBuffer::Resize + 重画）归 S3/S6，而且那时
            // 每帧比较一次 Console::Size() 比事件驱动更简单、不会漏 —— 见 PLAN 风险 E-12。
            break;

        default:
            // MOUSE_EVENT / MENU_EVENT 一律丢弃（本项目不用鼠标）
            break;
        }
    }
}

void InputWindows::BeginFrame()
{
    m_state.BeginFrame();
}

KeyState InputWindows::Get(Key k) const
{
    return m_state.Get(k);
}

bool InputWindows::IsDown(Key k) const
{
    return m_state.IsDown(k);
}

bool InputWindows::QuitRequested() const
{
    // 这里只是转发：Ctrl+C handler 是**进程级**的，没有"哪个 Input 收到了 Ctrl+C"
    // 这回事，所以真正的标志位住在 platform::Console 那边（见 Console.h）。
    // 保留这个转发是为了让游戏循环只面对一个统一接口：input->QuitRequested()。
    return platform::QuitRequested();
}

} // namespace

std::unique_ptr<Input> MakeInput()
{
    return std::make_unique<InputWindows>();
}

} // namespace mini::platform
