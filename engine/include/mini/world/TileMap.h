#pragma once

#include "mini/math/MathUtils.h"

#include <cstddef>
#include <cstdint>
#include <vector>

// ============================================================
// world/TileMap.h —— 大地图的格子表（S5）
//
// 这一层干什么：一张 W×H 的格表，回答"第 (tx, ty) 格是什么"。
//   **它不读输入、不画东西、不认识终端** → 可以完全离线单测。
// 依赖谁：math/MathUtils.h（**只用 FloorToInt**）+ <cstddef> / <cstdint> / <vector>。
//   ⚠️ 2026-09-30 放宽了原方案的"连 math 都不用"：ToIndex 就是 floor，
//      与其在这里再手写一份（两份实现会各自漂移），不如复用 S4 那个
//      带用例、带 constexpr 的 FloorToInt。world → math 是 PLAN A.3 允许的向下依赖。
// 被谁用：world::World 持有它（步骤 2）；game/Game.cpp 用它铺地图内容（步骤 3）。
//
// ⚠️ 两条"死在注释里"的约定（都不是笔误，是决定）：
//
//   1. **越界一律当 Wall**：`At` 越界返回 `Wall`、不抛异常、不返回 Empty。
//      收益：世界外都是墙 → S7 的碰撞直接得到"角色永远走不出地图"，
//      不用再写一遍边界判断。
//      代价：**S6 的相机不能靠 `At()` 判断边界** —— 必须自己用 Width()/Height() 夹取
//      （否则"世界外是墙"会让相机以为到处都是墙）。
//
//   2. **世界坐标 → tile 下标用 floor，不是 `(int)` 截断**（PLAN A.5 / E-8）：
//      负数下 `(int)-0.1f == 0`，会指向**上一格** —— 表现为"画面整体偏移半格"
//      或"走位比视觉慢一格"。这是本文件唯一一处需要小心取整的地方。
// ============================================================

namespace mini::world {

// 格子类型。底层类型取 `uint8_t` 是为了让 200×200 的地图只占 40 KB
// （与 FrameBuffer 的 Cell 同一套路：能塞小就别塞大）。
enum class Tile : std::uint8_t {
    Empty = 0,   // 可走（"可走"这件事由 S7 的碰撞来解释）
    Wall  = 1,   // 不可走
};

class TileMap {
public:
    // w / h 非正时抛 std::invalid_argument（与 FrameBuffer 同一规矩：
    // 0 宽的地图没有意义，而且要挡住"从终端读到 0 就一路传下来"这条链）。
    // fill 是初始内容 —— 默认全 Empty。
    TileMap(int w, int h, Tile fill = Tile::Empty);

    [[nodiscard]] int Width()  const noexcept { return m_w; }
    [[nodiscard]] int Height() const noexcept { return m_h; }

    // 谁会用：At/Set 内部（判越界）；S6 的相机夹取边界时也要用它
    //（见文件头约定 1 —— 相机**不能**靠 At() 判边界）。
    [[nodiscard]] bool InBounds(int tx, int ty) const noexcept;

    // ⚠️ 越界 → **Wall**（不是 Empty、不抛异常），见文件头约定 1。
    [[nodiscard]] Tile At(int tx, int ty) const noexcept;

    // 越界**静默忽略**（与 FrameBuffer::Set 同一条规矩：S7 之前 `@` 真的会走出地图，
    // 那时"写出去一格"不该变成异常，也不该裁剪调用方）。
    void Set(int tx, int ty, Tile t) noexcept;

    // 世界坐标（格，float）→ tile 下标：**floor**（见文件头约定 2）。
    //
    // 谁会用：game 层把 `@` 的浮点位置翻译成"HUD 里显示在第几格"（步骤 3）；
    //   S7 的碰撞会用它把"下一帧的位置"换算成格子。
    //   它是 S4 那批"只有测试调用"的工具里**第一个上生产**的（TECH_DEBT B5 的 D 类）。
    //
    // ⚠️ 输入约束（沿用 math::FloorToInt 的既定约定，这里不重复防御）：
    //   必须是**有限值**且 |v| < 2^31；NaN 或越界时 static_cast<int> 是 UB。
    //   真出现 NaN 要去**它产生的地方**查，而不是在这里兜。
    [[nodiscard]] static constexpr int ToIndex(float worldCoord) noexcept
    {
        return mini::math::FloorToInt(worldCoord);
    }

private:
    // 线性化索引：index = ty * m_w + tx（**行优先**，与 FrameBuffer 一致）。
    // ⚠️ stride 是**宽** m_w，不是高 —— 写成 m_h 在非方阵下会让两格共用同一格，
    //    单测 TileMap.IndexIsLinearized 用 5×3 专门钉这一条（方阵下测不出来）。
    [[nodiscard]] std::size_t Index(int tx, int ty) const noexcept;

    int               m_w = 0;
    int               m_h = 0;
    std::vector<Tile> m_tiles;   // 线性化：index = ty * m_w + tx
};

} // namespace mini::world
