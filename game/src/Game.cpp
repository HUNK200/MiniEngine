#include "game/Game.h"

#include "game/Bullet.h"           // S9：朝向 ↔ 向量、线段命中判定（纯函数）
#include "game/GameState.h"         // S10：四态 + 策略表 + 纯函数 NextState / PolicyOf
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
#include <initializer_list>  // S10：DrawCenteredBlock 的行表
#include <span>         // S8：把巡逻路点交给 Decide；S9：把敌人位置交给 FirstEnemyOnSegment
#include <string_view>  // S10：DrawCenteredBlock 要算文本长度（居中）

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
// ⚠️ 帧序（S5_CHECKLIST 7.1 + S6 7.1 + S8 §6.3 + S9 §7.1 + S10 §7.1，顺序不能换）：
//     ⓪ 造「状态事件」→ ① 转移状态（**唯一入口** = `NextState`）→ ② 取策略 → ③ 按策略分派：
//         读输入 → 归一化（顺手更新朝向）→ **写玩家速度** → 驱动 AI（**写敌人速度**）
//         → **生成子弹** → **命中判定/标删** → Update(dt) → **相机 Follow** → FlushRemovals
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
//   ⚠️ **S10 新加的 ⓪①②为什么在最前面**（三条，别换）：
//    1. ⓪ 在**读方向键之前**：`PLAN` 的验收要求"**HP 归零后不再响应移动**" ⇒ 死亡事件必须
//       先于方向键生效。代价是死亡响应**最早等下一帧**（伤害发生在上一帧的 ⑤b）——
//       这也可接受，且是唯一的正确时机（本帧开始时他还活着）。
//    2. ① 只经 `NextState`：转移**只有一个入口**（别在扣血那里直接写 `m_state`）。
//    3. ②③ 按策略分派：冻结状态下跳过的只有"模拟/输入/时钟/波次"那几步，
//       `④Q`/`④bF7`/`⑦相机`/`⑧F4`/`⑨FlushRemovals`/`⑨b压缩` **照跑**（清理端 + 调试出口）。
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

// 游戏 HUD 占的**行数** —— ⚠️ 它同时也是**世界视口的第一个屏幕行**（这两件事是同一个数）。
//
// ⚠️ 名字叫 `Row` 容易被读成"HUD 画在第几行"，但真正的语义是**行数**，三条公式一起看：
//     行 0             = 引擎顶栏（`Application` 画，`game` 不管）
//     行 1 .. kHudRow  = 游戏 HUD（`DrawHud` 画，每占一行就少一行可见世界）
//     行 kHudRow ..    = 世界视口（`viewH = 画布高 − kHudRow`；`DrawMap`/`DrawEntities` 都从这里开始）
//   ⇒ **第 `kHudRow` 行既是最后一行 HUD、又是世界的第一个逻辑行**：地图照画、HUD 后画会盖掉它。
//   ⚠️ 把 `kHudRow = 1` 代进上面三条，结果与 S6~S9 的旧行为**逐字一致** —— 所以 1→2 是一次
//      **泛化**而不是改语义（旧注释写的"HUD 占第 1 行"，说的正是 `kHudRow = 1` 这个特例）。
//
// ⚠️ **2026-10-10（S9 步骤 5）：1 → 2** —— HUD 变两行（行 1 位置类 / 行 2 状态类）。
//    代价 = **垂直可见世界再少 1 行**（22 → 21 行）；换来的是**一个栏目都不用砍**：
//    单行已经 78/80 列没有余量，而 `kills` 与 `dir` 两个新栏目必须显示（分叉 ③/④）。
//    ⚠️ 为什么**必须"占行"而不是"叠加"**：叠加（视口不变、多画一行盖住）会让玩家消失 ——
//      玩家最小 `y = 1.5`（tile 行 1）⇒ 屏幕行 = `1 − originRow + kHudRow`，相机夹到 `originRow = 0`
//      ⇒ 正好落在**第 3 行**；而两行 HUD 占的是第 1、2 行 ⇒ 安全。叠加时那一格会落到第 2 行、
//      正被第二行 HUD 盖住。人眼验收 R7 专门盯这条（它只有人眼能验）。
//
// ⚠️ 因此"可用视口高 = 画布高 − kHudRow"这条不能写错：写成整屏高会让底部多露一行，
//    并让 HUD 压错行（S6 坑 8）。
// ⚠️ 名字**保留** `kHudRow`（不改成 `kHudLines`）：它同时也是"世界视口第一行"，
//    改名反而丢掉这一层信息 —— 语义靠这段注释说准，代码里处处用它，别再造第二个常量。
constexpr int kHudRow = 2;

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

// ── S10 步骤 2：三个「状态键」──────────────────────────────────────────────
// ⚠️ 三个键**早就映射好了**（S2 起就在 `KeyCode.h` 里）⇒ **S10 对 `engine/` 的改动是 0 行**。
//    · `Enter` = 开始（只在 `Title` 有效）
//    · `Esc`   = 切换（`Playing ↔ Paused`，**以及** `GameOver → Title` —— 一个键三种去向，
//                去向由 `NextState` 决定，不是这里）
//    · `R`     = 重开（只在 `GameOver` 有效；⚠️ **世界重建是步骤 3 的 `StartNewRun()`**，
//                本步它只切状态 ⇒ "按 R 不重开"是已知的中间态）
// ⚠️ 键名在代码里**只有两处**（同 `kDumpKey`/`kFireKey` 那条纪律）：常量 + 相应状态画面里的提示字。
// ⚠️ 三个都用 `Pressed`（**边沿**）：用 `Held` 会每帧触发一次转移 ——
//    `R` 变成"每帧重开"、`Esc` 让 Paused/Playing 每帧来回翻（画面疯狂闪）。反向验证 R7 打这一条。
constexpr mini::platform::Key kStartKey   = mini::platform::Key::Enter;
constexpr mini::platform::Key kPauseKey   = mini::platform::Key::Escape;
constexpr mini::platform::Key kRestartKey = mini::platform::Key::R;

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

// S10 步骤 2：把若干行文本**居中**画出来（三个"状态专属画面"共用这一个）。
// ⚠️ 列不够时退到第 0 列；行/列越界由 `fb.Draw` 自己忽略（它契约就是"逐格越界忽略"）。
// ⚠️ 收 `std::initializer_list<const char*>` 而不是"数组 + 行数"：调用点直接写 `{...}`，
//    **不用数行数** —— 而"增删一行时忘了改行数"正是这类代码最典型的 bug。
void DrawCenteredBlock(mini::render::FrameBuffer& fb,
                       std::initializer_list<const char*> lines,
                       int topRow)
{
    int row = topRow;
    for (const char* line : lines) {
        const std::string_view text{line};
        int col = (fb.Width() - static_cast<int>(text.size())) / 2;
        if (col < 0) {
            col = 0;
        }
        fb.Draw(col, row, text);
        ++row;
    }
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

        // ── `Dead` 的后果（S8 加的；**S9 步骤 4 起它在正常路径上不可达**）────────────
        //  S8 时代它是"敌人死亡"的**唯一**后果：把实体停更（`World::Update` 因 `!active`
        //  跳过它）+ 不绘制（`DrawEntities` 跳过它 → 从屏幕上消失），但 `EnemyData` 留在
        //  `m_enemies` 里 ⇒ `F7` 面板仍然能显示 `state=Dead`（那是 S8 的取证手段）。
        //
        //  ⚠️ **2026-10-10（S9 步骤 4）：真删落地之后这里不可达了。**
        //     hp 只在 `ResolveBullets`（⑤d）里跨过 0，而它**当场**就把那条 `EnemyData` 从
        //     `m_enemies` 里 erase 掉、并把实体登记进 `pendingRemove`（⑨ 一起真删）
        //     ⇒ 本循环（⑤b）再也不会看到 `hp <= 0` 的条目。
        //
        //  ⚠️ **为什么选"留着"而不是删掉**（本人 2026-10-10 拍板，取法 A）：
        //     · `ai::Decide` 的 `Dead` 语义与那 18 条纯函数用例都还在 —— 删这里只是删掉
        //       `game` 侧的接线，不是删掉状态机的能力；
        //     · 它是**最后一道**防御：将来若出现第二条伤害路径（近战 / 陷阱 / S10 的怪），
        //       谁漏走了 ⑤d 的名单，这里至少还能把实体停掉，而不是留下一具会动的尸体。
        //     代价是"一个正常路径不可达的分支"，已按纪律登记 `TECH_DEBT`（触发条件：
        //     第二条伤害路径落地时复核它该删还是该接上）。
        //
        //  ⚠️ **下游别动**：`DrawEntities` 里那句 `if (!entity.active) continue;` 是 `active`
        //     唯一的**读取点**（玩家路径也靠它）。删了它，`active` 就彻底没人看。
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

    // 本帧"hp 跨过 0"的敌人（**存 id，不存下标** —— 理由见下面的名单写入处）。
    // 定长数组：最多 `kEnemyCount` 个跨过 0，不分配。
    std::array<mini::world::EntityId, kEnemyCount> pending{};
    std::size_t pendingCount = 0;

    // ⚠️ 遍历的是 `m_bullets`（**不是** `m_entities`）：这一点让 PLAN 点名的
    //    "遍历中 push_back 导致迭代器失效"这个坑**在架构上消失**。
    //    ⚠️ S9 步骤 4 起本函数**确实会 erase 了 —— 但只 erase `m_enemies`**（真删，在循环之后），
    //       与脚下的 `m_bullets` / `m_entities` 无关 ⇒ "不许在遍历中改动脚下的容器"这条仍然成立。
    //       `m_entities` 上唯一的改动点是 ⑤c 的 `Spawn`（`FireBullet`），那时还没有任何遍历。
    //       手里的 `bullet` 指针在本帧也不会失效（`m_entities` 只增不减，且 `Find` 返回的指针
    //       只在本次调用内有效 —— 这里每次循环都重新 `Find`）。
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
        //    ⚠️ S9 步骤 4 起"真删"发生在**整个循环之后** ⇒ 循环内就靠下面这个 `hp <= 0`
        //       过滤器让"已经死了的"从表里消失（两者是同一件事的两半，缺一不可）。
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

                // ★ 只记进"待真删"名单，**既不设 `active`、也不在这里 erase**。★
                //   · 不设 `active`：那是 `DriveEnemyAi` 的职责（S8 定的"怎么死只有一条路径"）；
                //   · 不在循环内 erase：`which[]` 给的是**下标**，而 `erase_if` 是 **swap-erase**
                //     ⇒ 下标会在同一帧内**静默指错**另一个敌人。所以推迟到循环之后，按 id 删。
                //   ⚠️ 存 id 而不是下标：`EntityId` 永不复用 ⇒ 最坏只是"找不到"，**不是"找错"**。
                if (pendingCount < pending.size()) {
                    pending[pendingCount] = target.id;
                    ++pendingCount;
                }
            }
        }
    }

    // ── 真删（S9 步骤 4，`TECH_DEBT` **C16** 到期）────────────────────────────
    // ⚠️ **必须在遍历 `m_bullets` 之外**：名单里的 id 是"这一帧跨过 0 的敌人"，
    //    而在循环内 erase `m_enemies` 会让**同一帧后面的子弹**的 `which[]` 下标失效。
    // ⚠️ **两边一起删**（S8 分叉 ① 的"两套真相"代价，`game/Enemy.h` 的文件头讲了这两种症状）：
    //    · 只 `MarkForRemoval(Entity)` → `EnemyData` 变成**悬空 id**（面板会印 `<id gone>`）；
    //    · 只 erase `EnemyData`        → 实体还在容器里、但**再没人驱动它**（原地当活靶子）。
    //    这两条各有反向验证（R5 / R5′），守卫是新用例
    //    `Game.KilledEnemyIsRemovedAndLeavesNoDanglingId`。
    // ⚠️ `MarkForRemoval` **只登记、不失效**（`World.h:73`）⇒ 这具实体在 ⑥ 会被**多积分一帧**，
    //    ⑨ `FlushRemovals` 才真删。因为 `OnRender` 排在 `OnUpdate` **之后**，所以它**当帧就不画了**
    //    ——屏幕上的 `'E'` 与 `F7` 面板的 `n=` 在同帧一起变。
    for (std::size_t i = 0; i < pendingCount; ++i) {
        m_world.MarkForRemoval(pending[i]);
        std::erase_if(m_enemies, [id = pending[i]](const EnemyData& enemy) { return enemy.id == id; });
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

    // ── ⓪ 造「状态事件」（S10 步骤 2）──────────────────────────────────────
    // ⚠️ **优先级：死亡 > 其他** —— 死亡那一帧的按键不算数（`S10_CHECKLIST` §7.1 理由 1）。
    // ⚠️ 除了那条必须的 `m_state == Playing`，这里**不再多判状态**：
    //    "哪个事件在当前状态下有效"是 `NextState` 的职责 —— 这里只回答"这一帧发生了什么"；
    //    两处都判就等于有了第二个真相。
    game::StateEvent event = game::StateEvent::None;
    if (m_playerHp <= 0 && m_state == GameState::Playing) {
        event = game::StateEvent::PlayerDied;
    } else if (input.Get(kStartKey) == KeyState::Pressed && m_state == GameState::Title) {
        event = game::StateEvent::StartPressed;
    } else if (input.Get(kPauseKey) == KeyState::Pressed) {
        event = game::StateEvent::PauseToggled;
    } else if (input.Get(kRestartKey) == KeyState::Pressed && m_state == GameState::GameOver) {
        event = game::StateEvent::RestartPressed;
    }

    // ① 转移 —— **全项目唯一的 `m_state` 写入点**（纯函数，非法边自动返回原状态）
    m_state = game::NextState(m_state, event);
    // ⚠️ **S10 步骤 3 会在这里插一句**：`StartPressed`/`RestartPressed` → `StartNewRun()`。
    //    本步先只切状态 ⇒ "按 `R` 不重开"（世界没被清空，下一帧 ⓪ 又会判出 `PlayerDied`
    //    ⇒ 状态在 `GameOver`/`Playing` 之间闪一帧）是**已知的中间态**，不是 bug
    //    （`S10_CHECKLIST` §5 步骤 2 与 §进度板 都记着）。

    // ② 本帧跑哪些系统（纯数据表；越界兜底 = 全 false）
    const game::StatePolicy policy = game::PolicyOf(m_state);

    // ③ 按策略分派：下面这些步骤在 `Title` / `Paused` / `GameOver` 下会被 `policy` 关掉。
    //    ⚠️ **没被包起来的步骤（④Q / ④bF7 / ⑦相机 / ⑧F4 / ⑨FlushRemovals / ⑨b压缩）在任何
    //       状态下都跑** —— 它们是"调试出口"与"清理端"（理由见 `game/GameState.h` 的策略表注释）。
    if (policy.readMoveKeys) {
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
    }

    // ④ Q 退出。Esc 刻意不退出 —— 它从 S10 起要用来关菜单（S2 的决策），
    //    进程级退出（Ctrl+C / 关窗口）由 Application 看 Input::QuitRequested()。
    //    ⚠️ **不在策略表里** ⇒ 冻结状态下也跑：它是"关掉程序"的唯一出口，暂停时更要能按。
    if (input.Get(Key::Q) == KeyState::Pressed) {
        m_quit = true;
    }

    // ④b `kAiPanelKey` 开/关敌人面板（S8 的临时面板，S11 会换成正式 Overlay）。
    //     只在 `Pressed` 翻转：用 `Held` 会每帧闪。
    //     ⚠️ **不在策略表里** ⇒ 冻结状态下也跑：暂停/结算时"看敌人状态"恰恰是最需要的。
    if (input.Get(kAiPanelKey) == KeyState::Pressed) {
        m_showAiPanel = !m_showAiPanel;
    }

    // ④c **已删除**（2026-10-10，S9 步骤 3）：S8 的临时杀敌键 `K` 被真入口（子弹）取代，
    //     `Key::K` 也从 `KeyCode.h` 删了（`TECH_DEBT` C15 到期）。
    //     ⚠️ 连带影响：S8 的用例 `Game.EnemyDiesAndDisappears` 原来按 `F7`+`K`，
    //        本步同步改写成"朝敌人开火把它打死"（不然它会因为键没了而变红）。

    // ⑤ Σdt（**只在 `Playing` 加**）：HUD 拿它自证 `pos ≈ v × sum dt`（10.2 的运行时验收）。
    //    ⚠️ 冻结状态下**一点也不加** —— 否则"暂停 1 分钟"会让 `sumdt` 凭空多 60 秒，
    //       那条自证就废了（S10_CHECKLIST 决策 D9）。⚠️ 恢复那一帧用的就是**本帧的正常 dt**，
    //       本项目**没有任何"积压 dt"通路**（`Time` 每帧独立给 dt）—— 别去引入一个。
    if (policy.advanceClock) {
        m_elapsed += ctx.deltaSeconds;
    }

    // ⑤b 敌人决策（S8）—— **必须在 ⑥ 之前**：敌人和玩家在**同一个循环里**被积分，
    //     所以两边的速度都得先写好（与 ③ 是同一条帧序约束）。
    //     反过来的现象：敌人慢一帧，或（若挪到 ⑥ 之后并写了速度）完全不产生位移。
    if (policy.simulate) {
        DriveEnemyAi(ctx);
    }

    // ⑤c 开火（S9）—— **必须在 ⑥ 之前**：子弹当帧就要飞出去（否则慢一帧）。
    //     ⚠️ `Pressed` 是**边沿触发**（按一下一发）：用 `IsDown` 会按住连射。
    //     ⚠️ 冻结状态下**连按键都不读**（`readFireKey`）⇒ 暂停时按 SPACE 不扣弹药、不出子弹。
    if (policy.readFireKey && (input.Get(kFireKey) == KeyState::Pressed)) {
        FireBullet();
    }

    // ⑤d 子弹的命中判定 + 标删（S9）—— **也在 ⑥ 之前**，用的是**预测线段**。
    //     理由写在 `Game.h` 的 `ResolveBullets` 注释里（挪到 ⑥ 之后就要存"上一帧位置"，
    //     而反推在被墙夹住时是错的）。
    if (policy.simulate) {
        ResolveBullets(ctx);
    }

    // ⑥ 积分（world 唯一的"物理"）。dt 已由 S3 clamp 过，这里**不再夹**（坑 #7）
    //    ⚠️ 子弹也在这里被积分 —— 它是普通 `Entity`，所以 S7 的碰撞/夹紧**白拿**。
    //    ⚠️ 这一个 `if` 就是"暂停时世界真的停了"的**全部实现**（`PolicyOf` 那条纯函数
    //       已经在编译期钉住"只有 Playing 会模拟"，这里只是执行它）。
    if (policy.simulate) {
        m_world.Update(ctx.deltaSeconds);
    }

    // ⑦ 相机跟随 —— **必须在 ⑥ 之后**（用本帧最新的玩家位置，S6 的帧序硬约束）。
    //    ⚠️ **不在策略表里** ⇒ 冻结状态下也跑：玩家没动，所以 `Follow` 的结果也不变
    //       （留着它反而是对的 —— 终端 resize 之后冻结中的画面也该跟着算对）。
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

    // ── HUD 两行（S9 步骤 5）───────────────────────────────────────────────
    // 行 1 = **位置类**（`pos` / `tile` / `sumdt` / `v`），行 2 = **状态类**（`hp` / `kills` / `dir` / 提示）。
    // ⚠️ 行号不是写死的 1/2，而是从 `kHudRow` 派生 —— 它同时是"HUD 占的行数"与"世界视口第一行"
    //    （见本文件顶部那三条公式）。`kHudRow = 2` 时：行 1 = `kHudRow − 1`、行 2 = `kHudRow`。
    // ⚠️ 下面这条 `static_assert` **不是空断言**：它挡的是"有人把 `kHudRow` 改回 1"
    //    —— 那时第二行 HUD 会画到**引擎顶栏（行 0）**上，把 FPS/Frame 顶掉。
    constexpr int kHudPosRow   = kHudRow - 1;   // 位置类那一行（= 1）
    constexpr int kHudStateRow = kHudRow;       // 状态类那一行（= 2）；**同时**是世界的第一个逻辑行
    static_assert(kHudRow >= 2,
                  "两行 HUD 需要 kHudRow >= 2；退回单行时请把 DrawHud 也一起改回单行");

    // 先把**两行都**铺满空格，再写字：否则 HUD 右边会露出地图的内容（地图照画、HUD 后画），
    // 看着像"画面坏了"（S3 的顶栏也是这样独占一行的）。
    // ⚠️ 只铺其中一行会让另一行**整行漏出世界** —— 这是本步最容易漏的一处。
    for (int row = kHudPosRow; row <= kHudStateRow; ++row) {
        for (int col = 0; col < fb.Width(); ++col) {
            fb.Set(col, row, ' ');
        }
    }

    // tile 坐标用**同一个 floor**（`ToIndex`）—— HUD 说的第几格必须和画面上的一致。
    //
    // `v` 这一格是**速度**（格/秒）：看起来是调试信息，其实是 10.2 那条自证的**必需项** ——
    // 屏幕上要能一眼核对 `pos ≈ v × sum dt`。速度现在可用 argv[2] 覆盖，所以必须显示出来。
    // ⚠️ 所以 `sumdt` 与 `v` **必须和 `pos` 留在同一行**（自证是"一行之内"的核对），
    //    分到第二行去也能看，但跨行对照更费眼 —— S9 分叉 ④ 的示例布局里把它们漏了，
    //    落地时按"一个栏目都不砍"补回行 1（见 `S9_CHECKLIST` 偏差 11）。
    //
    // 行 1（位置类）：**58 列**。
    char posText[96];
    std::snprintf(posText, sizeof(posText),
                  "pos %6.2f,%6.2f | tile %4d,%4d | sumdt %6.2f | v %4.1f",
                  static_cast<double>(px), static_cast<double>(py),
                  mini::world::TileMap::ToIndex(px),
                  mini::world::TileMap::ToIndex(py),
                  static_cast<double>(m_elapsed),
                  static_cast<double>(m_moveSpeed));
    fb.Draw(0, kHudPosRow, posText);

    // 行 2（状态类）：**44 列**（`kills` 与 `dir` 是 S9 新增的；`F7` 提示顺带解掉 S8 偏差 6）。
    // ⚠️ `dir` 的字形来自 `Bullet.h` 的 `FacingGlyph`（**定宽 2 字符**、全 ASCII，用例 B4 钉着）
    //    ⇒ HUD 后面的列不会左右跳。分叉 ③ 把它列为"**必要配套**"：朝向是"移动方向 + 停下沿用"，
    //    看不见朝向玩家就不知道该往哪打。
    // ⚠️ 提示行里的键名与代码里的常量是**同一个键的两个落点**（同 `kDumpKey`/`kFireKey` 的纪律）：
    //    换键时这一行要一起改。⚠️ 这里是 `F7` 的唯一提示落点（面板表头那句是另一处）。
    char stateText[96];
    std::snprintf(stateText, sizeof(stateText),
                  "hp %2d | kills %2d | dir %s | WASD SPC Q F4 F7",
                  m_playerHp, m_kills, game::FacingGlyph(m_facing));
    fb.Draw(0, kHudStateRow, stateText);
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
    // ⚠️ **2026-10-10（S9 步骤 3 收尾）：表头里的 `K=kill nearest` 删掉了。**
    //    `Key::K` 连同 `kKillKey` / `KillNearestEnemy` 已在步骤 3 一起删（`TECH_DEBT` C15 到期），
    //    但这一处**漏扫**了 —— 表头一直在教人去按一个已经不存在的键，属于**假提示**
    //    （`TECH_DEBT` C15 原话就是"删的时候三处一起：常量 / **面板表头里那个字母** / `KeyCode.h` 的映射"）。
    //    ⚠️ 教训：按 `Key::K` 这种**标识符**去 grep 是扫不到这里的（它写成了中文提示里的 `K=`）。
    //       接线/删键时除了 grep 标识符，还要 grep **提示文本里的那个字母**。
    std::snprintf(line, sizeof(line), "F7 AI  n=%d", static_cast<int>(m_enemies.size()));
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

// ------------------------------------------------------------
// S10 步骤 2：三个「状态专属画面」
//
// ⚠️ 分工：`OnRender` 按 `state` 决定**画什么**，这三个函数只管**自己那几行字**
//    —— 它们**不认识** `state`，也不知道谁调它们（同"纯函数要中立"那条纪律）。
// ⚠️ 全 ASCII：终端代码页是 936，非 ASCII 在这里会乱码（S8 的坑表记过）。
// ------------------------------------------------------------

void Game::DrawTitle(mini::render::FrameBuffer& fb) const
{
    // 开局提示：**只有这一屏不画世界**（`PLAN` 的"开局提示 → 按 Enter"，`S10` 步骤 2）。
    // ⚠️ 提示里的键名与 `kStartKey`/`kPauseKey` 是**同一个键的两个落点**（同 `kDumpKey` 的纪律）：
    //    换键时这里要一起改。⚠️ 这里也是 `kStartKey` 唯一的提示落点。
    // ⚠️ **刻意不写"5 个敌人"这类会随波次变的话** —— 波次在步骤 4 才落地，
    //    写死数字就会变成"假提示"（同 S9 偏差 10 的 `K=kill nearest` 那个教训）。
    const int top = fb.Height() / 2 - 2;
    DrawCenteredBlock(fb,
                      {"MiniEngine",
                       "",
                       "PRESS ENTER TO START",
                       "",
                       "WASD move   SPACE fire   ESC pause   Q quit"},
                      top > 0 ? top : 0);
}

void Game::DrawPausedBanner(mini::render::FrameBuffer& fb) const
{
    // 暂停横幅：**叠在世界与 HUD 之上**（世界照画 —— 只是冻住了，玩家要看得见自己站在哪）。
    const int top = fb.Height() / 2;
    DrawCenteredBlock(fb, {"-- PAUSED --", "ESC to resume"}, top > 0 ? top : 0);
}

void Game::DrawGameOver(mini::render::FrameBuffer& fb) const
{
    // 结算：叠在世界与 HUD 之上（要看得见自己死在哪、打了多少击杀 —— 击杀数在 HUD 第 2 行）。
    // ⚠️ **这里刻意只写 `ESC back to title`，不写 `R restart`**：`R` 的世界重建是**步骤 3** 的
    //    `StartNewRun()`；本步按 `R` 只会让状态闪一帧。**写了就是假提示**
    //    （S9 偏差 10 的教训：面板一直在教人按一个已经不存在的键）。
    //    → 步骤 3 落地 `StartNewRun` 之后，**这里要补上 `R restart`**。
    const int top = fb.Height() / 2;
    DrawCenteredBlock(fb, {"GAME OVER", "", "ESC back to title"}, top > 0 ? top : 0);
}

void Game::OnRender(mini::render::FrameBuffer& fb)
{
    // `Title` 是**纯文字画面**：世界与 HUD 都不画。
    // ⚠️ 不依赖 `DrawHud` 内部那句 `player == nullptr` 早退来"挡住 HUD"——
    //    那是"碰巧正确"（玩家的确还没出生），换成显式判断才不靠巧合。
    if (m_state != GameState::Title) {
        DrawMap(fb);
        DrawEntities(fb);
        // HUD 压在**视口第 0 行**上（= 相机原点那一行；同"顶栏压在最上面"一个道理）。
        // ⚠️ 顺序不能换：HUD 要盖掉地图，而 `@` 必须画在地图之后、HUD 之前。
        DrawHud(fb);

        // 面板在 HUD 之后。它**底部对齐**，与 HUD（第 1 行）以及敌人常在的第 3~7 行都不重叠。
        if (m_showAiPanel) {
            DrawAiPanel(fb);
        }
    }

    // 状态横幅**最后画**（压在 HUD / 面板之上）。
    // ⚠️ `switch` **不写 `default`**：以后加第五态，编译器会在这里提醒"你没处理新状态"
    //    （与 `StateName` 的穷尽性是同一个手法）。
    switch (m_state) {
    case GameState::Title:    DrawTitle(fb);        break;
    case GameState::Playing:                        break;   // 玩的时候不叠任何东西
    case GameState::Paused:   DrawPausedBanner(fb); break;
    case GameState::GameOver: DrawGameOver(fb);     break;
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

// 同上：真相是私有的 `m_state`（`OnUpdate` 的 ① 是**唯一**写入点）。
// ⚠️ 这个 getter 存在的理由与 `PlayerHp`/`Kills`/`Facing` 不同：**用例离不开它** ——
//    "当前是不是暂停了"除了它没有别的公开观察面（屏幕上看不出 `Playing` 和 `Paused` 的区别，
//    除非去解析横幅文本，而那正是 `ScreenText` 注释里说的"最脆的取证方式"）。
GameState Game::State() const noexcept
{
    return m_state;
}

bool Game::ShouldQuit() const
{
    return m_quit;
}

} // namespace game
