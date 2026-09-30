#include "mini/world/World.h"

#include <cstddef>

// ============================================================
// World 实现（S5 步骤 2）
//
// 这里**不出现任何终端 / 输入 API**：整层都能离线单测。
// 全部逻辑就四件事：分配 id、线性找、积分、延迟删除。
//
// 一份扫描逻辑（IndexOf）服务三个调用点（Find 两版 + FlushRemovals）——
// 这是"同一个概念只写一份实现"在本文件的落地（见 S5_CHECKLIST 落地偏差 6）。
// ============================================================

namespace mini::world {

World::World(int mapW, int mapH)
    : m_map(mapW, mapH)
{
}

EntityId World::Spawn(const Entity& prototype)
{
    // ⚠️ 先取 id、再 push_back：万一 push_back 抛（bad_alloc），这个 id 就**浪费**了。
    //    这是有意的 —— "id 永不复用"比"id 不浪费"重要得多（PLAN A.5 / EntityId 的注释）。
    const EntityId id{m_nextId};
    ++m_nextId;

    // 拷一份再改 id：调用方传进来的 prototype 不该被我们改（Spawn 的注释说"覆盖 id"，
    // 指的是**分配出来的那个实体**的 id，不是调用方手里的那份）。
    Entity entity = prototype;
    entity.id = id;

    m_entities.push_back(entity);
    return id;
}

std::size_t World::IndexOf(EntityId id) const noexcept
{
    // 无效 id 与"找不到"用**同一个答案**（size()），调用方因此不必分两种情况写
    if (!id.IsValid()) {
        return m_entities.size();
    }

    for (std::size_t i = 0; i < m_entities.size(); ++i) {
        if (m_entities[i].id == id) {
            return i;
        }
    }
    return m_entities.size();   // size() 就是"没找到"的哨兵：合法下标永远 < size
}

Entity* World::Find(EntityId id) noexcept
{
    const std::size_t idx = IndexOf(id);
    return idx < m_entities.size() ? &m_entities[idx] : nullptr;
}

const Entity* World::Find(EntityId id) const noexcept
{
    const std::size_t idx = IndexOf(id);
    return idx < m_entities.size() ? &m_entities[idx] : nullptr;
}

std::span<Entity> World::Entities() noexcept
{
    return std::span<Entity>{m_entities};
}

std::span<const Entity> World::Entities() const noexcept
{
    return std::span<const Entity>{m_entities};
}

TileMap& World::Map() noexcept
{
    return m_map;
}

const TileMap& World::Map() const noexcept
{
    return m_map;
}

void World::Update(float dt) noexcept
{
    for (Entity& entity : m_entities) {
        // active == false → 不参与积分（但**仍在容器里**，也仍然 Find 得到：
        // "停更"和"删除"是两件事，删要走 MarkForRemoval）
        if (!entity.active) {
            continue;
        }
        entity.transform.pos += entity.velocity.perSecond * dt;
    }
}

void World::MarkForRemoval(EntityId id)
{
    // IndexOf 对"无效 id"和"找不到"返回同一个哨兵 → 两种情况一起跳过（静默、幂等）
    if (IndexOf(id) == m_entities.size()) {
        return;
    }

    // 已经登记过就不再登记 —— 同一帧里两个系统都标记同一个实体是很常见的
    for (const EntityId pending : m_pendingRemove) {
        if (pending == id) {
            return;
        }
    }

    m_pendingRemove.push_back(id);
}

void World::FlushRemovals() noexcept
{
    for (const EntityId id : m_pendingRemove) {
        // ⚠️ **每个 id 都重新扫一遍下标**，不能缓存上一轮算出的下标 ——
        //    swap-erase 会把队尾元素搬到空位，缓存的下标会指向另一个实体。
        const std::size_t idx = IndexOf(id);
        if (idx >= m_entities.size()) {
            continue;   // 已经不在了（重复 Flush / 被同一批里的别人删掉）
        }

        m_entities[idx] = m_entities.back();   // swap：把队尾搬过来
        m_entities.pop_back();                 // erase
    }
    m_pendingRemove.clear();
}

} // namespace mini::world
