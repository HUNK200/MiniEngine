#include "mini/core/Time.h"

#include "mini/platform/Wait.h"

#include <chrono>
#include <thread>

// ============================================================
// core::Time 里"要读时钟"的那部分（S3）
//
// 头里只留纯逻辑（ClampDelta / FrameStats），所有 steady_clock 与等待
// 都收在这一个文件里 —— 这样 FrameStats 的窗口算法能被离线单测，
// 而"等多久、怎么等"只在一个地方，改的时候不用翻遍代码。
//
// ⚠️ 等待用的是 `platform::Wait` 而**不是** `std::this_thread::sleep_for`：
//    Windows 上 Sleep 的粒度是调度器 tick（本机实测 15.625ms），而 60fps 的
//    目标帧时间 16.667ms 只比它大 1.04ms —— 用 sleep_for 会周期性睡过头一整跳
//    （实测 Sleep(16) → 31.26ms），顶栏 FPS 会在 40~60 之间抖。
//    见 platform/Wait.h 的注释与 doc/S3_CHECKLIST.md 的决策 8。
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
    //
    // 理由要说准（2026-09-28 修正，原先写的"float 累加会漂"是**错的**）：
    //   ✅ 真正的收益是**避免毫秒截断** —— 写成 milliseconds(1000/60) 只得到 16ms
    //      （比 16.667ms 短 4%），顶栏会稳定显示 ~62 而不是 60，一眼就能看出来。
    //   ❌ "float(1.0f/60) 累加会漂成 59.94" 站不住：
    //      ① 每帧都以**当帧的 m_begin** 重新算 elapsed，误差没有跨帧累积的通路；
    //      ② 1.0f/60 的浮点误差约 1ns/帧，而顶栏的分辨率是 0.1ms —— 差 1000 倍；
    //      ③ 59.94 是 NTSC 的 1000/1001 系数，跟浮点累加没关系。
    //   所以整数版不是"防漂移"，是"帧时间精确 + 可复现"。详见 TECH_DEBT.md B4。
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
    //
    // 用纳秒而不是毫秒存：targetFps > 125 时 target/4 < 8ms，
    // 一 cast 到毫秒就截断成 0（等于完全不细磨），精度白白丢掉。
    auto spin = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(2));
    const auto quarter = target / 4;
    if (quarter < spin) {
        spin = quarter;
    }

    if (remain > spin) {
        // platform::Wait 而不是 sleep_for：后者的粒度是调度器 tick（本机 15.6ms），
        // 在这里会把"睡到差 2ms"直接变成"睡过头一整跳"。见文件头注释。
        platform::Wait(remain - spin);
    }

    // 细磨：把最后一点时间用 yield 蹭掉。
    // yield 而不是空转，是为了把时间片让给别的线程（不会把 CPU 占成 100% 的忙等）。
    while (Clock::now() - m_begin < target) {
        std::this_thread::yield();
    }
}

} // namespace mini::core
