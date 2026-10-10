#include "game/Game.h"

#include "game/Bullet.h"           // S9：朝向 ↔ 向量、线段命中判定（纯函数）
#include "mini/ai/EnemyBrain.h"   // S8：敌人每帧的"决策"（纯函数；只输出速度）
#include "mini/math/Vec2.h"
#include "mini/platform/Input.h"
#include "mini/render/Color.h"
#include "mini/world/Collision.h"   // 调试快照用：IsSolidAt / ToIndex / 点模型的判据
#include "mini/world/TileMap.h"

#include <algorithm>    // std::max（S8：HP 夹到 0）；std::erase_if（S9：子弹表自愈压缩）
#include <array>        // S9：定长暂存（kEnemyCount 是编译期常量 ⇒ 判定时不分配）
#include <cstddef>      // std::size_t（S8：reserve 与巡逻表下标）
#include <cstdio>
#include <filesystem>   // 调试快照：判断日志文件是不是"新建的"（见 DumpSnapshot）
#include <fstream>      // 调试快照（追加写日志）
#include <span>         // S8：把巡逻路点交给 Decide；S9：把敌人位置交给 FirstEnemyOnSegment

// ============================================================
// Game 实现（S5）—— 引擎与游戏分家之后的**唯一**游戏侧文件
//
// 这里做六件事，别的一件都不做：
//   1. 开局：铺地图、Spawn 玩家、铺敌人（BuildDemoMap / SpawnEnemies / OnStart）
//   2. 每帧：输入 → 速度 → 交给 World 积分（OnUpdate，帧序是硬约束，见下）
//   3. 每帧：世界 → 屏幕（OnRender，S6 起走相机）
//   4. 每帧：相机跟随（OnUpdate 末尾，**必须在 Update 之后**）—— S6 加的
//   5. 每帧：敌人决策（DriveEnemyAi，**必须在 Update 之前**）—— S8 加的
//   6. 每帧：生成子弹 + 命中判定（FireBullet / ResolveBullets，**都在 Update 之前**）—— S9 加的
//
// ⚠️ 帧序（S5_CHECKLIST 7.1 + S6 7.1 + S8 §6.3 + S9 §7.1，顺序不能换）：
//     读输入 → 归一化（顺手更新朝向）→ **写玩家速度** → 驱动 AI（**写敌人速度**）
//     → **生成子弹** → **命中判定/标删** → Update(dt) → **相机 Follow** → FlushRemovals
//   写速度必须在 Update 之前：反过来的现象是"按一下动一下、但慢一帧"，
//   看着像手感问题，其实是顺序问题。
//   驱动 AI 同理：敌人和玩家**在同一个循环里**被积分，所以两边的速度都得先写好。
//   ⚠️ 代价（S8 的位置口径）：AI 这一帧读到的是**上一帧末**的双方位置 ——
//     它追的是 1/60 秒前的你（≈0.33 格，肉眼看不见），换来的是"帧序不变 + 可离线复现"。
//   ⚠️ 子弹的**生成**与**判定**为什么也都在 Update 之前（S9 定）：判定用的是
//     **预测线段** `[pos, pos + velocity × dt]`；若挪到 Update 之后就得知道"上一帧位置"，
//     而用 `pos_after − v·dt` 反推在被墙夹住时是错的（起点落进墙里 ⇒ 假命中墙后的敌人）。
//   相机必须在 Update 之后：用上一帧的玩家位置会让 @ 与地图差一帧相位
//   （现象是"@ 偶尔不在正中，偶尔闪一格"）—— S6 坑 3。
//
// ⚠️ 坐标：`dt` 用 `ctx.deltaSeconds`（S3 已 clamp 过，**这里不夹**）；
//   世界→屏幕的取整**一律 floor**（7.2），而且 S6 起**只取整一次**：
//   相机整数原点 = floor(相机 pos)，之后全是整数加减：
//     世界列 = 相机整数原点 + 屏幕列
//     世界行 = 相机整数原点 + (屏幕行 − kHudRow)
//   `TileMap::ToIndex` 与相机内部用的 `FloorToInt` 是同一个函数 —— 不许再写第二条取整路径。
// ============================================================

namespace game {

namespace {

// 游戏 HUD 占第 1 行（第 0 行是引擎顶栏）。
//
// ⚠️ 代价说清（S6 起措辞改准）：HUD 压在**视口第 0 行**上 —— 也就是相机原点那一行
//    （`OriginRow()` 对应的世界行）。地图照画、HUD 后画会盖掉它，所以**垂直方向真正
//    可见的世界只有 22 行**（可用视口 23 行减去这一行）。
//    ⚠️ 因此"可用视口高 = 画布高 − kHudRow"这条不能写错：写成整屏高会让底部多露一行，
//       并让 HUD 压错行（S6 坑 8）。
constexpr int kHudRow = 1;

// 调试快照的**按键**与**落盘文件名**。
//
// ⚠️ 按键字母在代码里**只出现在两处**：这个常量 + `DrawHud` 里那句提示字面量。
//    再换键 = 只改那两处。（其余注释一律写 `kDumpKey`、不抄字母 —— 免得下次又漏改一处。）
// ⚠️ 为什么是 F4：2026-10-08 本人**连报两次冲突** —— 先是 F1 被别的软件占，换成 F3 之后
//    F3 也不行 → 落到 F4。`KeyCode.h` 目前只映射了 `F1..F4`；想用 F5+ 得先去那儿加映射。
//    `F2` 留着给 S11 的 Debug Overlay（`PLAN:549` 的"F2 显示碰撞盒"），所以不占它。
constexpr mini::platform::Key kDumpKey = mini::platform::Key::F4;
const char* kDumpFileName = "miniengine_debug.log";   // *.log 已被 .gitignore 忽略

// ── `kAiPanelKey`：S8 的**临时**面板键（`TECH_DEBT` C15 记着"什么时候删"）────────
//   kAiPanelKey  开/关敌人面板   → S11 的正式 Debug Overlay 落地时删
//   ⚠️ `kKillKey`（S8 的临时杀敌键）已于 **2026-10-10（S9 步骤 3）删除** —— `TECH_DEBT` C15 到期：
//      S9 接上了**真入口**（子弹），不需要那个替代品；`Key::K` 也从 `KeyCode.h` 删掉了。
//
// ⚠️ **为什么面板键是 `F7` 而不是 `F3`**（`PLAN:572` 写的是 F3，`S8_CHECKLIST` §6.7 也按 F3 写的）：
//    2026-10-09 实测 **F3 在本机被别的软件占用**，按下去我们的程序**收不到**。
//    这不是新问题 —— `kDumpKey` 那段注释记着 2026-10-08 的一模一样的经历
//    （"F1 撞别的软件 → 换 F3 → **F3 也不行** → 落到 F4"）。这次轮到了面板键。
//    ⚠️ 换键的代价被压到**只改这一行**：其余代码只认 `kAiPanelKey`，不认具体是哪个键。
//    再撞下一个候选：`Tab`（本项目目前没人用；已在 `KeyCode.h` 里映射）。
constexpr mini::platform::Key kAiPanelKey = mini::platform::Key::F7;

// ── `kFireKey`：S9 的开火键 ────────────────────────────────────────────────
// ⚠️ 键字母在代码里**只有两处**（同 `kDumpKey` 那条纪律）：这个常量 + `DrawHud` 提示行里的 `SPC`。
// ⚠️ **边沿触发**（`Get(...) == KeyState::Pressed`，不是 `IsDown`）—— 按一下一发。
//    用 `IsDown` 会变成"按住连射"：用例 `Game.HoldingSpaceFiresOnlyOnce` 钉这一条，
//    反向验证 R4 就是"把它改成 `IsDown`"。连发是 S10 的手感活（那时有 `Ammo` 兜着）。
// ⚠️ `Space` **早就映射好了**（S2 起就在 `KeyCode.h` 里）⇒ S9 对 `engine/` 的改动
//    只有"删 `Key::K`"这一处。
constexpr mini::platform::Key kFireKey = mini::platform::Key::Space;

// 子弹画什么字符。⚠️ 它就是普通实体的 `symbol`（分叉 ①：子弹没有额外字段）——
// 绘制走 `DrawEntities` 的既有规则（玩家黄 / 敌人红 / **其余白**）⇒ 子弹自动是白的，绘制零改动。
// ⚠️ 不另立 `kBulletSymbol` 常量：`'*'` 在本文件只有这一处落点；用例里数 `'*'` 是"读屏幕"，
//    与 `'@'`/`'E'` 一样直接写字面量 —— 保持同一风格。
constexpr char kBulletGlyph = '*';

// 面板的行数 = `kAiPanelHeaderLines` 行表头 + 每敌一行；它是**底部对齐**的（见 DrawAiPanel）。
// ⚠️ **不画在"HUD 的下一行"**，这是实现时才看出来的：
//    敌人待在 `y = 2..6`，而玩家靠近地图上边缘时相机会把原点夹到 0 ——
//    于是**敌人正好落在屏幕第 3~7 行**。面板若从第 2 行往下画，**一按面板键就把敌人全盖住**，
//    "按 K 看它消失"那个验收根本做不了。
//    底部对齐两头都不挡：玩家在画面里的行号永远 <= 13（相机居中/夹取的结果），敌人又在第 3~7 行。
//    这条已登记为落地偏差 5（施工图 §6.3 只写了"HUD 行的下一行"）。
constexpr int kAiPanelHeaderLines = 1;

// 状态名（面板用）。
// ⚠️ `switch` **不写 default**：以后加第六态，编译器会在这里提醒"你没处理新状态"
//    —— 这正是 `PLAN:568` 说的"`switch` 的穷尽性"。
//    末尾那句 return 是给"枚举里出现了表外值"兜底的（也让 /W4 闭嘴），正常走不到。
const char* StateName(mini::ai::EnemyState state) noexcept
{
    switch (state) {
    case mini::ai::EnemyState::Idle:   return "Idle";
    case mini::ai::EnemyState::Patrol: return "Patrol";
    case mini::ai::EnemyState::Chase:  return "Chase";
    case mini::ai::EnemyState::Attack: return "Attack";
    case mini::ai::EnemyState::Dead:   return "Dead";
    }
    return "?";
}

// 调试快照用：一格是空还是实心？
// ⚠️ 它**只负责打印**，不承担任何判定职责 —— 判碰撞一律走 `Collision.h`（同一个事实只有一个来源）。
//    标记用 `.`/`#`（不用 W/E 之类字母：W 既可能被读成"墙"也可能被读成"西边"）。
//    越界经 `IsSolidAt` 也返回 true → 打成 '#'（世界外就是墙，这正是要看的事实之一）。
char CellMark(const mini::world::TileMap& map, int tx, int ty)
{
    return mini::world::IsSolidAt(map, tx, ty) ? '#' : '.';
}

} // namespace

// ------------------------------------------------------------
// 构造 / 开局
// ------------------------------------------------------------

// 速度可覆盖（`MiniEngine.exe 60 30`）。**非正数 / NaN 一律回落默认值**：
// 一个"速度为 0 的玩家"看起来和"按键坏了"一模一样，宁可退回默认也别让它发生。
Game::Game(float moveSpeed) noexcept
    : m_moveSpeed(moveSpeed > 0.0f ? moveSpeed : kMoveSpeed)
{
}

// 确定性布局：地图外沿一圈墙 + 规则柱阵 + 一个不对称地标。
// 不随机、不读文件（PLAN：本阶段不做关卡格式）
void Game::BuildDemoMap()
{
    using mini::world::Tile;

    mini::world::TileMap& map = m_world.Map();
    const int w = map.Width();
    const int h = map.Height();

    // 1) 地图外沿一圈墙。
    //    ⚠️ 开局视野只有左上角 80×24，所以**开局**这一圈里只有左边那条看得见：
    //    上边会被游戏 HUD 占掉（HUD 压在视口第 0 行），右边/下边**S6 起**走到地图边缘就能看到
    //    —— S6 的人眼验收里"走到右下角不出现错位的墙"看的就是它们。
    //    （其实不写这一圈也没关系：`TileMap::At` 越界本来就返回 Wall —— S5 决策 5。
    //      写出来只是让"地图里有什么"在代码里看得见。）
    for (int x = 0; x < w; ++x) {
        map.Set(x, 0, Tile::Wall);
        map.Set(x, h - 1, Tile::Wall);
    }
    for (int y = 0; y < h; ++y) {
        map.Set(0, y, Tile::Wall);
        map.Set(w - 1, y, Tile::Wall);
    }

    // 2) **规则柱阵**：每 kPillarStep 格一根柱子，x/y 都落在倍数上。
    //    ⚠️ 2026-09-30 重写过这一节：原来放的是"三根随手挑的柱子"（x=10 竖条、
    //    y=8 横条、x=35 竖条），在 80×24 的视野里看着就是"东一块西一块、没规律"
    //    —— 是本人实测指出来的。规则点阵的另一个好处：它是 **S6 相机的参照物**。
    //    开局视野里能看见 x = 10..70（7 列）、y = 10 与 20（2 行）的整齐点阵。
    constexpr int kPillarStep = 10;
    for (int py = kPillarStep; py < h - 1; py += kPillarStep) {
        for (int px = kPillarStep; px < w - 1; px += kPillarStep) {
            map.Set(px, py, Tile::Wall);
        }
    }

    // 3) 一个**不对称地标**（7×4 的一栋"楼"）。
    //    为什么光有规则点阵不够：点阵平移一个周期（10 格）之后长得**一模一样**，
    //    光看点阵分不清"画面动了"还是"没动、只是数字在跳"。
    //    有个独一份的方块，才能一眼看出"确实在动、动了多远"。位置也选在开局视野内。
    //    ⚠️ y 刻意取 5..8（不取 6..9）：它的下边界要跟柱阵那一行（y=10）**隔开一格**，
    //       否则屏幕上"楼"和正下方那颗柱子连成一串，看着像画错了。
    constexpr int kBlockX = 30;
    constexpr int kBlockY = 5;
    constexpr int kBlockW = 7;
    constexpr int kBlockH = 4;
    for (int y = kBlockY; y < kBlockY + kBlockH; ++y) {
        for (int x = kBlockX; x < kBlockX + kBlockW; ++x) {
            map.Set(x, y, Tile::Wall);
        }
    }
}

void Game::OnStart()
{
    BuildDemoMap();

    // 玩家：出生在 (kSpawnX, kSpawnY) —— 格中心，所以 floor 之后是第 2 格。
    // 速度留空（{0,0}）：松开键就停，不需要额外"刹车"逻辑。
    mini::world::Entity player{};
    player.transform.pos    = mini::math::Vec2{kSpawnX, kSpawnY};
    player.velocity.perSecond = mini::math::Vec2{};
    player.symbol           = '@';

    m_player = m_world.Spawn(player);

    // S8：铺敌人。放在**玩家之后**是刻意的 —— 渲染顺序 = `Entities()` 的顺序，
    // 玩家先画、敌人后画；两者位置刻意隔开（见 `kPatrolPaths` 的注释），不会互相盖掉。
    SpawnEnemies();
}

// ------------------------------------------------------------
// 敌人（S8）
//
// 三个函数的分工：
//   SpawnEnemies   开局铺 5 个：一个 `Entity`（`symbol = 'E'`）+ 一条 `EnemyData`
//   DriveEnemyAi   每帧：对每个敌人跑一次 `ai::Decide`，**只写速度**（不碰位置）
//   IsEnemyId      只给绘制上色用
// ------------------------------------------------------------

void Game::SpawnEnemies()
{
    m_enemies.reserve(static_cast<std::size_t>(kEnemyCount));

    for (int i = 0; i < kEnemyCount; ++i) {
        // 出生点 = 这条路线的第一个端点。它必须是**空格**（人工核对过，见 `kPatrolPaths`）。
        mini::world::Entity prototype{};
        prototype.transform.pos      = kPatrolPaths[i][0];
        prototype.velocity.perSecond = mini::math::Vec2{};   // 速度每帧由 AI 写，这里只是初值
        prototype.symbol             = 'E';

        const mini::world::EntityId id = m_world.Spawn(prototype);

        // ⚠️ 逐敌一条路：`patrolPath` = 它负责哪一行；`waypointIdx` = 在这行里走到第几个点。
        //    这是"全敌共用一张平表"的实现方式 —— 表是编译期常量，逐敌只多 1 个字节。
        EnemyData enemy{};
        enemy.id          = id;
        enemy.state       = mini::ai::EnemyState::Idle;
        enemy.patrolPath  = static_cast<std::uint8_t>(i);
        enemy.waypointIdx = 0;
        // `idleSeconds` 是"Idle 该持续多久"的**唯一**来源，由这里写进定时器（见 EnemyBrain.h 的说明）
        enemy.idleTimer   = mini::ai::kDefaultTuning.idleSeconds;
        enemy.attackTimer = 0.0f;
        enemy.hp          = kEnemyMaxHp;

        m_enemies.push_back(enemy);
    }
}

void Game::DriveEnemyAi(const mini::core::FrameContext& ctx)
{
    // 目标位置 = 玩家。拿不到（还没出生 / 已经被删）就用 {0,0} —— 那是退化的"假目标"：
    // 敌人照常走自己的巡逻路线（本阶段的出生点离原点都很远，不会误进战），不崩、不除零。
    const mini::world::Entity* player = m_world.Find(m_player);
    const mini::math::Vec2 playerPos =
        (player != nullptr) ? player->transform.pos : mini::math::Vec2{};

    for (EnemyData& enemy : m_enemies) {
        // ⚠️ 遍历 `m_enemies` 时**禁止 Spawn**（E-9：会让 `Entities()` 的 span 与
        //    `Find()` 的指针一起失效）。本函数只读世界 + 写速度，不增删实体。
        mini::world::Entity* entity = m_world.Find(enemy.id);
        if (entity == nullptr) {
            // 悬空 id 防御：这是分叉 ①（"两套真相"）的代价之一 —— 删敌人时两边没同步就走到这。
            // 静默跳过而不是崩，是因为"父容器里少了一个实体"不该让游戏挂掉。
            continue;
        }

        // 这个敌人负责的那条路线（2 个点 → `(idx+1) % 2` 就是往返）。
        // ⚠️ `patrolPath` 只由 `SpawnEnemies` 写成 `0 .. kEnemyCount-1`，所以这里不会越界；
        //    两个常量都显式转 size_t，免得 /W4 的 signed/unsigned 提示。
        const std::span<const mini::math::Vec2> path{
            kPatrolPaths[static_cast<std::size_t>(enemy.patrolPath)],
            static_cast<std::size_t>(kPatrolPathLen)};

        mini::ai::BrainInput in{};
        in.self        = entity->transform.pos;
        in.target      = playerPos;
        in.waypoints   = path;
        in.waypointIdx = enemy.waypointIdx;
        in.state       = enemy.state;
        in.attackTimer = enemy.attackTimer;
        in.idleTimer   = enemy.idleTimer;
        in.selfAlive   = (enemy.hp > 0);     // S8 里恒 true（还没有伤害来源）；S9 的子弹扣它
        in.targetAlive = (m_playerHp > 0);   // ⚠️ **S8 步骤 3 改动的那一行**：让敌人开始会追人。
                                             //    HP 归 0 → 敌人把它当"死了的目标"而脱战
                                             //    （S8 里玩家不会死、也不会重开 —— 那是 S10）。
        in.dt          = ctx.deltaSeconds;

        const mini::ai::BrainOutput out = mini::ai::Decide(in, mini::ai::kDefaultTuning);

        // ⚠️ **只写速度**：位移仍然由 `World::Update` → `MoveAndCollide` 做。
        //    全项目只有那一条积分路径，所以敌人**自动**会撞墙、会沿墙滑 —— 一行都不用写。
        entity->velocity.perSecond = out.velocity;

        enemy.state       = out.state;
        enemy.waypointIdx = out.waypointIdx;
        enemy.attackTimer = out.attackTimer;
        enemy.idleTimer   = out.idleTimer;

        // 攻击结算（S8 的"伤害"总共就这一行：没有弹道、没有命中判定，纯"距离 + 冷却"）。
        // ⚠️ 夹到 0：HP 不许变负 —— 否则 HUD 会显示 `hp -3`，而且"<= 0 = 死"这条判据会失去意义。
        if (out.damage > 0) {
            m_playerHp = std::max(0, m_playerHp - out.damage);
        }

        // Dead 的**唯一后果**就在这里（S8_CHECKLIST 6.6）：把实体"停更"，**不是删掉**。
        //   · `World::Update` 会因为 `!active` 跳过它 → 不再移动；
        //   · `DrawEntities` 也会跳过它 → **从屏幕上消失**（"被击杀"的观感）；
        //   · 但 `EnemyData` 留在 `m_enemies` 里 → **面板仍然能显示 `state=Dead`**（取证）。
        // ⚠️ 真删（MarkForRemoval + 两边同步移除）留到 S9。
        if (enemy.hp <= 0) {
            entity->active = false;
        }
    }
}

// ------------------------------------------------------------
// S9：子弹（生成 / 判定）
// ------------------------------------------------------------

void Game::FireBullet()
{
    const mini::world::Entity* player = m_world.Find(m_player);
    if (player == nullptr) {
        return;   // 玩家还没出生（OnStart 之前）
    }

    mini::world::Entity bullet{};
    bullet.transform.pos      = player->transform.pos;                                       // 从玩家脚下出发
    bullet.velocity.perSecond = game::FacingToVec2(m_facing) * game::kDefaultBullet.speed;    // 只朝 8 个方向
    bullet.symbol             = kBulletGlyph;

    // ⚠️ `Spawn` 内部是 `m_entities.push_back`（**可能重新分配**）——
    //    所以调用点必须在"遍历 `m_entities`"**之外**。帧序保证了这一点：
    //    ⑤c 之后才轮到 ⑥（积分）与 `DrawEntities`（渲染），此前没有任何遍历。
    // ⚠️ 子弹**不需要任何额外数据**（分叉 ①）：位置/速度/字符全在 `Entity` 里，
    //    所以这里只把 id 记进 `m_bullets` —— 那是"哪些实体是子弹"的索引，不是数据的副本。
    m_bullets.push_back(m_world.Spawn(bullet));
}

void Game::ResolveBullets(const mini::core::FrameContext& ctx)
{
    if (m_bullets.empty()) {
        return;
    }

    // ⚠️ 遍历的是 `m_bullets`（**不是** `m_entities`）：这一点让 PLAN 点名的
    //    "遍历中 push_back 导致迭代器失效"这个坑**在架构上消失** ——
    //    本函数既不 Spawn 也不 erase，手里的 `Entity*` 全程有效。
    for (const mini::world::EntityId id : m_bullets) {
        mini::world::Entity* bullet = m_world.Find(id);
        if (bullet == nullptr) {
            continue;   // 防御：理论上 ⑨b 的压缩已经清过了（同一帧内的极端情况）
        }

        const mini::math::Vec2 from = bullet->transform.pos;
        // **预测线段**：这一帧子弹要走的整段路。判定必须在 ⑥ 之前，理由见 Game.h 的注释。
        const mini::math::Vec2 to = from + bullet->velocity.perSecond * ctx.deltaSeconds;

        // ① 每次都重新收一遍"活着且 id 有效的敌人位置"（5 个元素，栈上定长数组，不分配）。
        //    ⚠️ **每发子弹重新收**是刻意的：这一帧前面的子弹可能已经打死了某个敌人，
        //       那样它就该从表里消失 —— 否则（a）`kills` 会被同一具尸体重复计数，
        //       （b）后面的子弹会被一具看不见的尸体挡住。
        std::array<mini::math::Vec2, kEnemyCount> positions{};
        std::array<std::size_t, kEnemyCount>      which{};   // 第 i 项对应 m_enemies 的哪一个
        std::size_t aliveCount = 0;
        for (std::size_t i = 0; i < m_enemies.size() && aliveCount < kEnemyCount; ++i) {
            const mini::world::Entity* e = m_world.Find(m_enemies[i].id);
            if (e == nullptr || m_enemies[i].hp <= 0) {
                continue;   // 悬空 id（分叉 ① 的代价）或已经死了
            }
            positions[aliveCount] = e->transform.pos;
            which[aliveCount]     = i;
            ++aliveCount;
        }
        // 纯函数拿到的只是"一串位置" —— 它不认识 Entity，也不需要认识（所以能离线单测）。
        const std::span<const mini::math::Vec2> enemyPositions{positions.data(), aliveCount};

        const game::BulletHit hit = game::FirstEnemyOnSegment(m_world.Map(), from, to, enemyPositions);

        // ② 撞墙 或 命中敌人 ⇒ 这发子弹当帧就结束（`MarkForRemoval` 只碰 `m_pendingRemove`，
        //    在遍历里调用是安全的 —— 真正的移除在 ⑨ 的 `FlushRemovals`）。
        if (hit.blocked || hit.enemyIndex >= 0) {
            m_world.MarkForRemoval(id);
        }

        if (hit.enemyIndex >= 0) {
            EnemyData& target = m_enemies[which[static_cast<std::size_t>(hit.enemyIndex)]];
            target.hp -= game::kDefaultBullet.damage;

            // ⚠️ 只在**跨过 0**的那一次计数（不是"打中一次 +1"）。因为上面每发子弹都重收表，
            //    已经 <= 0 的敌人根本不在表里，所以这里天然只会走到一次。
            if (target.hp <= 0) {
                ++m_kills;
                // ⚠️ 这里**不设** `entity->active = false`：那是 `DriveEnemyAi` 的职责
                //    （同一帧的 ⑤b 已经跑过了，所以下一帧生效）——"怎么死"只有一条路径。
                //    真删（`MarkForRemoval` + 移除 `EnemyData`）是 **S9 步骤 4** 的事。
            }
        }
    }
}

bool Game::IsEnemyId(mini::world::EntityId id) const noexcept
{
    for (const EnemyData& enemy : m_enemies) {
        if (enemy.id == id) {
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------------
// 每帧更新
// ------------------------------------------------------------

void Game::OnUpdate(const mini::core::FrameContext& ctx)
{
    using mini::platform::Key;
    using mini::platform::KeyState;

    const mini::platform::Input& input = ctx.input;

    // ① 方向：按住就算（IsDown = Pressed 或 Held）。
    //    这里**不用** Pressed —— 那是 S3"按一次跳一格"的写法；
    //    "按住连续走"靠的是速度 × dt，不是系统的按键自动重复。
    mini::math::Vec2 dir{};
    if (input.IsDown(Key::W) || input.IsDown(Key::ArrowUp))    { dir.y -= 1.0f; }
    if (input.IsDown(Key::S) || input.IsDown(Key::ArrowDown))  { dir.y += 1.0f; }
    if (input.IsDown(Key::A) || input.IsDown(Key::ArrowLeft))  { dir.x -= 1.0f; }
    if (input.IsDown(Key::D) || input.IsDown(Key::ArrowRight)) { dir.x += 1.0f; }

    // ② 归一化：不归一的斜向会快 √2 倍（对角 {1,1} 的长度是 1.414）。
    //    零向量不用管：`Normalized()` 对 len2 == 0 返回零向量（S4 已定，含 NaN 一起挡）。
    if (dir.LengthSq() > 0.0f) {
        dir = dir.Normalized();
    }

    // ③ 写速度 —— **必须在 ④ 之前**（帧序硬约束）。
    //    用 `m_moveSpeed` 而不是 `kMoveSpeed`：后者只是默认值，实际速度可被 argv[2] 覆盖。
    if (mini::world::Entity* player = m_world.Find(m_player); player != nullptr) {
        player->velocity.perSecond = dir * m_moveSpeed;
    }

    // ③b 朝向（S9，分叉 ③）：**移动方向即朝向，停下沿用**。
    //     ⚠️ 这里用**返回值**判断，而不是"反正它对零向量不写 out" —— 把"停下沿用"这条规则
    //        摆在**调用点**上：以后有人改 `DirectionToFacing` 的行为，这里不会静默跟着变。
    //     ⚠️ 例：松开所有方向键时 `dir` 是零向量 ⇒ 返回 false ⇒ `m_facing` 保持上一帧的值，
    //        所以"站着不动也能朝原方向开火"。
    Facing8 nextFacing = m_facing;
    if (game::DirectionToFacing(dir, nextFacing)) {
        m_facing = nextFacing;
    }

    // ④ Q 退出。Esc 刻意不退出 —— 它从 S10 起要用来关菜单（S2 的决策），
    //    进程级退出（Ctrl+C / 关窗口）由 Application 看 Input::QuitRequested()。
    if (input.Get(Key::Q) == KeyState::Pressed) {
        m_quit = true;
    }

    // ④b `kAiPanelKey` 开/关敌人面板（S8 的临时面板，S11 会换成正式 Overlay）。
    //     只在 `Pressed` 翻转：用 `Held` 会每帧闪。
    if (input.Get(kAiPanelKey) == KeyState::Pressed) {
        m_showAiPanel = !m_showAiPanel;
    }

    // ④c **已删除**（2026-10-10，S9 步骤 3）：S8 的临时杀敌键 `K` 被真入口（子弹）取代，
    //     `Key::K` 也从 `KeyCode.h` 删了（`TECH_DEBT` C15 到期）。
    //     ⚠️ 连带影响：S8 的用例 `Game.EnemyDiesAndDisappears` 原来按 `F7`+`K`，
    //        本步同步改写成"朝敌人开火把它打死"（不然它会因为键没了而变红）。

    // ⑤ Σdt：HUD 拿它自证 `pos ≈ v × sum dt`（10.2 的运行时验收；v 就是 m_moveSpeed）
    m_elapsed += ctx.deltaSeconds;

    // ⑤b 敌人决策（S8）—— **必须在 ⑥ 之前**：敌人和玩家在**同一个循环里**被积分，
    //     所以两边的速度都得先写好（与 ③ 是同一条帧序约束）。
    //     反过来的现象：敌人慢一帧，或（若挪到 ⑥ 之后并写了速度）完全不产生位移。
    DriveEnemyAi(ctx);

    // ⑤c 开火（S9）—— **必须在 ⑥ 之前**：子弹当帧就要飞出去（否则慢一帧）。
    //     ⚠️ `Pressed` 是**边沿触发**（按一下一发）：用 `IsDown` 会按住连射。
    if (input.Get(kFireKey) == KeyState::Pressed) {
        FireBullet();
    }

    // ⑤d 子弹的命中判定 + 标删（S9）—— **也在 ⑥ 之前**，用的是**预测线段**。
    //     理由写在 `Game.h` 的 `ResolveBullets` 注释里（挪到 ⑥ 之后就要存"上一帧位置"，
    //     而反推在被墙夹住时是错的）。
    ResolveBullets(ctx);

    // ⑥ 积分（world 唯一的"物理"）。dt 已由 S3 clamp 过，这里**不再夹**（坑 #7）
    //    ⚠️ 子弹也在这里被积分 —— 它是普通 `Entity`，所以 S7 的碰撞/夹紧**白拿**。
    m_world.Update(ctx.deltaSeconds);

    // ⑦ 相机跟随 —— **必须在 ⑥ 之后**（用本帧最新的玩家位置，S6 的帧序硬约束）。
    //    尺寸用 ctx 而不是 fb：`Application::Run` 第 ③ 步先 Resize、第 ④ 步才造 ctx，
    //    所以**本帧**两者是一致的（S6_CHECKLIST 前置事实第 5 条）→ 不需要 resize 钩子。
    //    ⚠️ 视口高要减掉引擎顶栏那一行：相机只管"多少格"，不知道 HUD 的存在（6.5 的契约）。
    if (const mini::world::Entity* player = m_world.Find(m_player); player != nullptr) {
        m_camera.Follow(player->transform.pos,
                        mini::render::CameraLimits{ctx.screenWidth,
                                                   ctx.screenHeight - kHudRow,
                                                   m_world.Map().Width(),
                                                   m_world.Map().Height()});
    }

    // ⑧ 调试快照（键 = `kDumpKey`，见本文件顶部的常量）。放在相机之后 = 记的是"本帧最终"的位置与相机。
    //    ⚠️ **只读**：写文件 + 递增计数器，不碰世界 → 帧序不受影响。
    //    ⚠️ 只在 `Pressed` 时写（不是 `Held`）—— 否则按住会每帧刷盘。
    if (input.Get(kDumpKey) == KeyState::Pressed) {
        DumpSnapshot(ctx);
    }

    // ⑨ 延迟删除。**S9 起第一次真的有东西被删**（子弹撞墙/命中后会进这里）
    m_world.FlushRemovals();

    // ⑨b 子弹成员表**自愈压缩**（S9，分叉 ① 的"只增 + 一处压缩"）。
    //     放在 `FlushRemovals` **之后**：这样表里**永远只有活着的**子弹，
    //     `ResolveBullets` 里那次 `Find(id) == nullptr` 就只是防御（不是常规路径）。
    //     ⚠️ 这是**唯一**一处从 `m_bullets` 里删东西的地方 —— 不存在"两边必须同步"的双向责任。
    //     ⚠️ 判据是"世界还找得到它吗"，不是"我记得它该死"：即使将来有别的系统删了子弹，
    //        这里也会自动清掉（这正是不存数据副本换来的好处）。
    std::erase_if(m_bullets, [this](const mini::world::EntityId id) {
        return m_world.Find(id) == nullptr;
    });
}

// ------------------------------------------------------------
// 每帧绘制（只写后缓冲，不碰终端）
// ------------------------------------------------------------

void Game::DrawMap(mini::render::FrameBuffer& fb) const
{
    const mini::world::TileMap& map = m_world.Map();
    const int w = fb.Width();
    const int h = fb.Height();

    // S6：屏幕第 c 列显示世界第 (相机整数原点 + c) 格。
    // ⚠️ 相机原点**只在这里取一次**（`OriginCol/OriginRow` 内部是 S6 里唯一的 floor），
    //    下面的循环全是整数加减 —— 不能再出现第二次取整，否则就是施工图 6.4 说的
    //    "两条取整路径"（E-8 的成因）。
    const int originCol = m_camera.OriginCol();
    const int originRow = m_camera.OriginRow();

    for (int row = kHudRow; row < h; ++row) {
        const int worldRow = originRow + (row - kHudRow);
        for (int col = 0; col < w; ++col) {
            if (map.At(originCol + col, worldRow) == mini::world::Tile::Wall) {
                fb.Set(col, row, '#', mini::render::Color::BrightBlack);
            }
        }
    }
}

void Game::DrawEntities(mini::render::FrameBuffer& fb) const
{
    // 与 DrawMap 用**同一个整数原点**（施工图 7.2 的硬规矩）：
    //   实体必须画在"它脚下那一格"被画到的列上 → 屏幕列 = ToIndex(pos) − OriginCol()。
    // ⚠️ 不能写成 `floor(pos.x − 相机 pos)`（PLAN A.5 的原始写法）：相机 pos 含小数时
    //    两者差一格 → 症状是 `@` 与它脚下的格子错位（E-8）。反向验证 R2 就钉这条。
    const int originCol = m_camera.OriginCol();
    const int originRow = m_camera.OriginRow();

    // const 版 Entities()：本方法是 const。span 只在这次遍历里有效
    for (const mini::world::Entity& entity : m_world.Entities()) {
        if (!entity.active) {
            continue;   // `active == false` → **这里也不画**（不只是"不参与积分"）
        }
        // ⚠️ 上面这个 `continue` 是 S8 的 `Dead` 敌人"从屏幕上消失"的**唯一**原因
        //    （`DriveEnemyAi` 把 hp<=0 的实体置 inactive）。删掉它 → `Game.EnemyDiesAndDisappears` 会红。

        // 世界 → 屏幕：**floor**（`ToIndex`），再减去相机原点；行还要加回 kHudRow
        const int col = mini::world::TileMap::ToIndex(entity.transform.pos.x) - originCol;
        const int row = mini::world::TileMap::ToIndex(entity.transform.pos.y) - originRow + kHudRow;

        // 颜色：玩家黄 / 敌人红 / 其余（将来的子弹等）白。
        // ⚠️ 这里靠 `IsEnemyId` 扫一遍 `m_enemies` 才知道"它是不是敌人" ——
        //    `Entity` 里只有 `symbol`，"谁是敌人"这个身份归 game/ 说了算（分叉 ①）。
        mini::render::Color color = mini::render::Color::BrightWhite;
        if (entity.id == m_player) {
            color = mini::render::Color::BrightYellow;
        } else if (IsEnemyId(entity.id)) {
            color = mini::render::Color::BrightRed;
        }

        fb.Set(col, row, entity.symbol, color);
    }
}

void Game::DrawHud(mini::render::FrameBuffer& fb) const
{
    // const 版 Find：本方法是 const，所以只能拿 const Entity*（改速度那条路走非 const 版）
    const mini::world::Entity* player = m_world.Find(m_player);
    if (player == nullptr) {
        return;   // OnStart 之前不画 HUD：宁可空着，也不显示假的 0
    }

    const float px = player->transform.pos.x;
    const float py = player->transform.pos.y;

    // 先把这一行铺满空格，再写字：否则 HUD 右边会露出地图在**视口第 0 行**的内容
    //（= 相机原点那一行），看着像"画面坏了"（S3 的顶栏也是这样独占一行的）
    for (int col = 0; col < fb.Width(); ++col) {
        fb.Set(col, kHudRow, ' ');
    }

    // tile 坐标用**同一个 floor**（`ToIndex`）—— HUD 说的第几格必须和画面上的一致。
    //
    // `v` 这一格是**速度**（格/秒）：看起来是调试信息，其实是 10.2 那条自证的**必需项** ——
    // 屏幕上要能一眼核对 `pos ≈ v × sum dt`。速度现在可用 argv[2] 覆盖，所以必须显示出来。
    // 提示文字压缩过，否则这一行会超过 80 列（终端宽度）。
    // ⚠️ 2026-10-08 加了调试快照键（`kDumpKey`，见本文件顶部常量）：加之前约 71 列，加之后 **73 列**。
    // ⚠️ 2026-10-09（S8 步骤 3）这一行**改窄过一次**：要加 `hp`，而 73 列已没有余量，
    //    所以同时砍掉了两个纯粹的冗余（`S8_CHECKLIST` 坑 8 预判的"要么砍栏目、要么简写"）：
    //      · `sum dt %6.2fs` → `sumdt %6.2f`（少一个空格、去掉单位 's'）
    //      · `WASD  Q` → `WASD Q`（双空格合并）
    //    现在是 **78 列**。⚠️ 想再加栏目必须先砍掉别的 —— 这条预算从 S6 决策 5 起就没松过
    //    （当时为了它没显示 `cam`）。
    char text[96];
    std::snprintf(text, sizeof(text),
                  "pos %6.2f,%6.2f | tile %4d,%4d | sumdt %6.2f | v %4.1f | hp %2d | WASD Q F4",
                  static_cast<double>(px), static_cast<double>(py),
                  mini::world::TileMap::ToIndex(px),
                  mini::world::TileMap::ToIndex(py),
                  static_cast<double>(m_elapsed),
                  static_cast<double>(m_moveSpeed),
                  m_playerHp);
    fb.Draw(0, kHudRow, text);
}

// ------------------------------------------------------------
// `kAiPanelKey` 敌人面板（S8 的**临时**面板；S11 换成正式 Debug Overlay）
//
// 逐敌一行：`序号 状态 到玩家的距离 占的格`。这就是 `PLAN:572` 要的
// "F3 打开后能实时看到 State / Target / Distance" 里**能看到**的那一半。
//
// ⚠️ 三条约定：
//   · **底部对齐**（见 `kAiPanelHeaderLines` 那段说明）—— 不是"HUD 的下一行"，理由在那里；
//   · **只写左边一条窄带**（不铺满整行）—— 右边留给地图，面板是"压在画面上的便条"；
//   · 全 ASCII。⚠️ 不写中文：终端代码页是 936，UTF-8 中文在那种控制台里是乱码
//     （同一行 HUD 也一直只用 ASCII）。
//
// ⚠️ 关掉面板不会有残留：`Application::Run` **每帧**都先 `Clear()` 再 `OnRender`
//    （`Application.cpp:126`），所以下一帧那几行会被地图正常重画。
// ------------------------------------------------------------
void Game::DrawAiPanel(mini::render::FrameBuffer& fb) const
{
    const int lines = kAiPanelHeaderLines + static_cast<int>(m_enemies.size());

    // 底部对齐：面板的**最后一行**贴住画布的最后一行。
    int firstRow = fb.Height() - lines;

    // 画布太矮（敌人比剩余行还多）→ 退回 HUD 下面，能画几行画几行（循环里会自己截断）
    if (firstRow <= kHudRow) {
        firstRow = kHudRow + 1;
    }
    if (firstRow >= fb.Height()) {
        return;   // 连一行都放不下：宁可什么都不画，也不越界写
    }

    // 玩家位置（用来算 distance）。拿不到就用 {0,0} —— 与 `DriveEnemyAi` 同一个口径。
    const mini::world::Entity* player = m_world.Find(m_player);
    const mini::math::Vec2 playerPos =
        (player != nullptr) ? player->transform.pos : mini::math::Vec2{};

    char line[64];

    // ⚠️ 表头里那个键名与 `kAiPanelKey` 是**同一个字母的两个落点**（同 `kDumpKey` 的纪律）：
    //    换键时要一起改这两处，别的注释一律写 `kAiPanelKey`、不抄字母。
    // ⚠️ 面板键**不进 HUD 提示行**：那一行已经 78/80 列，塞不下第三个键（偏差 4 / 坑 8）。
    std::snprintf(line, sizeof(line), "F7 AI  n=%d   K=kill nearest",
                  static_cast<int>(m_enemies.size()));
    fb.Draw(0, firstRow, line);

    int row = firstRow + kAiPanelHeaderLines;
    int idx = 0;
    for (const EnemyData& enemy : m_enemies) {
        if (row >= fb.Height()) {
            break;   // 敌人比剩余行多：画不下的就不画（不滚屏、不折行）
        }

        const mini::world::Entity* entity = m_world.Find(enemy.id);
        if (entity == nullptr) {
            // 悬空 id：面板要**看得见**这件事，不能像 DriveEnemyAi 那样静默跳过
            std::snprintf(line, sizeof(line), "%2d %-7s  <id gone>", idx, StateName(enemy.state));
        } else {
            const mini::math::Vec2 pos = entity->transform.pos;
            const float dist = (pos - playerPos).Length();
            std::snprintf(line, sizeof(line), "%2d %-7s  d=%6.2f  t=(%3d,%3d)", idx,
                          StateName(enemy.state), static_cast<double>(dist),
                          mini::world::TileMap::ToIndex(pos.x),
                          mini::world::TileMap::ToIndex(pos.y));
        }

        fb.Draw(0, row, line);
        ++row;
        ++idx;
    }
}

void Game::OnRender(mini::render::FrameBuffer& fb)
{
    DrawMap(fb);
    DrawEntities(fb);
    // HUD 压在**视口第 0 行**上（= 相机原点那一行；同"顶栏压在最上面"一个道理）。
    // ⚠️ 顺序不能换：HUD 要盖掉地图，而 `@` 必须画在地图之后、HUD 之前。
    DrawHud(fb);

    // 面板最后画。它**底部对齐**，与 HUD（第 1 行）以及敌人常在的第 3~7 行都不重叠。
    if (m_showAiPanel) {
        DrawAiPanel(fb);
    }
}

// ------------------------------------------------------------
// 调试快照（键 = `kDumpKey`，2026-10-08 加；**同日随"点模型"改过字段**）
//
// 目的：查"**看着没贴墙、却动不了**"那一类问题。
//
//   第一版是为抓"盒子占 4 格、屏幕只画 1 格"这种**看不见的挡路者**
//   （报障原文：`pos=(29.5,4.84)` 按 D 不动 —— 盒子探进了看不见的 `(30,5)`）。
//   2026-10-08 碰撞模型改成**点模型**（占格 = `ToIndex(pos)`）之后，这类问题**结构上消失**了，
//   所以字段也换成新模型真正有用的那几样：
//     · `tile` / `frac`：占哪一格、在这一格里的什么位置（离边界多远）
//     · `now` + 四邻：往某个方向能不能走，就是看**那一格**实不实心
//     · `solidNow` / `tryStepX/Y`：现在站的格 / 用本帧 v×dt 试算之后的格实不实心
//   留着它仍然有用：人眼验收（"顶墙停在哪"、"贴墙滑到哪"）也得取证，而不是靠眼睛估。
//
// ⚠️ 契约：
//   · **只读**世界（不改 pos、不碰相机）→ 放在帧序哪一步都不影响别的系统；
//   · 打开失败（目录只读 / 文件被占）就**静默返回** —— 调试输出不许影响游戏能不能跑；
//   · 追加模式 + 每次 flush（进程被杀也不丢；按一次才写，代价可忽略）；
//   · ⚠️ 这是 S11 的 Logger 之前的**临时出路**（`TECH_DEBT` C14）。
// ------------------------------------------------------------
void Game::DumpSnapshot(const mini::core::FrameContext& ctx)
{
    // ⚠️ 判断"新文件"**必须在打开之前**问文件系统 —— 两种错误做法都踩过：
    //   · `out.tellp()`：追加模式下未写入前它**不可靠**（2026-10-08 实测：**每按一次都返回 0**
    //     → BOM 和表头被反复插进文件中间）。**这就是那个 bug 的成因。**
    //   · `m_dumpCount == 1`：计数器每次进程启动都从 0 开始 → 第二次运行也会重复插。
    std::error_code ec;
    const bool isNewFile = !std::filesystem::exists(kDumpFileName, ec) ||
                           std::filesystem::file_size(kDumpFileName, ec) == 0;

    std::ofstream out(kDumpFileName, std::ios::app);
    if (!out) {
        return;
    }

    ++m_dumpCount;

    // 新文件（或空文件）→ 先写 **UTF-8 BOM**，再写一段字段说明（**只写一次**）。
    //
    // ⚠️ 为什么非写 BOM 不可：Windows PowerShell 的 `Get-Content` 不带 `-Encoding` 时按
    //    **GBK(936)** 解码 UTF-8，中文表头就显示成 `鐜╁涓績` 这种乱码（2026-10-08 实测）。
    //    有 BOM 它就会自动认成 UTF-8 —— 这是让"数据文件在默认命令下就能读"最省事的一次性修法。
    //    代价：文件头多 3 个字节（`*.log` 已被 `.gitignore` 忽略，与仓库无关）。
    if (isNewFile) {
        out << "\xEF\xBB\xBF";   // UTF-8 BOM
        out << "# MiniEngine 调试快照（按下 HUD 里显示的那个调试键；每按一次追加两行）\n"
               "#   pos = 玩家中心的浮点坐标；tile = 它占的那一格（== 屏幕上 @ 所在的那一格）\n"
               "#   frac = 在这一格里的位置（0~1）—— 离哪条边界还有多远看它\n"
               "#   now/E/W/S/N = 自己那一格与四邻（E=+x  W=-x  S=+y  N=-y）：  '.'=空  '#'=实心（含越界）\n"
               "#   solidNow = 现在站的这一格实心吗；tryStepX/tryStepY = 用本帧 v×dt 试算后占的那一格实心吗（1/0）\n"
               "#   ⚠️ 新模型（2026-10-08 起）：实体占的**就是**它画出来的那一格 —— 挡路的一定是看得见的那一格\n";
    }

    const mini::world::Entity* player = m_world.Find(m_player);
    if (player == nullptr) {
        out << "dump=" << m_dumpCount << " (player not spawned)\n";
        return;
    }

    const float px = player->transform.pos.x;
    const float py = player->transform.pos.y;
    const float vx = player->velocity.perSecond.x;
    const float vy = player->velocity.perSecond.y;

    // 占格 + "在这一格里的位置"：新模型下**这两样就是全部几何**（不再有盒子，也就没有 box=）
    const int   tx = mini::world::TileMap::ToIndex(px);
    const int   ty = mini::world::TileMap::ToIndex(py);
    const float fx = px - static_cast<float>(tx);   // 0 ~ 1
    const float fy = py - static_cast<float>(ty);

    const float dx = vx * ctx.deltaSeconds;
    const float dy = vy * ctx.deltaSeconds;

    out << "dump=" << m_dumpCount
        << " sumdt=" << m_elapsed
        << " v=" << vx << ',' << vy
        << " pos=" << px << ',' << py
        << " tile=(" << tx << ',' << ty << ')'
        << " frac=(" << fx << ',' << fy << ')'
        << " canvas=" << ctx.screenWidth << 'x' << ctx.screenHeight
        << " fps=" << ctx.fps
        << " ms=" << ctx.frameMs
        << " max=" << ctx.maxFrameMs
        << " work=" << ctx.workMs
        << " cam=" << m_camera.OriginCol() << ',' << m_camera.OriginRow() << '\n';

    // 自己那一格 + 四邻：新模型里"往某方向能不能走"就是看**那一格**实不实心
    out << "  now(" << tx << ',' << ty << ")=" << CellMark(m_world.Map(), tx, ty)
        << " E(" << (tx + 1) << ',' << ty << ")=" << CellMark(m_world.Map(), tx + 1, ty)
        << " W(" << (tx - 1) << ',' << ty << ")=" << CellMark(m_world.Map(), tx - 1, ty)
        << " S(" << tx << ',' << (ty + 1) << ")=" << CellMark(m_world.Map(), tx, ty + 1)
        << " N(" << tx << ',' << (ty - 1) << ")=" << CellMark(m_world.Map(), tx, ty - 1)
        << " solidNow="
        << (mini::world::IsSolidAt(m_world.Map(), mini::math::Vec2{px, py}) ? 1 : 0)
        << " tryStepX="
        << (mini::world::IsSolidAt(m_world.Map(), mini::math::Vec2{px + dx, py}) ? 1 : 0)
        << " tryStepY="
        << (mini::world::IsSolidAt(m_world.Map(), mini::math::Vec2{px, py + dy}) ? 1 : 0) << '\n';

    out.flush();
}

// ------------------------------------------------------------
// 玩家状态 / 退出（S8 加的那个只读接口）
// ------------------------------------------------------------

// HUD 与单测的公开只读入口。真正的真相是私有的 `m_playerHp`（`DriveEnemyAi` 直接改它）。
int Game::PlayerHp() const noexcept
{
    return m_playerHp;
}

// 同上：真相是私有的 `m_kills`（只有 `ResolveBullets` 在"hp 跨过 0"那一次改它）。
// ⚠️ 它是**击杀数**，不是"命中数"：一发打中但没打死不算（用例 `BulletDamagesEnemy` 钉这一条）。
int Game::Kills() const noexcept
{
    return m_kills;
}

// 同上：真相是私有的 `m_facing`（`OnUpdate` 的 ③b 每帧更新它）。
Facing8 Game::Facing() const noexcept
{
    return m_facing;
}

bool Game::ShouldQuit() const
{
    return m_quit;
}

} // namespace game
