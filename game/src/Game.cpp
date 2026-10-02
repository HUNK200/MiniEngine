#include "game/Game.h"

#include "mini/math/Vec2.h"
#include "mini/platform/Input.h"
#include "mini/render/Color.h"
#include "mini/world/TileMap.h"

#include <cstdio>

// ============================================================
// Game 实现（S5）—— 引擎与游戏分家之后的**唯一**游戏侧文件
//
// 这里做三件事，别的一件都不做：
//   1. 开局：铺地图、Spawn 玩家（BuildDemoMap / OnStart）
//   2. 每帧：输入 → 速度 → 交给 World 积分（OnUpdate，帧序是硬约束，见下）
//   3. 每帧：世界 → 屏幕（OnRender，S6 起走相机）
//   4. 每帧：相机跟随（OnUpdate 末尾，**必须在 Update 之后**）—— S6 加的
//
// ⚠️ 帧序（S5_CHECKLIST 7.1 + S6_CHECKLIST 7.1，顺序不能换）：
//     读输入 → 归一化 → **写速度** → Update(dt) → **相机 Follow** → FlushRemovals
//   写速度必须在 Update 之前：反过来的现象是"按一下动一下、但慢一帧"，
//   看着像手感问题，其实是顺序问题。
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

    // ④ Q 退出。Esc 刻意不退出 —— 它从 S10 起要用来关菜单（S2 的决策），
    //    进程级退出（Ctrl+C / 关窗口）由 Application 看 Input::QuitRequested()。
    if (input.Get(Key::Q) == KeyState::Pressed) {
        m_quit = true;
    }

    // ⑤ Σdt：HUD 拿它自证 `pos ≈ v × sum dt`（10.2 的运行时验收；v 就是 m_moveSpeed）
    m_elapsed += ctx.deltaSeconds;

    // ⑥ 积分（world 唯一的"物理"）。dt 已由 S3 clamp 过，这里**不再夹**（坑 #7）
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

    // ⑧ 延迟删除。S5 恒为空 —— 留着是为了让帧序在 S9 之前就是对的
    m_world.FlushRemovals();
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
            continue;   // 不更新 ≠ 不画；active 只管 Update（见 World 的注释）
        }

        // 世界 → 屏幕：**floor**（`ToIndex`），再减去相机原点；行还要加回 kHudRow
        const int col = mini::world::TileMap::ToIndex(entity.transform.pos.x) - originCol;
        const int row = mini::world::TileMap::ToIndex(entity.transform.pos.y) - originRow + kHudRow;

        const mini::render::Color color = (entity.id == m_player)
                                              ? mini::render::Color::BrightYellow
                                              : mini::render::Color::BrightWhite;
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
    // 提示文字压缩成 `WASD  Q`，否则这一行会超过 80 列（终端宽度）。
    char text[96];
    std::snprintf(text, sizeof(text),
                  "pos %6.2f,%6.2f | tile %4d,%4d | sum dt %6.2fs | v %4.1f | WASD  Q",
                  static_cast<double>(px), static_cast<double>(py),
                  mini::world::TileMap::ToIndex(px),
                  mini::world::TileMap::ToIndex(py),
                  static_cast<double>(m_elapsed),
                  static_cast<double>(m_moveSpeed));
    fb.Draw(0, kHudRow, text);
}

void Game::OnRender(mini::render::FrameBuffer& fb)
{
    DrawMap(fb);
    DrawEntities(fb);
    // 最后画：压在**视口第 0 行**上（= 相机原点那一行；同"顶栏压在最上面"一个道理）。
    // ⚠️ 顺序不能换：HUD 要盖掉地图，而 `@` 必须画在地图之后、HUD 之前。
    DrawHud(fb);
}

// ------------------------------------------------------------
// 退出
// ------------------------------------------------------------

bool Game::ShouldQuit() const
{
    return m_quit;
}

} // namespace game
