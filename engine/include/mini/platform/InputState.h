#pragma once

#include "mini/platform/KeyCode.h"

#include <array>
#include <cstddef>

// ============================================================
// InputState —— 按键状态机（S2 的核心）
//
// 整个 S2 最值得写的一个类，因为它是**唯一**可以被离线单测的输入逻辑：
// 它不 include <windows.h>、不读句柄、不做 IO，只是三个 bool 数组 + 一次转移。
// InputWindows 的责任被压缩到一句话：把 Win32 事件翻译成 OnKeyDown/OnKeyUp 喂进来。
//
// 【三个数组的分工 —— 这是本类全部的设计】
//   m_downAtEnd  本帧事件流结束后，该键的物理状态
//   m_downPrev   上一帧的 m_downAtEnd
//   m_pulse      本帧内**发生过** down（不管之后是否又 up）
//
// m_pulse 为什么必须单独存在：
//   只看 m_downPrev / m_downAtEnd 的话，一帧内 down+up（用户飞快敲一下，
//   或者某一帧恰好特别长）会让这个按键**被完全吃掉**。表现是"偶尔按 W 不动"——
//   到 S5 移动 @ 时会变成极难查的隐性 bug。m_pulse 专门救这一条，
//   并且是全类唯一需要测试专门守着的行为（见 tests/test_input_state.cpp）。
//
// 【帧序】Poll 写状态 → 游戏读 Get()/IsDown() → BeginFrame 推进。
//   写反（BeginFrame 放到读状态之前）会让 Pressed 永远显示不出来。
// ============================================================

namespace mini::platform {

class InputState {
public:
    // Poll 期间：收到一个"按下"事件。
    // 长按的自动重复就是连续多次 OnKeyDown，没有对应的 OnKeyUp ——
    // 所以 m_downAtEnd 保持 true，Get() 自然返回 Held，不需要任何特判。
    void OnKeyDown(Key k) noexcept
    {
        const std::size_t i = KeyIndex(k);
        if (i >= kKeyCount) {
            return;   // Unknown / 越界：**丢掉**。这就是"未映射的键互相干扰"的根治点
        }
        m_downAtEnd[i] = true;
        m_pulse[i]     = true;
    }

    // Poll 期间：收到一个"抬起"事件。
    void OnKeyUp(Key k) noexcept
    {
        const std::size_t i = KeyIndex(k);
        if (i >= kKeyCount) {
            return;
        }
        m_downAtEnd[i] = false;
        // 注意：**故意不碰 m_pulse**。
        // "本帧内见过 down"这件事必须活到帧尾，否则上面那个极速点击就被吃掉了。
    }

    // 窗口失焦。切走窗口时系统不会给你 KEY_UP，按住的键会永远卡在 Held
    // （回到窗口后 @ 会自己一直走），所以这里把三个数组全清掉。
    void OnFocusLost() noexcept
    {
        m_downAtEnd.fill(false);
        m_downPrev.fill(false);
        m_pulse.fill(false);
    }

    // 帧尾推进。**必须在游戏读完之后调**。
    void BeginFrame() noexcept
    {
        m_downPrev = m_downAtEnd;
        m_pulse.fill(false);
    }

    // 查询。判定顺序**不能换**，理由见下。
    [[nodiscard]] KeyState Get(Key k) const noexcept
    {
        const std::size_t i = KeyIndex(k);
        if (i >= kKeyCount) {
            return KeyState::Up;
        }

        const bool was = m_downPrev[i];
        const bool now = m_downAtEnd[i];

        // 长按时 m_pulse 也是 true，所以 Held 必须排在 pulse 前面判，
        // 否则"按住不放"会被误判成"刚按下"，Pressed 会一直闪。
        if (was && now) {
            return KeyState::Held;
        }
        if (was) {
            return KeyState::Released;   // 上一帧按着、这一帧松了
        }
        if (m_pulse[i]) {
            return KeyState::Pressed;    // 含"本帧内 down 又 up"的极速点击
        }
        return KeyState::Up;
    }

    [[nodiscard]] bool IsDown(Key k) const noexcept
    {
        const KeyState s = Get(k);
        return s == KeyState::Pressed || s == KeyState::Held;
    }

private:
    std::array<bool, kKeyCount> m_downAtEnd{};
    std::array<bool, kKeyCount> m_downPrev{};
    std::array<bool, kKeyCount> m_pulse{};
};

} // namespace mini::platform
