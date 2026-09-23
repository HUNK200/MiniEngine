#include "MiniTest.h"

#include "mini/core/Version.h"

#include <cstddef>
#include <string>

// ------------------------------------------------------------
// S0 的两个用例。刻意选成"能挡住已知风险"，而不是"能通过就算数"。
// ------------------------------------------------------------

MINI_TEST(version_is_reported)
{
    CHECK(mini::core::VersionString() != nullptr);
    CHECK_EQ(mini::core::VersionMajor(), 0);
    CHECK_EQ(mini::core::VersionMinor(), 1);
}

MINI_TEST(source_is_compiled_as_utf8)
{
    // 源文件里"中"是 UTF-8 的 E4 B8 AD 三个字节。
    // 少了 /utf-8 时，MSVC 会按本机代码页 936(GBK) 解析源码，这个断言立刻挂 ——
    // 也就是说它是风险 E-1 的自动化体检，而不是一句废话断言。
    CHECK_EQ(std::string("中").size(), std::size_t{3});
}

int main()
{
    return minitest::RunAll();
}
