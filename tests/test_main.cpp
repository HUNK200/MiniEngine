#include "MiniTest.h"

#include <cstdio>
#include <cstring>
#include <vector>

// ============================================================
// MiniTest 的 runner 与**唯一的**注册表实例（规格见 doc/MINITEST_SPEC.md 第 3 节）。
// 用例写在 test_smoke.cpp（以后按 suite 拆新文件），这里只放"跑测试的机制"：
// 解析参数 → 遍历注册表 → 逐个执行 → 统计 → 返回失败数。
// ============================================================

namespace minitest {
namespace {

// 注册表必须是"函数内静态局部变量"：跨 TU 的静态初始化顺序不确定，
// 若写成别处的全局 vector，注册对象的构造函数可能比它先跑 → 注册落空（规格 2.2）
std::vector<TestCase>& Registry()
{
    static std::vector<TestCase> registry;
    return registry;
}

int& FailureCount()
{
    static int failures = 0;
    return failures;
}

void PrintUsage(const char* exe)
{
    std::printf("usage: %s [suite] | --list\n", exe);
}

} // namespace

void RegisterTest(const TestCase& testCase)
{
    Registry().push_back(testCase);
}

void ReportFailure(const char* file, int line, const char* expr)
{
    ++FailureCount();
    // 缩进 + 换行：用 --output-on-failure 时失败信息自成一块（规格第 6 节坑 6）
    std::printf("       %s:%d\n       %s\n", file, line, expr);
}

int RunAll(int argc, char** argv)
{
    if (argc > 2) {
        PrintUsage(argv[0]);
        return 1;
    }

    const char* suiteFilter = nullptr;
    bool listOnly = false;
    if (argc == 2) {
        if (std::strcmp(argv[1], "--list") == 0) {
            listOnly = true;
        } else if (argv[1][0] == '-') {
            // 未知开关：立刻失败，绝不能静默跑全部（规格第 3 节）
            std::printf("unknown option: %s\n", argv[1]);
            PrintUsage(argv[0]);
            return 1;
        } else {
            suiteFilter = argv[1];
        }
    }

    const std::vector<TestCase>& all = Registry();

    if (listOnly) {
        for (const TestCase& tc : all) {
            std::printf("%s.%s\n", tc.suite, tc.name);
        }
        std::printf("%zu case(s)\n", all.size());
        // 列出用例不是"跑测试"，永远返回 0（规格第 6 节坑 5）
        return 0;
    }

    std::vector<const TestCase*> selected;
    for (const TestCase& tc : all) {
        if (suiteFilter == nullptr || std::strcmp(tc.suite, suiteFilter) == 0) {
            selected.push_back(&tc);
        }
    }

    // 空注册表 / suite 名拼错都必须报错返回非 0：
    // 否则 CTest 会报 "100% tests passed"，属于假绿（规格第 3 节 + 坑 4）
    if (selected.empty()) {
        if (suiteFilter != nullptr) {
            std::printf("no case in suite '%s' (%zu case(s) registered)\n", suiteFilter,
                        all.size());
        } else {
            std::printf("no case registered\n");
        }
        return 1;
    }

    int failedCases = 0;
    for (const TestCase* tc : selected) {
        const int before = FailureCount();
        tc->fn();
        if (FailureCount() == before) {
            std::printf("[ OK ] %s.%s\n", tc->suite, tc->name);
        } else {
            ++failedCases;
            std::printf("[FAIL] %s.%s\n", tc->suite, tc->name);
        }
    }

    const int passedCases = static_cast<int>(selected.size()) - failedCases;
    std::printf("%d passed, %d failed\n", passedCases, failedCases);

    // 返回值 = 失败用例数（0 = 全过），CTest 只看这个退出码
    return failedCases;
}

} // namespace minitest

int main(int argc, char** argv)
{
    return minitest::RunAll(argc, argv);
}
