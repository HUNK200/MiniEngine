#include "mini/core/Version.h"

namespace mini::core {

const char* VersionString()
{
    // 中文注释不是装饰：它和 tests/test_main.cpp 里的 UTF-8 断言一起，
    // 用来确认 /utf-8 生效（风险 E-1，本机代码页是 936）。
    return "0.1.0-dev";
}

int VersionMajor() { return 0; }

int VersionMinor() { return 1; }

} // namespace mini::core
