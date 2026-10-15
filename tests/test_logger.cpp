#include "MiniTest.h"

#include <mini/core/Logger.h>

#include <cstddef>
#include <cstdio>      // std::remove（用例自己收拾临时文件）
#include <fstream>
#include <iterator>   // std::istreambuf_iterator
#include <string>
#include <string_view>

// ------------------------------------------------------------
// core::Logger 的单元测试（S11）。
//
// 不需要终端、不需要时钟：Logger 只碰文件系统。
// 用例一律**用自己删自己的临时文件**（`miniengine_test_logger.log`，在 ctest 的工作目录里）
// —— 不这么做的话，第二次跑就会读到上一次的内容，"追加"那一条会假绿。
//
// 唯一没法自动测的是"人看着读起来顺不顺"（前缀长度、时间戳格式）——
// 那靠 §9.3 的人眼项（`F4` 之后打开 `miniengine_debug.log` 看一眼）。
// ------------------------------------------------------------

namespace {

using mini::core::Log;   // 自由函数（转发给 current）
using mini::core::Logger;
using mini::core::LogLevel;

constexpr const char* kTestPath = "miniengine_test_logger.log";

void RemoveLogFile()
{
    (void)std::remove(kTestPath);
}

// 按**二进制**读回来：写的时候是二进制（行尾只有 '\n'），
// 所以"写出去几个字节 == 读回来几个字节"这条在这里是可断言的事实。
std::string ReadWholeFile(const char* path)
{
    std::ifstream in(path, std::ios::in | std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::size_t CountOccurrences(std::string_view haystack, std::string_view needle)
{
    std::size_t count = 0;
    std::size_t pos   = 0;
    while ((pos = haystack.find(needle, pos)) != std::string_view::npos) {
        ++count;
        pos += needle.size();
    }
    return count;
}

std::size_t CountNewlines(std::string_view text)
{
    return CountOccurrences(text, "\n");
}

} // namespace

// 低于级别的行**一个字节都不写**；同时钉住"session 头不受过滤影响"。
MINI_TEST(Logger, FiltersBelowLevel)
{
    RemoveLogFile();
    {
        Logger log(kTestPath);
        MINI_CHECK(log.Good());

        log.SetLevel(LogLevel::Warn);
        // ⚠️ 这里**不能**用 MINI_CHECK_EQ：`enum class` 没有 operator<<，
        //    而 MINI_CHECK_EQ 在失败分支里会 ToString 两侧（编译期就要实例化）。
        MINI_CHECK(log.Level() == LogLevel::Warn);

        log.Log(LogLevel::Info, "info-must-be-dropped");
        log.Log(LogLevel::Warn, "warn-must-be-written");
    }

    const std::string text = ReadWholeFile(kTestPath);
    MINI_CHECK(text.find("info-must-be-dropped") == std::string::npos);
    MINI_CHECK(text.find("warn-must-be-written") != std::string::npos);

    // 头 3 行是用 WriteLine 直接写的 ⇒ 它们**必须**还在（否则"日志级别设成 Warn 之后
    // 连 session 头都没了"，那种日志第一眼看不出是哪个进程写的）
    MINI_CHECK_EQ(CountOccurrences(text, "[INFO]"),
                  static_cast<std::size_t>(Logger::kSessionHeaderLines));
    MINI_CHECK_EQ(CountOccurrences(text, "[WARN]"), std::size_t{1});
}

// 前缀格式：每行都以 '[' 开头、级别出现在前缀里、文件以 '\n' 收尾（没有半行）。
MINI_TEST(Logger, PrefixesLevelAndSingleNewline)
{
    RemoveLogFile();
    {
        Logger log(kTestPath);
        log.SetLevel(LogLevel::Trace);   // 全放行，方便数行
        log.Log(LogLevel::Debug, "payload-debug");
        log.Log(LogLevel::Error, "payload-error");
    }

    const std::string text  = ReadWholeFile(kTestPath);
    const std::size_t lines = CountNewlines(text);

    // 期望行数由**公开常量**算出来，不写死 3（`S5_CHECKLIST` 10.4 的纪律）
    MINI_CHECK_EQ(lines, static_cast<std::size_t>(Logger::kSessionHeaderLines) + std::size_t{2});

    std::size_t start = 0;
    for (std::size_t i = 0; i < lines; ++i) {
        const std::size_t nl = text.find('\n', start);
        MINI_CHECK(nl != std::string::npos);
        if (nl == std::string::npos) {
            break;
        }
        MINI_CHECK(text[start] == '[');   // 每行行首就是前缀的左括号
        start = nl + 1;
    }

    MINI_CHECK(!text.empty());
    MINI_CHECK(text.back() == '\n');   // 最后一行写完就换行 ⇒ 不会留半行

    // 级别写进了前缀，且**每条正文只出现一次**（防止"每行都印同一个级别"这种错法）
    MINI_CHECK_EQ(CountOccurrences(text, "[DEBUG]"), std::size_t{1});
    MINI_CHECK_EQ(CountOccurrences(text, "[ERROR]"), std::size_t{1});
    MINI_CHECK_EQ(CountOccurrences(text, "payload-debug"), std::size_t{1});
}

// ★ C14 的守卫：**同一个路径第二次构造**不许把第一次的内容冲掉。
MINI_TEST(Logger, AppendsAcrossInstances)
{
    RemoveLogFile();
    {
        Logger first(kTestPath);
        first.Log(LogLevel::Info, "from-first-instance");
    }
    {
        Logger second(kTestPath);
        second.Log(LogLevel::Info, "from-second-instance");
    }

    const std::string text = ReadWholeFile(kTestPath);
    const std::size_t firstPos  = text.find("from-first-instance");
    const std::size_t secondPos = text.find("from-second-instance");

    MINI_CHECK(firstPos != std::string::npos);    // 第一次的内容还在
    MINI_CHECK(secondPos != std::string::npos);   // 第二次的也写进去了
    MINI_CHECK(firstPos < secondPos);             // 而且顺序没乱（真的是追加，不是覆盖后重排）

    // 两次构造 ⇒ 两条 session 头（"一眼看出这一段是哪个进程写的"）。
    // ⚠️ 判据是"每构造一次写一条 `session start`"，与 `kSessionHeaderLines` 的大小无关
    //    —— 所以这里写 2，不拿那个常量去凑算式（凑出来的是假精确）。
    MINI_CHECK_EQ(CountOccurrences(text, "session start"), std::size_t{2});
}

// 路径不可写**不是**崩溃：`Good() == false`，之后 `Log`/`Flush` 全部静默 no-op。
MINI_TEST(Logger, BadPathIsNotFatal)
{
    // 目录不存在 ⇒ 打开失败。刻意用"这个名字不会被真创建"的路径，
    // 而不是去依赖文件权限（权限在不同机器上不一样，判据会飘）。
    Logger log("miniengine_no_such_dir/never_created.log");

    MINI_CHECK(!log.Good());
    log.Log(LogLevel::Error, "must not throw");   // 抛了/崩了 = 用例失败（不是"没写"）
    log.Flush();
    MINI_CHECK(!log.Good());                      // 也不许因为写过一次就变成 good
}

// current 指针：装/卸一个来回；**未装时自由函数静默丢弃**。
MINI_TEST(Logger, CurrentAccessorRoundTrip)
{
    // 前置：别的用例没有把 current 留下来（每个用例自己负责卸干净）
    MINI_CHECK(mini::core::CurrentLogger() == nullptr);

    RemoveLogFile();

    Log(LogLevel::Info, "dropped-before-install");   // 未装 ⇒ 丢弃，且不崩

    {
        Logger log(kTestPath);
        mini::core::SetCurrentLogger(&log);
        MINI_CHECK(mini::core::CurrentLogger() == &log);

        Log(LogLevel::Info, "through-current");      // 装上了 ⇒ 真的写进去
        log.Flush();

        mini::core::SetCurrentLogger(nullptr);
        MINI_CHECK(mini::core::CurrentLogger() == nullptr);

        Log(LogLevel::Info, "dropped-after-uninstall");
    }

    const std::string text = ReadWholeFile(kTestPath);
    MINI_CHECK(text.find("through-current") != std::string::npos);
    MINI_CHECK(text.find("dropped") == std::string::npos);   // 前后两次丢弃都没落盘
}
