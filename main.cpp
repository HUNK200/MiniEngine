// 尖括号 + `mini/` 前缀：引擎头只能从 PUBLIC include 目录找，
// 不允许用相对路径穿透到 engine/src/（S0_CHECKLIST 第 5.5 节 / R7）
#include <mini/render/FrameBuffer.h>
#include <mini/render/TerminalPresenter.h>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

// ============================================================
// S1：让终端里出现"带边框的盒子 + 一个 @"。
//
// 这一版刻意**没有游戏循环**（PLAN C.4 的 GameLoop 是 S3 的事）：
//   阶段 1  连推 600 帧（≈10 秒）→ 验证"连续重绘不闪"
//   阶段 2  停住、不再重绘，等 Ctrl+C → 慢慢看画面，并验证退出后终端干净
// S2 起 main 会被 Application 取代（PLAN C.3）。
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

    // 退出后会看回正常屏，这行提示就在提示符上方（刻意用 ASCII，此时码页还没切 UTF-8）
    std::printf("[S1] press Ctrl+C to quit\n");

    mini::render::BeginTerminal();

    try {
        // 阶段 1：连续重绘 10 秒。每帧完整重画 —— 会闪的话这里就能看出来。
        // 按**真实时间**驱动而不是按帧数：每帧还有序列化 + 写终端的开销，
        // 帧数写死的话"10 秒"会变成 15 秒以上（实测过）
        const auto start = std::chrono::steady_clock::now();
        while (!mini::render::QuitRequested() &&
               std::chrono::steady_clock::now() - start < 10s) {
            mini::render::Present(fb);
            std::this_thread::sleep_for(16ms);
        }

        // 阶段 2：画面留在屏幕上、不再重绘，等 Ctrl+C
        while (!mini::render::QuitRequested()) {
            std::this_thread::sleep_for(50ms);
        }
    } catch (...) {
        mini::render::EndTerminal();   // 别把终端留在"备用屏 + 隐光标"状态
        throw;
    }

    mini::render::EndTerminal();
    return 0;
}
