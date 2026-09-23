#pragma once

// ============================================================
// MiniTest —— 自制极简断言框架
//
// 为什么不用 GoogleTest（PLAN 风险 E-18）：S0 只需要"能跑一个绿用例"，
// 为此引入 FetchContent + 网络配置属于测试框架膨胀。
// 等到出现"参数化 / fixture / CI 要 JUnit 报告"（约 S10 之后、断言数 > 200）
// 再换 GoogleTest，那时这个文件的存在刚好证明了"我们确实需要它"。
//
// 用法：
//     MINI_TEST(名字) {
//         CHECK(cond);
//         CHECK_EQ(a, b);
//     }
//
//     int main() { return minitest::RunAll(); }
//
// 退出码：全绿 0，有失败 1 —— CTest 靠退出码判定。
// ============================================================

#include <cstdio>
#include <string>
#include <vector>

namespace minitest {

struct TestCase {
    const char* name;
    void (*fn)();
};

inline std::vector<TestCase>& Registry()
{
    static std::vector<TestCase> registry;
    return registry;
}

inline int& Failures()
{
    static int failures = 0;
    return failures;
}

inline int& Checks()
{
    static int checks = 0;
    return checks;
}

// 宏展开成静态对象，构造即注册（自注册模式，不需要手写测试清单）
struct Registrar {
    Registrar(const char* name, void (*fn)()) { Registry().push_back({name, fn}); }
};

inline void Fail(const char* file, int line, const std::string& message)
{
    ++Failures();
    std::printf("    [FAIL] %s:%d\n           %s\n", file, line, message.c_str());
}

inline int RunAll()
{
    int failedCases = 0;
    const std::size_t total = Registry().size();

    std::printf("MiniTest: %zu case(s)\n\n", total);

    for (const TestCase& tc : Registry()) {
        const int before = Failures();
        std::printf("[ RUN  ] %s\n", tc.name);
        tc.fn();
        if (Failures() == before) {
            std::printf("[  OK  ] %s\n", tc.name);
        } else {
            ++failedCases;
            std::printf("[ FAIL ] %s\n", tc.name);
        }
    }

    std::printf("\n%zu case(s), %d check(s), %d failure(s)\n",
                total, Checks(), Failures());

    return failedCases == 0 ? 0 : 1;
}

} // namespace minitest

#define MINI_TEST(testName)                                                 \
    static void testName();                                                 \
    static ::minitest::Registrar mini_registrar_##testName(#testName,       \
                                                           &testName);      \
    static void testName()

#define CHECK(cond)                                                        \
    do {                                                                   \
        ++::minitest::Checks();                                            \
        if (!(cond)) {                                                     \
            ::minitest::Fail(__FILE__, __LINE__, "CHECK failed: " #cond);  \
        }                                                                  \
    } while (false)

#define CHECK_EQ(lhs, rhs)                                                    \
    do {                                                                      \
        ++::minitest::Checks();                                               \
        if (!((lhs) == (rhs))) {                                              \
            ::minitest::Fail(__FILE__, __LINE__,                              \
                             "CHECK_EQ failed: " #lhs " == " #rhs);           \
        }                                                                     \
    } while (false)
