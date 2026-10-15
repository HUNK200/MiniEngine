#include "mini/core/Logger.h"

#include "mini/core/Version.h"   // session 头第 2 行：版本号（它就是那条"活化石"链子）

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <ctime>

// ============================================================
// core::Logger 实现（S11）
//
// 一件事值得先记住：**行格式是契约**。
//   [14:03:27.481][INFO] wave=2 foes=3 ammo=17
//   ^^^^^^^^^^^^^^^^^^^^ ^^^^^^^^^^^^^^^^^^^^^
//        Logger 拼的前缀        调用方给的正文（原样，一个字符都不动）
// 这条分工让"老日志还能 grep"成立：C14 时期的文件里是纯 `key=value`，
// 新文件只是每行前面多了一段固定格式的前缀（§8 坑 13）。
//
// 时间戳为什么用 system_clock 而不是 steady_clock：
//   日志是给人看的**日历时间**（"我是几点几分按的 F4"），而 steady_clock 的起点是
//   开机时刻，写进文件里没法读。⚠️ 所以它和 `Time` / `Profiler` 用的时钟**故意不同**，
//   别为了"统一"把它改掉。
// ============================================================

namespace mini::core {
namespace {

// 进程级"当前 Logger"。**故意用裸指针**：Logger 的生命周期归 `Application`（它的成员），
// 这里只是"最近被装进来的那一个"的弱引用，不参与所有权、也不需要智能指针。
// 单线程项目 ⇒ 不加锁（`AGENTS.md` §3：不做用不上的东西）。
Logger* g_current = nullptr;

// `[HH:MM:SS.mmm][LEVEL] ` —— 定长格式，进栈上缓冲，不分配。
// 抽出来是为了让"时间戳 + 级别"这段只有一处实现（`WriteLine` 是唯一调用者）。
void FormatPrefix(char* out, std::size_t cap, LogLevel lv) noexcept
{
    const auto now = std::chrono::system_clock::now();

    // 毫秒单独取：`time_t` 只有秒精度，直接格式化会丢掉毫秒那一格
    const auto ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm           tmBuf{};
    // ⚠️ MSVC 的 `localtime` 在 /W4 下报 C4996（不安全，要 `_s` 版）。
    //    本项目**只做 Windows**（`AGENTS.md` §3）⇒ 直接用 `localtime_s`。
    //    这不是"平台分支"，是唯一那条实现路径。
    (void)::localtime_s(&tmBuf, &tt);

    std::snprintf(out, cap, "[%02d:%02d:%02d.%03d][%s] ",
                  tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec,
                  static_cast<int>(ms.count()),
                  ToString(lv));
}

} // namespace

const char* ToString(LogLevel lv) noexcept
{
    switch (lv) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Error: return "ERROR";
    case LogLevel::Off:   return "OFF";
    }
    // 越界枚举（只能由强制转换造出来）：给一个**看得见**的兜底，不返回 nullptr
    // （同 `KeyCode.h` 的 `KeyIndex` 哨兵、`GameState.h` 的 `PolicyOf` 兜底精神）。
    return "?";
}

Logger::Logger(const char* path)
    // 追加 + **二进制**模式：
    //   · `app`  —— 绝不截断（C14 的老文件要留着）
    //   · `binary` —— 不把 '\n' 翻成 CRLF。行格式是契约，写出去几个字节就读回几个字节；
    //     用例也是按字节读回来数的（`ConsoleWindows` 给 stdout 切二进制是同一个理由）。
    : m_out((path != nullptr) ? path : kDefaultPath,
            std::ios::out | std::ios::app | std::ios::binary)
    , m_good(m_out.is_open())
{
    if (!m_good) {
        return;   // 打不开就降级成"什么都不做"：日志设施不该把程序拖下水
    }
    WriteSessionHeader();
}

Logger::~Logger()
{
    // `ofstream` 析构本来就会 flush + close；这里显式来一次，是为了让
    // "最后一行一定落盘"不依赖标准库的实现细节。
    Flush();
}

void Logger::SetLevel(LogLevel lv) noexcept
{
    m_level = lv;
}

LogLevel Logger::Level() const noexcept
{
    return m_level;
}

bool Logger::Good() const noexcept
{
    return m_good;
}

void Logger::Log(LogLevel lv, std::string_view line)
{
    // ⚠️ **一条分支都没多写**：
    //    · "文件没打开"不用单独判 —— `m_good == false` 时 `m_out` 处于 fail 状态，
    //      写进去是 no-op（多一条早退分支就是多一条走不到的路径，`TECH_DEBT` C33 的教训）；
    //    · "级别是 Off"也不用特例 —— `Off` 是**最高**那一档，`lv < Off` 恒成立（见头文件）。
    if (lv < m_level) {
        return;   // 低于级别：一个字节都不写
    }
    WriteLine(lv, line);
}

void Logger::Flush()
{
    m_out.flush();
}

// ------------------------------------------------------------
// 私有：前缀 + 正文 + '\n' + flush
// ------------------------------------------------------------
void Logger::WriteLine(LogLevel lv, std::string_view line)
{
    char prefix[64];   // "[00:00:00.000][ERROR] " 最长 24 字符 ⇒ 64 足够，且全程不分配
    FormatPrefix(prefix, sizeof(prefix), lv);

    m_out << prefix;
    m_out.write(line.data(), static_cast<std::streamsize>(line.size()));
    m_out << '\n';
    m_out.flush();   // 崩溃前不丢行（头文件第 2 条取舍）
}

void Logger::WriteSessionHeader()
{
    // 3 行（行数就是 `kSessionHeaderLines`，用例按它算期望）。
    // ⚠️ 走的是 `WriteLine`（**不过滤**）—— 级别是构造完之后才可能被 `SetLevel` 改的，
    //    用 `Log()` 写头，会把头也一起过滤掉。
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tmBuf{};
    (void)::localtime_s(&tmBuf, &tt);

    char text[96];
    std::snprintf(text, sizeof(text), "session start %04d-%02d-%02d %02d:%02d:%02d",
                  tmBuf.tm_year + 1900, tmBuf.tm_mon + 1, tmBuf.tm_mday,
                  tmBuf.tm_hour, tmBuf.tm_min, tmBuf.tm_sec);
    WriteLine(LogLevel::Info, text);

    std::snprintf(text, sizeof(text), "version %s", VersionString());
    WriteLine(LogLevel::Info, text);

    std::snprintf(text, sizeof(text), "level %s", ToString(m_level));
    WriteLine(LogLevel::Info, text);
}

// ------------------------------------------------------------
// current 指针 + 自由函数（未装即静默丢弃）
// ------------------------------------------------------------
void SetCurrentLogger(Logger* logger) noexcept
{
    g_current = logger;
}

Logger* CurrentLogger() noexcept
{
    return g_current;
}

void Log(LogLevel lv, std::string_view line)
{
    if (Logger* logger = CurrentLogger(); logger != nullptr) {
        logger->Log(lv, line);
    }
    // 未装：什么都不做。**这是有意的**，不是漏了错误处理 ——
    // 有它，`game/` 侧才不必每一处都写 `if (auto* lg = CurrentLogger())`。
}

} // namespace mini::core
