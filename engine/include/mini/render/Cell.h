#pragma once

#include "mini/render/Color.h"

// ============================================================
// Cell —— 屏幕上一个字符格（S1 决策 2：尽量单字节）
//
// 布局：char(1) + Color(1) + Color(1) = 3 字节，无填充。
// 2000 个格子的 80×25 画面 ≈ 6 KB，一个 Cache 友好的连续块。
//
// 代价：ch 是单字节，**画不了中文和 UTF-8 制表符**，边框只能用 ASCII 的 + - |。
// 这是有意的取舍 —— S1 要验证的是渲染管线通不通，不是美术。
// 将来（S13 想画 █▓▒░ 时）把 ch 换成 char32_t，同时在 Serialize 里补 UTF-8 编码：
// 那时 Cell 变 8 字节、内存翻 2.6 倍，而使用点只有 Set/Draw/Serialize 三处。
// ============================================================

namespace mini::render {

struct Cell {
    char  ch = ' ';
    Color fg = Color::Default;
    Color bg = Color::Default;
};

// 决策 2 的硬约束：一旦有人给 Cell 加了字段（比如 bool bold），这里立刻炸，
// 逼着他回来重新想"内存翻倍值不值"，而不是悄悄涨上去。
static_assert(sizeof(Cell) == 3, "Cell 必须保持 3 字节（char + 两个 uint8_t，无填充）");
static_assert(alignof(Cell) == 1, "Cell 不能有对齐填充");

} // namespace mini::render
