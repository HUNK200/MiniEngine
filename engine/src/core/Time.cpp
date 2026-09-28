#include "mini/core/Time.h"

#include <chrono>
#include <thread>

// ============================================================
// core::Time 里"要读时钟"的那部分（S3）
//
// 头里只留纯逻辑（ClampDelta / FrameStats），所有 steady_clock 与 sleep_for
// 都收在这一个文件里 —— 这样 FrameStats 的窗口算法能被离线单测，
// 而"睡多久"这种依赖系统调度器的事只在一个地方，改的时候不用翻遍代码。
// ============================================================

namespace mini::core {

void Time::BeginFrame() noexcept
{
    const auto now = Clock::now();

    if (m_hasPrev) {
        // 帧间隔 = 本次 Begin 到上次 Begin。**包含**了上一帧的干活时间和限帧睡眠，
        // 这才是"真实帧率"。用 Begin→End 的耗时当 dt 会得到假的漂亮数字。
        const std::chrono::duration<float, std::milli> interval = now - m_prevBegin;
        m_frameMs = interval.count();
        m_delta   = ClampDelta(m_frameMs * 0.001f);
    } else {
        // 第一帧：没有上一帧可比。0 而不是"猜一个 1/60"——
        // 猜出来的值会让第一帧的积分凭空多走一段。
        m_hasPrev = true;
        m_frameMs = 0.0f;
        m_delta   = 0.0f;
    }

    m_prevBegin = now;
    m_begin     = now;
    ++m_frames;
}

void Time::EndFrame() noexcept
{
    const std::chrono::duration<float, std::milli> work = Clock::now() - m_begin;
    m_workMs = work.count();

    // 统计用的是**未 clamp** 的帧间隔：clamp 只是"别让游戏瞬移"的保护，
    // 报告的帧率必须是真实的。否则卡到 200ms 时顶栏还显示 60fps。
    //
    // 第一帧（m_frames == 1）没有间隔可统计，塞进去会把平均值拉高。
    if (m_frames > 1) {
        m_stats.Push(m_frameMs * 0.001f);
    }
}

void Time::LimitFPS(int maxFps) noexcept
{
    if (maxFps <= 0) {
        return;   // 不限帧
    }

    // 目标帧时间用**整数纳秒**算：1e9/60 = 16666666ns。
    // 用 float(1.0f/60) 累加会漂（60fps 稳定跑成 59.94）。
    const auto target = std::chrono::nanoseconds(1'000'000'000LL / maxFps);

    const auto elapsed = Clock::now() - m_begin;
    if (elapsed >= target) {
        return;   // 这一帧已经超时，不睡
    }

    const auto remain = target - elapsed;

    // 自旋窗口 = min(2ms, 目标帧时间的 1/4)。
    //
    // **不能写死 2ms**：targetFps=300 时目标帧时间只有 3.33ms，
    // 2ms 自旋占了 60% 的帧时间 —— 限帧器自己变成 CPU 烧穿机。
    // 取 1/4 让高帧率下自旋窗口自动缩小。
    auto spin = std::chrono::milliseconds(2);
    const auto quarter = target / 4;
    if (quarter < spin) {
        spin = std::chrono::duration_cast<std::chrono::milliseconds>(quarter);
    }

    if (remain > spin) {
        std::this_thread::sleep_for(remain - spin);
    }

    // 细磨：把最后一点时间用 yield 蹭掉。
    // yield 而不是空转，是为了把时间片让给别的线程（不会把 CPU 占成 100% 的忙等）。
    while (Clock::now() - m_begin < target) {
        std::this_thread::yield();
    }
}

} // namespace mini::core
