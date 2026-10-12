#include "MiniTest.h"

#include <game/GameState.h>

#include <cstddef>   // std::size_t

// ------------------------------------------------------------
// game/GameState 的单元测试（S10 步骤 1）
//
// ⚠️ 这一层**完全离线**：它不认识 `Entity` / `World` / 输入 / 屏幕，也不 include 任何 `mini/` 头
//    （`GameState.h` 自己就只有 `<cstddef>` + `<cstdint>`）。
//    所以这里能**穷举整张表**——"4 状态 × 5 事件" = 20 个组合逐个走一遍。
//    这正是分叉 ① 取乙换来的东西："暂停时世界真的停了"从"读 `OnUpdate` 的 `switch` 时
//    眼睛别漏行"变成一条**可断言的**性质。
//
// ⚠️ 两条纪律（沿用 `test_bullet.cpp` 的）：
//   · **枚举不能塞 `MINI_CHECK_EQ`**（它要 `operator<<`，而 scoped enum 没有隐式转换）
//     ⇒ 一律写 `MINI_CHECK(a == b)`；
//   · **期望值不许从被测函数反推** —— 转移表单独在下面写成 `kLegalEdges` 那张小表
//     （与 `S10_CHECKLIST` §6.1 的表逐条对应），否则"拿实现算期望值"就是空断言。
// ------------------------------------------------------------

namespace {

using game::GameState;
using game::kStateCount;
using game::kStatePolicies;
using game::NextState;
using game::PolicyOf;
using game::StateEvent;
using game::StatePolicy;

// 四个状态（从枚举本身取，不写 `0..3` 这种魔法数）。
constexpr GameState kAllStates[kStateCount] = {
    GameState::Title, GameState::Playing, GameState::Paused, GameState::GameOver,
};

constexpr std::size_t kEventCount = 5;

constexpr StateEvent kAllEvents[kEventCount] = {
    StateEvent::None, StateEvent::StartPressed, StateEvent::PauseToggled,
    StateEvent::RestartPressed, StateEvent::PlayerDied,
};

// ── 转移表（**独立于 `NextState` 写死的期望值**）────────────────────────────
// 与 `S10_CHECKLIST` §6.1 的转移表逐条对应，共 **6 条合法边**。
struct Edge {
    GameState  from;
    StateEvent event;
    GameState  to;
};

constexpr Edge kLegalEdges[] = {
    {GameState::Title,    StateEvent::StartPressed,   GameState::Playing},   // Enter 开始
    {GameState::Playing,  StateEvent::PauseToggled,   GameState::Paused},    // Esc 暂停
    {GameState::Playing,  StateEvent::PlayerDied,     GameState::GameOver},  // HP 归零
    {GameState::Paused,   StateEvent::PauseToggled,   GameState::Playing},   // Esc 恢复
    {GameState::GameOver, StateEvent::RestartPressed, GameState::Playing},   // R 重开
    {GameState::GameOver, StateEvent::PauseToggled,   GameState::Title},     // ★ Esc 回标题（附加项）
};

constexpr std::size_t kLegalEdgeCount = sizeof(kLegalEdges) / sizeof(kLegalEdges[0]);

[[nodiscard]] constexpr bool IsLegalEdge(GameState s, StateEvent e) noexcept
{
    for (const Edge& edge : kLegalEdges) {
        if (edge.from == s && edge.event == e) {
            return true;
        }
    }
    return false;
}

// 只在合法边上调用（非法边由用例 2 负责）。
[[nodiscard]] constexpr GameState ExpectedAfterLegal(GameState s, StateEvent e) noexcept
{
    for (const Edge& edge : kLegalEdges) {
        if (edge.from == s && edge.event == e) {
            return edge.to;
        }
    }
    return s;
}

// 五个字段全为假？（"冻结"这个词的全部含义）
[[nodiscard]] bool IsFullyInert(const StatePolicy& p) noexcept
{
    return !p.simulate && !p.readMoveKeys && !p.readFireKey && !p.advanceClock && !p.advanceWave;
}

[[nodiscard]] bool SamePolicy(const StatePolicy& a, const StatePolicy& b) noexcept
{
    return a.simulate == b.simulate && a.readMoveKeys == b.readMoveKeys &&
           a.readFireKey == b.readFireKey && a.advanceClock == b.advanceClock &&
           a.advanceWave == b.advanceWave;
}

} // namespace

// ------------------------------------------------------------
// 1. 六条合法边逐条走一遍
// ------------------------------------------------------------
MINI_TEST(GameState, NextStateWalksEveryLegalEdge)
{
    int legalSeen = 0;
    for (const GameState s : kAllStates) {
        for (const StateEvent e : kAllEvents) {
            if (!IsLegalEdge(s, e)) {
                continue;
            }
            ++legalSeen;
            MINI_CHECK(NextState(s, e) == ExpectedAfterLegal(s, e));
        }
    }

    // ⚠️ 六条边**一条都不能漏**：漏了说明"枚举出来的边"与"实现里处理的边"对不上
    //    （只测实现处理了的那几条是空转，S8 第一次反向验证就栽在这上面）
    MINI_CHECK_EQ(legalSeen, static_cast<int>(kLegalEdgeCount));
}

// ------------------------------------------------------------
// 2. 非法边一律"不转移"（共 10 条）
// ------------------------------------------------------------
MINI_TEST(GameState, IllegalEdgesAreNoOps)
{
    int illegalSeen = 0;
    for (const GameState s : kAllStates) {
        for (const StateEvent e : kAllEvents) {
            if (e == StateEvent::None) {
                continue;   // `None` 由用例 3 单独管
            }
            if (IsLegalEdge(s, e)) {
                continue;
            }
            ++illegalSeen;
            MINI_CHECK(NextState(s, e) == s);   // ★ 非法边 = 这一帧什么都不做
        }
    }

    // 20 个组合 − 6 条合法边 − 4 个 `None` = **10 条非法边**（施工图 §10.1 用例 2 的数）
    const int expected = static_cast<int>(kStateCount * kEventCount) -
                         static_cast<int>(kLegalEdgeCount) - static_cast<int>(kStateCount);
    MINI_CHECK_EQ(illegalSeen, expected);

    // ⚠️ 点名那条**防御分支**（施工图专门交代过）：暂停期间 `simulate == false` ⇒
    //    敌人不在 ⑤b 跑 ⇒ 伤害事件**不该被造出来**。"`Paused` 收到 `PlayerDied` 不转移"
    //    是把这条推理写进代码的地方 —— 它正常路径不可达，但按"实现了的分支必须有用例"要钉住。
    MINI_CHECK(NextState(GameState::Paused, StateEvent::PlayerDied) == GameState::Paused);
}

// ------------------------------------------------------------
// 3. `None` 在四个状态下都不转移
// ------------------------------------------------------------
MINI_TEST(GameState, NoneEventNeverTransitions)
{
    for (const GameState s : kAllStates) {
        MINI_CHECK(NextState(s, StateEvent::None) == s);
    }
}

// ------------------------------------------------------------
// 4. `R` 只在 `GameOver` 有效
// ------------------------------------------------------------
MINI_TEST(GameState, RestartOnlyFromGameOver)
{
    MINI_CHECK(NextState(GameState::GameOver, StateEvent::RestartPressed) == GameState::Playing);
    MINI_CHECK(NextState(GameState::Title,    StateEvent::RestartPressed) == GameState::Title);
    MINI_CHECK(NextState(GameState::Playing,  StateEvent::RestartPressed) == GameState::Playing);
    MINI_CHECK(NextState(GameState::Paused,   StateEvent::RestartPressed) == GameState::Paused);
}

// ------------------------------------------------------------
// 5. 死亡事件只从 `Playing` 出发
// ------------------------------------------------------------
MINI_TEST(GameState, PlayerDiedOnlyFromPlaying)
{
    MINI_CHECK(NextState(GameState::Playing,  StateEvent::PlayerDied) == GameState::GameOver);
    MINI_CHECK(NextState(GameState::Title,    StateEvent::PlayerDied) == GameState::Title);
    MINI_CHECK(NextState(GameState::Paused,   StateEvent::PlayerDied) == GameState::Paused);
    MINI_CHECK(NextState(GameState::GameOver, StateEvent::PlayerDied) == GameState::GameOver);
}

// ------------------------------------------------------------
// 6. `Esc` 在 Playing/Paused 之间是**自反**的（"切换"而不是"进入暂停"）
// ------------------------------------------------------------
MINI_TEST(GameState, PauseTogglesBothWays)
{
    MINI_CHECK(NextState(GameState::Playing, StateEvent::PauseToggled) == GameState::Paused);
    MINI_CHECK(NextState(GameState::Paused,  StateEvent::PauseToggled) == GameState::Playing);

    // 连按两次回到原状态 —— HUD 与用例都依赖这条（"按一下暂停、再按一下接着玩"）
    const GameState paused = NextState(GameState::Playing, StateEvent::PauseToggled);
    MINI_CHECK(NextState(paused, StateEvent::PauseToggled) == GameState::Playing);
}

// ------------------------------------------------------------
// 6b. ★ 附加项那条边：`GameOver` 下同一个 `Esc` 回**标题**
// ------------------------------------------------------------
MINI_TEST(GameState, EscapeFromGameOverReturnsToTitle)
{
    MINI_CHECK(NextState(GameState::GameOver, StateEvent::PauseToggled) == GameState::Title);

    // 回到标题之后 `Enter` 又能开始 ⇒ `Title` 不再只在进程启动时可见一次
    MINI_CHECK(NextState(GameState::Title, StateEvent::StartPressed) == GameState::Playing);

    // ⚠️ **同一个事件在两个状态下去向不同**（`Paused` 是"恢复"、`GameOver` 是"回标题"）——
    //    这正是它最容易被后来人"顺手统一成 `Paused ↔ Playing`"的地方，所以单独钉一遍。
    MINI_CHECK(NextState(GameState::Paused, StateEvent::PauseToggled) == GameState::Playing);
}

// ------------------------------------------------------------
// 7. 每个状态都取得到策略，且 `Playing` 是全开的
// ------------------------------------------------------------
MINI_TEST(GameState, PolicyOfEveryStateIsTotal)
{
    // ① 取到的**就是表里那一条**（守下标算术：枚举顺序与表顺序必须一一对应）
    for (const GameState s : kAllStates) {
        const std::size_t idx = static_cast<std::size_t>(s);
        MINI_CHECK(SamePolicy(PolicyOf(s), kStatePolicies[idx]));
    }

    // ② `Playing` **五个字段全开**（漏一个 = 那件事在正常游戏里永远不发生）
    const StatePolicy playing = PolicyOf(GameState::Playing);
    MINI_CHECK(playing.simulate);
    MINI_CHECK(playing.readMoveKeys);
    MINI_CHECK(playing.readFireKey);
    MINI_CHECK(playing.advanceClock);
    MINI_CHECK(playing.advanceWave);
}

// ------------------------------------------------------------
// 8. ★ "暂停时世界真的停了"的**机器守卫**：只有 `Playing` 会模拟
// ------------------------------------------------------------
MINI_TEST(GameState, OnlyPlayingSimulates)
{
    for (const GameState s : kAllStates) {
        MINI_CHECK_EQ(PolicyOf(s).simulate, s == GameState::Playing);
    }
}

// ------------------------------------------------------------
// 9. 冻结状态不许推进 `m_elapsed`（HUD 的 `sumdt` 不能在暂停时偷涨）
// ------------------------------------------------------------
MINI_TEST(GameState, FrozenStatesDoNotAdvanceClock)
{
    for (const GameState s : kAllStates) {
        MINI_CHECK_EQ(PolicyOf(s).advanceClock, s == GameState::Playing);
    }
}

// ------------------------------------------------------------
// 10. 冻结状态不许读移动键 / 开火键，也不许推进波次
// ------------------------------------------------------------
MINI_TEST(GameState, FrozenStatesDoNotReadMoveOrFireKeys)
{
    for (const GameState s : kAllStates) {
        MINI_CHECK_EQ(PolicyOf(s).readMoveKeys, s == GameState::Playing);
        MINI_CHECK_EQ(PolicyOf(s).readFireKey,  s == GameState::Playing);
        MINI_CHECK_EQ(PolicyOf(s).advanceWave,  s == GameState::Playing);
    }
}

// ------------------------------------------------------------
// 11. 越界状态是"惰性"的（纯函数要全域安全）
// ------------------------------------------------------------
MINI_TEST(GameState, OutOfRangeStateIsInert)
{
    // 恰好越界一格
    MINI_CHECK(IsFullyInert(PolicyOf(static_cast<GameState>(kStateCount))));

    // 一个远离枚举范围的值
    MINI_CHECK(IsFullyInert(PolicyOf(static_cast<GameState>(200))));

    // ⚠️ 越界状态收到事件也**不许转移** —— 否则一个"野状态"会被某个事件"救"成合法状态
    const GameState bogus = static_cast<GameState>(200);
    for (const StateEvent e : kAllEvents) {
        MINI_CHECK(NextState(bogus, e) == bogus);
    }
}

// ------------------------------------------------------------
// 12. 纯函数：同输入同输出，且没有静态状态
// ------------------------------------------------------------
MINI_TEST(GameState, NextStateIsPureFunctionOfInputs)
{
    for (const GameState s : kAllStates) {
        for (const StateEvent e : kAllEvents) {
            const GameState first = NextState(s, e);

            MINI_CHECK(NextState(s, e) == first);

            // 中间夹几个**别的**调用再算一遍：若函数有静态状态（例如"记住上次的边"），
            // 这一步会露馅 —— 纯函数不该受调用顺序影响。
            (void)NextState(GameState::GameOver, StateEvent::PlayerDied);
            (void)NextState(GameState::Paused,   StateEvent::PlayerDied);
            (void)NextState(GameState::Title,    StateEvent::PauseToggled);

            MINI_CHECK(NextState(s, e) == first);
        }
    }
}
