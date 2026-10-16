#pragma once

#include "mini/render/Color.h"

// ============================================================
// Cell —— 屏幕上一个字符格
//
// ⚠️ **2026-10-16（S12）重新决策：`ch` 从 `char` 扩宽到 `char32_t`。**
//    起因：S12 的伪 3D 要按距离画 `█ ▓ ▒ ░`（U+2588 等，UTF-8 是 **3 字节**），
//    单字节 `ch` 根本装不下。原决策把这件事排在"S13 想画时"，**S12 提前做了**
//    （理由与影响面：`doc/S12_CHECKLIST.md` §3 分叉 A · §5.3）。
//    代价照实记：3 → 8 字节（`char32_t` 4 + 两个 `Color` 各 1 + **2 字节对齐填充**）；
//    3000 格 = 9 KB → 24 KB —— 绝对值可以忽略，真正的成本在**改动面**（见下）。
//
//    护栏（两条必须同时成立，否则就是"悄悄改行为"）：
//      · `FrameBuffer::AppendRowTo` 里 **`ch < 0x80` 走原分支** ⇒ ASCII 画面的
//        序列化结果与本文件扩宽前**逐字节相同**；
//      · 有用例钉着它：`FrameBuffer.AsciiSerializationIsByteIdentical`。
//    语义：**一个格 = 一个码位**（code point）。不做 CJK 宽字符（那会占 2 列），
//    也不做组合符（那会把"1 格 = 1 个字形"这条打破）。
//
// ── 原决策保留（S1 决策 2「尽量单字节」，供对照）────────────────
//   布局 char(1) + Color(1) + Color(1) = 3 字节，无填充；80×25 画面 ≈ 6 KB，
//   一个 Cache 友好的连续块。
//   代价：画不了中文和 UTF-8 制表符，边框只能用 ASCII 的 + - |。
//   这是有意的取舍 —— S1 要验证的是渲染管线通不通，不是美术。
// ============================================================

namespace mini::render {

struct Cell {
    char32_t ch = U' ';
    Color    fg = Color::Default;
    Color    bg = Color::Default;
};

// 决策 2 的硬约束（**2026-10-16 起放宽到 8 字节 / 对齐 4**）：一旦有人再给 Cell 加字段，
// 这里立刻炸，逼着他回来重新想"这次涨得值不值"，而不是悄悄涨上去。
static_assert(sizeof(Cell) == 8, "Cell 必须是 8 字节（char32_t + 2×uint8_t + 2 字节填充）");
static_assert(alignof(Cell) == 4, "Cell 的对齐必须是 4（= alignof(char32_t)）");

} // namespace mini::render
