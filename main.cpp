// 尖括号 + `mini/` / `game/` 前缀：引擎头只能从 PUBLIC include 目录找，
// 不允许用相对路径穿透到 engine/src/ 或 game/src/（S0_CHECKLIST 第 5.5 节 / R7）
#include <game/Game.h>
#include <mini/core/Application.h>

#include <cstdio>
#include <cstdlib>
#include <exception>

// ============================================================
// S5 的 main —— **359 行 → 不到 40 行**
//
// 它现在只做三件事：解析 argv、造对象、交给 Application::Run。
// 游戏侧（原来的 DemoGame，约 250 行）全部搬去了 game/：
//   · `main → game → engine` 单向（PLAN A.3：engine 绝不 include game）
//   · main 不认识 `@`、不认识地图、不认识世界 —— 它只知道"有个 IGame"
//
// 这是 S5 三个"看得见的变化"里的第三个：引擎与游戏第一次分家。
// ============================================================

int main(int argc, char** argv)
{
    mini::core::AppConfig cfg;   // 宽高 0 = 用终端可视区

    // 第一个参数 = 目标帧率。用来验证"限帧真的在限帧，而不是工作变快了"：
    //   MiniEngine.exe 300  → FPS 明显上升，而 Work 基本不变
    //   0 或负数 = 不限帧（顶栏会显示几百帧）
    if (argc > 1) {
        cfg.targetFps = std::atoi(argv[1]);
    }

    // 第二个参数 = 移动速度（格/秒）。**手感数字要试出来，不要猜**
    //（2026-09-30：12 格/秒时"跨一格"要按住 83ms，轻点几乎看不到 `@` 动 → 默认已调到 20）。
    // 缺省 / 非数字 / 负数都交给 Game 的构造函数回落默认值（0 会解析成 0.0 → 回落）。
    const float moveSpeed = (argc > 2) ? static_cast<float>(std::atof(argv[2]))
                                       : game::Game::kMoveSpeed;

    try {
        // 顺序有讲究：game 先建、app 后建，析构就是反过来的 ——
        // 终端由 app 先恢复、再轮到 game，出错信息才不会被备用屏吞掉。
        game::Game               theGame(moveSpeed);
        mini::core::Application  app(cfg);

        app.Run(theGame);   // 阻塞；返回前 theGame 不能析构
    } catch (const std::exception& e) {
        // 只会因为一件事走到这里：stdout / stdin 不是控制台（比如 `> out.txt`）。
        // 抛异常 + 人话提示，比"静默什么都不显示"好得多（见 Console.h）。
        std::fprintf(stderr, "fatal: %s\n", e.what());
        return 1;
    }

    return 0;
}
