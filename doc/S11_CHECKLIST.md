# S11 施工图：Debug Overlay + Logger + Profiler —— v0.4「看得清」

> 本文件是 **S11 的施工图 + 进度板 + 验收方案**。按 `PLAN` 的 G 节，S11 的目标是 v0.4「看得清」：
> **"能说出『渲染占了多少 ms』"**。它是 `H.1` **保底路径（S0~S10）之后的第一站**，也是本项目
> 第一次**必须动 `engine/`** 的阶段（S5~S10 里 `engine/` 的改动屈指可数）。
>
> ⚠️ 本文件按 **S10 的模板**写（`doc/S10_CHECKLIST.md`）：§0 前置事实 · §1 目标/不做 · §2 交付物 ·
> §3 依赖 · §4 数据结构 · §5 逐文件步骤 · §6 接口（只列声明）· §7 伪代码 · §8 决策表 · §9 坑表 ·
> §10 验收（用例 / 正向+人眼 / 反向验证 / 自检）· §11 要同步的文档 · §12 与 PLAN 差异 · §13 学习点 ·
> **§14 分叉（待本人拍板）**。

---

## 当前状态

| 项 | 值 |
| --- | --- |
| 本文件 | ✅ **2026-10-12 落纸**（前置事实 18 条 + 坑表 15 条 + 决策表 D1~D13 + 验收方案） |
| **分叉** | ⏳ **未拍板** —— 本人 2026-10-12 明确："**我不需要现在就完整方案，我只需要记录**"。<br>⇒ §14 的 5 条**保持待办**；那 5 个"推荐"只是 **AI 的建议默认值**，⛔ **不是结论**，别当已定 |
| 实现 | ⬜ **未开工**（本阶段一行代码都没写；`AGENTS.md` §1：默认只出方案，开工等本人发话） |
| 前置依赖 | ⚠️ **S10 的人眼验收（`S10_CHECKLIST` §10.5 的 28 条）还没走完** ⇒ 建议顺序：先把 S10 收口，再开 S11 |
| 上一阶段 | S10 机器验收全绿（**197 用例** · 12 场反向验证 8 红 5 绿 · 代码全部还原）· `engine/` 在 S0~S10 全程**零改动** |

## 分工约定（沿用 `AGENTS.md` §1.4，S8/S9/S10 已跑通）

| 谁 | 做什么 |
| --- | --- |
| **AI** | **只编辑 / 新建源文件**。不跑 `cmake`、不跑 `ctest`、不跑 `MiniEngine.exe` |
| **本人** | 构建、跑用例、跑 exe、人眼观察，把结果回报 |

⚠️ 报告按 **7 节格式**（`.agents/skills/step-report/SKILL.md`）· 不许写"构建通过/已实测"除非本人回报 ·
"有什么用"要写**挡住了哪个具体失败**（不许写成卖点）· 人眼项**按阶段合并**、到验收步一次走完。

## 进度板（两个前置 + §5 的 8 步 + 验收）

| # | 内容 | 状态 |
| --- | --- | --- |
| **0a** | **S10 人眼验收走完**（`S10_CHECKLIST` §10.5 的 28 条） | ⬜ 未开始（前置） |
| **0b** | **§14 的 5 个分叉拍板** | ⏳ **本人 2026-10-12 明确"先不拍板、只记录"** ⇒ 待定（它决定 §4/§5 的文件与签名形状，**开工前必须先落**） |
| **1** | `debug/Profiler.h` + `test_profiler.cpp`（环形缓冲 / 深度配对 / 聚合，纯逻辑离线测） | ⬜ |
| **2** | `core/Logger.h/.cpp` + `test_logger.cpp`（分级 / 追加 / BOM 只写一次） | ⬜ |
| **3** | `Application` 接线五个 scope（Poll/Update/Render/Present/LimitFPS）—— ⚠️ **中间态：数字攒了但看不见** | ⬜ |
| **4** | `debug/DebugOverlay.h/.cpp` + `F1` 转发 + `test_overlay.cpp` | ⬜ |
| **5** | `F2` 实心格高亮（兑现 **C13**，若分叉⑤ 取甲） | ⬜ |
| **6** | `F3` 实体表 + 删 `F7` 面板（兑现 **C15**；给 **C23** 一个观察面） | ⬜ |
| **7** | `DumpSnapshot` 改走 `Logger`（**F4 语义不变**，兑现 **C14**） | ⬜ |
| **8** | 收口：§11 文档逐条同步 + `TECH_DEBT`（关 C13/C14/C15 + 新条目）+ `PLAN` 的 S11 小节重写 | ⬜ |
| **9** | 验收：正向 + 人眼 8 条 + **反向验证 6 场**（★ R5 专门验 C31 的守卫真的会响） | ⬜ |

---

## 0. 前置事实（写方案前逐条核过的，不是推测）

| # | 事实 | 位置 | 对 S11 的意义 |
| --- | --- | --- | --- |
| 1 | `Time::WorkMs()` **已经存在**（本帧 Begin→End 的干活耗时，毫秒）；`Time.h` 的文件头注释自己写着"**也是 S11 Profiler 的雏形**" | `engine/include/mini/core/Time.h:22` | S11 不是从零开始：**帧级**耗时已经有了，缺的是**子系统级**（谁占了多少） |
| 2 | `Time::Stats()` 访问器**已被删除**（0 调用者），但注释留了话："将来真需要（比如**把整组统计交给 S11 的 Profiler**）时加回来是一行" | `Time.h` 末尾注释 | ⇒ 分叉②的一个现成选项；也可能是"零改动"的证据（不必加回来） |
| 3 | `FrameStats` 是**纯逻辑**（不读时钟、可离线单测）+ 60 帧滑窗 + `MaxFrameMs` **只被 `Reset()` 清**（生产 0 调用） | `Time.h` | ★ **Profiler 的模板**：窗口统计做成纯逻辑类 ⇒ 可离线测；"只增不清"的口径要在面板上写明 |
| 4 | 主循环 ①`BeginFrame` → ②`Input::Poll` → ③画布对齐 → ④造 `FrameContext` → ⑤`OnUpdate` → ⑥`Clear`+`OnRender`(+顶栏) → ⑦`Present` → ⑧`BeginFrame`/`EndFrame`/`LimitFPS` → 退出判定 | `engine/src/core/Application.cpp` 的 `Run` | ★ **所有可计时的子系统都在这条线上**：Poll / 游戏更新 / 游戏渲染 / 提交(含 RowDiff) / 限帧(睡+自旋) / 顶栏 |
| 5 | `FrameContext` **刻意不含 `Time&`**（硬规矩 2：不给游戏可变通路，否则游戏能调 `EndFrame`/`LimitFPS` 搞乱时序，且没有编译器拦得住） | `Application.h` 文件头 | ★★ **决定分叉②**：Profiler 若要在游戏代码里打点，**不能**通过 `FrameContext` 暴露可变引用 —— 否则把同一条规矩破掉 |
| 6 | 顶栏由 `Application::DrawStatsBar` 在 **`game.OnRender` 之后**画（硬规矩 3），占**第 0 行**；`AppConfig::showStats` 是**启动期**布尔 ⇒ **没有运行时开关** | `Application.h` / `Application.cpp` | F1 开关要么住 `Application`（它持有 `Input`），要么住 `Game`（它也能读输入）⇒ 决策 D3 |
| 7 | `DrawStatsBar` 的写法：`char text[96]` + `snprintf` + 定宽 `%5.1f` + 手写裁剪 + **右侧擦到行尾**（防缩窄残留）+ `Color::BrightBlack` | `Application.cpp` | Overlay 直接复用这套（**定宽**与**擦行尾**两条都是踩出来的，别重造） |
| 8 | `KeyCode.h` 里 **`F1/F2/F3/F4` 早就映射好了**，注释原文写着"**S11 的 Debug Overlay 用**"；VK 映射 `map[0x70..0x73]` | `engine/include/mini/platform/KeyCode.h:31,96-100` | ⇒ **大概率不用改 `KeyCode.h`**（同 S10：`Enter`/`Esc`/`R` 也是早就备好的） |
| 9 | ⚠️ **F4 已被 `game/` 占用**：`constexpr Key kDumpKey = Key::F4;`（`DumpSnapshot`，`TECH_DEBT` **C14**） | `game/src/Game.cpp:100` | ★ **PLAN 原稿的"F1~F4 开关"与现状冲突** ⇒ 分叉④ + §12 差异 |
| 10 | `F7` = `kAiPanelKey`（S8 的临时敌人面板，`TECH_DEBT` **C15**）；C15 的到期条件原文就是"**S11 正式 Debug Overlay 落地时删**" | `game/src/Game.cpp:114` | S11 **该**兑现它（含"删键要三处一起 + 另加一遍字面文本 grep"那条教训） |
| 11 | 渲染侧**已有现成的量化口径**：`RowDiff::Stats{rowsTotal, rowsSent, bytes}` + `Console::LastFrameRowsSent()`（顶栏 `Rows n/n` 就是它） | `RowDiff.h` / `ConsoleWindows.cpp:283` | S11 只要**把它搬进 Overlay**（+ 可加 `bytes`，现在生产 0 读） |
| 12 | `FrameBuffer` 公开面：`Set(x,y,c,fg,bg)` / `Draw(x,y,string_view)` / `DrawRect` / `InBounds` / `Width/Height` | `FrameBuffer.h` | Overlay **不需要**任何新绘制 API |
| 13 | `Console::Present(...)` 返回 `bool`（false = 终端没了） | `Application.cpp` ⑦ | 提交段可以独立计时 |
| 14 | 既有规模：生产 **5991 行**（`engine/` 3814 · `game/` 2124）· 用例 **197**（`Game` 33 · `GameState` 13 · …）· 测试 6487 行 | 2026-10-12 实测 | S11 的验收要**保证这 197 条一条不红**（Overlay 默认关闭是前提，见坑表 1） |
| 15 | `TECH_DEBT` 与本阶段**到期/相关**的条目：**C13**（F2 显示碰撞盒）· **C14**（dump 搬进正式 Logger）· **C15**（F7 面板 → 正式 Overlay）· **C23**（`m_bullets` 压缩守卫：建议加 `bullets=N` 观察面）· **C31**（★★ 屏幕字形命名空间：**Overlay 往屏幕上写字就要碰它**） | `doc/TECH_DEBT.md` | ★ **C31 的触发条件就是本阶段**（它预言的"`Enemy: 3` 里的 `E`"会在这里真实发生） |
| 16 | 顶层还有个**引擎顶栏**（第 0 行，`BrightBlack`），游戏版面从**第 1 行**开始；`game` 的 HUD 占**第 1~2 行**（行 1 = 58 列、行 2 = 64 列） | `Game.cpp` `DrawHud` / `Application.cpp` | ⚠️ Overlay 的**行区间必须避开 1~2 行**，否则盖住 HUD（而 HUD 是 10.2 的运行时自证口径）⇒ 坑表 7 |
| 17 | MiniTest 的能力边界：`MINI_CHECK` / `_EQ` / `_NEAR` / `FAIL`；**scoped enum 与 `Vec2` 不能进 `MINI_CHECK_EQ`**；suite 过滤是**精确 `strcmp`** | `tests/MiniTest.h` / `test_main.cpp` | Overlay/Logger 的用例要按这个约束写 |
| 18 | 构建：C++20 · MSVC · `/W4 /permissive- /utf-8` · **无 `/WX`**；**加测试源文件要重新 configure**；`file(GLOB)` 禁用（源文件手写进 `CMakeLists.txt`） | `CMakeLists.txt` / `tests/CMakeLists.txt` | 新增 3~4 个源文件 ⇒ 第一次构建要 `cmake -S . -B build` |

---

## 1. 目标 / 明确不做

| | 内容 |
| --- | --- |
| **目标** | 让"**渲染占了多少 ms**"这句话**有数字**：<br>① `PROFILE_SCOPE("Xxx")` RAII 打点（任意作用域）；<br>② 最近 **120 帧**的固定环形缓冲（**零分配**）+ 按子系统**聚合**成一张表；<br>③ `F1` 把这个表**画在屏幕上**（含帧总耗时、渲染/AI/更新/提交/限帧、RowDiff 的 `Rows n/n` 与字节数、`bullets=N`）；<br>④ `F2` 把**实心格**用背景色标出来（兑现 `TECH_DEBT` **C13**：让"越界 = 墙"看得见）；<br>⑤ `F3` 画**实体表**（取代 S8 的 `F7` 临时面板 ⇒ 兑现 **C15**）；<br>⑥ `Logger` 分级写文件（`F4` 的 dump 改成走 Logger ⇒ 兑现 **C14**） |
| **PLAN 原话** | "F1~F4 开关、`PROFILE_SCOPE("Physics")` RAII 计时、最近 120 帧的环形缓冲 + 表格输出"；可运行效果"终端右侧显示 `Input/Game/Physics/AI/Renderer/Total/FPS` 表格，F4 切换"；验收"**Renderer 是最耗时的项**这一结论能被数据验证；**Profiler 本身开销 < 0.2ms**" |
| **明确不做** | ❌ **Chrome Trace / JSON 导出**（`H.2` 扩展池，真需要时再做）· ❌ 多线程采样 / 时间轴并发（本项目单线程）· ❌ P95/P99 分位数与直方图（先 min/avg/max；真需要时登记 C 组）· ❌ 跨进程/远程 profiling · ❌ 把 Profiler 做成"一直开着"（默认关，见坑表 1）· ❌ 改 `FrameContext`（**不让可变 Profiler 通路进 ctx**，理由同 `Application.h` 硬规矩 2）· ❌ 平台抽象层的任何改动（只 Windows 一条路） |
| **本阶段的额外价值** | 它是**求职视角**最"能证明"的一步：把定性说法（"渲染慢"）变成**可复现的数字**，且顺带关掉 4 条技术债（C13/C14/C15/C23 的观察面） |

---

## 2. 交付物

| # | 交付物 | 类型 |
| --- | --- | --- |
| 1 | `engine/include/mini/debug/Profiler.h`（+ 可能 `.cpp`） | 新增（引擎） |
| 2 | `engine/include/mini/core/Logger.h` + `engine/src/core/Logger.cpp` | 新增（引擎，PLAN 目标树的 `core/Logger.h`） |
| 3 | `engine/include/mini/debug/DebugOverlay.h`（+ `.cpp`） | 新增（引擎；**只负责排版与画**，不认识游戏） |
| 4 | `Application` 接线：每帧 `Profiler::BeginFrame/EndFrame` + 五个子系统打点 + `F1/F2/F3` 转发 | 改（引擎） |
| 5 | `game/` 侧：填"实体表"的行 + `DumpSnapshot` 改走 Logger + 删 `F7` 面板 | 改（游戏） |
| 6 | `tests/test_profiler.cpp` · `tests/test_logger.cpp` · `tests/test_overlay.cpp` | 新增测试（+ `tests/CMakeLists.txt`） |
| 7 | 本文件 §10 的验收记录 + `TECH_DEBT`（关 C13/C14/C15，开新条目） | 文档 |
| 8 | `PLAN` 的 S11 小节按实际落地重写 + `AGENTS.md` §2 补一行 | 文档 |

---

## 3. 依赖规则（与既有分层一致）

```
main → game → engine → stdlib
```

- `engine/debug/*` 只依赖 `engine/render`（`FrameBuffer`）+ `engine/core`（`Time`）⇒ **不认识 game** ✓
- `engine/core/Logger` 只依赖 `<fstream>` / `<string_view>` ⇒ **不认识终端**（写文件，不写屏幕）✓
- `game/` **可以**调 `mini::debug::PROFILE_SCOPE`（宏）与 `Logger` ✓ —— 单向依赖不变 ✓
- ⛔ **Overlay 不允许认识 `EnemyData`**：实体表由 `game/` **准备好行**交给引擎（分叉① 取乙），
  否则引擎就认识游戏了（`PLAN` A.3 那条硬规矩）

---

## 4. 数据结构与所有权

```cpp
namespace mini::debug {

// 一帧里的一个打点。⚠️ `name` 指向**静态字符串字面量**（宏传进来的），不拥有、不复制、不分配。
struct TraceEvent {
    const char*    name  = nullptr;   // 例："Update" / "Render" / "Present"
    float          ms    = 0.0f;
    std::uint8_t   depth = 0;         // 嵌套深度（0 = 最外层）
};

// 一帧 = 定长数组 + 计数（**满了就丢**，不扩容 —— 这是"零分配"的代价与保证）
struct FrameTrace {
    static constexpr std::size_t kMaxEvents = 24;
    std::array<TraceEvent, kMaxEvents> events{};
    std::uint8_t  count   = 0;
    std::uint8_t  dropped = 0;        // 本帧溢出丢弃的条数（面板上要看得见）
    float         workMs  = 0.0f;     // 本帧干活耗时（= Time::WorkMs()，冗余留一份便于离线测）
};

class Profiler {
public:
    static constexpr std::size_t kWindowFrames = 120;   // 环形缓冲的帧数
    // 帧边界（由 Application 调）
    void BeginFrame() noexcept;                 // 清当前帧的 events（**不**碰环形缓冲）
    void EndFrame(float workMs) noexcept;       // 把当前帧压进环形缓冲（覆盖最老的一帧）
    // 打点（由 PROFILE_SCOPE 的构造/析构调）
    void Push(const char* name, float ms, std::uint8_t depth) noexcept;
    [[nodiscard]] std::uint8_t Depth() const noexcept;
    // 读侧（给 Overlay / 用例）
    [[nodiscard]] std::size_t FrameCount() const noexcept;    // 已填帧数（<= kWindowFrames）
    [[nodiscard]] std::size_t DroppedTotal() const noexcept;
    [[nodiscard]] const FrameTrace& Frame(std::size_t i) const noexcept;   // 0 = 最新
    // 聚合：把窗口内同名 scope 归并（**遍历 120×24 次比较**，面板每帧调一次）
    struct Row { const char* name; float avgMs; float maxMs; float minMs; std::uint16_t samples; };
    [[nodiscard]] std::size_t Aggregate(Row* out, std::size_t cap) const noexcept;
private:
    std::array<FrameTrace, kWindowFrames> m_frames{};
    std::size_t m_next  = 0;      // 下一个写入位置（环形）
    std::size_t m_count = 0;      // 已填帧数（<= kWindowFrames）
    FrameTrace  m_current{};      // 本帧（Begin→End 之间）
    std::uint8_t m_depth = 0;     // 作用域栈深度（PROFILE_SCOPE 进出配对）
    std::size_t m_droppedTotal = 0;
};

// 作用域计时器：构造取时间 + ++depth，析构累加 + --depth（RAII）
class ScopedTimer {
public:
    explicit ScopedTimer(const char* name) noexcept;
    ~ScopedTimer() noexcept;                 // ← 计时的唯一落点
    ScopedTimer(const ScopedTimer&)            = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
};
} // namespace mini::debug

// ⚠️ 唯一入口是宏（理由：要 `__FUNCTION__`/字面量名字，且**不给 ctx 加可变通路**）
#define PROFILE_SCOPE(name) \
    ::mini::debug::ScopedTimer MINI_CONCAT(_profileScope_, __LINE__){name}
```

**日志（引擎侧，写文件不写屏幕）**

```cpp
namespace mini::core {
enum class Severity : std::uint8_t { Trace = 0, Info, Warn, Error };
class Logger {
public:
    static Logger& Instance();                         // 进程内单例（不是单例模式，是"一份文件"）
    void Open(std::string_view path);                  // 显式打开（避免静态初始化期碰文件系统）
    void SetMinSeverity(Severity s) noexcept;          // 分级过滤**在格式化之前**判（省一次 snprintf）
    void Write(Severity s, std::string_view file, int line, std::string_view msg);
    [[nodiscard]] bool IsOpen() const noexcept;
};
#define LOG_INFO(msg)  ::mini::core::Logger::Instance().Write(::mini::core::Severity::Info,  __FILE__, __LINE__, (msg))
}
```

**Overlay（引擎侧，"只排版不认识游戏"）**

```cpp
namespace mini::debug {
class DebugOverlay {
public:
    // 三块面板各自可开关（F1/F2/F3 一一对应）
    enum class Panel : std::uint8_t { Timing = 0, SolidTiles, Entities };
    void Toggle(Panel p) noexcept;
    [[nodiscard]] bool Visible(Panel p) const noexcept;
    // 游戏每帧把"实体表"的行填进来（**行是 game 生成的字符串**；引擎只搬进画面）
    struct Rows {
        static constexpr std::size_t kMax = 16;
        std::array<std::string_view, kMax> lines{};
        std::size_t count = 0;          // 0 = 这一块没数据（就不画）
        std::size_t firstRow = 4;       // 从第几行开始画（见坑表 7）
    };
    void SetEntityRows(const Rows& rows) noexcept;   // game 在 OnRender 之前调
    void Draw(render::FrameBuffer& fb, const Profiler& prof) const;   // 引擎在 OnRender 之后调
};
}
```

**"实体行"的所有权（关键）**：字符串**由 `game/` 拥有**（`std::array<std::string, 16>` 成员，
每帧 `assign` 复用缓冲区 ⇒ **稳态零分配**）；引擎只拿 `string_view` 画一笔、不存。⇒ 引擎侧
0 个 `std::string` 成员，`game` 侧 16 个字符串**只增不减**（典型的"用满就够"）。

---

## 5. 逐文件实施步骤（建议 commit 切分）

| 步 | 文件 | 做什么 | 能跑起来的判据 |
| --- | --- | --- | --- |
| **1** | `engine/include/mini/debug/Profiler.h` + `tests/test_profiler.cpp` | 纯逻辑：环形缓冲（未满/环绕两条路径）· 深度配对 · 溢出丢弃 · 聚合（min/avg/max）· `sizeof`/`capacity` 断言 | `mini_tests.exe Profiler` 全绿；**不碰终端**（离线） |
| **2** | `engine/include/mini/core/Logger.h` + `engine/src/core/Logger.cpp` + `tests/test_logger.cpp` | 分级过滤 · 追加语义 · **BOM/表头只写一次**（先问文件系统，别用 `tellp()`） | `mini_tests.exe Logger` 全绿 |
| **3** | `Application` 接线（①Poll ②OnUpdate ③OnRender ④Present ⑤LimitFPS 五个 scope + `BeginFrame/EndFrame`）+ `Time::Stats()` 按需加回 | 数字先能**攒起来**（还没有 UI） | `MiniEngine.exe` 照常可玩；`F1` 还没反应（中间态，允许） |
| **4** | `engine/include/mini/debug/DebugOverlay.h/.cpp` + `Application` 的 `F1` 转发 + `tests/test_overlay.cpp` | 时序表能**画出来**（定宽 + 擦行尾 + 行区间） | 按 `F1` 出表；再按关掉；**197 条既有用例一条不红** |
| **5** | `F2` 实心格高亮（`Game` 侧读 `m_world.Map()` 画背景色）| 兑现 **C13** | 按 `F2` 看得见"墙/越界"；关掉后画面**逐字节**回到原样（用例可断言） |
| **6** | `F3` 实体表（`game` 填行：id / 状态 / hp / 位置 + `bullets=N`）⇒ 删 `F7` 面板（**C15 三处一起 + 字面文本 grep**） | 兑现 **C15** + 给 **C23** 一个观察面 | 按 `F3` 看到实体表；`F7` 不再有任何反应且 `KeyCode.h` 的那条映射已删 |
| **7** | `DumpSnapshot` 改走 `Logger`（**F4 语义不变**：还是"追加一行快照"） | 兑现 **C14** | 按 `F4` 仍然写 `miniengine_debug.log`，BOM 与表头**不重复** |
| **8** | 收口：§11 文档逐条同步 + `TECH_DEBT`（关 C13/C14/C15 + 新条目）+ `PLAN` 的 S11 小节按实际重写 | — | — |

⚠️ **第 3 步是"中间态"**：数字攒了但看不见。它必须**能构建能运行**（`PLAN` 原则 1）—— 这是
"每次收尾都留下一个能跑的 commit"那条纪律的具体体现。

---

## 6. 接口设计（只列声明，不写实现体）

| 接口 | 在哪 | 谁调 | 备注 |
| --- | --- | --- | --- |
| `Profiler::BeginFrame()` / `EndFrame(float)` | `engine/debug/Profiler.h` | `Application::Run` ① / ⑧ | 与 `Time::BeginFrame/EndFrame` **同一对位置**（顺序：Time 先、Profiler 后） |
| `Profiler::Push(name, ms, depth)` | 同上 | `ScopedTimer::~ScopedTimer` | 内部用 |
| `Profiler::Aggregate(Row*, cap)` | 同上 | `DebugOverlay::Draw` | 返回值 = 实际写了几行 |
| `PROFILE_SCOPE("X")` | 同上 | 任何作用域 | **唯一入口**；⛔ 不许在静态初始化期用 |
| `Logger::Instance()/Open/SetMinSeverity/Write` | `engine/core/Logger.h` | `main`（Open）+ 任何地方（Write） | `Open` 显式调（⛔ 不在静态期开文件） |
| `DebugOverlay::Toggle(Panel)` / `SetEntityRows` / `Draw` | `engine/debug/DebugOverlay.h` | `Application`（Toggle + Draw）· `Game`（SetEntityRows） | `Draw` **必须在 `game.OnRender` 之后**（同顶栏那条硬规矩） |
| `Game::BuildEntityRows()`（private） | `game/include/game/Game.h` | `Game::OnRender` 末尾（或 `OnUpdate` 末尾） | 返回/填 `DebugOverlay::Rows`；字符串归 game 所有 |

---

## 7. 算法与伪代码

**① 环形缓冲（Profiler 的核心）**

```
BeginFrame:  m_current = {}          // count=0, dropped=0, depth=0
Push(name, ms, depth):
    if m_current.count == kMaxEvents: m_current.dropped++; return
    m_current.events[m_current.count++] = {name, ms, depth}
EndFrame(workMs):
    m_current.workMs = workMs
    m_frames[m_next] = m_current
    m_next  = (m_next + 1) % kWindowFrames      // ★ 取模：不取模 = 越界（反向验证 R1）
    m_count = min(m_count + 1, kWindowFrames)   // ★ 未满时不许当成满（反向验证 R2）
```

**② 聚合（面板那张表怎么来）**

```
Aggregate(out, cap):
    n = 0
    for each 帧 f in 环形缓冲（遍历 m_count 个，**不是** kWindowFrames 个）:
        for each 事件 e in f:
            在 out[0..n) 里按 name 找（**指针比较**：字面量地址稳定 ⇒ 不必 strcmp）
            找到 → 累加 sum/count/samples、更新 min/max
            没找到 → 若 n < cap 则新开一行；否则**丢掉**（并计数）
    for each 行: avg = sum / samples       // samples == 0 的行不输出
```

**③ 作用域计时（RAII）**

```
ScopedTimer::ScopedTimer(name):
    m_name = name
    m_start = steady_clock::now()
    m_depth = profiler.Depth()          // 记下进入时的深度
    profiler.m_depth++                  // 配对：构造 +1
ScopedTimer::~ScopedTimer():
    const float ms = duration(now() - m_start)
    profiler.m_depth--                  // ← 先减（自己已经出栈）
    profiler.Push(m_name, ms, m_depth)  // 用**进入时**的深度记
```

⚠️ **`now()` 的两次调用之间必须"有副作用"**：`Push` 写环形缓冲就是副作用 ⇒ 计时器**不会**被优化掉
（坑表 6）。这也是"空 scope 计时不能证明开销"的原因。

**④ Overlay 排版（复用顶栏那套）**

```
Draw(fb, prof):
    if !Visible(Timing) → 跳过这块
    row = kFirstTimingRow                     // 见坑表 7（必须避开 HUD 的 1~2 行）
    画表头： "scope        avg     max     min   n"   ← ⚠️ 字面量里不许有 '@'/'E'/'*'（C31）
    for each 聚合行（按 avg 降序）:
        snprintf 定宽行 → fb.Draw(x=1, row, line)
        **右侧擦到行尾**（防缩窄残留）
        row++
    末尾两行： "total   <workMs>"  +  "profiler own <聚合耗时>"（自证不贵）
```

---

## 8. 接口决策表

| # | 决策 | 取值 | 理由 / 代价 |
| --- | --- | --- | --- |
| D1 | 窗口长度 | **120 帧**（PLAN 原稿） | 60fps 下 2 秒；固定数组 ⇒ 零分配。调大只改一个常量 |
| D2 | 每帧最多几个 scope | **24** | 12 个已知子系统 × 嵌套 2 层够用；满了**丢并计数**（不扩容） |
| D3 | `F1/F2/F3` 由谁读 | **`Application`**（它持有 `Input`） | 顶栏已经是它的职责；`game` 不必知道 Overlay 存在。⚠️ 但 `F2` 的**绘制**要 game 的数据 ⇒ `Toggle` 在引擎、**数据**由 game 提供 |
| D4 | Overlay 画在哪几行 | **第 4 行起**（HUD 占 1~2，留 3 空一行） | 避免盖 HUD（HUD 是 10.2 的运行时自证口径）；常量放 `DebugOverlay.h` 公开，**用例和游戏侧都能引** |
| D5 | Overlay 默认 | **全部关闭** | ★ 否则污染 34 处"数屏幕字符"的既有用例（C31 的教训）；用例要显式 `Toggle` |
| D6 | 聚合精度 | `float`（毫秒） | 与顶栏一致（`%5.1f`）；不要 `double`（没必要） |
| D7 | 名字比较 | **指针比较** | 宏传字面量 ⇒ 地址稳定；`strcmp` 会慢 10 倍且没必要（真出现动态名字再换） |
| D8 | Logger 打开时机 | `main` 里显式 `Open` | ⛔ 静态期开文件 = 未定义顺序 + 异常（本项目 `main` 只有 50 行，加一行无妨） |
| D9 | Logger 文件名 | 复用 `miniengine_debug.log` | `DumpSnapshot` 已经用它（且 `.gitignore` 已忽略 `*.log`）⇒ **零迁移成本** |
| D10 | BOM/表头 | **先问文件系统**（`exists` + `file_size`） | `tellp()` 踩过（追加模式下未写前返回 0）⇒ 别重犯 |
| D11 | `F7` 删除的时机 | **第 6 步一起**（同一次 commit） | C15 要求"三处一起"；分开删会有一段时间面板在但键没了（假提示） |
| D12 | `Time::Stats()` 要不要加回 | **先不加** | 若 Profiler 只需要 `WorkMs()`（值快照已有）⇒ 加了就是 0 调用者的新债 |
| D13 | "Profiler 开销 < 0.2ms" 怎么验 | **人眼指标，不做机器断言** | 任何"测绝对时间"的断言都会 flaky（CI / 杀毒 / 调度）⇒ 见坑表 8；机器侧只断言**计数与配对** |

---

## 9. 坑表

| # | 坑 | 怎么防 |
| --- | --- | --- |
| 1 | ★★ **Overlay 往屏幕上写字 ⇒ 屏幕是"唯一观察面"**：`game` 那 34 处 `CountChar/FindChar/ScreenText` 会被**污染**；而且字面量里出现 `@`/`E`/`*` 会**静默算错**（`TECH_DEBT` **C31** 预言的正是这里） | ① Overlay **默认全关**；② 文案⛔不许出现 `@`/`E`/`*`（"Enemy"写成 `Ent`/`Mob`）；③ 行区间常量化（D4）；④ 用例里"开 Overlay"的那些要**显式**开、并且**只数自己那几行** |
| 2 | `thread_local` 的两个真问题：**静态初始化期访问**（未构造）+ 析构顺序 | ① ⛔ 禁止在全局对象构造里用 `PROFILE_SCOPE`；② 本项目**单线程** ⇒ 不担心并发；③ 用具例钉"没有全局 ScopedTimer"（这条靠 review） |
| 3 | 环形缓冲**未满**与**环绕**是两条路径（`FrameStats` 已有前例） | 专门两条用例（`WindowNotFull` / `WindowWraps`），别只测一种 |
| 4 | "只增不清"的数字要**标明口径**（顶栏 `Max` 就是"自启动以来"） | 面板上写清：`avg/max` 是**最近 120 帧**，`total` 是本帧；不许混 |
| 5 | 计时精度：`steady_clock`（Windows = QPC）≈ 100ns 级 | 不测 < 1µs 的东西；表里加 `n`（样本数）让人自己判断可信度 |
| 6 | **空 scope 的耗时测不出来**（可能被优化掉），而且它是"计时器自身开销"的**错**的测法 | 用**有副作用**的 scope（真做一个空循环）估开销；或直接看"Instrumentation"那一行的数字 |
| 7 | ★ **Overlay 行区间与既有画面的冲突**：引擎顶栏占第 0 行、game HUD 占 1~2 行；Overlay 从第 1 行画就会**盖住 HUD** | D4：从**第 4 行**起；且"擦行尾"只擦自己那块（别把 HUD 擦了） |
| 8 | **时间类断言 = flaky 之源** | 纪律：**不断言绝对时间**，只断"计数 / 配对 / 窗口行为 / 相对关系"（B1 那条 `LimitFPS` 的教训：要把"有时发生"变成"**每次都发生**"） |
| 9 | 删 `F7` 的涟漪（`C15` 的三处 + **字面文本**） | `grep 'Key::F7\|kAiPanelKey\|DrawAiPanel\|F7'` **再加** `grep -rn 'kill\|K=\|面板'`（S9 漏过一次，见 C15 的教训）；`KeyCode.h` 删枚举会让后续枚举值前移 ⇒ 逐处核 |
| 10 | 日志：BOM/表头重复写（`tellp()` 踩过）；`PowerShell Get-Content` 按 GBK 解码 UTF-8 | `D10`：先问文件系统；写 BOM（`DumpSnapshot` 已证明有效） |
| 11 | `PROFILE_SCOPE` 的**命名冲突**（同一作用域两个同宏展开） | 宏里用 `__LINE__` 拼变量名（`MINI_CONCAT`）；并且**同一行写两个 scope 会撞** ⇒ 记进注释 |
| 12 | 面板自己的开销（120×24 次聚合）会**污染**它要测的东西 | ① 聚合只在**可见时**做（不可见就零成本）；② 面板上显示"聚合耗时"自证；③ 真嫌贵就降窗口（D1） |
| 13 | `FrameContext` 不许加可变通路（硬规矩 2） | Profiler 只走 `thread_local`（分叉② 取甲）；**ctx 一个字段都不加**（除已有的值快照） |
| 14 | 新增源文件 ⇒ 要重新 configure（`file(GLOB)` 禁用） | 第 1/2/4 步各自记得 `cmake -S . -B build` |
| 15 | `engine/` 这次**会有改动**（S5~S10 的"零改动"记录在这里结束） | §10.4 新增一条自检：**逐处核"为什么必须是引擎"**；`Logger`/`Profiler`/`Overlay` 都不认识游戏 ⇒ 合规 |

---

## 10. 验收

### 10.1 用例清单（预计 +30 条左右：197 → ~227）

| suite | 条数 | 覆盖 |
| --- | --- | --- |
| `Profiler` | ~12 | 环形未满 / 环绕 / 深度配对（进出） / 溢出丢弃计数 / 聚合 min·avg·max / 同名归并 / 空窗口 / `Aggregate` 的 cap 不够时丢行 / `BeginFrame` 清浅层 / `EndFrame` 不改浅层 / 零分配（`std::array` 的 `.size()` 断言 + `!contains` 动态容器 = 靠 review） / `sizeof(FrameTrace)` 上界 |
| `Logger` | ~6 | 分级过滤（低于阈值不写） / 追加语义 / **BOM 与表头只写一次** / 空路径不崩 / `IsOpen` / 同一行不多写 |
| `Overlay` | ~8 | 三块面板各自开关 / **行区间**（不碰 1~2 行） / **宽度 ≤ 80** / 字面量**不含 `@`/`E`/`*`**（★ 反向断言，C31 的机器守卫） / 数字跨位数不抖（定宽） / 不可见时**不写一个字节** / `SetEntityRows` 的 `count == 0` 不画 |
| 既有 197 条 | 197 | **一条都不许红**（Overlay 默认关是前提）；`Game` 侧因删 `F7` 而改的那几条要**逐条**说明改了哪一行、为什么 |

### 10.2 正向验收 + 人眼

```powershell
cmake -S . -B build                      # 新增源文件 ⇒ 必须先 configure
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\bin\Debug\MiniEngine.exe
```

**人眼（本阶段 8 条，登记进本表；S10 起人眼按阶段合并，一次走完）**：

| # | 看什么 | 期望现象 |
| --- | --- | --- |
| H1 | 按 `F1` | 出现时序表（`scope/avg/max/min/n`），**不盖住** HUD 两行、不折行；再按关闭后画面**逐字节**回到原样 |
| H2 | 看表里的数字 | **`Render`（或 `Present`）是最大项之一**（`PLAN` 的验收原话）；`total` ≈ 顶栏 `Work` |
| H3 | 看"Instrumentation / self"那一行 | **Profiler 自身开销 < 0.2ms**（`PLAN` 的硬指标 ⇒ 本阶段**人眼**判，不做机器断言，见 D13） |
| H4 | 按 `F2` | 实心格（墙/越界）用背景色标出 —— 一眼看出"为什么走不过去"（兑现 C13） |
| H5 | 按 `F3` | 实体表（id/状态/hp/位置 + `bullets=N`）；**取代** S8 的 `F7` 面板；按 `F7` **毫无反应** |
| H6 | 按 `F4` | 仍然写 `miniengine_debug.log`（语义不变），但**不重复**写 BOM/表头；用 `Get-Content` 能直接读（中文不乱码） |
| H7 | 连按 `F1/F2/F3` 各 20 次 | 不崩、不闪烁残留（擦行尾有效）、内存稳定（顶栏 `Work` 不单调上升） |
| H8 | 拖窗口到很窄 / 很矮 | Overlay 跟着裁剪，不越界、不把顶栏或 HUD 弄花 |

### 10.3 反向验证（预计 6 场；每条都必须看到红，跑完还原 + 重建）

| # | 破坏方式 | 应变红 |
| --- | --- | --- |
| R1 | `m_next = (m_next + 1)` **去掉取模** | `Profiler.WindowWraps`（越界写；大概率崩或算错） |
| R2 | `m_count` 直接 `++`（**未满时也当满**） | `Profiler.WindowNotFull`（分母错 ⇒ avg 偏小） |
| R3 | `ScopedTimer` 析构**不 `--depth`**（配对断） | `Profiler.DepthPairing`（嵌套全错） |
| R4 | Overlay 的 `firstRow` 改成 **1**（盖 HUD） | `Overlay.DoesNotTouchHudRows` + 既有 `Game.HudFits…` |
| R5 | ★ Overlay 文案里写 **`Enemy`**（含字母 `E`） | `Overlay.NoGlyphLetters` + **既有** `Game.RestartThreeTimesDoesNotAccumulate`（`CountChar('E')` 失真）—— **这一场验证 C31 的守卫真的会响** |
| R6 | Logger 每次 `Open` 都写 BOM/表头 | `Logger.HeaderWrittenOnce` |

### 10.4 交付后的自检

| # | 自检项 | 怎么查 |
| --- | --- | --- |
| 1 | **`engine/` 的每一处改动都能解释"为什么必须是引擎"** | `git diff <S10 末端>..HEAD -- engine/` 逐处过（本次**不再**是"零改动"） |
| 2 | Overlay/Profiler/Logger **都不认识 game** | `grep -rn "#include <game/\|game::" engine/` 应为 0 |
| 3 | 用例总数 | `mini_tests.exe Nope` 括号里 = 预期（~227） |
| 4 | 既有 197 条 | 一条不红；因删 `F7` 而改的那几条要能逐行解释 |
| 5 | Overlay 默认关闭 | `grep -n "Visible(Panel::" ` 初始值全 false；且**用例之外**没有地方默认打开 |
| 6 | HUD / Overlay 宽度 | 两处都 ≤ 80（数格式串） |
| 7 | 收尾抄录 | 干净构建 + 4 条命令的尾行照抄（S8/S9/S10 的教训：不抄 = 没留下"当时是多少"） |

---

## 11. 需要同步的文档

| # | 文档 | 改什么 |
| --- | --- | --- |
| 1 | `doc/MiniEngine_PLAN.md` | **S11 小节整节重写并标 ✅**（对齐 S8/S9/S10 的"实际落地"格式）· `A.2` 目录树加 `debug/Profiler.h`、`debug/DebugOverlay.h`、`core/Logger.h` 与 3 个测试文件 · **`G` 节 v0.4 标达成** · `A.6` 的容量约定按 Overlay 的新计数（若加 `bullets=N`）复核 |
| 2 | `doc/TECH_DEBT.md` | **关 C13**（F2 落地）· **关 C14**（dump 走 Logger）· **关 C15**（F7 面板删除）· **C23** 补"观察面已落地 ⇒ 可补用例"· **C31** 补"触发点已到，看 R5 的结果"· **新增**：Overlay 行区间常量（新的一处"多处同步"）、`PROFILE_SCOPE` 的静态期禁用（注释级纪律）、聚合精度/窗口长度这类"手感参数"· 加 **S11 进度段** |
| 3 | `doc/S11_CHECKLIST.md`（本文件） | 分叉拍板结果回填 + 进度板打勾 + 落地偏差追加 + §10 回填 |
| 4 | `doc/S5~S10_CHECKLIST.md` | **不改**（历史留档） |
| 5 | `AGENTS.md` | §2 文件表补 `doc/S11_CHECKLIST.md` 一行 |
| 6 | `README` | **仍不建**（S14 的事） |

---

## 12. 与 `PLAN` 原文的差异（写方案时就已知）

| # | PLAN 原稿 | 本施工图 | 理由 |
| --- | --- | --- | --- |
| 1 | "**F1~F4** 开关" | **F1 时序 / F2 实心格 / F3 实体表 / F4 = dump（语义不变）** | **F4 已被 `kDumpKey` 占用**（前置事实 9）⇒ 抢过来要动 HUD 提示 + 文档 + 人眼项，收益不值 |
| 2 | "终端**右侧**显示表格" | Overlay 占**第 4 行起**的左侧整段（可能扩到右侧） | 右侧分栏要处理"终端宽度不足 100 列"这类情况；本项目顶栏/HUD 都是整行风格 ⇒ 先统一 |
| 3 | "`PROFILE_SCOPE("Physics")`" | 名字用**本项目的**子系统名（`Input` / `Update` / `Render` / `Present` / `LimitFPS` / 以及 game 内的 `AI` / `Bullets`） | 本项目没有 `Physics` 这个名字（位移在 `Collision.h`，由 `m_world.Update` 驱动） |
| 4 | 验收"Profiler 自己开销 **< 0.2ms**" | **人眼指标**（H3），**不做**机器断言 | 任何测绝对时间的断言都会 flaky（D13 + 坑表 8） |
| 5 | "`F2` 显示碰撞盒"（`PLAN:549`） | 做成"**实心格高亮**"（背景色） | 本项目是**点模型**（1 格 = 1 字符），没有"盒"可画；"哪一格是墙"才是真问题（C13 的原话："1 格 = 1 字符下信息量≈0" —— 用背景色就不再是 0） |
| 6 | 没提 `F7` 面板的下场 | **删**（连同 `KeyCode.h` 的映射） | `TECH_DEBT` **C15** 的到期条件就是"S11 正式 Overlay 落地时" |

---

## 13. 学习点（写给自己）

| 主题 | 具体是什么 |
| --- | --- |
| **RAII 计时的完整形态** | 构造取时间 + 记深度，析构算差值 + 压栈 ⇒ 连异常路径都自动配对（本项目不用异常做控制流，但 RAII 的**对称性**是这一节的核心） |
| `thread_local` | 为什么它比"传引用"更适合打点（不打乱签名、不给 `ctx` 加通路）；它的两个真陷阱（静态期、析构顺序） |
| 环形缓冲 | "未满 / 环绕"两条路径；`min(count+1, N)` 的写法为什么比 `if (count < N) ++count` 更难写错；用 `.size()` 换"零分配"的证据 |
| **可观测性** | 没有数字就只能猜 —— 这是本阶段唯一"面向未来调试自己"的投资；也是求职时**能被问到"你怎么知道渲染慢"**的答案 |
| 分层纪律的极限测试 | 这一次引擎必须拿到游戏的**数据**（实体表）却**不能认识**游戏 ⇒ 用"行字符串"划界（同 `A.6` 的"容量约定"思路：**用接口形状控制耦合**） |
| 屏幕是唯一观察面（第二次） | C31 的触发点；"往屏幕上写字"在这套测试体系里是**危险动作**（这一次要在**设计阶段**就把守卫（文案禁令 + 默认关闭 + 行区间）做进去，而不是等红） |

---

## 14. 分叉（⏳ **待本人拍板** —— ⚠️ 本人 2026-10-12 明确："先不拍板，只记录"）

> ⛔ **这 5 条现在没有结论。** 下面每一行的"推荐"只是 **AI 的建议默认值**（写出来是为了以后要拍板时
> 有个起点），**不是**已经定下的方案。⚠️ 别在下次开工时把它当成"已经决定过"的东西。
>
> 📌 **为什么现在拍板值不高**（AI 的判断，供参考）：这 5 条都**不阻塞**任何已落纸的内容 ——
> 它们决定的是 §4 的数据结构细节与 §5 的文件形状；而本阶段**还没开工**，且 S10 的人眼验收
> （`S10_CHECKLIST` §10.5 的 28 条）**还没走完**。⇒ 真正的拍板时机 = **开工前那一次**
> （那时你对"想不想先看数字/先看实体表"的取舍会比现在清楚）。

| # | 问题 | 选项 | 推荐（**建议默认值，未采纳**） | 代价 |
| --- | --- | --- | --- | --- |
| ① | **Overlay 住哪 / 游戏数据怎么进去？** | 甲 引擎画时序表 + game 另画实体表（两块各自守宽度） · **乙 引擎统一排版，game 只填"行字符串"** · 丙 全放 game（引擎只暴露 metrics） | **乙** | 多一个 `DebugOverlay::Rows` 结构 + game 侧 16 个复用的 `std::string`；换来**一处排版 / 一处宽度守卫 / 引擎零游戏知识** |
| ② | **打点怎么进代码？** | 甲 `thread_local` + `PROFILE_SCOPE` 宏（PLAN 原稿） · 乙 显式传 `Profiler&`（要改 ctx 与每个函数签名） | **甲** | 宏 + `thread_local` 的两个坑（坑表 2）；换来 `FrameContext` **一个字段都不加**（硬规矩 2 不破） |
| ③ | **统计口径做到哪？** | 甲 min/avg/max（120 帧） · 乙 + P95（要直方图或排序） | **甲**（乙登记 C 组） | 均值会被尖峰掩盖；真需要再补，先别把面板做成"读不懂的一片数字" |
| ④ | **F4 冲突怎么解？** | 甲 F4 让位给 Overlay（dump 换键，HUD 提示/文档/人眼一起动） · **乙 F4 语义不变**，Overlay 用 F1/F2/F3 | **乙** | 与 `PLAN` 原稿的"F1~F4"不一致（已记 §12 差异 1）；换来**零涟漪** |
| ⑤ | **C13（F2 碰撞盒）本阶段做不做？** | 甲 做（背景色标实心格） · 乙 继续推迟 | **甲** | 一次地图遍历 + 背景色（成本很低）；它是 C13 的到期条件，且**"越界 = 墙"目前只能靠撞** |

> 拍板后把结论回填到本表（加一列"✅ 2026-xx-xx 拍板"），再按 §5 的 8 步开工。
