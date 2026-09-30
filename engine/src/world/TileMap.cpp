#include "mini/world/TileMap.h"

#include <cstddef>
#include <stdexcept>

// ============================================================
// TileMap 实现（S5 步骤 1）
//
// 这里**不出现任何终端 / 输入 API**：它只是一张格子的数组，
// 所以能被单元测试完整覆盖（不需要终端）。
//
// 两条语义已经在头文件里写死，本文件只负责让它们真的成立：
//   · At 越界 → Wall
//   · Set 越界 → 静默忽略
// ============================================================

namespace mini::world {
namespace {

void ValidateSize(int w, int h)
{
    if (w <= 0 || h <= 0) {
        throw std::invalid_argument("TileMap: width and height must be positive");
    }
}

} // namespace

TileMap::TileMap(int w, int h, Tile fill)
{
    ValidateSize(w, h);
    m_w = w;
    m_h = h;
    // 先转成 size_t 再相乘：避免 w*h 在 int 里先溢出（200×200 当然不会，
    // 但没理由埋这个坑 —— 与 FrameBuffer::Reallocate 同一写法）
    m_tiles.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), fill);
}

bool TileMap::InBounds(int tx, int ty) const noexcept
{
    return tx >= 0 && tx < m_w && ty >= 0 && ty < m_h;
}

Tile TileMap::At(int tx, int ty) const noexcept
{
    if (!InBounds(tx, ty)) {
        return Tile::Wall;   // 约定 1：世界外都是墙
    }
    return m_tiles[Index(tx, ty)];
}

void TileMap::Set(int tx, int ty, Tile t) noexcept
{
    if (!InBounds(tx, ty)) {
        return;              // 越界写静默忽略（与 FrameBuffer::Set 同规矩）
    }
    m_tiles[Index(tx, ty)] = t;
}

std::size_t TileMap::Index(int tx, int ty) const noexcept
{
    // 行优先：index = ty * m_w + tx（与 FrameBuffer::Index 一致）
    return static_cast<std::size_t>(ty) * static_cast<std::size_t>(m_w)
         + static_cast<std::size_t>(tx);
}

} // namespace mini::world
