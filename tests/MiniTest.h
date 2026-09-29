#pragma once

// ============================================================
// MiniTest —— 自制极简断言框架（接口按 doc/MINITEST_SPEC.md 实现）
//
// 为什么不用 GoogleTest（PLAN 风险 E-18）：S0 只需要"能跑一个绿用例"，
// 为此引入 FetchContent + 网络配置属于测试框架膨胀。
// 等到出现"参数化 / fixture / CI 要 JUnit 报告"（约 S10 之后、断言数 > 200）
// 再换 GoogleTest，那时这个文件的存在刚好证明了"我们确实需要它"。
//
// 用法：
//     MINI_TEST(Core, VersionIsReported) {
//         MINI_CHECK(cond);
//         MINI_CHECK_EQ(a, b);
//     }
//
//     int main(int argc, char** argv) { return minitest::RunAll(argc, argv); }
//
// 分工（MINITEST_SPEC 第 1、2 节）：
//   - 本头文件只有**宏** + 两个函数**声明**（RegisterTest / ReportFailure）
//   - 注册表实例与 runner 都定义在 tests/test_main.cpp，**绝不能放进头里**：
//     头被 N 个 .cpp 包含就会得到 N 份注册表，现象是"用例数不对"，而且不报错
// ============================================================

#include <sstream>
#include <string>

namespace minitest {

struct TestCase {
    const char* suite;   // 分组名，runner 用它做过滤
    const char* name;    // 用例名
    void (*fn)();        // 用例体
};

// 注册表入口：定义在 test_main.cpp
void RegisterTest(const TestCase& testCase);

// 断言失败时调用：定义在 test_main.cpp（文件/行号由宏里的 __FILE__ / __LINE__ 传进来）
void ReportFailure(const char* file, int line, const char* expr);

// 断言失败信息要"打印两侧的值"，而 ReportFailure 的签名里没有值，
// 所以消息在宏里就拼好，由这个 header-only 的 inline 模板负责转字符串。
// 它无状态、不碰注册表，放头里不会产生 ODR 问题。
template <typename T>
inline std::string ToString(const T& value)
{
    std::ostringstream out;
    out << value;
    return out.str();
}

namespace detail {

// 宏展开出的静态对象，构造即注册（自注册模式，不需要手写测试清单）
struct Registrar {
    Registrar(const char* suite, const char* name, void (*fn)())
    {
        RegisterTest(TestCase{suite, name, fn});
    }
};

} // namespace detail
} // namespace minitest

// ------------------------------------------------------------
// MINI_TEST(suite, name)
//
// 展开成三件事：函数声明 + 静态注册对象 + 函数定义。
// 标识符里必须拼上 suite / name / __LINE__：同文件里多个用例否则会重复定义
// （MINITEST_SPEC 第 2 节第 1 条）。
// ------------------------------------------------------------
#define MINI_TEST(suite, name)                                                          \
    static void miniTest_##suite##_##name##_##__LINE__();                               \
    static const ::minitest::detail::Registrar                                          \
        miniRegistrar_##suite##_##name##_##__LINE__(                                    \
            #suite, #name, &miniTest_##suite##_##name##_##__LINE__);                    \
    static void miniTest_##suite##_##name##_##__LINE__()

// ------------------------------------------------------------
// 断言：全部是"失败不中断"语义（不做 ASSERT 版，理由见 MINITEST_SPEC 第 1 节）
//
// 为什么都先落到一个 `const bool miniOk` 再判：
//   断言里写常量是**正常**的（`MINI_CHECK_NEAR(0.1 + 0.2, 0.3, 1e-9)` 整条就是常量），
//   而 MSVC /W4 会对 `if (常量)` 报 C4127，把用例代码搞脏。先存进变量再判即可避开，
//   且失败分支仍然只在失败时才构造消息（没有"每次断言都拼一遍字符串"的开销）。
//   `(void)miniOk;` 是给"表达式本身有副作用但取值为常量"的情形留的后路，避免 C4189。
//
// ⚠️ **为什么 MINI_CHECK 用可变参数 `...` 而不是 `(expr)`**（2026-09-29 改）：
//   函数宏的参数按**顶层逗号**切分，而"哪一层算顶层"只数**圆括号**——
//   **花括号不保护逗号**。所以 `MINI_CHECK(Vec2{1.0f, 2.0f}.LengthSq() > 0.0f)`
//   会被拆成两个参数，报 C4002「参数过多」，再引发一串看不懂的下游语法错误。
//   因为 `MINI_CHECK` 只需要**一个**表达式，用 `...` 原样接收就彻底没有这个问题。
//
// ⚠️ 但 `MINI_CHECK_EQ(lhs, rhs)` / `MINI_CHECK_NEAR(lhs, rhs, eps)` **必须区分参数**，
//   可变参数拆不开 → 它们的参数里若含顶层逗号，**调用方要自己多包一层圆括号**：
//       MINI_CHECK_EQ((AABB{1.0f, 2.0f, 3.0f, 4.0f}.min.x), 1.0f)   ← 这对括号是必需的
//       MINI_CHECK_NEAR((Vec2{3.0f, 4.0f}.Perp().Length()), 5.0f, 1e-5f)
//   而本来就落在函数调用圆括号里的花括号**不需要**处理：
//       MINI_CHECK(v.ApproxEquals(Vec2{1.0f, 2.0f}))   ← 逗号已在第 2 层，安全
// ------------------------------------------------------------

#define MINI_CHECK(...)                                                                 \
    do {                                                                                \
        const bool miniOk = static_cast<bool>(__VA_ARGS__);                             \
        (void)miniOk;                                                                   \
        if (!miniOk) {                                                                  \
            ::minitest::ReportFailure(__FILE__, __LINE__, "MINI_CHECK(" #__VA_ARGS__ ")"); \
        }                                                                               \
    } while (false)

#define MINI_CHECK_EQ(lhs, rhs)                                                     \
    do {                                                                            \
        const bool miniOk = ((lhs) == (rhs));                                       \
        (void)miniOk;                                                               \
        if (!miniOk) {                                                              \
            const std::string miniMsg =                                             \
                std::string("MINI_CHECK_EQ(" #lhs ", " #rhs ")  lhs=") +             \
                ::minitest::ToString(lhs) + "  rhs=" + ::minitest::ToString(rhs);   \
            ::minitest::ReportFailure(__FILE__, __LINE__, miniMsg.c_str());         \
        }                                                                           \
    } while (false)

// 浮点比较：禁写 ==。写成"两个方向都 <= eps"是为了不引入 <cmath> 与 abs 重载问题，
// 同时天然避开 -Wfloat-equal。
#define MINI_CHECK_NEAR(lhs, rhs, eps)                                              \
    do {                                                                            \
        const bool miniOk =                                                         \
            (((lhs) - (rhs)) <= (eps) && ((rhs) - (lhs)) <= (eps));                 \
        (void)miniOk;                                                               \
        if (!miniOk) {                                                              \
            const std::string miniMsg =                                             \
                std::string("MINI_CHECK_NEAR(" #lhs ", " #rhs ", " #eps ")  lhs=") + \
                ::minitest::ToString(lhs) + "  rhs=" + ::minitest::ToString(rhs);   \
            ::minitest::ReportFailure(__FILE__, __LINE__, miniMsg.c_str());         \
        }                                                                           \
    } while (false)

#define MINI_FAIL(msg)                                                       \
    do {                                                                     \
        ::minitest::ReportFailure(__FILE__, __LINE__, "MINI_FAIL(" msg ")"); \
    } while (false)
