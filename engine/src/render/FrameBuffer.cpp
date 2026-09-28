#include "mini/render/FrameBuffer.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

// ============================================================
// FrameBuffer 实现（S1）
//
// 这里**不出现任何终端 API**：它只把像素拼成字符串，怎么送到终端是
// platform::Console 的事。所以本文件可以被单元测试完整覆盖（不需要终端）。
// ============================================================

namespace mini::render {
namespace {

// 每格最坏输出量：1 个字符 + 前景色码最长 11 字节 + 背景色码最长 11 字节。
// 取 20 留点余量，避免 Serialize 过程中反复扩容（那是最容易吃到性能的地方）
constexpr std::size_t kBytesPerCellGuess = 20;

void ValidateSize(int w, int h)
{
    if (w <= 0 || h <= 0) {
        throw std::invalid_argument("FrameBuffer: width and height must be positive");
    }
}

// 十进制输出，不经过 std::to_string（避免每次换色都分配一次内存）
void AppendUint(std::string& out, unsigned value)
{
    char digits[4];   // 0..255 最多 3 位，留一个给 '\0' 的位置感
    int  n = 0;
    do {
        digits[n++] = static_cast<char>('0' + static_cast<int>(value % 10));
        value /= 10;
    } while (value != 0);

    while (n > 0) {
        out.push_back(digits[--n]);
    }
}

void AppendFg(std::string& out, Color c)
{
    if (IsDefault(c)) {
        out += "\x1b[39m";           // 默认前景
        return;
    }
    out += "\x1b[38;5;";
    AppendUint(out, ToIndex(c));
    out += 'm';
}

void AppendBg(std::string& out, Color c)
{
    if (IsDefault(c)) {
        out += "\x1b[49m";           // 默认背景
        return;
    }
    out += "\x1b[48;5;";
    AppendUint(out, ToIndex(c));
    out += 'm';
}

} // namespace

FrameBuffer::FrameBuffer(int w, int h)
{
    ValidateSize(w, h);
    Reallocate(w, h);
}

void FrameBuffer::Resize(int w, int h)
{
    ValidateSize(w, h);
    Reallocate(w, h);
}

void FrameBuffer::Reallocate(int w, int h)
{
    m_w = w;
    m_h = h;
    m_cells.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), Cell{});

    // 缓冲区大小跟着画面走，且不保留旧内容 —— 序列化出来的东西每次都要完整重写
    m_scratch.clear();
    m_scratch.reserve(m_cells.size() * kBytesPerCellGuess + 16);
}

void FrameBuffer::Clear(Cell fill)
{
    std::fill(m_cells.begin(), m_cells.end(), fill);
}

void FrameBuffer::Set(int x, int y, char c, Color fg, Color bg)
{
    if (!InBounds(x, y)) {
        return;   // 越界静默忽略：调用方不需要到处写边界判断
    }
    m_cells[Index(x, y)] = Cell{c, fg, bg};
}

void FrameBuffer::Draw(int x, int y, std::string_view text)
{
    for (std::size_t i = 0; i < text.size(); ++i) {
        Set(x + static_cast<int>(i), y, text[i]);
    }
}

void FrameBuffer::DrawRect(int x, int y, int w, int h, char c)
{
    if (w <= 0 || h <= 0) {
        return;
    }

    // 注意：四条边用的坐标是**未夹取**的 x0/x1/y0/y1，
    // 只把"遍历范围"夹到画布内。这样落在画布外的边直接不出现，
    // 不会因为夹取而折回来画到另一侧（那是很隐蔽的错）。
    const int x0 = x;
    const int y0 = y;
    const int x1 = x + w - 1;
    const int y1 = y + h - 1;

    const int cx0 = (x0 < 0) ? 0 : x0;
    const int cy0 = (y0 < 0) ? 0 : y0;
    const int cx1 = (x1 > m_w - 1) ? m_w - 1 : x1;
    const int cy1 = (y1 > m_h - 1) ? m_h - 1 : y1;

    if (cx0 > cx1 || cy0 > cy1) {
        return;   // 整个矩形都在画布外
    }

    for (int i = cx0; i <= cx1; ++i) {
        Set(i, y0, c);   // 上边
        Set(i, y1, c);   // 下边
    }
    for (int i = cy0; i <= cy1; ++i) {
        Set(x0, i, c);   // 左边
        Set(x1, i, c);   // 右边
    }
}

bool FrameBuffer::InBounds(int x, int y) const
{
    return x >= 0 && y >= 0 && x < m_w && y < m_h;
}

Cell FrameBuffer::At(int x, int y) const
{
    if (!InBounds(x, y)) {
        return Cell{};   // 越界读：给空白，不给 UB、也不给随机内存
    }
    return m_cells[Index(x, y)];
}

std::string_view FrameBuffer::Serialize() const
{
    m_scratch.clear();

    // 帧首重置 SGR：ESC[H 只移光标、**不重置颜色**。
    // 不重置的话，新一帧里第一个"默认色"的格子会继承上一帧末尾的颜色 → 随机花格
    m_scratch += "\x1b[39;49m";

    Color curFg = Color::Default;
    Color curBg = Color::Default;

    for (int y = 0; y < m_h; ++y) {
        const Cell* row = m_cells.data() + static_cast<std::size_t>(y) * static_cast<std::size_t>(m_w);
        for (int x = 0; x < m_w; ++x) {
            const Cell& cell = row[x];
            if (cell.fg != curFg) {
                AppendFg(m_scratch, cell.fg);
                curFg = cell.fg;
            }
            if (cell.bg != curBg) {
                AppendBg(m_scratch, cell.bg);
                curBg = cell.bg;
            }
            m_scratch.push_back(cell.ch);
        }

        // 行尾用 "\r\n" 回第 1 列（我们顺序写，不逐行发 ESC[{y};1H）。
        // 最后一行不写：写满最后一格再换行会让整屏上滚一行，画面永久错位
        if (y + 1 < m_h) {
            m_scratch += "\r\n";
        }
    }

    return m_scratch;
}

std::size_t FrameBuffer::Index(int x, int y) const
{
    return static_cast<std::size_t>(y) * static_cast<std::size_t>(m_w) + static_cast<std::size_t>(x);
}

} // namespace mini::render
