#pragma once

#include <cstdint>

// ============================================================
// Color —— 终端 256 色（S1 决策 1：直接按 256 色索引来）
//
// 表示：
//   0 .. 254 = ANSI 256 色索引（0~15 基础色 + 16~231 的 6×6×6 色立方 + 232~254 灰阶）
//   255      = 哨兵，表示"用终端默认颜色"，输出时发 ESC[39m / ESC[49m
//
// 代价（写在明处，别让三天后的自己骂人）：
//   256 色里第 255 号（最亮的灰阶）用不了，因为 0xFF 被借去当哨兵了。
//   真需要它时：把底层换成 std::uint16_t（Cell 会从 8 字节变成 10 字节），
//   改动只落在本文件与 FrameBuffer::Serialize 里的两处色码输出。
//
// 底层类型是 std::uint8_t：sizeof(Color) == 1。
// ⚠️ **2026-10-16（S12）更正**：这里原写"……`Cell` 才能保持 3 字节"，**已经过期** ——
//    S12 把 `Cell::ch` 扩宽到了 `char32_t`（`Cell` = 8 字节，含 2 字节对齐填充）。
//    本文件仍是 1 字节；它只是不再是"决定 Cell 大小上限的那一项"了
//    （来龙去脉：`doc/S12_CHECKLIST.md` §3 分叉 A）。
// ============================================================

namespace mini::render {

enum class Color : std::uint8_t {
    // 0~7 标准色
    Black = 0,
    Red = 1,
    Green = 2,
    Yellow = 3,
    Blue = 4,
    Magenta = 5,
    Cyan = 6,
    White = 7,
    // 8~15 高亮色
    BrightBlack = 8,
    BrightRed = 9,
    BrightGreen = 10,
    BrightYellow = 11,
    BrightBlue = 12,
    BrightMagenta = 13,
    BrightCyan = 14,
    BrightWhite = 15,
    // 哨兵
    Default = 0xFF,
};

// 16~231（色立方）与 232~254（灰阶）不逐个命名，用这两个构造：
//   FromIndex(RgbCube(5, 0, 0))   → 索引 196，亮红
//   FromIndex(GrayLevel(12))      → 索引 244
constexpr Color FromIndex(std::uint8_t index)
{
    return index == 0xFF ? Color::Default : static_cast<Color>(index);
}

constexpr std::uint8_t ToIndex(Color c)
{
    return static_cast<std::uint8_t>(c);
}

constexpr bool IsDefault(Color c)
{
    return c == Color::Default;
}

// 6×6×6 色立方：每个通道 0~5，越界不检查（调用者自己保证）
constexpr std::uint8_t RgbCube(int r, int g, int b)
{
    return static_cast<std::uint8_t>(16 + 36 * r + 6 * g + b);
}

// 灰阶：level 0~22 → 索引 232~254（255 被哨兵占了，所以到不了 23）
constexpr std::uint8_t GrayLevel(int level)
{
    return static_cast<std::uint8_t>(232 + (level < 0 ? 0 : (level > 22 ? 22 : level)));
}

} // namespace mini::render
