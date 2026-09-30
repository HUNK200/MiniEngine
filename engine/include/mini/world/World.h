#pragma once

#include "mini/world/Entity.h"
#include "mini/world/TileMap.h"

#include <cstddef>   // std::size_t（私有 IndexOf 的返回类型）
#include <cstdint>
#include <span>
#include <vector>

// ============================================================
// world/World.h —— 实体容器 + 每帧推进（S5 步骤 2）
//
// 这一层干什么：装实体、按 id 找、每帧把速度积分成位移、以及延迟删除。
//   **它不读输入、不画东西、不认识终端** → 可以完全离线单测
//   （S5 决策 6：输入→速度由 game 层做，world 只做积分）。
// 依赖谁：同层的 Entity.h / TileMap.h + 标准库。
// 被谁用：game/Game.cpp（步骤 3）持有它；tests/test_world.cpp 的 World suite。
//
// ⚠️ 本阶段唯一的"物理"就是 `pos += perSecond * dt`（PLAN:982：纯运动学，
//    无重力、无阻尼、无迭代求解）。S7 的分轴碰撞会插在同一个循环里。
//
// ⚠️ **遍历中禁止 Spawn / 删除**：
//    · 删除 → 走 MarkForRemoval + FlushRemovals（容器操作留到遍历之外做）
//    · Spawn → 会让 Entities() 的 span 与 Find() 的指针**一起失效**
//      （vector 可能扩容）；FlushRemovals 的 swap-erase 同样让它们失效
//    这条规矩有 3 个用例钉着，不是"注释说说"（PLAN:967 的 E-9）。
// ============================================================

namespace mini::world {

class World {
public:
    // 内部构造一张 mapW×mapH 的格子表（尺寸非正时照旧抛 std::invalid_argument）。
    // 当前用例都只用它当"世界有个地图"这件事实，不关心内容。
    World(int mapW, int mapH);

    // 分配一个新 id、推入、返回它。prototype 的 id 字段会被**覆盖**。
    // ⚠️ **刻意不标 noexcept**：内部 push_back 在 bad_alloc 时会抛 ——
    //    标上就等于声明"分配失败直接 terminate"（施工图原文标了，已改：落地偏差 5）。
    [[nodiscard]] EntityId Spawn(const Entity& prototype);

    // 线性扫。找不到返回 nullptr（**永远不会"找到错的"**，见 EntityId 的注释）。
    // ⚠️ 返回的指针**只在本次调用内有效**：任何 Spawn / FlushRemovals 之后失效。
    //
    // 为什么两版都要：只有非 const 版 → `const World&` 上根本调不了
    //   （HUD / 将来的 Debug Overlay / 单测里的 const 引用全被挡住）；
    //   只有 const 版 → 改不了速度（Game::OnUpdate 没得写）。
    // 为什么两版**不各写一遍**：共用一个私有 IndexOf()，而 FlushRemovals 也要"找下标"
    //   → 一份扫描逻辑、三个调用点（落地偏差 6）。
    [[nodiscard]] Entity*       Find(EntityId id) noexcept;
    [[nodiscard]] const Entity* Find(EntityId id) const noexcept;

    // ⚠️ 返回的 span 在**任何 Spawn 之后失效**（vector 可能扩容）。别在持 span 时 Spawn。
    // 谁会用：渲染遍历（game 的 DrawEntities）—— 它只需要"看一眼"，不需要按 id 找。
    [[nodiscard]] std::span<Entity>       Entities()       noexcept;
    [[nodiscard]] std::span<const Entity> Entities() const noexcept;

    // 地图的所有权在 World 手里，这里只是把它借出去（game 铺地图内容用）。
    [[nodiscard]] TileMap&       Map()       noexcept;
    [[nodiscard]] const TileMap& Map() const noexcept;

    // 本阶段唯一的"物理"：对每个 **active** 实体做 pos += perSecond * dt。
    // ⚠️ 不夹 dt —— ctx.deltaSeconds 已经在 S3 的 ClampDelta 里夹过
    //    （S5 坑 #7：夹两次不会错，但会掩盖"到底谁夹的"）。
    // 顺序：**先由调用方写好速度，再调它**（见 S5_CHECKLIST 7.1 的帧序）。
    void Update(float dt) noexcept;

    // 延迟删除：只登记，不动容器。重复登记同一个 id 是**幂等**的。
    // ⚠️ 登记 ≠ 失效：Update 仍然会更新它，Find 仍然找得到它 ——
    //    只有 FlushRemovals 之后它才真的消失（用例 MarkedEntitySurvivesUntilFlush 钉着）。
    // ⚠️ **刻意不标 noexcept**：push_back 会抛（同 Spawn，落地偏差 5）。
    void MarkForRemoval(EntityId id);

    // 真正删除：swap-erase（把队尾搬到空位）。
    // ⚠️ **会打乱实体顺序** —— 这是"O(1) 搬运"的直接代价，不是 bug；
    //    任何依赖 Entities() 顺序的代码都是它自己的 bug。
    void FlushRemovals() noexcept;

private:
    // 线性扫，返回实体在 m_entities 里的下标；**找不到、或 id 无效 → 返回 m_entities.size()**。
    // （索引永远 < size，所以拿 size() 当"没找到"的哨兵，不必再引入一个 npos 常量。）
    // 谁用：Find 的非 const 版、Find 的 const 版、FlushRemovals —— 一份扫描，三个调用点。
    [[nodiscard]] std::size_t IndexOf(EntityId id) const noexcept;

    TileMap               m_map;
    std::vector<Entity>   m_entities;
    std::vector<EntityId> m_pendingRemove;   // 只在遍历之外消费
    std::uint32_t         m_nextId = 1;      // 0 留给"无效 id"；**只增不减**
};

} // namespace mini::world
