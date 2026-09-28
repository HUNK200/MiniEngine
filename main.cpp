// 尖括号 + `mini/` 前缀：引擎头只能从 PUBLIC include 目录找，
// 不允许用相对路径穿透到 engine/src/（S0_CHECKLIST 第 5.5 节 / R7）
#include <mini/platform/Console.h>
#include <mini/render/FrameBuffer.h>

#include <chrono>
#include <cstdio>
#include <exception>
#include <string>
#include <thread>

// ============================================================
// S2 / 步骤 1：终端管理层换成 platform::Console（RAII），**画面一个字符都不改**。
//
// 这一步只做"搬家"，所以演示内容仍然是 S1 的：
//   阶段 1  连推 10 秒 → 验证"连续重绘不闪"
//   阶段 2  停住、等 Ctrl+C → 慢慢看画面，并验证退出后终端干净
// 步骤 4 才会把演示内容换成按键状态显示。
//
// 与 S1 的两处区别：
//   1. BeginTerminal/EndTerminal 那对全局函数没了 —— Console 的构造/析构接管了它们，
//      所以 S1 里那层 try/catch(...) { EndTerminal(); throw; } 补丁可以删掉
//   2. 退出信号从 render::QuitRequested() 换成 platform::QuitRequested()
// ============================================================

namespace {

constexpr int kWidth = 80;
constexpr int kHeight = 25;

// 画一个 ASCII 边框：Cell::ch 是单字节，写不了 UTF-8 制表符（S1 决策 2）
void DrawScene(mini::render::FrameBuffer& fb)
{
    using mini::render::Color;

    fb.Clear();

    const std::string horizontal(static_cast<std::size_t>(kWidth - 2), '-');
    const std::string edge = "+" + horizontal + "+";
    fb.Draw(0, 0, edge);
    fb.Draw(0, kHeight - 1, edge);
    for (int y = 1; y < kHeight - 1; ++y) {
        fb.Set(0, y, '|', Color::BrightBlack);
        fb.Set(kWidth - 1, y, '|', Color::BrightBlack);
    }

    // 内框：顺便演示 DrawRect 的"只画边 + 出界自动裁剪"
    fb.DrawRect(10, 5, 20, 7, '.');

    // 两个**非对称**标记：验收要求"画面内容与代码坐标一致"，
    // 左上 @ + 右下 #，一旦 x/y 写反或画面转置，一眼就看出来
    fb.Set(1, 1, '@', Color::BrightYellow);
    fb.Set(kWidth - 2, kHeight - 2, '#', Color::BrightRed);
}

} // namespace

int main()
{
    using namespace std::chrono_literals;

    mini::render::FrameBuffer fb(kWidth, kHeight);
    DrawScene(fb);

    // 提示行在切码页之前打：此时码页还没切成 UTF-8，所以刻意只用 ASCII
    std::printf("[S2] press Ctrl+C to quit\n");

    try {
        // 构造 = 原 BeginTerminal()：存原状态 → 切 UTF-8 → 开 VT → 关 QUICK_EDIT
        //        → 进备用屏 → 隐光标 → 装 Ctrl+C handler
        auto console = mini::platform::MakeConsole();

        // 阶段 1：连续重绘 10 秒。按**真实时间**驱动而不是按帧数：
        // 每帧还有序列化 + 写终端的开销，帧数写死的话"10 秒"会变成 15 秒以上
        const auto start = std::chrono::steady_clock::now();
        while (!mini::platform::QuitRequested() &&
               std::chrono::steady_clock::now() - start < 10s) {
            console->Present(fb);
            std::this_thread::sleep_for(16ms);
        }

        // 阶段 2：画面留在屏幕上、不再重绘，等 Ctrl+C
        while (!mini::platform::QuitRequested()) {
            std::this_thread::sleep_for(50ms);
        }

        // 离开作用域 = 原 EndTerminal()：显光标 → 离开备用屏 → 还原模式与码页
    } catch (const std::exception& e) {
        // 只会因为一件事走到这里：stdout / stdin 不是控制台（比如你 `> out.txt`）。
        // 这时抛异常 + 人话提示，比"静默什么都不显示"好得多（见 Console.h）。
        std::fprintf(stderr, "%s\n", e.what());
        return 1;
    }

    return 0;
}
