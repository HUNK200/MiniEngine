#pragma once

// ============================================================
// mini/core/Profiler.h —— 分段计时 + 最近 120 帧的环形缓冲（S11 的"看得清"之二）
//
// 它要回答的问题只有一个：**这一帧的时间花在哪**。
// 做法是最小可用的那一种（§2「明确不做什么」：不做 Chrome Trace JSON 导出、不做多线程）：
//   · `MINI_PROFILE_SCOPE("Game.Update")` 在作用域里放一个 RAII 计时器；
//   · 每帧结束把"本帧各段的耗时"推一格进**定长环形缓冲**（绝不每帧分配内存
//     —— 那会污染被测量的东西，见 `S11_CHECKLIST` §8 坑表第 2 条的同类理由）；
//   · `Snapshot()` 把窗口内的 `avg / max` 写进**调用方数组**（不返回内部指针，
//     同 `FrameBuffer::At` 与 `RowDiff::Compose` 的取舍：调用方不必关心内部布局）。
//
// ------------------------------------------------------------
// ⚠️ 与施工图 §4.2 的**两处偏差**（都在本文件里落地，报告里单列了原因）：
//
//   1. §4.2 的字段表写着 `int m_depth = 0; // BeginScope/EndScope 配平自检`，
//      但它的**公开声明表里没有** `BeginScope`/`EndScope` —— 只有 `AddSample` 的话
//      `m_depth` **没有任何改变者**（也就没有 `UnbalancedScopeIsReported` 这条用例）。
//      ⇒ 补齐 `BeginScope()` / `EndScope()`：`ScopedTimer` 是它们唯一的**生产**调用者，
//        而"配平自检"这条路径由用例手动制造（见 tests/test_profiler.cpp 的最后一条）。
//   2. 自检结果得**看得见**才有意义（同 `TECH_DEBT` **C23** 的出路是"给一个公开观察面"）：
//      ⇒ 多两个只读访问器 `Unbalanced()` / `Dropped()`；外加 `HistoryFrames()`
//        （让 overlay 能显示"-- profile (57f) --"这种真实窗口大小，而不是死的 120）。
//
//   §4.2 里列的 `m_used` 与 `m_names` **合并成了一个 `m_nameCount`** ——
//   同一件事不存两份（那种"两个计数器迟早对不上"的活儿本项目不干）。
// ------------------------------------------------------------

#include <array>
#include <chrono>

namespace mini::core {

// 计时用 steady_clock：它单调、不受系统时间调整影响。
// ⚠️ 与 `Logger` 的 system_clock **故意不同** —— 那边要的是"日历时间"，这边要的是"两个时间点之差"。
using ProfileClock = std::chrono::steady_clock;

class Profiler {
public:
    // 一帧最多登记 8 个段（现用的 7 个名字 + 1 条余量）。
    // ⚠️ 超了**只丢 + 计数**，**绝不扩容**：扩容就意味着每帧可能分配内存，
    //    而被测数据会被"分配"这件事本身污染（§8 坑 2）。
    static constexpr int kMaxScopes = 8;

    // 最近 120 帧（`PLAN` 的 S11 原话）。120 帧在 60fps 下正好是 2 秒。
    static constexpr int kHistoryFrames = 120;

    // 关闭时**近似零开销**：`ScopedTimer` 在构造那一刻就查它，
    // 关闭时**连时钟都不取**（§8 坑 5 —— "关闭时仍有开销"会让"< 0.2ms"这条自证不成立）。
    void SetEnabled(bool on) noexcept;
    [[nodiscard]] bool Enabled() const noexcept;

    // 帧首：清本帧累加器（**不动**已登记的段名，见下）。
    void BeginFrame() noexcept;

    // 累加入口。**它是正式 API 而不是"测试专用"**：`ScopedTimer` 的析构就是通过它落库的
    // ⇒ 用例可以喂**已知毫秒数**，从而不需要伪造时钟就能做确定性断言（`S11_CHECKLIST` §10 分叉 E 取甲）。
    // ⚠️ `name` 必须是**静态存储**的字符串字面量：Profiler 只存指针（§8 坑 1）。
    void AddSample(const char* name, float ms) noexcept;

    // 帧尾：把本帧各段耗时推进环形缓冲。**每帧恰好调一次**。
    void EndFrame() noexcept;

    // 作用域配对（只计深度，不记时间 —— 时间是 `ScopedTimer` 自己取的）。
    // 生产调用者是 `ScopedTimer`；手动不配对时由 `Unbalanced()` 计入。
    void BeginScope() noexcept;
    void EndScope() noexcept;

    // 一段的统计。`name` 指向调用方传进来的字面量（Profiler 不拥有它）。
    struct Row {
        const char* name;
        float       avgMs;
        float       maxMs;
    };

    // 把**已登记**的段写进调用方数组，返回条数（<= min(段数, cap)）。
    // 顺序 = **段名首次出现的顺序**（固定的列序；不会因为某一帧少跑一段就串行）。
    // ⚠️ 分母是"**窗口内的帧数**"，不是"这一段被采样的次数" ——
    //    与 `FrameStats` 按帧计窗口是同一个口径（某一帧没跑这段就按 0 计入平均）。
    int Snapshot(Row* out, int cap) const noexcept;

    [[nodiscard]] int HistoryFrames() const noexcept;   // 已收帧数（<= kHistoryFrames）
    [[nodiscard]] int Dropped() const noexcept;         // 因超过 kMaxScopes 被丢掉的采样次数
    [[nodiscard]] int Unbalanced() const noexcept;      // 配平自检报警次数（两种误用都算）

private:
    struct Acc {
        float sum = 0.0f;   // 本帧累计
        float max = 0.0f;   // 本帧峰值
    };

    // 段名登记表：**跨帧稳定**（首现顺序），所以 overlay 的表格行不会跳来跳去。
    std::array<const char*, kMaxScopes> m_names{};
    int                                 m_nameCount = 0;

    // 本帧累加器：下标与 `m_names` 一一对应 ⇒ `BeginFrame` 只清数值、不清名字。
    std::array<Acc, kMaxScopes> m_acc{};

    // 环形缓冲：`m_histHead` = **下一次写入**的位置。写满之后"最老的那一帧"正好也在 head 上。
    std::array<std::array<float, kMaxScopes>, kHistoryFrames> m_hist{};
    int m_histCount = 0;
    int m_histHead  = 0;

    int  m_depth       = 0;   // 当前未结束的作用域数
    int  m_unbalanced  = 0;
    int  m_dropped     = 0;
    bool m_enabled     = true;   // 默认开：overlay 一开就有数可看（开关在 game 侧）
};

// RAII 计时器：构造记名字 + **取时钟**；析构算出毫秒交给 `AddSample`。
//
// ⚠️ 三条容易踩的（都在实现里落成注释）：
//   1. 关闭时**不取时钟**（§8 坑 5）；
//   2. 构造时把"哪一台 Profiler"记下来：中途换过 `SetCurrentProfiler` 也不会把耗时报给别的实例；
//   3. 未装 profiler（`CurrentProfiler() == nullptr`）时**空转** ——
//      这是默认情形（几乎所有既有用例都没装），所以它必须是安全的。
class ScopedTimer {
public:
    explicit ScopedTimer(const char* name) noexcept;
    ~ScopedTimer();

    ScopedTimer(const ScopedTimer&)            = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;

private:
    const char*              m_name     = nullptr;
    ProfileClock::time_point m_start{};             // 只在 m_active 为真时有效
    Profiler*                m_profiler = nullptr;   // 关闭/未装时为 null
    bool                     m_active   = false;
};

// 进程级"当前 Profiler"（与 `Logger` 同款模式：未装即空转，不是错误路径）。
void SetCurrentProfiler(Profiler* profiler) noexcept;
[[nodiscard]] Profiler* CurrentProfiler() noexcept;

} // namespace mini::core

// ------------------------------------------------------------
// MINI_PROFILE_SCOPE(name)
//
// 展开成一个"用行号唯一化"的 `ScopedTimer`。名字**直接传字面量**（Profiler 只存指针）。
// ⚠️ 唯一化靠 `__LINE__` ⇒ **同一行里放两个 scope 会重名**，那是编译错误（响亮，不是静默）。
// ⚠️ 两级拼接是必需的：`foo_##__LINE__` 里的 `__LINE__` **不会被展开**
//    （`##` 的操作数不参与宏展开），必须先过一层普通参数。
// ------------------------------------------------------------
#define MINI_PROFILE_CONCAT_(a, b) a##b
#define MINI_PROFILE_CONCAT(a, b)  MINI_PROFILE_CONCAT_(a, b)

#define MINI_PROFILE_SCOPE(name) \
    const ::mini::core::ScopedTimer MINI_PROFILE_CONCAT(miniProfileScope_, __LINE__)(name)
