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
    // （**脏行 diff 之后**，这一句要改成"一帧最多写一次"：内容没变时一个字节都不发）
    //
    // 【返回值】false = **终端已经不在了**（控制台被销毁 / 写失败）。
    //
    // 为什么要返回值：把"终端可能没了"变成类型系统里的东西，逼调用方处理，
    // 而不是让它悄悄烂在循环里。背景是 S2 实测的那个坑 —— 关掉控制台窗口后
    // 进程成孤儿，对着死控制台一直写，**锁住 exe 让后续所有构建失败（LNK1168）**。
    [[nodiscard]] virtual bool Present(const render::FrameBuffer& fb) = 0;

    // 终端**可视区**宽高（srWindow，不是缓冲区尺寸）。
    // S2 没用它（main 按固定 80x25 建 FrameBuffer）；S3 每帧比较它来响应窗口缩放。
    virtual ConsoleSize Size() const = 0;

    // 把**可视区**改成 `want`（S11.5：让 demo 一启动就有个能看的尺寸）。
    //
    // 【语义】尽力而为，四条别搞错：
    //   1. **"只放大不缩小"不是这里的事** —— 那是**调用方**的策略，落在纯函数
    //      `core::ComputeSizeRequest`（可离线测）；这一层只负责"把要求的尺寸设下去"。
    //   2. 缓冲区不够大时**先扩缓冲区、再改窗口** —— 顺序反了系统会拒绝
    //      （`SetConsoleWindowInfo` 要求窗口落在缓冲区里）。
    //   3. **失败不致命**：`false` = 没改成（ConPTY / Windows Terminal 里可能不让改、
    //      或窗口被固定在某个尺寸）。调用方**应当忽略返回值**继续跑 ——
    //      画布尺寸另有守卫（`core::IsUsableCanvasSize`），不会因为改不动就崩。
    //   4. 改了之后**画布尺寸不会自动跟着变**：`Application::Run` 第 ③ 步每帧比较
    //      `Size()`，下一帧就会 `Resize` 画布（并用一次清屏收尾）。
    [[nodiscard]] virtual bool TryResizeWindow(ConsoleSize want) = 0;

    // 上一帧真正**发出去**的行数（脏行 diff 的结果）。顶栏的 "Rows: n/总行数" 用它。
    //
    // 为什么是"上一帧"：本帧到底发了多少行，要等 Present() 之后才知道，
    // 而顶栏是在 Present() **之前**画的。差一帧在视觉上没差别，
    // 但能避免"先画顶栏就要先知道发送量、而发送量又要先画完才知道"的循环依赖。
    [[nodiscard]] virtual int LastFrameRowsSent() const = 0;
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
