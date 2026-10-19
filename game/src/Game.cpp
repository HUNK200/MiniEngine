#include "game/Game.h"

#include "game/Bullet.h"           // S9：朝向 ↔ 向量、线段命中判定（纯函数）
#include "game/DebugOverlay.h"     // S11：overlay 的纯数据 + 绘制（`BuildOverlayLines` 可离线测）
#include "game/GameState.h"         // S10：四态 + 策略表 + 纯函数 NextState / PolicyOf
#include "mini/ai/EnemyBrain.h"   // S8：敌人每帧的"决策"（纯函数；只输出速度）
#include "mini/core/Logger.h"      // S11：调试快照改走它（`TECH_DEBT` C14 的正式落点）
#include "mini/math/Vec2.h"
#include "mini/platform/Input.h"
#include "mini/render/Color.h"
#include "mini/render/RayCaster.h"   // S12（第 4 笔）：3D 视角的纯函数（DDA 射线 / 墙高 / 明暗档）
#include "mini/world/Collision.h"   // 调试快照用：IsSolidAt / ToIndex / 点模型的判据
#include "mini/world/TileMap.h"

#include <algorithm>    // std::max（S8：HP 夹到 0）；std::erase_if（S9：子弹表自愈压缩）
#include <array>        // S9：定长暂存（`kWaveMax` 是编译期常量 ⇒ 判定时不分配）
#include <cmath>        // S12（第 4 笔）：3D 墙高取整用 std::ceil
#include <cstddef>      // std::size_t（S8：reserve 与巡逻表下标）
#include <cstdio>
#include <initializer_list>  // S10：DrawCenteredBlock 的行表
#include <span>         // S8：把巡逻路点交给 Decide；S9：把敌人位置交给 FirstEnemyOnSegment
#include <sstream>      // S11：调试快照先拼成一行字符串，再交给 Logger（不再自己写文件）
#include <string_view>  // S10：DrawCenteredBlock 要算文本长度（居中）

// ============================================================
// Game 实现（S5）—— 引擎与游戏分家之后的**唯一**游戏侧文件
//
// 这里做六件事，别的一件都不做：
//   1. **一局的初始状态**：铺地图、Spawn 玩家、铺敌人 —— 全是 `StartNewRun()`（S10 步骤 3）
//      ⚠️ 开局与重开**走同一个入口**；`OnStart()` 现在是空的（它曾经做这三件事）
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
//
//   ⚠️ **2026-10-16 的一次"差一帧"调查（结论：不必修；此处留档，别再走一遍）** ——
//     人眼报告："子弹撞到底部会贴着最后一排 `#` 滑动，那一排 `#` 还会闪一下，斜向时滑一两格才消失。"
//     查证结论是**没有 bug**，三条事实（都实测过）：
//       · 那排 `#` 是**柱子阵**的一行（`kPillarStep = 10` ⇒ 每 10 行长得一模一样
//         —— 这正是"`#` 的数量与位置和中间的完全一致"那条观察的来源）。
//         ⚠️ 该柱阵**已于同一天删除**（临时需求档 1：地图改成"默认全墙 + 挖家厅"，
//            见 `S12_CHECKLIST` §16）—— 这条记录写的是**当时**的地形，不是现在的；
//       · 子弹在飞过那一行的几帧里，**该行每帧都要重画**（`RowDiff` 只发变化行）⇒ 观感是"闪"
//         （实拍顶栏 `Rows 4/39` = 顶栏 + 2 行 HUD + 子弹那行，四个数正好对上）；
//       · 世界是 **200×200**（`Game::kWorldW/H`）、速 0.75 格/帧 ⇒ 从出生点朝南要飞
//         **≈262 帧**才到下边界；中途"消失"只是**飞出了视口**（相机只跟玩家、不跟子弹）。
//     ⇒ 期间曾按**错误诊断**加过一条兜底（"Update 之后再扫一遍子弹，实心就删"，编号 ⑥b），
//       **已撤掉**：`MoveAndCollide` 的分轴夹紧 + ⑤d 的线段采样已经保证"子弹进不了实心格"
//       （判定比位移**更保守**）⇒ 那条兜底防的失败**不可能发生**（B2 的"不写会怎样"纪律）。
//       真契约改用例钉住：`Game.BulletFliesThenDiesAtTheMapBorder`（逐帧 + 最终两条判据）。
//
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

// ⚠️ **2026-10-15（S11）删掉了这里的 `kDumpFileName`**：文件名归 `core::Logger`
//    （`Logger::kDefaultPath` = `miniengine_debug.log`）—— 同一个名字不留两个真相。
//    同一批退役的还有 `<fstream>` / `<filesystem>` 两个 include（见 `DumpSnapshot`）。

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
//    · `R`     = 重开（只在 `GameOver` 有效）。⚠️ 它经 ①b 调 `StartNewRun()` ——
//                **状态回 `Playing` 且世界重铺**（S10 步骤 3 起；步骤 2 时它只是"闪一帧"）
// ⚠️ 键名在代码里**只有两处**（同 `kDumpKey`/`kFireKey` 那条纪律）：常量 + 相应状态画面里的提示字。
// ⚠️ 三个都用 `Pressed`（**边沿**）：用 `Held` 会每帧触发一次转移 ——
//    `R` 变成"每帧重开"、`Esc` 让 Paused/Playing 每帧来回翻（画面疯狂闪）。反向验证 R7 打这一条。
constexpr mini::platform::Key kStartKey   = mini::platform::Key::Enter;
constexpr mini::platform::Key kPauseKey   = mini::platform::Key::Escape;
constexpr mini::platform::Key kRestartKey = mini::platform::Key::R;

// ── S12（分叉 B = 甲′）：两个「原地转身键」────────────────────────────────────
// ⚠️ 它们是**玩法键**（不是调试键）⇒ 与移动键**同一组门控**（`policy.readMoveKeys`）：
//    `ESC` 暂停时**不许**转身 —— 否则"暂停 = 世界完全冻住"这条（S11 人眼项 12）就破了。
// ⚠️ 用 `Pressed`（**边沿**）：按一下转 45°。用 `IsDown` 会变成"按住连续转" ——
//    那是**后续迭代**（连续转身），不是本阶段的东西（`S12_CHECKLIST` §3 末尾）。
// ⚠️ 字母键在代码里只有两处（同 `kDumpKey`/`kFireKey` 的纪律）：这两个常量 + `KeyCode.h` 的映射。
//    ⚠️ 它们**不进** HUD 提示行（提示行有溢出用例钉着）⇒ 键位进 README / Release 表。
constexpr mini::platform::Key kTurnCcwKey = mini::platform::Key::Z;   // 逆时针 45°
constexpr mini::platform::Key kTurnCwKey  = mini::platform::Key::C;   // 顺时针 45°

// ── S12（第 4 笔）：**切 2D/3D 视角**（主键 `V` + 备份 `F5`）──────────────────────────
// ⚠️ 它**不受玩法门控**：`ESC` 暂停时也能切（视图行为，同 `T`/`F2`/`F4`）—— 见 `OnUpdate` ④f。
// ⚠️ 用 `Pressed`（**边沿**）：按一下切一次。
// ⚠️ 键字母在代码里只有两处（同 `kDumpKey` 的纪律）：这两个常量 + `KeyCode.h` 的映射。
//    ⚠️ 它们**不进** HUD 提示行（提示行有溢出用例钉着）⇒ 键位进 README / Release 表。
constexpr mini::platform::Key kViewKey    = mini::platform::Key::V;    // 主键（字母键，不在热键靶场上）
constexpr mini::platform::Key kViewKeyAlt = mini::platform::Key::F5;   // 备份（`PLAN` 原稿的键）
// ⚠️ **2026-10-17 人眼实测（第 4 笔验收）**：`V` 通、**`F5` 本机收不到**（`VK_F5` 的 key-down
//    根本没进进程）。这**不是**映射错：枚举 / `map[0x74]` / 用例断言三段都完整。
//    ⇒ `F5` 只是"换台机器可能复活"的备份，**别指望它**；真要换备份键只改这一行 + `KeyCode.h`。
//    （`TECH_DEBT` C37 记着这类键的可达性"永远只能人眼验"；这已是第四个被抢的功能键。）

// ── S12（第 4 笔）：3D 的观感参数 + 两张小表 ────────────────────────────────────────
// ⚠️ `kFovTan` **没有理论值**（`S12_CHECKLIST` §13 的 T2）：与字体/字号/窗口都有关。
constexpr float kFovTan = 0.66f;   // 相机平面半长 ⇒ 约 66° FOV（与 `RayCaster.h` 的默认值同源）

// ── S12（第 5 笔）：纵横比从**常量**改成**按视口算** ─────────────────────────────────
// 第 4 笔用的是常量 `kCellAspect = 2.0f`（"看着差不多"）。第 5 笔推了一遍：
//
//   一格宽 × 一格高的墙面，在垂距 `perp` 处：
//     · 占**宽** = cols / (2·fovTan·perp) 列        （相机平面在深度 perp 上的总宽 = 2·fovTan·perp 格）
//     · 占**高** = viewH·aspect / perp 行           （`WallHeightRows`）
//   终端字符格**不是正方形**：1 行在物理上比 1 列高 `r` 倍（≈2）⇒ 物理高 = 行数 × r。
//   令物理宽 == 物理高 ⇒ cols/(2·fovTan·perp) = viewH·aspect/perp × r
//                   ⇒ **aspect = cols / (2·fovTan·viewH·r)**
//
//   ⚠️ **这个数随窗口变**（`cols` 与 `viewH` 都在里面）：100×30 ⇒ 100/(2×0.66×28×2) ≈ **1.353**；
//      80×25 ⇒ ≈ **1.318**；120×40 ⇒ ≈ **1.196**。第 4 笔写死的 2.0 会让墙比"方"高约 48%。
//   ⇒ 所以它**不该是常量**：一拉大窗口墙就变形（第 4 笔的代价，本笔修掉）。
//   ⚠️ `r = 2` 也只是**估的**（Consolas 之类在常见字号下约 1:2，换字体/字号要改）⇒ 它和下面的
//      `kAspectTaste` 都是**观感参数**，不是物理量：看着不对就调这两个数，别去改公式。
constexpr float kCellHeightPerWidth = 2.0f;   // 终端字符格 高:宽（估；观感参数）
constexpr float kAspectTaste        = 1.0f;   // 纯观感旋钮：>1 墙更高、<1 更矮（1.0 = 理论上"方"）

// 视口 → 纵横比（S12 第 5 笔）。**唯一**的换算点：别在列循环里就地再算一遍。
// ⚠️ 调用方已保证 `viewW > 0 && viewH > 0`；下面那句 `denom <= 0` 只是防御 ——
//    真除出 inf/NaN 会让整屏墙高变成 NaN，那种画面很难反推到"是除零"。
[[nodiscard]] float AspectForViewport(int viewW, int viewH) noexcept
{
    const float denom = 2.0f * kFovTan * static_cast<float>(viewH) * kCellHeightPerWidth;
    if (denom <= 0.0f) {
        return 1.0f;
    }
    return static_cast<float>(viewW) / denom * kAspectTaste;
}

// 明暗：**字形 + 颜色同档**（档数由 `mini::render::kShadeCount` 定死 = 4）。
// ⚠️ 字形本身自带"疏密"语义（`█` 最密 → `░` 最稀），比纯颜色更稳：
//    颜色在有的终端/主题下会被改（亮度重映射），字形不会。
// ⚠️ 最后一档**刻意与第 3 档同色**（`BrightBlack`）：`░` 已经比 `▒` 稀一档，再压颜色（`Black`）
//    会在黑底上**彻底看不见** ⇒ 走廊尽头会变成一个"空洞"。别"顺手修"成 `Black`。
// ⚠️ 若某台机器的 conhost 光栅字体不认 `█`（显示成 `?`）⇒ 换 Consolas / 新终端，
//    或把这张表整体换成 ASCII（`#` `%` `*` `.`）—— 只改这一处（坑 10）。
constexpr char32_t kShades[mini::render::kShadeCount] = {U'█', U'▓', U'▒', U'░'};
constexpr mini::render::Color kShadeColors[mini::render::kShadeCount] = {
    mini::render::Color::BrightWhite,   // 最近：最亮
    mini::render::Color::White,
    mini::render::Color::BrightBlack,
    mini::render::Color::BrightBlack,   // 最远：字形已最稀，颜色不再压（见上）
};

// ── S12（第 5 笔）：地板 / 天花板的行线（E 选**乙** = 静态、不随距离变）──────────────
// ⚠️ 用 **ASCII**（`-` / `.`）而不是 `═` / `·` 这一类：这一层只是"背景"，不值得再赌一次字体（坑 10）。
// ⚠️ 两者**同色**（`BrightBlack`），区分靠字形。想让它们更好分就只改这两个颜色常量
//    （候选 `White`）—— **逻辑里没有任何地方假设它们颜色不同**，别去动那两段循环。
// ⚠️ 不做"按行渐变"（那是 §3 末尾的后续迭代）：本笔只给**参照物**，用来判断鱼眼与胖瘦。
constexpr char32_t kCeilingGlyph = U'-';
constexpr char32_t kFloorGlyph   = U'.';
constexpr mini::render::Color kCeilingColor = mini::render::Color::BrightBlack;
constexpr mini::render::Color kFloorColor   = mini::render::Color::BrightBlack;

// ── `kQuitKey`：退出键（2026-10-13 补，`TECH_DEBT` **B6**）──────────────────────────
// ⚠️ **它是最后一个被补上常量的键**（2026-10-13，`TECH_DEBT` B6）：在那之前读点写的是
//    `mini::platform::Key` 的**枚举值本身** —— 它**没有一个可搜的名字**，于是那条
//    "换键时要一起改全部落点"的纪律对它**不成立**：
//    · 搜枚举值：只命中**读点 1 处**，下面两处提示**扫不到**；
//    · 搜字母：本文件出 **11 行、真落点只有 3 行**（其余是注释与 `ShouldQuit`）。
//    ⚠️ 这正是那一课：**注释里别再抄枚举值 / 字母**（抄了就又把噪声放回来了）。
// ⚠️ 键字母在代码里**只有三处**（同 `kDumpKey` 那条纪律）：这个常量 + `DrawHud` 行 2 的
//    提示段 + `DrawTitle` 的 `in game:` 横幅。换键 = 这三处一起改。
// ⚠️ 它治的是"**能查到**"，不是"能一起改"：提示文本是给玩家看的**字面量**、没法用常量渲染
//    ⇒ 那两处靠"旁边的注释指向 `kQuitKey`"来兜住（grep 常量名即可找全）。
constexpr mini::platform::Key kQuitKey = mini::platform::Key::Q;

// ── `kOverlayKey`：正式 Debug Overlay 的开关（S11，2026-10-15 拍板）──────────────────
// ⚠️ **它是字母键 `T`，不是 `F1`**：`F1` / `F3` 在本机被别的软件抢占（按下去我们的程序收不到），
//    `F3` 已经栽过两次（见 `kDumpKey` 与 `kAiPanelKey` 那两段注释的来历）。
//    ⇒ 教训：**F 行不是自由区**，新键优先选字母键。
// ⚠️ **键字母在代码里只有这一处** —— 与其它键不同，overlay 键**不进 HUD 提示行**
//    （§10-A 的附带决定：它是开发者工具，不是玩法键）。所以"换键"在这里真的是一行的事。
//    ⛔ 别顺手把它加进 `DrawHud` 的提示段：那一行的精确串与列宽被用例钉着（`TECH_DEBT` C24）。
constexpr mini::platform::Key kOverlayKey = mini::platform::Key::T;

// ── `kGeometryKey`：几何层开关（S11 步骤 5c）＝ `TECH_DEBT` **C13** 的到期处置 ─────────────
// ⚠️ 它**不是** `PLAN:549` 字面上的"碰撞盒"。S7 判过"C13 不做"，理由是
//    "1 格 1 字符下，把格子边界画出来 ≈ 没画"（同一格既画格子又画实体，看不出区别）。
//    5c 走的是**另一条通道 —— 底色**（字形一个字节都不动）：
//      · 实心格（墙 / 越界）→ 暗灰底（`DrawMap`）；
//      · 玩家占格 → 蓝底、敌人占格 → 红底（`DrawEntities`）。
//    ⇒ S7 那条理由对它**不成立**（那说的是字符通道），而"实体占的是哪一格"
//      （点模型正好一格）第一次看得见。
// ⚠️ **`F2` 在本机可用**（实测：`F1`/`F3` 被别的软件抢占，见 `kOverlayKey` 那段）；
//    `PLAN:549` 里留给它的正是这个位置，所以这次不用换键。
// ⚠️ 同 `kOverlayKey`：**不进 HUD 提示行**（开发者工具，不是玩法键）⇒ 键字母在本文件只有这一处。
constexpr mini::platform::Key kGeometryKey = mini::platform::Key::F2;

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

    // ── 2026-10-16（临时需求 · 档 1）：**默认全墙，只挖出能走的"家厅"** ────────────────
    // 为什么反过来做：以前是"默认全空、摆几根柱子" ⇒ 视口里 90% 是空白，看着不像地图
    // （本人原话："地图有点太难看了"）。现在是"**默认全墙**（= 岩石），只挖出真正能走的区域"
    // ⇒ 屏幕四周**任何位置**都有 `#`，而且窗口拉多大都一样（没挖到的地方就是墙）。
    // ⚠️ 为什么用循环、而不是用 `TileMap` 的构造初值参数（它本来就支持 `TileMap(w, h, fill)`）：
    //    `m_world` 是成员 ⇒ 地图**在 `OnStart` 之前就建好了**。4 万次 `Set` 只在开局跑一次，
    //    不值得为它去改 `World` 的接口（那是一片没必要的涟漪）。
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            map.Set(x, y, Tile::Wall);
        }
    }

    // **家厅**：`[kHallX, kHallX + kHallW) × [kHallY, kHallY + kHallH)` = `[1, 79) × [1, 10)`。
    // ⚠️ 这四个数**不是拍脑袋**，是四条硬约束算出来的（要动它先把这四条一起核一遍）：
    //   ① **出生点 `(2.5, 2.5)` 与 8 个巡逻路点（x 44.5~72.5、y 2.5~8.5）必须是空格** ——
    //      敌人出生在每条路线的第一个端点，埋进墙里就是"AI 原地抖"（`kPatrolPaths`，`Game.h:233`）；
    //   ② **向东 1 秒 = `kMoveSpeed`(20) 格**（用例 `MoveSpeedMatchesOneSecondOfWalking`）
    //      ⇒ x 至少要开到 `2.5 + 20 = 22.5`；
    //   ③ **斜向 0.45 s ≈ 6.4 格/轴**（同文件的斜向用例）⇒ y 至少要开到 `2.5 + 6.4 ≈ 8.9`；
    //   ④ **北墙夹紧要落在 `y = 1.5`**（用例 `WallStopsPlayerAtOneCell`）⇒ `y = 0` 是墙、`y = 1` 是空。
    // ⇒ `[1, 79) × [1, 10)` 一次满足全部四条；右下两侧特意留了余量（将来挖房间/门就在那一带）。
    constexpr int kHallX = 1;
    constexpr int kHallY = 1;
    constexpr int kHallW = 78;
    constexpr int kHallH = 9;
    for (int y = kHallY; y < kHallY + kHallH; ++y) {
        for (int x = kHallX; x < kHallX + kHallW; ++x) {
            map.Set(x, y, Tile::Empty);
        }
    }

    // 一个**不对称地标**（7×4 的一栋"楼"）：位置与尺寸**沿用旧地图**（S6 拿它当相机参照物）。
    // ⚠️ 为什么必须留一个"独一份"的方块：规则图案（柱阵/房间网格）平移一个周期就长得**一模一样**，
    //    光看它们分不清"画面动了"还是"没动、只是数字在跳" —— 这是 S6 的教训，别删。
    constexpr int kBlockX = 30;
    constexpr int kBlockY = 5;
    constexpr int kBlockW = 7;
    constexpr int kBlockH = 4;
    for (int y = kBlockY; y < kBlockY + kBlockH; ++y) {
        for (int x = kBlockX; x < kBlockX + kBlockW; ++x) {
            map.Set(x, y, Tile::Wall);
        }
    }

    // ⚠️ 旧的 **"规则柱阵"（每 10 格一根，`kPillarStep`）已删除**：它的 y 全部 ≥ 10 ⇒ 全在家厅之外
    //    ⇒ 那些格子现在本来就是墙（在"岩石"里），留着只是三段**看不见的循环**（B2 纪律的近亲：
    //    看不见的代码不该留）。它当年有两个用途：① 让空旷的地图看着有规律；② 给 S6 相机当参照物。
    //    ① 由家厅的墙接管；② 由上面那栋楼接管（`S6_CHECKLIST` 那条历史人眼项**不改** —— 那是当时的记录）。
    // ⚠️ 想做得更像"地图"（房间群 + 门）见 `S12_CHECKLIST` §16 与 `TECH_DEBT` **C40**。
}

// ------------------------------------------------------------
// S10 步骤 3：`StartNewRun` —— **一局的唯一入口**（开局与重开走同一条路）
//
// 重开清单（**顺序即依赖**；`S10_CHECKLIST` §4 的 6 步 + 第 5 步是步骤 3 补的，共 7 步）：
//   1. 遍历 `Entities()` 逐个 `MarkForRemoval`  ← ★ 只 push `m_pendingRemove`，**遍历中调是安全的**
//   2. **一次** `FlushRemovals()`               ← ★ 此刻实体表为空，而 `m_nextId` **保持不变**
//   3. `m_enemies.clear()` / `m_bullets.clear()` （两张表存的都是**上一局的 id**）
//   4. 标量归位：HP / kills / Σdt / 朝向
//   5. `BuildDemoMap()`                          ← 见下面的取舍说明
//   6. `Spawn` 玩家（拿到一个**新** id）
//   7. `BeginWave(1)`                            ← ✅ 步骤 4 落地（原来是 `SpawnEnemies()`）
//      ⚠️ `m_wave = 1` 与 `m_waveTimer = 0` 都由它负责 ⇒ §4 清单第 4 步里写的 `m_wave = 0`
//        **在这里被吸收了**（少一次写，也就少一个"两处都能改 `m_wave`"的口子）。
//
// ⚠️ 1 与 2 **必须连贯**（先全部登记、再一次 flush）：⛔ 在 `Entities()` 的循环里调
//    `FlushRemovals` 才会真的让手里的 span 失效 —— `MarkForRemoval` 之所以安全，正因为
//    它只碰另一个 vector（坑表 3）。
// ⚠️ 6 必须在 2 **之后**：`Spawn` 往刚清空的表里推；顺序反了会把新玩家一起删掉。
// ⚠️ **不重建 `m_world`** ⇒ `m_nextId` 单调递增 ⇒ 本局 id 与上一局**永不撞车**（分叉 ② 取丙）。
//    ⛔ 改成 `m_world = World{w, h}` 会把 `m_nextId` 重置回 1 ⇒ 坑表 1 那张对照表立刻成真。
// ⚠️ 不清 `m_state` / `m_showAiPanel` / `m_dumpCount` / `m_quit`：它们**不属于"一局"**（坑表 9）。
// ------------------------------------------------------------
void Game::StartNewRun()
{
    // 1) + 2) 清空实体表。⚠️ 这两步不许拆开，也不许换别的办法（理由见 `Game.h` 的注释）。
    for (const mini::world::Entity& entity : m_world.Entities()) {
        m_world.MarkForRemoval(entity.id);
    }
    m_world.FlushRemovals();

    // 3) 两张成员表 —— 里面全是**上一局的 id**，不清就是悬空 id。
    m_enemies.clear();
    m_bullets.clear();

    // 4) 标量归位。⚠️ **`m_wave` 与 `m_ammo` 刻意不在这里** —— 它们由第 7 步的
    //    `BeginWave(1)` 负责（那个函数是"一波的初始状态"的唯一入口，两个值都是"一波"的量）
    //    ⇒ 少一处写入点，就少一个"两处都能改它"的口子。见 `BeginWave` 的定义处。
    m_playerHp = kPlayerMaxHp;
    m_kills    = 0;
    m_elapsed  = 0.0f;
    m_facing   = Facing8::East;   // 与玩家的初始朝向一致（HUD 的 `dir ->` 开局恒为东）

    // 5) 地图**每次重铺一遍**。
    //    ⚠️ 这一条与 `S10_CHECKLIST` §4 末尾那句"m_map 不动"**不一致**，是本步的落地偏差：
    //       §4 的分叉 ② 条件 2 是**假设**"地图在整局之间不变"，靠的是"整局中没人改地图"；
    //       §0 前置事实 13 与 §5 步骤 3 则明确要求"把 `OnStart` 里的**铺地图**搬进来"。
    //       取后者：`BuildDemoMap` 是确定性的、且只重写 `m_map`（不动实体表），
    //       重铺的结果与上一次**逐格相同** ⇒ 把一条**假设**降级成**可验证的事实**，
    //       "将来地图可变（可破坏的墙 / 从文件加载关卡）"那个触发条件也提前关闭。
    BuildDemoMap();

    // 6) 玩家：出生在 (kSpawnX, kSpawnY) —— 格中心，所以 floor 之后是第 2 格。
    //    速度留空（{0,0}）：松开键就停，不需要额外"刹车"逻辑。
    mini::world::Entity player{};
    player.transform.pos      = mini::math::Vec2{kSpawnX, kSpawnY};
    player.velocity.perSecond = mini::math::Vec2{};
    player.symbol             = '@';
    m_player = m_world.Spawn(player);

    // 7) 刷第 1 波。⚠️ 放在**玩家之后**是刻意的 —— 渲染顺序 = `Entities()` 的顺序，
    //    玩家先画、敌人后画；两者位置刻意隔开（见 `kPatrolPaths` 的注释），不会互相盖掉。
    //    ⚠️ `BeginWave` 顺带把 `m_wave = 1` / `m_waveTimer = 0` 也定了（见函数头那句）。
    BeginWave(1);
}

void Game::OnStart()
{
    // ⚠️ **S10 步骤 3 起这里是空的**（原来做"铺地图 + Spawn 玩家 + 铺敌人"，
    //    那三件事全部搬进了 `StartNewRun()`）。
    //    理由：`PLAN` 的可运行效果是"**开局提示** → 按 Enter → 刷怪" ⇒ 第一次按 `Enter`
    //    才该铺世界；这里再铺一遍就是"两个入口"，而且那次铺的东西会被 `Enter` 触发的
    //    重开当场覆盖掉（纯白做）。
    // ⚠️ 保留空实现而**不是删掉虚函数**：`IGame::OnStart` 是引擎钩子，
    //    `Application::Run` 第 ① 步无条件调它（前置事实 7/13）。
    // ⛔ **别再往这里加东西** —— "一局的初始状态"只能有 `StartNewRun()` 一个入口。
}

// ------------------------------------------------------------
// 敌人（S8）+ 波次（S10 步骤 4）
//
// 五个函数的分工（**决策与接线分家** 的第 5 次实践）：
//   WaveEnemyCount   **决策**（`Game.h`，`static constexpr`）：第 n 波该刷几个
//   BeginWave        **接线**：记下 `m_wave`、清倒计时、按决策刷怪
//   SpawnWaveEnemies **接线**：铺 count 个：一个 `Entity`（`symbol = 'E'`）+ 一条 `EnemyData`
//   TickWave         **接线**：本波全灭 → 喘息倒计时 → 下一波
//   DriveEnemyAi     每帧：对每个敌人跑一次 `ai::Decide`，**只写速度**（不碰位置）
//   IsEnemyId        只给绘制上色用
// ------------------------------------------------------------

void Game::BeginWave(int wave)
{
    m_wave = wave;
    SpawnWaveEnemies(WaveEnemyCount(wave));

    // 本波**不在倒计时**（哨兵 0 = 未起倒计时；见 `m_waveTimer` 的注释）
    m_waveTimer = 0.0f;

    // ★ S10 步骤 5：**每波回满**（分叉 ④ 取"乙+丁"的那一半"乙"）。
    //    ⚠️ 放在 `BeginWave` 里而**不是** `TickWave` 里，是为了让**重开的那一波**也回满
    //       （分叉 ④ 的定稿原话："每波回满 ⇒ 重开那一波也算一波"）。
    //    ⚠️ 所以它和 `StartNewRun()` 是**互补**关系，别在两边都写：重开走 `BeginWave(1)`
    //       ⇒ 这一句已经覆盖了"重开时弹药回满"。
    m_ammo = kStartAmmo;
}

void Game::TickWave(float dt)
{
    // ── 判据：**"这一波清完了吗"** ──────────────────────────────────────────
    // ⛔ **不能只有 `m_enemies.empty()`**（坑表 4）：它在"还没刷第一波"时也为真
    //    ⇒ 会立刻再刷一次（现象 = "第一波刷两遍"，很难查）。
    //    这里用**两个条件的合取**：已经开过波（`m_wave >= 1`）**且** 场上没敌人了。
    //    ⚠️ 写成一个 `if` 而不是"早退的防御分支"：这样就没有"实现了但不可达"的分支
    //       （`TECH_DEBT` C22 那类东西要尽量不造）。
    if (m_wave < 1 || !m_enemies.empty()) {
        m_waveTimer = 0.0f;   // 本波还没清完（或还没开始）⇒ 倒计时清零（= 未起倒计时）
        return;
    }

    // ── 刚清完：起倒计时（`m_waveTimer == 0` 既是"未起"也是"上一步刚清零"）─────
    if (m_waveTimer == 0.0f) {
        m_waveTimer = kWaveBreakSeconds;
        return;
    }

    // ── 倒计时进行中 ──────────────────────────────────────────────────────
    m_waveTimer -= dt;
    if (m_waveTimer <= 0.0f) {
        BeginWave(m_wave + 1);   // ⚠️ 这里会把 `m_waveTimer` 清 0（见 `BeginWave`）
    }
}

void Game::SpawnWaveEnemies(int count)
{
    m_enemies.reserve(static_cast<std::size_t>(count));

    for (int i = 0; i < count; ++i) {
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
        // ⚠️ `patrolPath` 只由 `SpawnWaveEnemies` 写成 `0 .. count-1`（`count <= kWaveMax`），
        //    所以这里不会越界；两个常量都显式转 size_t，免得 /W4 的 signed/unsigned 提示。
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
    // ── ★ S10 步骤 5：弹药判定必须在**最前面** ────────────────────────────────
    //    理由（分叉 ④ 的"生成端"）：判定放在 `Spawn` 之后的话，"发出去再发现没弹药"
    //    就要么留下一个白打的子弹、要么写负数弹药。放在最前面 ⇒ 打光了就是**什么都不发生**
    //    （屏幕上不出 `*`，玩家立刻从 `ammo` 那栏看出来）。
    //    ⚠️ 判据是 `<= 0` 而不是 `== 0`：不变量上它不会为负，但"≤"让这条不变量**不必被信任**
    //      （真为负也只是"开不了火"，不会变成负得更多）。
    if (m_ammo <= 0) {
        return;
    }

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

    // ⚠️ **扣减放在最后**（`Spawn` 成功之后）：`World::Spawn` 刻意不标 `noexcept`，
    //    万一它抛了，弹药就不该白扣（与"先确认 Spawn 成功再记账"同一条道理，§7.4）。
    --m_ammo;
}

void Game::ResolveBullets(const mini::core::FrameContext& ctx)
{
    if (m_bullets.empty()) {
        return;
    }

    // 本帧"hp 跨过 0"的敌人（**存 id，不存下标** —— 理由见下面的名单写入处）。
    // 定长数组：最多 `kWaveMax` 个跨过 0，不分配。
    // ⚠️ **2026-10-12（S10 步骤 4）从 `kEnemyCount`(5) 改成 `kWaveMax`(8)** —— 这是本步
    //    最险的一处涟漪：封顶值从 5 涨到 8 之后，5 个元素的栈上数组会被**写越界**
    //    （8 个敌人同帧跨过 0 是完全可能的：一发子弹只打一个，但可以同时有多发命中）。
    //    上界由 **D13** 保证：只有全灭才刷下一波 ⇒ 场上敌人 ≤ `kWaveMax`。
    std::array<mini::world::EntityId, kWaveMax> pending{};
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

        // ① 每次都重新收一遍"活着且 id 有效的敌人位置"（≤ `kWaveMax` 个元素，栈上定长数组，不分配）。
        //    ⚠️ **每发子弹重新收**是刻意的：这一帧前面的子弹可能已经打死了某个敌人，
        //       那样它就该从表里消失 —— 否则（a）`kills` 会被同一具尸体重复计数，
        //       （b）后面的子弹会被一具看不见的尸体挡住。
        //    ⚠️ S9 步骤 4 起"真删"发生在**整个循环之后** ⇒ 循环内就靠下面这个 `hp <= 0`
        //       过滤器让"已经死了的"从表里消失（两者是同一件事的两半，缺一不可）。
        //    ⚠️ S10 步骤 4：同样从 `kEnemyCount`(5) 改成 `kWaveMax`(8)（同上，越界风险）。
        std::array<mini::math::Vec2, kWaveMax> positions{};
        std::array<std::size_t, kWaveMax>      which{};   // 第 i 项对应 m_enemies 的哪一个
        std::size_t aliveCount = 0;
        for (std::size_t i = 0; i < m_enemies.size() && aliveCount < kWaveMax; ++i) {
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

                // ── ★ S10 步骤 5：弹药返还（分叉 ④ 的"返还端"）──────────────────────
                //  ⚠️ **返还点必须与 `++m_kills` 同一处**（"跨过 0 那一次"），
                //     ⛔ **不是**"打中一次 +1" —— 否则一个敌人被打 3 次就返还 3 发、
                //     净耗变成 0，弹药这件事等于不存在（`AmmoRefundHappensOnlyOnKill` 钉住它）。
                //  ⚠️ **`min(..., kMaxAmmo)` 不是可选项**：`kMaxAmmo == kStartAmmo == 18`，
                //     而"每击杀 +1"在一波内会累积 ⇒ 没有夹取就能**慢慢磨着囤弹**
                //     （挑软柿子的波次刷满），把后面的难度设计整个废掉。
                m_ammo = std::min(m_ammo + kAmmoPerKill, kMaxAmmo);

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

    // ── ★ S11：拷一份引擎帧数据的**只读快照**（分叉 F 甲；后面 `OnRender` 要靠它）───────
    // 为什么需要它：`IGame::OnRender` 只收 `(FrameBuffer&)`，**拿不到 `FrameContext`**
    //   （`engine/include/mini/core/Application.h:99`）。而 overlay 要显示这四项 ⇒ 只能在这儿留一份。
    // ⚠️ **代价是有意的**：overlay 的引擎侧数字**慢一帧** —— 顶栏的 `Rows` 也是这个毛病
    //   （`S11_CHECKLIST` §16.3 ② 有实测）。观测工具不在乎 16ms 的延迟，而在意"改接口"的代价。
    // ⚠️ 放在 OnUpdate **最前面**：它在任何状态下都执行（`Title` / `Paused` / `GameOver` 也要有数
    //   可看），而且必须在 `OnRender` 之前。
    m_lastFrame = FrameSnapshot{ctx.fps,         ctx.frameMs,     ctx.maxFrameMs,
                                ctx.workMs,      ctx.screenWidth, ctx.screenHeight};

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

    // ①b 开始 / 重开 ⇒ **世界重建**（S10 步骤 3）。`Title` 的 `Enter` 与 `GameOver` 的 `R`
    //     走**同一条路**（`S10_CHECKLIST` §5 步骤 3 的要求："开局与重开一个入口"）。
    //     ⚠️ 位置必须在 **①之后、②③之前**：
    //       · 在 ① 之后 —— 转移决定了"这一帧要不要重开"；
    //       · 在 ②③ 之前 —— 否则本帧的"读方向键 / 写速度"会作用在**已经被删掉的旧玩家**上
    //         （`Find(m_player)` 返回 nullptr ⇒ 白写），而且清空/重铺必须发生在**任何别的遍历之前**
    //         （它是 `StartNewRun` 唯一被允许的时机：帧首，坑表 3）。
    if (event == game::StateEvent::StartPressed || event == game::StateEvent::RestartPressed) {
        StartNewRun();
    }

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

        // ③c 原地转身（S12，分叉 B = 甲′）：**只改朝向，不动位置**。
        //     ⚠️ **方向约定（2026-10-16 实测纠正）**：`Facing8` 的枚举顺序在**屏幕上**是
        //        **逆时针**（东 = 3 点钟 → 东北 = 1:30 ⇒ 往左转）⇒ `StepFacing(+1)` = **逆时针**、
        //        `-1` = **顺时针**。所以 `Z`（承诺逆时针）传 `+1`、`C`（承诺顺时针）传 `-1`。
        //        ⚠️ 这两行**写反过一次**（第一版把 `Z` 传 `-1`）：`Game.TurnKeysRotateFacingInPlace`
        //           把它抓了出来 —— 按一次 `Z` 之后朝向是**西北**（顺时针）而不是西南。
        //     ⚠️ 与 ③b 的顺序有意义：先按移动方向定朝向、**再**应用转身 ⇒ 同一帧里
        //        按 `D`+`C` 是"朝东、再顺时针转 45°"= 东南（可预期，见用例）。
        //     ⚠️ 两键同一帧都按下 ⇒ 净效果 0（下面先逆后顺两条独立 `if`）。
        //        这是**有意**的：不做"抵消检测"，两条 `if` 的语义各自独立、好解释。
        //     ⚠️ 它**在门控里** ⇒ `Title`/`Paused`/`GameOver` 下按 `Z`/`C` 什么都不发生。
        if (input.Get(kTurnCcwKey) == KeyState::Pressed) {
            m_facing = game::StepFacing(m_facing, +1);   // 逆时针（+1 = 屏幕上逆时针）
        }
        if (input.Get(kTurnCwKey) == KeyState::Pressed) {
            m_facing = game::StepFacing(m_facing, -1);   // 顺时针
        }
    }

    // ④f 切 2D/3D 视角（S12 第 4 笔）—— ⚠️ **不在门控里**：暂停时也能切。
    //     理由：它只改"怎么看"（`m_viewMode`），不动世界、不动帧序、不参与任何游戏逻辑
    //     ⇒ 与 `T`/`F2`/`F4` 同一类（视图/调试出口）。反向对照：上面 ③c 的转身是**玩法**键
    //     ⇒ 受门控（暂停时不许转）。
    //     ⚠️ 用 `Pressed`（边沿）：按一下切一次；用 `IsDown` 会每帧来回翻（画面疯狂闪）。
    //     ⚠️ 两个键都查（主键 `V` + 备份 `F5`）：键可达性只能人眼验（`TECH_DEBT` C37），
    //        有备份才敢在"某个键被抢"时一处换掉。
    if (input.Get(kViewKey) == KeyState::Pressed || input.Get(kViewKeyAlt) == KeyState::Pressed) {
        m_viewMode = (m_viewMode == ViewMode::Mode2D) ? ViewMode::Mode3D : ViewMode::Mode2D;
    }

    // ④ `kQuitKey` 退出。Esc 刻意不退出 —— 它从 S10 起要用来关菜单（S2 的决策），
    //    进程级退出（Ctrl+C / 关窗口）由 Application 看 Input::QuitRequested()。
    //    ⚠️ **不在策略表里** ⇒ 冻结状态下也跑：它是"关掉程序"的唯一出口，暂停时更要能按。
    //    ⚠️ 这里是 `kQuitKey` 的**读点**；它的两处**提示落点**在 `DrawHud` 行 2 与 `DrawTitle`，
    //       两边都留了"与 `kQuitKey` 同步"的注释（`TECH_DEBT` B6 —— 在那之前这里写的是枚举值本身）。
    if (input.Get(kQuitKey) == KeyState::Pressed) {
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

    // ④d `kOverlayKey` 开/关正式 Debug Overlay（S11）。
    //     只在 `Pressed` 翻转（同 ④b：用 `Held` 会每帧闪）。
    //     ⚠️ **不在策略表里** ⇒ 冻结状态下也跑：暂停时看 profiler 表恰恰最有意义
    //        （画面冻住了，表也冻住了，正好读数）。
    //     ⚠️ 它**只影响画不画**：不动世界、不动帧序、不参与任何游戏逻辑。
    if (input.Get(kOverlayKey) == KeyState::Pressed) {
        m_showOverlay = !m_showOverlay;
    }

    // ④e `kGeometryKey` 开/关几何层（S11 步骤 5c；`TECH_DEBT` C13）。
    //     同 ④d：只在 `Pressed` 翻转、**不在策略表里**（冻结状态下也该能看"哪一格是实心"）。
    //     ⚠️ 它**只影响底色**：不动字形、不动世界、不动帧序 ——
    //        所以"数屏幕上 `'E'`"那些判据**一个都不受影响**（那是 C31 的安全边界）。
    if (input.Get(kGeometryKey) == KeyState::Pressed) {
        m_showGeometry = !m_showGeometry;
    }

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
        MINI_PROFILE_SCOPE("Game.AI");   // S11：段名只在**真的跑**的时候才登记
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
        MINI_PROFILE_SCOPE("Game.Bullets");   // S11：同 ⑤b —— 只在真的跑的时候才登记
        ResolveBullets(ctx);
    }

    // ⑥ 积分（world 唯一的"物理"）。dt 已由 S3 clamp 过，这里**不再夹**（坑 #7）
    //    ⚠️ 子弹也在这里被积分 —— 它是普通 `Entity`，所以 S7 的碰撞/夹紧**白拿**。
    //    ⚠️ 这一个 `if` 就是"暂停时世界真的停了"的**全部实现**（`PolicyOf` 那条纯函数
    //       已经在编译期钉住"只有 Playing 会模拟"，这里只是执行它）。
    if (policy.simulate) {
        m_world.Update(ctx.deltaSeconds);
    }

    // ⑤x 波次推进（S10 步骤 4）—— **必须在 ⑥ 之后**：判据是"场上还有没有敌人"，
    //     而这一帧被 bullet 打死的敌人在 ⑤d 就已经从 `m_enemies` 里 erase 了
    //     ⇒ 放在这里"当帧"就能判出全灭（放在 ⑤d 之前会晚一帧）。
    //     ⚠️ 也必须在 ⑤b/⑤d/⑥ **之后**：否则 `BeginWave` 会在本帧刚 `Spawn` 出来的
    //        敌人身上又跑一遍 AI/积分（多推一帧的位移）。
    //     ⚠️ 只在 `advanceWave` 时调（`Title`/`Paused`/`GameOver` 下不推进波次 —— 冻结的一部分）。
    if (policy.advanceWave) {
        TickWave(ctx.deltaSeconds);
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
    //    ⚠️ **只读**：往 Logger 写一行 + 递增计数器，不碰世界 → 帧序不受影响。
    //    ⚠️ 只在 `Pressed` 时写（不是 `Held`）—— 否则按住会每帧刷盘。
    //    ⚠️ S11 起 sink 是 `core::Logger`（以前是自开 `ofstream`）：**未装 Logger 时静默丢弃**
    //       ⇒ 单测里按 `F4` 也是安全的（`tests/test_game.cpp` 的 `DumpSnapshotGoesThroughLogger`）。
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

    // S11 几何层（键 = `kGeometryKey`）：**只加底色、不碰字形** ——
    //   实心格铺一层暗灰底（`#` 之外多一个通道），于是"哪一片是墙 / 哪里是地图外"一眼可见。
    // ⚠️ 底色**只给实心格**：空格保持默认底色（否则整屏都是灰的，反而看不出边界在哪）。
    // ⚠️ 它在循环外算一次（`Default` 是"不铺"的哨兵，见 `Set` 的默认参数）——
    //    图层关着时**一个字节的绘制差异都没有**（这是默认关的意义）。
    const mini::render::Color solidBg = m_showGeometry ? mini::render::Color::BrightBlack
                                                       : mini::render::Color::Default;

    for (int row = kHudRow; row < h; ++row) {
        const int worldRow = originRow + (row - kHudRow);
        for (int col = 0; col < w; ++col) {
            if (map.At(originCol + col, worldRow) == mini::world::Tile::Wall) {
                // ⚠️ 越界也走这一支（`TileMap::At` 把地图外当实心 = S7 的"越界 = 墙"）
                //    ⇒ 几何层顺带把那条规则**画出来**了。
                fb.Set(col, row, '#', mini::render::Color::BrightBlack, solidBg);
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
        // ⚠️ 2026-10-15（S11 5c）：把两个判断提成 `isPlayer` / `isEnemy` 两个布尔量 ——
        //    几何层要用**同一套身份**决定底色，写成两份 `if` 链迟早会不一致。
        const bool isPlayer = (entity.id == m_player);
        const bool isEnemy  = !isPlayer && IsEnemyId(entity.id);

        mini::render::Color color = mini::render::Color::BrightWhite;
        if (isPlayer) {
            color = mini::render::Color::BrightYellow;
        } else if (isEnemy) {
            color = mini::render::Color::BrightRed;
        }

        // ── S11 几何层（键 = `kGeometryKey`）：把**实体占的那一格**用底色标出来 ──────────
        // 点模型下实体**正好占一格**（`ToIndex(pos)`），而此前"它占哪一格"是**看不见**的
        //   （`E` 画在空地上，读者不知道格界在哪 —— S7 判 C13 不做时说的就是这件事）。
        // ⚠️ 底色用**非亮版同色系**（玩家蓝 / 敌人红）：与前景色同源 ⇒
        //    "这是谁"与"它占哪一格"两条信息由同一个色相表达，不会互相打架。
        // ⚠️ 子弹（其余）**不铺底**：它小、快、数量多，铺底只会闪（也不提供新信息 ——
        //    子弹的位置本来就一眼可见）。⇒ 这一档的底色恒为默认值。
        mini::render::Color bg = mini::render::Color::Default;
        if (m_showGeometry) {
            if (isPlayer) {
                bg = mini::render::Color::Blue;
            } else if (isEnemy) {
                bg = mini::render::Color::Red;
            }
        }

        fb.Set(col, row, entity.symbol, color, bg);
    }
}

// ------------------------------------------------------------
// S12（第 4 笔）：3D 视角（伪 3D 射线投射）
// ------------------------------------------------------------

Game::ViewMode Game::CurrentView() const noexcept
{
    return m_viewMode;
}

mini::math::Vec2 Game::CameraDir() const noexcept
{
    // ⚠️ **唯一**把"朝向"变成单位向量的地方（分叉 C 的接缝）：将来做连续转身只改这一个函数体。
    //    `FacingToVec2` 返回的**都是单位向量**（斜向用 1/√2 拼出来，长度恰好 1）——
    //    这是 `RayCaster` 那条 "rayDir·dir == 1 ⇒ perp = t" 推导的**前提**：
    //    在这里乘任何系数（比如"走得更快"）都会**悄悄**把 3D 的距离算歪。
    return game::FacingToVec2(m_facing);
}

void Game::DrawRaycastWorld(mini::render::FrameBuffer& fb) const
{
    const mini::world::Entity* player = m_world.Find(m_player);
    if (player == nullptr) {
        return;   // `OnStart` 之前（同 `DrawHud` 的口径：宁可空着，也不画假的）
    }

    // 视口与地平线：**沿用 2D 那套口径** —— 世界从行 `kHudRow` 开始，HUD 之后画在上面。
    // ⚠️ 于是 3D 的"可用视口"是 `fb.Height() - kHudRow` 行（100×30 时 = **28**），**不是 30**
    //    ⇒ 验收口径要按这个说（坑 16），也别按整屏去算地平线。
    const int viewH = fb.Height() - kHudRow;
    if (viewH <= 0 || fb.Width() <= 0) {
        return;
    }
    const int horizon = kHudRow + viewH / 2;

    // ── ① 铺底（S12 第 5 笔）：地平线以上整行 `-`、以下整行 `.` ──────────────────
    // ⚠️ **必须在列循环之前**：墙是**后画**的，"墙挡住地板/天花板"靠的就是后画覆盖。
    //    反过来（先墙后铺底）会把墙整行抹成**条纹** —— 反向验证 R6 打的正是这一场。
    // ⚠️ 行 `horizon` 归**地板**（半开区间 `[horizon, h)`）；`viewH` 是奇数时上面比下面
    //    **少一行**（`viewH / 2` 是 floor 除法）—— 这是取整的自然结果，别当 bug。
    // ⚠️ 行 0/1 绝不碰（顶栏 / HUD）；行 `kHudRow` 会被铺底、随后被 `DrawHud` 盖掉（同 2D 口径）。
    // ⚠️ 每帧 `cols × viewH` 次 `Set`（100×30 时 = 2800 次）—— 别为它引第二条绘制路径
    //    （比如先拼一个整行字符串再 `Draw`）：那会多一个"行线怎么生成"的真相。
    for (int row = kHudRow; row < horizon; ++row) {
        for (int col = 0; col < fb.Width(); ++col) {
            fb.Set(col, row, kCeilingGlyph, kCeilingColor);
        }
    }
    for (int row = horizon; row < fb.Height(); ++row) {
        for (int col = 0; col < fb.Width(); ++col) {
            fb.Set(col, row, kFloorGlyph, kFloorColor);
        }
    }

    // 纵横比：**按视口算**（S12 第 5 笔）—— 常量会让窗口一变就把墙拉变形（见 `AspectForViewport`）。
    const float aspect = AspectForViewport(fb.Width(), viewH);

    const mini::render::RayCamera cam{player->transform.pos, CameraDir(), kFovTan};

    for (int col = 0; col < fb.Width(); ++col) {
        const mini::render::RayHit hit =
            mini::render::CastColumn(m_world.Map(), cam, col, fb.Width());

        if (!hit.hit) {
            continue;   // 病态输入（一路没撞到）：这一列只剩铺底的地板/天花板（第 5 笔起不再留白）
        }

        // ⚠️ `float → int` 的取整**只在这里做一次**（坑 17）：半高用 floor、总高用 ceil
        //    ⇒ 墙不会因为浮点抖动反复差一行；而"远墙至少 1 行"由 `WallHeightRows` 兜底。
        const float lineH  = mini::render::WallHeightRows(hit.perp, viewH, aspect);
        const int   halfH  = static_cast<int>(lineH * 0.5f);      // 正数 ⇒ 截断即 floor
        const int   totalH = static_cast<int>(std::ceil(lineH));
        const int   top    = horizon - halfH;
        const int   bottom = top + totalH;

        const int shade = mini::render::ShadeIndex(hit.perp);

        // 裁剪到"可用视口"（行 `kHudRow .. fb.Height()-1`）：贴脸时 `totalH` 会远超视口。
        // ⚠️ `fb.Set` 本来就忽略越界，但**先夹一次**更省（省掉几万次白调用），
        //    而且"这一列到底画了哪几行"在这段代码里变成显式的（好读、好断）。
        const int y0 = (top > kHudRow) ? top : kHudRow;
        const int y1 = (bottom < fb.Height()) ? bottom : fb.Height();
        for (int row = y0; row < y1; ++row) {
            fb.Set(col, row, kShades[shade], kShadeColors[shade]);
        }
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

    // 行 2（状态类）：**64 列**（S10 步骤 6 **定稿并实测**；`S10_CHECKLIST` 前置事实 9 预测 ~67 ⇒ 差 3，见下面那条约束）。
    //    逐段：`hp 10` 5 + ` | ammo 18` 10 + ` | kills  0` 11 + ` | wave  1` 10 + ` | dir ->` 9
    //          + ` | WASD SPC Q F4 F7` 19 = **64**。
    //    ⚠️ 距 80 还有 16 列余量 —— `kScreenW = 80` 是终端的一屏，超了会**静默截断**
    //       （`fb.Draw` 逐格裁掉），屏幕上看不出错 ⇒ 宁可贵在"没写完"上。
    // ⚠️ `dir` 的字形来自 `Bullet.h` 的 `FacingGlyph`（**定宽 2 字符**、全 ASCII，用例 B4 钉着）
    //    ⇒ HUD 后面的列不会左右跳。分叉 ③ 把它列为"**必要配套**"：朝向是"移动方向 + 停下沿用"，
    //    看不见朝向玩家就不知道该往哪打。
    // ⚠️ 所有数字栏都用 `%2d`（而不是 `%d`）：**定宽才不会让后面的列左右跳**
    //    （`HudFitsInTwoLinesWithoutOverflow` 有一条用例专门核"数值跨过位数边界时各栏偏移不变"）。
    //
    // ★★ **HUD 两行里禁止出现 `@` / `E` / `*`**（这三条是玩家 / 敌人 / 子弹的**字形**）：
    //    屏幕是**唯一**的观察面，用例靠 `FindChar('@')`、`CountChar('E')`、`CountChar('*')` 数它。
    //    ⚠️ **2026-10-12（S10 步骤 6）实测踩过这个坑**：给提示段加 `ESC` 之后，"`E`" 多了一个
    //    ⇒ `CountChar(fb,'E')` 数出 **4** 而面板真值是 **3** ⇒ `RestartThreeTimesDoesNotAccumulate`
    //    红（`drawn=4 rhs=3`）。`HudFitsInTwoLinesWithoutOverflow` 里那条"不含 `@`/`E`/`*`"
    //    的断言**当场就抓住了**它（那条断言存在的唯一理由就是这个）。
    //    ⇒ 结论：**键名里带 `E` 的（`ESC` / `ENTER`）一个字都不能写进 HUD**，只能放横幅
    //      （横幅画在世界行上，`CollectEnemies` 从 `kTestFirstWorldRow` 起扫、只做"前后对比"，
    //       不会拿它跟面板真值比 ⇒ 安全）。
    //
    // ★ **提示段 = 在 `Playing` 下真的有用、且不含 `E` 的键**（两条约束的交集）：
    //    · `WASD` 移动 / `SPC` 开火 / `Q` 退出 / `F4` 快照 / `F7` 面板 —— 五个都在 `Playing` 有效 ✓
    //    · ⛔ **不列 `ENTER` / `R`**：那两个键在 `Playing` 下是 **no-op**（`NextState` 的 ⓪b 只在
    //      `Title` / `GameOver` 收它们）⇒ 列在这里就是**假提示**（它的**真落点**是横幅：
    //      `ENTER` 在 `DrawTitle` 的 "PRESS ENTER TO START"、`R` 在 `DrawGameOver` 的 "R restart"）。
    //      这条纪律的来源是 S9 偏差 10（"面板一直在教人按一个已经不存在的键"）。
    //    · ⛔ **不列 `ESC`**：它虽然**在 `Playing` 下确实有用**（暂停），但**含 `E`** ⇒ 见上面那条禁令。
    //      `ESC` 的提示落在**横幅**：`DrawTitle` 的 `in game:` 那行（教学）+ `DrawPausedBanner`
    //      的 `ESC to resume`（当时当刻）。**代价**：`Playing` 里看不到"怎么暂停"，
    //      但这比"把敌人的字形弄脏"划算得多。
    //    ⚠️ 用例里有**反向断言**：提示段**必须恰好**是这五个键（含则红）—— 防止后来人"顺手统一"。
    // ⚠️ 提示行里的键名与代码里的常量是**同一个键的两个落点**（同 `kDumpKey`/`kFireKey` 的纪律）：
    //    换键时这一行要一起改。⚠️ 这里是 `F7` 的唯一提示落点（面板表头那句是另一处）。
    //    ⚠️ 段里那个 `Q` 是 **`kQuitKey`** 的两处字面落点之一（另一处在 `DrawTitle` 的 `in game:` 横幅）
    //       —— 换 `kQuitKey` 时**这一行要一起改**（`TECH_DEBT` B6）。提示是给玩家看的字面量、
    //       没法用常量渲染，所以靠这行注释兜住：grep `kQuitKey` 就能找到这里。
    char stateText[96];
    std::snprintf(stateText, sizeof(stateText),
                  "hp %2d | ammo %2d | kills %2d | wave %2d | dir %s | WASD SPC Q F4 F7",
                  m_playerHp, m_ammo, m_kills, m_wave, game::FacingGlyph(m_facing));
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
    // ⚠️ 提示里的键名与 `kStartKey`/`kPauseKey`/`kQuitKey` 是**同一个键的两个落点**（同 `kDumpKey` 的纪律）：
    //    换键时这里要一起改。⚠️ 这里也是 `kStartKey` 唯一的提示落点。
    //    ⚠️ 末段那句 `Q quit` 是 **`kQuitKey`** 的两处字面落点之一（另一处在 `DrawHud` 行 2 的提示段）
    //       —— 换 `kQuitKey` 时**这一行要一起改**（`TECH_DEBT` B6）。
    //    ⚠️⚠️ **这条横幅没有机器守卫**：用例只钉了 HUD 那条提示段（精确串那一行），
    //       横幅**一个字都没被断言过** ⇒ **改键漏了它没人会提醒你**，只有人眼（§10.5 的 A1）能看见。
    // ⚠️ **刻意不写"5 个敌人"这类会随波次变的话** —— 波次在步骤 4 才落地，
    //    写死数字就会变成"假提示"（同 S9 偏差 10 的 `K=kill nearest` 那个教训）。
    //
    // ⚠️⚠️ **2026-10-12（S10 步骤 6，"各状态横幅过一遍"）改了最后一行**：
    //    原来是 `WASD move   SPACE fire   ESC pause   Q quit` —— 但那三个键在 `Title` 下
    //    **全是 no-op**（`Title` 的 `StatePolicy` 五项全 false；`NextState(Title, PauseToggled) → Title`）
    //    ⇒ 那是**假提示**（S9 偏差 10 的同一个毛病：提示里的每个键都必须**在这个状态下**真的有用）。
    //    改成加前缀 **`in game:`** —— 把它从"现在按这个"明确降级为"**进去之后**能干什么"，
    //    于是这一行不再声称任何 `Title` 下能做的事；`Title` 下唯一真有用的键（`ENTER`）
    //    仍然由上一行 `PRESS ENTER TO START` 单独承担。
    //    ⚠️ 别把前缀删回去 —— 删了它这一行又开始"教人按 ESC"。
    const int top = fb.Height() / 2 - 2;
    DrawCenteredBlock(fb,
                      {"MiniEngine",
                       "",
                       "PRESS ENTER TO START",
                       "",
                       "in game:   WASD move   SPACE fire   ESC pause   Q quit"},
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
    // ⚠️ **2026-10-12（S10 步骤 3）补回了 `R restart`**：`StartNewRun()` 到位之后它不再是假提示。
    //    （步骤 2 时这里只写 `ESC back to title`，因为那时按 `R` 只会让状态闪一帧 ——
    //     那半天的"缺一行提示"就是 S9 偏差 10 那条教训的现场应用：**提示里的每个键都必须真的有用**。）
    // ⚠️ 提示里的键名与 `kRestartKey`/`kPauseKey` 是**同一个键的两个落点**（同 `kDumpKey` 的纪律）：
    //    换键时这里要一起改。
    const int top = fb.Height() / 2;
    DrawCenteredBlock(fb, {"GAME OVER", "", "R restart     ESC back to title"}, top > 0 ? top : 0);
}

void Game::OnRender(mini::render::FrameBuffer& fb)
{
    // `Title` 是**纯文字画面**：世界与 HUD 都不画。
    // ⚠️ 不依赖 `DrawHud` 内部那句 `player == nullptr` 早退来"挡住 HUD"——
    //    那是"碰巧正确"（玩家的确还没出生），换成显式判断才不靠巧合。
    if (m_state != GameState::Title) {
        // ── S12（第 4 笔）：**2D / 3D 的分叉只在这一处** ─────────────────────────────
        // ⚠️ `DrawHud` **不在分叉里**：3D 也要 HUD（`hp`/`ammo`/`dir` 一样得看得见）。
        // ⚠️ 3D 下**不画** `DrawMap`/`DrawEntities`（§1.2 有意不做"在 3D 里看见自己"）
        //    ⇒ 屏幕上不再出现 `@`/`E`/`*`，也不再有 `#` —— 这正是"3D 屏上的判据不能
        //    沿用 2D 那套"的原因（坑 9：3D 的新用例**禁止**用 `ScreenText`）。
        if (m_viewMode == ViewMode::Mode3D) {
            DrawRaycastWorld(fb);
        } else {
            DrawMap(fb);
            DrawEntities(fb);
        }
        // HUD 压在**视口第 0 行**上（= 相机原点那一行；同"顶栏压在最上面"一个道理）。
        // ⚠️ 顺序不能换：HUD 要盖掉地图，而 `@` 必须画在地图之后、HUD 之前。
        DrawHud(fb);

        // 面板在 HUD 之后。它**底部对齐**，与 HUD（第 1 行）以及敌人常在的第 3~7 行都不重叠。
        if (m_showAiPanel) {
            DrawAiPanel(fb);
        }

        // ── S11：正式 Debug Overlay（键 = `kOverlayKey`）─────────────────────────
        // 位置：**在 `DrawHud` 与 `F7` 面板之后**（§8 坑 10）—— 它是浮层，读数优先于底下的东西。
        // ⚠️ 但状态横幅（`PAUSED` / `GAME OVER`）仍然画在它**之上**（下面那段 switch）：
        //    那是玩家必须先看到的信息，抢不过它。
        // ⚠️ `rows` 是**栈上数组**（8 行小结构），不分配、不缓存进 `Game`：
        //    Profiler 自己拥有数据，这里每帧问一次快照（`Snapshot` 写进调用方数组的取舍）。
        if (m_showOverlay) {
            mini::core::Profiler::Row rows[mini::core::Profiler::kMaxScopes]{};
            int                        rowCount = 0;
            if (mini::core::Profiler* profiler = mini::core::CurrentProfiler();
                profiler != nullptr) {
                rowCount = profiler->Snapshot(rows, mini::core::Profiler::kMaxScopes);
            }
            // ⚠️ 未装 profiler（单测里就是这样）⇒ rowCount 保持 0：overlay 照画，只是没有表体。
            //    这条路径是**默认情形**（`Game` 能在没有 `Application` 的情况下被构造）。
            DrawDebugOverlay(fb, BuildOverlayData(rows, rowCount));
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
// ⚠️ **S11 起它不再自己开文件**（`TECH_DEBT` C14 的到期处置，2026-10-15）：
//   · 写入走 `mini::core::Log(LogLevel::Info, 一行)` —— 分级、前缀（`[时间][级别] `）、
//     追加、每行 `flush()` 全归 `core/Logger` 管（那些坑在 `Logger.cpp` 里踩过一遍了）；
//   · **未装 Logger 时静默丢弃**（单测里就是这样）⇒ 调试输出不许影响游戏能不能跑
//     —— 这条契约从第一版起就在，只是换了实现者；
//   · **文件名也归 Logger**（`Logger::kDefaultPath` = `miniengine_debug.log`）
//     ⇒ 本文件不再有第二个"文件名真相"（删掉了 `kDumpFileName`）。
//
// ⚠️ 关于**旧文件头**（UTF-8 BOM + 中文字段说明）为什么退役：
//   Logger 不写 BOM，而没有 BOM 时 PowerShell 的 `Get-Content` 会按 **GBK(936)** 解码
//   ⇒ 中文字段说明会显示成乱码。所以字段说明改成**纯 ASCII 的两行**（`dump-doc:` 开头，
//   **每个进程第一次 dump 时写一次**），中文解释留在本段注释里（源码里读得到、不会乱码）。
//   ⚠️ **老日志文件不受影响**：追加模式只往后写，它原有的 BOM 与中文头照旧可读。
//
// ⚠️ 契约（不变）：
//   · **只读**世界（不改 pos、不碰相机）→ 放在帧序哪一步都不影响别的系统；
//   · 每次 dump 写**两行**（主行 + 四邻行），保持老文件的可读形状（§8 坑 13）；
//   · 打开失败（目录只读 / 文件被占）也**不影响游戏** —— 现在这由 Logger 兜（`Good()`）。
// ------------------------------------------------------------
void Game::DumpSnapshot(const mini::core::FrameContext& ctx)
{
    // 每个**进程**第一次 dump 时写一次字段说明。
    // ⚠️ 判据是 `m_dumpCount == 0`（计数器进程内从 0 起）。老实现用 `std::filesystem`
    //    判断"文件是不是新的" —— 那正是 2026-10-08 那个"BOM 与表头被反复插进文件中间"的
    //    bug 来源（① `tellp()` 在追加模式下未写入前不可靠；② `m_dumpCount == 1` 每次启动都成立）。
    //    现在不需要那种判断了：**session 头由 Logger 在构造时写**（每个进程一条），
    //    "本轮说明"由这个计数器守 —— 两件事各归各的，不会互相插队。
    if (m_dumpCount == 0) {
        mini::core::Log(mini::core::LogLevel::Info,
                        "dump-doc: pos/tile/frac = player center, its cell, offset inside the cell (0..1)");
        mini::core::Log(mini::core::LogLevel::Info,
                        "dump-doc: now/E/W/S/N + solidNow/tryStepX/tryStepY: 0=empty 1=solid "
                        "(out-of-bounds counts as solid; E=+x W=-x S=+y N=-y)");
    }

    ++m_dumpCount;

    const mini::world::Entity* player = m_world.Find(m_player);
    if (player == nullptr) {
        std::ostringstream line;
        line << "dump=" << m_dumpCount << " (player not spawned)";
        mini::core::Log(mini::core::LogLevel::Info, line.str());
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

    // 两行都先**拼成字符串**再交给 Logger（不是 `<<` 进文件）——
    // 这样"格式"与"去哪里"彻底分开：换 sink（文件/终端/内存）不用碰格式（§8 坑 13 的口径）。
    {
        std::ostringstream line;
        line << "dump=" << m_dumpCount
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
             << " cam=" << m_camera.OriginCol() << ',' << m_camera.OriginRow();
        mini::core::Log(mini::core::LogLevel::Info, line.str());
    }

    // 自己那一格 + 四邻：新模型里"往某方向能不能走"就是看**那一格**实不实心
    {
        std::ostringstream line;
        line << "  now(" << tx << ',' << ty << ")=" << CellMark(m_world.Map(), tx, ty)
             << " E(" << (tx + 1) << ',' << ty << ")=" << CellMark(m_world.Map(), tx + 1, ty)
             << " W(" << (tx - 1) << ',' << ty << ")=" << CellMark(m_world.Map(), tx - 1, ty)
             << " S(" << tx << ',' << (ty + 1) << ")=" << CellMark(m_world.Map(), tx, ty + 1)
             << " N(" << tx << ',' << (ty - 1) << ")=" << CellMark(m_world.Map(), tx, ty - 1)
             << " solidNow=" << (mini::world::IsSolidAt(m_world.Map(), mini::math::Vec2{px, py}) ? 1 : 0)
             << " tryStepX="
             << (mini::world::IsSolidAt(m_world.Map(), mini::math::Vec2{px + dx, py}) ? 1 : 0)
             << " tryStepY="
             << (mini::world::IsSolidAt(m_world.Map(), mini::math::Vec2{px, py + dy}) ? 1 : 0);
        mini::core::Log(mini::core::LogLevel::Info, line.str());
    }
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

// 同上：真相是私有的 `m_wave`（`BeginWave` 是**唯一**写入点）。
// ⚠️ 它在 `HUD` 上真的看得见（`wave` 那一栏）⇒ 同 `PlayerHp`/`Kills` 一个口径，不是"为测试而加"。
int Game::Wave() const noexcept
{
    return m_wave;
}

// 同上：真相是私有的 `m_ammo`（三个写入点全在 Game.cpp，见 `Game.h` 的成员注释）。
// ⚠️ HUD 的 `ammo` 那一栏真的读它 ⇒ 同 `PlayerHp`/`Kills` 一个口径。
int Game::Ammo() const noexcept
{
    return m_ammo;
}

// ------------------------------------------------------------
// S11：Debug Overlay 的三个观察面 + 数据装配
// ------------------------------------------------------------

int Game::EnemyCount() const noexcept
{
    return static_cast<int>(m_enemies.size());   // 真删之后这里就是"场上还有几个"
}

int Game::BulletCount() const noexcept
{
    // "存活"的判据是**世界还找得到它**（同 ⑨b 自愈压缩用的那条判据）：
    // 表里可能有已死的 id（帧中途标删的），它们不算数。
    // ⚠️ 所以这个数**不是** `m_bullets.size()` —— 后者要等帧末才收敛。
    int alive = 0;
    for (const mini::world::EntityId id : m_bullets) {
        if (m_world.Find(id) != nullptr) {
            ++alive;
        }
    }
    return alive;
}

bool Game::OverlayOn() const noexcept
{
    return m_showOverlay;
}

OverlayData Game::BuildOverlayData(const mini::core::Profiler::Row* rows, int rowCount) const noexcept
{
    OverlayData d;

    // ── 引擎侧：来自 `OnUpdate` 开头拷下的快照 ⇒ **慢一帧**（分叉 F 甲）──
    d.frameMs    = m_lastFrame.frameMs;
    d.maxFrameMs = m_lastFrame.maxFrameMs;
    d.workMs     = m_lastFrame.workMs;
    d.fps        = m_lastFrame.fps;

    // ── Profiler 那张表：调用方给的（`OnRender` 的栈数组）──
    d.rows     = rows;
    d.rowCount = rowCount;
    // 已收帧数：让表头显示**真实**窗口大小（首屏不满 120 时若恒显示 120 就是骗人，§8 坑 8）
    d.historyFrames =
        (mini::core::CurrentProfiler() != nullptr) ? mini::core::CurrentProfiler()->HistoryFrames() : 0;

    // ── 游戏侧：**当帧**现读（这些 `Game` 自己随时都有）──
    d.wave  = m_wave;
    d.foes  = EnemyCount();
    d.shots = BulletCount();
    d.ammo  = m_ammo;
    d.hp    = m_playerHp;
    // S11 5c：几何层开关也进面板 —— 底色层开没开**从读数上要能确认**
    //   （不然"屏幕上那片灰是图层还是别的什么"只能靠猜；同 `Rows` 之于顶栏的作用）
    d.geometry = m_showGeometry;

    return d;
}

bool Game::ShouldQuit() const
{
    return m_quit;
}

} // namespace game
