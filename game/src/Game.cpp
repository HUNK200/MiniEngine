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
//   3. 每帧：世界 → 屏幕（OnRender，**没有相机**，视口原点恒为世界 (0,0)）
//
// ⚠️ 帧序（S5_CHECKLIST 7.1，顺序不能换）：
//     读输入 → 归一化 → **写速度** → Update(dt) → FlushRemovals
//   写速度必须在 Update 之前：反过来的现象是"按一下动一下、但慢一帧"，
//   看着像手感问题，其实是顺序问题。
//
// ⚠️ 坐标：`dt` 用 `ctx.deltaSeconds`（S3 已 clamp 过，**这里不夹**）；
//   世界→屏幕的取整**一律 floor**（7.2）：列 = floor(x)、行 = floor(y) + 1
//   （+1 是给引擎顶栏让出第 0 行）。`TileMap::ToIndex` 就是那个 floor。
// ============================================================

namespace game {

namespace {

// 游戏 HUD 占第 1 行（第 0 行是引擎顶栏）。
//
// ⚠️ 代价说清：HUD **压在世界的第 0 行上** —— 那一行等于被 HUD 占用（地图照画，
//    HUD 后画会盖掉它）。这是"世界→屏幕只差一个 +1"的必然结果：要让游戏内容
//    完整可见，要么加相机（S6），要么把 +1 变成 +2（那 7.2 的公式就得改）。
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
    //    ⚠️ 开局视野只有左上角 80×24，所以这一圈里**只有左边那条看得见**：
    //    上边会被游戏 HUD 占掉，右边/下边要等 S6 的相机走到地图边缘才见得到。
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

    // ⑦ 延迟删除。S5 恒为空 —— 留着是为了让帧序在 S9 之前就是对的
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

    // S5 没有相机：屏幕(c,r) 对应世界 (c, r-1)
    for (int row = kHudRow; row < h; ++row) {
        const int worldRow = row - 1;
        for (int col = 0; col < w; ++col) {
            if (map.At(col, worldRow) == mini::world::Tile::Wall) {
                fb.Set(col, row, '#', mini::render::Color::BrightBlack);
            }
        }
    }
}

void Game::DrawEntities(mini::render::FrameBuffer& fb) const
{
    // const 版 Entities()：本方法是 const。span 只在这次遍历里有效
    for (const mini::world::Entity& entity : m_world.Entities()) {
        if (!entity.active) {
            continue;   // 不更新 ≠ 不画；active 只管 Update（见 World 的注释）
        }

        // 世界 → 屏幕：**floor**，不是 round（7.2 统一后的唯一取整）
        const int col = mini::world::TileMap::ToIndex(entity.transform.pos.x);
        const int row = mini::world::TileMap::ToIndex(entity.transform.pos.y) + 1;

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

    // 先把这一行铺满空格，再写字：否则 HUD 右边会露出地图第 0 行的边框，
    // 看着像"画面坏了"（S3 的顶栏也是这样独占一行的）
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
    DrawHud(fb);   // 最后画：压在世界的第 0 行上（同"顶栏压在最上面"一个道理）
}

// ------------------------------------------------------------
// 退出
// ------------------------------------------------------------

bool Game::ShouldQuit() const
{
    return m_quit;
}

} // namespace game
