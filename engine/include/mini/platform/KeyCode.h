#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// ============================================================
// KeyCode.h —— 按键"词汇表"：键、状态、VK 映射（S2）
//
// 零依赖（只 include 标准库），所以可以被单元测试完整覆盖。
// 也正因为不能 include <windows.h>，VK 码在这里是**裸数字 + 注释**，
// 不是 VK_* 宏 —— 这是 A.3 硬规矩（公共头不出现 windows.h）的代价。
//
// 三样东西：
//   Key              项目要用的按键（Unknown = 0，让数组默认值即"无效"）
//   KeyState         按键的四态
//   KeyFromVirtualKey  VK 码 → Key（未映射的返回 Unknown）
//   KeyIndex          Key → 数组下标，**无效值返回 kKeyCount 哨兵**
// ============================================================

namespace mini::platform {

enum class Key : std::uint8_t {
    // 0 必须是 Unknown：std::array<..., kKeyCount> 的默认初始化就落在它上面，
    // 于是"没映射过的键"天然等于"无效键"，不需要额外初始化代码
    Unknown = 0,

    W, A, S, D, Q, E, R,          // 移动 + 交互（S5 起用）
    Space, Enter, Escape, Tab,
    ArrowUp, ArrowDown, ArrowLeft, ArrowRight,
    F1, F2, F3, F4,               // S11 的 Debug Overlay 用

    // ⚠️ **2026-10-10（S9 步骤 3）：`K` 已删除。**
    //    它是 S8 的临时调试键（"杀掉离玩家最近的敌人"），S9 接上真正的子弹之后它就是多余的
    //    —— `TECH_DEBT` **C15** 到期关闭。删掉它会让 `F7` 的枚举值**往前挪一位**：
    //    这是安全的（枚举值只在进程内当数组下标用，没有任何持久化），
    //    但 `tests/test_input_state.cpp` 里那条 `KeyFromVirtualKey(0x4B) == Key::K` 必须一起删。
    //    ⚠️ 保留这条注释的理由：下次有人问"K 去哪了"，答案在这里，不用去翻 git 历史。

    // S8：敌人面板（那个**临时**面板）的开关键。
    // ⚠️ **为什么是 F7 而不是 `PLAN:572` 写的 F3**：2026-10-09 本机实测 **F3 被别的软件占用**
    //    —— 按下去我们的程序收不到。（2026-10-08 已经演过一遍：F1 冲突 → 换 F3 还是冲突
    //    → 最后落到 F4；见 `game/src/Game.cpp` 里 `kDumpKey` 的注释。）
    // ⚠️ `F7` 与 `F1..F4` 的 VK 码**不连续**（0x70..0x73 vs 0x76），所以只能在
    //    `MakeKeyMap` 里单独补一条映射 —— 不能靠"F1~F4 连续"那行注释推出来。
    F7,

    Count                         // 哨兵：等于数组长度，本身不是合法按键
};

inline constexpr std::size_t kKeyCount = static_cast<std::size_t>(Key::Count);

// 按键四态。
//
// 为什么不是三态（PLAN C.3 最早写的是 {Released, Pressed, Held}）：
//   三态下无法区分"本帧刚抬起"和"一直没按过"，而 S2 的验收要求
//   "松开显示 Released 仅一帧"—— 三态物理上做不到。加一个 Up 就解决了，
//   状态转移图依然是一张直白的表（见 doc/S2_CHECKLIST.md 6.3）。
enum class KeyState : std::uint8_t {
    Up,         // 一直没按（或已经松开超过一帧）
    Pressed,    // 本帧刚按下，含"本帧内按下又抬起"的极速点击；只维持一帧
    Held,       // 持续按住
    Released    // 本帧刚抬起；只维持一帧
};

namespace detail {

// VK 码 → Key 的查表。全表先填 Unknown，只点亮项目要用的那些。
// 用表而不是 switch：查表是 O(1) 且没有分支预测失败，VK 码本来就是 0~255 的稠密空间。
constexpr std::array<Key, 256> MakeKeyMap() noexcept
{
    std::array<Key, 256> map{};   // 值初始化 → 全是 Key::Unknown

    // 字母键：VK 码就是 ASCII 大写字母
    map[0x57] = Key::W;   // 'W'  VK_W
    map[0x41] = Key::A;   // 'A'  VK_A
    map[0x53] = Key::S;   // 'S'  VK_S
    map[0x44] = Key::D;   // 'D'  VK_D
    map[0x51] = Key::Q;   // 'Q'  VK_Q
    map[0x45] = Key::E;   // 'E'  VK_E
    map[0x52] = Key::R;   // 'R'  VK_R

    // 控制键
    map[0x20] = Key::Space;    // VK_SPACE
    map[0x0D] = Key::Enter;    // VK_RETURN
    map[0x1B] = Key::Escape;   // VK_ESCAPE
    map[0x09] = Key::Tab;      // VK_TAB

    // 方向键。**不区分小键盘与主键盘**：VK_UP/DOWN/LEFT/RIGHT 对两者是同一个码，
    // 想区分要靠 dwControlKeyState 的 ENHANCED_KEY 位，S2 不需要。
    map[0x25] = Key::ArrowLeft;    // VK_LEFT
    map[0x26] = Key::ArrowUp;      // VK_UP
    map[0x27] = Key::ArrowRight;   // VK_RIGHT
    map[0x28] = Key::ArrowDown;    // VK_DOWN

    // 功能键：VK_F1~VK_F4 是连续的
    map[0x70] = Key::F1;
    map[0x71] = Key::F2;
    map[0x72] = Key::F3;
    map[0x73] = Key::F4;

    // S8：**F7** —— 敌人面板的开关键。⚠️ 它跟上面那四条**不连续**（0x70..0x73 → 0x76），
    // 所以是单独一条映射：别看到"F1~F4 连续"就以为 F5~F12 也在表里（原来都不在）。
    map[0x76] = Key::F7;           // VK_F7（S8 的敌人面板；F3 在本机被别的软件占用）

    // ⚠️ **S9 步骤 3 删掉了 `map[0x4B] = Key::K;`**（S8 的临时调试键，`TECH_DEBT` C15 到期）。
    //    ⇒ 现在按 `K` **收不到任何事件**（`KeyFromVirtualKey(0x4B)` 返回 `Unknown`）。
    //    这正是"删干净"的判据：留着映射但删了枚举会变成 `Unknown` 也一样，
    //    所以两处一起删，且 `tests/test_input_state.cpp` 里那条断言也删了。
    //    （"有意不映射"的样本仍然是 'B'。）

    // 修饰键（VK_SHIFT / VK_CONTROL / VK_MENU 及其左右变体）**有意不映射**：
    // 本项目不需要"按住 Shift 跑"这类功能，映进来只会多出要维护的状态。
    return map;
}

inline constexpr std::array<Key, 256> kKeyMap = MakeKeyMap();

} // namespace detail

// VK 码 → Key。没见过的一律 Unknown。
//
// 参数是 uint16_t 而不是 uint8_t：Windows 给的 wVirtualKeyCode 是 WORD，
// 先截断成 8 位再查表会把"高位非零的码"悄悄折到别的键上。
constexpr Key KeyFromVirtualKey(std::uint16_t vk) noexcept
{
    return vk < detail::kKeyMap.size() ? detail::kKeyMap[vk] : Key::Unknown;
}

// Key → 数组下标。**无效值（Unknown / Count / 越界）返回 kKeyCount 哨兵**，
// 调用者必须拿它和 kKeyCount 比一次再决定丢不丢。
// 这样"多个未映射的键一起写同一个槽位"这类错就没了源头：它们根本拿不到槽位。
constexpr std::size_t KeyIndex(Key k) noexcept
{
    const auto raw = static_cast<std::uint8_t>(k);
    return (raw >= 1 && raw < static_cast<std::uint8_t>(Key::Count))
               ? static_cast<std::size_t>(raw)
               : kKeyCount;
}

} // namespace mini::platform
