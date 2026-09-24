#include "MiniTest.h"

#include <mini/render/Cell.h>
#include <mini/render/FrameBuffer.h>

#include <cstddef>
#include <string>
#include <string_view>

// ------------------------------------------------------------
// FrameBuffer 的单元测试。
//
// 这个文件**不需要终端**：FrameBuffer 只产字符串、不碰 IO，
// 所以 S1 里真正能被自动验证的就是这一层。
// ------------------------------------------------------------

namespace {

using mini::render::Cell;
using mini::render::Color;
using mini::render::FrameBuffer;

// 用 int 比较字符：失败信息里能看到 'A'(65) 这种可读的数字，而不是一个空格
int ChAt(const FrameBuffer& fb, int x, int y)
{
    return static_cast<int>(fb.At(x, y).ch);
}

int Ch(char c)
{
    return static_cast<int>(c);
}

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

} // namespace

// S1 决策 2「尽量单字节」的运行时镜像（编译期那份是 Cell.h 里的 static_assert）
MINI_TEST(FrameBuffer, CellIsThreeBytes)
{
    MINI_CHECK_EQ(sizeof(Cell), std::size_t{3});
    MINI_CHECK_EQ(alignof(Cell), std::size_t{1});
    MINI_CHECK_EQ(sizeof(Color), std::size_t{1});
}

// 越界写必须静默忽略：S5 之后实体天天走出屏幕边缘，靠的就是这条
MINI_TEST(FrameBuffer, SetOutOfBoundsIsIgnored)
{
    FrameBuffer fb(3, 2);
    fb.Clear();

    fb.Set(-1, 0, 'X');
    fb.Set(3, 0, 'X');       // 正好越右边界
    fb.Set(0, -1, 'X');
    fb.Set(0, 2, 'X');       // 正好越下边界
    fb.Set(9999, 9999, 'X');

    for (int y = 0; y < fb.Height(); ++y) {
        for (int x = 0; x < fb.Width(); ++x) {
            MINI_CHECK_EQ(ChAt(fb, x, y), Ch(' '));
        }
    }
}

// index = y*m_w + x —— 写反了画面会转置，用非方阵才测得出来
MINI_TEST(FrameBuffer, IndexIsLinearized)
{
    FrameBuffer fb(4, 3);   // 宽 != 高：转置 bug 无处遁形
    fb.Clear();
    fb.Set(1, 2, 'A');
    fb.Set(3, 0, 'B');

    MINI_CHECK_EQ(ChAt(fb, 1, 2), Ch('A'));
    MINI_CHECK_EQ(ChAt(fb, 3, 0), Ch('B'));

    int painted = 0;
    for (int y = 0; y < fb.Height(); ++y) {
        for (int x = 0; x < fb.Width(); ++x) {
            if (ChAt(fb, x, y) != Ch(' ')) {
                ++painted;
            }
        }
    }
    MINI_CHECK_EQ(painted, 2);
}

// DrawRect 只画边 + 四条边各自按自己的坐标裁剪（不会折回来画到另一侧）
MINI_TEST(FrameBuffer, DrawRectClipsAtEdges)
{
    // 左上角越界：矩形是 x=-2..1, y=-1..1
    // 可见边：下边(y=1, x=0..1)、右边(x=1, y=0..1)；上边与左边整条在画布外
    FrameBuffer fb(5, 4);
    fb.DrawRect(-2, -1, 4, 3, '*');
    MINI_CHECK_EQ(ChAt(fb, 0, 0), Ch(' '));   // 上边/左边被裁掉，内部不画
    MINI_CHECK_EQ(ChAt(fb, 1, 0), Ch('*'));   // 右边
    MINI_CHECK_EQ(ChAt(fb, 0, 1), Ch('*'));   // 下边
    MINI_CHECK_EQ(ChAt(fb, 1, 1), Ch('*'));   // 右下角
    MINI_CHECK_EQ(ChAt(fb, 2, 1), Ch(' '));   // 下边只到 x=1

    // 完全在画布外：一个格子都不许动
    FrameBuffer far(5, 4);
    far.DrawRect(10, 10, 3, 3, '*');
    far.DrawRect(-10, -10, 3, 3, '*');
    MINI_CHECK_EQ(ChAt(far, 0, 0), Ch(' '));
    MINI_CHECK_EQ(ChAt(far, 4, 3), Ch(' '));

    // 右下角越界：x=3..12, y=2..11 → 只留上边(y=2, x=3..4)与左边(x=3, y=2..3)
    FrameBuffer near(5, 4);
    near.DrawRect(3, 2, 10, 10, '*');
    MINI_CHECK_EQ(ChAt(near, 3, 2), Ch('*'));
    MINI_CHECK_EQ(ChAt(near, 4, 2), Ch('*'));   // 上边被裁到画布右边界
    MINI_CHECK_EQ(ChAt(near, 3, 3), Ch('*'));   // 左边
    MINI_CHECK_EQ(ChAt(near, 4, 3), Ch(' '));   // 右下角在画布外，不补

    // 退化尺寸不能崩、也不能画成一片
    FrameBuffer tiny(3, 3);
    tiny.DrawRect(1, 1, 1, 1, '*');
    MINI_CHECK_EQ(ChAt(tiny, 1, 1), Ch('*'));
    tiny.DrawRect(0, 0, 0, 0, '*');   // 空矩形：直接返回
    tiny.DrawRect(0, 0, -3, -3, '*');
    MINI_CHECK_EQ(ChAt(tiny, 0, 0), Ch(' '));
}

// 越界读给空白 Cell，不给 UB、也不给随机内存
MINI_TEST(FrameBuffer, AtOutOfBoundsReturnsEmpty)
{
    FrameBuffer fb(2, 2);
    fb.Clear(Cell{'Z', Color::Red, Color::Blue});

    MINI_CHECK_EQ(ChAt(fb, 0, 0), Ch('Z'));
    MINI_CHECK(fb.At(1, 1).fg == Color::Red);

    const Cell oob = fb.At(-1, 0);
    MINI_CHECK_EQ(static_cast<int>(oob.ch), Ch(' '));
    MINI_CHECK(oob.fg == Color::Default);
    MINI_CHECK(oob.bg == Color::Default);
    MINI_CHECK(fb.At(2, 2).fg == Color::Default);

    MINI_CHECK(!fb.InBounds(-1, 0));
    MINI_CHECK(!fb.InBounds(0, 2));
    MINI_CHECK(fb.InBounds(1, 1));
}

// Serialize 是"防闪烁"里唯一可自动测的部分：颜色只在变化时输出、
// 行尾用 \r\n、最后一行不换行（换了整屏会上滚一行）
MINI_TEST(FrameBuffer, SerializeEmitsColorOnlyOnChange)
{
    FrameBuffer fb(4, 2);
    fb.Clear();
    fb.Set(0, 0, 'A', Color::Red);
    fb.Set(1, 0, 'B', Color::Red);       // 与前一格同色 → 不该再发色码
    fb.Set(2, 0, 'C');                   // 回默认色
    fb.Set(3, 1, 'D', Color::Blue);

    const std::string s(fb.Serialize());

    MINI_CHECK(s.starts_with("\x1b[39;49m"));   // 帧首必须重置 SGR（否则继承上一帧颜色）
    MINI_CHECK_EQ(CountOccurrences(s, "\x1b[38;5;1m"), std::size_t{1});   // Red 只发一次
    MINI_CHECK_EQ(CountOccurrences(s, "\x1b[38;5;4m"), std::size_t{1});   // Blue 只发一次
    MINI_CHECK_EQ(CountOccurrences(s, "\r\n"), std::size_t{1});           // 2 行 → 1 个行尾
    MINI_CHECK(!s.ends_with("\r\n"));

    // 像素顺序：同色的 A、B 必须紧挨着（中间只有字符、没有色码）；
    // 而 C 回默认色，所以它前面必然恰好跟一个 ESC[39m。
    // 这两条合起来证明"颜色只在变化时输出"且"字符顺序没乱"。
    // 注意别写成 find("ABC") —— A/B 与 C 之间必然夹着色码，那种断言是错的
    MINI_CHECK(s.find("AB") != std::string::npos);
    MINI_CHECK(s.find("\x1b[39mC") != std::string::npos);

    // 再来一次：结果必须逐字节相同（幂等 —— 每帧内容一致时输出也一致）
    const std::string again(fb.Serialize());
    MINI_CHECK(again == s);
}
