#include "MiniTest.h"   // 同目录，用引号

#include <mini/core/Application.h>   // 画布尺寸谓词（TECH_DEBT C7 收窄后新增）
#include <mini/core/Version.h>       // 引擎头走 PUBLIC include，用尖括号

#include <cstddef>
#include <string>

// ------------------------------------------------------------
// S0 的用例都在这里。刻意选成"能挡住已知风险"，而不是"能通过就算数"。
// suite 名是 runner 的过滤单位：mini_tests.exe Core 只跑 Core 组。
// ------------------------------------------------------------

MINI_TEST(Core, VersionIsReported)
{
    MINI_CHECK(mini::core::VersionString() != nullptr);
    MINI_CHECK_EQ(mini::core::VersionMajor(), 0);
    MINI_CHECK_EQ(mini::core::VersionMinor(), 1);
}

// 画布尺寸的接受条件（TECH_DEBT C7 收窄：这段逻辑以前埋在 Application.cpp 的匿名
// namespace 里，测试碰不到）。它决定 `FrameContext` 的 `screenWidth/Height`，
// **S6 起相机每帧都读它** —— 所以边界值值得用机器钉住，而不是只靠"拖到极窄不崩"。
MINI_TEST(Core, CanvasSizeAcceptanceIsBoundarySafe)
{
    using mini::core::IsUsableCanvasSide;
    using mini::core::IsUsableCanvasSize;

    // 正常窗口
    MINI_CHECK(IsUsableCanvasSize(80, 25));

    // **边界值**：`>= 2` 与 `> 2` 只差这个数 —— 写成 `> 2` 时 2 列的画布会被拒，
    // 而 FrameBuffer 其实接受 2（它只拒绝非正尺寸）。
    MINI_CHECK(IsUsableCanvasSize(2, 2));
    MINI_CHECK_EQ(mini::core::kMinUsableCanvasSide, 2);

    // **两个方向都要查**：把 w/h 搞反、或只查其中一轴的实现会被这几条抓住
    MINI_CHECK(!IsUsableCanvasSize(1, 25));
    MINI_CHECK(!IsUsableCanvasSize(80, 1));
    MINI_CHECK(!IsUsableCanvasSize(0, 0));
    MINI_CHECK(!IsUsableCanvasSize(-3, 30));

    // 单轴谓词服务的是**启动**那条路径（逐轴替换，不是整体判断）：
    // (100, 1) 只换高、宽保持 100 —— 见 Application.h 里那段说明
    MINI_CHECK(IsUsableCanvasSide(100));
    MINI_CHECK(!IsUsableCanvasSide(1));
    MINI_CHECK(!IsUsableCanvasSide(0));
}

MINI_TEST(Build, SourceIsCompiledAsUtf8)
{
    // 源文件里"中"是 UTF-8 的 E4 B8 AD 三个字节。
    // 少了 /utf-8 时，MSVC 会按本机代码页 936(GBK) 解析源码，这个断言立刻挂 ——
    // 也就是说它是风险 E-1 的自动化体检，而不是一句废话断言。
    MINI_CHECK_EQ(std::string("中").size(), std::size_t{3});
}

MINI_TEST(TestHarness, FloatAssertionWorks)
{
    // 不是为了"多一条绿"，而是为了在 S4 写 Math 测试之前，
    // 先证明 MINI_CHECK_NEAR 这条路本身是通的。
    MINI_CHECK_NEAR(0.1 + 0.2, 0.3, 1e-9);
    MINI_CHECK_NEAR(1.0f / 3.0f * 3.0f, 1.0f, 1e-6);
}
