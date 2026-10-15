# S11 施工图 —— Debug Overlay + Logger + Profiler（v0.4「看得清」）

> **建立于 2026-10-14**。本文件 = S11 的施工图 + 进度板 + 待拍板分叉。
>
> 前置约定见 `AGENTS.md` §1：**默认只出方案、不写实现**；§1.4：**编译与运行由本人执行，
> AI 只做代码编辑**。所以本文件里的每一处都写成「谁做什么 → 跑什么命令 → 期望看到什么」。
>
> ⚠️ **本文件是第二版**：2026-10-12 曾按 `SHOWCASE_PLAN` 的「第 4 步」写过一版，
> 本人当天要求**回退**（原话「目前不需要具体确定」）⇒ 已删除。旧稿在 git 历史里：
> `git show c3ffa95:doc/S11_CHECKLIST.md`（或看 `c3ffa95` / `458d8be` 两个 commit）。
> **本版不沿用旧稿** —— 旧稿写的时候还没有 `TECH_DEBT` C23 / C31 这两条约束。
>
> 相关文档：`doc/MiniEngine_PLAN.md`（S11 小节 + G 节 v0.4 判定）、`doc/TECH_DEBT.md`、
> `doc/SHOWCASE_PLAN.md`（动作 4）、`README.md`。

---

## 0. 当前状态

> 状态图例：⬜ 待办 · 🔵 **待拍板**（本次落纸时尚未定）· ⏳ 待构建/待运行/待肉眼 · ✅ 已完成

| 项 | 状态 |
| --- | --- |
| 本文件 | ✅ **2026-10-14 落纸**（施工图 + 前置事实 + 坑表 + 测试计划 + 正反向验收 + commit 切分） |
| 方案评审 | ✅ **2026-10-15 审完并拍板** —— §14 的 4 个重点逐条有结论；§10 的 **A~E 全部定案**，并**新增分叉 F（取甲）** |
| 代码 | ⬜ **未开工**（本文件不含任何实现；**未构建、未运行** —— 按 `AGENTS.md` §1.4，构建与运行归本人）。⚠️ 2026-10-15：**拍板已完成，只等本人说「开工」** |
| 涉及引擎改动面 | `engine/core/`（新增 2 对 .h/.cpp）+ `engine/src/core/Application.cpp` 插桩 + 1 处注释修正 + **`engine/include/mini/platform/KeyCode.h`**（分叉 A 的新键 `T`：枚举 + `map[0x54]` + 改 `:31` 那句过期注释）；**`engine/render/` 不新增类** · **`IGame` 接口不动**（分叉 F 取甲） |
| 同时关闭的债 | C13（F2 几何层）· C14（快照搬出 `game/`）· C15（删 F7 临时面板）· C23（子弹数观察面）· C31 补一条机器守卫 |
| 规模预估 | engine ≈ 310 行 + game ≈ 120 行 + 测试 ≈ 15 条用例 ≈ **400 行量级**（与 `PLAN` 估的一致） |

---

## 1. 前置事实（本方案依赖的现状锚点，都可核）

| # | 事实 | 位置 |
| --- | --- | --- |
| 1 | 主循环帧序固定：`BeginFrame → [读尺寸/重建画布] → ctx 构造 → OnUpdate → OnRender → DrawStatsBar → Present → input.BeginFrame → EndFrame → LimitFPS` | `engine/src/core/Application.cpp:81-140` |
| 2 | `FrameContext` = 值快照：`deltaSeconds / frameMs / maxFrameMs / workMs / fps / screenWidth / screenHeight + const Input&`，**无可变通路** | `engine/include/mini/core/Application.h:74-84` |
| 3 | `workMs` 口径 = `BeginFrame` 到 `EndFrame`（**不含** `LimitFPS` 的 sleep） | `engine/include/mini/core/Time.h:138-161` |
| 4 | 已映射的键：`W A S D Q E R / Space Enter Escape Tab / 方向键 / F1 F2 F3 F4 / F7`；**F5、F6 未映射** | `engine/include/mini/platform/KeyCode.h:23-117` |
| 5 | `kHudRow = 2`（**文件内**常量，同时是"世界视口第一行"）；HUD 占行 1~2，世界视口 = 画布高 − 2 | `game/src/Game.cpp` 顶部 |
| 6 | 现行调试出口：`kDumpKey = F4`（手写 `std::ofstream` 追加写 `miniengine_debug.log`）、`kAiPanelKey = F7`（`DrawAiPanel`）、`kQuitKey = Q` | `game/src/Game.cpp` |
| 7 | `FrameBuffer::DrawRect` **生产代码 0 调用**（注释写着"服务 S12+ 的边框/HUD 面板"） | `engine/include/mini/render/FrameBuffer.h:45-47` |
| 8 | `Application.h:128` 注释预测"S11 的覆盖层开关也会从 `RequestQuit` 走" —— 与 §10 分叉 B 的推荐取法不一致，**要改** | `engine/include/mini/core/Application.h:128` |
| 9 | 用例 **197 条 / 13 suite**；`Game` suite 有多条靠"数屏幕字符"做判据（`CountChar(fb,'E')`） | `tests/test_game.cpp` |
| 10 | `engine` **绝不 include `game`**（单向依赖） | `AGENTS.md` §3 / `PLAN` A.3 |

---

## 2. 目标 / 明确不做什么

**目标（对齐 `PLAN` v0.4「看得清」）**：按一次键，屏幕右侧出现实时表，能**用数字回答**"这一帧的时间花在哪"；判定标准 = "能说出渲染占多少 ms"且"Profiler 自身开销 < 0.2ms"。

**做什么**：

1. `Logger`：分级（`Trace..Error`）+ 追加写文件 + RAII；成为 `game/` 里调试快照的**正式落点**（C14 的到期处置）。
2. `Profiler`：`PROFILE_SCOPE` RAII 计时 + **120 帧环形缓冲** + 可读的 `avg/max` 快照；关闭时近似零开销。
3. `DebugOverlay`：右上角浮层面板，内容 = 引擎帧数据 + Profiler 表 + 游戏计数（wave/foes/shots/ammo/hp）。
4. **清债**：C13（F2 几何层）、C14（快照搬出 `game/`）、C15（删 F7 临时面板）、C23（子弹数观察面）、C31（overlay 字形纪律 + 机器守卫）。

**明确不做什么**：

| 不做 | 理由 |
| --- | --- |
| ⛔ Chrome Trace JSON 导出 | `PLAN` H.2 明确排在"S11 之后" |
| ⛔ 日志轮转 / 多线程 / 异步 sink | 单线程、每日几十 KB 量级；无病吃药（同 C18/C25 的纪律） |
| ⛔ 引入 `std::format` / `{fmt}` | `AGENTS.md` §3 零依赖；Logger 收 `string_view`，由调用方拼 |
| ⛔ 动 C25 / C26（屏幕尺寸依赖、颜色归属） | 触发条件是"真有第二个后端"；现在不成立 |
| ⛔ 换碰撞实现 / DDA / A* | S12 / S14 的事 |
| ⛔ 给 `FrameContext` 加 `Logger&`/`Profiler&` 字段 | 会破坏"只给值快照"这条已定硬规矩（前置事实 2） |
| ⛔ **改 `IGame::OnRender` 的签名**（**分叉 F 取甲**，2026-10-15 拍板） | 那会把 `FrameContext`（最后一个是 `const Input&`）交到渲染路径手里 ⇒ **输入漏进 `OnRender`**，与 S5/C4 的既定契约冲突；而且它是 S11 里唯一会改引擎公共接口的分叉（§4.5 有详细对比） |
| ⛔ 把 overlay 的开关键写进 HUD 提示行（**分叉 A 拍板的附带决定**） | overlay 是**开发者工具**、不是玩法键 ⇒ 提示行只留"玩的时候真会用到"的键（该行已被 `HudFitsInTwoLinesWithoutOverflow` 精确钉住，改它要连列宽一起改） |

---

## 3. 文件清单

**新增（4 源文件 + 2 测试）**

| 文件 | 量级 | 内容 |
| --- | --- | --- |
| `engine/include/mini/core/Logger.h` | ~70 行 | 级别枚举 + `Logger` 类 + current 指针访问器 |
| `engine/src/core/Logger.cpp` | ~90 行 | 文件打开/追加、前缀拼接、flush |
| `engine/include/mini/core/Profiler.h` | ~80 行 | `Profiler` + `ScopedTimer` + `MINI_PROFILE_SCOPE` |
| `engine/src/core/Profiler.cpp` | ~70 行 | 累加、环形缓冲、`Snapshot` |
| `game/include/game/DebugOverlay.h` | ~60 行 | `OverlayData` + `BuildOverlayLines` + `DrawDebugOverlay` |
| `tests/test_logger.cpp` | ~5 用例 | 见 §6 |
| `tests/test_profiler.cpp` | ~7 用例 | 见 §6 |

**改动（9 处）**

| 文件 | 改什么 | 为什么必须改 |
| --- | --- | --- |
| `engine/CMakeLists.txt` | 加 `Logger.cpp` / `Profiler.cpp` | 否则不参与构建 |
| `engine/src/core/Application.cpp` | `Run` 里插 `BeginFrame/EndFrame` + 6 个 `PROFILE_SCOPE` | 计时点必须落在主循环 |
| `engine/include/mini/core/Application.h` | **改 `:128` 那条过期注释** | 它预测覆盖层开关走 `RequestQuit`，与 §10 分叉 B 取法冲突 |
| `engine/include/mini/render/FrameBuffer.h` | `DrawRect` 的"0 生产调用"注释 | S11 起它**有**生产调用者（前置事实 7） |
| `game/src/Game.cpp` | overlay 开关（**`T`**）+ 绘制 + `kDumpKey` 改走 Logger + 删 `DrawAiPanel` + **提示段删 `F7`**（§3.1）+ **`m_lastFrame` 快照**（§4.5） | 接线；提示段与键位必须**同一 commit 内**改完，否则屏幕上会留一个按了没反应的键 |
| `game/include/game/Game.h` | 加 `EnemyCount()/BulletCount()/OverlayOn()` 声明 + **私有 `FrameSnapshot m_lastFrame`**（§4.5） | 观察面（消 C23）+ 分叉 F 甲的落点 |
| `tests/CMakeLists.txt` | 加两个测试文件 | 否则不进 runner |
| `tests/test_game.cpp` | `SummarizeEnemyPanel` 相关判据改走新访问器 + **提示段精确串与 58/64 列宽断言**（§3.1 的 2/3 两条） | F7 面板删了、提示段字面量也改了 ⇒ 旧判据会**红一片**（坑 17 + `TECH_DEBT` C24） |
| `engine/include/mini/platform/KeyCode.h` | 加 `Key::T` + `map[0x54] = Key::T`；改 `:31` 那句 `F1, F2, F3, F4 // S11 的 Debug Overlay 用` | 不加映射 ⇒ 新键**收不到任何事件**（`KeyFromVirtualKey` 返回 `Unknown`）；注释不改 ⇒ 留下**假信息**（§3.1 第 6 条） |

### 3.1 分叉 A 的连带改动清单（**2026-10-15 补**）

⚠️ 施工图原稿只写了"删 F7 面板"**一处**。实际上 `F7` 现在**在 HUD 提示段里**（S10 步骤 6 加进去的）⇒ 改键位要扫 **6 处**：

| # | 位置 | 改什么 |
| --- | --- | --- |
| 1 | `game/src/Game.cpp` `DrawHud` 行 2 的字面量（`:1012-1015`） | 去掉 `F7`（**不加 `T`** —— 见下） |
| 2 | `tests/test_game.cpp:1381` | `MINI_CHECK(hint == "WASD SPC Q F4")` 的精确串 |
| 3 | `tests/test_game.cpp:1311-1312` | 两行真实宽度 **58 / 64 → 58 / 61**（删掉 `" F7"` 三个字符）。⚠️ **以落地时的实测为准** —— 这条断言本来就是"数出来的真实宽度"，不是推导出来的（C24 的纪律） |
| 4 | `game/src/Game.cpp:974-976` 的列宽注释 | 里面逐段数了 64 列，要重数 |
| 5 | `Game.cpp:996-1005`"提示段恰好那五个键"的**注释** | 段落理由跟着改（断言本体是第 2 条） |
| 6 | `engine/include/mini/platform/KeyCode.h:31` | `F1, F2, F3, F4, // S11 的 Debug Overlay 用` → 改成实际用到的键，并注明"F1/F3 在本机被别的软件抢占" |

**⚠️ 提示段不写 `T`（2026-10-15 与键位一起拍板）**：overlay 是**开发者工具**，不是玩法键。
好处：第 1~5 条全部退回"只删 `F7`"，改动面最小；代价：屏幕上没有 overlay 的提示，靠你记。
（`TECH_DEBT` **C15** 当年记的"面板键不在 HUD 提示行里"这条已因 S10 步骤 6 失效 —— 现在它在。）

---

## 4. 接口设计（只给声明，不含实现）

### 4.1 `mini/core/Logger.h`

```cpp
namespace mini::core {

enum class LogLevel : int { Trace, Debug, Info, Warn, Error, Off };
[[nodiscard]] const char* ToString(LogLevel lv) noexcept;   // "TRACE".."ERROR"

class Logger {
public:
    static constexpr const char* kDefaultPath = "miniengine_debug.log";  // 沿用 C14 的文件名

    explicit Logger(const char* path = kDefaultPath);  // 追加模式；打不开 → m_good=false
    ~Logger();                                          // RAII 关文件

    Logger(const Logger&)            = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&)                 = delete;
    Logger& operator=(Logger&&)      = delete;

    void SetLevel(LogLevel lv) noexcept;
    [[nodiscard]] LogLevel Level() const noexcept;
    [[nodiscard]] bool     Good() const noexcept;       // 文件真打开了？

    // 收**一整行**（不含 '\n'）；低于 level 直接丢。
    // 实现补 "[HH:MM:SS.mmm][LEVEL] " 前缀 + '\n' + flush（崩溃前不丢行）
    void Log(LogLevel lv, std::string_view line);

    void Flush();
private:
    std::ofstream m_out;
    LogLevel      m_level = LogLevel::Info;
    bool          m_good  = false;
};

// 进程级"当前 Logger"；未装 → Log 静默丢弃（同 Profiler 的模式）
void SetCurrentLogger(Logger* logger) noexcept;
[[nodiscard]] Logger* CurrentLogger() noexcept;

} // namespace mini::core
```

**行格式**：`[14:03:27.481][INFO] wave=2 foes=3 ammo=17`
**session 头**：构造后前 3 行写进程启动时间 / `VersionString()` / 日志级别。

### 4.2 `mini/core/Profiler.h`

```cpp
namespace mini::core {

using ProfileClock = std::chrono::steady_clock;

class Profiler {
public:
    static constexpr int kMaxScopes     = 8;    // 一帧最多 8 段（现用 7 个名字 + 1 余量）
    static constexpr int kHistoryFrames = 120;  // PLAN 原话：最近 120 帧

    void SetEnabled(bool on) noexcept;
    [[nodiscard]] bool Enabled() const noexcept;

    void BeginFrame() noexcept;                 // 帧首：清本帧累加器
    // 累加入口。**生产路径由 ScopedTimer 析构调用**，因此它是正式 API 而非测试专用：
    // 用例可注入已知 ms ⇒ 不用伪造时钟也能做确定性断言
    void AddSample(const char* name, float ms) noexcept;
    void EndFrame() noexcept;                   // 本帧累加推进环形缓冲（每帧恰好一次）

    struct Row { const char* name; float avgMs; float maxMs; };

    // 把最近 N 帧（N = min(已收帧数, kHistoryFrames)）写进**调用方数组**，返回条数。
    // 不分配、不返回内部指针（同 FrameBuffer::At 的取舍）
    int Snapshot(Row* out, int cap) const noexcept;

private:
    struct Acc { const char* name = nullptr; float sum = 0.0f; float max = 0.0f; };
    std::array<Acc, kMaxScopes>                               m_acc{};    // 本帧
    int                                                       m_used = 0; // 本帧已用段数
    std::array<const char*, kMaxScopes>                       m_names{};  // 按首现顺序固定
    std::array<std::array<float, kMaxScopes>, kHistoryFrames>  m_hist{};
    int  m_histCount = 0;
    int  m_histHead  = 0;
    int  m_depth     = 0;    // BeginScope/EndScope 配平自检
    int  m_dropped   = 0;    // 超 kMaxScopes 被丢的次数（不崩，只计数）
    bool m_enabled   = true;
};

// RAII：ctor 记名字 + 取时钟；dtor 算出 ms 交给 AddSample
class ScopedTimer {
public:
    explicit ScopedTimer(const char* name) noexcept;
    ~ScopedTimer();
    ScopedTimer(const ScopedTimer&)            = delete;
    ScopedTimer& operator=(const ScopedTimer&) = delete;
private:
    const char*              m_name;
    ProfileClock::time_point m_start;
};

void SetCurrentProfiler(Profiler* profiler) noexcept;
[[nodiscard]] Profiler* CurrentProfiler() noexcept;

} // namespace mini::core

// 展开 = 一个匿名 ScopedTimer（名字靠 __LINE__ 拼接）
#define MINI_PROFILE_SCOPE(name)
```

参考的 7 个段名（**必须全是字符串字面量**）：`Frame / Game.Update / Game.AI / Game.Bullets / Game.Render / StatsBar / Present`（`Input` 视需要第 8 个）。

### 4.3 `game/DebugOverlay.h`

```cpp
namespace game {

inline constexpr int kOverlayMaxLines = 14;
inline constexpr int kOverlayLineCap  = 48;

struct OverlayData {
    // 引擎侧
    float frameMs = 0, maxFrameMs = 0, workMs = 0, fps = 0;
    const mini::core::Profiler::Row* rows = nullptr;
    int   rowCount = 0;
    // 游戏侧
    int   wave = 0, foes = 0, shots = 0, ammo = 0, hp = 0;
    bool  geometry = false;
};

// 纯函数（决策）：把数据拍成文本行，**不碰 FrameBuffer**。返回行数（<= kOverlayMaxLines）。
int BuildOverlayLines(const OverlayData& d,
                      std::array<std::array<char, kOverlayLineCap>, kOverlayMaxLines>& out) noexcept;

// 接线：把那几行画成右上角面板（边框用 FrameBuffer::DrawRect）
void DrawDebugOverlay(mini::render::FrameBuffer& fb, const OverlayData& d) noexcept;

} // namespace game
```

**「决策与接线分家」**：`BuildOverlayLines` 无 IO、无 FrameBuffer ⇒ §6 的字形断言（C31）与行数断言都能**离线**做。

### 4.4 `Game` 新增的公开接口

```cpp
[[nodiscard]] int  EnemyCount() const noexcept;   // = m_enemies.size()，overlay 真用
[[nodiscard]] int  BulletCount() const noexcept;  // 存活子弹数（Find(id) != nullptr），overlay 真用
[[nodiscard]] bool OverlayOn() const noexcept;    // overlay 开关（给用例断言"默认关"）
```

⚠️ 三条都**不是"为测试而加"**：overlay 的表里真的显示它们。这也是 `TECH_DEBT` **C23** 的出路（"给一个公开观察面"）。

### 4.5 分叉 F 取甲：`Game` 的帧数据快照（**2026-10-15 新增**）

⚠️ **施工图原稿漏了一环**：`IGame::OnRender` **只有 `(render::FrameBuffer&)`**（`Application.h:99`）
⇒ `frameMs / maxFrameMs / workMs / fps` **送不进渲染路径**，§6 原来写的 `BuildOverlayData(ctx)` **根本写不出来**。

**取甲**：`Game` 在 `OnUpdate` 里拷一份**只读标量快照**，`OnRender` 读它。

```cpp
// game/Game.h（私有）
struct FrameSnapshot {                 // ⛔ 故意**不放** const Input& —— 那正是 C4 关掉的东西
    float fps = 0, frameMs = 0, maxFrameMs = 0, workMs = 0;
    int   screenWidth = 0, screenHeight = 0;
};
FrameSnapshot m_lastFrame{};
```

| 项 | 结论 |
| --- | --- |
| 数据新鲜度 | **引擎 4 项滞后一帧**（⚠️ 与顶栏 `Rows` 同款，见 §16.3 ② —— 已实测确认顶栏也滞后）；**游戏计数当帧**（`OnRender` 里现读 `m_enemies.size()`、`m_ammo` 等） |
| 为什么不用**乙**（把 `ctx` 传给 `OnRender`） | `FrameContext` 的最后一个成员是 **`const Input&`** ⇒ 等于**把输入交到渲染路径手里**，与 S5/C4 的既定契约（`Game.h:25-43`："`OnRender` 完全不碰输入"）冲突；且它是 S11 唯一会改引擎**公共接口**的分叉（连带 `tests/test_game.cpp` **8 处** `game.OnRender(fb)` 调用点） |
| 为什么不用**乙′**（新窄结构体，不带 Input） | 干净，但**多一个只有一处使用的公共结构体** ⇒ 同 C18/C25 那类"给不存在的病吃药" |
| 为什么不用**丙**（引擎画面板） | 引擎要画 `foes/shots` 就必须认识游戏概念 ⇒ 破坏单向依赖（`AGENTS.md` §3 / `PLAN` A.3）；只画 Profiler 那半则**功能残缺** |
| `engine/` 改动 | **0 行** ← 这是甲的主要价值（§3 的"engine 改动面小"因此重新成立） |
| Profiler 表从哪来 | **不需要 ctx**：`CurrentProfiler()->Snapshot(rows, cap)` —— §4.2 那两个全局访问器的**第一个真实用途** |
| 新增测试面 | **0 条**（`BuildOverlayLines` 是纯函数、不进这一层；`Game` suite 那 3 条只碰 `OverlayOn()` / `EnemyCount()` / `BulletCount()`） |
| 何时会后悔 | 若将来要让 overlay 显示"**本帧内**刚发生的东西"（例：本帧击杀数）—— 解法已内建：**引擎 4 项走滞后快照，游戏计数当帧现读** |

---

## 5. 数据结构与所有权

| 对象 | 谁拥有 | 生命周期 | 备注 |
| --- | --- | --- | --- |
| `Logger` | `Application` 成员 | 进程 | 构造时装到 current 指针，`~Application` 卸 |
| `Profiler` | `Application` 成员 | 进程 | 同上；**允许未装**（无 profiler 时 `PROFILE_SCOPE` 直接空转） |
| `ScopedTimer` | 栈 | 一个作用域 | 只存 `const char*` + 起点，**不拥有名字** |
| 段名字符串 | 调用方 | **静态存储** | Profiler 只存指针 ⇒ 头文件写明"只接受字面量"契约（坑 1） |
| overlay 文本 | 栈上定长 `char[14][48]` | 一帧 | 不缓存进 `Game`、不堆分配 |
| `m_showOverlay` / `m_showGeometry` | `Game` | **进程**（不随重开归零，同 `m_showAiPanel` 的作用域） | 纯"要不要画"，不参与游戏逻辑 |
| `FrameSnapshot m_lastFrame`（分叉 F 甲） | `Game` | **一帧**（每帧被 `OnUpdate` 覆盖） | **只读标量**：引擎 4 项滞后一帧、游戏计数当帧现读；⛔ 结构体里不放 `Input`（§4.5） |

Profiler 内存：`120 × 8 × 4B = 3.84 KB` + 小头 ⇒ 成员即可。

---

## 6. 帧序与接线（算法步骤）

```
① m_profiler.BeginFrame()                    ← Application::Run 帧首，紧跟 m_time.BeginFrame()
② 帧体 { MINI_PROFILE_SCOPE("Frame") }
     ├ 尺寸不可用 → continue 的分支**照旧**（不额外处理）
     ├ { "Game.Update" }  → game.OnUpdate(ctx)
     │     └ game 内部自加 { "Game.AI" } DriveEnemyAi、{ "Game.Bullets" } ResolveBullets
     ├ { "Game.Render" }  → game.OnRender(fb)
     ├ { "StatsBar"    }  → DrawStatsBar(ctx)
     └ { "Present"     }  → m_console->Present(fb)
③ m_profiler.EndFrame()                      ← 紧贴 m_time.EndFrame() **之前**（与 workMs 同口径）
④ m_time.EndFrame(); m_time.LimitFPS(...)    ← sleep 不在任何 scope 内
```

`Game::OnUpdate` **不新增帧序步骤**，只在既有"④ 按键处理"里加三条：
**`T` → `m_showOverlay` 取反** / `F2 → m_showGeometry 取反` / `F4 → 走 Logger 写快照`
（与 `④Q` / `④bF7` 同级，**冻结状态下照跑**，同 `④bF7` 的理由）。
⚠️ **不是 `F1` / `F3`** —— 两个都在本机被别的软件占用（`F1` 是 2026-10-15 的新证据，见 §3.1 / 坑 14）。
`OnUpdate` 里还多**一行**：拷 `m_lastFrame`（§4.5 分叉 F 甲）。
`Game::OnRender` 末尾：`if (m_showOverlay) DrawDebugOverlay(fb, BuildOverlayData(m_lastFrame, rows, rowCount));`
⚠️ **原稿这里写的 `BuildOverlayData(ctx)` 是错的**：`OnRender` 拿不到 `ctx`（`Application.h:99`）—— 见 §4.5。

**overlay 内容（示例形态；标签全部规避保留字形，见坑 11）**：

```
+--------------------------+
| frame 16.7  max 33.2     |
| work   1.42 fps  60      |
| wave 2   foes 3  shots 1 |
| hp 10    ammo 17         |
| -- profile (120f) --     |
| Frame        2.11 / 4.00 |
| Game.Update  0.62 / 1.10 |
| Game.AI      0.18 / 0.40 |
| Game.Render  0.44 / 0.90 |
| Present      0.31 / 0.80 |
+--------------------------+
```

---

## 7. 测试计划（**16 条**：Logger 5 + Profiler 7 + Game 4；**判据一律不写死数字**，能用常量算就用常量算 —— `S5_CHECKLIST` 10.4 的纪律）

### 7.1 Logger suite（5）

| 用例 | 判据 |
| --- | --- |
| `FiltersBelowLevel` | `SetLevel(Warn)` 后 `Info` 不写、`Warn` 写（读回文件内容） |
| `PrefixesLevelAndSingleNewline` | 每行以 `[` 开头、含 `ToString(lv)`、`'\n'` 计数 == 行数 |
| `AppendsAcrossInstances` | 同一路径构造两次 → 第一次内容**仍在**（守 C14 的"老文件还能读"） |
| `BadPathIsNotFatal` | 路径不可写 → `Good()==false`，`Log` 不抛不崩 |
| `CurrentAccessorRoundTrip` | 装/卸 current；未装时 `Log` 静默丢弃 |

### 7.2 Profiler suite（7）

| 用例 | 判据 |
| --- | --- |
| `AveragesOverWindow` | 喂 N 帧已知 ms ⇒ `Snapshot().avgMs` 对上 |
| `MaxIsTracked` | `maxMs` = 窗口内最大值 |
| `RingKeepsLastN` | 喂 `kHistoryFrames + 5` 帧 ⇒ 最早的 5 帧不再影响 avg |
| `PartialHistoryDividesByCount` | 只喂 3 帧 ⇒ avg 除 **3** 而非 120 |
| `OverflowDropsWithoutCrash` | 一帧登记 `kMaxScopes + 2` 段 ⇒ 不越界、不崩、`dropped` 计数 |
| `DisabledIsNoOp` | `SetEnabled(false)` ⇒ `AddSample`/`EndFrame` 不改任何数 |
| `UnbalancedScopeIsReported` | `m_depth > 0` 时 `EndFrame` 留下可观察标志 |

### 7.3 Game suite（4）

| 用例 | 判据 |
| --- | --- |
| `CountsMatchWorld` | 杀 1 个 ⇒ `foes` 减 1；开 1 枪 ⇒ `shots` 增 1（与 `EnemyCount/BulletCount` 一致） |
| `OverlayLinesAvoidReservedGlyphs` | 对 `BuildOverlayLines` 的**每一行**断言不含 `'@' / 'E' / '*'`（C31 的机器守卫） |
| `OverlayIsOffByDefault` | 不按键时 `OverlayOn()==false`，且世界行的屏幕字符数与按键前一致 |
| `OverlayTogglesWithT`（**2026-10-15 补，配分叉 A**） | 按 `T` ⇒ `OverlayOn()` 变 `true`、再按变 `false`；并在 `tests/test_input_state.cpp` 加一条 `KeyFromVirtualKey(0x54) == Key::T`（**镜像 S9 删 `K` 时的做法**：那时删的是那条断言） |

反向验证（预测 → 实测 → 比对 → 还原）**不属于用例，属于 §9 流程**。

---

## 8. 坑与边界（每条给"不处理会怎样"）

| # | 坑 | 不处理会怎样 |
| --- | --- | --- |
| 1 | 段名只存 `const char*` | 传 `std::string::c_str()` ⇒ 悬空指针，表格印乱码 |
| 2 | 一帧段数 > `kMaxScopes`(8) | 无守卫会越界写 `m_acc` ⇒ UB；取法是"丢 + 计数"，**绝不扩容** |
| 3 | `Begin/End` 不成对（提前 return） | 深度计数 + `EndFrame` 时自检；否则累加值静默错 |
| 4 | **嵌套会双算** | 规定"当前只用平铺"；嵌套时不把内层加到外层（注释 + 用例钉住） |
| 5 | 关闭时仍有开销 | 必须**先查 `Enabled()` 再取时钟**，否则"< 0.2ms"这条自证不成立 |
| 6 | `EndFrame` 放到 `LimitFPS` 之后 | Total 把 sleep 算进去 ⇒ 与 `workMs` 口径不符、数字对不上 |
| 7 | 尺寸变化帧提前 `continue` | 那一帧少几段；已知，`Snapshot` 按**实际收帧数**取平均 |
| 8 | 首帧历史不满 120 | 分母用 `min(已收, 120)`，否则首屏数字偏小一半 |
| 9 | overlay 超窄终端 | `x = max(0, fb.Width() - panelW)`，内容靠 `fb.Draw` 的"逐格越界忽略"裁 |
| 10 | overlay 层级 | 顶栏由 `Application` **最后**画（现有硬规矩，游戏盖不掉）；overlay 画在 `DrawHud` 之后 |
| 11 | **C31：overlay 写出 `@`/`E`/`*`** | `CountChar(fb,'E')` 静默多算 ⇒ 既有用例假红/假绿（不是红，是**算错**）。定死标签表 + §7.3 那条字形用例 |
| 12 | `F4` 写盘污染那一帧 `workMs` | 保持"只有手动按才写"；默认 `Info`，**绝不**每帧刷盘 |
| 13 | C14 的格式兼容 | 快照正文**保持逐行 `key=value`**，Logger 只加前缀 ⇒ 老文件仍可 grep |
| 14 | **F 行的键会被别的软件抢走**：`F3` 栽过两次；**`F1` 2026-10-15 实测也被占** | ⇒ overlay 改用**字母键 `T`**；`F3` 仍**不映射**。"按了没反应"是**假提示**（C15 的教训），留空比映射一个收不到的键好 |
| 15 | `DrawRect` 注释写"等 S12+" | S11 成了首个生产调用者 ⇒ 注释必须同步，否则后人当死代码删 |
| 16 | overlay 默认**关** | 保持关，既有 197 条"数屏幕字符"判据不受影响（这是 C31 的默认安全边界） |
| 17 | `SummarizeEnemyPanel`（`tests/test_game.cpp`）依赖 F7 面板 | 删面板会**红一片**；必须先让判据改走 `EnemyCount()`，**再**删面板（顺序不能反） |
| 18 | **新键 `T` 的可达性只能到 commit 4 才验** | 它需要一个**可观察效果**才有"收到没收到"的判据 ⇒ commit 4 的验收项里必须有"**按 `T` 出现/消失 overlay**"。万一也收不到 ⇒ 走 fallback 阶梯 `O` → `I` → `Tab` → `F6`（每个 +2 行） |
| 19 | `KeyCode.h:31` 注释写 `F1, F2, F3, F4 // S11 的 Debug Overlay 用` | 键位一改它就是**假信息**（"看起来配好了"那一类，同 A4b 的教训）⇒ 必须同步；实际只用到 `F2`（`F4` 是快照） |
| 20 | 改了 HUD 提示段字面量 ⇒ **列宽断言**（58 / 64）会红 | 那是**安全的失败方向**（C24）：按实测值改成 58 / 61 即可，**别以为是 bug**（§3.1 第 3 条） |
| 21 | **overlay 常驻会污染读数**：它自己每帧都在变 ⇒ 顶栏 `Rows` / `Work` 被永久抬高 | 所以 §16 的基线是**插桩前 + overlay 前**采的；对比数字时别把"开了 overlay 的 Work"与 §16.2 的 0.5ms 直接比 |

---

## 9. 验收标准

### 9.1 机器验收（本人跑，AI 按结果回填）

```powershell
cmake --build build --config Debug                     # 期望 0 warning
.\build\bin\Debug\mini_tests.exe                       # 期望 197 + 16 = 213 passed, 0 failed
ctest --test-dir build -C Debug --output-on-failure    # 期望 1/1
```

### 9.2 反向验证（预测 → 实测 → 比对 → 还原）

| 编号 | 破坏方式 | 预测（必须逐字核） |
| --- | --- | --- |
| **R1** | 去掉 Logger 的 level 过滤 | 低级别行出现 ⇒ `FiltersBelowLevel` 红 |
| **R2** | 注释掉 `Profiler::EndFrame()` | 表格恒 0/陈旧 ⇒ Profiler 用例红、**既有 197 条全绿**（证明是新守卫） |
| **R3** | overlay 文本里塞一个 `'E'` | §7.3 字形用例红 ⇒ 证明 C31 那条赛道真有人守 |
| **R4** | 关掉 `Enabled()` 短路 | `workMs` 上升、人眼可见 ⇒ 证明坑 5 是真的 |
| **R5** | 去掉 `kMaxScopes` 守卫 | 超限场景越界被用例抓住；**若抓不住，如实登记"没有机器守卫"**（同 C28/C32 的口径） |

### 9.3 人眼验收（登记进本文件的**合并清单**，验收步**一次走完**，不按步骤碎片化 —— `AGENTS.md` §1.4）

| # | 看什么 |
| --- | --- |
| 1 | **`T`** 开 overlay（⚠️ **不是 `F1`** —— `F1`/`F3` 本机被别的软件占）：右侧出现 `frame/max/work/fps` + profile 表，数字随场景变。★ **这一条同时是"新键没被别的软件抢走"的验证**（坑 18） |
| 2 | **v0.4 判定**：能指着表说出"渲染/更新占了多少 ms"；`Game.Render` 或 `Game.Update` 是最大项之一 |
| 3 | 开/关 overlay 前后，`work` 与顶栏 `Max` 的差 **< 0.2ms**（Profiler 自身开销）；⚠️ 对照物是 §16.2 的 **0.5ms**（那次是**没有 overlay** 时采的） |
| 4 | `F2`：实心格 / 实体占格用不同底色可见（C13） |
| 5 | `F4`：`miniengine_debug.log` 仍有完整 `key=` 行、且新增 `[时间][LEVEL]` 前缀（C14 未破坏） |
| 6 | `F7`：按下去**没反应**，且 **HUD 提示行里也不再出现 `F7`**（C15 删净：枚举映射、常量、面板表头提示、提示段字面量） |
| 7 | overlay 满屏时 `@` 仍可见（层级正确） |
| 8 | **开了 overlay 之后既有观察面不变**：世界行里 `@` / `E` / `*` 的个数与关掉时**一致**（C31 的肉眼侧；机器侧是 §7.3 的字形用例 + §9.2 的 R3） |

---

## 10. 拍板记录（**2026-10-15 定案**）—— 原「待拍板的 5 个分叉」

> 下半张表**保留原推荐与理由** —— 半年后回看"当时为什么这么选"比"选了什么"更重要。
> ⚠️ **2026-10-15 的新证据**：`F1` 在本机**也被别的软件占用**（`F3` 早有两次记录）⇒ A 的键位从 `F1` 改为字母键 **`T`**。

| 分叉 | 选项 | 推荐与理由 |
| --- | --- | --- |
| **A. 键位** | ① `F1`=overlay / `F2`=几何 / `F3`=**不映射** / `F4`=写快照 / 删 `F7`；② 照 `PLAN` 字面 "F4 切换"、把写快照挪别处 | **①**：F4 已是快照键、F3 本机被占（前置事实 4/6/14）；`PLAN` 原话与现状冲突，必须改一处 |
| **B. overlay 归属** | ① `game/DebugOverlay.h`（引擎 `render/` 不新增类）；② `engine/render/DebugOverlay` | **①**：面板内容（foes/shots）只有 game 知道；引擎侧抽"面板"只有一个调用者 = 给不存在的病吃药（同 C18/C25 纪律）。**代价**：`Application.h:128` 那条注释要改 |
| **C. 观察面** | ① 加 `EnemyCount()/BulletCount()`；② 继续解析面板文本 | **①**：一次性消掉 C23，并把 `tests/test_game.cpp` 从"数字符"迁走（判据更稳）。代价：两个新公开接口（但 overlay 真用） |
| **D. 日志文件** | ① 沿用 `miniengine_debug.log` + 前缀；② 另开 `miniengine.log` | **①**：C14 明写"格式别改、老文件要还能读" |
| **E. Profiler 可测性** | ① `AddSample(name,ms)` 走生产路径；② 注入时钟函数 | **①**：无"测试专用接口"，用例确定性也更好；② 会引入一个只有测试用的函数指针 |
| **F. overlay 的帧数据从哪来**（**2026-10-15 新增，原稿漏了**） | **甲** `Game` 缓存只读标量快照；**乙** 改 `IGame::OnRender` 签名传 `ctx`；**乙′** 传一个新的窄结构体；**丙** 引擎画面板 | **甲**：唯一"`engine/` 0 行 + 测试 0 处 + 不把 `Input` 漏进渲染路径"的选项；代价只有滞后一帧（与顶栏 `Rows` 同款，§16.3 ② 已实测）。详见 §4.5 |

### 拍板结论（2026-10-15，本人）

| 分叉 | 拍板 |
| --- | --- |
| **A** | ✅ **取 ①，但 overlay 改用 `T`**（`F1` 在本机也被占）⇒ 定案：**`T`=overlay · `F2`=几何 · `F4`=写快照 · `F3` 不映射 · 删 `F7`**；**提示行不加 `T`**；`PLAN` 的 S11 表改一处 |
| **B** | ✅ ① `game/DebugOverlay.h` |
| **C** | ✅ ① 加 `EnemyCount()/BulletCount()`（同时消 C23） |
| **D** | ✅ ① 沿用 `miniengine_debug.log` + 前缀 |
| **E** | ✅ ① `AddSample` 走生产路径 |
| **F** | ✅ **甲**（`Game` 侧只读快照；**不动 `IGame`**） |

⚠️ 两条连带：**A 有 6 处 ripple**（原稿只列 1 处 ⇒ 见 §3.1）· **F 是原稿漏掉的一环**（`OnRender` 拿不到 `ctx` ⇒ 见 §4.5）。
⇒ 原稿那句"这 5 条不定…无法冻结"到此解除：**§3 的文件清单、§7 的测试计划、§11 的 commit 切分现在都是冻结状态**。

---

## 11. commit 切分（每个都能编译能跑，3~6 个）

| # | commit | 内容 |
| --- | --- | --- |
| 1 | `feat: core/Logger（分级 + 写文件 + 用例）` | `Logger.h/.cpp` + `test_logger.cpp` + `engine/CMakeLists.txt` + `tests/CMakeLists.txt` |
| 2 | `feat: core/Profiler（累加器 + 120 帧环形缓冲 + 用例）` | `Profiler.h/.cpp` + `test_profiler.cpp` |
| 3 | `feat: Application 接线 PROFILE_SCOPE` | `Application.cpp` 插桩 + `Application.h:128` 注释修正 |
| 4 | `feat: game 侧 Debug Overlay（`T` 开关）+ EnemyCount/BulletCount` | `DebugOverlay.h` + `Game` 接线（含 §4.5 的 `m_lastFrame`）+ **`KeyCode.h` 的 `T` 映射** + `FrameSnapshot` 的填/读 + **4 条用例**（§7.3） |
| 5 | `refactor: C14 快照改走 Logger + 删 F7 面板（C15）+ F2 几何层（C13）` | ⚠️ **必须按坑 17 的顺序**：**先迁判据 → 再删面板**；**并把 §3.1 的 6 处 ripple 一起改完**（提示段字面量 / 精确串 / 58·61 列宽 / 两处注释 / `KeyCode.h:31`） |
| 6 | `docs: S11 收口（PLAN/TECH_DEBT/SHOWCASE/README/AGENTS + 版本 bump）` | — |

---

## 12. 需要同步的文档

| 文档 | 改什么 |
| --- | --- |
| `doc/MiniEngine_PLAN.md` | S11 小节标 ✅ + 落地偏差；G 节 v0.4 判定回填；⚠️ **S11 表的键位要按分叉 A 改**（`F1~F4 开关` / `F4 切换` → 实际的 `T` / `F2` / `F4`）—— 这是本阶段唯一与 `PLAN` 正面冲突处 |
| `doc/TECH_DEBT.md` | 关 **C13 / C14 / C15**；改 **C23**（观察面 = 新访问器）；更新 **C31**（overlay 纪律 + R3）；新增条目（"Logger 用 current 指针"、"overlay 数据走 Game 侧快照（分叉 F 甲）"等） |
| `doc/SHOWCASE_PLAN.md` | 动作 4 状态 + §2 / §5 |
| `README.md` | 操作表加 **`T`**（overlay）与 `F2`（几何）；状态行版本 → `0.4.0-dev` |
| `engine/src/core/Version.cpp` + `tests/test_smoke.cpp` | 版本 bump（**4 处落点 + 1 处举例注释**，同 C3 的口径） |
| `AGENTS.md` §2 | 本文件的一行（**落纸时已加**） |

---

## 13. 与 `PLAN` 的偏差预告（落地时登记进偏差表）

1. **`F1` / `F3` 都不启用，overlay 走字母键 `T`**：`PLAN` 写"F1~F4 开关"，它的 S11 表还写"F4 切换" ⇒ 实际是 **`T`**（overlay）/ **`F2`**（几何）/ **`F4`**（写快照）。⚠️ 依据是**本机实测**：`F3` 栽过两次、**`F1` 2026-10-15 也被占用**。
2. **overlay 放 `game/`**：`PLAN` 没写归属；按"引擎不认识游戏"落到 game 侧。
3. **不引 `std::format`**：`PLAN` 的 S11 表里列了它，实际用 `string_view` + 调用方拼接。
4. **`AddSample` 是正式 API**：`PLAN` 只写 `ScopedTimer`，本方案多一层以换取可测性。
5. **`Application.h:128` 的预测落空**：覆盖层开关**不走** `RequestQuit`（它是 game 侧状态）。
6. **`IGame` 接口不变**（分叉 F 甲）：`PLAN` 没写 overlay 的数据通路；本方案**不**改 `OnRender` 签名，改用 `Game` 侧只读快照。⚠️ 它同时是一次**修正** —— 原稿 §6 写的 `BuildOverlayData(ctx)` 在 `OnRender` 里**根本写不出来**。
7. **HUD 提示段不含 overlay 键**：`PLAN` 没说要不要进提示行；本方案**不进**（开发者工具；且那一行被 `HudFitsInTwoLinesWithoutOverflow` 的列宽断言钉住）。

---

## 14. 评审重点 —— **2026-10-15 已审，结论如下**

> 2026-10-14 本人要求：把方案与「审核内容」一起落纸，之后再决定。
> 2026-10-15 审完并拍板；每题保留**原问题的表述**，前面加**结论**。

1. **§10 分叉 A（键位）** ⇒ ✅ **取 ①，但 overlay 改用 `T`**（`F1` 本机也被占）。
   落地时必须**同时改 `PLAN` 的 S11 表**（唯一正面冲突处），并按 **§3.1 扫 6 处 ripple**。
2. **§4.2 的拆法**（累加器 / 时钟分家）⇒ ✅ **保留**。"是不是多余的一层"这个怀疑**不成立**：
   `AddSample` 在**生产路径**上（`ScopedTimer` 析构调它）⇒ 用例可以注入已知 ms 做**确定性**断言。
   砍掉它，Profiler 只能测"跑过没有"、测不到"数对不对" —— 同 `TECH_DEBT` **C34** 那种
   "看着有守卫、其实顶不到边界"的坑。
3. **§8 坑 11 + §9.2 的 R3（C31）** ⇒ ✅ **认，而且比原稿更硬**。§16.3 ① 刚实测证明
   "屏幕是共享命名空间"这类问题会**静默失真**（`Rows` 就是被顶栏自己污染的）。
   ⇒ overlay **默认关**（坑 16）+ 标签定死（用 `foes`/`shots`，**不写 `Enemy`**）+ §7.3 字形用例 + **R3 必须真跑**。
4. **§11 的 commit 5 顺序**（坑 17）⇒ ✅ **同意，且范围要扩**：除"先迁判据、再删面板"，
   还要带上 **§3.1 的 6 处 ripple**（至少提示段精确串与列宽断言），否则 commit 5 会红一片。
5. **（本步新增）§10 分叉 F**（overlay 的帧数据通路）⇒ ✅ **取甲**（§4.5）。
   ⚠️ 它是**原稿漏掉的一环**：不动它，commit 4 根本写不完。

---

## 15. 下一步

| 步 | 谁 | 做什么 |
| --- | --- | --- |
| 1 | 本人 | ✅ **已完成（2026-10-15）**：审 §14 的重点 + 拍 §10 的 A~E **与新增分叉 F**（记录见 §10 的「拍板结论」） |
| 2 | 本人 | ✅ **已完成（2026-10-15）**：说「开工」后按 §11 推进 —— ⚠️ **实际是 9 个 commit**（不是原稿的 6 个：落地时把"快照迁移 / 用例判据迁移 / 删 F7+几何层"拆开，见 §17） |
| 3 | 本人 | ✅ **已完成（2026-10-15）**：按 §12 同步文档 + 版本 bump 到 **`0.4.0-dev`** |

⚠️ **开工前的最后一道约定**（`AGENTS.md` §1.1）：**即使已经拍板，AI 也要先问一句「要我开始写代码吗」**再动源文件。

⚠️ 与展示层的关系：`SHOWCASE_PLAN` §2 明写「**不要为了做 1~3 而暂停 S11**」；本文件的落纸**不改任何代码**，也就不占 S11 的施工预算。

---

## 16. 插桩前基线（2026-10-15 采集 · `TECH_DEBT` A2 的落点）

> **为什么放进 S11 的施工图**：这组数是 S11 **唯一的对照物** —— Profiler 一旦插进主循环，
> `Work` / `Frame` 的口径就被改了（多出 clock 读取与帧末累加），**"插桩前"的数之后再也采不到**。
> 原落点（`TECH_DEBT` A2 节）写的是 `doc/S3_CHECKLIST.md` 10.2；**2026-10-15 本人拍板改落本节**
> （理由：S3 清单是历史留档，而消费者是 S11）。
> ⚠️ 数字来自**本人回报**（AI 不构建不运行，`AGENTS.md` §1.4）。

### 16.1 采集条件

| 项 | 值 |
| --- | --- |
| 机器 / 终端 / 窗口列数 | ⚠️ **未记录**（当时没抄；要跨机跨终端对比时补一次） |
| 画布行数（`Rows` 分母） | **18** ⇒ ⚠️ **这是"小窗口基线"，本组绝对值只在同一窗口尺寸下可比** |
| 运行参数 | 默认（`MiniEngine.exe`，目标 60fps，`main.cpp:28-30` 的 argv[1] 缺省） |
| 方法 | 目测采样。`Max` 直接读（单调、无需采样）；`FPS` 是**60 帧窗口平均**（`Time.h:47,79-83`）；`Frame`/`Work` 是**当帧瞬时值**（`Time.h:139-140,160-161`），只能采样 |
| 用例 | 197 passed, 0 failed |

### 16.2 读数

| 字段 | 读数 | 判读 |
| --- | --- | --- |
| `FPS` | **60** | 限帧命中（60 帧平均本来就稳） |
| `Frame` | **15ms** | 当帧瞬时值（⚠️ 与 `Max` 的量级很近，见 16.6 第 1 条） |
| `Max` | **17ms** | 自启动以来最差一帧 ≈ 常规帧 ⇒ **30 秒内无尖峰** |
| `Work` | **0.5ms** | ★ 16.7ms 的帧里只有 **3%** 在干活 ⇒ **≈97% 的帧时间是 sleep** |

**`Rows` 画像**（同一轮内四个 regime）

| regime | 实测 | 判读 |
| --- | --- | --- |
| 标题屏静止 | **1** | 只剩顶栏在变 ⇒ 见 16.3 ① |
| `Playing` 站立不动 | **2** | 顶栏 + HUD 行 1（`sumdt` 每帧累加：`Game.cpp:784-785`） |
| `ESC` 暂停 10s | **1** | ⇒ **"暂停时世界真的冻住"的屏幕侧证据（首次拿到）** —— 逻辑侧早有 13 条 `GameState` 用例 + 2 条 `static_assert` |
| 拖窗口 | 峰值 **9** / 分母 **18** | ⚠️ **未解释**，见 16.5 |

### 16.3 三条结构性发现（比数字本身更值钱）

① **`Rows` 的下界是 1，不可能是 0**。引擎顶栏画在**画布第 0 行**，而它自己就含
`FPS / Frame / Max / Work / Rows`（`Application.cpp:166-173`）⇒ **顶栏那一行每帧都脏**。
⇒ **`Rows` 不是"画面变没变"的干净观察面**：**不能用它证明"暂停冻住了"**
（本次采集前我正是这么推的 ⇒ 预测失误，见 16.5）。
⇒ 顺带一条：顶栏是**每帧固定的 1 行**写入开销。

② **顶栏的 `Rows n/n` 不是同一帧的快照**：分子 = `m_rowDiff.LastStats()`，**滞后一帧**
（`ConsoleWindows.cpp:279-284`，注释原话"顶栏差一帧无妨"）；分母 = **当帧** `ctx.screenHeight`
（`Application.cpp:172`）⇒ **尺寸变化前后必然会读到"混合对"**（例：上一帧画布 9 行、这一帧 18 行
⇒ 屏上显示 `9/18`）。**两个数各自都对，只是差一帧。**

③ **全量重绘那一帧只活 16ms**。尺寸一变 ⇒ `Invalidate()` ⇒ `dirtyAll` ⇒ `sent = rows`
（`ConsoleWindows.cpp:243` → `RowDiff.cpp:82`）；而 `AppendRowTo` **不裁行尾空格**、每行写满全宽
（`FrameBuffer.cpp:156-171`）⇒ 宽度一变，18 行里每一行都脏。
**这个峰值在 60fps 下只存在一帧，肉眼极易看漏。**

### 16.4 这组数怎么用

- **"Profiler 自身开销 < 0.2ms"** 这条自证：拿插桩后的 `Work` 与本节 **0.5ms** 比
  （`Work` 口径 = `BeginFrame→EndFrame`、**不含** sleep，`Time.h:128-129`）。
- ⚠️ **overlay 一旦常驻，它自己每帧都在变 ⇒ 会永久抬高 `Rows` 典型值与 `Work`**
  ⇒ 本节这组数**必须在 overlay 之前采**（已采）。
- `Work` 会被 Profiler 拆成 per-scope 的 `avg/max` 表 ⇒ 本表的 `Work` 是那张表的**总和对照物**。

### 16.5 未解释 / 未做（带触发条件，**不追**）

| 项 | 现状 | 触发条件 |
| --- | --- | --- |
| 拖窗口峰值 **9 / 18**（与 16.3 ③ 推算的 `18/18` 不符） | 两个候选：**H1** = ②的"分子滞后一帧"造成的混合对（最可能，且**不是 bug**）；**H2** = 真的只脏了 9 行（那 ③ 的推算要改） | **S11 的 overlay 落地后**真想量化"全屏重绘那一帧"时 —— overlay 的 `avg/max` 表比 `Rows` 可靠：`Rows` 这条路受"滞后一帧 + 峰值只 16ms"两个限制 |
| E1/E2（只拖**侧边**改宽、高度不动 ⇒ 预测 `18/18`） | 未跑 | 同上 |
| `MiniEngine.exe 300`（第 2 跑） | 未跑 | ⚠️ **价值已降低**：`Work / Frame = 0.5 / 16.7` 已直接证明"FPS 是被限帧托住的，不是工作变快" |

> ⚠️ 本次采集**踩过一条 `AGENTS.md` §7 的纪律**：`Rows` 那条线越挖越深（还差一次 E1 才收口），
> 实际耗时超出预算 ⇒ 按**40 分钟规则**用"最蠢的办法"收口：把未解释的那一项**登记成带触发条件的欠账**，
> 不再追。数字侧已够用（判据 = "有了以后能对比的量级"）。

### 16.6 读数陷阱（下次照抄时会踩）

1. **`Frame` 与 `Max` 都是 ms 且量级接近**（本次 15 / 17）⇒ 别把两列对调。本次**按顶栏字段顺序**
   记为 `Frame = 15 / Max = 17`；若实际相反，改这两个数即可（**不影响任何结论**）。
2. **`Frame` / `Work` 是当帧瞬时值**：60fps 下每 16ms 换一次数 ⇒ 只能采样，不能"读一次当典型值"。
3. **`Max` 是自启动以来**，生产代码**没有任何地方**调 `Reset()`（`Time.h:63-66`）⇒ 只增不降；
   启动头几帧的尖峰会被**永久**烙进去（`PLAN` 坑 24：**那是预期的，别当 bug 去消**）。
4. **不能拿 `MiniEngine.exe 10` 那一跑的数进基线**（`FPS≈10`、`Frame≈100ms`）——
   低帧率那次只用于"把 `Rows`/峰值看清"。
5. **两个进程的 `Max` 不可比**（各自从 0 起）。

⇒ **A2 关闭（2026-10-15）**：`TECH_DEBT` A2 翻 ✅，落点从 `S3_CHECKLIST` 10.2 改到本节。

---

## 17. 收口记录（2026-10-15）

### 17.1 实际 commit 序列 —— **9 个**（不是 §11 原稿的 6 个）

| # | commit | 内容 | 用例 |
| --- | --- | --- | --- |
| 0 | `d66da85` | `docs:` S11 施工图落纸 + `TECH_DEBT` A2 关闭（基线落 §16） | 197 |
| 1 | `a5a2d26` | `feat:` `core::Logger`（分级 + 前缀 + 追加 + 进程级 current） | **202** |
| 2 | `30afec2` | `feat:` `core::Profiler`（RAII 段计时 + 120 帧环形 + 快照） | **210** |
| 3 | `819926d` | `feat:` `Application` 接线（①b/⑦b + 引擎侧 5 段） | 210 |
| 4 | `4e93fa7` | `feat:` Debug Overlay（`T`）+ `EnemyCount/BulletCount` + 帧快照（分叉 F 甲） | **214** |
| 5 | `accd83b` | `refactor:` **5a** —— `DumpSnapshot` 改走 `core::Logger`（C14） | **215** |
| 6 | `2123b39` | `refactor:` **5b** —— 用例判据迁到 `EnemyCount()`（25 处） | 215 |
| 7 | `832735a` | `feat:` **5c** —— `F2` 几何层（底色通道，C13）+ overlay 加 `geom` | **216** |
| 8 | （本步） | `docs:` 收口（`PLAN` / `TECH_DEBT` / `README` / `SHOWCASE_PLAN` / 本文件 + 版本 bump） | 216 |

⚠️ **为什么多出 3 个**：原稿把"快照迁移 + 删 `F7` + 几何层"捆成一个 commit，
落地时按 `AGENTS.md` §7 的第三条纪律**先扫全部调用点**，发现"删 `F7`"会牵动
**4 条既有用例的判据迁移 + 25 个调用点** ⇒ 拆成三步，让每一步都能单独定位红点。
（这也让"5b 全绿"变成一次**语义等价性**的证明：用例数 215 → 215、行为零变化。）

### 17.2 拍板分叉的**实际**结果 —— 其中两条**推翻了原计划**

| 分叉 | 原稿 | **实际落地** | 依据 |
| --- | --- | --- | --- |
| **A 键位** | `F1`=overlay / `F2`=几何 / `F3` 不映射 / `F4`=快照 | ✅ 只剩"`F1` 改成 `T`" | 本机实测 `F1`/`F3` 都被别的软件抢占（§8 坑 18/19） |
| **B overlay 住哪** | 取①：住 `game/` | ✅ 照原稿 | 面板内容一半是游戏侧数据 |
| **C 观察面** | 取①：加 `EnemyCount/BulletCount` | ✅ 照原稿，并**进一步**：把 25 处用例判据也换成它（原稿只说"加"） | C23 建议的"公开观察面" |
| **D / E** | 见 §10 | ✅ 照原稿 | — |
| **F 帧数据回渲** | 取甲：`Game` 侧 `FrameSnapshot` | ✅ 照原稿 | `IGame::OnRender` 拿不到 `ctx`（§4.5） |
| **★ 推翻 1：`F7` 面板** | "正式 Overlay 落地时**删**"（C15） | 🔄 **不删，降级为"常驻列表类调试工具"** | ① 它扛着两条无处可搬的守卫（`aliveTiles` / `<id gone>`）；② `T` 是**摘要**、它是**列表**，不是子集；③ 本人实测"排障里确实有用"。⇒ `TECH_DEBT` C15 的"临时出路"定性失效 |
| **★ 推翻 2：C13 的"碰撞盒"** | `PLAN:549` 字面的"显示碰撞盒"（S7 判过"不做"：1 格 1 字符下信息量≈0） | ✅ **改走底色通道**：实心格暗灰底 / 玩家蓝底 / 敌人红底，**字形一个字节不动** | S7 那条理由针对**字符**通道 ⇒ 换通道后理由不成立 |
| **新增** | — | **C37**：`T`/`F2` 的**物理可达性**没有机器守卫（用例走 `FakeInput`，绕过真实键盘） | `F1`/`F3` 各被抢两次的历史 |

### 17.3 落地偏差（施工图 → 现实）

| # | 施工图写的 | 实际 | 后果 |
| --- | --- | --- | --- |
| 1 | `Application` 里 **6 个** `PROFILE_SCOPE`（§3） | **引擎侧 5 个**（`Frame`/`Game.Update`/`Game.Render`/`StatsBar`/`Present`）+ **游戏侧 2 个**（`Game.AI`/`Game.Bullets`） | §3 的"6"是笔误；总段数 7 |
| 2 | §6 写 `BuildOverlayData(ctx)` | **写不出来** —— `IGame::OnRender` 只收 `FrameBuffer&` | ⇒ 分叉 F 甲（`m_lastFrame` 快照，**慢一帧**） |
| 3 | §6/坑 7 说 resize 分支"照旧 `continue`" | **代码里没有 `continue`**（只是不接受新尺寸） | ⇒ `EndFrame` 每帧必然被执行一次（本阶段的"①b/⑦b 同拍"契约靠它成立） |
| 4 | `Application.h` 那处 `RequestQuit` 注释 | **两处都错**（不只"s11 覆盖层不走这里"；"S10 死亡重开走这里"也不成立） | 已更正 → `RequestQuit` 的真正入口只有三条 |
| 5 | `KeyCode.h:31` "`F1, F2, F3, F4 // S11 的 Debug Overlay 用`" | 实际只用 `F2`（+`F4` 是快照），`F1`/`F3` **本机收不到** | 已更正（否则是"看起来配好了"的假信息） |
| 6 | （施工图没提）`FrameBuffer.h` 的 `DrawRect` 注释 | 原写"生产代码 0 调用，等 S12+" | S11 起它是 **overlay 边框的第一个生产调用者** ⇒ 注释已改 |
| 7 | （施工图没提）C14 迁移的连带 | 删 `kDumpFileName`（文件名归 `Logger::kDefaultPath`）、`<fstream>`/`<filesystem>` 退役、字段说明从"6 行中文 + BOM"改成"2 行纯 ASCII" | 老日志段落**一行未动**（追加模式）⇒ 仍可读 |
| 8 | （施工图没提）5b 的改动面 | **只动测试**（原以为要改 `Game.cpp` 18 处） | 那 18 处是"删 `F7`"才需要的 ripple ⇒ 随推翻 1 一起作废 |
| 9 | §7 计划 16 条新用例 | 实际 **19 条**（197 → 216）：Logger 5 · Profiler 8 · overlay 4 · dump 走 Logger 1 · 几何层 1 | 多出来的是"防回归靶子"（字形守卫 / 未装即丢弃 / 只碰颜色通道） |
| 10 | 人眼预期里的 `-- profile (Nf) --` "会从很小的数慢慢涨到 120" | ❌ **错**：窗口与段名表是**进程级**累积，且**面板在 `Title` 屏不画** ⇒ 你看到它时它必然已是 120 | 记录见 §16.6 第 6~8 条（"读数陷阱"） |
| 11 | 人眼预期里的"表会先 5 行、进 `Playing` 后变 7 行" | ❌ **错**：面板第一次可见时 `Game.AI`/`Game.Bullets` **当帧就已登记** ⇒ 恒 7 行 | 同上 |

### 17.4 读数陷阱（**追加到 §16.6**）

6. **`dump=` 是"进程内"计数器** ⇒ 跨进程会从头数（同一个文件里会出现两个 `dump=1`）。
   **找边界看 `session start`**（每次启动 exe 写 3 行头）。
7. **`work` / `ms` / `max` 跨画布不可比**（成本 ∝ 画布面积）⇒ 比"插桩前后"必须**同画布**。
   §16.2 那组数（`work 0.5ms`）采于**小窗口**，与后续任何大窗口读数**不可直接相减**。
8. **overlay 的引擎侧数字慢一帧**（分叉 F 甲：`m_lastFrame` 是上一帧 `OnUpdate` 拷的）——
   与顶栏 `Rows` 同病。⇒ 别拿"面板里的 `work`"与"顶栏同一时刻的 `Work`"逐位对。
9. **`geom on` 的切换那一帧会有一批行变脏**（`RowDiff` 按含色码的行字符串比较）⇒
   按 `F2` 的瞬间 `Rows` 会跳一下、`Max` 可能被顶；这是**切换成本**，不是持续成本。

### 17.5 收口状态

| 项 | 值 |
| --- | --- |
| 用例 | 197 → **216**（`Game` suite 33 → **39**；suite **21 → 23**，新增 `Logger` / `Profiler`） |
| ⚠️ 顺带更正一处**长期存在的错数** | `README` 与 `TECH_DEBT` 从 S10 起写的"**13 个 suite"**一直是错的** —— 按 `tests/*.cpp` 里的 suite 名实测：**S10 时是 21 个**，S11 后 **23 个**（`grep -rhoE "MINI_TEST(_F)?\([A-Za-z_]+," tests/*.cpp \| sed 's/,//' \| sort -u \| wc -l`）。已在本步的 `README` 与 `TECH_DEBT` 里改成实数。⚠️ **"用例数 197" 那个数是对的**（`grep -rhoE "MINI_TEST(_F)?\(" tests/*.cpp \| wc -l`），只有 suite 数错 |
| 生产代码 | `engine/` +4 文件（`Logger.h/.cpp`、`Profiler.h/.cpp`）· `game/` +1 文件（`DebugOverlay.h`）· `Application`/`Game` 插桩 |
| 债 | **关** C13 / C14 / C23 · **关掉一半** C31 · **改判** C15 · **新增** C37 |
| 版本 | `0.4.0-dev`（四处落点 + 一处举例注释，全改） |
| v0.4 判定 | ✅ **达成** —— 按 `T` 能指着面板说出"渲染占多少 ms"（`Game.Render` / `Present` 是两行的数字） |

### 17.6 ⚠️ 还没做的（**别把"代码落库"当"阶段收口"**）

| 项 | 谁 | 说明 |
| --- | --- | --- |
| **5 场反向验证 R1~R5**（§9.2） | **本人** | 按 S10 的纪律："**凡是新增防回归的测试，验收里必须带一次反向验证**"。R1 前缀/级别 · R2 注释掉 `Profiler::EndFrame` · R3 往 overlay 塞一个 `'E'` · R4 关掉 `Enabled()` 短路 · R5 去掉 `kMaxScopes` 守卫 ⇒ 预测红集 → 实测 → 逐条比对 → 还原 | 
| **§9.3 的人眼验收合并清单** | **本人** | 各步只是**逐条登记**，还没在收口时**一次走完**（`T`/`F2`/`F7` 三个出口的层级、暂停时读数冻结、小窗口下 HUD 折行等） |
| **push** | **本人** | 本地已累计 9 个 commit 未推（CI 会在推送后跑一次，`ci.yml` 的 runner 是 `windows-2022`） |

> ⇒ 本文件的**"收口"只指"代码与文档落库 + 机器验收全绿"**；上面这三项走完才算 `TECH_DEBT` 第 6 轮真正关闭。
