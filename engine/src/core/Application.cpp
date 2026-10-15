#include "mini/core/Application.h"

#include "mini/platform/Input.h"

#include <cstdio>
#include <cstring>

// ============================================================
// core::Application 实现（S3）
//
// 整个循环在 Run() 里，顺序是硬约束（S3_CHECKLIST 第 6.5 节）：
//
//   ① m_time.BeginFrame()      ← 帧首，早于一切。放到别处会让 dt 只量"干活耗时"
//   ①b m_profiler.BeginFrame() ← S11：与帧首**同一拍**（两者的"帧首"必须是同一个点，
//                                否则表格里各段之和与顶栏的 `Work` 对不上）
//   ② m_input->Poll()          ← 抓事件，让本帧能看见按键
//   ③ 对齐画布尺寸             ← 必须在 game.OnUpdate **之前**，否则游戏这一帧
//                                拿到旧尺寸算出来的位置
//   ④ game.OnUpdate(ctx)
//   ⑤ Clear → game.OnRender → 顶栏
//   ⑥ Present（失败 = 终端没了 → 请求退出）
//   ⑦ 输入帧尾推进 + EndFrame + LimitFPS
//      └ ⑦b m_profiler.EndFrame() 插在 `m_input->BeginFrame()` 与 `m_time.EndFrame()` **之间**
//         （S11）—— 这样它和 `Work` 是同一口径；**绝不能**挪到 `LimitFPS` 之后，
//         那会把 sleep 算进每一段（`S11_CHECKLIST` §8 坑 6）
//   ⑧ 退出判定（放最后：本帧该干的活一件都不能少）
//
// S11 加进来的 5 个 PROFILE_SCOPE（都在 Run 里，花括号即作用域）：
//   Frame（②~⑥ 整段）· Game.Update · Game.Render · StatsBar · Present
//   ⚠️ 另外两个段名（Game.AI / Game.Bullets）在 `game/` 里 —— 本文件看不到它们，
//      `engine` 不认识游戏（`PLAN` A.3）。
//
// 本文件**不出现任何 `<windows.h>`**：终端细节全在 platform 里。
// ============================================================

namespace mini::core {
namespace {

// 顶栏固定画在第 0 行。游戏应该把这一行让出来（见 AppConfig::showStats）
constexpr int kStatsRow = 0;

// 终端读不到尺寸时的兜底。80x25 是老终端的默认值，拿来当"最小可用画面"
constexpr int kFallbackWidth  = 80;
constexpr int kFallbackHeight = 25;

// ⚠️ "最小可用尺寸"原来是这里的两个常量（`kMinUsableWidth/Height = 2`），2026-10-02 搬到了
//    `core/Application.h` 的 `kMinUsableCanvasSide` + 两个谓词 —— 因为下面两处用法**语义不同**
//    却共用同一个数，而测试需要能碰到它们（TECH_DEBT C7 收窄）。见 Application.h 的说明。

// 画布尺寸：配置里显式给了就用它，否则用终端可视区，再不行退回 80x25。
//
// 抽成自由函数是为了在**成员初始化列表里**用：FrameBuffer 没有默认构造函数，
// 必须在列表里就拿到尺寸。它只依赖 m_cfg 与 m_console（都声明在前面，已初始化）。
platform::ConsoleSize ResolveCanvasSize(const AppConfig& cfg, const platform::Console& console)
{
    const platform::ConsoleSize term = console.Size();

    int w = (cfg.width  > 0) ? cfg.width  : term.width;
    int h = (cfg.height > 0) ? cfg.height : term.height;

    // ⚠️ **逐轴替换**：只有"那一轴"不可用时才换成兜底值 —— 不是整体判断。
    //    例：(100, 1) → (100, 25)（宽保持 100）。写成 `if (!IsUsableCanvasSize(w,h))` 会
    //    把宽也换成 80，那是行为变更（S6 之后相机吃的就是这个尺寸）。见 Application.h。
    if (!IsUsableCanvasSide(w)) {
        w = kFallbackWidth;
    }
    if (!IsUsableCanvasSide(h)) {
        h = kFallbackHeight;
    }
    return platform::ConsoleSize{ w, h };
}

} // namespace

Application::Application(AppConfig cfg)
    : m_cfg(cfg)
      // 顺序不能反：输入模式（关 QUICK_EDIT、开 WINDOW_INPUT）是 Console 构造时设的，
      // Input 只负责读事件。反过来会在 Input 构造时抛异常。
    , m_console(platform::MakeConsole())
    , m_input(platform::MakeInput())
      // 尺寸这两个都依赖 *m_console，所以必须排在它后面（声明顺序也是这个顺序）
    , m_size(ResolveCanvasSize(m_cfg, *m_console))
    , m_frameBuffer(m_size.width, m_size.height)
{
    // 构造完就进备用屏、隐藏光标（Console 构造函数里做的）。
    // 之后由 main 负责保证这个对象活得够久 —— 它的析构就是"恢复终端"。

    // S11：把自己装成"进程级 current profiler"，这样 `game/` 里的 `MINI_PROFILE_SCOPE`
    // 不必拿到 Application 就能记账。
    // ⚠️ 未装时那些宏**空转** —— 单测里就是这种情形（`tests/test_profiler.cpp` 最后一条
    //    专门钉了这条路径），所以它必须是安全的。
    SetCurrentProfiler(&m_profiler);
}

Application::~Application()
{
    // 卸掉 current 指针：**必须发生在成员析构之前**（Application 的析构体先跑、
    // 成员再逆序析构），否则别处再问 `CurrentProfiler()` 会拿到一个半死的对象。
    // Profiler 自己没有别的清理动作，所以这里不做第二件事。
    SetCurrentProfiler(nullptr);
}

void Application::Run(IGame& game)
{
    game.OnStart();

    while (!QuitRequested()) {
        // ① 帧首：dt 从这里算起
        m_time.BeginFrame();

        // ①b S11：Profiler 与帧首**同一拍**（理由见文件头那句"两者必须是同一个点"）
        m_profiler.BeginFrame();

        // ★ S11 的计时块：`Frame` 覆盖 ②~⑥ 整段。
        //   ⚠️ 它**必须在这里收口**（块结束即析构）—— 若让它活到 while 体末尾，
        //      `LimitFPS` 的 sleep 就会被算进 `Frame`（`S11_CHECKLIST` §8 坑 6）。
        //   ⚠️ 本块**不含** ⑦ 的 `m_input->BeginFrame()`：所以 `Frame` 会比顶栏的
        //      `Work` 略小一点点（那一步的成本 = 每键一个 bool 的复位，量级可忽略）。
        {
            MINI_PROFILE_SCOPE("Frame");

            // ② 抓事件 → 本帧就能看见按键
            m_input->Poll();

            // ③ 画布对齐（**必须在 OnUpdate 之前**）
            //
            // 为什么要自己再问一次尺寸，而不是等 Console 报告：
            //   Console 只在 Present 时才发现尺寸变了（那时帧已经画完了），
            //   而游戏需要"这一帧就是新尺寸"。所以这里主动问，问了就重建。
            //
            // 拒绝过小的尺寸：FrameBuffer 会抛 invalid_argument（w/h 必须为正），
            // 而且 1 列的画面没有意义。**别拿异常当控制流** —— 拖窗口到极窄时
            // 会连续触发 resize，用异常处理既慢又难查。跳过这一次，下一帧再说。
            // ⚠️ 注意它**不 `continue`**（只是不接受新尺寸）⇒ 每帧必然走到 ⑦ 的
            //    `m_profiler.EndFrame()`，"每帧恰好一次"这条契约因此成立。
            const platform::ConsoleSize now = m_console->Size();
            if (now.width != m_size.width || now.height != m_size.height) {
                // 两轴**一起**判断（与启动路径的逐轴替换不同）：条件成立才接受新尺寸，
                // 否则跳过这一帧、保留旧尺寸。这段逻辑现在有机器用例守着（Core.CanvasSizeAcceptanceIsBoundarySafe）
                if (IsUsableCanvasSize(now.width, now.height)) {
                    m_frameBuffer.Resize(now.width, now.height);
                    m_size = now;
                }
            }

            // ④ 一帧的输入快照 + 更新
            const FrameContext ctx{
                m_time.Delta(),
                m_time.FrameMs(),
                m_time.MaxFrameMs(),   // 自启动以来最差的一帧（MaxFrameMs 仅有的生产调用点）
                m_time.WorkMs(),
                m_time.Fps(),
                m_frameBuffer.Width(),
                m_frameBuffer.Height(),
                *m_input
            };

            {
                // 只包住 `OnUpdate`：`ctx` 的构造算"引擎的账"，不算游戏更新（§6 的段名表）
                MINI_PROFILE_SCOPE("Game.Update");
                game.OnUpdate(ctx);
            }

            // ⑤ 画：Clear 由引擎负责，游戏只管往上写
            //     ⚠️ `Clear` **不进任何 scope**：它是引擎自己的全屏填充，每帧成本恒等于
            //        画布面积 —— 混进 `Game.Render` 会把"游戏到底画了多少"糊掉。
            m_frameBuffer.Clear();

            {
                MINI_PROFILE_SCOPE("Game.Render");
                game.OnRender(m_frameBuffer);
            }

            if (m_cfg.showStats) {
                MINI_PROFILE_SCOPE("StatsBar");
                DrawStatsBar(ctx);   // 在游戏之后 → 游戏盖不掉
            }

            // ⑥ 提交。false = 终端已经没了（Console 被销毁 / 写失败）
            {
                MINI_PROFILE_SCOPE("Present");
                if (!m_console->Present(m_frameBuffer)) {
                    RequestQuit();
                }
            }
        }   // ← `Frame` 在这里结束（析构即落库，进的是**本帧**的累加器）

        // ⑦ 帧尾推进 + 计时收尾 + 限帧（**最后一步**：上面的活一件都不能少）
        m_input->BeginFrame();

        // ⑦b S11：**必须紧贴在 `m_time.EndFrame()` 之前** —— 这两句是"同一拍"的：
        //     `Work`（顶栏）量到 EndFrame 为止，Profiler 的窗口也必须量到同一个点，
        //     否则两组数字永远差一截、而且差在哪查不出来。
        //     ⚠️ 挪到 `LimitFPS` 之后 = 把 sleep 算进每一段（§8 坑 6）。
        m_profiler.EndFrame();

        m_time.EndFrame();
        m_time.LimitFPS(m_cfg.targetFps);

        // ⑧ 退出判定放最后，保证本帧的输入/绘制/推进都做完了
        //    注意 input->QuitRequested() 是**进程级**的（Ctrl+C / 关窗口），
        //    与 m_quit 这个"应用级"开关不是一回事
        if (m_input->QuitRequested() || game.ShouldQuit()) {
            RequestQuit();
        }
    }
}

void Application::DrawStatsBar(const FrameContext& ctx)
{
    using render::Color;

    // 定宽字段（%5.1f 这种）能让数字跳动时**位置不抖** ——
    // 否则 FPS 从 99.9 掉到 100.0 会让整行往右挪一格，看着像画面在晃
    //
    // `Max` 紧挨着 `Frame`：这两个是**同一个口径**（都是未 clamp 的帧间隔），
    // 放一起才能一眼比"这一帧 vs 史上最差"。`Work` 是另一个量（帧内干活耗时），
    // 排在后面。
    //
    // `Max` 是**自启动以来**的最差值，没有任何 UI 能重置它（Reset() 只有测试在调），
    // 所以它**只会单调不降** —— 拖过一次窗口之后它会永久记住那次尖峰。
    // 这是有意的：PLAN 坑 #24 写了"resize 那一帧的尖峰 MaxFrameMs() 会记下它，别当 bug 去消"。
    char text[96];
    std::snprintf(text, sizeof(text),
                  "FPS %5.1f | Frame %5.1fms | Max %5.1fms | Work %4.1fms | Rows %2d/%2d",
                  static_cast<double>(ctx.fps),
                  static_cast<double>(ctx.frameMs),
                  static_cast<double>(ctx.maxFrameMs),
                  static_cast<double>(ctx.workMs),
                  m_console->LastFrameRowsSent(),
                  ctx.screenHeight);

    // 用暗灰：顶栏是调试信息，不该比游戏内容还抢眼
    for (int x = 0; x < ctx.screenWidth && text[x] != '\0'; ++x) {
        m_frameBuffer.Set(x, kStatsRow, text[x], Color::BrightBlack, Color::Default);
    }

    // 右边擦到行尾：原来更长的残留不清掉的话，缩窄窗口后会留半截数字
    const int len = static_cast<int>(std::strlen(text));
    for (int x = len; x < ctx.screenWidth; ++x) {
        m_frameBuffer.Set(x, kStatsRow, ' ', Color::BrightBlack, Color::Default);
    }
}

} // namespace mini::core
