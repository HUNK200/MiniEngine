#pragma once

// ============================================================
// ai/EnemyBrain.h —— 敌人的"决策"（S8，header-only，纯函数）
//
// 这一层只回答一个问题：**"这一帧我该是什么态、该往哪走、打不打？"**
//   · 它**知道**：自己的位置、目标的位置、巡逻路点、自己的态与定时器、dt。
//   · 它**不知道**：Entity / World / TileMap（不认识格子、不认识墙）、输入、屏幕、颜色。
//   · 它**不做**：不动位置（只**输出速度**）、不做碰撞（位移仍归 World::Update）、
//                不分配、不抛异常、不记状态（公开函数是纯函数，能完全离线单测）。
//
// 依赖谁：math/Vec2.h + 标准库（<algorithm> / <cstddef> / <cstdint> / <span>）。
// 被谁用：game/（S8 步骤 2/3 的 DriveEnemyAi）。engine 里没有任何调用者 ——
//   它和 world/Collision.h 一样是"给 game 用、给 tests 测"的纯逻辑。
//
// ⚠️ 为什么它不在 world/ 里、也不认识 Entity（S8 分叉 ①，见 doc/S8_CHECKLIST.md）：
//   敌人的"位置+速度"归 World 里的 Entity，"状态+血量+定时器"归 game 侧的 EnemyData。
//   这一层只碰 Vec2 和 EnemyState —— 于是它**不需要地图、不需要 Entity** 就能测。
//
// ⚠️ 类型名必须写全 **`mini::math::Vec2`**：本文件在 `mini::ai` 里，裸写 `Vec2` 找不到
//   （它在下一层命名空间 `mini::math`）—— 同 world/Entity.h:58-61 记的那个坑。
//
// ⚠️ 五态的顺序与名字来自 PLAN:562（`Idle → Patrol → Chase → Attack → Dead`）。
//   `switch` 的穷尽性在这次是有意义的：以后加第六态，编译器会提醒你没处理。
// ============================================================

#include "mini/math/Vec2.h"

#include <algorithm>   // std::max
#include <cstddef>     // std::size_t
#include <cstdint>     // std::uint8_t
#include <span>

namespace mini::ai {

// ------------------------------------------------------------
// 状态
// ------------------------------------------------------------

enum class EnemyState : std::uint8_t {
    Idle = 0,   // 出生后的短暂静止（idleTimer 耗尽即转 Patrol）
    Patrol,     // 沿预置路点巡逻
    Chase,      // 直线朝目标走（**不做寻路**：撞墙会卡住，这是有意保留的动机）
    Attack,     // 贴到攻击距离内：停下 + 按冷却扣血
    Dead,       // 终止态：不再决策、速度恒为 {0,0}
};

// ------------------------------------------------------------
// 参数（默认值表见 doc/S8_CHECKLIST.md 6.4）
//
// 聚合体 + 默认成员初始化（**没有构造函数**），所以 `EnemyTuning{}` 就是默认档，
// 也可以只覆盖其中一两项来造用例。先例：render/Camera.h 的 CameraLimits。
//
// ⚠️ `idleSeconds` 是唯一一个**不被 Decide 读**的字段：它是"Idle 该持续多久"的
//    唯一真相来源，由**调用方**在出生时用它初始化 `BrainInput::idleTimer`。
//    放在这里而不是散在 game/ 里，是为了让手感参数只有一处。
// ------------------------------------------------------------

struct EnemyTuning {
    float sightRange     = 10.0f;   // 进战视距（格）
    float loseRange      = 14.0f;   // 脱战距离（格）—— **必须 > sightRange**，否则会抖
    float attackEnter    = 1.5f;    // 进入攻击的距离（格）
    float attackExit     = 2.0f;    // 退出攻击的距离（格）—— **必须 >= attackEnter**
    float moveSpeed      = 6.0f;    // 速度（格/秒）—— 必须 < Game::kMoveSpeed，否则玩家跑不掉
    float attackCooldown = 0.8f;    // 两次攻击的最小间隔（秒）
    int   attackDamage   = 1;       // 每次攻击扣多少 HP
    float idleSeconds    = 0.5f;    // 出生后静止多久（由调用方写进 idleTimer）
    float arriveRadius   = 0.15f;   // 巡逻"到点"判据的一部分（见 Decide 第 ⑪ 步）
};

inline constexpr EnemyTuning kDefaultTuning{};

// 迟滞是"不抖"的**全部**机制，所以把它钉进编译期 —— 与 Collision.h 用
// static_assert 钉几何不变量同一套路。改了默认值而破坏迟滞，这里先报错。
static_assert(kDefaultTuning.loseRange > kDefaultTuning.sightRange,
              "脱战距离必须 > 视距：否则中间没有迟滞区间，敌人会在 Chase/Patrol 之间抖");
static_assert(kDefaultTuning.attackExit >= kDefaultTuning.attackEnter,
              "退出攻击距离必须 >= 进入攻击距离：否则敌人会在 Chase/Attack 之间抖");
static_assert(kDefaultTuning.sightRange > 0.0f,   "视距必须为正");
static_assert(kDefaultTuning.moveSpeed > 0.0f,    "速度必须为正");
static_assert(kDefaultTuning.attackEnter >= 0.0f, "攻击距离必须非负");
static_assert(kDefaultTuning.arriveRadius >= 0.0f, "到点半径必须非负");

// ------------------------------------------------------------
// 入参 / 出参
//
// 都是聚合体（**没有构造函数**），就地构造：
//     BrainInput in{ .self = ...,  ... };      // 指定初始化（C++20）
//   ⚠️ 少写一个字段就是**默认值**（selfAlive/targetAlive 默认 true，dt 默认 0）——
//      这比"未初始化内存"安全，但也意味着"忘了填"不会报错。构造后请逐项核对。
//
// ⚠️ 定时器放在入参/出参里（而不是藏在 Decide 内部）是**刻意的**：
//    它让 Decide 成为纯函数 —— 无静态状态、可重放、可拿两遍同样输入验伪。
// ------------------------------------------------------------

struct BrainInput {
    mini::math::Vec2 self{};      // 自己（**本帧积分前**的位置，格中心）
    mini::math::Vec2 target{};    // 目标（玩家），同上

    // 巡逻路点。允许为空 span：此时 Patrol 原地不动（不崩、不除零）。
    // ⚠️ 指向的内容必须活过本次调用（通常指向 game/ 的 constexpr 数组）。
    std::span<const mini::math::Vec2> waypoints{};

    std::uint8_t waypointIdx = 0; // 当前目标路点下标；越界会被先取模（防御）
    EnemyState   state       = EnemyState::Idle;

    float attackTimer = 0.0f;     // 距下次可攻击还剩多少秒（<= 0 表示可以打）
    float idleTimer   = 0.0f;     // 距离"Idle 结束"还剩多少秒

    bool  selfAlive   = true;     // false → 立刻 Dead（调用方用 hp <= 0 算出来）
    bool  targetAlive = true;     // false → 脱战（不追死物）
    float dt          = 0.0f;     // 本帧间隔（S3 已 clamp；这里**不再夹**）
};

struct BrainOutput {
    EnemyState       state{};       // 本帧的态
    mini::math::Vec2 velocity{};    // 格/秒 —— 调用方写回 Entity::velocity
    std::uint8_t     waypointIdx = 0;
    float            attackTimer = 0.0f;   // 推进后的冷却（调用方存回去）
    float            idleTimer   = 0.0f;   // 推进后的静止计时（调用方存回去）
    int              damage      = 0;      // > 0 = 本帧对 target 造成这么多 HP 伤害
};

// ------------------------------------------------------------
// 决策（纯函数）
//
// 优先级顺序**就是** doc/S8_CHECKLIST.md §7.1 的那张表，从上到下：
//     ① 计时器推进 → ② Dead 终止 → ③ 路点索引归一化 → ④ 距离
//     ⑤ 脱战 → ⑥ 进战 → ⑦ 进攻击 → ⑧ 退攻击 → ⑨ 开火 → ⑩ Idle 到期 → ⑪ 输出速度
//
// ⚠️ **⑤⑥ 的先后顺序就是迟滞本身**：先判脱战、再判进战，而 `loseRange > sightRange`
//    → 中间那一段（sight ~ lose）**保持原态**，所以不会抖（用例 HysteresisKeepsChaseUntilLoseRange 钉它）。
//
// ⚠️ **⑤ 只对"交战态"（Chase / Attack）生效**，不是对"所有态"。
//    施工图 §7.1 那一行写的是 `*`，实现时发现它与第 ⑦ 行（Idle 到期才转 Patrol）**互相矛盾**：
//    若对 Idle 也生效，则"目标很远"（开局的常态）会让 Idle 一帧就跳成 Patrol，
//    Idle 形同虚设、`IdleBecomesPatrolAfterIdleSeconds` 永远红。
//    → 收敛成"脱战 = 离开交战态"，语义也更准。这条已登记为落地偏差 2。
//
// ⚠️ ⑨ 开火**不是**"进入 Attack 那一帧才判"：⑦ 只改态、不 return，所以进入 Attack 的那一帧
//    如果冷却已经好，当帧就会打出第一击（否则首次攻击会莫名慢一帧）。
// ============================================================

[[nodiscard]] inline BrainOutput Decide(const BrainInput& in, const EnemyTuning& t) noexcept
{
    BrainOutput out{};
    out.state = in.state;

    // ── ① 计时器推进（先推进、再判据；下限夹 0，不许变负）──────────────────
    //    紧贴上限的那一步夹到 0：`max` 也顺手挡住了"dt 比剩余时间还大"的情形。
    out.attackTimer = std::max(0.0f, in.attackTimer - in.dt);
    out.idleTimer   = std::max(0.0f, in.idleTimer - in.dt);

    // ── ② Dead 是终止态 ────────────────────────────────────────────────
    //    两个入口：调用方说"我死了"（selfAlive），或上一帧已经是 Dead（粘住）。
    //    ⚠️ Dead 分支**不碰路点索引**（原样透传）—— 它已经不再需要巡逻了。
    if (!in.selfAlive || in.state == EnemyState::Dead) {
        out.state       = EnemyState::Dead;
        out.waypointIdx = in.waypointIdx;
        return out;   // velocity 已是 {0,0}、damage 已是 0（BrainOutput 的默认值）
    }

    // ── ③ 路点索引先归一化 ─────────────────────────────────────────────
    //    调用方传进来的下标可能是脏的（越界 / 表变短了）。先取模，后面才敢用它去索引。
    //    ⚠️ `waypointIdx` 是 uint8_t：路由点超过 255 个时这里会静默截断 ——
    //       本项目巡逻环只有几个点，不为此加一条"上限"逻辑（0 调用者不建）。
    const std::size_t waypointCount = in.waypoints.size();
    std::size_t       idx = (waypointCount > 0)
                                ? (static_cast<std::size_t>(in.waypointIdx) % waypointCount)
                                : 0;

    // ── ④ 距离：这就是"感知"的全部 ─────────────────────────────────────
    //    ⚠️ 不查墙、不做视线遮挡（S8 明确不做）→ 隔墙也算"看见"。
    const float dist = (in.target - in.self).Length();

    EnemyState s = in.state;

    // ── ⑤ 脱战（只对交战态生效，见文件头那段）─────────────────────────
    const bool engaged = (s == EnemyState::Chase || s == EnemyState::Attack);
    if (engaged && (!in.targetAlive || dist > t.loseRange)) {
        s = EnemyState::Patrol;
    }

    // ── ⑥ 进战 ────────────────────────────────────────────────────────
    if ((s == EnemyState::Idle || s == EnemyState::Patrol) && in.targetAlive
        && dist <= t.sightRange) {
        s = EnemyState::Chase;
    }

    // ── ⑦ 进攻击（只改态，**不 return** —— 让 ⑨ 当帧就能开火）──────────
    if (s == EnemyState::Chase && dist <= t.attackEnter) {
        s = EnemyState::Attack;
    }

    // ── ⑧ 退攻击 ──────────────────────────────────────────────────
    //    边界口径：`<= attackEnter` 进、`> attackExit` 出，中间那一段（enter ~ exit）
    //    保持原态 —— 这是攻击侧的迟滞。
    if (s == EnemyState::Attack && dist > t.attackExit) {
        s = EnemyState::Chase;
    }

    // ── ⑨ 开火（冷却到才打；打完重置冷却）────────────────────────────
    if (s == EnemyState::Attack && out.attackTimer <= 0.0f) {
        out.damage      = t.attackDamage;
        out.attackTimer = t.attackCooldown;
    }

    // ── ⑩ Idle 到期 → Patrol ──────────────────────────────────────────
    if (s == EnemyState::Idle && out.idleTimer <= 0.0f) {
        s = EnemyState::Patrol;
    }

    // ── ⑪ 输出速度（Chase 朝目标 / Patrol 朝路点 / 其余 {0,0}）─────────
    mini::math::Vec2 toward{};
    bool             moving = false;

    if (s == EnemyState::Chase) {
        toward = in.target - in.self;
        moving = true;
    } else if (s == EnemyState::Patrol && waypointCount > 0) {
        mini::math::Vec2 waypoint = in.waypoints[idx];

        // "到点"判据 = **本帧会到达或越过**，而不是"必须落进 arriveRadius 内"。
        // ⚠️ 写成"落进半径内"的话，一旦某帧位移大于半径（dt 偏大 / 速度偏大），
        //    就会**一步跨过路点** → 下一帧距离开始变大 → "到点"永远不满足
        //    → 敌人在路点两侧来回抽（doc/S8_CHECKLIST.md 坑 4）。
        if ((waypoint - in.self).Length() <= t.moveSpeed * in.dt + t.arriveRadius) {
            idx      = (idx + 1) % waypointCount;   // 环形前进
            waypoint = in.waypoints[idx];
        }

        toward = waypoint - in.self;
        moving = true;
    }
    // Idle / Attack / Dead / 空路点表 → moving 保持 false → velocity 保持 {0,0}

    // ⚠️ 先判"长度平方 > 0"再归一化：`self` 与目标/路点**完全重合**时直接给 {0,0}。
    //    不判也行（Vec2::Normalized() 对零长度返回零向量），但判一次让它显式、
    //    且从此不可能出现 NaN —— 而 `ToIndex(NaN)` 是 UB（TileMap.h:69-71 明写）。
    if (moving && toward.LengthSq() > 0.0f) {
        out.velocity = toward.Normalized() * t.moveSpeed;
    }

    out.state       = s;
    out.waypointIdx = static_cast<std::uint8_t>(idx);
    return out;
}

} // namespace mini::ai
