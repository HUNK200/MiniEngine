#include "MiniTest.h"

#include <mini/core/Time.h>
#include <mini/platform/Wait.h>

#include <chrono>
#include <cstddef>

// ------------------------------------------------------------
// core::Time 的单元测试（S3）。
//
// 这个文件**不需要时钟、不需要终端**：`ClampDelta` 与 `FrameStats` 都是纯逻辑，
// 样本是我们自己 Push 进去的，所以"窗口算法对不对"可以精确断言，
// 而不是"跑一会儿看数字大小像不像 60"。
//
// 碰真实时钟的三条（`platform::Wait` / `LimitFPS`）也能测，但测法与上面不同：
// 只能给**上下界**，不能要精确值。能这么测的前提是 S3 把 `sleep_for` 换成了
// `platform::Wait`（高精度可等待计时器）—— 用 `sleep_for` 时帧时间会被
// 15.625ms 的调度器 tick 撞到 31ms，上界卡不住任何东西。
// ------------------------------------------------------------

namespace {

using mini::core::ClampDelta;
using mini::core::FrameStats;
using mini::core::kMaxDelta;

constexpr float kMs60 = 1000.0f / 60.0f;   // 16.667ms，60fps 的一帧

} // namespace

// 负 dt 必须归零：时钟回拨会给出负间隔，直接喂给积分就是"往反方向走"
MINI_TEST(Time, ClampDeltaRejectsNegative)
{
    MINI_CHECK_NEAR(ClampDelta(-1.0f), 0.0f, 1e-6f);
    MINI_CHECK_NEAR(ClampDelta(-0.001f), 0.0f, 1e-6f);
    MINI_CHECK_NEAR(ClampDelta(0.0f), 0.0f, 1e-6f);   // 第一帧就是 0
}

// 大 dt 必须被夹住：拖窗口/断点产生的几百毫秒会让实体瞬移穿过墙（S5 会亲眼看到）
MINI_TEST(Time, ClampDeltaCapsSpikes)
{
    MINI_CHECK_NEAR(ClampDelta(0.5f), kMaxDelta, 1e-6f);
    MINI_CHECK_NEAR(ClampDelta(100.0f), kMaxDelta, 1e-6f);
    MINI_CHECK_NEAR(ClampDelta(kMaxDelta), kMaxDelta, 1e-6f);   // 正好在上限上，不削

    // 正常范围内必须原样通过 —— clamp 不是"把所有值都改成 0.05"
    MINI_CHECK_NEAR(ClampDelta(0.01f), 0.01f, 1e-6f);
    MINI_CHECK_NEAR(ClampDelta(kMaxDelta * 0.5f), kMaxDelta * 0.5f, 1e-6f);
}

// 60 帧全是 16.667ms → FPS 就该是 60
MINI_TEST(Time, FpsUsesAverageInterval)
{
    FrameStats stats;
    for (int i = 0; i < 60; ++i) {
        stats.Push(1.0f / 60.0f);
    }

    MINI_CHECK_NEAR(stats.Fps(), 60.0f, 0.05f);
    MINI_CHECK_NEAR(stats.AvgFrameMs(), kMs60, 0.02f);
    MINI_CHECK_EQ(stats.SampleCount(), std::size_t{60});
}

// 【本文件最重要的一条】一帧异常快不能把 FPS 带飞。
//
// 用的是"平均帧间隔的倒数"，所以一帧 0.1ms 只把平均拉低一点点（60 → 61）；
// 如果写成"逐帧 FPS 求平均"，同样的数据会报 ~225fps。
// 这个差别在真实场景里的表现是：偶发的一帧极短会让顶栏数字乱跳。
MINI_TEST(Time, FpsIgnoresOneFastFrame)
{
    FrameStats stats;
    for (int i = 0; i < 59; ++i) {
        stats.Push(1.0f / 60.0f);
    }
    stats.Push(0.0001f);   // 0.1ms 的一帧

    const float fps = stats.Fps();

    // 精确值 ≈ 61.0；关键是它**没有**跳到 225
    MINI_CHECK(fps > 60.0f);
    MINI_CHECK(fps < 63.0f);
    MINI_CHECK(fps < 100.0f);   // 这一条才是"没写错算法"的证据
}

// 没样本时不能除以零
MINI_TEST(Time, FpsZeroSamplesIsZero)
{
    FrameStats stats;

    MINI_CHECK_NEAR(stats.Fps(), 0.0f, 1e-6f);
    MINI_CHECK_NEAR(stats.AvgFrameMs(), 0.0f, 1e-6f);
    MINI_CHECK_NEAR(stats.MaxFrameMs(), 0.0f, 1e-6f);
    MINI_CHECK_EQ(stats.SampleCount(), std::size_t{0});
}

// 窗口是环形的：填满之后新样本覆盖最老的，而不是"记满就停"
MINI_TEST(Time, WindowOverwritesOldest)
{
    FrameStats stats;

    // 先塞 10 个"极慢帧"（1 秒一帧 = 1fps），它们必须被后面的 60 个样本全部挤出去
    for (int i = 0; i < 10; ++i) {
        stats.Push(1.0f);
    }
    for (int i = 0; i < 60; ++i) {
        stats.Push(0.01f);   // 10ms 一帧 = 100fps
    }

    MINI_CHECK_EQ(stats.SampleCount(), FrameStats::kWindow);
    MINI_CHECK_NEAR(stats.Fps(), 100.0f, 0.5f);   // 10 个慢帧一个都不该剩
}

// 最大帧耗时**不随窗口滑走**：它的用途是"跑完一段之后问最坏有多坏"，
// 所以只有 Reset() 能清掉它。报表窗口和极值窗口是两件事。
MINI_TEST(Time, MaxFrameMsSurvivesWindow)
{
    FrameStats stats;

    stats.Push(0.5f);   // 500ms 的尖峰，随后会被 100 个正常样本挤出滑动窗口
    for (int i = 0; i < 100; ++i) {
        stats.Push(0.01f);
    }

    MINI_CHECK_NEAR(stats.MaxFrameMs(), 500.0f, 0.5f);   // 还记得那次尖峰
    MINI_CHECK_NEAR(stats.Fps(), 100.0f, 0.5f);          // 但平均已经恢复正常

    stats.Reset();
    MINI_CHECK_NEAR(stats.MaxFrameMs(), 0.0f, 1e-6f);    // 只有 Reset 能清
    MINI_CHECK_EQ(stats.SampleCount(), std::size_t{0});
}

// platform::Wait 的精度是 S3 限帧能不能达到 58~61 的**前提**。
//
// 本机实测（10 次采样）：`Wait(7ms)` → 7.05~7.56ms，而 `sleep_for(7ms)` → 16.10ms。
// 上界取 12ms：比 15.6ms 的调度器 tick 低 3.6ms（能抓住"有人把 Wait 改回 sleep_for"），
// 又比实测最大 7.56ms 高 4.4ms（高负载下不至于假报警）。
MINI_TEST(Time, PlatformWaitBeatsSleepGranularity)
{
    const auto start = std::chrono::steady_clock::now();
    mini::platform::Wait(std::chrono::milliseconds(7));
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                        std::chrono::steady_clock::now() - start).count();

    MINI_CHECK(us >= 6500);   // 不能早醒
    MINI_CHECK(us < 12000);   // 关键：不能是 15.6ms 的调度器 tick
}

// 【限帧的总守卫】`LimitFPS` 必须真的把一帧拉长到目标帧时间。
//
// 为什么必须有这一条 —— 它是"S3 步骤 1 那个 bug 还能无声回来"的唯一防线：
//
//   当时的 `LimitFPS` 用 `sleep_for`，帧时间被 15.625ms 的调度器 tick 撞坏，
//   顶栏 FPS 抖在 40~60。是**靠肉眼看 FPS 抖**才发现的。
//   而 `PlatformWaitBeatsSleepGranularity` 测的是 `platform::Wait()` 本身 ——
//   它照不到 `LimitFPS` 的组合逻辑（怎么算 remain、spin 取多少、
//   早退条件、和 `BeginFrame` 是不是用同一个 m_begin）。
//   **所以把它改回 `sleep_for`，34 个用例依然全绿。**
//
// 采样点为什么是 60 和 **56**（56 不是笔误）：
//
//   `LimitFPS` 是先 `Wait(remain - spin)` 再自旋，`spin = min(2ms, target/4)`。
//   把 `Wait` 换成 `sleep_for` 之后，实际睡多久取决于**请求值落在 tick 网格的哪一侧**：
//
//     60fps：请求 14.667ms → `Sleep(15)` → 15ms **不到** 15.625ms 的 tick
//            → 只睡一跳（15.6ms）→ 后面自旋补到 16.667ms → **帧时间正常**
//            → 单看 60fps **抓不住这个 bug**
//     56fps：请求 15.857ms → `Sleep(16)` → 16ms **越过** 15.625ms 的 tick
//            → 必须睡两跳 = **31.25ms** → 确定性地超时
//
//   所以 56fps 是**故意挑在 tick 的另一侧**，把"tick 粒度睡眠"从"有时发生"
//   变成"每次都发生"。这才是这条用例站得住的理由。
//
// 4 个界的来历：
//   60fps 下界 16ms —— 目标 16.667ms；早醒说明 remain/spin 算错或根本没等
//   60fps 上界 25ms —— 卡在"正确 ≈16.7ms"与"31.25ms（两跳）"之间
//   56fps 下界 17ms —— 目标 17.857ms；自旋循环保证不会早于目标
//   56fps 上界 25ms —— 正确 ≈18ms；tick 粒度版会到 31.25ms
//
// ⚠️ 这条用例**只钉住"帧时间对不对"，钉不住"CPU 花得值不值得"**：
//    自旋窗口 2ms 是每帧实打实烧的 CPU，而它发生在 `EndFrame()` 之后，
//    所以顶栏的 `Work` 看不见它。要管那个得另外量（见 `TECH_DEBT.md` C 组）。
MINI_TEST(Time, LimitFpsAchievesTargetFrameTime)
{
    mini::core::Time time;

    // ---- 采样 1：真实用的 60fps ----
    {
        const auto start = std::chrono::steady_clock::now();
        time.BeginFrame();     // 锚定基准：LimitFPS 就是从这一刻算 elapsed
        time.LimitFPS(60);
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - start).count();

        MINI_CHECK(us >= 16000);   // 不能早醒
        MINI_CHECK(us < 25000);
    }

    // ---- 采样 2：56fps —— 让 tick 粒度睡眠暴露出来的那个采样点 ----
    {
        const auto start = std::chrono::steady_clock::now();
        time.BeginFrame();
        time.LimitFPS(56);
        const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now() - start).count();

        MINI_CHECK(us >= 17000);   // 目标 17.857ms，自旋保证不会早于它
        MINI_CHECK(us < 25000);    // tick 粒度版会到 31250us
    }
}
