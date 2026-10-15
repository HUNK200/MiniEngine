#pragma once

#include "mini/core/Logger.h"     // S11：它也是 Application 的**成员** ⇒ 要完整定义
#include "mini/core/Profiler.h"   // S11：它是 Application 的**成员** ⇒ 要完整定义
#include "mini/core/Time.h"
#include "mini/platform/Console.h"
#include "mini/render/FrameBuffer.h"

#include <memory>

// ============================================================
// core::Application —— 引擎骨架（S3）
//
// 从前到后一条线：**拥有子系统 → 跑循环 → 不认识具体游戏**。
//
//   main 只有几十行：造一个 IGame，交给 Application::Run()
//   Application 负责：时间、输入推进、画布尺寸对齐、提交、退出判定、顶栏、
//                     **分段计时（S11）**、**分级日志（S11）**
//   game 只负责：读 ctx 更新自己的状态 + 往 FrameBuffer 上画
//
// 三条硬规矩（都是踩出来的，别改）：
//   1. `IGame` 只持**引用**，Application 不拥有它 —— 游戏对象的生命周期归调用方
//   2. `FrameContext` **不含 `Time&`** —— 游戏拿到可变 Time 就能调 EndFrame/LimitFPS
//      把循环时序搞乱，而且没有编译器拦得住。只给值快照。
//   3. 顶栏由 Application 在 `game.OnRender` **之后**画，游戏盖不掉它
//
// 依赖方向：core → platform（同层内已有的边）、core → render。不认识 game。
// ============================================================

namespace mini::platform {
class Input;   // FrameContext 只存它的引用，不需要完整定义
}

namespace mini::core {

// ── 画布尺寸的谓词（2026-10-02 加；TECH_DEBT C7 收窄的产物）────────────────────
//
// 为什么是自由函数、而且放**公共头**：`Application::Run` 第 ③ 步那个"这一帧的尺寸要不要接受"
// 分支是**真逻辑** —— 它决定 `FrameContext::screenWidth/Height`，而 S6 起**相机每帧都读它**。
// 但它以前写在 `.cpp` 的匿名 namespace 里，测试碰不到，只能靠"拖到极窄不崩"肉眼兜。
// 抽成 `constexpr` 自由函数之后，边界值可以用机器钉住（见 `tests/test_smoke.cpp` 的 `Core` 组）。
//
// ⚠️ **两条路径的语义不同，不要把这两个函数合成一个整体判断**：
//   · 启动时 `ResolveCanvasSize` 是**逐轴**判断：不可用的那一轴**替换**成兜底 80/25
//     —— 例：(100, 1) → (100, 25)，**宽不动**
//   · resize 时 `Run` 第 ③ 步是**两轴一起**判断：不够大就**跳过这一帧**（保留旧尺寸）
// 合成 `if (!IsUsableCanvasSize(w,h)) { w = 80; h = 25; }` 会把上面那个例子变成 (80, 25)，
// 属于**行为变更**（S6 之后相机吃的就是这个尺寸）。
inline constexpr int kMinUsableCanvasSide = 2;   // 1 列/1 行画不出东西；FrameBuffer 也拒绝非正尺寸

[[nodiscard]] constexpr bool IsUsableCanvasSide(int side) noexcept
{
    return side >= kMinUsableCanvasSide;
}

[[nodiscard]] constexpr bool IsUsableCanvasSize(int w, int h) noexcept
{
    return IsUsableCanvasSide(w) && IsUsableCanvasSide(h);
}

struct AppConfig {
    // 画布尺寸。**0 = 用终端可视区**（推荐）。终端读不到尺寸时退回 80x25。
    int width  = 0;
    int height = 0;

    // 目标帧率。<= 0 表示**不限帧**（顶栏会显示几百帧，用于验证限帧真的在限帧）
    int targetFps = 60;

    // 是否画顶栏。顶栏占**第 0 行**，游戏应该从第 1 行开始画。
    bool showStats = true;
};

// 一帧的输入快照。**只有值和 const 引用**，没有可变通路。
//
// 它是个聚合体（没有构造函数），所以每帧这样就地构造：
//     const FrameContext ctx{ dt, frameMs, maxMs, workMs, fps, w, h, *m_input };
struct FrameContext {
    float deltaSeconds = 0.0f;   // 已 clamp 的帧间隔（秒）→ 喂给游戏做积分
    float frameMs      = 0.0f;   // 同一个间隔，毫秒，**未 clamp** → 顶栏用（要看得到卡顿）
    float maxFrameMs   = 0.0f;   // 自启动以来最差的那一帧（毫秒，同样未 clamp）→ 顶栏 Max
    float workMs       = 0.0f;   // 本帧实际干活耗时（毫秒）
    float fps          = 0.0f;   // 滑动窗口平均
    int   screenWidth  = 0;      // 本帧画布尺寸（resize 那一帧就已经是新值）
    int   screenHeight = 0;

    const platform::Input& input;
};

// 游戏侧唯一要实现的东西。引擎不认识 Player / Enemy，只认识这五个函数。
class IGame {
public:
    virtual ~IGame() = default;

    // 循环开始前调一次。用来建初始状态（S5 起会建 World）
    virtual void OnStart() = 0;

    // 每帧更新。**别在这里画东西**，画在 OnRender 里
    virtual void OnUpdate(const FrameContext& ctx) = 0;

    // 每帧画。进来时 FrameBuffer 已经 Clear 过了，直接往上画即可。
    // 不要试图在这里碰终端 —— 提交是 Application 的事。
    virtual void OnRender(render::FrameBuffer& fb) = 0;

    // 游戏自己的退出条件（比如 S10 里"死亡后按 R 重开"以外的那些）。
    // 注意：Application 每帧都会问它，所以**不要**在这里做有副作用的判断。
    [[nodiscard]] virtual bool ShouldQuit() const = 0;
};

class Application {
public:
    // 构造顺序：Console → Input → 画布尺寸 → FrameBuffer。
    // Console 必须最先（输入模式由它设，画布尺寸也从它读）。
    //
    // stdout / stdin 不是控制台时构造函数会抛（S2 已有的行为），由 main 捕获。
    explicit Application(AppConfig cfg = {});
    ~Application();   // RAII：恢复终端、显光标、离开备用屏

    // 持有 unique_ptr + "独占整个终端"的语义 → 明确禁掉拷贝与移动。
    // 显式写出来，是为了让"误拷贝"的报错一眼能看懂，而不是一屏模板错误。
    Application(const Application&)            = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&)                 = delete;
    Application& operator=(Application&&)      = delete;

    // 跑主循环直到退出。**阻塞**。
    // game 的生命周期由调用方保证：Run 返回前不能析构。
    void Run(IGame& game);

    // 统一退出入口。谁想让循环停下来都走这里：
    //   终端死了（Present 返回 false）、输入说该退了（Ctrl+C / 关窗口）、游戏说该退了。
    //
    // ⚠️ **2026-10-15 更正**：这里原先写着"S10 的死亡重开、S11 的覆盖层开关也会从这里走" ——
    //    **两条都不成立**：
    //      · 死亡 → 重开（`R` 键）与覆盖层开关（`T` 键）都是**纯 game 侧状态**
    //        （`Game::m_state` / `Game::m_showOverlay`），一次都不碰这条边；
    //      · 真正走这条边的只有三条：终端没了、进程级 Ctrl+C/关窗口、以及
    //        `game.ShouldQuit()`（`Q` 键那条）。
    //    这条更正与 `S11_CHECKLIST` §12/§13 记的"过期注释"是同一处。
    void RequestQuit() noexcept { m_quit = true; }

    [[nodiscard]] bool QuitRequested() const noexcept { return m_quit; }

private:
    // 顶栏：画在第 0 行。**必须在 game.OnRender 之后调**，否则游戏能盖掉它
    void DrawStatsBar(const FrameContext& ctx);

    AppConfig                          m_cfg;
    std::unique_ptr<platform::Console> m_console;   // 先于 Input：输入模式由它设
    std::unique_ptr<platform::Input>   m_input;
    platform::ConsoleSize              m_size{};    // 先于 FrameBuffer：画布尺寸来源
    render::FrameBuffer                m_frameBuffer;
    Time                               m_time;

    // S11：分级日志（`TECH_DEBT` C14 的**正式落点**）。与 `m_profiler` 同一个手法 ——
    // 构造时把自己装到进程级 current 指针上，于是 `game/`（甚至以后别的模块）不必拿到
    // `Application` 就能写日志。
    // ⚠️ 它的构造函数会**追加写 3 行 session 头**（时间 + 版本 + 级别）⇒ 每跑一次 exe，
    //    日志里都会留一条"这一段是哪个进程写的"。旧文件不受影响（追加模式）。
    // ⚠️ 声明顺序有意排在 `m_console` **之后**：`console` 构造失败会抛（不是控制台就退出）
    //    那一路**不该**留下一条 session 头 —— 日志文件里不该出现"根本没跑起来"的段落。
    Logger                             m_logger;

    // S11：分段计时器。`Application` 构造时把自己**装到进程级 current 指针**上
    //（见 .cpp），于是 `game/` 里的 `MINI_PROFILE_SCOPE` 不必拿到 Application 就能记账。
    // ⚠️ 它**不进 `FrameContext`** —— 那条"只给值快照"的硬规矩不破：覆盖层要显示的
    //    引擎帧数据由分支 F 甲的 `Game` 侧快照带回渲染路径（`S11_CHECKLIST` §4.5）。
    Profiler                           m_profiler;

    bool                               m_quit = false;
};

} // namespace mini::core
