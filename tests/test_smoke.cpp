#include "MiniTest.h"   // 同目录，用引号

#include <mini/core/Application.h>   // 画布尺寸谓词（TECH_DEBT C7 收窄后新增）
#include <mini/core/Version.h>       // 引擎头走 PUBLIC include，用尖括号

#include <cstddef>
#include <cstring>   // std::strcmp（C18：版本字符串要比对内容，不能只判非空）
#include <string>

// ------------------------------------------------------------
// S0 的用例都在这里。刻意选成"能挡住已知风险"，而不是"能通过就算数"。
// suite 名是 runner 的过滤单位：mini_tests.exe Core 只跑 Core 组。
// ------------------------------------------------------------

MINI_TEST(Core, VersionIsReported)
{
    // ⚠️ 2026-10-10（C3 改版本号 + C18 当场修）：
    //
    // 原来第一句是 `MINI_CHECK(VersionString() != nullptr)` —— 那是**恒真**的
    //（字符串字面量不可能是空指针），所以它**根本没在守版本字符串**：
    // 把返回值改成 "banana" 也照样全绿（这是 S8 学到的"**空用例比没有更坏**"的微型样本）。
    // 现在改成**比对内容**，于是它真的在守两件事：
    //   1. `VersionString()` 的取值（改错就红）；
    //   2. "字符串"与"数字 API"**不许各说各话**（Major/Minor 改了、字符串忘了改 → 红）。
    // ⚠️ 代价是**故意的**：以后改版本号要动**四处**（字符串 + 两个函数 + 这里），
    //    逼着四个落点一起走 —— 同 `kDumpKey` 那种"一个字母两个落点"的纪律。
    //    ✅ **2026-10-13 这次 bump**（`0.2.0-dev` → `0.3.0-dev`，依据 `PLAN` G 节的 v0.3 达成）
    //    **四处一起改了**；这条用例就是它的机器守卫：只改字符串不改数字（或反过来）当场红。
    // ⚠️ 这里的逗号在 `strcmp(...)` 的括号内，`MINI_CHECK` 是变参宏，不会被拆成两个参数。
    MINI_CHECK(std::strcmp(mini::core::VersionString(), "0.3.0-dev") == 0);
    MINI_CHECK_EQ(mini::core::VersionMajor(), 0);
    MINI_CHECK_EQ(mini::core::VersionMinor(), 3);
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
