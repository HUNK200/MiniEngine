#include "MiniTest.h"

#include <mini/core/Time.h>

#include <cstddef>

// ------------------------------------------------------------
// core::Time 的单元测试（S3）。
//
// 这个文件**不需要时钟、不需要终端**：`ClampDelta` 与 `FrameStats` 都是纯逻辑，
// 样本是我们自己 Push 进去的，所以"窗口算法对不对"可以精确断言，
// 而不是"跑一会儿看数字大小像不像 60"。
//
// 真正碰系统时钟的部分（BeginFrame / EndFrame / LimitFPS）**测不了**：
// 它依赖调度器的睡眠粒度（Windows 上是 ~15.6ms 的 tick），
// 只能靠 S3_CHECKLIST 10.2 的手动验收 + 顶栏读数。
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
