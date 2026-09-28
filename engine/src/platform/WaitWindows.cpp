#include "mini/platform/Wait.h"

#include <windows.h>

#include <thread>

// ============================================================
// platform::Wait 的 Windows 实现（S3）
//
// 用高精度可等待计时器替代 Sleep —— 后者粒度是调度器 tick（本机实测 15.625ms），
// 而 60fps 的目标帧时间 16.667ms 只比它大 1.04ms，会周期性睡过头一整跳。
// 详见 Wait.h 的注释。
//
// 三处容易踩的地方：
//   1. CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 是 Win10 1803 才有的宏，
//      老 SDK 里没有 —— 要自己兜一个 #ifndef
//   2. SetWaitableTimer 的到期时间单位是 **100 纳秒**，且**负数才表示相对时间**
//      （正数是绝对时间，会把 1970 年当成目标）
//   3. 计时器句柄只创建一次、进程内复用；不能每次 Wait 都 Create/Close
//      （那个开销比等 2ms 还大）
// ============================================================

namespace mini::platform {
namespace {

// 老 SDK 里可能没有这个名字（值来自 synchapi.h / Windows 10 1803）
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
#define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

// 懒创建 + 进程内复用。函数内静态变量的初始化是线程安全的（magic static），
// 所以不需要额外加锁。
//
// 成功 → 返回句柄；失败（老系统不支持这个 flag）→ 返回 nullptr，
// 调用方退回 sleep_for。句柄故意不 Close：它跟着进程活到结束，
// 注册进 CRT 的析构顺序问题不值得为这点资源去冒。
HANDLE HighResolutionTimer() noexcept
{
    static HANDLE timer = [] () -> HANDLE {
        return ::CreateWaitableTimerExW(nullptr,
                                        nullptr,
                                        CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                        TIMER_ALL_ACCESS);
    }();
    return timer;
}

// nanoseconds → SetWaitableTimer 要的 100ns 单位（负数 = 相对时间）
LONGLONG ToHundredNanos(std::chrono::nanoseconds duration) noexcept
{
    LONGLONG ticks = static_cast<LONGLONG>(duration.count()) / 100;
    // 不足 100ns 时不能变成 0：0 是"立即到期"，这里要的是"至少等一下"
    if (ticks == 0) {
        ticks = 1;
    }
    return -ticks;
}

} // namespace

void Wait(std::chrono::nanoseconds duration) noexcept
{
    if (duration <= std::chrono::nanoseconds::zero()) {
        return;
    }

    const HANDLE timer = HighResolutionTimer();
    if (timer == nullptr) {
        // 老系统（Win10 1803 以前）。宁可退回粗粒度，也不抛异常
        std::this_thread::sleep_for(duration);
        return;
    }

    LARGE_INTEGER due{};
    due.QuadPart = ToHundredNanos(duration);

    if (::SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE) == 0) {
        std::this_thread::sleep_for(duration);
        return;
    }

    (void)::WaitForSingleObject(timer, INFINITE);
}

} // namespace mini::platform
