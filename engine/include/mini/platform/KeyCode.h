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
    // S11（2026-10-15 拍板，分叉 A）：**Debug Overlay 的开关用字母键 `T`**。
    //   为什么不是 `F1`：`F1` 在本机被别的软件抢占（按下去我们的程序收不到），
    //   而 `F3` 已经栽过两次（见下面 `F7` 那段注释的来历）。字母键不在那类全局热键的靶场上。
    //   ⚠️ 它**不进 HUD 提示行** —— overlay 是开发者工具，不是玩法键（§10-A 的附带决定）。
    T,
    Space, Enter, Escape, Tab,
    ArrowUp, ArrowDown, ArrowLeft, ArrowRight,
    // ⚠️ **2026-10-15 更正**：这一行原来是"`F1, F2, F3, F4, // S11 的 Debug Overlay 用`"，
    //    而实际用到的只有 **`F2`**（几何层，C13）+ **`F4`**（快照，C14）。
    //    `F1` / `F3` 在这台机器上**收不到**（被别的软件抢占）⇒ 别看到它们"有映射"就以为可用。
    F1, F2, F3, F4,

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

    // S12（2026-10-16 拍板，分叉 B = **甲′**）：**原地转身键** —— `Z` 逆时针 45°、`C` 顺时针 45°。
    //   · 为什么用**字母键**：`F1`/`F3` 在本机被别的软件抢占（`TECH_DEBT` C37），`F5` 在 VS Code 里
    //     是"启动调试" ⇒ 字母键不在那些全局热键的靶场上。
    //   · 为什么放在**枚举末尾**（而不是插进上面 `W..R` 那一行）：不挪动既有键的值 ——
    //     `tests/test_input_state.cpp` 里那批断言是按"具体键 + 具体槽位"写的，挪了就得跟着改。
    //   · 与 `T` 一样**不进 HUD 提示行**：提示行宽度有溢出用例钉着
    //     （`Game.HudFitsInTwoLinesWithoutOverflow`）⇒ 键位写在 README / Release 的键位表里。
    //   · ⚠️ 它**受玩法门控**（`policy.readMoveKeys`）—— 暂停时不许转身，见 `Game.cpp` ③c。
    Z, C,

    // S12（2026-10-16，第 4 笔）：**切 2D/3D 视角键** —— `V`（主键）+ `F5`（备份）。
    //   · 主键还是**字母键**：`F1`/`F3` 在本机被抢、`F5` 在 VS Code 里是"启动调试"
    //     ⇒ 字母键不在那些全局热键的靶场上（同 `T`/`Z`/`C` 的先例，`TECH_DEBT` C37）。
    //   · `F5` 是 `PLAN` 原稿的键 ⇒ 映射保留，当**备份**（万一 `V` 也收不到）。
    //   · 与 `T` 一样**不进 HUD 提示行**（提示行有溢出用例钉着）⇒ 键位写在 README / Release 表里。
    //   · ⚠️ 它**不受玩法门控**（视图行为：暂停时也能切）—— 见 `Game.cpp` ④f。
    V, F5,

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
    map[0x54] = Key::T;   // 'T'  VK_T（S11 的 Debug Overlay 开关，2026-10-15 加）

    // S12：原地转身（**玩法键**，2D / 3D 都生效；受玩法门控）
    map[0x5A] = Key::Z;   // 'Z'  VK_Z（逆时针 45°）
    map[0x43] = Key::C;   // 'C'  VK_C（顺时针 45°）

    // S12：切 2D/3D 视角（**视图行为**，不受玩法门控）
    map[0x56] = Key::V;    // 'V'  VK_V（主键）
    // ⚠️ `VK_F5`（0x74）与上面的 `VK_F1..F4`（0x70..0x73）**恰好连续**，但它在枚举里
    //    排在**末尾**（`Key::F5`）—— 枚举顺序与 VK 顺序**无关**，别按"连续"去推枚举值。
    map[0x74] = Key::F5;   // VK_F5（备份键；⚠️ VS Code 里 F5 = "启动调试"，只当备份）

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
