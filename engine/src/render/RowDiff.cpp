#include "mini/render/RowDiff.h"

#include "mini/render/FrameBuffer.h"

#include <string>

// ============================================================
// render::RowDiff 实现（S3）
//
// 全量帧与脏行帧的输出格式都写在这里，是"防闪烁"那条线的落点：
//   - 一帧只产出**一个**字符串，调用方一次 write 出去（PLAN C.5 第 1 条）
//   - 全量帧用 ESC[H 覆写，**不清屏**（ESC[2J 是闪烁的来源，只允许尺寸变化时发一次）
//   - 脏行帧用 ESC[{y};1H 绝对定位，每行前后各补一个序列（见下）
// ============================================================

namespace mini::render {
namespace {

constexpr char kCursorHome[] = "\x1b[H";        // 光标归位（不清屏、不重置 SGR）

// 重置 SGR：**脏行必须自己带**。
// 绝对定位到第 y 行之后，终端的 SGR 还是上一次写的残留（可能是任何颜色），
// 而 SerializeRow() 假定"行首是默认色"，不重置的话这一行的第一个格子颜色是错的。
constexpr char kResetSgr[] = "\x1b[39;49m";

// 擦到行尾：行内容变短时（比如 "Released" → "-"）右边会留着上一帧的垃圾。
// 反向验证里专门有一条"删掉它就留垃圾"。
constexpr char kEraseToEol[] = "\x1b[0K";

// 终端的行列是 1-based，FrameBuffer 是 0-based —— 这里就差这一个 +1。
// 用 std::to_string 而不是自己写 AppendUint：一帧最多一两行脏行，
// 这个开销可以忽略（FrameBuffer 那边是**每格**都可能拼数字，所以那边才要手写）。
void AppendRowPosition(std::string& out, int y)
{
    out += "\x1b[";
    out += std::to_string(y + 1);
    out += ";1H";
}

} // namespace

// 原先这里有个 `RowDiff::Resize(int)`，2026-09-28 删掉了（全仓库 0 处调用）。
// 它的三行逻辑（行数变了 → assign + m_dirtyAll）已经在 Compose() 里就地做了，
// 留着等于把同一件事写两遍，还得靠调用方记得调。

void RowDiff::Invalidate() noexcept
{
    m_dirtyAll = true;
}

std::string_view RowDiff::Compose(const FrameBuffer& fb)
{
    const int rows = fb.Height();

    // 行数不对（换过画布尺寸 / 第一次用）→ 快照全作废，本帧全量。
    // **这是唯一的入口**：RowDiff 不为"告诉它行数"提供任何接口，
    // 所以不存在"忘了告诉它"这种错法（原先有个 Resize()，2026-09-28 删了）。
    if (static_cast<int>(m_prevRows.size()) != rows) {
        m_prevRows.assign(static_cast<std::size_t>(rows), std::string{});
        m_dirtyAll = true;
    }

    m_scratch.clear();
    int sent = 0;

    if (m_dirtyAll) {
        m_scratch += kCursorHome;
        m_scratch += kResetSgr;

        for (int y = 0; y < rows; ++y) {
            const std::string_view row = fb.SerializeRow(y);
            m_scratch.append(row);
            m_prevRows[static_cast<std::size_t>(y)].assign(row);

            // 最后一行**不写** "\r\n"：写满最后一格再换行会让整屏上滚一行，
            // 画面从此永久错位（S1 踩过，这里保持一样）
            if (y + 1 < rows) {
                m_scratch += "\r\n";
            }
        }

        sent = rows;
        m_dirtyAll = false;
    } else {
        for (int y = 0; y < rows; ++y) {
            // ⚠️ row 指向 fb 的缓冲，下一次 SerializeRow() 就失效 ——
            //    所以下面的比较与拷贝都必须在这一轮里做完
            const std::string_view row = fb.SerializeRow(y);
            std::string&           prev = m_prevRows[static_cast<std::size_t>(y)];

            if (std::string_view(prev) == row) {
                continue;   // 内容没变：一个字节都不发
            }

            AppendRowPosition(m_scratch, y);
            m_scratch += kResetSgr;
            m_scratch.append(row);
            m_scratch += kEraseToEol;

            prev.assign(row);
            ++sent;
        }
    }

    m_stats = Stats{ rows, sent, m_scratch.size() };
    return m_scratch;
}

} // namespace mini::render
