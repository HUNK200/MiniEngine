#include "mini/core/Profiler.h"

#include <cstddef>   // std::size_t（环形缓冲的下标：别依赖别的头顺带包含它）
#include <cstring>   // std::strcmp（段名合并用，见 AddSample 的第一段注释）

// ============================================================
// core::Profiler 实现（S11）
//
// 数据流（一帧）：
//     BeginFrame()        清本帧累加器（名字表不动）
//       AddSample(...)    × N  —— 生产路径由 ScopedTimer 的析构调用
//     EndFrame()          本帧各段的 sum 推一格进环形缓冲
//
// ⚠️ 两个"不分配、不长、不抛"的原则贯穿全程：
//   · 段的**数量**有上界（`kMaxScopes`）：超了丢 + 计数，绝不扩容（§8 坑 2）；
//   · 段的**名字**只存指针、不拷贝字符串：契约是"传字面量"（§8 坑 1）。
// 于是整个 Profiler 的内存是**构造时就定死**的 4 KB 多一点，跑多久都不变。
// ============================================================

namespace mini::core {
namespace {

Profiler* g_current = nullptr;   // 与 Logger 同款：弱引用，不参与所有权

} // namespace

void Profiler::SetEnabled(bool on) noexcept
{
    m_enabled = on;
}

bool Profiler::Enabled() const noexcept
{
    return m_enabled;
}

void Profiler::BeginFrame() noexcept
{
    // 关闭时"三个入口全是 no-op"（本函数 / AddSample / EndFrame）——
    // 这样历史数据会**原样冻结**（overlay 还显示上一次的值，而不是一夜清零），
    // 重新打开之后从下一帧接着累加。用例 `DisabledIsNoOp` 钉的就是这条。
    if (!m_enabled) {
        return;
    }
    for (Acc& acc : m_acc) {
        acc = Acc{};
    }
    // ⚠️ 清的是**数值**，不是名字：名字表跨帧稳定 ⇒ 表格列序不会串。
}

void Profiler::AddSample(const char* name, float ms) noexcept
{
    if (!m_enabled || name == nullptr) {
        return;
    }

    // 找段：**先比指针，再比内容**。
    //   · 指针比较是快路径：同一个字面量在同一个 TU 里地址相同（正常情形，命中）；
    //   · `strcmp` 兜的是"同一段文字在两个 .cpp 里各写了一遍" —— 那种情况两个字面量
    //     地址不同，只比指针会得到**两行同名数据**（数字不错、但表会看着莫名其妙）。
    // 存下来的仍然是**第一个**被登记的指针：字面量是静态存储 ⇒ 永远不会悬空。
    int index = -1;
    for (int i = 0; i < m_nameCount; ++i) {
        if (m_names[i] == name || std::strcmp(m_names[i], name) == 0) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        if (m_nameCount >= kMaxScopes) {
            ++m_dropped;   // 丢 + 计数：**不崩、不截断、不扩容**（§8 坑 2）
            return;
        }
        index              = m_nameCount;
        m_names[index]     = name;
        m_acc[index]       = Acc{};
        ++m_nameCount;
    }

    Acc& acc = m_acc[index];
    acc.sum += ms;
    if (ms > acc.max) {
        acc.max = ms;   // 本帧峰值（不是"历史峰值" —— 历史由 Snapshot 从窗口里算）
    }
}

void Profiler::EndFrame() noexcept
{
    if (!m_enabled) {
        return;
    }

    // 配平自检：理论上 `ScopedTimer` 是 RAII，正常路径**不可能**不平衡；
    // 能走到这里说明有人手动 `BeginScope()` 之后没配对。
    // ⚠️ 发现之后必须**把深度归零**：否则一次误用会让之后每一帧都被计一次，
    //    报警数从此只增不减（那就变成噪声了）。
    if (m_depth != 0) {
        ++m_unbalanced;
        m_depth = 0;
    }

    std::array<float, kMaxScopes>& slot = m_hist[static_cast<std::size_t>(m_histHead)];
    for (int i = 0; i < kMaxScopes; ++i) {
        // 没登记过的槽位写 0（不是垃圾值）：Snapshot 会按"窗口帧数"取平均，
        // 0 正好表示"这一段这一帧没跑"。
        slot[static_cast<std::size_t>(i)] = (i < m_nameCount) ? m_acc[i].sum : 0.0f;
    }

    m_histHead = (m_histHead + 1) % kHistoryFrames;
    if (m_histCount < kHistoryFrames) {
        ++m_histCount;
    }
}

void Profiler::BeginScope() noexcept
{
    ++m_depth;
}

void Profiler::EndScope() noexcept
{
    if (m_depth > 0) {
        --m_depth;
    } else {
        ++m_unbalanced;   // 多调一次 EndScope 也是配平错 —— 不静默吞掉
    }
}

int Profiler::Snapshot(Row* out, int cap) const noexcept
{
    if (out == nullptr || cap <= 0) {
        return 0;   // 调用方给的数组由调用方负责；传错就"没数据"，不给 UB
    }

    const int rows = (m_nameCount < cap) ? m_nameCount : cap;
    if (rows == 0) {
        return 0;
    }

    // 写满之后，"最老的那一帧"就是 `m_histHead`（下一个要被覆盖的位置）；
    // 没写满时最老的永远是 0。
    const int oldest = (m_histCount < kHistoryFrames) ? 0 : m_histHead;
    // ⚠️ 分母是**窗口内的帧数**（`m_histCount`），不是 `kHistoryFrames`：
    //    首屏历史不满 120 时若拿 120 去除，数字会偏小一半还多（§8 坑 8）。
    const int frames = m_histCount;

    for (int i = 0; i < rows; ++i) {
        float sum = 0.0f;
        float max = 0.0f;
        for (int f = 0; f < frames; ++f) {
            const int  slot  = (oldest + f) % kHistoryFrames;
            const float value = m_hist[static_cast<std::size_t>(slot)][static_cast<std::size_t>(i)];
            sum += value;
            if (value > max) {
                max = value;
            }
        }
        out[i] = Row{m_names[i], (frames > 0) ? (sum / static_cast<float>(frames)) : 0.0f, max};
    }
    return rows;
}

int Profiler::HistoryFrames() const noexcept
{
    return m_histCount;
}

int Profiler::Dropped() const noexcept
{
    return m_dropped;
}

int Profiler::Unbalanced() const noexcept
{
    return m_unbalanced;
}

// ------------------------------------------------------------
// ScopedTimer
// ------------------------------------------------------------
ScopedTimer::ScopedTimer(const char* name) noexcept
    : m_name(name)
{
    // ⚠️ 顺序有讲究：**先判断要不要计时，再取时钟**（§8 坑 5）。
    //    关闭时连 `now()` 都不调 —— 不然"关闭时近似零开销"就是一句空话，
    //    而 R4（关掉 `Enabled()` 短路）正是照着这一行设计的反向验证靶子。
    Profiler* profiler = CurrentProfiler();
    if (profiler != nullptr && profiler->Enabled()) {
        m_profiler = profiler;
        m_active   = true;
        m_profiler->BeginScope();
        m_start = ProfileClock::now();
    }
}

ScopedTimer::~ScopedTimer()
{
    if (!m_active) {
        return;
    }
    const float ms = std::chrono::duration<float, std::milli>(ProfileClock::now() - m_start).count();
    m_profiler->AddSample(m_name, ms);
    m_profiler->EndScope();
}

// ------------------------------------------------------------
// current 指针（与 Logger 同款：未装即空转）
// ------------------------------------------------------------
void SetCurrentProfiler(Profiler* profiler) noexcept
{
    g_current = profiler;
}

Profiler* CurrentProfiler() noexcept
{
    return g_current;
}

} // namespace mini::core
