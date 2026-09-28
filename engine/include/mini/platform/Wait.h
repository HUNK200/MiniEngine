#pragma once

#include <chrono>

// ============================================================
// platform::Wait —— 精确等待（S3）
//
// 为什么不能直接用 std::this_thread::sleep_for：
//   Windows 上 Sleep 的粒度是**调度器 tick（本机实测 15.625ms）**，
//   而 60fps 的目标帧时间是 16.667ms —— 两者只差 1.04ms。
//   用 sleep_for 会周期性地"睡过头一整跳"（实测 Sleep(16) → 31.26ms），
//   表现是每 15 帧左右崩一帧、顶栏 FPS 在 40~60 之间抖。
//
// 这里用的是**高精度可等待计时器**（CreateWaitableTimerExW +
// CREATE_WAITABLE_TIMER_HIGH_RESOLUTION，Win10 1803+），精度 1ms 以内，
// 而且**不需要链任何额外的库**（kernel32 默认就链），
// 也不用 timeBeginPeriod —— 那个会影响全系统定时器精度，副作用太大。
//
// 平台降级：老系统（< 1803）拿不到高精度计时器时**退回 sleep_for**，
// 行为退回"粒度 15.6ms"。宁可慢，也不能崩或抛异常。
//
// 位置：这是 Win32 调用，所以按 A.3 硬规矩只能待在 platform/ 的 .cpp 里。
// ============================================================

namespace mini::platform {

// 等待至少 duration。duration <= 0 时立即返回。
// 精度：Win10 1803+ 约 1ms 以内；更老的系统退回 sleep_for（约 15.6ms 一跳）。
void Wait(std::chrono::nanoseconds duration) noexcept;

} // namespace mini::platform
