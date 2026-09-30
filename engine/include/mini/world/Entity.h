#pragma once

// ============================================================
// world/Entity.h —— 实体与它的两个最小组件（S5）
//
// 这一层干什么：定义"世界里有个东西"这件事的**最小形态**：
//   一个 Entity = 一个身份(id) + 一个位置 + 一个速度 + 一个字符 + 一个 active 标记。
// 依赖谁：math/Vec2.h（以及它带的 MathUtils.h）+ <cstdint>。
// 被谁用（已核对）：本阶段唯一的调用者是 game/Game.cpp（步骤 3 才建）；
//   之后的 S7(碰撞) / S8(敌人) / S9(子弹) 都会往 Entity 里**加字段**，
//   而不是新建"另一种实体类型"。
//
// ⚠️ **这是 PLAN D.1 的 D-0/D-1 形态**：所有实体共用这一个结构体、都有全部字段，
//   用 `active` 区分，**不做 ECS、不做组件指针、不做按类型分容器**。
//   PLAN:920 的结论是"停在 D-1，永远不到 D-3" —— 本文件就是那个起点。
//   判断依据很简单：现在只有一个实体、只有一个会动的逻辑，
//   任何"按组件拆分"的收益都还没有出现（A.4：结构被复杂度逼出来）。
//
// ⚠️ 这里没有成员函数（除了 EntityId 那一个谓词）：Entity 是**数据**，
//   行为在 world::World::Update（步骤 2）和 game::Game（步骤 3）里。
//   "数据在哪、行为在哪"分开写，是为了让 world 层能离线单测。
// ============================================================

#include "mini/math/Vec2.h"

#include <cstdint>

namespace mini::world {

// 实体的**句柄**（不是下标、不是指针）。0 = 无效。
//
// ⚠️ 为什么不用 `std::vector` 的下标：`swap-erase` 之后下标会指向**另一个**实体，
//    而且错得**静默无声** —— 最难查的那一类。自增 id 永不复用，于是"悬空 id"
//    只会表现为"找不到"（Find 返回 nullptr），不会"找错"。
// 它是**值类型**（4 字节、可随便拷贝、可存进容器）：所以可以按值传、按值比较。
struct EntityId {
    std::uint32_t value = 0;

    // 谁会用：World::Spawn 之后到处都是"这个 id 还有效吗"；也用来把
    // `EntityId{}` 表达成"没有实体"（例如 game 里"玩家还没出生"）。
    [[nodiscard]] constexpr bool IsValid() const noexcept { return value != 0; }

    // 只写 ==，`!=` 由 C++20 自动生成。比较是**按值**的：两个相同的 id
    // 指向同一个实体（id 是句柄，不存在"两个不同的句柄指向同一实体"这回事）。
    friend constexpr bool operator==(EntityId, EntityId) noexcept = default;
};

// 位置。单位 = **格（float）**，原点 = 地图左上角，X 向右、Y 向下。
// ⚠️ `(0.5, 0.5)` 是**第 1 格（索引 (0,0)）的中心**，不是"第 0 格" —— 换个说法：
//    `world 0.0` 是格子的**角**，`world 0.5` 才是**中心**（PLAN A.5）。
//    取整一律用 **floor**：`floor(0.5) == 0` 就是"这个点所在的格"，于是 HUD 显示的 tile
//    与画面落点**永远一致**。（原方案在这里写"画在哪一列用 round"已废弃：
//    它和用例 23 的期望值差一格，2026-09-30 统一成 floor，见 S5_CHECKLIST 7.2。）
//
// 为什么包成 struct 而不是直接在 Entity 里放两个 float：
//   它是 S7 的 Collider、S9 的朝向/枪口的自然宿主，现在独立出来只多一个类型名。
//
// ⚠️ 类型名必须写全 **`mini::math::Vec2`**：本文件在 `mini::world` 里，
//    裸写 `Vec2` **找不到**（它在下一层命名空间 `mini::math`）—— MSVC 会报
//    C3646「"pos": 未知重写说明符」+ 一串 `{` 的语法错误，很难一眼对上。
//    别为了好看把它"简化"回 `Vec2`。（施工图 6.1 原文就是裸写，见落地偏差 4。）
struct Transform {
    mini::math::Vec2 pos{};
};

// 速度。单位 = **格/秒**（不是格/帧）。
// 用"格/秒"是本阶段的核心：`pos += perSecond * dt` 之后位移与帧率无关；
// 写成"格/帧"在 60fps 与 240fps 下速度会差 4 倍。
struct Velocity {
    mini::math::Vec2 perSecond{};
};

// 一个实体 —— 也就是"游戏里的一个具体实体"。
struct Entity {
    EntityId  id{};                  // 由 World::Spawn 分配；prototype 里写什么都会被覆盖
    Transform transform{};
    Velocity  velocity{};
    char      symbol = '?';          // 画在屏幕上用哪个字符（玩家是 '@'）
    bool      active = true;         // false = 不参与 Update（不是"删掉"，只是不更新）
};

// 布局自检（先例：Cell.h 的 static_assert、S4 的 Vec2/AABB/Ray）。
// 字段顺序/类型一旦变化，这里会**先报错**，比"跑起来行为变了"早一步发现。
//   4(id) + 8(Transform) + 8(Velocity) + 1(symbol) + 1(active) = 22
//   → 按 alignof(float) == 4 上取整 = 24
static_assert(sizeof(EntityId) == 4, "EntityId 必须是 4 字节：它是值语义的句柄");
static_assert(sizeof(Entity) == 24, "Entity 的布局变了：确认这是有意的，再改这个数");

} // namespace mini::world
