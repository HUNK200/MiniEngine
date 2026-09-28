#include "MiniTest.h"

#include <mini/render/Color.h>
#include <mini/render/FrameBuffer.h>
#include <mini/render/RowDiff.h>

#include <cstddef>
#include <string>
#include <string_view>

// ------------------------------------------------------------
// render::RowDiff 的单元测试（S3）。
//
// 不需要终端：RowDiff 只产字符串，把字节写出去是 Console 的事。
// 全量帧与脏行帧的格式、以及"一行只取决于这一行内容"这个前提，
// 都在这里被钉住。
//
// 唯一没法自动测的是"发出去的字节在真实终端上画得对不对"——
// 那要靠 S3_CHECKLIST 10.2 的手动验收（尤其"最后一行会不会滚屏"）。
// ------------------------------------------------------------

namespace {

using mini::render::Color;
using mini::render::FrameBuffer;
using mini::render::RowDiff;

std::size_t CountOccurrences(std::string_view haystack, std::string_view needle)
{
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = haystack.find(needle, pos)) != std::string_view::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

// 取一帧的输出并 **copy 出来**：Compose 的返回值指向 RowDiff 内部缓冲，
// 下一次 Compose 就冲掉了（和 FrameBuffer::Serialize 一个规矩）。
std::string Frame(RowDiff& diff, const FrameBuffer& fb)
{
    return std::string(diff.Compose(fb));
}

} // namespace

// 首帧没有"上一帧"可比 → 全量，且以 ESC[H + 重置 SGR 开头
MINI_TEST(RowDiff, FirstFrameSendsEverything)
{
    FrameBuffer fb(8, 3);
    fb.Clear();
    RowDiff diff;

    const std::string out = Frame(diff, fb);

    MINI_CHECK_EQ(diff.LastStats().rowsTotal, 3);
    MINI_CHECK_EQ(diff.LastStats().rowsSent, 3);
    MINI_CHECK_EQ(diff.LastStats().bytes, out.size());
    MINI_CHECK(out.starts_with("\x1b[H\x1b[39;49m"));   // 归位 + 重置 SGR
}

// 内容一个字节都没变 → **一个字节都不发**。这是脏行 diff 的全部价值所在
MINI_TEST(RowDiff, IdenticalFrameSendsNothing)
{
    FrameBuffer fb(8, 3);
    fb.Clear();
    RowDiff diff;

    (void)Frame(diff, fb);              // 首帧全量
    const std::string out = Frame(diff, fb);

    MINI_CHECK(out.empty());
    MINI_CHECK_EQ(diff.LastStats().rowsSent, 0);
    MINI_CHECK_EQ(diff.LastStats().bytes, std::size_t{0});
}

// 只有被改的那一行会被定位重发；别的行一个定位序列都不该出现
MINI_TEST(RowDiff, OnlyChangedRowIsSent)
{
    FrameBuffer fb(8, 4);
    fb.Clear();
    RowDiff diff;

    (void)Frame(diff, fb);

    fb.Set(0, 2, 'X');                 // 0-based 第 2 行 = 屏幕上第 3 行
    const std::string out = Frame(diff, fb);

    MINI_CHECK_EQ(diff.LastStats().rowsSent, 1);
    MINI_CHECK(out.find("\x1b[3;1H") != std::string::npos);   // 1-based → 3
    MINI_CHECK(out.find('X') != std::string::npos);
    MINI_CHECK(out.find("\x1b[1;1H") == std::string::npos);   // 别的行不该被定位
    MINI_CHECK(out.find("\x1b[4;1H") == std::string::npos);
}

// 行内容变短（"Released" → "-"）时必须发 ESC[0K 擦掉右边残留，
// 否则屏幕上会留着上一帧的尾巴（反向验证里专门有一条删掉它看现象）
MINI_TEST(RowDiff, ShrinkingRowClearsTail)
{
    FrameBuffer fb(12, 2);
    fb.Clear();
    RowDiff diff;

    fb.Draw(0, 0, "Released");
    (void)Frame(diff, fb);

    fb.Clear();                        // 整屏变干净
    fb.Draw(0, 0, "-");
    const std::string out = Frame(diff, fb);

    MINI_CHECK_EQ(diff.LastStats().rowsSent, 1);
    MINI_CHECK(out.find("\x1b[0K") != std::string::npos);
    MINI_CHECK(out.find('e') == std::string::npos);   // 旧字符不再发（擦除交给终端）
}

// 清屏 / 尺寸变化之后必须能强制回到全量
MINI_TEST(RowDiff, InvalidateForcesFullRedraw)
{
    FrameBuffer fb(8, 3);
    fb.Clear();
    RowDiff diff;

    (void)Frame(diff, fb);
    MINI_CHECK(Frame(diff, fb).empty());              // 没变化 → 空

    diff.Invalidate();
    const std::string out = Frame(diff, fb);

    MINI_CHECK_EQ(diff.LastStats().rowsSent, 3);
    MINI_CHECK(out.starts_with("\x1b[H"));
}

// 行数变了（resize）→ 旧快照全部作废，**不能去读越界的旧行**
MINI_TEST(RowDiff, RowCountChangeIsFullRedraw)
{
    FrameBuffer fb(8, 3);
    fb.Clear();
    RowDiff diff;

    (void)Frame(diff, fb);
    MINI_CHECK(Frame(diff, fb).empty());

    fb.Resize(8, 5);                                   // 3 行 → 5 行
    const std::string taller = Frame(diff, fb);
    MINI_CHECK_EQ(diff.LastStats().rowsTotal, 5);
    MINI_CHECK_EQ(diff.LastStats().rowsSent, 5);
    MINI_CHECK(taller.starts_with("\x1b[H"));

    fb.Resize(8, 2);                                   // 5 行 → 2 行（变矮也不能越界）
    const std::string shorter = Frame(diff, fb);
    MINI_CHECK_EQ(diff.LastStats().rowsTotal, 2);
    MINI_CHECK_EQ(diff.LastStats().rowsSent, 2);
    MINI_CHECK(shorter.starts_with("\x1b[H"));
}

// 全量帧的行尾：恰好 rows-1 个 "\r\n"，且**不以它结尾**。
// 不然写满最后一格再换行会让整屏上滚一行，画面永久错位。
MINI_TEST(RowDiff, LastRowHasNoTrailingCrLf)
{
    FrameBuffer fb(6, 3);
    fb.Clear();
    RowDiff diff;

    const std::string out = Frame(diff, fb);

    MINI_CHECK_EQ(CountOccurrences(out, "\r\n"), std::size_t{2});
    MINI_CHECK(!out.ends_with("\r\n"));
}

// 【RowDiff 的前提在这里被钉住】
//   同一行的字节只取决于这一行的内容：起点是"默认色"，且别的行怎么变都不影响它。
// 前提破了的现象是"某行每帧白重发"——不崩、不报错、只是白干，所以必须有这条守卫。
MINI_TEST(RowDiff, SerializeAndSerializeRowAgree)
{
    FrameBuffer fb(4, 2);
    fb.Clear();
    fb.Set(0, 0, 'A', Color::Red);
    fb.Set(1, 0, 'B', Color::Red);     // 与前一格同色 → 不该再发色码
    fb.Set(2, 0, 'C');                 // 回默认色
    fb.Set(0, 1, 'D', Color::Blue);

    // Serialize() 的第 0 行段 == 帧首重置 + SerializeRow(0)
    const std::string full(fb.Serialize());          // 先 copy：view 会被下一次调用冲掉
    const std::size_t nl = full.find("\r\n");
    MINI_CHECK(nl != std::string::npos);

    const std::string row0 = full.substr(0, nl);
    MINI_CHECK_EQ(row0, std::string("\x1b[39;49m") + std::string(fb.SerializeRow(0)));

    // 改**第 1 行**，第 0 行的字节必须一字不变
    const std::string before(fb.SerializeRow(0));
    fb.Set(3, 1, 'Z', Color::Green);
    const std::string after(fb.SerializeRow(0));
    MINI_CHECK_EQ(before, after);

    // 越界给空，不给 UB
    MINI_CHECK(fb.SerializeRow(-1).empty());
    MINI_CHECK(fb.SerializeRow(fb.Height()).empty());
}

// 脏行 diff 的**收益**也要有一条守卫。
//
// 它同时是"前提没被破坏"的总开关：如果哪天 SerializeRow 变得依赖别的内容，
// 每一行的字节每帧都不一样 → 每帧全量 → 这里的 10 倍断言立刻变红。
// 只说"输出正确"是不够的，还得证明"输出更少"—— 否则这个类就白写了。
MINI_TEST(RowDiff, DirtyFrameIsFarSmallerThanFullFrame)
{
    FrameBuffer fb(80, 25);            // 和真实画面一样大
    fb.Clear();
    RowDiff diff;

    const std::string full = Frame(diff, fb);
    MINI_CHECK_EQ(diff.LastStats().rowsSent, 25);

    fb.Set(0, 0, 'X');                 // 顶栏那一行
    fb.Set(40, 12, '@');               // 中间一行（@ 动了）
    const std::string dirty = Frame(diff, fb);

    MINI_CHECK_EQ(diff.LastStats().rowsSent, 2);   // 只发 2 行，不是 25 行
    MINI_CHECK(dirty.size() * 10 < full.size());   // 至少省一个数量级
}
