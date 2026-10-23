#pragma once

#include "game/Bullet.h"      // S9：子弹的规则（朝向 + 命中判定纯函数）
#include "game/DebugOverlay.h"   // S11：`BuildOverlayData` 的返回类型（overlay 的纯数据）
#include "game/Enemy.h"
#include "game/GameState.h"   // S10：游戏状态机（四态 + 策略表，纯函数、零依赖）
#include "mini/core/Application.h"
#include "mini/math/Vec2.h"
#include "mini/render/Camera.h"
#include "mini/render/FrameBuffer.h"
#include "mini/render/Sprite.h"   // S13 第 3 笔：`SpriteBox`（投影结果，`DrawnSprite` 的成员）
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
//   按一次就把当前帧的关键参数**追加**写进 `miniengine_debug.log`
//   （实现见 `Game.cpp` 的 `DumpSnapshot`；**S11 起写入交给 `core::Logger`**）。
//   它记的东西里有一样是专门为那类 bug 准备的：**盒子压到的 4 个格各自是墙还是空**
//   —— 因为"盒占 4 格、屏幕只画 1 格"，挡住你的常常是**看不见**的那一格。
//   ⚠️ **具体是哪个 F 键是运行时的事**：按键写在 `Game.cpp` 顶部的 `kDumpKey` 一处，
//      并在 HUD 提示里显示出来（不用记文档）。历程：F1 撞别的软件 → F3 也不行 → 现在 **F4**。
//   ⚠️ 它做文件 I/O，只在手动按时发生（会污染那一帧的 `work` 值，属可接受）。
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

    // 当前游戏状态（S10）。**真接口**，不是"为测试而加"：`OnRender` 按它决定画什么
    // （`Title` 是纯文字画面 / `Paused` 叠暂停横幅 / `GameOver` 叠结算横幅）。
    // 用例断言 `State() == GameState::Paused` 不用比字符串，也不必去读屏幕。
    // ⚠️ 初值是 `Title` —— **`OnStart()` 之后世界并不会自己动**，得先按一次 `Enter`
    //    （`PLAN` 的"开局提示 → 按 Enter"）。这条改过之后，所有"要推帧看世界动"的用例
    //    都必须先过一遍 `Enter`（见 `tests/test_game.cpp` 的 `StartPlaying`）。
    [[nodiscard]] GameState State() const noexcept;

    // 当前波次（S10 步骤 4）。**从 1 开始**；**0 = 还没开始**（`StartNewRun` 之前）。
    // 同 `State()`：**真接口** —— HUD 真的会显示它（`wave` 那一栏），不是"为测试而加"。
    [[nodiscard]] int Wave() const noexcept;

    // 剩余弹药（S10 步骤 5）。同 `State()`/`Wave()`：**真接口** —— HUD 上真的看得见（`ammo` 那一栏）。
    // ⚠️ 它只在 `Playing` 变化（`FireBullet` 扣、`ResolveBullets` 返还、`BeginWave` 回满）。
    [[nodiscard]] int Ammo() const noexcept;

    // ── S12（第 4 笔）：2D / 3D 视角开关（键 = `kViewKey`，见 `Game.cpp` 顶部）──────────
    // ⚠️ 枚举定义在**类内**（public）⇒ 类型名是 `Game::ViewMode`，用例照这个写。
    // ⚠️ 访问器叫 `CurrentView()` 而**不是** `ViewMode()`：C++ 里**成员函数不能与成员类型同名**
    //    （施工图 §4 写的 `ViewMode()` 是笔误 ⇒ 落地时改名，登记为偏差）。
    enum class ViewMode { Mode2D, Mode3D };

    // **真接口**，不是"为测试而加"：`OnRender` 真的按它分支（这一条就是"2D/3D 二选一"的真相）。
    [[nodiscard]] ViewMode CurrentView() const noexcept;

    // ── S13 第 2 笔：3D 列深度（**只记不读**的观察面）────────────────────────────
    // 3D 一帧里"这一列最近的墙有多远"：`perp`，与 `RayHit::perp` **同口径**
    //（所以第 3 笔能和精灵的 `box.perp` 直接比大小）；这一列没打中墙 = `kRayFar`。
    // ⚠️ **现在没有任何消费者**（第 3 笔的精灵遮挡才读它）⇒ 它存在的唯一理由是：
    //    让"墙 pass 记下来的值 == `CastColumn(...).perp`"**可以被用例钉住**（第 2 笔的守卫）。
    // ⚠️ 越界列返回 `kRayFar`，**绝不返回 0**：`box.perp >= 0` 恒真 ⇒ 精灵会被全判成"墙在前"
    //    ⇒ 3D 里一个精灵都不画（坑 1 的形态①，症状是"3D 里什么都没有"，最难联想到深度）。
    [[nodiscard]] float ColumnDepth(int col) const noexcept;

    // 只读地图（S13 的用例要**自己** `CastColumn` 算期望值，否则只能写死数字）。
    // ⚠️ **只有 const 版**：地图的写入点只有 `BuildDemoMap()`（重开时重铺，`TECH_DEBT` C27）。
    // ⚠️ 别拿它做玩法判断 —— 渲染/碰撞已经有 `World` 这一条路径（`IsSolidAt` 是唯一判据）。
    [[nodiscard]] const mini::world::TileMap& Map() const noexcept;

    // ── S13 第 3 笔：**临时调试接口** —— 把一只敌人放到指定世界坐标 ──────────────
    // 存在理由：3D 遮挡用例需要"可控摆位"（墙后 / 侧向 / 近远对比），而正式玩法里敌人
    //   只从 `kPatrolPaths` 刷出、且会巡逻（6 格/秒）—— 用例根本抓不住"同一个敌人"。
    // ⚠️ **它的 AI 照常工作**：新敌人挂在第 0 条巡逻路线上（`patrolPath = 0`），
    //    所以"摆完推一帧"它就开始走。要**稳定观察**就"摆完当帧渲染"（不开 `OnUpdate`）。
    // ⚠️ **只在帧与帧之间调用**（`OnUpdate` / `OnRender` 之外）：它 `Spawn` 实体，
    //    会让 `Entities()` 的 span 与 `Find()` 的指针失效（同 `FireBullet` 的约束）。
    // ⚠️ 登记在 `TECH_DEBT`（C44）：它是**为用例而开的生产 API**，将来有正式的关卡/刷怪
    //    工具时应当被它取代，而不是长期留着。
    void SpawnEnemyAt(mini::math::Vec2 pos);

    // ── S11：Debug Overlay 的三个观察面（§10 分叉 C 取①、分叉 F 取甲）──────────────
    // ⚠️ 三条都**不是"为测试而加"**：overlay 的表里真的显示它们（`foes` / `shots` 与开关状态）。
    //    这也是 `TECH_DEBT` **C23**（"子弹数没有观察面"）的出路 —— 它的原话就是
    //    "最省的是在面板上给一个公开观察面"。
    // ⚠️ 分叉 C 选①的**另一半价值**：`tests/test_game.cpp` 从此可以问真值，
    //    不必再去"数屏幕上 `E` 的个数"（那条路会被重叠与字形污染咬到，见 C31）。

    // 还活着的敌人数。⚠️ 真删之后（S9 步骤 4）它就是"场上还有几个"的**唯一真值**
    //（`F7` 面板的 `n=` 也是它）。
    [[nodiscard]] int EnemyCount() const noexcept;

    // 存活的子弹数：`m_bullets` 里那些**世界还找得到**的 id。
    // ⚠️ 与 `m_bullets.size()` 的区别：表在 ⑨b 才自愈压缩，所以帧中途可能有已死的 id。
    [[nodiscard]] int BulletCount() const noexcept;

    // overlay 开关（键 = `kOverlayKey`，见 `Game.cpp` 顶部）。
    // **真接口**：用例要断言"默认关"（那是 `TECH_DEBT` C31 的默认安全边界 ——
    // 面板一开，屏幕上就多出一堆字符，既有"数屏幕字符"的判据会被扰动）。
    [[nodiscard]] bool OverlayOn() const noexcept;

    // 敌人 HP 上限。⚠️ **2026-10-10 从 private 挪到 public**（S9 步骤 3）：
    //    子弹的"三发打死一个"必须由**公开常量**算出来（`kEnemyMaxHp / kDefaultBullet.damage`），
    //    否则用例只能写死 3 —— 那就变成"调手感要改测试"（S5_CHECKLIST 10.4 的纪律）。
    static constexpr int kEnemyMaxHp = 3;

    // 玩家 HP 上限（S10 步骤 3 从 private 挪到 **public**）。⚠️ 与 `kEnemyMaxHp` **同一条理由**：
    //    "重开后 HP 回满"（用例 `RestartLeavesNoStaleState`）需要一个**期望值** ⇒ 用例必须拿这个
    //    常量算，不能写死 10；写死就变成"调 HP 就要改测试"（S5_CHECKLIST 10.4 的纪律）。
    //    ✅ 这正是 `S10_CHECKLIST` 前置事实 16 预测的那处改动。
    // ⚠️ 定 10 的理由留在 private 区（见那里的一行指路注释）。
    static constexpr int kPlayerMaxHp = 10;

    // ── 波次四常量（S10 步骤 4；分叉 ③ 取甲、D18 定"全 public"）────────────────
    // 第 n 波刷 `min(kWaveBase + (n-1) * kWaveGrowth, kWaveMax)` 个 ⇒ 3,4,5,6,7,8,8,8,…
    // ⚠️ **四个都公开**（D18）：用例要拿它们算"第 n 波该有几个"、"喘息该等几帧"；
    //    写在 private 就只能写死数字（同 `kEnemyMaxHp` / `kPlayerMaxHp` 的纪律）。
    // ⚠️ 手感关系式（改数值前先代一遍；`S10_CHECKLIST` 分叉 ④ 的意图 A）：
    //    打死一个要 `kEnemyMaxHp / kDefaultBullet.damage` 发、每击杀返还 `kAmmoPerKill`
    //    ⇒ 净耗 2 发/敌人；要求 `(3 − kAmmoPerKill) × kWaveMax < kStartAmmo`，即 2×8 = 16 < 18 ✓。
    //    ⇒ **改 `kWaveMax` 会同时动难度和弹药张力**（分叉 ④ 那句"两个意图不能同时满足"）。
    static constexpr int   kWaveBase   = 3;   // 第 1 波几个
    static constexpr int   kWaveGrowth = 1;   // 每波 +几个
    static constexpr int   kWaveMax    = 8;   // 封顶（**同时**是弹药张力旋钮）
    // 一波清完到下一波的**喘息秒数**。
    // ⚠️ **施工图没定这个数**（它只在 §7.3 的伪代码里用了 `kWaveBreakSeconds`，没给值）
    //    ⇒ 2026-10-12 步骤 4 落地时取 **2.0 秒**：够走到下一波出生点附近看一眼，
    //    又不至于让人以为"游戏卡住了"。**手感旋钮，可调**（见报告的落地偏差）。
    static constexpr float kWaveBreakSeconds = 2.0f;

    static_assert(kWaveBase >= 1, "第 1 波至少要 1 个（否则清空那一帧会立刻又推进）");
    static_assert(kWaveBreakSeconds > 0.0f,
                  "喘息必须为正：`m_waveTimer == 0` 是\"未起倒计时\"的哨兵（坑表 16）");

    // 第 `wave` 波该刷几个（**纯决策**；`BeginWave` 只是接线 —— 又一次"决策与接线分家"）。
    // ⚠️ 定义域 `wave >= 1`：唯一调用方是 `StartNewRun` 的 `BeginWave(1)` 与
    //    `TickWave` 的 `BeginWave(m_wave + 1)`，而 `m_wave` 恒 ≥ 1。
    // ⚠️ 它是 `static constexpr` ⇒ **用例不用造 `Game` 实例就能逐波核对公式**
    //    （"封顶"那条断言因此才可靠 —— 真要跑到第 6 波得杀 33 个敌人，见 `tests` 的注释）。
    [[nodiscard]] static constexpr int WaveEnemyCount(int wave) noexcept
    {
        const int n = kWaveBase + (wave - 1) * kWaveGrowth;
        return (n < kWaveMax) ? n : kWaveMax;
    }

    // ── 弹药三常量（S10 步骤 5；分叉 ④ 定稿：18 / 18 / 1，cap = 初值）────────────
    // ⚠️ **三个都公开**（D18）：用例要拿它们算"打几发该空"、"击杀该返还几发"、"cap 在哪"；
    //    写在 private 就只能写死数字（同 `kEnemyMaxHp` / `kPlayerMaxHp` 的纪律）。
    // ⚠️ **手感关系式**（改数值前先代一遍）：
    //    打死一个敌人要 `kEnemyMaxHp / kDefaultBullet.damage` = 3 发，每击杀返还 `kAmmoPerKill` = 1
    //    ⇒ **净耗 = 3 − 1 = 2 发/敌人**；意图 A（全中够用、失误才紧）要求
    //      `(3 − kAmmoPerKill) × kWaveMax < kStartAmmo`，即 **2 × 8 = 16 < 18 ✓**。
    //    ⚠️ 它与 `kWaveMax` **共用同一个旋钮**：只调 `kWaveMax` 会把意图 A 破坏掉
    //      （分叉 ④ 那句"两个意图不能同时满足"）。
    static constexpr int kStartAmmo   = 18;   // 每波起手弹药（`BeginWave` 里回满到这个值）
    static constexpr int kMaxAmmo     = 18;   // = 起手值 ⇒ **不许囤过初值**（配下面的 static_assert）
    static constexpr int kAmmoPerKill = 1;    // 每击杀返还；N = 1 ⇒ 净耗 2 发/敌人

    static_assert(kMaxAmmo >= kStartAmmo,
                  "上限不能低于起手值：否则 `BeginWave` 的\"回满\"会当场把自己顶到超上限状态");
    static_assert(kAmmoPerKill >= 0, "返还不能为负：那会让\"击杀\"变成惩罚");

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

    // ── 敌人（S8；S10 步骤 4 起"几个"由波次决定）────────────────────────────
    // ⚠️ **S10 步骤 4 删掉了 `kEnemyCount = 5`**：场上敌人个数不再是常量，而是
    //    `WaveEnemyCount(m_wave)`（3,4,5,6,7,8,8,…）。`PLAN:572` 那句"**5 个敌人同时追**
    //    不出现抖动打转"的验收**在第 3 波（5 个）达成**，第 6 波起是 8 个（比原验收更狠）。
    static constexpr int kPatrolPathLen = 2;   // 一条巡逻路线 = 两个端点 → `(idx+1)%2` 就是**往返**
    // ⚠️ `kEnemyMaxHp` 已于 2026-10-10（S9 步骤 3）挪到 **public** —— 用例要用它算"几发打死"。

    // 玩家 HP 上限。定 10 的理由：一个敌人贴脸是 0.8s 打 1 点，5 个同时贴脸 ≈ 4 点/秒
    // → 约 **2.5 秒**掉光。够快到人眼验收时一眼能看见数字在掉，又不至于一进视野就归零。
    // ⚠️ **2026-10-12（S10 步骤 3）挪到 public** —— 用例要拿它算"重开后 HP 回满"的期望值
    //    （同 `kEnemyMaxHp` 的理由）。定义与注释见 public 区。
    // ⚠️ 原注释里那句"用例断言的是 HP 下降、不是 HP == 某个数"**仍然成立**：
    //    既有用例确实只断"下降"；只有**重开**那条用例需要"回满"这个具体值。

    // 巡逻路线表：**全敌共用**一张平表，逐敌用 `EnemyData::patrolPath` 选一行。
    // ⚠️ 每行只有 **2** 个点 → `(idx + 1) % 2` 恰好就是**往返**（PLAN:567 要的"预置路径点往返"）。
    //    `ai::EnemyBrain` 只保证"环形前进"（它是无状态的，推不出上次往哪个方向走）——
    //    把"往返"落在**表**上，是这个分工的结果，见 S8_CHECKLIST 落地偏差 1。
    //
    // ⚠️ **行数 = 8**（S10 步骤 4 从 5 扩到 8，因为封顶值是 `kWaveMax = 8`）：
    //    `BeginWave` 按 `i` 索引它（`i < WaveEnemyCount(n) <= kWaveMax`）⇒ **表长必须 ≥ 封顶值**，
    //    下面那条 `static_assert` 就是这条耦合的守卫（坑表 7）。
    // ⚠️ 位置是**人工核对**过的（S8 不做地图校验，S8_CHECKLIST §6.8）：
    //    · 全落在空格上：柱阵只在 y = 10,20,… **且** x = 10,20,… 的交叉点（两者都满足才是墙）；
    //      地标楼在 x = 30..36 / y = 5..8。这里取的 y = 2/4/6/8、x = 44..72 与两者都不相交
    //      （x = 60 虽然是 10 的倍数，但 y = 2/4/6/8 不是 ⇒ 那里没有柱子）；
    //    · 离玩家出生点 (2.5, 2.5) 最近也有 42 格 >> 视距(10) → **开局不会被追**；
    //    · 全落在开局视口内（相机原点被夹到 0、视口 80×23）⇒ 直接看得见敌人巡逻／刷出；
    //    · 全避开 Game suite 那几条用例的行走区（x <= 23、y <= 9）→ 不会把 '@' 盖掉
    //      （S8_CHECKLIST 坑 14 那张表逐条核过；S10 步骤 4 新增的 3 行也按同一张表核过）。
    // ⚠️ 行数写成**字面量 8**、而**不是** `= kWaveMax`：后者会让下面那条 `static_assert` 变成
    //    **恒真断言**（`TECH_DEBT` C18 警告的正是这一类 —— "恒真断言比没有更坏"，因为它
    //    看着像守卫、实际永远不响）。写成字面量之后，"有人把 `kWaveMax` 调到 9 却忘了扩表"
    //    会在**编译期**被抓住。
    static constexpr int kPatrolPathRows = 8;
    static constexpr mini::math::Vec2 kPatrolPaths[kPatrolPathRows][kPatrolPathLen] = {
        {{44.5f, 2.5f}, {56.5f, 2.5f}},
        {{60.5f, 2.5f}, {72.5f, 2.5f}},
        {{44.5f, 4.5f}, {56.5f, 4.5f}},
        {{60.5f, 4.5f}, {72.5f, 4.5f}},
        {{44.5f, 6.5f}, {56.5f, 6.5f}},
        // ↓↓↓ 2026-10-12（S10 步骤 4）新增的 3 行：沿用同一条规律（y 取 6.5/8.5 这两条无柱的行）
        {{60.5f, 6.5f}, {72.5f, 6.5f}},
        {{44.5f, 8.5f}, {56.5f, 8.5f}},
        {{60.5f, 8.5f}, {72.5f, 8.5f}},
    };
    static_assert(kWaveMax <= kPatrolPathRows,
                  "封顶值超过了巡逻表长 ⇒ BeginWave 会越界读表（坑表 7）");

    // ── S10 步骤 3：**一局的唯一入口** ──────────────────────────────────────
    // 把"这一局的初始状态"全部铺出来：清空实体 → 重置标量 → 建地图 → Spawn 玩家 → 铺敌人。
    // 顺序即依赖（7 步清单见 `Game.cpp` 的定义处与 `S10_CHECKLIST` §4）。**幂等**：连调两次结果一样。
    //
    // ⚠️ 调用时机**只有帧首**（`OnUpdate` 的 ①b —— 此时没有任何别的遍历）。
    //    它会清空实体表并重新 `Spawn` ⇒ `Entities()` 的 span 与 `Find()` 的指针都会失效
    //    ⇒ ⛔ **不许**在 `DrawEntities` / `DriveEnemyAi` / `ResolveBullets` 里或它们之间调。
    //
    // ⚠️ 它用的是**既有公开接口**（遍历 `Entities()` + `MarkForRemoval` → **一次** `FlushRemovals`），
    //    **不重建 `m_world`** ⇒ `m_nextId` 保持不变、**id 跨局不复用**（`S10_CHECKLIST` 分叉 ② 取丙）。
    //    ⛔ **别改成 `m_world = World{w, h}`**：那会把 `m_nextId` 重置回 1 ⇒ 上一局的 id
    //    在新实体上重现 ⇒ 残留的 id 会**静默命中别人**（坑表 1，`S10_CHECKLIST` 分叉 ② 那张对照表）。
    //
    // ⚠️ 它**不清** `m_state` / `m_showAiPanel` / `m_dumpCount` / `m_quit`：
    //    那四样**不属于"一局"**，它们的作用域是"这次进程"（坑表 9 写了为什么）。
    void StartNewRun();

    // ── S10 步骤 4：波次 ───────────────────────────────────────────────────
    // 刷第 `wave` 波（**从 1 开始**）：个数 = `WaveEnemyCount(wave)`，出生点取 `kPatrolPaths[i][0]`。
    // ⚠️ 两个调用点**只有**这两个：`StartNewRun` 的 `BeginWave(1)`，
    //    和 `TickWave` 的 `BeginWave(m_wave + 1)`。⛔ 别在别处调 ——
    //    它是"`m_wave` 与场上敌人数一致"这条不变量的唯一维护者。
    // ⚠️ 内含"分叉 ④ 的**每波回满**"（步骤 5 落地：`m_ammo = kStartAmmo`）。
    void BeginWave(int wave);

    // 波次推进（帧序 **⑥ 之后、⑦ 之前**，只在 `advanceWave` 时调）：
    // 本波全灭 → 起喘息倒计时 → 到点 `BeginWave(m_wave + 1)`。
    // ⛔ 判据**不能**只有 `m_enemies.empty()` —— 它在"还没刷第一波"时也为真（坑表 4，§7.3）。
    void TickWave(float dt);

    // 地图布局：**确定性**写死（不随机、不读文件）。
    // 内容 = 地图外沿一圈墙 + **每 10 格一根的规则柱阵** + 一个不对称地标（"一栋楼"）。
    // ⚠️ 规则性是有要求的（2026-09-30 实测被指出"# 摆得没规律"）：柱阵一眼看出是
    //    故意的，且平移一个周期会重合 → 所以还要那个地标才能判断"画面动没动"。
    // ⚠️ S10 起它由 `StartNewRun()` 调（**每次重开都重铺一遍**）：
    //    它是确定性的，重铺的结果与上一次逐格相同；这样做把"地图在整局之间不变"这条**假设**
    //    直接从依赖里删掉了（分叉 ② 条件 2 的触发条件也就此关闭）。
    void BuildDemoMap();

    // 铺敌人：在 `kPatrolPaths[i][0]` 处 Spawn 第 i 个（`i = 0 .. count-1`）+ 一条 `EnemyData`。
    // ⚠️ 由 `BeginWave` 调（"几个"是**决策**，铺是**接线** —— 所以拆成两个函数）。
    // ⚠️ `count` 必须 ≤ `kPatrolPathRows`（调用方 `BeginWave` 由 `WaveEnemyCount` 保证 ≤ `kWaveMax`）。
    void SpawnWaveEnemies(int count);

    // 铺**一只**敌人：`Spawn` 实体 + 一条 `EnemyData`（HP / 计时器 / 巡逻路线的**唯一**初始化清单）。
    // ⚠️ `SpawnWaveEnemies` 与 `SpawnEnemyAt`（调试接口）**共用它** ⇒ "一只敌人由哪些字段组成"
    //    只有一处真相；两处各写一份迟早漏字段（S8 的"两套真相"教训）。
    mini::world::EntityId SpawnEnemyEntity(mini::math::Vec2 pos, int patrolPath);

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
    // ⚠️ `kHudRow`（`Game.cpp` 顶部的**文件内**常量）= 游戏 HUD 占的行数，**同时**是世界视口的
    //    第一个屏幕行。S9 步骤 5 起 = **2**（两行 HUD）⇒ 视口高 = 画布高 − 2 = 23。
    //    它是文件内的：别处（包括测试）要用只能"照抄"，`Game` 刻意不为它开公开接口。
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

    // ── S11：正式 Debug Overlay（键 = `kOverlayKey`）──────────────────────────
    // 把"引擎帧数据 + Profiler 表 + 游戏计数"装进一个纯数据结构，`OnRender` 里现装现画。
    // ⚠️ `rows` 由调用方持有（`OnRender` 里的**栈数组**）⇒ 本函数不拥有它，只在一次绘制内有效。
    // ⚠️ 它是 `const`：只读状态、不改进度（overlay 纯属"看"，不参与任何游戏逻辑）。
    [[nodiscard]] OverlayData BuildOverlayData(const mini::core::Profiler::Row* rows,
                                               int                                 rowCount) const noexcept;

    // ── S10 步骤 2：三个「状态专属画面」──────────────────────────────────
    // 分工：`OnRender` 按 `m_state` 决定**画什么**，这三个函数只管**自己那几行字**。
    // ⚠️ 全 ASCII + `fb.Draw`（逐格越界忽略）—— 代码页 936 下非 ASCII 会乱码（S8 的坑表）。
    // ⚠️ `DrawTitle` 是唯一"不画世界"的那个：`Title` 下连地图都不画（纯文字画面）。
    void DrawTitle(mini::render::FrameBuffer& fb) const;        // 开局提示（按 Enter 开始）
    void DrawGameOver(mini::render::FrameBuffer& fb) const;     // 结算（叠在世界之上）
    void DrawPausedBanner(mini::render::FrameBuffer& fb) const;  // 暂停横幅（叠在世界之上）

    // ── S12（第 4 笔）：3D 视角的两块 ────────────────────────────────────────
    // 相机方向 = **唯一**把"朝向"翻译成"单位向量"的地方（分叉 C 的接缝）：
    //   现在 = `game::FacingToVec2(m_facing)`；将来做**连续转身**只改这一个函数体。
    // ⚠️ 私有：它没有对外观察面（HUD 不显示向量；用例要断的是 `CurrentView()` / 屏幕）。
    [[nodiscard]] mini::math::Vec2 CameraDir() const noexcept;

    // 3D 一帧：**只画墙**（地板/天花板是第 5 笔）+ 每列一条射线（S13 第 3 笔起再加精灵）。
    // ⚠️ 它**不认识** HUD：视口从 `kHudRow` 起算（HUD 之后画、盖在上面），
    //    地平线在 `kHudRow + viewH/2` —— 与 2D 的"世界从 `kHudRow` 起"是同一条约定。
    // ⚠️ **S13 第 2 笔起去掉 `const`**（分叉 B 甲）：它要写 `m_colDepth`。
    //    代价必须认：`mutable` 是"在 const 里偷改状态"，比去 const 更难查（施工图 §2 分叉 B）。
    void DrawRaycastWorld(mini::render::FrameBuffer& fb);

    // ── S13 第 2 笔：按列的墙深度（"这一列最近的墙有多远"，单位 = 格）───────
    // 每帧由 `DrawRaycastWorld` 的墙 pass **顺手写下**（`RayHit::perp`；没命中 = `kRayFar`）。
    // ⚠️ **本笔"只记不读"**（0 个消费者）：第 3 笔的精灵遮挡才拿它做比较。
    //    ⇒ 唯一的价值是"让第 3 笔的遮挡有个已被验证过的地基"，而不是本笔的任何可见变化。
    // ⚠️ **复用**（分叉 B 甲）：宽度变了才 `resize`，绝不每帧分配（坑 7：帧时间抖动）。
    //    初值/未渲染时是**空表** ⇒ 存取器按"越界 = `kRayFar`"处理（空表 = 全世界都能画）。
    std::vector<float> m_colDepth;

    // ── S13 第 3 笔：精灵（敌人 / 子弹）──────────────────────────────────────
    // 两段式（分叉 E-2）：**先收集**（只读世界）→ **再投影**（唯一的深度真相）→ 排序（远→近）。
    // ⚠️ 为什么不在收集时就算 `perp`：那会出现**两处深度公式**（收集处 vs 投影处），
    //    迟早各说各话 —— `dist` vs `perp` 的教训（坑 2）。
    struct SpriteItem {   // "要画什么"（还没投影）
        mini::math::Vec2      pos{};
        float                 z         = 0.0f;   // ★ **脚底**高度（第 4 笔起子弹用它）
        float                 cellsTall = 1.0f;
        float                 cellsWide = 1.0f;
        char32_t              glyph     = U'?';
        mini::render::Color   color     = mini::render::Color::Default;
        mini::world::EntityId id{};               // ★ 排序的**次级键**（同深度画序确定）
    };

    struct DrawnSprite {  // "画到哪"（投影结果 + 排序键）
        mini::render::SpriteBox box{};
        char32_t                glyph = U'?';
        mini::render::Color     color = mini::render::Color::Default;
        mini::world::EntityId   id{};             // ★ 次级键：`box.perp` 相同时按 id 升序
    };

    // 收集：**身份判定照抄 `DrawEntities`**（跳 `!active`、跳玩家、`IsEnemyId` 分色），
    // 字形直接取 `entity.symbol`（分叉 F 甲：不为 3D 另建一张 kind→字形表）。
    // ⚠️ `const`：只读世界、只写调用方给的 out。
    void CollectSprites(std::vector<SpriteItem>& out) const;

    // 画：收集 → 逐项投影 → 按 `box.perp` 降序（远→近）→ 逐列与 `m_colDepth` 比较后写格。
    // ⚠️ **非 const**：它复用成员缓冲（`m_spriteItems` / `m_spriteBuf`），且要读 `m_colDepth`。
    // ⚠️ 裁剪**不在这里**：`ProjectSprite` 已经保证输出在视口内（分叉 R4 甲）。
    void DrawSprites(mini::render::FrameBuffer& fb);

    // 两个**复用**缓冲（坑 7：绝不在每帧里新建 vector）
    std::vector<SpriteItem>  m_spriteItems;
    std::vector<DrawnSprite> m_spriteBuf;

    // 调试快照（键 = `kDumpKey`，见 `Game.cpp` 顶部常量）：把当前帧的参数交给 `core::Logger`
    // （追加写 `miniengine_debug.log`；`TECH_DEBT` **C14 于 S11 到期** —— 以前它自己开 `ofstream`）。
    // ⚠️ **非 const**：要写日志、还要递增 `m_dumpCount`（它同时是"本进程第一次 dump"的判据）。
    // ⚠️ 它**只读**世界，不改任何状态（所以放在帧序的哪一步都不影响别的系统 —— 实现里放在相机之后）。
    // ⚠️ **未装 Logger 时静默丢弃**（单测里就是这样）⇒ 调它总是安全的。
    void DumpSnapshot(const mini::core::FrameContext& ctx);

    // ── S10 步骤 4：波次（"一局"的数值状态之一 ⇒ `StartNewRun` 会把它归位）────────
    // `Wave()` 从 1 开始；**0 = 还没开始**。每波个数 = `WaveEnemyCount(m_wave)`。
    int m_wave = 0;

    // 本波清完之后的**喘息倒计时**（秒）。⚠️ **哨兵：`0` = 没在倒计时**（坑表 16）：
    //   "已清完但还没起倒计时" 与 "倒计时进行中" 用同一个字段区分 ⇒ 0 必须不可能是"已到点"。
    //   配 `static_assert(kWaveBreakSeconds > 0)`（在 public 区）。
    // ⚠️ 它在 `BeginWave` 里被清 0；`TickWave` 是**唯一**递减它的地方。
    float m_waveTimer = 0.0f;

    // ── S10 步骤 5：弹药（"一局"的数值状态之一）─────────────────────────────────
    // ⚠️ **初值 = `kStartAmmo`**（而不是 0）：`Title` 下它是"下一局会拿到的量"，
    //    而 0 会是**谎话**（还没开始玩，不可能"打光了"）。反正 `BeginWave(1)` 会再写一次。
    // ⚠️ **写入点只有三个**（都在 `Game.cpp`）：`FireBullet` 扣 `−1` / `ResolveBullets` 击杀返还
    //    `min(+kAmmoPerKill, kMaxAmmo)` / `BeginWave` 回满。**别在别处改它** ——
    //    否则"打几发该空"这条算式（用例 12/13/15 全部依赖它）就不成立了。
    int m_ammo = kStartAmmo;

    // ── S10：状态机（**最重要的一件事**，所以放在成员区最前面）────────────
    // 规则在 `game/GameState.h`（纯函数 + 策略表），这里只存**当前在哪一态**。
    //
    // ⚠️ **初值 `Title`** —— `PLAN` 的可运行效果原话是"**开局提示** → 按 Enter → 刷怪 …"。
    //    ⇒ `OnStart()` 之后**世界不会自己动**：`Title` 的 `StatePolicy` 五项全 false
    //      （`simulate` = 不积分 / `readMoveKeys` = 不读 WASD / `advanceClock` = 不加 Σdt …）。
    //    ⚠️ 这一条改动的涟漪很大：**所有"要推帧看世界动"的用例都得先按一次 Enter**
    //       （见 `tests/test_game.cpp` 的 `StartPlaying` 与 `S10_CHECKLIST` 落地偏差 3）。
    //
    // ⚠️ **转移只有一处**（`OnUpdate` 顶部的 ⓪① 经 `NextState`）——
    //    别在伤害代码里直接写 `m_state = ...`：那会造出"第二个真相"，
    //    而"暂停时世界真的停了"这条性质的守卫正是"转移与策略都只有一个入口"。
    GameState m_state = GameState::Title;

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

    // ── S11：overlay 开关 + 引擎帧数据快照（分叉 F 甲）───────────────────────────
    // overlay 开关（键 = `kOverlayKey`，见 `Game.cpp` 顶部）。**默认关**：
    //   · 它是**开发者工具**，不是玩法键（所以不进 HUD 提示行，见 §10-A 的附带决定）；
    //   · 一开就往屏幕上多写一堆字符 ⇒ 既有"数屏幕字符"的判据会被扰动
    //     （`TECH_DEBT` C31）。**默认关**就是那条纪律的安全边界。
    // ⚠️ 与 `m_showAiPanel` 同作用域：**进程级**，不随 `StartNewRun()` 归零（它不参与游戏逻辑）。
    bool m_showOverlay = false;

    // ── S12（第 4 笔）：当前视角（键 = `kViewKey`，见 `Game.cpp` 顶部）─────────────────
    // ⚠️ **默认 `Mode2D`**：既有 14 条"读屏幕"用例（`ScreenText` / `FindChar` / `CountChar`）
    //    全建立在 2D 屏上 ⇒ 默认 3D 会让它们**全部**失效。这条默认值不是随口定的
    //    （`S12_CHECKLIST` 坑 9 的第 ① 条对策）。
    // ⚠️ 与 `m_showOverlay` 同作用域：**进程级**，不随 `StartNewRun()` 归零
    //    （它是"怎么看"，不是"一局的数值状态"）。
    ViewMode m_viewMode = ViewMode::Mode2D;

    // ── S11：几何层开关（键 = `kGeometryKey`）＝ `TECH_DEBT` **C13** 的到期处置 ────────────
    // ⚠️ 它**不是** `PLAN:549` 字面上的"碰撞盒"：S7 判过"C13 不做"，理由是"1 格 1 字符下
    //    画出格子边界 ≈ 没画"。5c 走的是**另一条通道 —— 底色**：
    //      · 实心格（墙 / 越界）→ 暗灰底（`DrawMap`）
    //      · 玩家占格 → 蓝底、敌人占格 → 红底（`DrawEntities`）
    //    ⇒ **字形一个字节都不动**（所以"数屏幕上 `'E'`"那些判据不受影响），
    //      而"实体占的是哪一格"（点模型正好一格）第一次**看得见**了。
    // ⚠️ **默认关**：同 `m_showOverlay` —— 一开就多一层视觉信息，会盖掉"原始画面"。
    //    它是**开发者工具**，不进 HUD 提示行（§10-A 的附带决定）。
    // ⚠️ 与 `m_showOverlay` / `m_showAiPanel` 同作用域：**进程级**，不随 `StartNewRun()` 归零。
    // ⚠️ **没有 `GeometryOn()` 这种公开访问器**（与 `OverlayOn()` 不同）：本层的效果
    //    **完全在屏幕上**（底色就是 `Cell` 的一部分）⇒ 用例直接断 `fb.At(x,y).bg`，
    //    不必给 `Game` 开一个只为测试存在的开关读数（"0 调用者不建"的另一面）。
    bool m_showGeometry = false;

    // 引擎帧数据的**只读标量快照**（分叉 F 甲：`IGame::OnRender` 拿不到 `FrameContext`）。
    //   · 每帧在 `OnUpdate` 开头拷一次 ⇒ overlay 的**引擎侧**数字慢一帧
    //     （⚠️ 顶栏的 `Rows` 也是这样，`S11_CHECKLIST` §16.3 ② 有实测）；
    //   · **游戏侧**计数（`foes`/`shots`/`hp`…）在 `OnRender` 里当帧现读 ⇒ 两类新鲜度不同，
    //     这是有意的：引擎那 4 项只有 `OnUpdate` 看得到，而游戏计数自己随时都有。
    //   ⛔ **不要**往这个结构里塞 `const Input&` —— 那正是 S5 / `TECH_DEBT` C4 关掉的东西
    //     （"`OnRender` 完全不碰输入"这条契约）。
    struct FrameSnapshot {
        float fps          = 0.0f;
        float frameMs      = 0.0f;
        float maxFrameMs   = 0.0f;
        float workMs       = 0.0f;
        int   screenWidth  = 0;
        int   screenHeight = 0;
    };
    FrameSnapshot m_lastFrame{};

    // S8：玩家 HP。
    // ⚠️ **S10 步骤 2 起它有两个后果**（原来是"什么都不发生"）：
    //   ① 敌人把它当成"死了的目标"而脱战（`targetAlive = m_playerHp > 0`，S8 的既有逻辑）；
    //   ② **下一帧的 ⓪ 判出 `PlayerDied` ⇒ 状态转 `GameOver` ⇒ 世界冻结**（S10 新加）。
    //      ⚠️ 刻意**不在扣血那一行**直接改 `m_state`：转移只有 ⓪① 一个入口（见 `m_state` 注释）。
    //      ⚠️ 代价（S10_CHECKLIST §7.1 理由 1）：死亡响应**最早也要等下一帧** ——
    //         换来的是"死那一帧起就不再响应移动"（`PLAN` 的验收原话），因为 ⓪ 在读方向键之前。
    // ⚠️ **重开**（`R`）从 S10 **步骤 3** 起**真的会从 0 恢复**：`StartNewRun()` 把它重设成 `kPlayerMaxHp`
    //    （那是"一局"的数值状态之一 —— 与 `m_kills`/`m_elapsed`/`m_facing` 一起归位）。
    int m_playerHp = kPlayerMaxHp;

    // S8：敌人。⚠️ **S9 步骤 4 起会真删了**（原先写"只增不减"）：
    // `ResolveBullets`（⑤d）在 hp 跨过 0 时把 id 记进名单，**循环之外**做
    // `m_world.MarkForRemoval(id)` + `std::erase_if(m_enemies, 按 id 匹配)` 两件事
    // （只删一边都是 bug：悬空 id / 没人驱动的活靶子 —— 见 `game/Enemy.h` 文件头）。
    // ⚠️ 因此 **遍历 `m_enemies` 时不许在循环内 erase**（`erase_if` 是 swap-erase，
    //    会让同帧"当前下标"静默指错别人）；`DriveEnemyAi` 的遍历只读 + 写速度，安全。
    // ⚠️ 真删之后 `F7` 面板的 `n=` 才是"还活着几个"；S8 那套"留着看 `state=Dead`"的
    //    取证手段**没有了**（面板的行数本身就是结果）。
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
