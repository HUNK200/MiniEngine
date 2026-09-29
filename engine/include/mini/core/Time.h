#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

// ============================================================
// core::Time —— 帧时间（S3）
//
// 职责：**只回答"这一帧花了多久、现在多少 FPS"**，不碰终端、不碰输入、不认识游戏。
//
// 三个时间量必须分清（混用会得到"性能完美"的假象）：
//   Delta()     帧间隔，**已 clamp**，秒   → 喂给游戏做积分
//   FrameMs()   同一个帧间隔，**未 clamp** → 顶栏用（要看得到卡顿）
//   WorkMs()    本帧 Begin→End 的耗时      → 顶栏用，也是 S11 Profiler 的雏形
//
// FPS 用**帧间隔**算，不是 1/WorkMs：限帧 60 但只干了 0.3ms 时，后者会报 3000fps。
//
// 分层：`ClampDelta` 与 `FrameStats` 是**纯逻辑**（不读时钟），所以能离线单测；
//       只有 `Time` 自己碰 `steady_clock` 与 `sleep_for`。
// ============================================================

namespace mini::core {

// dt 上限（PLAN S3 设计结论：variable dt + clamp）。
//
// 拖窗口 / 断点 / 系统抢占会产生几百毫秒的大 dt，直接喂给实体就是"瞬移穿墙"。
// 0.05s 意味着低于 20fps 之后就不再按真实时间推进了 —— 宁可"游戏变慢"，
// 也不要"实体穿墙"（等 S9 子弹穿透真的出现，再考虑局部子步进）。
constexpr float kMaxDelta = 0.05f;

// 负 dt 归零：时钟回拨、以及第一帧（没有上一帧可比）都会走到这条分支
constexpr float ClampDelta(float dt) noexcept
{
    if (dt < 0.0f) {
        return 0.0f;
    }
    return dt > kMaxDelta ? kMaxDelta : dt;
}

// 帧间隔的滑动窗口统计。**纯逻辑、不读时钟**，所以能离线单测。
class FrameStats {
public:
    // 窗口按**帧数**算，不是秒数：60 帧在 60fps 下是 1 秒，在 10fps 下是 6 秒。
    // 所以顶栏的"迟钝程度"随实际帧率变化。想更灵敏就调小（30），想更稳就调大。
    static constexpr std::size_t kWindow = 60;

    void Push(float dtSeconds) noexcept
    {
        m_samples[m_next] = dtSeconds;
        m_next = (m_next + 1) % kWindow;
        if (m_count < kWindow) {
            ++m_count;
        }

        const float ms = dtSeconds * 1000.0f;
        if (ms > m_maxMs) {
            m_maxMs = ms;
        }
    }

    void Reset() noexcept
    {
        m_samples.fill(0.0f);
        m_count = 0;
        m_next  = 0;
        m_maxMs = 0.0f;
    }

    // **平均帧间隔的倒数**，不是逐帧 FPS 的平均。
    //
    // 两者差别很大：`平均(1/dtᵢ)` 会被最短那一帧主导（一帧 0.1ms 就能把均值拉到 2000fps），
    // 而 `1 / 平均(dtᵢ)` 才等于"我平均多久出一帧"。测试里有专门钉这条的用例。
    [[nodiscard]] float Fps() const noexcept
    {
        const float avg = AverageSeconds();
        return avg > 0.0f ? 1.0f / avg : 0.0f;
    }

    [[nodiscard]] float AvgFrameMs() const noexcept { return AverageSeconds() * 1000.0f; }

    // 历史最大帧耗时。**滑出窗口也不清零**——它的用途是"长时间跑完之后问一句最坏有多坏"，
    // 所以只被 Reset() 清掉。
    [[nodiscard]] float MaxFrameMs() const noexcept { return m_maxMs; }

    [[nodiscard]] std::size_t SampleCount() const noexcept { return m_count; }

private:
    [[nodiscard]] float AverageSeconds() const noexcept
    {
        if (m_count == 0) {
            return 0.0f;   // 没样本 → 0，调用方负责别做除法（Fps 已经挡住了）
        }
        float sum = 0.0f;
        for (std::size_t i = 0; i < m_count; ++i) {
            sum += m_samples[i];
        }
        return sum / static_cast<float>(m_count);
    }

    std::array<float, kWindow> m_samples{};
    std::size_t m_count = 0;   // 已填样本数（<= kWindow）
    std::size_t m_next  = 0;   // 下一个写入位置（环形）
    float       m_maxMs = 0.0f;
};

class Time {
public:
    // 帧首。算本帧与**上一次 BeginFrame** 的间隔（不是 Begin→End 的耗时）。
    // 第一帧没有"上一帧"，所以 Delta()/FrameMs() 都是 0。
    //
    // ⚠️ 这个方法必须放在循环的**最前面**（在 LimitFPS 之前）。
    //    如果放在 LimitFPS 之后，dt 就只度量"干活耗时"（0.3ms），
    //    顶栏会报 3000fps —— 看起来性能完美，其实全错。
    void BeginFrame() noexcept;

    // 帧尾。算本帧实际干活耗时，并把**未 clamp** 的帧间隔塞进 FrameStats
    void EndFrame() noexcept;

    // 混合睡眠限帧。**会阻塞**。maxFps <= 0 表示不限帧。
    //
    // 为什么不能只 sleep：Windows 上 Sleep 的粒度是调度器 tick（约 15.6ms），
    // 想睡 16.7ms 会睡过头、想睡 1ms 也会睡成 15.6ms（PLAN 的坑）。
    // 所以"粗睡 + 细磨"：先睡到差 spin，再自旋蹭完最后一点。
    void LimitFPS(int maxFps) noexcept;

    [[nodiscard]] float Delta() const noexcept { return m_delta; }
    [[nodiscard]] float FrameMs() const noexcept { return m_frameMs; }
    [[nodiscard]] float WorkMs() const noexcept { return m_workMs; }
    [[nodiscard]] float Fps() const noexcept { return m_stats.Fps(); }
    [[nodiscard]] float MaxFrameMs() const noexcept { return m_stats.MaxFrameMs(); }
    // 这里原先还有一个 `FrameCount()`（返回 m_frames），2026-09-28 删掉了：
    // 全仓库 0 处调用（连测试都没有）—— 留着会让人以为"第几帧"是被用到的信息。
    // 注意 **m_frames 成员本身要保留**：EndFrame 用它跳过首帧的统计。
    // 将来真需要"第几帧"时加回来是一行。

    [[nodiscard]] const FrameStats& Stats() const noexcept { return m_stats; }

private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point m_begin{};       // 本帧起点（EndFrame 量 WorkMs 用）
    Clock::time_point m_prevBegin{};   // 上一帧起点（算帧间隔用）
    bool  m_hasPrev = false;           // 第一帧没有"上一帧起点"
    float m_delta   = 0.0f;            // 已 clamp，秒
    float m_frameMs = 0.0f;            // 未 clamp，毫秒
    float m_workMs  = 0.0f;            // 本帧干活耗时，毫秒
    FrameStats    m_stats;
    std::uint64_t m_frames = 0;
};

} // namespace mini::core
