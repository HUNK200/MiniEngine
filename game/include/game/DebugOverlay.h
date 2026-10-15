#pragma once

// ============================================================
// game/DebugOverlay.h —— 右上角的实时性能面板（S11 的"看得清"之三）
//
// ⚠️ **它住在 `game/`，不在 `engine/render/`**（`S11_CHECKLIST` §10 分叉 B 取①）：
//    面板里有一半是**游戏侧**的东西（`wave` / `foes` / `shots` / `ammo` / `hp`），
//    引擎不认识它们。引擎侧只提供 Profiler 的表与帧数据，谁把它们摆在一起是**游戏的事**。
//
// 分工（与 `GameState.h` / `Bullet.h` 同一条纪律："决策与接线分家"）：
//   · `BuildOverlayLines` —— **纯函数**：数据 → 文本行。不碰 FrameBuffer、不碰终端、不分配。
//     ⇒ 于是"字形纪律"（下面那段 ⚠️⚠️）和"行数/行宽"都能**离线**用机器钉住。
//   · `DrawDebugOverlay`  —— **接线**：把那些行画成右上角的一个方框（边框用 `FrameBuffer::DrawRect`，
//     它是 S11 起第一次**有生产调用者**，见 `engine/include/mini/render/FrameBuffer.h` 的注释）。
//
// ⚠️⚠️ **字形纪律（`TECH_DEBT` C31，本阶段唯一"实测踩过"的债）**：
//    屏幕上 `@` / `E` / `*` 三个字符**既是实体字形、又是普通字母**，而既有用例靠
//    "数屏幕上 `E` 的个数"来数敌人 ⇒ 面板里只要写出其中一个，那些断言就会**静默算错**
//    （不是变红，是**算错** —— 比红更坏）。
//    ⇒ 所以上面那句"纯函数"很重要：`tests/test_game.cpp` 的 `OverlayLinesAvoidReservedGlyphs`
//      会对**每一行**断言不含这三个字符（含标点、含段名）。
//    ⇒ 连带一条给以后**所有往屏幕上写字的东西**的规矩：**标签表里不许出现大写 `E`**
//      （本项目因此用 `foes` 而不是 `Enemy`）。
// ============================================================

#include <mini/core/Profiler.h>   // Profiler::Row（面板要显示那张表）
#include <mini/render/FrameBuffer.h>   // DrawDebugOverlay 的参数与 `DrawRect`/`Draw`/`Set`

#include <array>
#include <cstddef>
#include <cstdio>    // std::snprintf（定长缓冲，全程不分配、也不会越界）
#include <cstring>   // std::strlen

namespace game {

// 内容行的上限。当前最多用到 5 + `kMaxScopes`(8) = 13 行。
inline constexpr int kOverlayMaxLines = 14;

// 每行的字符上限（含结尾的 '\0'）。`snprintf` 超长会**截断**，不会越界。
inline constexpr int kOverlayLineCap = 48;

// 面板要显示的东西。**两类数据的新鲜度不同**（`S11_CHECKLIST` §4.5 分叉 F 甲）：
//   · 引擎侧（前 6 项）：来自 `Game` 的帧快照 ⇒ **慢一帧**（顶栏的 `Rows` 也是这样）；
//   · 游戏侧（后面那几个 int）：`Game::OnRender` 里**当帧现读**。
struct OverlayData {
    // ── 引擎侧 ──
    float frameMs       = 0.0f;
    float maxFrameMs    = 0.0f;
    float workMs        = 0.0f;
    float fps           = 0.0f;
    const mini::core::Profiler::Row* rows = nullptr;   // 由调用方持有（本结构不拥有）
    int   rowCount      = 0;
    int   historyFrames = 0;   // 已收帧数 ⇒ 显示 "-- profile (57f) --"，首屏不满 120 时**不骗人**

    // ── 游戏侧 ──
    int   wave  = 0;
    int   foes  = 0;
    int   shots = 0;
    int   ammo  = 0;
    int   hp    = 0;
    // 几何层开关（`F2`，S11 步骤 5c）。⚠️ 它**不是**引擎数据 —— 归 `Game::m_showGeometry`，
    //   放在这里是因为"看读数的人得知道屏幕上那层底色为什么亮着"。
    bool  geometry = false;
};

using OverlayLines = std::array<std::array<char, kOverlayLineCap>, kOverlayMaxLines>;

// 内容行数（不含边框）的固定部分：frame / work / wave / hp / profile 表头 = 5 行；
// 表体最多 `kMaxScopes` 行。行不够就是**写不下**，所以拿 `static_assert` 钉在编译期
//（两个常量分属两处 ⇒ 这条断言不是恒真的，它挡的是"以后有人加了段名/改了行数上限"）。
inline constexpr int kOverlayFixedLines = 5;
static_assert(kOverlayMaxLines >= kOverlayFixedLines + mini::core::Profiler::kMaxScopes,
              "内容行放不下：kOverlayMaxLines 至少要有 5 + kMaxScopes 行");

// 把数据拍成文本行，返回**行数**（<= kOverlayMaxLines）。**纯函数**：
// 不碰 FrameBuffer、不碰终端、不分配、不抛。
// 写进调用方给的定长数组（不返回内部指针 —— 同 `Profiler::Snapshot` / `RowBuffer::Compose` 的取舍）。
inline int BuildOverlayLines(const OverlayData& d, OverlayLines& out) noexcept
{
    int n = 0;

    // 每行都用 `snprintf` 写进**定长**缓冲：超长自动截断 ⇒ 这个函数永远不会写坏内存
    //（`fb.Draw` 的"逐格越界忽略"是第二道保险，见 §8 坑 9）。
    std::snprintf(out[static_cast<std::size_t>(n)].data(), kOverlayLineCap,
                  "frame %6.1f  max %6.1f",
                  static_cast<double>(d.frameMs), static_cast<double>(d.maxFrameMs));
    ++n;

    std::snprintf(out[static_cast<std::size_t>(n)].data(), kOverlayLineCap,
                  "work  %6.2f  fps %5.1f",
                  static_cast<double>(d.workMs), static_cast<double>(d.fps));
    ++n;

    std::snprintf(out[static_cast<std::size_t>(n)].data(), kOverlayLineCap,
                  "wave %2d  foes %2d  shots %2d", d.wave, d.foes, d.shots);
    ++n;

    // `geom` 用定宽 `%-3s`：开关切换时这一行**不会左右抖**（同 HUD 里 `%2d` 的理由）。
    // ⚠️ `on` / `off` 全是小写 —— 面板里不许出现 `@` / `E` / `*`（C31 的字形纪律）。
    std::snprintf(out[static_cast<std::size_t>(n)].data(), kOverlayLineCap,
                  "hp %2d    ammo %2d  geom %-3s",
                  d.hp, d.ammo, d.geometry ? "on" : "off");
    ++n;

    std::snprintf(out[static_cast<std::size_t>(n)].data(), kOverlayLineCap,
                  "-- profile (%3df) --", d.historyFrames);
    ++n;

    for (int i = 0; i < d.rowCount && n < kOverlayMaxLines; ++i) {
        // `name` 兜底成 "?"：面板是调试工具，**不该因为一个空名字就崩**
        const char* name = (d.rows[i].name != nullptr) ? d.rows[i].name : "?";
        std::snprintf(out[static_cast<std::size_t>(n)].data(), kOverlayLineCap,
                      "%-12s %6.2f / %6.2f",
                      name,
                      static_cast<double>(d.rows[i].avgMs),
                      static_cast<double>(d.rows[i].maxMs));
        ++n;
    }

    return n;
}

// 接线：把内容画成右上角的方框面板。
// ⚠️ 层级：调用方在 `DrawHud` **之后**画它（`S11_CHECKLIST` §8 坑 10），所以它会盖住
//    HUD 右侧与地图的右上角 —— 这是"浮层"的应有之义（读数优先）。
// ⚠️ 状态横幅（PAUSED / GAME OVER）仍然画在它**之上**：那是玩家必须先看到的东西。
inline void DrawDebugOverlay(mini::render::FrameBuffer& fb, const OverlayData& d) noexcept
{
    OverlayLines lines{};
    const int    count = BuildOverlayLines(d, lines);
    if (count <= 0) {
        return;
    }

    int textWidth = 0;
    for (int i = 0; i < count; ++i) {
        const int len = static_cast<int>(std::strlen(lines[static_cast<std::size_t>(i)].data()));
        if (len > textWidth) {
            textWidth = len;
        }
    }

    const int panelW = textWidth + 2;   // 左右各一格内边距
    const int panelH = count + 2;       // 上下两条边框

    // 贴右边；终端太窄时夹到 0（§8 坑 9：宁可贴左边，也不要算出负坐标）
    int x = fb.Width() - panelW;
    if (x < 0) {
        x = 0;
    }
    const int y = 1;   // 行 0 是引擎顶栏，**必须让给它**（它由 Application 最后画）

    // 先把整块铺成空格 ⇒ 面板是**不透明**的。不铺的话 HUD/地图会从文字缝里透出来，
    // 读数在动的东西上根本看不清（这也让 `F7` 面板那套"底部对齐躲开敌人"的技巧不再需要）。
    for (int row = y; row < y + panelH; ++row) {
        for (int col = x; col < x + panelW; ++col) {
            fb.Set(col, row, ' ');
        }
    }

    // 边框：S11 起 `DrawRect` 有了第一个**生产**调用者（前置事实 7 / 坑 15）
    fb.DrawRect(x, y, panelW, panelH, '+');

    for (int i = 0; i < count; ++i) {
        fb.Draw(x + 1, y + 1 + i, lines[static_cast<std::size_t>(i)].data());
    }
}

} // namespace game
