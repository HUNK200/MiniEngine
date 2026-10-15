#include "MiniTest.h"

#include <mini/platform/InputState.h>
#include <mini/platform/KeyCode.h>

#include <cstddef>
#include <cstdint>

// ------------------------------------------------------------
// InputState / KeyCode 的单元测试（S2）。
//
// 这个文件**不需要终端**，也不需要 window：整个四态状态机与 VK 映射表都是
// 纯逻辑（不 include <windows.h>），这正是把 InputState 从 InputWindows 里
// 拆出来的全部理由 —— 输入逻辑里唯一值得自动验证的部分，现在是可测的。
//
// 真正碰终端的那部分（QUICK_EDIT、FOCUS_EVENT、Present）**测不了**，
// 只能靠 S2_CHECKLIST 9.1/9.2 的手动正向 + 反向验证。
// ------------------------------------------------------------

namespace {

using mini::platform::InputState;
using mini::platform::Key;
using mini::platform::KeyFromVirtualKey;
using mini::platform::KeyIndex;
using mini::platform::KeyState;
using mini::platform::kKeyCount;

// 比 int 而不是比枚举：MINI_CHECK_EQ 失败时要用 operator<< 打印两侧的值，
// enum class 没有 operator<<，而 MSVC 对枚举的 << 会挑到整型重载打出数字，
// 不如我们自己显式转成 int，读失败信息时不会含糊。
int St(KeyState s)
{
    return static_cast<int>(s);
}

// VK 码的裸数字（公共头不能 include <windows.h>，所以测试这边也写裸数字）
constexpr std::uint16_t kVkW    = 0x57;
constexpr std::uint16_t kVkUp   = 0x26;
constexpr std::uint16_t kVkEsc  = 0x1B;
constexpr std::uint16_t kVkF1   = 0x70;
constexpr std::uint16_t kVkF7   = 0x76;   // S8 的敌人面板开关（刻意映射；与 F1~F4 不连续）
// ⚠️ 2026-10-10（S9 步骤 3）：`kVkK`（0x4B）连同 `Key::K` 一起删了（`TECH_DEBT` C15 到期）。
//    现在按 K 收不到任何事件 —— `KeyFromVirtualKey(0x4B)` 返回 `Unknown`，
//    与下面那条"有意不映射"的 'B' 走同一条路径（所以这里不再单独钉它）。
constexpr std::uint16_t kVkB    = 0x42;   // 本项目**有意不映射**的键

} // namespace

// 静止状态：什么事件都没来过，所有键都是 Up，没有一个算"按下"
MINI_TEST(InputState, InitialIsUp)
{
    InputState st;

    MINI_CHECK_EQ(St(st.Get(Key::W)), St(KeyState::Up));
    MINI_CHECK_EQ(St(st.Get(Key::Space)), St(KeyState::Up));
    MINI_CHECK_EQ(St(st.Get(Key::Unknown)), St(KeyState::Up));
    MINI_CHECK(!st.IsDown(Key::W));

    // 连续空转多少帧都还是 Up（BeginFrame 不能把"没按过"变成别的什么）
    st.BeginFrame();
    st.BeginFrame();
    MINI_CHECK_EQ(St(st.Get(Key::W)), St(KeyState::Up));
}

// 按下的生命周期：Pressed（一帧）→ Held（一直）→ ...
//
// 顺带压掉"长按自动重复"这条：Win32 送来的是一串 bKeyDown=TRUE 且没有 KEY_UP，
// 所以按住不放会连续调 OnKeyDown —— 这不能把状态打回 Pressed，否则 HUD 上
// "按住"会一直闪成"刚按下"。
MINI_TEST(InputState, DownThenFrameGivesPressedThenHeld)
{
    InputState st;

    st.OnKeyDown(Key::W);
    MINI_CHECK_EQ(St(st.Get(Key::W)), St(KeyState::Pressed));   // 本帧刚按下
    MINI_CHECK(st.IsDown(Key::W));                              // Pressed 也算按下

    st.BeginFrame();
    MINI_CHECK_EQ(St(st.Get(Key::W)), St(KeyState::Held));

    // 长按期间又来了几次"按下"事件（自动重复）—— 必须还是 Held
    st.OnKeyDown(Key::W);
    st.OnKeyDown(Key::W);
    MINI_CHECK_EQ(St(st.Get(Key::W)), St(KeyState::Held));

    st.BeginFrame();
    MINI_CHECK_EQ(St(st.Get(Key::W)), St(KeyState::Held));

    // 别的键不受影响
    MINI_CHECK_EQ(St(st.Get(Key::A)), St(KeyState::Up));
}

// 松开：Released 恰好一帧，然后回 Up。
// 这条正是"四态 vs 三态"的分水岭 —— 三态下 Released 与 Up 是同一个值，
// 这个断言根本写不出来（见 KeyCode.h 里 KeyState 的注释）。
MINI_TEST(InputState, UpGivesReleasedForOneFrame)
{
    InputState st;

    st.OnKeyDown(Key::W);
    st.BeginFrame();                                // → Held

    st.OnKeyUp(Key::W);
    MINI_CHECK_EQ(St(st.Get(Key::W)), St(KeyState::Released));   // 刚抬起，只此一帧
    MINI_CHECK(!st.IsDown(Key::W));                              // 已经不算按下了

    st.BeginFrame();
    MINI_CHECK_EQ(St(st.Get(Key::W)), St(KeyState::Up));         // 之后就是"没按"
    MINI_CHECK(!st.IsDown(Key::W));
}

// 【本文件最重要的一条】一帧内 down + up 的极速点击不能丢。
//
// 桌面上的真实表现：用户飞快敲一下空格，或者某一帧恰好特别长（序列化 + 写终端
// 碰上调度抖动），按下和抬起落在同一帧的事件流里。只靠 prev/now 两个数组的话，
// 这一帧结束时 now == false，按键**整个消失**，玩家看到的是"偶尔没反应"。
// m_pulse 就是为这条存在的；把它删掉这个用例立刻变红（见 S2_CHECKLIST 9.2 的反向验证 3）。
MINI_TEST(InputState, DownAndUpInSameFrameIsNotLost)
{
    InputState st;

    st.OnKeyDown(Key::Space);
    st.OnKeyUp(Key::Space);                          // 同一帧内又抬起了

    MINI_CHECK_EQ(St(st.Get(Key::Space)), St(KeyState::Pressed));   // ← 必须还能看到
    MINI_CHECK(st.IsDown(Key::Space));

    st.BeginFrame();
    MINI_CHECK_EQ(St(st.Get(Key::Space)), St(KeyState::Up));        // 只维持一帧

    // 同帧内 down→up→down 的乱序（自动重复 + 抬起挤在一起）算"按下"
    st.OnKeyDown(Key::Space);
    st.OnKeyUp(Key::Space);
    st.OnKeyDown(Key::Space);
    MINI_CHECK(st.IsDown(Key::Space));
}

// 失焦要把所有键放掉：切走窗口时系统不给 KEY_UP，不清的话回到窗口后
// 按住的 W 会永远卡在 Held（@ 自己一直走）。
MINI_TEST(InputState, FocusLostReleasesEverything)
{
    InputState st;

    st.OnKeyDown(Key::W);
    st.OnKeyDown(Key::D);
    st.BeginFrame();
    MINI_CHECK_EQ(St(st.Get(Key::W)), St(KeyState::Held));
    MINI_CHECK_EQ(St(st.Get(Key::D)), St(KeyState::Held));

    st.OnFocusLost();

    MINI_CHECK_EQ(St(st.Get(Key::W)), St(KeyState::Up));
    MINI_CHECK_EQ(St(st.Get(Key::D)), St(KeyState::Up));
    MINI_CHECK(!st.IsDown(Key::W));
    MINI_CHECK(!st.IsDown(Key::D));

    // 而且不能"下一帧又冒出来"（m_downPrev 也必须被清）
    st.BeginFrame();
    MINI_CHECK_EQ(St(st.Get(Key::W)), St(KeyState::Up));
}

// VK 映射表：项目要用的键必须一一对上，其余（含超出表范围的高位码）一律 Unknown
MINI_TEST(KeyCode, KeyFromVirtualKeyMapsUsedKeys)
{
    MINI_CHECK(KeyFromVirtualKey(kVkW) == Key::W);
    MINI_CHECK(KeyFromVirtualKey(0x41) == Key::A);          // 'A'  VK_A
    MINI_CHECK(KeyFromVirtualKey(0x53) == Key::S);          // 'S'  VK_S
    MINI_CHECK(KeyFromVirtualKey(0x44) == Key::D);          // 'D'  VK_D
    MINI_CHECK(KeyFromVirtualKey(0x51) == Key::Q);          // 'Q'  VK_Q
    MINI_CHECK(KeyFromVirtualKey(0x20) == Key::Space);      // VK_SPACE
    MINI_CHECK(KeyFromVirtualKey(0x0D) == Key::Enter);      // VK_RETURN
    MINI_CHECK(KeyFromVirtualKey(kVkEsc) == Key::Escape);   // VK_ESCAPE
    MINI_CHECK(KeyFromVirtualKey(kVkUp) == Key::ArrowUp);   // VK_UP
    MINI_CHECK(KeyFromVirtualKey(0x25) == Key::ArrowLeft);  // VK_LEFT
    MINI_CHECK(KeyFromVirtualKey(0x27) == Key::ArrowRight); // VK_RIGHT
    MINI_CHECK(KeyFromVirtualKey(0x28) == Key::ArrowDown);  // VK_DOWN
    MINI_CHECK(KeyFromVirtualKey(kVkF1) == Key::F1);        // VK_F1
    MINI_CHECK(KeyFromVirtualKey(0x73) == Key::F4);         // VK_F4
    MINI_CHECK(KeyFromVirtualKey(kVkF7) == Key::F7);        // VK_F7（S8 的敌人面板；不连续所以要单独钉）
    // S11（2026-10-15 加）：overlay 的开关是**字母键 `T`**（F1/F3 在本机被别的软件抢占，
    // 见 `KeyCode.h` 那段注释）。镜像 S9 删 `K` 时的做法：枚举与映射一加，这里就补一条断言。
    MINI_CHECK(KeyFromVirtualKey(0x54) == Key::T);          // 'T'  VK_T

    // 有意不映射：字母 B 与修饰键。映进来只会多出要维护的状态
    MINI_CHECK(KeyFromVirtualKey(kVkB) == Key::Unknown);
    MINI_CHECK(KeyFromVirtualKey(0x10) == Key::Unknown);    // VK_SHIFT
    MINI_CHECK(KeyFromVirtualKey(0x11) == Key::Unknown);    // VK_CONTROL

    // 超出 256 的高位码不能悄悄折到别的键上（参数是 uint16_t 就是为了这条）
    MINI_CHECK(KeyFromVirtualKey(0x0100) == Key::Unknown);
    MINI_CHECK(KeyFromVirtualKey(0xFFFF) == Key::Unknown);
}

// KeyIndex 是"未映射的键互相干扰"的根治点：Invalid 拿不到槽位，
// 所以它们根本写不进任何一个键的状态。下标越界这类错在源头就没了。
MINI_TEST(KeyCode, InvalidKeyIndexIsIgnored)
{
    // Unknown 与 Count 都必须落到哨兵上，不能是合法下标
    MINI_CHECK_EQ(KeyIndex(Key::Unknown), kKeyCount);
    MINI_CHECK_EQ(KeyIndex(Key::Count), kKeyCount);

    // 所有合法键都必须有自己的、互不相同的槽位
    for (int raw = 1; raw < static_cast<int>(Key::Count); ++raw) {
        const std::size_t idx = KeyIndex(static_cast<Key>(raw));
        MINI_CHECK(idx < kKeyCount);
        MINI_CHECK_EQ(idx, static_cast<std::size_t>(raw));
    }

    // 结构不变式：Unknown == 0（数组默认值即"无效"），Count 就是数组长度
    MINI_CHECK_EQ(static_cast<int>(Key::Unknown), 0);
    MINI_CHECK_EQ(kKeyCount, static_cast<std::size_t>(Key::Count));

    // 真金白银那一条：灌一堆无效键之后，真正的键不能被带脏
    InputState st;
    st.OnKeyDown(Key::Unknown);
    st.OnKeyDown(Key::Count);
    st.OnKeyUp(Key::Unknown);
    MINI_CHECK_EQ(St(st.Get(Key::W)), St(KeyState::Up));
    MINI_CHECK(!st.IsDown(Key::W));
}
