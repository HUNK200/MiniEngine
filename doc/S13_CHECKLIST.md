# S13 施工图 —— Sprite + Z-buffer（伪 3D 里"看得见东西"）· v1.0 的第 ① 条

> **状态（2026-10-26 收口）**：**第 1~4 笔 + 收口全部完成** —— 机器门 **259 用例全绿**（`Game` 56 / `Sprite` 8 / `Raycaster` 10 / 其余 185；由**本人回报**，见 §12）。
> 第 1 笔（`RayCamera.h` 搬家 + `Sprite.h` + 8 条纯函数用例）· 第 2~3 笔（`m_colDepth` + `CollectSprites`/`DrawSprites` + 4 条 Game 用例）· 第 4 笔（`kBulletHeight`/`kBulletCellsTall`/`kBulletCellsWide` + **5 条** Game 用例）。
> ✅ **反向验证 5 场**（§12 总账，含 **3 处预测失误**的如实记录）· ✅ **人眼通过**（墙后遮挡、子弹高度两条是本人反复盯过的）· ✅ 版本 **`0.6.0-dev`** · ⏳ **`v0.6.0` tag 待打**（机器门与人眼都已满足）。
> ⚠️ 两处照实记的偏差：① 第 4 笔**多 1 条**施工图没有的用例（`SpritesDisappearWhenFacingAway`）⇒ 总数 **259**，不是施工图算的 258；② `kBulletHeight` 定在 **1.0**（1.2 / 0.8 都被人眼否掉，见 §11 的 T2）。
>
> 2026-10-22 审核留下的 7 处修改（供对照）：① 用例 **11 → 16**（纯函数 6→8、Game 5→8）；② 计数更正（`Game` 45→**47**、基数 240→**242**、期望 **258**）；
> ③ **R1 的预测反了**（不写 depth ⇒ 精灵**全不画**，不是穿墙可见）；④ **R4 改为"裁剪下沉进纯函数"**（`SpritesNeverTouchHudRows` 降级为冒烟）；
> ⑤ 坑表 +**15/16**；⑥ `ProjectSprite` 签名补 **`aspect` + `cellHeightPerWidth`**、`horizon` 改由 `viewTop` 派生；⑦ 第 1 笔新增 **`render/RayCamera.h` 搬家**（纯搬运）。
> **前置**：S12 伪 3D ✅ **已收口**（2026-10-22；242 用例 / 反向验证 7 场 / 版本 `0.5.0-dev`）。✅ **`v0.5.0` tag 已于 2026-10-22 打并推送**（annotated，指向 `77502d2`）。
> ⚠️ 本文是**方案层 + 实测记录**：接口、伪代码、坑表、验收在 §0~§11；**反向验证的实测**在 §12；**收口记录**在 §13。

---

## 0. 目标 / 明确不做

| 项 | 内容 |
| --- | --- |
| **目标** | 3D 视角里**看得见敌人（`E`）与子弹（`*`）**，并且**遮挡正确**：墙在它们前面就挡住，在它们后面就让它们出现 |
| ⚠️ **目标不是"多次渲染"** | "两趟（先墙、后精灵）"只是**载体**；S13 **唯一的新增量**是一张**按列的深度数组 + 一次比较**（见 §0.1）。S12 的 `OnRender` 本来就已"分好几趟在写同一块 `FrameBuffer`"（地图/墙 → 实体 → HUD → 面板 → overlay → 横幅）⇒ **分层不是新东西，深度测试才是** |
| **v1.0 的判定**（`PLAN:1338`） | ① **精灵 + z-buffer 遮挡正确** ② README + GIF + CI ③ 干净克隆后三条命令能跑（后两条不属于 S13） |
| **不做** | ① **不画玩家自己**（第一人称 ⇒ 有意不做，`S12_CHECKLIST` §1.2）；② 不做**精灵贴图/多帧动画**（1 格 = 1 字符）；③ 不做**精灵之间的像素级深度测试**（走"远 → 近"画家算法）；④ 不做真 3D（`TECH_DEBT` **C8** 仍然挂着）；⑤ **不改 2D 路径**（`DrawMap`/`DrawEntities` 一行不动） |

### 0.1 三个概念别混：S13 到底"新"在哪

| 概念 | 是什么 | 在 S13 里的角色 | 现在有没有 |
| --- | --- | --- | --- |
| **多趟（pass）** | 往**同一块 `FrameBuffer`** 上按顺序多写几遍 | **实现载体** | ✅ **S12 就已经是这样**（墙/地图 → 实体 → HUD → 面板 → overlay → 横幅） |
| **深度测试（depth test）** | 记下"每列最近的墙有多远"，精灵**更近才写** | ★ **S13 的唯一新增量** | ❌ 没有（每列只画一次墙 ⇒ 本来不需要） |
| **遮挡正确（occlusion）** | **目标/验收**：墙挡住的东西不许露出来 | **目标**（上面"目标"那一行） | ❌ 3D 里现在压根没有精灵，谈不上遮挡 |

**新增量就这么三行**（其余都是壳）：
```text
m_colDepth[col] = hit.perp                    # ① 墙 pass 顺手记下（1 行）
if (box.perp >= m_colDepth[col]) continue     # ② 精灵逐列比深度（1 行）
fb.Set(col, row, glyph, color)                # ③ 更近才写（这句本来就要写）
```

**"能不能一趟做完？"—— 能，但更差**：

| 做法 | 代价 |
| --- | --- |
| **两趟**（推荐） | 每个精灵的投影**算一次**、排序**排一次**、代码分两段各自可读 |
| 一趟（每列画完墙顺手画精灵） | 同一个精灵的投影要**按列重算 N 次**（100 列就 100 次）；精灵之间的远近关系**变成按列**处理（同一个精灵在不同列上跟不同邻居比）⇒ 又慢又难读 |

⇒ **结论**："两趟"是**性能与可读性的选择**，不是正确性要求；**S13 的目标永远是"3D 里看得见东西且遮挡正确"**。

---

## 1. 前置事实（动手前先核这 10 条）

| # | 事实 | 出处 | 对 S13 的含义 |
| --- | --- | --- | --- |
| 1 | `DrawRaycastWorld` 每列算了 `hit.perp`，**但用完就扔** | `Game.cpp`（S12 第 4 笔） | S13 要把它**留下来** ⇒ 列深度数组 |
| 2 | 每列**只画最近那面墙** ⇒ 现在不需要深度 | 同上（`S12_CHECKLIST` §2 第 6 步） | 一旦加精灵，这个"免费遮挡"就**必须**换成显式深度比较 |
| 3 | `WallHeightRows(perp, viewH, aspect) = viewH/perp*aspect`，且 `≥ 1 行` | `engine/render/RayCaster.h` | 精灵的**垂直尺寸必须复用同一个缩放**（否则敌人"飘在空中"/"埋在墙里"） |
| 4 | 视口 = 行 `kHudRow .. fb.Height()-1`，地平线 = `kHudRow + viewH/2` | `Game.cpp` | 精灵**不许画到 HUD 行上** |
| 5 | 实体是**点模型**（`Entity{id, transform.pos, symbol}`），没有尺寸/朝向，且 `sizeof(Entity) == 24` 有 `static_assert` 钉住 | `world/Entity.h`（S5 起，`Entity.h:83`） | 精灵的宽高只能是**约定值**（随 `SpritePlace` 传入）⇒ **z 与尺寸都放 `game` 侧，绝不加进 `Entity`**（加一个 `float` 断言当场红，分叉 D 甲） |
| 6 | 敌人死 = **真删实体**（`S9`） | `Game.cpp` `FlushRemovals` | 3D 里死敌人**自动消失**（与 2D 一致，不用额外判断） |
| 7 | 2D 的 `E`/`*`/`@` 是**被计数的字形**（C31 纪律） | `TECH_DEBT` C31 | 3D 下画 `E`/`*` **不冲突**（两个模式不同帧共存）⇒ **沿用同一套字形**，见分叉 F |
| 8 | `PLAN:1203` 已经预告过这件事 | "墙的渲染天然产生了按列深度数组……z-buffer 有 320 项，比 GPU 的 2D 缓冲区小 5 个数量级，但**算法一模一样**" | 本阶段就是把它做出来 ✓ |
| **9** | ★ **`DrawRaycastWorld` 是 `const` 成员**（`void DrawRaycastWorld(FrameBuffer&) const;`） | `Game.h:371`（2026-10-22 核） | 分叉 B 甲要写 `m_colDepth` ⇒ **必须去掉 const**（`OnRender` 本身非 const，`Game.cpp:1462` ⇒ 无涟漪） |
| **10** | ★ **`AspectForViewport` 已经让墙带 = 物理 1 格**（`aspect = cols/(2·fovTan·viewH·r)` 是"1 格宽 = 1 格高"的唯一解） | `Game.cpp:203-215`（S12 第 5 笔） | ⇒ 分叉 C 的 `kWallHeightCells = 2.0` **不是"把墙调高"**，而是把"1 格"定义成**半条墙带**（敌人 = 半墙高、头顶齐眼平线） |

---

## 2. 六个分叉（**✅ 2026-10-22 全部拍板**）

| # | 分叉 | **决定** | 理由 / 连带 |
| --- | --- | --- | --- |
| **A** | 精灵的**投影数学**放哪 | **甲 + A-2** | `engine/render/Sprite.h`（header-only 纯函数，同 S12 的 C=甲）⇒ **可离线单测**。**A-2**：把 `RayCamera` + `kMinPerp` 搬进**新头 `render/RayCamera.h`**（`RayHit` 留原地）⇒ `Sprite.h` 的 include 图里**没有** `TileMap`/`Collision`。⚠️ 若只搬 `RayCamera` 而不搬 `kMinPerp`，`Sprite.h` 照样要 include `RayCaster.h` ⇒ A-2 白做 |
| **B** | **列深度数组**的所有权 | **甲 + 去 `const`** | `m_colDepth` 是 `Game` 成员（宽度变了才 `resize`）⇒ 不每帧分配、不设上限。**代价必须认**：`DrawRaycastWorld` 现在是 `const`（`Game.h:371`）⇒ **去掉 const**（`mutable` 是"在 const 里偷改状态"，不选）。顺带 `m_spriteBuf` 精灵列表**也复用**（坑 7 是同一个毛病） |
| **C** | 精灵的**世界尺寸**基准 | **甲** = `kWallHeightCells = 2.0f` | ⚠️ **语义已更正**：`AspectForViewport` 的推导本身把墙带锁定成**物理 1 格** ⇒ 声明 2.0 **不改变墙**，只把"1 格"定义成**半条墙带** ⇒ **敌人 = 半墙高、头顶齐地平线**（100×28、`perp=3.5`：5.41 行 vs 10.8 行）。**不是"让墙变高"** —— 别去改 `WallHeightRows` |
| **D** | 精灵的**高度（z）** | **甲 / 甲 / 照抄 / 坑 15** | ① `z` 只在 `game` 侧（`SpriteItem.z`），**`Entity` 一个字节都不动**（`Entity.h:83` 的 `static_assert(sizeof(Entity) == 24)` 会当场红）；② `z` 语义 = **脚底**离地高度，子弹在调用处写 `z = kBulletHeight − cellsTall/2`；③ `CollectSprites` **照抄 `DrawEntities` 的身份判定**（跳 `!active`、跳玩家、`IsEnemyId` 分色），**字形取 `entity.symbol`**（不建 kind→字形映射）；④ 新坑 **15**：精灵行数**至少 1** |
| **E** | 精灵之间的**画序** | **甲 + E-2** | 甲：按 `perp` **降序**（远→近）画家算法，**不做**精灵之间逐列深度。**E-2：先投影、后排序** —— 排序键用 `box.perp`（唯一真相），收集阶段**不算** `perp`（否则两处 `perp` 迟早分叉）。**次级键 `EntityId`**：同深度画序确定 ⇒ 屏幕可复现 |
| **F** | 3D 下精灵用**什么字形** | **甲**（由 D ③ 自动成立） | 沿用 `E`/`*`，且**直接取 `entity.symbol`** ⇒ 不建映射表、加新实体类型时 3D 自动跟上。代价：一个敌人跨 ≈10.8 列 ⇒ 屏幕上有十几个 `E`，被墙挡住一半时列数还会变 ⇒ **判据纪律见坑 16**（只断"至少一次"，禁止搬 2D 的计数辅助） |

---

## 3. 接口设计

### 3.1 `engine/include/mini/render/Sprite.h`（新，header-only 纯函数）

```cpp
namespace mini::render {

// 输入：一个"贴在世界上某处、有高度 z"的精灵（**只读**，不认识 Entity/game）
struct SpritePlace {
    math::Vec2 pos{};              // 世界坐标（格，含亚格精度）
    float      z = 0.0f;           // ★ **脚底**离地高度（格）；0 = 站在地上（D-2 甲）
    float      cellsTall = 1.0f;   // 世界高度（格）；1 = 一格高（C 甲：= 半条墙带）
    float      cellsWide = 1.0f;   // 世界宽度（格）
};

// 输出：落到屏幕上的矩形（**半开区间**：列 [col0, col1)、行 [row0, row1)）+ 深度
// ★ 出来时**已经在视口内**（R4 甲：裁剪在纯函数里做完）、且**至少 1 行**（坑 15）
struct SpriteBox {
    int   col0 = 0, col1 = 0;
    int   row0 = 0, row1 = 0;
    float perp = 0.0f;             // ★ 与墙**同一个**深度口径（垂距），用于比较
    bool  visible = false;         // false = 相机背后 / 贴脸 / 裁完为空
};

// 唯一入口：世界 → 屏幕（**不认识** FrameBuffer，也不画）
// ⚠️ 观感参数必须**从外面喂**（`aspect` / `cellHeightPerWidth`），引擎不认识 Game.cpp 的策略
[[nodiscard]] SpriteBox ProjectSprite(const SpritePlace& place, const RayCamera& cam,
                                      int cols, int viewTop, int viewH,
                                      float aspect, float cellHeightPerWidth,
                                      float wallHeightCells) noexcept;

} // namespace mini::render
```

**约定（和 `RayCaster.h` 对齐）**

- `perp` **必须**用与墙同一个定义（`RayHit::perp`，即沿 `dir` 的垂距）⇒ 否则"墙 vs 精灵"的比较没有意义；
- 相机背后（`dot(toSprite, dir) <= 0`）⇒ `visible = false`（**不做**"绕过背后"的裁剪）；
- 输入不 clamp，**输出必须有限且在视口内**（贴脸 ⇒ 走 `kMinPerp`；行/列**在函数内夹**；行数**至少 1**）；
- `horizon` **不再作为参数**：由 `viewTop + viewH/2` 在函数内派生（少一个可能自相矛盾的输入）。

### 3.2 `game` 侧接线（只碰 `Game.h` / `Game.cpp`）

```cpp
// Game.h（私有）
std::vector<float>      m_colDepth;     // 分叉 B 甲：复用，宽度变了才 resize
std::vector<DrawnSprite> m_spriteBuf;   // 同上：**精灵列表也复用**（坑 7 是同一个毛病）

struct SpriteItem {                      // "要画什么"（收集结果，还没投影）
    mini::math::Vec2 pos; float z; float cellsTall; float cellsWide;
    char32_t glyph; mini::render::Color color;
};
struct DrawnSprite {                     // "画到哪"（投影结果 + 排序键）
    mini::render::SpriteBox box;
    char32_t glyph; mini::render::Color color;
    mini::world::EntityId id;            // ★ 只用于排序的**次级键**（同深度画序确定）
};

void CollectSprites(std::vector<SpriteItem>& out) const;   // 只读世界 ⇒ 仍可以是 const
// ⚠️ 下面两个**都不加 const**（分叉 B 甲：它们要写成员缓存）
void DrawSprites(mini::render::FrameBuffer& fb);
void DrawRaycastWorld(mini::render::FrameBuffer& fb);
```

`DrawRaycastWorld` 变成**两趟**：
1. **墙**：逐列 `CastColumn` → 画墙条 → **顺手把 `hit.perp` 写进 `m_colDepth[col]`**（没命中写 `kRayFar`）；
2. **精灵**：`CollectSprites`（身份照抄 `DrawEntities`）→ **逐项投影**（丢掉 `!visible`）→ 按 `box.perp` 降序、`EntityId` 升序排 → 逐列比 `m_colDepth[col]` 后写格。

> ⚠️ **顺序是"投影在前、排序在后"**（分叉 E-2）：`box.perp` 是**唯一**的深度真相，收集阶段**不**算 `perp` ⇒ 不会出现"两处 `perp` 公式各说各话"（`dist` vs `perp` 的教训）。

---

## 4. 算法步骤（伪代码）

```text
# ① 墙（S12 已有，只加一行"记深度"）
for col in [0, cols):
    hit = CastColumn(map, cam, col, cols)
    m_colDepth[col] = hit.hit ? hit.perp : kRayFar      # ← 新增
    画墙条（原有逻辑不动）

# ② 收集 → **投影** → 排序（远 → 近）· ★ E-2：**投影在前**，排序键用 `box.perp`（唯一真相）
sprites = CollectSprites()          # 身份判定**照抄 `DrawEntities`**：跳 `!active`、跳玩家、`IsEnemyId` 分色；
                                    # 字形取 `entity.symbol`；敌人 z=0，子弹 z = kBulletHeight − cellsTall/2
drawn = []
for s in sprites:
    box = ProjectSprite(s, cam, cols, viewTop, viewH, aspect, r, kWallHeightCells)
    if not box.visible: continue                 # 相机背后 / 贴脸 / 裁完为空
    drawn.push({box, s.glyph, s.color})
sort(drawn, by box.perp DESC, then EntityId ASC) # ★ 次级键 ⇒ 同深度画序确定（用例可复现）

# ③ 逐个精灵：逐列比深度后写格（**裁剪已在 ProjectSprite 里做完** —— R4 甲）
for d in drawn:
    for col in [d.box.col0, d.box.col1):
        if d.box.perp >= m_colDepth[col]: continue   # ★ 墙更近 ⇒ 这一列跳过（遮挡）
        for row in [d.box.row0, d.box.row1):
            fb.Set(col, row, d.glyph, d.color)
```

**投影公式（`ProjectSprite` 内部，`cells` 单位）**

> ⚠️ **2026-10-16 复核后更正（初版横向公式写错了，见坑 4b）**：**尺寸**一律从 `lineH`（该距离下"一整面墙"的行数）推；**只有定位**（`centerCol`）才用几何公式。
>
> ⚠️ **2026-10-19 再更正（S12 第 5 笔落地后）**：上一版写的是"`kCellAspect` 是观感常数 ⇒ **不存在**对所有窗口都物理正确的常数 ⇒ 墙与精灵必须**共用一个（会偏的）尺度**"。
> 这条**结论已经作废**：S12 第 5 笔把墙的纵横比改成了**按视口算** ——
> `AspectForViewport(cols, viewH) = cols / (2·fovTan·viewH·r)`（`r` = 字符格高/宽 ≈ 2，`Game.cpp` 的 `kCellHeightPerWidth`），
> 100×28 ⇒ **1.353**、60×28 ⇒ 0.81（那个 `k*` 就是它，`kAspectTaste = 1.0` 时）。
>
> ⇒ **精灵直接用同一个 `aspect` 即可**：每个窗口都物理正确，且与墙天然一致，**不需要"共用一个会偏的尺度"这种妥协**。
> ⚠️ **纪律**：`AspectForViewport` 住在 `Game.cpp` 的匿名命名空间 ⇒ **`engine/render/Sprite.h` 必须把它当参数收**（与 `RayCaster::CastColumn/WallHeightRows` 收 `cellAspect` 同一条：**引擎不认识观感策略**）。
```text
toCam   = place.pos - cam.pos
forward = dot(toCam, cam.dir)                      # 沿 dir 的垂距（与 RayHit::perp 同口径）
if forward <= kMinPerp: visible = false             # 相机背后 / 贴脸

horizon  = viewTop + viewH / 2                      # ★ 由 viewTop 派生（**不再从外面传** ⇒ 少一个可能自相矛盾的参数）
lineH    = viewH / forward * aspect                 # ★ 与画墙**同一个式子/同一个值**（必须）
rowsPerU = lineH / wallHeightCells                  # 1 个世界格 = 多少行（C 甲：wallHeightCells = 2.0）
rowsTall = place.cellsTall * rowsPerU
colsWide = place.cellsWide * rowsPerU * cellHeightPerWidth   # ★ 物理上"方" ⇒ 列数 = 行数 × r
floorRow = horizon + lineH / 2                      # 地面线（= 墙脚）
row1     = floorRow - place.z * rowsPerU            # ★ `z` = **脚底**离地高度（D-2 甲）
row0     = row1 - rowsTall

# 只有**横向位置**用几何（fovTan）—— 否则精灵和墙在屏幕上对不齐
centerCol = cols/2 + (dot(toCam, plane) / forward) * (cols / (2 * cam.fovTan))
col0 = centerCol - colsWide/2 ; col1 = col0 + colsWide

# ★ R4 甲：**裁剪在纯函数里做**（Game 级用例守不住 —— 见坑 5）
row0 = max(row0, viewTop) ; row1 = min(row1, viewTop + viewH)
col0 = max(col0, 0)       ; col1 = min(col1, cols)
if row1 - row0 < 1: row1 = min(row0 + 1, viewTop + viewH)    # 坑 15：至少 1 行（远子弹不许消失）
if row1 <= row0 or col1 <= col0: visible = false
```

**算例（100×28、`aspect = AspectForViewport(100,28) = 1.353`、`wallHeightCells=2`、`perp=3.5`；`horizon=16`）**

> ⚠️ **2026-10-19 重算**（上一版按 `kCellAspect=2` 算的：`lineH=16` / `rowsPerU=8` / 1 格宽 `16 列`）。
> 现在墙用 `1.353` ⇒ 精灵**等比缩小到 68%**：`lineH` 16→**10.8**、`rowsPerU` 8→**5.41**、1 格宽 16→**10.8 列**。

| 量 | 值 | 说明 |
| --- | --- | --- |
| `lineH` | **10.8 行** | 那面墙在 3.5 格处占 10.8 行 ⇒ 上下各 5.41 行（约 `10.6..21.4`） |
| `rowsPerU` | **5.41 行/格** | 10.82 ÷ 2 |
| 1 格高精灵 | **5.41 行** | 站在地上 ⇒ 占 `16.0..21.4`（**底边 21.4 = 地面线**，**顶边 16.0 正好在地平线 = 相机高度** ✓ 自洽没坏） |
| 1 格宽精灵 | **10.8 列** | `1 × 5.41 × 2`（`r` = 字符格高/宽）⇒ 物理 10.8w × 10.8w = **方** ✓ |
| 若误用几何横向公式 | 21.6 列 | `100/(2·0.66·3.5)` = 21.6 ⇒ 物理 21.6w × 10.8w ⇒ **胖 2.0×** ✗（坑 4b）。⚠️ **2.0 就是 `r`** —— 别与"aspect 1.353"混为一谈（那是两个不同的比） |

---

## 5. 坑表（按"会出什么现象"排）

| # | 坑 | 症状 / 判据 | 对策 |
| --- | --- | --- | --- |
| 1 | **忘了写 `m_colDepth`** | ⚠️ **两种形态要分清**：① 数组保持**初值 0** ⇒ `box.perp >= 0` 恒真 ⇒ **精灵一个都不画**（症状是"3D 里什么都没有"，**不是**穿墙可见）；② 写了数组但**忘了做比较** ⇒ 精灵穿墙可见 | 用例：① 由 `EnemyVisibleIn3D` 抓（★ R1 的靶子）；② 由 `WallOccludesEnemy` 抓。⚠️ **这两条要分别存在**，只有一条时会漏掉另一种形态 |
| 2 | **深度用 `dist`（斜距）而不是 `perp`** | 屏幕两侧的精灵**误判遮挡**（明明在墙后被判成墙前，或反之） | 只用 `perp`；用例 `OcclusionUsesPerpNotSlant`（侧向摆位） |
| 3 | **从近往远画** | 远处的敌人盖住近处的（"人越远越大"） | 分叉 E 甲：降序排序；用例 `NearSpriteWinsOverFarOne` |
| 4 | **精灵的垂直尺度与墙不一致** | 敌人**飘在空中**或**埋进地板**；改 `kCellHeightPerWidth` / `kAspectTaste` 后更明显 | 分叉 C 甲：`kWallHeightCells` 显式化；用例断"贴地精灵的底边 == 地面行"。⚠️ **精灵必须调与墙同一个 `aspect`**（`AspectForViewport`），别自己再写第二份 |
| **4b** | **横向尺寸用几何公式（`cols/(2·fovTan·perp)`），不是墙的尺度** | 精灵**横向胖**（像块饼）；100×28 下差 **2.0×**（= 字符格高宽比 `r`） | ⚠️ 2026-10-16 复核发现：**施工图初版写错的就是这条**。尺寸必须**从 `lineH` 推**（见 §4 更正），几何公式**只用来定位**（`centerCol`） |
| 5 | **画到 HUD 行上** | HUD 第二行出现 `E`/`*`（玩家读不到 `hp`） | ✅ **R4 甲：裁剪下沉进 `ProjectSprite`** —— 纯函数收 `viewTop`，行夹到 `[viewTop, viewTop+viewH)`、列夹到 `[0, cols)` ⇒ 守卫是**纯函数**用例 `SpriteStaysInsideViewport`。⚠️ **Game 级那条 `SpritesNeverTouchHudRows` 只能算冒烟**：HUD 后画会把行 1/2 整行抹掉（`Game.cpp:1481`）⇒ 它**永远绿**，别拿它当"裁剪存在"的证据 |
| 6 | **列/行下标越界** | 贴脸时 `col0` 为负、`row1 > h` | 裁到视口（`fb.Set` 虽忽略越界，但**下标算术**必须先夹，否则 `int` 溢出） |
| 7 | **每帧 `std::vector` 分配** | 帧时间抖动（`T` overlay 的 `Game.Render` 峰值） | 分叉 B 甲：成员复用，宽度变了才 `resize` |
| 8 | **精灵比 `kMinPerp` 还近** | 行数爆成几万行 | `forward <= kMinPerp ⇒ visible = false`（贴脸直接不画，同"墙高夹紧"） |
| 9 | **同一精灵跨多列 ⇒ 数 `E` 会数多** | 用例"屏幕上 `E` 的个数 == 敌人数"**不成立** | 3D 用例只断"**至少出现一次**"，或按"每列最多一个"计数（**别**沿用 2D 那条判据） |
| 10 | **同距离精灵画序不定** | 像素级断言不稳定 | 用例只钉"距离差 > 1 格"的场景 |
| 11 | **`kBulletHeight` 与 2D 的"点是中心"口径冲突** | 3D 里子弹看着"贴地"或"飘太高" | 分叉 D 甲 + 人眼调；常量写在 `Game.cpp` 顶部（和 `kCellHeightPerWidth` / `kAspectTaste` 这些观感旋钮放在一起）。⚠️ `kBulletHeight` 是**中心**高度，`SpritePlace.z` 是**脚底** ⇒ 调用处写 `z = kBulletHeight − cellsTall/2`（D-2 甲） |
| 12 | **`E`/`*` 在 3D 与 2D 的含义漂移** | 有人把 3D 的字形改成别的，判据就两边都要维护 | 分叉 F 甲（沿用）+ 注释写清"同一字形两种视角" |
| 13 | **暂停时精灵还在动** | 破坏"暂停 = 世界冻住" | 精灵**只读**实体位置 ⇒ 世界不动它自然不动（无需额外门控）—— 但要有用例钉 |
| 14 | **3D 下整屏变化让 `Rows` 顶满** | 顶栏 `Rows n/n`（S12 已登记，坑 14） | 不是 bug；顺带成为"3D 真在动"的判据 |
| **15** | **小精灵被取整成 0 行**（2026-10-22 新增，D-4） | 远处的子弹**整颗消失**：`cellsTall ≈ 0.2`、10 格外 `rowsPerU ≈ 1.9 行/格` ⇒ 0.38 行 ⇒ 取整后 0 行 | 与 S12 坑 17 同病：`row1 - row0` **至少 1**；用例 `SpriteAtLeastOneRowVisible` |
| **16** | **3D 里"数 `E`"的三种错误变体**（2026-10-22 新增，F 甲的代价） | 一个敌人跨 ≈10.8 列 ⇒ 屏幕上有十几个 `E`；被墙挡住一半时只剩 5~6 列 ⇒ **个数随墙变化** | ① 只断"**至少出现一次**"；② **禁止**搬 `CollectEnemies` / `MinEnemyDistanceSq` 那类"一个实体 = 一个字符"的 2D 辅助；③ **也别写**"`E` 个数 ≤ 敌人数"（把"必红"换成"偶发红"，且没有信息量） |

---

## 6. 测试计划

### 6.1 新 suite `Sprite`（纯函数，离线，**8 条**）

| # | 用例 | 钉什么 |
| --- | --- | --- |
| 1 | `CenteredSpriteIsCentered` | 正前方（`cameraX = 0`）⇒ `col0/col1` 围绕 `cols/2` 对称、行区间在视口内 |
| 2 | `SpriteShrinksWithDistance` | `perp` 翻倍 ⇒ 行数与列数**都**约减半（单调） |
| 3 | `SpriteStandsOnFloor` | `z = 0` ⇒ `row1` == 该距离下的地面行（**用 `WallHeightRows` 同尺度算**，不许写死行数） |
| 4 | `HeightLiftsSprite` | `z` 增大 ⇒ 行区间**整体上移**、高度不变 |
| 5 | `BehindCameraIsInvisible` | `dot(toCam, dir) <= 0` ⇒ `visible == false` |
| 6 | `WideSpriteSpansMoreColumns` | `cellsWide` 翻倍 ⇒ 列数翻倍（`row` 不变） |
| **7** | `SpriteAtLeastOneRowVisible` | ★ **坑 15**：远处的小精灵（子弹，`cellsTall ≈ 0.2`）**仍至少占 1 行**，不许被取整成 0 行 |
| **8** | `SpriteStaysInsideViewport` | ★ **R4 甲的靶子**：`col ∈ [0, cols)`、`row ∈ [viewTop, viewTop + viewH)` —— **裁剪在纯函数里做**（R4 甲），所以这条能红 |

### 6.2 `Game` 侧新用例（3D，**禁用 `ScreenText`**，**8 条**）

| # | 用例 | 钉什么 |
| --- | --- | --- |
| 1 | `EnemyVisibleIn3D` | 切到 3D 后屏幕上**出现 `E`**（至少一次）—— 这是 S13 的"开箱"判据 |
| 2 | `WallOccludesEnemy` | 敌人放在墙后 ⇒ 3D 里 **`E` == 0**（★ R1 的对照项：R1 时它**假绿**） |
| 3 | `OcclusionUsesPerpNotSlant` | ★ **R2 的靶子**：侧向摆位的精灵**必须露出来**（用斜距会误判成墙后） |
| 4 | `BulletVisibleIn3D` | 开一枪后 3D 里出现 `*`（且在**地面行之上** —— 验证 `z` 的脚底→中心换算） |
| 5 | `NearSpriteWinsOverFarOne` | ★ **R3 的靶子**：距离差 > 1 格时，**近的盖住远的** |
| 6 | `SpritesNeverTouchHudRows` | 行 `< kHudRow+1` 里不出现 `E`/`*`。⚠️ **降级为集成冒烟**：HUD 后画会整行抹掉 ⇒ 它守不住裁剪，真正的守卫是纯函数 `SpriteStaysInsideViewport`（见 R4 行） |
| 7 | `SpritesFreezeWhenPaused` | ⚠️ **改成"暂停后连续两帧逐格相同"**（原写"暂停前后逐格相同"**必红** —— ESC 会叠 `PAUSED` 横幅，`Game.h:359`） |
| 8 | `DepthBufferMatchesRaycaster` | ★ **第 2 笔的"只记不读"守卫**：逐列 `m_colDepth[col] == CastColumn(...).perp` |

### 6.3 不许破的

- `Game` suite 既有 **47 条**（尤其 14 条读屏用例 —— 它们在 2D 下跑，S13 不碰 2D 路径）
- `Raycaster` **10 条**、`Sprite` 新 **8 条**、其余 suite ⇒ 总量 **242 + 16 = 258**
- **每个 commit 都能构建 + 用例全绿**（`AGENTS.md` §7）

---

## 7. 验收

### 7.1 正向（机器 + 人眼）

| 项 | 判据 |
| --- | --- |
| 构建 | **0 warning**（`/W4`） |
| 用例 | **258 passed, 0 failed**（S12 收口 **242** + S13 的 **16**：纯函数 8 + Game 8） |
| 3D 开箱 | 按 `V` 进 3D，**走廊里看得见 `E`**（敌人挡在墙前） |
| 遮挡 | 绕到墙后 ⇒ 敌人**消失**；走到墙前 ⇒ 敌人**重新出现** |
| 子弹 | 开火后能看到 `*` 从`你`飞向对方；飞到墙后**被墙挡住** |
| 不出界 | 贴墙站着时精灵**不糊到 HUD 上**、不越出左右边界 |
| 观感 | 敌人**站在地上**（脚踩地面行），不飘不埋 |

### 7.2 反向验证（**5 场**，每场先写"预测"再实测）

| # | 改坏什么 | 预测（**2026-10-22 按代码重推过，与初稿不同**） |
| --- | --- | --- |
| **R1** | 不写 `m_colDepth`（保持初值 0） | ⚠️ **不是"穿墙可见"，是"精灵全不画"**：`box.perp >= 0` 恒真 ⇒ 每列都 `continue` ⇒ **`EnemyVisibleIn3D` 红**，而 `WallOccludesEnemy`（判据 `E == 0`）**假绿** —— 这是"绿出来的假守卫"，必须**先写进预测**再实测 |
| **R2** | 深度比较把 `perp` 换成 `dist`（斜距） | `OcclusionUsesPerpNotSlant` **红**（侧向摆位：斜距偏大 ⇒ 本该露出的精灵被判成墙后） |
| **R3** | 排序改成升序（近→远） | `NearSpriteWinsOverFarOne` **红**（近的被远的盖住） |
| **R4** | **删掉 `ProjectSprite` 里的行裁剪**（R4 甲：裁剪已下沉进纯函数） | `SpriteStaysInsideViewport` **红**。⚠️ **`SpritesNeverTouchHudRows` 会绿** —— 因为 HUD 后画、把行 1/2 整行抹掉（`Game.cpp:1481`）⇒ 那条 Game 级用例**只能算集成冒烟，不能算守卫** |
| **R5**（新增） | 深度比较方向反过来（`>=` → `<=`） | **整屏精灵消失** ⇒ `EnemyVisibleIn3D` **红**。价值：把"深度比较写了但方向反了"与 R1 的"没写"区分开 —— S12 的教训是"只注掉、不反号"很容易漏掉反向错误 |

> ⚠️ 每场都要**先写下预测**（含"预计绿场"的可能），实测后逐条比对 —— 这是 S9 ~ S11 的规矩。

---

## 8. 人眼验收清单（本阶段合并到最后一步走，别每步碎片化）

1. 进 3D：走廊深处有 `E`（敌人），**大小随距离变化**；
2. 敌人走到墙后：**消失**；绕出来：**重新出现**；
3. 两个敌人在同一条视线上：**近的挡住远的**（不是反的）；
4. 敌人**脚踩地面**（与地平线的关系看起来对）；
5. 开火：`*` 可见，且**飞到墙后就没了**；
6. 贴墙站着：精灵**不盖 HUD**、不出左右边界；
7. `ESC` 暂停：精灵**完全静止**（含子弹）；
8. 切回 2D：画面**与 S12 完全一致**（`E`/`*` 位置、HUD、面板都不变）；
9. 按 `T`（overlay）：`Game.Render` 读数**没有明显变差**（预期 < 0.1 ms 增量）；
10. `F2` 在 3D 下仍然无反应（预期，见 S12 坑 11）。

---

## 9. commit 切分（5 笔，每笔都能构建）

| # | 内容 | 能否独立构建 | 验收 |
| --- | --- | --- | --- |
| 1 | **`render/RayCamera.h`（新头）**：把 `RayCamera` + `kMinPerp` 搬过去；`RayCaster.h` 反向 include 它（`RayHit` 留在原地）→ 再落 `engine/render/Sprite.h` + `tests/test_sprite.cpp`（**8 条**）+ `tests/CMakeLists.txt` **一行** | ✅ **纯搬运 + 纯增量**，无人调用 | 既有 **242 条全绿、一条不红**（= 搬运零行为变化）+ 8 条新用例绿 |
| 2 | `Game`：`m_colDepth` 填值（**无行为变化**）+ 守卫用例 `DepthBufferMatchesRaycaster`（逐列 `m_colDepth[col] == CastColumn(...).perp`） | ✅ 只记不读 | 全绿（0 行为变化）+ 新用例绿 |
| 3 | 精灵绘制 + 遮挡：`CollectSprites`（**身份判定照抄 `DrawEntities`**）+ `DrawSprites`（**先投影、后排序**）+ `EnemyVisibleIn3D` / `WallOccludesEnemy` / `OcclusionUsesPerpNotSlant` | ✅ | 3 条新用例绿 |
| 4 | `z`（**脚底**语义 / `kBulletHeight`）+ 排序**次级键 `EntityId`** + `BulletVisibleIn3D` / `NearSpriteWinsOverFarOne` / `SpritesNeverTouchHudRows` / `SpritesFreezeWhenPaused` | ✅ | 4 条新用例绿 |
| 5 | 文档收口（`PLAN` S13 行、`TECH_DEBT`、`README`、`AGENTS §2`、版本号 `0.6.0-dev`）**+ 打 annotated tag `v0.6.0` 并 push**（`AGENTS.md` §5：机器门 + 人眼都过了才打） | ✅ | — |

> 第 2 笔刻意做成"只记不读" ⇒ 一旦有问题，问题一定在第 3 笔，**不会两笔混在一起**（S7 的教训）。

---

## 10. 文档同步（收口时）

| 文件 | 改什么 |
| --- | --- |
| `doc/MiniEngine_PLAN.md` | S13 行 ✅ + 落地偏差 · v1.0 判定 ① 回填 · B 节"精灵"一段 |
| `doc/TECH_DEBT.md` | **C8**（`Vec3`）现状复核 · 新增"精灵遮挡无机器守卫"若成立（视 R1 结果） |
| `README.md` | 操作表（无新键）· 状态行（用例数 / 3D 里看得见敌人）· 已知限制（"3D 看不见自己"删掉） |
| `AGENTS.md §2` | 本图一行 + S13 进度 |
| `study/RAYCASTING_NOTES.md` | §3「做不到什么」里"看不见敌人"那条 → 划掉并指向 S13 |
| `Version.cpp` / `test_smoke.cpp` | `0.5.0-dev` → `0.6.0-dev`（S13 收口时） |

---

## 11. 风险与待确认

| # | 项 | 状态 |
| --- | --- | --- |
| **T1** | `kWallHeightCells` 取多少 | ✅ **2026-10-22 已拍板 = `2.0f`**（分叉 C = 甲）。⚠️ **语义必须写清**：它**不改变墙的画法**，只定义"**1 格 = 半条墙带**" ⇒ 1 格高的敌人 = **半条墙带高**、头顶**齐地平线**（100×28、`perp=3.5` 时 5.41 行 vs 墙带 10.8 行）。它和 `kCellHeightPerWidth` / `kAspectTaste`（`Game.cpp` 顶部的观感旋钮）是一组 —— **别再引用已删除的 `kCellAspect`** |
| **T2** | 子弹的高度常量取多少 | ✅ **已拍板：叫 `kBulletHeight`，语义 = 子弹中心离地高度**（和一排观感旋钮放一起）。⚠️ `SpritePlace.z` 的语义是**脚底**高度 ⇒ 调用处写 `z = kBulletHeight − cellsTall/2`（D-2 甲）。**取值沿革（三次，两次是人眼反馈）**：① `1.2f`（2026-10-22 拍板）—— 子弹占 `[1.075, 1.325]` 格，**整块在眼高（1 格 = 地平线）之上**，而站立敌人的顶边**正好是地平线** ⇒ 「子弹永远从敌人头顶飞过」（本人原话：不管怎么移动，子弹始终比 E 高）；② `0.8f`（2026-10-26）—— 落回敌人身上，但**低于眼高 ⇒ 越靠近镜头屏幕行越往下掉**（3 格处行 14、1.5 格处行 15）⇒ 「子弹从 E 下面走」；③ **`1.0f`（现行）** —— 中心高度 = 相机高度 ⇒ `row = horizon + (1−z)×lineH/2` 与距离无关 ⇒ **屏幕行恒为地平线**，弹道看起来是直线，且在敌人所在距离上正好压在敌人**头顶行**。⚠️ 连带：子弹与敌人**同处地平线行带** ⇒ 近距离（≤3 格、8 列宽）的子弹会盖住正前方的精灵（`SpritesFreezeWhenPaused` 把敌人摆偏 2 格；`SpritesNeverTouchHudRows` 的"子弹在场"检查挪到摆敌人之前）；`BulletVisibleIn3D` 的判据从「行 < 地平线」改成「行 ∈ [地平线, 地平线+1]」 |
| **T3** | 精灵宽度是否该按 `cellsWide = 0.8`（比一格窄一点，两个敌人并排才分得开） | 待看着调（`cellsWide` 每帧不变，改它只影响横向列数） |
| **T4** | 性能：8 个敌人 + ~20 发子弹 的逐列逐行写入 | 预期 <0.1 ms；用 `T` overlay 的 `Game.Render` 实测 |
| **T5** | 精灵遮挡的**边缘**（半格露头）是否会出现"1 列闪现" | 人眼项 2 的强化观察点；必要时给 `perp` 加一点偏差（**先别做**，等真出现再说） |
| **T6** | 3D 下 `E` 的计数判据（坑 9） | 已定：只断"至少一次"/"每列至多一个" |

---

## 12. 反向验证总账（5 场 · 2026-10-26 实测）

> **流程**：每场先写预测（§7.2）→ 我改**一处** → 本人构建并跑 `mini_tests.exe` → 回报 `[FAIL]` 清单 → 我立刻复原 → 记账。
> **基线**：破坏前 `259 passed, 0 failed`（本人回报）。**5 场的破坏点全部已还原**（`grep -rn 临时破坏` 为空）。

### 12.1 逐场比对

| 场 | 破坏点（实际改的那一处） | 预测 | 实测 | 判定 |
| --- | --- | --- | --- | --- |
| **R1** | `m_colDepth` 初值 `kRayFar` → `0`，且**不写**逐列值 | 9 红 / 250 绿 | **9 红 / 250 绿**，同名逐条命中（`DepthBufferMatchesRaycaster` + 8 条精灵显示用例） | ✅ **完全命中** |
| **R2** | 投影后把 `drawn.box.perp` 覆盖成**斜距** `(item.pos − cam.pos).Length()` | 1 红 / 258 绿 | **1 红**：`OcclusionUsesPerpNotSlant`（`test_game.cpp:1413`） | ✅ 命中（⚠️ 这处同时改了**排序键** —— 口径偏差已记） |
| **R3** | `std::sort` 比较 `>` → `<`（近的先画） | 1 红，且红在**最后一句** `CountChar('*') == 0` | **1 红**，但红在**控制段**（`test_game.cpp:1506`，`CountChar('*') > 0`） | ⚠️ 红集对、**红点错**（预测失误 ①） |
| **R4** | `ProjectSprite` 的**行**边不再夹到视口 | 1 红：`Sprite.SpriteStaysInsideViewport` | **1 红**，`row1 = 34` vs `30`；`Game.SpritesNeverTouchHudRows` **绿** | ✅ 命中（顺带实测了「Game 级守不住裁剪」） |
| **R5** | 深度比较 `perp < depth` → `perp > depth` | 8 红（症状写的是「整屏精灵消失」） | **6 红 / 253 绿**：`WallOccludesEnemy`（含**穿墙**证据 `lhs = 3`）/ `OcclusionUsesPerpNotSlant` / `BulletVisibleIn3D` / `NearSpriteWinsOverFarOne` / `SpritesNeverTouchHudRows` / `SpritesFreezeWhenPaused` | ❌ **机制与红集都错**（预测失误 ②③） |

### 12.2 三处预测失误（如实记录）

1. **R3 的红点在「控制段」，不在最后一句。** 反序后，**巡逻敌人**（垂距 ≈39）与那颗远子弹（22.5）落在**同一格**（列 39、行 13），且比子弹**更远** ⇒ 它们**后画**、把 `*` 盖掉 ⇒ 控制段先说红。
   ⇒ 这条用例的控制段**本身也带画序信息量**（我原以为它只证明「子弹画得出来」）。
2. **R5 的症状说反了。** 我预测「整屏精灵消失」，实际是「**墙后的精灵反而被画出来**（穿墙可见）+ 墙前的全消失」—— 因为 `perp > depth` 恰好是「精灵比墙更远」。施工图 §7.2 的 R5 那一行也写错了，**以实现与实测为准**。
3. **`EnemyVisibleIn3D` / `SpritesDisappearWhenFacingAway` 在 R5 下「该红却绿」。** 它们只断「屏幕上至少有一个 `E`」；反序时**墙后的巡逻敌人漏了出来**，正好顶替了被藏起来的那个敌人 ⇒ 假绿。
   ⇒ **坑 16 的代价在此具体化**：3D 里「至少一次 `E`」是**很弱的判据**（能证明「有东西被画」，不能证明「对的东西被画」）。R5 真正抓住它的是 `WallOccludesEnemy`（它同时有「墙前必须画出来」的对照句和「墙后必须为 0」的判据）。

### 12.3 结论

- **契约层**：R1（不写深度）/ R2（斜距）/ R3（反序）/ R4（不裁剪）/ R5（反方向）**每一场都至少有一条用例真的红了** ⇒ 这四处机制**都有机器守卫**，不是「绿着没人守」。
- **分辨力**：R1 比 R5 多红一条 `DepthBufferMatchesRaycaster`（数组值 vs 比较逻辑分开可观测）⇒「**没写**」与「**写了但方向反了**」在用例上**可分** —— 这正是 R5 存在的理由。
- **已知薄弱**：`EnemyVisibleIn3D`（以及 `SpritesDisappearWhenFacingAway`）的 `E > 0` 判据会被「别处漏出来的敌人」满足。**本笔未改**：要不要收紧成「**指定列带内 `E > 0` 且列带外 `E == 0`**」由本人定 —— 收紧后 R5 会多红一条，但判据会与巡逻敌人的实际位置耦合（那正是坑 16 里禁止的做法）。

---

## 13. 收口记录（2026-10-26）

| 项 | 结果 | 出处 |
| --- | --- | --- |
| 机器门 | **259 passed, 0 failed**（`Game` 56 / `Sprite` 8 / `Raycaster` 10 / 其余 185） | 本人回报 |
| 反向验证 | **5 场**，每场都有用例真红；含 **3 处预测失误** | §12 |
| 人眼 | ✅ 通过（墙后遮挡、子弹高度是本人反复盯的两条；后者连带把 `kBulletHeight` 定到 **1.0**） | 本人回报 |
| 版本号 | `0.5.0-dev` → **`0.6.0-dev`**（四处功能落点 + `Version.h` 那句举例注释） | `Version.cpp` / `test_smoke.cpp` |
| tag | ⏳ **`v0.6.0` 待打**（annotated + `git push origin v0.6.0`，打完 `git tag -n1` 核一眼） | `AGENTS.md` §5 |

**收口时同步的文档**（5 处）：`PLAN`（S13 里程碑 + 落地偏差 5 条 + v1.0 的 ① 回填 ✅）· `README`（状态行 / `V` 键说明 / 目录 / 已知限制 / 深读入口）· `AGENTS`（§2 索引 + §5 的 `v0.6.0` 门槛行）· `TECH_DEBT`（C44 `SpawnEnemyAt` / C45 画序窗口）· `study/RAYCASTING_NOTES`（"看不见敌人"那条划掉）。

**S13 之后 v1.0 只剩两条**（都是 S14 的事）：② README + GIF + CI ③ 干净克隆后三条命令能跑。

---

> **一句话**：S13 只是**加一个按列的深度数组 + 一个纯函数的投影**，但它把 3D 从"能走"变成"能打" —— 这也是 `PLAN` 里 v1.0 的第一条。
