#pragma once

#include <cstddef>   // std::size_t
#include <cstdint>   // std::uint8_t

// ============================================================
// game/GameState.h —— 游戏**状态机**（S10 步骤 1，header-only）
//
// ⚠️ 这个文件**不是**"游戏对象的类"：它和 `Bullet.h` / `EnemyBrain.h` 一样是**无状态模块** ——
//    没有 `struct GameStateData`，状态就是 `Game` 里的一个枚举成员（`m_state`）。
//    本文件只回答两件事：**"发生了某事件之后去哪个状态"** 与 **"在那个状态里哪些系统该跑"**。
//
// 为什么值得单开一个文件（`S10_CHECKLIST` 分叉 ①）：
//   这个状态机的**真内容不是"4 个状态"**，而是"**哪些系统在每个状态下跑**"（4 状态 × ~11 个帧步）。
//   转移表只有 6 条边、错不到哪去；**漏停一个系统**（例如在 `GameOver` 里还在跑 AI）才是真实故障
//   —— 所以那张矩阵必须是**纯数据 + 可断言**的，而不是散在 `OnUpdate` 的一段 `switch` 里
//   （"决策与接线分家"的第 4 次兑现：`Collision.h` → `Bullet.h` → `EnemyBrain.h` → 本文件）。
//
// 依赖谁：**只有 `<cstddef>` + `<cstdint>`**。
//   ⚠️ **不 include 任何 `mini/` 头**：不认识 `Entity` / `World` / `Input` / 屏幕
//   ⇒ 这一层能被**完整离线单测**（同 `Bullet.h` / `EnemyBrain.h` 的纪律）。
//   ⚠️ 代价（要说清楚）：**它不知道"按住 `Esc` 会怎样"** —— 键是从 `Input` 读的，
//      "键 → `StateEvent`"这一步留在 `Game.cpp`（那是接线，不是规则）。
//
// 被谁用：`game/Game.h`（`m_state` 成员 + `OnUpdate` 顶部的 ⓪①）+ `tests/test_gamestate.cpp`。
// ============================================================

namespace game {

// 四个状态。
// ⚠️ 与 `ai::EnemyState` 一样用 `enum class`：用例断言 `State() == GameState::Paused`
//    **不必比浮点/字符串**（同 S8 的理由），HUD 也能直接查表画名字。
// ⚠️ 初值取 `Title`：`PLAN` S10 的可运行效果原话是"**开局提示** → 按 `Enter` → 刷怪 …"。
enum class GameState : std::uint8_t {
    Title = 0,   // 开局提示：什么都不模拟，等 Enter
    Playing,     // 玩：全部系统开
    Paused,      // 暂停（Esc）：全部系统关（只剩清理端与调试出口）
    GameOver,    // 结算（玩家 HP 归零）：全部系统关，等 R（重开）或 Esc（回标题）
};

inline constexpr std::size_t kStateCount = 4;

// 触发转移的"**这一帧发生过什么**"。
// ⚠️ 它是**事件的抽象，不是按键** —— "哪个键算哪个事件"由 `Game.cpp` 决定。
// ⚠️ `PauseToggled` 是**切换**语义而不是"进入暂停"语义，它有**三种去向**：
//       `Playing → Paused` · `Paused → Playing` · `GameOver → Title`
//    最后那条与"恢复"看起来不搭，所以它有专门的用例（`EscapeFromGameOverReturnsToTitle`）
//    + 一场反向验证（R2b）—— 它是最容易被后来人"顺手统一成 `Paused ↔ Playing`"的一条边。
// ⚠️ 优先级（`PlayerDied` 高于其余）不在这里定：那是**接线**（`Game.cpp` 的 ⓪b 顺序）。
enum class StateEvent : std::uint8_t {
    None = 0,        // 这一帧什么都没发生（`NextState` 对它的回答是"留在原状态"）
    StartPressed,    // Enter：只在 Title 有效
    PauseToggled,    // Esc：Playing ↔ Paused，以及 GameOver → Title
    RestartPressed,  // R：只在 GameOver 有效
    PlayerDied,      // 玩家 HP 跨到 <= 0：只在 Playing 有效
};

// ------------------------------------------------------------
// 状态 → "本帧跑哪些系统"。**这是本模块存在的主要理由。**
//
// ⚠️ 表里只能说"**跑不跑**"，**不许**放"怎么跑"（那是 `game` 的接线）——
//    这条边界一破，这个文件就会长成第二个 `Game`。
// ⚠️ **不在这张表里的那几步在任何状态下都跑**：`④ Q 退出` · `④b F7 面板` ·
//    `⑦ 相机 Follow` · `⑧ F4 快照` · `⑨ FlushRemovals` · `⑨b 子弹表压缩`。
//    理由：前两个是"调试出口"（暂停时**恰恰最需要**），后四个是"清理端"（停了只会
//    让已标删的东西留在容器里）。详见 `S10_CHECKLIST` §7.1 理由 3。
// ------------------------------------------------------------
struct StatePolicy {
    bool simulate     = false;   // ⑤b `DriveEnemyAi` / ⑤d `ResolveBullets` / ⑥ `m_world.Update`
    bool readMoveKeys = false;   // ①方向 / ②归一化 / ③写玩家速度 / ③b 写朝向
    bool readFireKey  = false;   // ⑤c `FireBullet`（内含 Ammo 判定）
    bool advanceClock = false;   // ⑤ `m_elapsed += dt`（HUD 里 `sumdt` 那条自证要用）
    bool advanceWave  = false;   // `TickWave`：全灭 → 喘息倒计时 → 下一波
};

// 顺序与 `GameState` **一一对应**（改顺序要同时改这张表和 `PolicyOf`）。
// ⚠️ 三个冻结状态的五个字段**全是 `false`**（"全关"比"关三个开两个"更难写错）。
inline constexpr StatePolicy kStatePolicies[kStateCount] = {
    /* Title    */ StatePolicy{false, false, false, false, false},
    /* Playing  */ StatePolicy{true,  true,  true,  true,  true },
    /* Paused   */ StatePolicy{false, false, false, false, false},
    /* GameOver */ StatePolicy{false, false, false, false, false},
};

namespace detail {

// "有几个状态的 `simulate` 为真" —— 只给下面那条 `static_assert` 用（编译期计算）。
[[nodiscard]] constexpr std::size_t SimulatingStateCount() noexcept
{
    std::size_t n = 0;
    for (std::size_t i = 0; i < kStateCount; ++i) {
        if (kStatePolicies[i].simulate) {
            ++n;
        }
    }
    return n;
}

} // namespace detail

// ⚠️ 下面两条 `static_assert` **不是空断言**（对照 `TECH_DEBT` C18 那条"永远不会红"的版本断言）：
//    它们挡的是"有人改这张表时把某个冻结状态打开"—— 那会让"暂停时世界真的停了"这条性质
//    **静默**失效，现象是"暂停了但敌人在动"。与 `EnemyBrain.h` 把迟滞区间钉进编译期是同一套路。
//    两条合起来正好等价于"**只有 `Playing` 会模拟**"（只用一条 `== 1` 不够：把 `Paused` 打开、
//    同时把 `Playing` 关掉也满足 `== 1`）。
static_assert(detail::SimulatingStateCount() == 1,
              "必须有且只有一个状态会模拟世界");
static_assert(kStatePolicies[static_cast<std::size_t>(GameState::Playing)].simulate,
              "模拟世界的那个状态必须是 Playing —— 否则暂停/标题/结算画面里世界还在跑");

// ⚠️ 加新状态时这条会**先报错**，提醒你同步 `kStatePolicies`（与 `GameState` 的 switch、
//    `EnemyBrain` 的 switch 不写 `default` 是同一个手法：让编译器替你数）。
static_assert(kStateCount == static_cast<std::size_t>(GameState::GameOver) + 1,
              "GameState 里加了新状态：请同步 kStateCount 与 kStatePolicies");

// 取某个状态的策略。
// ⚠️ 越界（**只能由强制转换造出来**，正常路径不存在）→ 返回**全 `false`**：
//    最保守的答案是"什么都不跑"，至少不会误伤世界（同 `FacingGlyph` 那条"越界要有兜底"的纪律）。
[[nodiscard]] constexpr StatePolicy PolicyOf(GameState state) noexcept
{
    const std::size_t i = static_cast<std::size_t>(state);
    return (i < kStateCount) ? kStatePolicies[i] : StatePolicy{};
}

// 事件 → 下一个状态。**纯函数**：同输入同输出、不抛、不分配、无静态状态。
//
// ⚠️ **非法边一律返回 `cur`**（转移表里的 `—` = "这一帧不转移"）：
//      · `StartPressed` 只在 `Title` 有效；
//      · `RestartPressed` 只在 `GameOver` 有效；
//      · `PlayerDied` 只在 `Playing` 有效；
//      · `PauseToggled` 在 `Title` 下**无效**（标题画面没有"暂停"这回事）。
//    用例 `IllegalEdgesAreNoOps` 把全部 10 种非法组合逐条钉住。
// ⚠️ **`Paused` 下收到 `PlayerDied` 走非法边**是刻意的：暂停时 `simulate == false` ⇒
//    敌人根本不在 ⑤b 跑 ⇒ 这个事件**不该被造出来**；返回 `cur` 是**兜底**
//    （防御分支，按"实现了的分支必须有用例"这条纪律由用例 #2 钉住）。
[[nodiscard]] constexpr GameState NextState(GameState cur, StateEvent ev) noexcept
{
    switch (cur) {
    case GameState::Title:
        // 开局提示：只有 Enter 能走出去
        return (ev == StateEvent::StartPressed) ? GameState::Playing : cur;

    case GameState::Playing:
        if (ev == StateEvent::PlayerDied)   { return GameState::GameOver; }
        if (ev == StateEvent::PauseToggled) { return GameState::Paused; }
        return cur;

    case GameState::Paused:
        if (ev == StateEvent::PauseToggled) { return GameState::Playing; }
        return cur;

    case GameState::GameOver:
        if (ev == StateEvent::RestartPressed) { return GameState::Playing; }
        // ⚠️ 同一个 `PauseToggled` 在这里的去向**与 `Paused` 下不同**（回标题而不是恢复）。
        if (ev == StateEvent::PauseToggled)   { return GameState::Title; }
        return cur;
    }

    // 越界枚举（只能由强制转换造出来）：不转移（同 `PolicyOf` 的兜底精神）。
    // ⚠️ 这一句同时让 /W4 闭嘴：上面已经列完所有枚举值，但编译器不知道 switch 的值一定合法。
    return cur;
}

} // namespace game
