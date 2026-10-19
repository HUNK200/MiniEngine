# S13 施工图 —— Sprite + Z-buffer（伪 3D 里"看得见东西"）· v1.0 的第 ① 条

> **状态（2026-10-16 落纸）**：**未开工**。六个分叉**待拍板**（§2）。
> **前置**：S12 的伪 3D 已落地（第 1 ~ 4 笔；第 4 笔的机器门/人眼待本人验证）。
> ⚠️ 本文是**方案层**：只写接口、伪代码、坑表、验收；不含可编译的实现。

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

## 1. 前置事实（动手前先核这 8 条）

| # | 事实 | 出处 | 对 S13 的含义 |
| --- | --- | --- | --- |
| 1 | `DrawRaycastWorld` 每列算了 `hit.perp`，**但用完就扔** | `Game.cpp`（S12 第 4 笔） | S13 要把它**留下来** ⇒ 列深度数组 |
| 2 | 每列**只画最近那面墙** ⇒ 现在不需要深度 | 同上（`S12_CHECKLIST` §2 第 6 步） | 一旦加精灵，这个"免费遮挡"就**必须**换成显式深度比较 |
| 3 | `WallHeightRows(perp, viewH, aspect) = viewH/perp*aspect`，且 `≥ 1 行` | `engine/render/RayCaster.h` | 精灵的**垂直尺寸必须复用同一个缩放**（否则敌人"飘在空中"/"埋在墙里"） |
| 4 | 视口 = 行 `kHudRow .. fb.Height()-1`，地平线 = `kHudRow + viewH/2` | `Game.cpp` | 精灵**不许画到 HUD 行上** |
| 5 | 实体是**点模型**（`Entity{id, transform.pos, symbol}`），没有尺寸/朝向 | `world/Entity.h`（S5 起） | 精灵的宽高只能是**约定值**（1 格宽 × `kSpriteCellsTall` 格高） |
| 6 | 敌人死 = **真删实体**（`S9`） | `Game.cpp` `FlushRemovals` | 3D 里死敌人**自动消失**（与 2D 一致，不用额外判断） |
| 7 | 2D 的 `E`/`*`/`@` 是**被计数的字形**（C31 纪律） | `TECH_DEBT` C31 | 3D 下画 `E`/`*` **不冲突**（两个模式不同帧共存）⇒ **沿用同一套字形**，见分叉 F |
| 8 | `PLAN:1203` 已经预告过这件事 | "墙的渲染天然产生了按列深度数组……z-buffer 有 320 项，比 GPU 的 2D 缓冲区小 5 个数量级，但**算法一模一样**" | 本阶段就是把它做出来 ✓ |

---

## 2. 六个分叉（**待拍板**）

| # | 分叉 | 选项 | 建议 | 理由 |
| --- | --- | --- | --- | --- |
| **A** | 精灵的**投影数学**放哪 | 甲 `engine/render/Sprite.h`（header-only 纯函数，同 S12 的 C=甲）· 乙 全塞 `Game.cpp` | **甲** | 可离线单测；`engine/render` 已有 `RayCaster.h` 先例 |
| **B** | **列深度数组**的所有权 | 甲 `Game` 成员复用（宽度变了才 `resize`）· 乙 每帧局部 `std::vector` · 丙 固定上限栈数组 | **甲** | 乙每帧分配；丙要定死上限（窗口可拉到几百列） |
| **C** | 精灵的**世界尺寸**基准 | 甲 先声明 `kWallHeightCells = 2.0`（"一面墙算两格高"），精灵尺寸从它推 · 乙 直接给精灵一个魔数行数 | **甲** | 墙高公式里那个"单位"现在是**隐式**的；不显式化就没法把精灵和墙对齐 |
| **D** | 精灵的**高度（z）** | 甲 加 `z`（子弹在空中 ≈1.2 格，敌人 0）· 乙 全部贴地 | **甲** | 否则子弹看起来贴地飞（观感差且和 2D 的"点是中心"口径不一致） |
| **E** | 精灵之间的**画序** | 甲 每帧 `std::sort` 按 `perp` **降序**（远→近）· 乙 按实体表顺序（不定） | **甲** | ≤ 10 个元素，排序成本可忽略；不排会出现"近的敌人被远的盖住" |
| **F** | 3D 下精灵用**什么字形** | 甲 沿用 `E`/`*`（跨模式语义一致）· 乙 3D 专用字形（如 `¤`/`·`） | **甲** | 同一串字符在两种视角下含义相同；3D 用例可以"数 `E`" |

---

## 3. 接口设计

### 3.1 `engine/include/mini/render/Sprite.h`（新，header-only 纯函数）

```cpp
namespace mini::render {

// 输入：一个"贴在世界上某处、有高度 z"的精灵（**只读**，不认识 Entity/game）
struct SpritePlace {
    math::Vec2 pos{};              // 世界坐标（格，含亚格精度）
    float      z = 0.0f;           // 离地高度（格）；0 = 站在地上
    float      cellsTall = 1.0f;   // 世界高度（格）；1 = 一格高
    float      cellsWide = 1.0f;   // 世界宽度（格）
};

// 输出：落到屏幕上的矩形（**半开区间**：列 [col0, col1)、行 [row0, row1)）+ 深度
struct SpriteBox {
    int   col0 = 0, col1 = 0;
    int   row0 = 0, row1 = 0;
    float perp = 0.0f;             // ★ 与墙**同一个**深度口径（垂距），用于比较
    bool  visible = false;         // false = 在相机背后 / 贴脸 / 完全不可见
};

// 唯一入口：世界 → 屏幕（**不认识** FrameBuffer，也不画）
[[nodiscard]] SpriteBox ProjectSprite(const SpritePlace& place, const RayCamera& cam,
                                      int cols, int viewH, int horizon,
                                      float wallHeightCells) noexcept;

} // namespace mini::render
```

**约定（和 `RayCaster.h` 对齐）**
- `perp` **必须**用与墙同一个定义（`RayHit::perp`，即沿 `dir` 的垂距）⇒ 否则"墙 vs 精灵"的比较没有意义；
- 相机背后（`dot(toSprite, dir) <= 0`）⇒ `visible = false`（**不做**"绕过背后"的裁剪）；
- 输入不 clamp，**输出必须有限**（贴脸 ⇒ 走 `kMinPerp`，行数可能远超视口 ⇒ 由调用方裁）。

### 3.2 `game` 侧接线（只碰 `Game.h` / `Game.cpp`）

```cpp
// Game.h（私有）
std::vector<float> m_colDepth;          // 分叉 B 甲：复用，宽度变了才 resize
struct SpriteItem { mini::math::Vec2 pos; float z; float cellsTall; float cellsWide;
                    char32_t glyph; mini::render::Color color; };
void CollectSprites(std::vector<SpriteItem>& out) const;   // 敌人 + 子弹（按当前状态）
void DrawSprites(mini::render::FrameBuffer& fb, const mini::render::RayCamera& cam,
                 int viewH, int horizon);                  // 分叉 E 甲：内部排序
```

`DrawRaycastWorld` 变成**两趟**：
1. 画墙，并把 `hit.perp` 写进 `m_colDepth[col]`；
2. `CollectSprites` → 排序 → `DrawSprites`（每列与 `m_colDepth[col]` 比）。

---

## 4. 算法步骤（伪代码）

```text
# ① 墙（S12 已有，只加一行"记深度"）
for col in [0, cols):
    hit = CastColumn(map, cam, col, cols)
    m_colDepth[col] = hit.hit ? hit.perp : kRayFar      # ← 新增
    画墙条（原有逻辑不动）

# ② 收集 + 排序（远 → 近）
sprites = CollectSprites()                              # 敌人：z=0、1×1 格；子弹：z=kBulletZ、小一圈
sort(sprites, by perp DESC)                             # 分叉 E 甲

# ③ 逐个精灵：先用纯函数算出屏幕矩形，再**逐列**比深度
for s in sprites:
    box = ProjectSprite(s, cam, cols, viewH, horizon, kWallHeightCells)
    if not box.visible: continue
    裁到视口：col ∈ [0,cols)、row ∈ [kHudRow, h)     # 坑 5：不许盖 HUD
    for col in [box.col0, box.col1):
        if box.perp >= m_colDepth[col]: continue      # ★ 墙更近 ⇒ 这一列跳过（遮挡）
        for row in [box.row0, box.row1):
            fb.Set(col, row, s.glyph, s.color)
```

**投影公式（`ProjectSprite` 内部，`cells` 单位）**

> ⚠️ **2026-10-16 复核后更正（初版横向公式写错了，见坑 4b）**：**尺寸**一律从 `lineH`（该距离下"一整面墙"的行数）推；**只有定位**（`centerCol`）才用几何公式。
> 为什么：`kCellAspect` 是**观感常数**，而"几何一致"的纵横比是 `k* = cols / (2·fovTan·viewH·kCellAspect)` —— **它依赖窗口宽度**（100×28 时 ≈1.35，60×28 时 ≈0.81）⇒ **不存在一个对所有窗口都"物理正确"的常数**。所以墙与精灵必须**共用一个（会偏的）尺度**，这样两者**至少彼此一致**。
```text
toCam  = place.pos - cam.pos
forward = dot(toCam, cam.dir)                     # 沿 dir 的垂距（与 RayHit::perp 同口径）
if forward <= kMinPerp: visible = false            # 相机背后 / 贴脸

lineH    = viewH / forward * kCellAspect           # ★ 与画墙**同一个式子**（必须）
rowsPerU = lineH / wallHeightCells                 # 1 个世界格 = 多少行
rowsTall = place.cellsTall * rowsPerU
colsWide = place.cellsWide * rowsPerU * kCellAspect  # ★ 物理上"方" ⇒ 列数 = 行数 × 字符格的宽高比
floorRow = horizon + lineH / 2                     # 地面线（= 墙脚）
row1     = floorRow - place.z * rowsPerU           # 高度 z 把精灵整体抬高
row0     = row1 - rowsTall

# 只有**横向位置**用几何（fovTan）—— 否则精灵和墙在屏幕上对不齐
centerCol = cols/2 + (dot(toCam, plane) / forward) * (cols / (2 * cam.fovTan))
col0 = centerCol - colsWide/2 ; col1 = col0 + colsWide
```

**算例（100×28、`kCellAspect=2`、`wallHeightCells=2`、`perp=3.5`；`horizon=16`）**

| 量 | 值 | 说明 |
| --- | --- | --- |
| `lineH` | **16 行** | 那面墙在 3.5 格处占 16 行 ⇒ 上下各 8 行（`8..24`） |
| `rowsPerU` | **8 行/格** | 16 ÷ 2 |
| 1 格高精灵 | **8 行** | 站在地上 ⇒ 占 `16..24`（顶正好在地平线 = 相机高度 ✓ 自洽） |
| 1 格宽精灵 | **16 列** | `1 × 8 × 2` ⇒ 物理 16w × 16w = **方** ✓ |
| 若误用几何横向公式 | 21.6 列 | `100/(2·0.66·3.5)` ⇒ 物理 21.6w × 16w ⇒ **胖 1.35×** ✗（坑 4b） |

---

## 5. 坑表（按"会出什么现象"排）

| # | 坑 | 症状 / 判据 | 对策 |
| --- | --- | --- | --- |
| 1 | **忘了写 `m_colDepth`** | 精灵**永远**画在墙前面（穿墙可见） | 用例 `SpriteBehindWallIsOccluded`（3D 里数 `E` == 0） |
| 2 | **深度用 `dist`（斜距）而不是 `perp`** | 屏幕两侧的精灵**误判遮挡**（明明在墙后被判成墙前，或反之） | 只用 `perp`；用例 `OcclusionUsesPerpNotSlant`（侧向摆位） |
| 3 | **从近往远画** | 远处的敌人盖住近处的（"人越远越大"） | 分叉 E 甲：降序排序；用例 `NearSpriteWinsOverFarOne` |
| 4 | **精灵的垂直尺度与墙不一致** | 敌人**飘在空中**或**埋进地板**；改 `kCellAspect` 后更明显 | 分叉 C 甲：`kWallHeightCells` 显式化；用例断"贴地精灵的底边 == 地面行" |
| **4b** | **横向尺寸用几何公式（`cols/(2·fovTan·perp)`），不是墙的尺度** | 精灵**横向胖**（像块饼）；100×28 下实测差 **1.35×** | ⚠️ 2026-10-16 复核发现：**施工图初版写错的就是这条**。尺寸必须**从 `lineH` 推**（见 §4 更正），几何公式**只用来定位**（`centerCol`） |
| 5 | **画到 HUD 行上** | HUD 第二行出现 `E`/`*`（玩家读不到 `hp`） | 行区间裁到 `[kHudRow, h)`；用例 `SpritesNeverTouchHudRows` |
| 6 | **列/行下标越界** | 贴脸时 `col0` 为负、`row1 > h` | 裁到视口（`fb.Set` 虽忽略越界，但**下标算术**必须先夹，否则 `int` 溢出） |
| 7 | **每帧 `std::vector` 分配** | 帧时间抖动（`T` overlay 的 `Game.Render` 峰值） | 分叉 B 甲：成员复用，宽度变了才 `resize` |
| 8 | **精灵比 `kMinPerp` 还近** | 行数爆成几万行 | `forward <= kMinPerp ⇒ visible = false`（贴脸直接不画，同"墙高夹紧"） |
| 9 | **同一精灵跨多列 ⇒ 数 `E` 会数多** | 用例"屏幕上 `E` 的个数 == 敌人数"**不成立** | 3D 用例只断"**至少出现一次**"，或按"每列最多一个"计数（**别**沿用 2D 那条判据） |
| 10 | **同距离精灵画序不定** | 像素级断言不稳定 | 用例只钉"距离差 > 1 格"的场景 |
| 11 | **`kBulletZ` 与 2D 的"点是中心"口径冲突** | 3D 里子弹看着"贴地"或"飘太高" | 分叉 D 甲 + 人眼调；常量写在 `Game.cpp` 顶部（和 `kCellAspect` 放在一起） |
| 12 | **`E`/`*` 在 3D 与 2D 的含义漂移** | 有人把 3D 的字形改成别的，判据就两边都要维护 | 分叉 F 甲（沿用）+ 注释写清"同一字形两种视角" |
| 13 | **暂停时精灵还在动** | 破坏"暂停 = 世界冻住" | 精灵**只读**实体位置 ⇒ 世界不动它自然不动（无需额外门控）—— 但要有用例钉 |
| 14 | **3D 下整屏变化让 `Rows` 顶满** | 顶栏 `Rows n/n`（S12 已登记，坑 14） | 不是 bug；顺带成为"3D 真在动"的判据 |

---

## 6. 测试计划

### 6.1 新 suite `Sprite`（纯函数，离线，6 条）

| # | 用例 | 钉什么 |
| --- | --- | --- |
| 1 | `CenteredSpriteIsCentered` | 正前方（`cameraX = 0`）⇒ `col0/col1` 围绕 `cols/2` 对称、行区间在视口内 |
| 2 | `SpriteShrinksWithDistance` | `perp` 翻倍 ⇒ 行数与列数**都**约减半（单调） |
| 3 | `SpriteStandsOnFloor` | `z = 0` ⇒ `row1` == 该距离下的地面行（与 `WallHeightRows` 同尺度） |
| 4 | `HeightLiftsSprite` | `z` 增大 ⇒ 行区间**整体上移**、高度不变 |
| 5 | `BehindCameraIsInvisible` | `dot(toCam, dir) <= 0` ⇒ `visible == false` |
| 6 | `WideSpriteSpansMoreColumns` | `cellsWide` 翻倍 ⇒ 列数翻倍（`row` 不变） |

### 6.2 `Game` 侧新用例（3D，**禁用 `ScreenText`**，5 条）

| # | 用例 | 钉什么 |
| --- | --- | --- |
| 1 | `EnemyVisibleIn3D` | 切到 3D 后屏幕上**出现 `E`**（至少一次）—— 这是 S13 的"开箱"判据 |
| 2 | `WallOccludesEnemy` | 敌人放在墙后 ⇒ 3D 里 **`E` == 0**（★ 反向验证 R1 的靶子） |
| 3 | `BulletVisibleIn3D` | 开一枪后 3D 里出现 `*`（且在**地面行之上**） |
| 4 | `SpritesNeverTouchHudRows` | 行 `< kHudRow+1`（HUD 两行）里**不出现** `E`/`*` |
| 5 | `SpritesFreezeWhenPaused` | `ESC` 暂停前后 3D 屏**逐格相同**（含精灵） |

### 6.3 不许破的

- `Game` suite 既有 **45 条**（尤其 14 条读屏用例 —— 它们在 2D 下跑，S13 不碰 2D 路径）
- `Raycaster` **10 条**、`Sprite` 新 6 条、其余 suite
- **每个 commit 都能构建 + 用例全绿**（`AGENTS.md` §7）

---

## 7. 验收

### 7.1 正向（机器 + 人眼）

| 项 | 判据 |
| --- | --- |
| 构建 | **0 warning**（`/W4`） |
| 用例 | 全量 **≥ 250 passed, 0 failed**（S12 收口时 240 + S13 的 11） |
| 3D 开箱 | 按 `V` 进 3D，**走廊里看得见 `E`**（敌人挡在墙前） |
| 遮挡 | 绕到墙后 ⇒ 敌人**消失**；走到墙前 ⇒ 敌人**重新出现** |
| 子弹 | 开火后能看到 `*` 从`你`飞向对方；飞到墙后**被墙挡住** |
| 不出界 | 贴墙站着时精灵**不糊到 HUD 上**、不越出左右边界 |
| 观感 | 敌人**站在地上**（脚踩地面行），不飘不埋 |

### 7.2 反向验证（4 场，每场先写"预测"再实测）

| # | 改坏什么 | 预测 |
| --- | --- | --- |
| **R1** | 不写 `m_colDepth`（保持初值 0） | `WallOccludesEnemy` **红**（敌人穿墙可见） |
| **R2** | 深度比较换成 `dist`（斜距） | `OcclusionUsesPerpNotSlant` **红**（侧向场景误判） |
| **R3** | 排序改成升序（近→远） | `NearSpriteWinsOverFarOne` **红** |
| **R4** | 行区间不裁到 `kHudRow` | `SpritesNeverTouchHudRows` **红** |

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
| 1 | `engine/render/Sprite.h` + `tests/test_sprite.cpp`（6 条）+ `tests/CMakeLists.txt` | ✅ 纯增量、无人调用 | 6 条新用例绿 |
| 2 | `Game`：`m_colDepth` 填值（**无行为变化**）+ 一条"depth == perp"的守卫用例 | ✅ 只记不读 | 全绿（0 行为变化） |
| 3 | 精灵绘制（含遮挡）+ `EnemyVisibleIn3D` / `WallOccludesEnemy` | ✅ | 两条新用例绿 |
| 4 | `z` 高度 + 排序 + `BulletVisibleIn3D` / `NearSpriteWinsOverFarOne` / `SpritesNeverTouchHudRows` / `SpritesFreezeWhenPaused` | ✅ | 全绿 |
| 5 | 文档收口（`PLAN` S13 行、`TECH_DEBT`、`README`、`AGENTS §2`、版本号）**+ 打 annotated tag `v0.6.0` 并 push**（`AGENTS.md` §5：机器门 + 人眼都过了才打；录 demo / 回退靠它） | ✅ | — |

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
| **T1** | `kWallHeightCells` 取 2.0 是否合适（决定"一面墙看起来多高"） | 待**看着调**；没有理论值，和 `kCellAspect` 是一对 |
| **T2** | `kBulletZ` 取多少（≈1.2 格？） | 待看着调 |
| **T3** | 精灵宽度是否该按 `cellsWide = 0.8`（比一格窄一点，两个敌人并排才分得开） | 待看着调 |
| **T4** | 性能：8 个敌人 + ~20 发子弹 的逐列逐行写入 | 预期 <0.1 ms；用 `T` overlay 的 `Game.Render` 实测 |
| **T5** | 精灵遮挡的**边缘**（半格露头）是否会出现"1 列闪现" | 人眼项 2 的强化观察点；必要时给 `perp` 加一点偏差（**先别做**，等真出现再说） |
| **T6** | 3D 下 `E` 的计数判据（坑 9） | 已定：只断"至少一次"/"每列至多一个" |

---

> **一句话**：S13 只是**加一个按列的深度数组 + 一个纯函数的投影**，但它把 3D 从"能走"变成"能打" —— 这也是 `PLAN` 里 v1.0 的第一条。
