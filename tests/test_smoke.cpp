#include "MiniTest.h"   // 同目录，用引号

#include <mini/core/Version.h>   // 引擎头走 PUBLIC include，用尖括号

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
