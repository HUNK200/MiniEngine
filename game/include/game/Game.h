#pragma once

#include "game/Bullet.h"      // S9：子弹的规则（朝向 + 命中判定纯函数）
#include "game/Enemy.h"
#include "mini/core/Application.h"
#include "mini/math/Vec2.h"
#include "mini/render/Camera.h"
#include "mini/render/FrameBuffer.h"
#include "mini/world/World.h"

#include <vector>

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
// ⚠️ S6 加的第 4 件事：**渲染走相机**，视口跟着 `@` 在地图上滚动。
//   S5 时视口原点恒为世界 (0,0)，所以往右走约 40 格 `@` 就出屏了 —— 那是当时的已知缺口，
//   现在由 `m_camera` + `OriginCol/OriginRow` 补上（相机本体在 `render/Camera.h`，能离线测）。
//   顺带：**碰撞仍然没有**（S7），所以走进地图外时 `@` 会离开画面，这是预期的。
//
// ⚠️ **调试快照键**（2026-10-08 加，为查"看着没贴墙却动不了"那类问题）：
//   按一次就把当前帧的关键参数**追加**写进 `miniengine_debug.log`（实现见 `Game.cpp` 的 `DumpSnapshot`）。
//   它记的东西里有一样是专门为那类 bug 准备的：**盒子压到的 4 个格各自是墙还是空**
//   —— 因为"盒占 4 格、屏幕只画 1 格"，挡住你的常常是**看不见**的那一格。
//   ⚠️ **具体是哪个 F 键是运行时的事**：按键写在 `Game.cpp` 顶部的 `kDumpKey` 一处，
//      并在 HUD 提示里显示出来（不用记文档）。历程：F1 撞别的软件 → F3 也不行 → 现在 **F4**。
//   ⚠️ 这是 S11 的 Logger 之前的**临时出路**（`TECH_DEBT` C14）；它做文件 I/O，
//   只在手动按时发生（会污染那一帧的 `work` 值，属可接受）。
//
// ⚠️ 本类**不 include `platform/Input.h`**：OnUpdate 只从 `FrameContext` 拿 `const Input&`
//   （Application.h 已前向声明），OnRender 完全不碰输入。
//
// ⚠️ S8 加的**第 5 件事**：世界上有敌人了（`E`）。
//   敌人 = 一个普通 `Entity`（`symbol = 'E'`）+ `m_enemies` 里的一条 `EnemyData`。
//   **`engine/` 一个字节都没改** —— 这是本阶段最值钱的性质（对标 S7 的"`game/` 一行没改"）：
//     位移仍然只有一条路径（`World::Update` → `MoveAndCollide`），
//     所以敌人**自动**会撞墙、会沿墙滑，一行都不用写；渲染也照旧扫 `Entities()`。
//   敌人每帧的"决策"在 `DriveEnemyAi`（帧序第 ⑤b 步）—— 它只**写速度**，不碰位置。
// ============================================================

namespace game {

class Game final : public mini::core::IGame {
public:
    void OnStart() override;
    void OnUpdate(const mini::core::FrameContext& ctx) override;
    void OnRender(mini::render::FrameBuffer& fb) override;
    [[nodiscard]] bool ShouldQuit() const override;

    // 玩家当前 HP（S8）。**这是给 HUD 与单测用的公开只读接口** ——
    // 不是"为测试而加的空接口"：`DrawHud` 真的会显示它（S8_CHECKLIST §6.3 的验收要求）。
    // ⚠️ 敌人**看不到**它：`DriveEnemyAi` 用的是私有的 `m_playerHp`。
    [[nodiscard]] int PlayerHp() const noexcept;

    // 击杀数（S9）。同 `PlayerHp`：**真接口**（HUD 第二行会显示它），不是"为测试而加"。
    [[nodiscard]] int Kills() const noexcept;

    // 玩家当前朝向（S9）。给 HUD 画 `dir ->` 用（分叉 ③ 的"必要配套"：停下沿用朝向就必须看得见）；
    // 用例也用它算"子弹该出现在哪个方向"。
    [[nodiscard]] Facing8 Facing() const noexcept;

    // 敌人 HP 上限。⚠️ **2026-10-10 从 private 挪到 public**（S9 步骤 3）：
    //    子弹的"三发打死一个"必须由**公开常量**算出来（`kEnemyMaxHp / kDefaultBullet.damage`），
    //    否则用例只能写死 3 —— 那就变成"调手感要改测试"（S5_CHECKLIST 10.4 的纪律）。
    static constexpr int kEnemyMaxHp = 3;

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

    // ── 敌人（S8）───────────────────────────────────────────────────────
    // 数量写死 5：PLAN:572 的验收原话是"5 个敌人同时追不出现抖动打转"。
    static constexpr int kEnemyCount    = 5;
    static constexpr int kPatrolPathLen = 2;   // 一条巡逻路线 = 两个端点 → `(idx+1)%2` 就是**往返**
    // ⚠️ `kEnemyMaxHp` 已于 2026-10-10（S9 步骤 3）挪到 **public** —— 用例要用它算"几发打死"。

    // 玩家 HP 上限。定 10 的理由：一个敌人贴脸是 0.8s 打 1 点，5 个同时贴脸 ≈ 4 点/秒
    // → 约 **2.5 秒**掉光。够快到人眼验收时一眼能看见数字在掉，又不至于一进视野就归零。
    // ⚠️ 私有：用例断言的是"HP **下降**"（`PlayerHp()` 是公开接口），不是"HP == 某个数"，
    //    所以没必要把它变成公开常量（避免了"调手感就要改测试"的那个反向陷阱）。
    static constexpr int kPlayerMaxHp = 10;

    // 巡逻路线表：**全敌共用**一张平表，逐敌用 `EnemyData::patrolPath` 选一行。
    // ⚠️ 每行只有 **2** 个点 → `(idx + 1) % 2` 恰好就是**往返**（PLAN:567 要的"预置路径点往返"）。
    //    `ai::EnemyBrain` 只保证"环形前进"（它是无状态的，推不出上次往哪个方向走）——
    //    把"往返"落在**表**上，是这个分工的结果，见 S8_CHECKLIST 落地偏差 1。
    // ⚠️ 位置是**人工核对**过的（S8 不做地图校验，S8_CHECKLIST §6.8）：
    //    · 全落在空格上：柱阵只在 y = 10,20,… **且** x = 10,20,… 的交叉点；地标楼在
    //      x = 30..36 / y = 5..8。这里取的 y = 2 / 4 / 6、x = 44..72 与两者都不相交；
    //    · 离玩家出生点 (2.5, 2.5) 最近也有 42 格 >> 视距(10) → **开局不会被追**；
    //    · 全落在开局视口内（相机原点被夹到 0、视口 80×23）→ 一开局就能看见 5 个 E 在巡逻；
    //    · 全避开 Game suite 那几条用例的行走区（x <= 23、y <= 9）→ 不会把 '@' 盖掉，
    //      既有 4 条用例的期望值不受影响（S8_CHECKLIST 坑 14 那张表逐条核过）。
    static constexpr mini::math::Vec2 kPatrolPaths[kEnemyCount][kPatrolPathLen] = {
        {{44.5f, 2.5f}, {56.5f, 2.5f}},
        {{60.5f, 2.5f}, {72.5f, 2.5f}},
        {{44.5f, 4.5f}, {56.5f, 4.5f}},
        {{60.5f, 4.5f}, {72.5f, 4.5f}},
        {{44.5f, 6.5f}, {56.5f, 6.5f}},
    };

    // 地图布局：**确定性**写死（不随机、不读文件）。
    // 内容 = 地图外沿一圈墙 + **每 10 格一根的规则柱阵** + 一个不对称地标（"一栋楼"）。
    // ⚠️ 规则性是有要求的（2026-09-30 实测被指出"# 摆得没规律"）：柱阵一眼看出是
    //    故意的，且平移一个周期会重合 → 所以还要那个地标才能判断"画面动没动"。
    void BuildDemoMap();

    // 铺敌人（OnStart 里、**玩家之后**调）。
    void SpawnEnemies();

    // 敌人每帧的"决策"：对每个 `EnemyData` 跑一次 `ai::Decide`，**只写速度**。
    // ⚠️ 必须在 `World::Update` **之前**调（写速度必须在积分之前 —— 与玩家同一条帧序硬约束，
    //    见 Game.cpp 的帧序注释）；反过来的现象是"敌人慢一帧"。
    // ⚠️ 读到的双方位置是**本帧积分前**的（= 上一帧末位置）—— 这是 S8 定稿的位置口径。
    void DriveEnemyAi(const mini::core::FrameContext& ctx);

    // ── 子弹（S9）───────────────────────────────────────────────────────
    // 生成一发子弹：位置 = 玩家当前位置，速度 = 朝向 × `kDefaultBullet.speed`，`symbol = '*'`。
    // ⚠️ 子弹就是**一个普通 `Entity`**（连一个额外字段都不需要，见 `S9_CHECKLIST` 分叉 ①）——
    //    所以积分（`World::Update`）、绘制（`DrawEntities`）、删除（`MarkForRemoval`）
    //    **全部是白拿的**，这里只负责"造出来 + 记下 id"。
    // ⚠️ 帧序：**必须**在 `World::Update` **之前**（子弹当帧就要飞出去）。
    // ⚠️ 键（`Space`）的判定在 `OnUpdate` 里（与其他按键一致），所以本函数**不带参数**。
    void FireBullet();

    // 逐发子弹做命中判定 + 标删（帧序第 ⑤d 步，**也在** `World::Update` **之前**）。
    // ⚠️ 它用的是**预测线段** `[pos, pos + velocity × dt]`：判定要读"这一帧子弹要走多远"，
    //    若挪到积分之后就得存"上一帧位置"，而用 `pos_after − v·dt` **反推**在被墙夹住时是错的
    //    （起点会落进墙里 ⇒ 线段穿墙 ⇒ 假命中墙后的敌人）。详见 `S9_CHECKLIST` 坑 2。
    // ⚠️ 遍历的是 `m_bullets`（**不是** `m_entities`）⇒ 遍历中 `Spawn`/`MarkForRemoval`
    //    都不会让我们脚下的容器重新分配（PLAN 点名的 S9 坑 1 在架构上消失）。
    void ResolveBullets(const mini::core::FrameContext& ctx);

    // 这个 id 是不是敌人？只给绘制上色用（玩家黄 / 敌人红 / 其余白）。
    // 线性扫 `m_enemies`：n = 5，每帧每实体扫一次，代价可忽略。
    [[nodiscard]] bool IsEnemyId(mini::world::EntityId id) const noexcept;

    // 渲染。S6 起**有相机**：屏幕格 ↔ 世界格的对应关系是
    //     世界列 = 相机整数原点 + 屏幕列
    //     世界行 = 相机整数原点 + (屏幕行 − kHudRow)
    // 相机的接口全是 const（`OriginCol/OriginRow`）→ 三个绘制函数仍然全是 const，
    // 于是它们用的是 `World` 的 **const 版** Find()/Entities()/Map()
    //（"Find 为什么要有两版"在这里第一次有了真实生产调用者）。
    void DrawMap(mini::render::FrameBuffer& fb) const;
    void DrawEntities(mini::render::FrameBuffer& fb) const;
    void DrawHud(mini::render::FrameBuffer& fb) const;

    // `kAiPanelKey` 打开的**临时**敌人面板（S8）：逐敌一行 `state / distance / tile`。
    // ⚠️ 它是 S11 正式 Debug Overlay 之前的临时出路（`TECH_DEBT` 登记）。
    // ⚠️ **底部对齐**（不是"HUD 的下一行"）：敌人常在屏幕第 3~7 行，画在上面会把它们盖住。
    //    只写左边一条窄带（约 34 列），右边留给地图。理由详见 `Game.cpp` 的 `DrawAiPanel`。
    void DrawAiPanel(mini::render::FrameBuffer& fb) const;

    // 调试快照（键 = `kDumpKey`，见 `Game.cpp` 顶部常量）：把当前帧的参数追加写进 `miniengine_debug.log`。
    // ⚠️ **非 const**：要写文件、还要递增 `m_dumpCount`。它**只读**世界，不改任何状态
    //    （所以放在帧序的哪一步都不影响别的系统 —— 实现里放在相机之后）。
    void DumpSnapshot(const mini::core::FrameContext& ctx);

    mini::world::World    m_world{kWorldW, kWorldH};
    // 视口位置（S6）。每帧在 `World::Update` **之后** Follow 一次（帧序见 Game.cpp）。
    // 值成员：相机是游戏侧状态，引擎不认识它；尺寸不存进相机（见 render/Camera.h 6.3）。
    mini::render::Camera  m_camera;
    mini::world::EntityId m_player{};         // 0 = 还没出生（OnStart 之前）
    float                 m_elapsed = 0.0f;   // Σdt（秒）—— 10.2 的"自证"用它
    int                   m_dumpCount = 0;    // `kDumpKey` 按了几次（日志里的 dump=N；**只增不减**）
    float                 m_moveSpeed = kMoveSpeed;   // 实际用的速度（argv[2] 可覆盖）
    bool                  m_quit = false;     // Q 退出（ShouldQuit 的来源）

    // S8：面板开关（键 = `kAiPanelKey`）。⚠️ 它只是"要不要画"，**不参与任何游戏逻辑** ——
    // 所以它不影响帧序、也不影响用例（用例从不按它）。
    bool m_showAiPanel = false;

    // S8：玩家 HP。⚠️ HP 归 0 **不发生任何事**（不会死、不会重开）——
    // 唯一的后果是敌人把它当成"死了的目标"而脱战（`targetAlive = m_playerHp > 0`）。
    // 真正的死亡/重开是 S10 的 `GameState`。
    int m_playerHp = kPlayerMaxHp;

    // S8：敌人。⚠️ **只增不减**（`Dead` 只把 Entity 的 active 置 false，见 S8_CHECKLIST 6.6），
    // 所以它不会在遍历中被改；真删留到 S9（届时用 std::erase_if，且必须在遍历之外）。
    std::vector<EnemyData> m_enemies;

    // ── S9：子弹 ────────────────────────────────────────────────────────
    // **只存 EntityId**（4 字节/发）—— 子弹的位置/速度/字符全在世界里（那是**唯一**一份真相）。
    // ⚠️ 这样就不存在"两份数据不一致"这个最难查的故障；表本身只是"哪些实体是子弹"的索引。
    // 维护规则**只有两处**（见 `Game.cpp`）：`FireBullet` 里 push_back、⑨ 之后自愈压缩。
    // ⚠️ 它**故意**不是 `std::vector<BulletData>`：子弹除 `Entity` 已有字段外，额外字段是 **0 个**
    //    （分叉 ①）。将来要带属性（穿透/不同伤害）时，把 `EntityId` 换成 `BulletData` 即可。
    // ⚠️ 那些"过期 id"是**自愈**的：⑨ 之后 `erase_if(Find(id) == nullptr)` 一处清掉，
    //    失败表现只是"多发一次 Find"，**不是"指错实体"**（`EntityId` 永不复用）。
    std::vector<mini::world::EntityId> m_bullets;

    // 玩家朝向（S9，分叉 ③）：**移动方向即朝向，停下沿用**。
    // ⚠️ 用枚举而不是 `Vec2`：用例断言 `Facing() == Facing8::East` 不用比浮点，
    //    HUD 也能直接查表画字形（`kFacingGlyphs`）。
    // ⚠️ 初值 = 东：与 `kSpawnX/Y` 无关，只是"第一发子弹往哪打"的确定性默认值。
    Facing8 m_facing = Facing8::East;

    // 击杀数（S9）：**只在敌人的 hp 从 > 0 掉到 <= 0 的那一次** +1（不是"打中一次 +1"）。
    // ⚠️ 它只增不减；HUD 第二行会显示它（S9 步骤 5）。
    int m_kills = 0;
};

} // namespace game
