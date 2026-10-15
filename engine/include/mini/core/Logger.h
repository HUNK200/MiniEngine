#pragma once

// ============================================================
// mini/core/Logger.h —— 分级日志（S11 的"看得清"之一）
//
// 它解决的具体问题（`TECH_DEBT` C14 的到期处置）：
//   2026-10-08 起 `game/` 里的调试快照是**手写 `std::ofstream`**（`Game::DumpSnapshot`），
//   那是最省的做法、但把"写日志"这件事放进了游戏侧。正式 Logger 属于**引擎**：
//   分级过滤、RAII、前缀格式、"程序崩了别丢最后一行"这些是**基础设施**，不是游戏逻辑。
//
// 三条设计取舍（都写在对应位置，这里先给总账）：
//   1. **收一整行 `string_view`，不收变参**：`AGENTS.md` §3 零依赖 ⇒ 不引 `std::format`
//      /`{fmt}`，拼接由调用方做（`Game` 侧本来就有 `snprintf` 的习惯）。
//   2. **每行 flush**：日志的用途是查"上一秒崩了"那一类问题，攒在缓冲区里等于没写。
//      代价是每次写盘几微秒 —— 它只在**手动按键**时才被调用（§8 坑 12），不在热路径上。
//   3. **current 指针**（`SetCurrentLogger`/`CurrentLogger`）+ 自由函数 `Log()`：
//      让 `game/` 侧不必每一处都写 `if (auto* lg = CurrentLogger())`。
//      没装 Logger 时**静默丢弃** —— 这正是"测试里不装也能跑"的前提。
//
// 与 commit 2 的 `Profiler.h` 是**同一个模式**（进程级 current 指针 + 未装即空转），
// 两处刻意保持一致 —— 先在这里写下来，落 Profiler 时照着抄，别各自发明一套。
// ============================================================

#include <fstream>      // std::ofstream（成员，所以必须在这里 include，不能前置声明）
#include <string_view>

namespace mini::core {

// 级别由低到高。`Off` 是"全关"，也是**最高**的那个值 ——
// 于是过滤只需要一句 `if (lv < m_level) return;`，不需要为 Off 写特例分支
// （`TECH_DEBT` C33 那条教训：别留"实现了一半"的守卫）。
enum class LogLevel : int {
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Off,
};

// 返回 "TRACE" / "DEBUG" / "INFO" / "WARN" / "ERROR" / "OFF"。
// 用例 `PrefixesLevelAndSingleNewline` 用它当判据，所以**返回值是契约的一部分**。
[[nodiscard]] const char* ToString(LogLevel lv) noexcept;

class Logger {
public:
    // 沿用 C14 的**文件名**（`Game::DumpSnapshot` 一直在写它）：
    // 老日志文件里是 `key=value` 样式的正文，新格式只是**在前面加了前缀**，
    // 所以拿旧的 `miniengine_debug.log` 继续 grep 仍然有效（§8 坑 13）。
    static constexpr const char* kDefaultPath = "miniengine_debug.log";

    // 构造时**总是**写 3 行 session 头（进程启动时间 / 版本 / 级别）：
    //   即使文件是追加的，也要能一眼看出"这一段是哪个进程写的"。
    //   ⚠️ 这 3 行**不经过级别过滤**（级别是构造完之后才可能被改的）。
    // 文件打不开（路径不存在 / 没权限）→ `Good() == false`，**不抛异常**：
    //   日志设施本身不该成为崩溃源（用例 `BadPathIsNotFatal` 钉住这条）。
    static constexpr int kSessionHeaderLines = 3;

    explicit Logger(const char* path = kDefaultPath);
    ~Logger();   // RAII：关文件（`ofstream` 的析构本来就会关，这里显式 flush 一次）

    // 不可拷贝 / 不可移动：它是"一个文件一把句柄"的资源持有者，
    // 而 current 指针指向的地址必须稳定（移动会让别处存的指针悬空）。
    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&)                 = delete;
    Logger& operator=(Logger&&)      = delete;

    void SetLevel(LogLevel lv) noexcept;
    [[nodiscard]] LogLevel Level() const noexcept;

    // 文件真打开了？ 用例与 `game/` 侧都靠它判断"日志到底有没有在写"。
    [[nodiscard]] bool Good() const noexcept;

    // 收**一整行**（不含 '\n'，也不含前缀）；低于当前级别直接丢。
    // 实现补 "[HH:MM:SS.mmm][LEVEL] " 前缀 + '\n' + flush。
    void Log(LogLevel lv, std::string_view line);

    void Flush();

private:
    // 不过滤、直接落盘。session 头与 `Log()` 共用它 ——
    // 有它才不用为"头那 3 行"复制一遍前缀拼接。
    void WriteLine(LogLevel lv, std::string_view line);
    void WriteSessionHeader();

    std::ofstream m_out;
    LogLevel      m_level = LogLevel::Info;   // 默认 Info：Debug/Trace 要显式打开
    bool          m_good  = false;
};

// 进程级"当前 Logger"。
// 未装（返回 nullptr）时，下面的自由函数 `Log()` 静默丢弃 —— 不是错误路径：
// 单测里不装 Logger、`game/` 在 `Application` 之外被构造，都属于正常情形。
void SetCurrentLogger(Logger* logger) noexcept;
[[nodiscard]] Logger* CurrentLogger() noexcept;

// 自由函数：转发给 `CurrentLogger()`；未装则什么都不做。
// ⚠️ 它不在 `S11_CHECKLIST` §4.1 的声明表里（那张表只列了成员 `Log`）——
//    但 §7.1 的用例 `CurrentAccessorRoundTrip` 要求"未装时 `Log` 静默丢弃"，
//    而未装时**根本不存在实例**，所以那句话只能指自由函数。**这是有意的增项**。
void Log(LogLevel lv, std::string_view line);

} // namespace mini::core
