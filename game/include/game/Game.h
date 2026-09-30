#pragma once

#include "mini/core/Application.h"
#include "mini/render/FrameBuffer.h"
#include "mini/world/World.h"

// ============================================================
// game/Game.h —— 本项目的游戏侧（S5 第一次有"游戏"这个东西）
//
// 这一层干什么：实现 engine 的 IGame（OnStart / OnUpdate / OnRender / ShouldQuit），
//   把"输入"翻译成"速度"、把"世界"画成"屏幕"。
// 依赖谁：engine 的 core / world / render / platform + math。
//   **engine 绝不 include game**（PLAN A.3），所以这条边是单向的。
//
// ⚠️ 与 S3 的 DemoGame（原在 main.cpp）的差别，只有三件事：
//   1. `@` 从"按一次跳一格"变成"按住连续移动"（速度 × dt）
//   2. HUD 从"15 键四态面板"换成**世界坐标面板**（pos / tile / Σdt）
//      → 于是 OnRender **不再读输入**，S3 那条 `const Input*` 缓存（TECH_DEBT C4）直接消失
//   3. 有一条真正的开局流程（OnStart 里建地图、Spawn 玩家）
//
// ⚠️ 本类**不 include `platform/Input.h`**：OnUpdate 只从 `FrameContext` 拿 `const Input&`
//   （Application.h 已前向声明），OnRender 完全不碰输入。
// ============================================================

namespace game {

class Game final : public mini::core::IGame {
public:
    void OnStart() override;
    void OnUpdate(const mini::core::FrameContext& ctx) override;
    void OnRender(mini::render::FrameBuffer& fb) override;
    [[nodiscard]] bool ShouldQuit() const override;

    // —— 下面三个数是**公有** constexpr，这是刻意的：单测要用它们算期望值 ——
    //    tests/test_game.cpp 拿 kMoveSpeed 算"1 秒该走几格 / 位移多少格"，拿 kSpawnX/Y 算
    //    "起点该画在哪一格"。**用例里不许写死格数**，必须由这几个常量算出来
    //    （否则调速度就要改测试，测试会慢慢变成"测空气"）。
    //    放在 public 而不是 private，是因为"测试只碰公开接口"（S5_CHECKLIST 10.4）。
    //
    // ⚠️ 2026-09-30 从 **12 调到 20**（落地偏差 12）。原因是可以量化的：
    //    "跨一格"要按住的时长 = 1 / kMoveSpeed ——  12 格/秒 → **83ms**，
    //    而人手一次轻点只有 30~80ms → 轻点几乎跨不过一格，`@` 看着完全没动
    //    （S5 换掉了 S3 的"按下即 +1 格"边沿触发，改成"速度 × dt"电平触发，
    //      所以"轻点没反馈"是新模型的固有代价，只能靠调参缓解）。
    //    20 格/秒 → 跨一格 50ms，轻点基本都能看见走一格；穿一屏 80 列约 4 秒。
    static constexpr float kMoveSpeed = 20.0f;   // 格/秒（**默认值**，可被构造参数覆盖）
    static constexpr float kSpawnX    = 2.5f;    // 开局位置（world 坐标，格）—— 格中心
    static constexpr float kSpawnY    = 2.5f;

    // 速度可以覆盖：`MiniEngine.exe 60 30` 的第二个参数就是格/秒。
    // 理由：**手感数字应该试出来，而不是猜出来**（试完再决定要不要改上面的常量）。
    // ⚠️ kMoveSpeed 永远是"默认值"：单测用 `Game game;`（走默认）算期望值。
    // 非正数 / NaN → 回落到 kMoveSpeed（见 Game.cpp）。
    explicit Game(float moveSpeed = kMoveSpeed) noexcept;

private:
    static constexpr int kWorldW = 200;          // PLAN：World = 200×200
    static constexpr int kWorldH = 200;

    // 地图布局：**确定性**写死（不随机、不读文件）。
    // 内容 = 地图外沿一圈墙 + **每 10 格一根的规则柱阵** + 一个不对称地标（"一栋楼"）。
    // ⚠️ 规则性是有要求的（2026-09-30 实测被指出"# 摆得没规律"）：柱阵一眼看出是
    //    故意的，且平移一个周期会重合 → 所以还要那个地标才能判断"画面动没动"。
    void BuildDemoMap();

    // 渲染。S5 **没有相机**：视口原点恒为世界 (0,0)，只差一个"顶栏占第 0 行"的 +1。
    // 三个都是 const → 它们用的是 `World` 的 **const 版** Find()/Entities()/Map()
    //（"Find 为什么要有两版"在这里第一次有了真实生产调用者）。
    void DrawMap(mini::render::FrameBuffer& fb) const;
    void DrawEntities(mini::render::FrameBuffer& fb) const;
    void DrawHud(mini::render::FrameBuffer& fb) const;

    mini::world::World    m_world{kWorldW, kWorldH};
    mini::world::EntityId m_player{};         // 0 = 还没出生（OnStart 之前）
    float                 m_elapsed = 0.0f;   // Σdt（秒）—— 10.2 的"自证"用它
    float                 m_moveSpeed = kMoveSpeed;   // 实际用的速度（argv[2] 可覆盖）
    bool                  m_quit = false;     // Q 退出（ShouldQuit 的来源）
};

} // namespace game
