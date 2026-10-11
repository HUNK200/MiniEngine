#pragma once

// ============================================================
// game/Enemy.h —— 敌人**特有**的数据（S8，分叉 ①）
//
// 为什么不把这些字段加进 `mini::world::Entity`（那是 PLAN D.1 的字面路径）：
//   · 保住 `static_assert(sizeof(Entity) == 24)` 与"engine 一个字节不用动"；
//   · 保住 PLAN A.3 的"engine 不认识 Enemy / 血量"；
//   · 玩家实体不必背 `waypointIdx` / `attackTimer` 这些对它毫无意义的字段。
//   完整取舍见 `doc/S8_CHECKLIST.md` 的「分叉 ①」。
//
// ⚠️ **这是在 `Entity` 之外的第二份真相**，靠 `id` join 回 world 里的那个实体：
//       "位置 + 速度" → 在 `World` 里（`Entity::transform` / `Entity::velocity`）
//       "状态 + 血量 + 定时器 + 巡逻" → 在这里
//   → **删敌人的时候两边必须一起删**，而且两边的 `swap-erase` **顺序互不知情**
//      （`World::FlushRemovals` 会把队尾搬到空位，`m_enemies` 也一样）：
//          ① m_world.MarkForRemoval(id)
//          ② 从 m_enemies 移除（用 std::erase_if，**在遍历之外**）
//          ③ m_world.FlushRemovals()
//     只删一边的两种症状：
//       · 只删 Entity → 悬空 id：`Find` 返回 nullptr（**静默**，靠 DriveEnemyAi 里那句 continue 兜住）
//       · 只删 EnemyData → 实体还在容器里、但再也**没人驱动它**（原地当活靶子）
//     本文件与 `Game.cpp` 的注释只负责把它说清楚；真正的守卫是
//     `Game.KilledEnemyIsRemovedAndLeavesNoDanglingId`（**S9 步骤 4 落地**，2026-10-10）。
//     ⚠️ S8 当时拟的名字是 `RemovingEnemyLeavesNoDanglingId`，但 S8 **写不了**（没有删除
//        路径 ⇒ 会是空用例，见 `S8_CHECKLIST` 偏差 8）；落地时改成了现在这个名字。
//        历史文档里的旧名字**不改**（S8 的档案留原样），以**代码里的名字**为准。
//
// 依赖谁：mini/world/Entity.h（`EntityId`）+ mini/ai/EnemyBrain.h（`EnemyState`）。
// 被谁用：`game/Game.h` 的 `std::vector<EnemyData> m_enemies`（**唯一**持有者）。
// ============================================================

#include "mini/ai/EnemyBrain.h"
#include "mini/world/Entity.h"

#include <cstdint>

namespace game {

struct EnemyData {
    // 指回 `World` 里的那个实体。⚠️ 它是**稳定句柄**（`swap-erase` 之后仍然有效，
    // 见 `world/Entity.h:30-35`）—— 所以这里可以放心按值存，不必存下标。
    mini::world::EntityId id{};

    mini::ai::EnemyState state = mini::ai::EnemyState::Idle;

    // 巡逻：走**哪一条**路线（`Game::kPatrolPaths` 的行号），以及在这条路线**内部**走到第几个点。
    // ⚠️ 分两个字段是因为路线表是**全敌共用**的一张平表：`patrolPath` 决定"这个敌人负责哪一段"，
    //    `waypointIdx` 才是 `ai::EnemyBrain` 每帧要推进、并原样存回来的那个下标。
    std::uint8_t patrolPath = 0;
    std::uint8_t waypointIdx = 0;

    float attackTimer = 0.0f;   // 距下次可攻击还剩多少秒（<= 0 表示可以打）
    float idleTimer   = 0.0f;   // 距离 `Idle` 结束还剩多少秒（出生时由 EnemyTuning::idleSeconds 初始化）

    int hp = 0;                 // <= 0 → 传给 `Decide` 的 `selfAlive` 是 false（S9 的子弹扣它）
};

} // namespace game
