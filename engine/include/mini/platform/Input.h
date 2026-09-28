#pragma once

#include "mini/platform/KeyCode.h"

#include <memory>

// ============================================================
// platform::Input —— 键盘输入（S2）
//
// 抽象基类 + 工厂。公共头里**不出现 <windows.h> / #ifdef**（PLAN A.3 硬规矩），
// Windows 实现（InputWindows.cpp）只拿得到 HANDLE，头里看不到。
//
// 本项目只做 Windows，所以**没有 ifdef 分派文件**：
// MakeInput() 直接定义在 src/platform/InputWindows.cpp 里。
// 真要跨平台时的成本是"新增一个 InputXxx.cpp"，不是现在把两套都写出来。
//
// 依赖方向：platform → stdlib（本类不认识 render，也不认识 game）。
// 与 Console 的关系：Input **不持有** Console。两者只共享一个前提 ——
// 输入模式（关 QUICK_EDIT、开 WINDOW_INPUT）已经由 Console 设好了，
// 所以 **MakeConsole() 必须先于 MakeInput() 调用**。顺序反了会在构造时抛异常。
// ============================================================

namespace mini::platform {

class Input {
public:
    virtual ~Input() = default;

    // 非阻塞地读空事件队列，把事件折进内部状态。
    // 每帧调一次，位置在"游戏读状态之前"。
    virtual void Poll() = 0;

    // 帧尾推进四态：把本帧的按下折成"上一帧"，并清掉本帧的按下脉冲。
    // **必须在游戏读完之后调**，否则 Pressed 永远看不到。
    virtual void BeginFrame() = 0;

    [[nodiscard]] virtual KeyState Get(Key k) const = 0;
    [[nodiscard]] virtual bool     IsDown(Key k) const = 0;

    // 只反映"外部强制退出"：Ctrl+C / 关窗口 / 注销。
    // **不含 ESC** —— ESC 从 S10 起要用来关菜单，它只是一个普通按键，
    // 该不该退出由 game 层决定（S2 的演示里用 Q 退出）。
    [[nodiscard]] virtual bool QuitRequested() const = 0;
};

std::unique_ptr<Input> MakeInput();

} // namespace mini::platform
