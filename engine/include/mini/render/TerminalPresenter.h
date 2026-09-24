#pragma once

// ============================================================
// TerminalPresenter —— 终端初始化 + 推帧（S1 的临时实现）
//
// ⚠️ S2 要搬家：这一层的正式位置是 engine/src/platform/ConsoleWindows.*，
//    到时这里删掉，BeginTerminal/EndTerminal 变成 platform::Console 的构造/析构。
//    留着 TODO 是为了防止它变成"永久房客"（S1 决策 4）。
//
// 这是全项目**唯一**允许 include <windows.h> 的文件（PLAN A.3 的硬规矩：
// 把平台 API 关在一个文件里，其余代码保持干净的 C++ 写法）。
//
// BeginTerminal / EndTerminal 必须成对：S2 起由 Application 的 RAII 负责，
// S1 里靠 main 里成对调用 + Ctrl+C handler 兜底。
// ============================================================

namespace mini::render {

class FrameBuffer;

// 存原控制台码页与模式 → 切 UTF-8 → 开 VT 处理 → 进备用屏 → 隐光标 → 装 Ctrl+C handler
void BeginTerminal();

// 反着来：显光标 → 离开备用屏 → 恢复原模式与码页
void EndTerminal();

// 光标归位 + 写整屏 + 刷缓冲。**一帧只写一次终端**（PLAN C.5 防闪烁的第一条措施）
void Present(const FrameBuffer& fb);

// S1 临时接口：Ctrl+C / 关窗口之后为 true，main 靠它退出。
// S2 会并入 platform::Input::QuitRequested()
bool QuitRequested();

} // namespace mini::render
