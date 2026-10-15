#include "MiniTest.h"

#include <mini/core/Profiler.h>

#include <cstddef>
#include <cstring>    // std::strcmp（段名是 const char*，不能拿 == 比字面量）
#include <iterator>   // std::size

// ------------------------------------------------------------
// core::Profiler 的单元测试（S11）。
//
// 不需要时钟、不需要终端：`AddSample(name, ms)` 是**正式 API**
// （生产路径由 `ScopedTimer` 的析构调用），所以用例可以喂**已知毫秒数**
// ⇒ 全部判据都是确定性的，不用伪造时钟（§10 分叉 E 取甲的全部理由就在这里）。
//
// ⚠️ 段名是 `const char*`：断言名字时**必须 `strcmp`**，不能用 `MINI_CHECK_EQ`
//    去比 `"ScopeA"`（那比的是指针，两个同样的字面量地址**不保证**相同）。
// ------------------------------------------------------------

namespace {

using mini::core::Profiler;

// 喂 n 帧，每帧一段固定耗时
void FeedFrames(Profiler& profiler, const char* name, float ms, int frames)
{
    for (int f = 0; f < frames; ++f) {
        profiler.BeginFrame();
        profiler.AddSample(name, ms);
        profiler.EndFrame();
    }
}

} // namespace

// 窗口内取平均（分母 = **帧数**）。
MINI_TEST(Profiler, AveragesOverWindow)
{
    Profiler profiler;

    const float kValues[] = {1.0f, 2.0f, 3.0f, 4.0f};
    for (const float value : kValues) {
        profiler.BeginFrame();
        profiler.AddSample("ScopeA", value);
        profiler.EndFrame();
    }

    Profiler::Row row{};
    MINI_CHECK_EQ(profiler.Snapshot(&row, 1), 1);
    MINI_CHECK(std::strcmp(row.name, "ScopeA") == 0);
    MINI_CHECK_NEAR(row.avgMs, 2.5f, 1e-4f);   // (1+2+3+4)/4
    MINI_CHECK_NEAR(row.maxMs, 4.0f, 1e-4f);
    MINI_CHECK_EQ(profiler.HistoryFrames(), 4);
}

// 峰值 = 窗口内最大那一帧（不是"最后一帧"，也不是"历史全部"）。
MINI_TEST(Profiler, MaxIsTracked)
{
    Profiler profiler;
    FeedFrames(profiler, "Spike", 1.0f, 2);
    FeedFrames(profiler, "Spike", 9.0f, 1);
    FeedFrames(profiler, "Spike", 1.0f, 1);

    Profiler::Row row{};
    MINI_CHECK_EQ(profiler.Snapshot(&row, 1), 1);
    MINI_CHECK_NEAR(row.avgMs, 3.0f, 1e-4f);   // (1+1+9+1)/4
    MINI_CHECK_NEAR(row.maxMs, 9.0f, 1e-4f);
}

// 环形：喂满 + 多喂 ⇒ 最老的那几帧被挤出去（同时钉住"窗口大小就是 kHistoryFrames"）。
MINI_TEST(Profiler, RingKeepsLastN)
{
    constexpr int   kExtra = 5;
    constexpr float kOld   = 10.0f;
    constexpr float kNew   = 100.0f;

    Profiler profiler;
    FeedFrames(profiler, "S", kOld, Profiler::kHistoryFrames);
    FeedFrames(profiler, "S", kNew, kExtra);

    // 期望值由公开常量算出来，不写死数字
    const float expectedAvg =
        (static_cast<float>(Profiler::kHistoryFrames - kExtra) * kOld +
         static_cast<float>(kExtra) * kNew) /
        static_cast<float>(Profiler::kHistoryFrames);

    Profiler::Row row{};
    MINI_CHECK_EQ(profiler.Snapshot(&row, 1), 1);
    MINI_CHECK_NEAR(row.avgMs, expectedAvg, 1e-3f);
    MINI_CHECK_NEAR(row.maxMs, kNew, 1e-4f);
    MINI_CHECK_EQ(profiler.HistoryFrames(), Profiler::kHistoryFrames);   // 满了就不再涨
}

// ★ 首屏历史不满窗口时，分母必须是**已收帧数**，不能是 kHistoryFrames。
MINI_TEST(Profiler, PartialHistoryDividesByCount)
{
    constexpr int   kFrames = 3;
    constexpr float kMs     = 5.0f;

    Profiler profiler;
    FeedFrames(profiler, "S", kMs, kFrames);

    Profiler::Row row{};
    MINI_CHECK_EQ(profiler.Snapshot(&row, 1), 1);
    // 若实现拿 120 当分母，这里会读到 0.125 ⇒ 这条用例就是为那个错法写的
    MINI_CHECK_NEAR(row.avgMs, kMs, 1e-4f);
    MINI_CHECK_EQ(profiler.HistoryFrames(), kFrames);
}

// 一帧登记超过 kMaxScopes 段：**丢 + 计数**，不越界、不崩、之后照常。
MINI_TEST(Profiler, OverflowDropsWithoutCrash)
{
    constexpr const char* kManyNames[] = {"n0", "n1", "n2", "n3", "n4",
                                          "n5", "n6", "n7", "n8", "n9"};
    // 多出来的那 2 个就是"该被丢掉的"。若以后有人调大 kMaxScopes，这条会先红，
    // 提醒他把名字表补上（而不是让用例悄悄变成"其实没超限"）。
    static_assert(std::size(kManyNames) == static_cast<std::size_t>(Profiler::kMaxScopes) + 2,
                  "名字表的条数必须 = kMaxScopes + 2（多出来的 2 个是故意超限的）");

    Profiler profiler;
    profiler.BeginFrame();
    for (const char* name : kManyNames) {
        profiler.AddSample(name, 1.0f);
    }
    profiler.EndFrame();

    Profiler::Row rows[Profiler::kMaxScopes]{};
    MINI_CHECK_EQ(profiler.Snapshot(rows, Profiler::kMaxScopes), Profiler::kMaxScopes);
    MINI_CHECK_EQ(profiler.Dropped(), 2);

    // "不崩"只是最低要求：再推一帧，确认状态没被踩坏、且 dropped 不会继续涨
    profiler.BeginFrame();
    profiler.AddSample("n0", 1.0f);
    profiler.EndFrame();
    MINI_CHECK_EQ(profiler.HistoryFrames(), 2);
    MINI_CHECK_EQ(profiler.Dropped(), 2);
    MINI_CHECK_EQ(profiler.Unbalanced(), 0);
}

// 关闭时：三个入口全是 no-op ⇒ 历史**原样冻结**，连新段都不许登记。
MINI_TEST(Profiler, DisabledIsNoOp)
{
    Profiler profiler;
    FeedFrames(profiler, "S", 1.0f, 5);

    Profiler::Row row{};
    MINI_CHECK_EQ(profiler.Snapshot(&row, 1), 1);
    MINI_CHECK_NEAR(row.avgMs, 1.0f, 1e-4f);

    profiler.SetEnabled(false);
    FeedFrames(profiler, "S", 100.0f, 5);
    profiler.AddSample("NewScopeWhileDisabled", 7.0f);

    Profiler::Row rows[2]{};
    MINI_CHECK_EQ(profiler.Snapshot(rows, 2), 1);   // 段的条数没变
    MINI_CHECK_NEAR(rows[0].avgMs, 1.0f, 1e-4f);    // 数字冻结在关闭那一刻
    MINI_CHECK_NEAR(rows[0].maxMs, 1.0f, 1e-4f);
    MINI_CHECK_EQ(profiler.HistoryFrames(), 5);     // 一帧都没多收
    MINI_CHECK_EQ(profiler.Dropped(), 0);
}

// 配平自检：手动不配对时**报出来**（RAII 正常路径做不到这件事，所以只能手动造）。
MINI_TEST(Profiler, UnbalancedScopeIsReported)
{
    Profiler profiler;
    MINI_CHECK_EQ(profiler.Unbalanced(), 0);

    profiler.BeginFrame();
    profiler.BeginScope();   // 故意不 EndScope
    profiler.EndFrame();
    MINI_CHECK_EQ(profiler.Unbalanced(), 1);

    // ★ 下一帧必须**不再**计：一次误用不该让报警数从此只增不减
    profiler.BeginFrame();
    profiler.EndFrame();
    MINI_CHECK_EQ(profiler.Unbalanced(), 1);

    // 多调一次 EndScope 同样算配平错（不静默吞掉）
    profiler.EndScope();
    MINI_CHECK_EQ(profiler.Unbalanced(), 2);
}

// `MINI_PROFILE_SCOPE` 的三条路径：未装 / 装着但关闭 / 装着且打开。
MINI_TEST(Profiler, ScopedTimerWithoutProfilerIsSafe)
{
    // ① 未装 profiler —— **这是默认情形**（既有用例都不装），必须空转
    mini::core::SetCurrentProfiler(nullptr);
    { MINI_PROFILE_SCOPE("no-profiler"); }

    // ② 装着但关闭 ⇒ 同样空转，且一个段都不许登记
    Profiler off;
    off.SetEnabled(false);
    mini::core::SetCurrentProfiler(&off);
    { MINI_PROFILE_SCOPE("disabled"); }

    Profiler::Row row{};
    MINI_CHECK_EQ(off.Snapshot(&row, 1), 0);
    MINI_CHECK_EQ(off.Dropped(), 0);

    // ③ 装着且打开 —— 这是 `ScopedTimer` 的**真实路径**，必须真的登记一段。
    //    没有这一半，前两半即使"计时器根本没接线"也照样是绿的。
    Profiler on;
    mini::core::SetCurrentProfiler(&on);
    on.BeginFrame();
    { MINI_PROFILE_SCOPE("scoped"); }
    on.EndFrame();

    MINI_CHECK_EQ(on.Snapshot(&row, 1), 1);
    MINI_CHECK(std::strcmp(row.name, "scoped") == 0);
    MINI_CHECK(row.maxMs >= 0.0f);        // 只钉"是个合法数字"，不钉具体耗时（那是机器相关的）
    MINI_CHECK_EQ(on.HistoryFrames(), 1);

    mini::core::SetCurrentProfiler(nullptr);
}
