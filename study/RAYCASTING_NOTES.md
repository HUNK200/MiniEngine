# RAYCASTING_NOTES.md —— 我们这套「伪 3D」是什么、业界有哪些经典案例、以后能往哪升级

> **起因**：2026-10-16（S12 第 4 笔「3D 接线」之后）本人的问题：
> *"目前实现 3D 的方式是一般游戏的实现方式么？我的理解是经过几个固定的旋转变换，而不是类似于 DOOM 那样的伪 3D。"*
> 本文是那次问答的**落纸版**，供**后续继续学习**用。
>
> ⚠️ 本文**不改代码、不进构建**（纯笔记）；权威内容请读 §7 的原文，本文只是"指路 + 对照我们自己的实现"。

---

## 0. 三句话结论

1. 我们做的是 **grid ray casting**（**Amanatides & Woo 的 DDA** 格步进 + **每列一条射线**）⇒ 它**就是货真价实的伪 3D**，属伪 3D 家族里**最简单**的那一支（Wolfenstein 档）。
2. "**经过几个固定的旋转变换**"（model → view → projection 矩阵 → 裁剪 → 光栅化 + z-buffer）是**真 3D 管线**；我们**没走那条** —— `math/Vec3.h` 至今只有契约（`TECH_DEBT` **C8**）。
3. **DOOM 也是伪 3D**，但比我们**高一档**（BSP + segs + visplane）。所以"伪 3D"不是"真 3D 的反面"，而是一整个技术家族。

---

## 1. 三档摆开（右边一列是我们在哪一档）

| 档 | 技法 | 能做 / 不能做 | 我们 |
| --- | --- | --- | --- |
| **① Wolfenstein 式 ray casting** | 每列一条射线 + 网格 DDA + `墙高 ∝ 1/垂距` | ✅ 轴对齐等高墙 · 自由转向 · 透视正确 · 地板/天花板只能刷纯色<br>❌ 斜墙 · 高低差 · 抬头低头 · 真地板贴图 | ✅ **就是这个** |
| **② DOOM 式 2.5D** | 墙 = **任意角度的线段**（segs），BSP 树决定画序，地板/天花板走 **visplane**（逐行扫） | ✅ 斜墙 · 不同房间高度 · 纹理地面<br>❌ 仍不是真 3D（楼上楼下做不到） | ❌ 没做，也没排期 |
| **③ 真 3D 变换管线** | 顶点 → model/world → view → projection → 裁剪 → NDC → **光栅化 + z-buffer** → 着色 | ✅ 任意几何 · 任意视角 · 真光照 | ❌ 没做（缺 `Vec3`/`Mat4`/三角形光栅器） |

**我们世界观的由来**：整套引擎是"**网格 + 点模型**"（S5~S7：`TileMap` + `IsSolidAt` + "越界=墙"）⇒ 射线投射是**唯一**能白拿这套语义的 3D 方案（不用新写第二份"什么是墙"）。

---

## 2. 我们的算法逐步（对着代码看）

### 第 0 步：一帧一次的三样输入
`Game::DrawRaycastWorld`（`game/src/Game.cpp`，S12 第 4 笔新增）：
```
cols    = fb.Width()             // 屏幕列数（终端宽度）
viewH   = fb.Height() - kHudRow  // 可用视口高（100×30 时 = 28；HUD 占掉上面两行）
cam     = { player.pos, CameraDir(), kFovTan }   // 位置 / **单位方向** / 视野半宽
horizon = kHudRow + viewH / 2    // 地平线所在的行
```

### 第 1 步：相机平面 —— 全流程里**唯一**的"旋转"
```cpp
const Vec2 plane{-cam.dir.y, cam.dir.x};   // RayCaster.h：把 dir 逆时针转 90°
```
`plane` 是单位向量（因为 `dir` 是单位向量）。它只为一件事服务：让每个 `rayDir` 满足

> **`rayDir · dir == 1`**（恒等，与列号无关）

★ **这条恒等式是"鱼眼校正免费"的全部秘密** —— 见第 4 步。

### 第 2 步：每列一条射线（"视锥"）
```cpp
const float cameraX = 2.0f * col / cols - 1.0f;              // ∈ [-1, +1]
const Vec2  rayDir{cam.dir.x + plane.x * cam.fovTan * cameraX,
                   cam.dir.y + plane.y * cam.fovTan * cameraX};
```
- `cameraX = −1` 最左列、`+1` 最右列、`0` 正中列；`fovTan = 0.66` 就是"相机平面半宽"（≈66° FOV）。
- 真 3D 里这一步是 **view + projection 矩阵**；我们这里就是**两个乘法**。
- ⚠️ 我们踩过的坑：`cols` 为**奇数**时取不到 `cameraX == 0`（零点只落在 `col = cols/2`）⇒ 已用 `static_assert` 钉住。

### 第 3 步：DDA 网格步进（算法主体）
```
mapX = floor(pos.x) ; mapY = floor(pos.y)            // floor（不是 (int) 截断：负数下会错一格）

deltaDistX = |1 / rayDir.x| ; deltaDistY = |1 / rayDir.y|    // "走多远参数距离跨一格"
     ⚠️ 分量为 0（正东/正南）时用 1e30，**不要**用 inf：inf×0 = NaN，后面的比较全毁

stepX = sign(rayDir.x) ; stepY = sign(rayDir.y)
sideDistX = (stepX > 0 ? (mapX + 1 - pos.x) : (pos.x - mapX)) * deltaDistX   // 到**下一条格线**
sideDistY = 同理

循环（上限 W + H 步，防死循环）:
    若 sideDistX < sideDistY → 前进一格 X（记为"命中东西向墙面"），sideDistX += deltaDistX
    否则                    → 前进一格 Y（记为"命中南北向墙面"），sideDistY += deltaDistY
    若 IsSolidAt(mapX, mapY) → 停
```
三个要点：
- **"越界 = 墙"是白拿的**：`IsSolidAt` → `TileMap::At`，越界返回 `Wall`（S7 定的口径）⇒ 射线**必然终止**，不需要额外的边界分支（另有 `W+H` 步上限兜底）。
- **参数 `t` 的含义**：射线走了多少个 `rayDir` 长度（因为 `deltaDist = |1/rayDir.分量|`，累加出来的 `sideDist` 正好是这个参数）。
- **一次只跨一格**：每次迭代 = 两次浮点比较 + 一次加法 ⇒ 这是它比"光线与物体求交"便宜的根本原因。

### 第 4 步：距离 —— 垂距（**鱼眼校正就发生在这里**）
```cpp
hit.perp = t;                // ★ 沿 dir 的垂距（画墙高**只用它**）
hit.dist = t * |rayDir|;     // 欧氏斜距（留给"按真距离衰减"：雾/火把那一类）
```
为什么 `perp` 能直接取 `t`：因为 `plane ⊥ dir` 且 `|dir| = 1` ⇒ `rayDir · dir == 1`
⇒ 从相机到命中面的**沿 dir 分量** = `t × (rayDir · dir)` = **`t`**（它同时也 = 欧氏斜距 × cos(夹角)）。

⚠️ 所以**不能再乘一次 cos** —— 那是**过校正**，画面会凹进去。
经典教程的等价写法：`perpWallDist = (mapX − pos.x + (1−stepX)/2) / rayDir.x`；或"先算欧氏距离再乘 cos"。三者数学等价，我们这种少一次 sqrt/除法。

### 第 5 步：投影成屏幕上的**竖直条**
```cpp
lineH  = WallHeightRows(hit.perp, viewH, kCellAspect);   // = viewH / perp × aspect（且 ≥ 1 行）
top    = horizon - floor(lineH / 2);
bottom = top + ceil(lineH);
shade  = ShadeIndex(hit.perp);                            // 0 = 最近最亮
for (row = max(top, kHudRow) .. min(bottom, h) - 1)
    fb.Set(col, row, kShades[shade], kShadeColors[shade]);
```
- 真 3D 里这一步是"三角形光栅化 + 透视除法"；我们这里就是**一个除法** + 一个行区间。
- 为什么是**等宽竖直矩形**（不是梯形）：墙**等高**且轴对齐。
- `kCellAspect ≈ 2.0` 是补偿"终端字符格 ≈ 1:2"的**观感参数**（**不是**物理量，换字体可能要调）。

### 第 6 步：遮挡 —— **为什么不需要 z-buffer**
每个屏幕列**只画最近的那一面墙**（射线停在哪，画到哪）⇒ 前后关系**免费**得到。
⇒ 这正是 S13 的存在理由：**一旦要画敌人/子弹/道具（精灵），"每列只画一次"就不成立了**，必须"按列比深度、近了才画"。
  `PLAN:1203` 原话：那个 z-buffer 只有 **320 项**，"比 GPU 的 2D 缓冲区小 5 个数量级，但**算法一模一样**"。

### 第 7 步：成本
`O(cols × 平均跨格数)`。116 列 × 十几格 ≈ **一千次"判一格"**/帧 —— 这就是终端里能跑 60fps（`Work` 0.3ms 量级）的原因。
真 3D 光栅化是"每格一次求交/着色"（一屏 3480 格）⇒ 量级不同。

### 第 8 步：三条数值约定（都踩过、都落档了）
| # | 约定 | 不遵守会怎样 |
| --- | --- | --- |
| 1 | `dir` **必须是单位向量**（调用方保证，本文件不替你归一化） | `perp = t` 的推导失效 ⇒ 距离系统性偏 |
| 2 | `deltaDist` 的分量为 0 时用**大数**，不用 `inf` | `inf × 0 = NaN` ⇒ 比较全乱、命中随机 |
| 3 | `perp` 夹到 `kMinPerp` · 墙高**至少 1 行** · 取整只做一次（半高 floor、总高 ceil） | 贴脸除零/溢出；远墙"消失"或"呼吸"（差一行） |

---

## 3. 这一档**做不到**什么（边界 + 谁负责补）

| 做不到 | 原因 | 谁来补 |
| --- | --- | --- |
| 斜墙、高低差、楼梯 | 世界是轴对齐网格；墙高公式只吃"垂距"，不含 y 信息 | 不做（属 DOOM 那一档） |
| 抬头/低头、上下飞 | 没有 pitch，也没有"视线在屏幕上的垂直偏移" | 不做（真 3D 才有） |
| 纹理地板/天花板 | 真纹理要**逐行**求交（visplane 那一族） | S12 第 5 笔只刷**纯色** |
| ~~看见敌人/子弹/道具~~ ✅ **已做（2026-10-26）** | 需要精灵 + 列深度比较 | ✅ **S13 收口**：`render/Sprite.h` 投影 + `Game::m_colDepth` 按列比深度（见 `S13_CHECKLIST` §12） |
| 真正的三维体（楼上楼下） | 这已经是"伪 3D"的**定义边界**（DOOM 也做不到） | 真 3D（§6） |

---

## 4. 业界经典案例（年代表）

| 年代 | 案例 | 与我们这套的关系 |
| --- | --- | --- |
| 1968 | **Arthur Appel**《Some Techniques for Shading Machine Renderings of Solids》 | ★ **"ray casting" 这个词的起源**（当时目的是**隐藏面消除**，不是游戏） |
| 1973–74 | **Maze War**（Imlac）/ **Spasim**（PLATO） | 最早的 FPS 雏形：网格迷宫 + 透视。⚠️ 但画法**不是** DDA 射线（是变换后的线段/点阵）—— 常被误归为同一支 |
| 1980 | **Battlezone**（Atari） | 矢量线框伪 3D（另一条技术路线） |
| **1987** | **Amanatides & Woo**《A Fast Voxel Traversal Algorithm for Ray Tracing》 | ★ **我们 DDA 那一份算法的正式出处** |
| **1992** | **Wolfenstein 3D**（id Software，John Carmack） | ★ **把"每列一条射线 + 网格步进 + 竖直画墙"带火的经典案例**，"伪 3D"这个词的来源场景（Carmack 在 `wl_draw.c` 里就是逐格跨；⚠️ 严格说它是"按列跨格"的增量做法，与标准 DDA 同族） |
| 1992 | **Comanche: Maximum Overkill**（NovaLogic，"Voxel Space"） | 高度场**射线步进**（ray marching）—— 同族不同支（地形，不是网格） |
| 1993 | **Doom**（id） | **升级一档**：BSP 树 + segs（任意角墙段）+ visplane ⇒ 已**不是** ray casting |
| 1996 | **Duke Nukem 3D**（Build 引擎） | 又一条：portal / sector 渲染（也是 2.5D，支持高低差） |
| 今天 | ① **体素引擎**（Teardown、Minecraft 类的射线拾取/LOS）② GPU compute shader 里的 **voxel DDA** ③ 游戏 AI 的**视线判定（LOS）** | ★ **DDA 格步进至今仍是"射线穿网格"的行业标准做法** —— 只是不再用来画主视图了 |
| 近年 | 风格化 ASCII/字符 3D 演示（如 WebGL 的 `asciicker`、终端里的 Doom 移植） | 大多走**真 3D 管线 + 后处理成字符**，与我们路线不同 |

---

## 5. 三个容易混的名词

| 名词 | 干什么 | 成本 |
| --- | --- | --- |
| **Ray casting**（我们） | 每列/每格一条射线，只求**最近命中** ⇒ 画墙 | 极低（一列一条） |
| **Ray tracing**（Whitted 1980 起） | 命中后**递归**反弹（反射/折射/阴影） | 高 |
| **Ray marching / Path tracing** | 逐像素步进 / 随机采样，解**全局光照** | 极高（离线或 GPU） |

---

## 6. 如果哪天真要做"真 3D"：升级清单

按门槛顺序：
1. `math/Vec3.h` + `Mat4`（平移 / 旋转 / 透视）——**`TECH_DEBT` C8 现在挂着的就是这件事**；
2. 一个**三角形/四边形光栅器**，把面片填进字符格；
3. **每格深度缓冲**（比列级 z-buffer 更细）；
4. 近/远裁剪 + 背面剔除；
5. 把墙/地板/精灵统统变成几何体。

⚠️ 触发条件（`C8` 原文口径）：**"第一次真的需要三个分量或非 AABB 的矩形时"**。今天不需要 —— 提前建 = 写一个没人 include 的半成品。

---

## 7. 权威阅读清单（想深入就看原文）

| 看什么 | 为什么值得看 |
| --- | --- |
| **Amanatides & Woo, 1987**（Eurographics） | 我们 DDA 的**原始论文**：增量式格遍历，为什么"一次只跨一格"是对的。<br>📄 **本地副本**：`study/papers/Amanatides_Woo_1987_FastVoxelTraversal.pdf`（`PDF v1.2, 6 页, 17322 字节`；SHA256 `05254dd9bed9a81d2c9da60e40bfe77b89e6b757428cc363a802a91ae835e5d5`。⚠️ 被 `.gitignore` 忽略 —— 第三方版权材料，**本地保存、不入库**）。<br>🔁 **"怎么来的 + 换台机器怎么复现同一个文件"** ⇒ `study/papers/README.md`（含原样可粘贴的下载命令 + 校验命令）<br>🔗 官方 OA 版：`https://doi.org/10.2312/egtp.19871000`（EG 数字图书馆，标注 Open Access；⚠️ 自动下载要鉴权，**用浏览器打开**即可）<br>🔗 作者主页（本地这份的来源，2026-10-16 实测 HTTP 200）：`http://www.cse.yorku.ca/~amana/research/grid.pdf` |
| **Lode Vandevenne, "Raycasting"**（lodev.org 的 CG 教程） | 最常被参照的教程；我们的 `dir + plane×cameraX` / `deltaDist` / `sideDist` 命名与结构同源 |
| **Fabien Sanglard《Game Engine Black Book: Wolfenstein 3D》** | 把 1992 那套渲染器（含 `wl_draw.c` 的按列跨格）逐段拆开讲 |
| **Fabien Sanglard《Game Engine Black Book: DOOM》** | 看"高一档"长什么样：BSP + segs + visplane |
| **Appel, 1968** | "ray casting" 一词的出处（隐藏面消除） |
| **Whitted, 1980**《An Improved Illumination Model for Shaded Display》 | 递归 ray tracing 的开山 |
| **Comanche "Voxel Space"** 的技术回顾（含 GitHub 上的复刻） | 高度场 ray marching：同族、不同支 |
| **GPU voxel DDA**（compute shader 里的体素遍历） | 看这条 1987 年的算法今天在 GPU 上怎么活 |

> 本节只写"是什么 / 为什么值得读"，**不抄内容**：一是没必要，二是二手总结替代不了原文。

---

## 8. 自测题（想确认自己真懂了，就回答这 6 个）

| # | 问题 | 答不出来去哪看 |
| --- | --- | --- |
| 1 | 为什么 `perp` 可以直接取 DDA 的参数 `t`？ | §2 第 1 步 + 第 4 步；代码 `RayCaster.h` 文件头「三条硬约定」 |
| 2 | 如果误把 `perp` 写成"欧氏斜距"，屏幕会怎么错？ | §2 第 4 步；用例 `Raycaster.PerpIsEqualAcrossColumns`（反向验证 R1 的靶子） |
| 3 | 为什么每列只画一次就不需要 z-buffer？什么时候这个前提会破？ | §2 第 6 步（答案指向 S13） |
| 4 | `cols` 为什么最好是偶数？ | §2 第 2 步；`tests/test_raycaster.cpp` 里的 `static_assert` |
| 5 | 墙高为什么要**至少一行**、取整为什么要"半高 floor / 总高 ceil"？ | §2 第 8 步（坑 17） |
| 6 | 我们这套与 DOOM 的差别**一句话**讲清 | §1 第 ② 档 + §4 年代表 1993 行 |

---

## 9. 与其他文档的关系

| 文档 | 关系 |
| --- | --- |
| `doc/S12_CHECKLIST.md` | 本阶段（Ray Casting 伪 3D）的施工图：接口/算法/坑表/验收；本文是它的**背景与延伸阅读** |
| `doc/MiniEngine_PLAN.md` | §C 设计（模块划分）· §H 长期路线（`S12 Ray Casting 伪3D → S13 Sprite/Z-buffer → S14 打磨`） |
| `doc/TECH_DEBT.md` | **C8**：`Vec3.h`/`Rect.h` 只有契约（做真 3D 的第一道门槛） |
