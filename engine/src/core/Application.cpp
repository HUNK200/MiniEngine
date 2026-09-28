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
//   ② m_input->Poll()          ← 抓事件，让本帧能看见按键
//   ③ 对齐画布尺寸             ← 必须在 game.OnUpdate **之前**，否则游戏这一帧
//                                拿到旧尺寸算出来的位置
//   ④ game.OnUpdate(ctx)
//   ⑤ Clear → game.OnRender → 顶栏
//   ⑥ Present（失败 = 终端没了 → 请求退出）
//   ⑦ 输入帧尾推进 + EndFrame + LimitFPS
//   ⑧ 退出判定（放最后：本帧该干的活一件都不能少）
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

// 1 列 / 1 行没法画（FrameBuffer 也会拒绝 w<=0），所以小于这个就当读不到
constexpr int kMinUsableWidth  = 2;
constexpr int kMinUsableHeight = 2;

// 画布尺寸：配置里显式给了就用它，否则用终端可视区，再不行退回 80x25。
//
// 抽成自由函数是为了在**成员初始化列表里**用：FrameBuffer 没有默认构造函数，
// 必须在列表里就拿到尺寸。它只依赖 m_cfg 与 m_console（都声明在前面，已初始化）。
platform::ConsoleSize ResolveCanvasSize(const AppConfig& cfg, const platform::Console& console)
{
    const platform::ConsoleSize term = console.Size();

    int w = (cfg.width  > 0) ? cfg.width  : term.width;
    int h = (cfg.height > 0) ? cfg.height : term.height;

    if (w < kMinUsableWidth) {
        w = kFallbackWidth;
    }
    if (h < kMinUsableHeight) {
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
}

Application::~Application() = default;

void Application::Run(IGame& game)
{
    game.OnStart();

    while (!QuitRequested()) {
        // ① 帧首：dt 从这里算起
        m_time.BeginFrame();

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
        const platform::ConsoleSize now = m_console->Size();
        if (now.width != m_size.width || now.height != m_size.height) {
            if (now.width >= kMinUsableWidth && now.height >= kMinUsableHeight) {
                m_frameBuffer.Resize(now.width, now.height);
                m_size = now;
            }
        }

        // ④ 一帧的输入快照 + 更新
        const FrameContext ctx{
            m_time.Delta(),
            m_time.FrameMs(),
            m_time.WorkMs(),
            m_time.Fps(),
            m_frameBuffer.Width(),
            m_frameBuffer.Height(),
            *m_input
        };

        game.OnUpdate(ctx);

        // ⑤ 画：Clear 由引擎负责，游戏只管往上写
        m_frameBuffer.Clear();
        game.OnRender(m_frameBuffer);
        if (m_cfg.showStats) {
            DrawStatsBar(ctx);   // 在游戏之后 → 游戏盖不掉
        }

        // ⑥ 提交。false = 终端已经没了（Console 被销毁 / 写失败）
        if (!m_console->Present(m_frameBuffer)) {
            RequestQuit();
        }

        // ⑦ 帧尾推进 + 计时收尾 + 限帧（**最后一步**：上面的活一件都不能少）
        m_input->BeginFrame();
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
    char text[96];
    std::snprintf(text, sizeof(text),
                  "FPS %5.1f | Frame %5.1fms | Work %4.1fms | Rows %2d/%2d",
                  static_cast<double>(ctx.fps),
                  static_cast<double>(ctx.frameMs),
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
