#pragma once

#include <memory>

// ============================================================
// platform::Console —— 终端模式 + 推帧（S2）
//
// 职责：**只管终端设施，不管输入事件**。
//   - 构造：存下原控制台状态 → 切 UTF-8 码页 → 开 VT 处理 → 关 QUICK_EDIT
//           → 开 WINDOW_INPUT（为了收 FOCUS_EVENT）→ 进备用屏 → 隐光标
//   - 析构：反着来
//   - Present()：光标归位 + 写整屏 + 一次 flush
//
// S1 那对"必须成对调用、漏一个就把终端留在备用屏"的全局函数
// （BeginTerminal / EndTerminal）到此变成 RAII：析构负责收尾，
// 所以 main 里的 try/catch(...) { EndTerminal(); throw; } 补丁可以删掉。
//
// 公共头里**不出现 <windows.h> / #ifdef**（PLAN A.3 硬规矩）：
// HANDLE、原模式、原码页全部锁在 src/platform/ConsoleWindows.cpp 里。
//
// 依赖方向：platform → render（因为 Present 要收 FrameBuffer）。
//           不认识 Cell 之外的渲染概念，也不认识 game。
// ============================================================

namespace mini::render {
class FrameBuffer;   // 只需要引用，不需要完整定义
}

namespace mini::platform {

struct ConsoleSize {
    int width = 0;
    int height = 0;
};

class Console {
public:
    virtual ~Console() = default;

    // 光标归位（ESC[H，**不清屏**）+ 写整屏 + flush。
    // 一帧只写一次终端 —— PLAN C.5 防闪烁的第一条措施。
    virtual void Present(const render::FrameBuffer& fb) = 0;

    // 终端**可视区**宽高（srWindow，不是缓冲区尺寸）。
    // S2 不用它（main 仍按固定 80x25 建 FrameBuffer），留给 S3 建/重建后缓冲。
    virtual ConsoleSize Size() const = 0;
};

// 唯一的创建入口。
//
// **失败路径是有意选的**：stdout 被重定向时（`> out.txt`、管道、VS Code 的
// "输出"面板）GetConsoleMode 会失败，此时**抛 std::runtime_error 并带人话提示**，
// 而不是静默降级成一个什么都不显示的程序。终端游戏在非终端里跑没有意义，
// 静默失败只会让人对着一个没画面的 exe 发呆。
std::unique_ptr<Console> MakeConsole();

// 进程级"该退出了"标志：Ctrl+C / 关窗口 / 注销。
//
// 为什么是自由函数而不是 Console 的成员：SetConsoleCtrlHandler 是**进程级**的，
// 不存在"哪个 Console 对象收到了 Ctrl+C"这回事，硬挂成成员会造出一个假的归属关系。
// handler 在 MakeConsole() 里安装（见 ConsoleWindows.cpp）。
//
// 注意：**不含 ESC**。ESC 从 S10 起要用来关菜单，它只是一个普通按键，
// 该不该退出由 game 层决定（S2 的演示里用 Q 退出）。
bool QuitRequested();

} // namespace mini::platform
