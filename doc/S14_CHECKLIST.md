# S14 施工图 —— 打磨与收口（v1.0「收得起来」）

> **状态（2026-10-26 落纸）**：**六个分叉已按推荐拍板**（§2）· **未开工**（预计 **0 行生产代码改动**）。
> **前置**：S13（精灵 + 遮挡）✅ 2026-10-26 收口 —— 259 用例全绿 · 反向验证 5 场 · 人眼通过 · **`v0.6.0` 已打并推送**（annotated，指向 `3174791`）。
> ⚠️ 本文是**方案层**：只写步骤、命令、坑表、验收；不含实现。

---

## 0. 目标 / 明确不做

| 项 | 内容 |
| --- | --- |
| **目标** | 把 `PLAN` G 节 v1.0 剩下的 **②③** 做完并打 `v1.0`：<br>② **README + GIF + CI** —— README/CI 已有，**缺 GIF**<br>③ **干净克隆后三条命令能跑** |
| **本阶段产物** | ① `doc/media/demo.gif`（真实录制，入库）② `S14` 的干净克隆验证记录 ③ 版本串 **`1.0.0`** ④ annotated tag **`v1.0`** ⑤（可选）`v0.6.0` 的 demo 包 |
| **不做** | ① **A\*/雾/粒子/关卡编辑器**（都在 `PLAN` H.2 扩展池，属 v1.x+）② 不改任何 `engine/` / `game/` 逻辑 ③ **不加新用例**（除非干净克隆暴露 bug）④ 不做真 3D（`TECH_DEBT` C8 的 `Vec3` 仍不需要） |

---

## 1. 前置事实（动手前先核这 8 条）

| # | 事实 | 出处 | 含义 |
| --- | --- | --- | --- |
| 1 | `.gitignore` **没有忽略 `*.gif`**；但 `dist/` 与 `*.exe` 被忽略 | `.gitignore` | GIF 可以入库；**demo 包只能走 Release 资产** |
| 2 | CI 的 `paths-ignore` 含 `doc/**` 与 `**/*.md` | `.github/workflows/ci.yml:28` | **GIF/文档提交不触发 CI**，badge 停在旧结果（README 已如实写）；补验用 `workflow_dispatch` |
| 3 | 默认画布 `kDefaultCanvas{ 100, 30 }` | `engine/include/mini/core/Application.h:70` | 录制窗口设 **100×30**，GIF 才与 README 的数字一致 |
| 4 | README 已有 GIF 占位 + 分镜注释 | `README.md:14-15` | 分镜：标题屏 → `ENTER` → 打怪/开火 → `ESC` 暂停 → 死亡 → `R` 重开 |
| 5 | 版本串现在是 `0.6.0-dev`，`-dev` 的含义是「还没到 v1.0」 | `engine/src/core/Version.cpp` | 打 `v1.0` 时改 **`1.0.0`**（去掉后缀） |
| 6 | 版本串有 **4 处功能落点** + 1 处举例注释 | `Version.cpp` / `test_smoke.cpp` / `Version.h` | 改版本号必须一起改；`Core.VersionIsReported` 是守卫 |
| 7 | **当前代码与 `v0.6.0` 完全一致**（`git diff --stat v0.6.0..HEAD -- game/ engine/ tests/` 输出为空） | git | ⇒ **不必 checkout tag 就能录 GIF**，录到的就是 v0.6.0 的画面 |
| 8 | `tools/pack_demo.ps1` 的 `-Version` 是**手填参数**（默认 `0.4.0`），与 `VersionString()` 不同源 | `tools/pack_demo.ps1:27` | 打错版本号不会被任何东西发现 ⇒ 打包前先跑 exe 核对 |

---

## 2. 六个分叉（**✅ 2026-10-26 全部按推荐拍板**）

| # | 分叉 | **决定** | 理由 / 连带 |
| --- | --- | --- | --- |
| **A** | GIF 放哪、多大 | **甲：`doc/media/demo.gif` 入库，≤3 MB** | README 永远能显示、不依赖外链；代价是仓库多几 MB。⚠️ GitHub 单文件 ≤100 MB、仓库建议 <1 GB ⇒ 3 MB 绰绰有余 |
| **B** | 用什么终端录 | **甲：经典控制台 `conhost`** | ConPTY（Windows Terminal / VS Code）收不到真 key-up ⇒「按住移动」退化成每按走一帧，录出来像幻灯片（`AGENTS.md` §4 的已知限制） |
| **C** | 干净克隆怎么验 | **甲：本机 `git clone . <临时目录>`** | 能抓「漏提交 / 绝对路径 / 手写源文件清单漏项」；「另一台机器」的差异留作可选加验 |
| **D** | demo 包要不要重打 | **甲：重打成 `v0.6.0`** 并附 Release | v0.6.0 才「3D 里看得见东西」，把 v0.4.0 的包换掉才有意义。⚠️ 包必须在 `v0.6.0` 上打 |
| **E** | v1.0 时版本串 | **甲：`0.6.0-dev → 1.0.0`**（去 `-dev`） | 原约定：`-dev` = 还没到 v1.0；到 v1.0 就该去掉 |
| **F** | 扩展池功能 | **甲：本轮都不做**，直接冲 v1.0 | A\* ≈ 2.5h 起，会把 v1.0 无限推后；v1.0 收口后想玩再开分支 |

---

## 3. 步骤（6 步，每步都能单独停）

| 步 | 内容 | 谁做 | 验收（判据见 §5） |
| --- | --- | --- | --- |
| **0 准备** | 控制台拉到 **100×30**；`V` 进 3D 确认 `█▓▒░` 不出 `?`；准备录制工具（ScreenToGif 之类，**开发工具、不是代码依赖**） | 你 | 3D 画面里墙是 `█▓▒▒░`，不是方块/问号 |
| **1 GIF 入库** | 按 §1 第 4 条的分镜录 **≈10 秒**，存成 `doc/media/demo.gif`；我改 README 嵌图（删掉 `TODO(发布前)` 注释） | 你录 / 我写 | `git ls-files doc/media/demo.gif` 有它；`≤3 MB` |
| **2 README 复核** | 补一条 S13 技术点（**z-buffer 只有 100 项**，与 S12 的「320 项」说法对齐）· 已知限制逐条核对 · demo 链接指向新包 | 我 | 与代码事实逐条对得上 |
| **3 干净克隆验证** | 新目录 clone → 跑 §4 那 6 条命令；**只记录结论，不改代码** | 你跑 / 我记 | `259 passed, 0 failed` + `1/1 passed` |
| **4 版本 `1.0.0`** | 四处功能落点 + `Version.h` 举例注释；文档同步 | 我 | 重建后 `259 passed, 0 failed` |
| **5 tag + demo 包** | `git tag -a v1.0` → `git push origin v1.0`；可选：按 `v0.6.0` 打 demo 包并传 Release | 你拍板 / 我打 tag 命令 | `git ls-remote --tags origin v1.0` 有值；`git rev-parse v1.0^{commit}` 指向版本 1.0.0 那一笔 |
| **6 收口** | 本文补「收口记录」· `PLAN` / `AGENTS` §2·§5 / `README` / `TECH_DEBT` / `SHOWCASE_PLAN` 同步 | 我 | 索引行都标 ✅ |

---

## 4. 干净克隆验证的命令（原样可粘贴，PowerShell）

```powershell
cd $env:TEMP
git clone "d:\Project\fish\c++_simple_engine" MiniEngine-clean
cd MiniEngine-clean
cmake --version                                        # 期望 ≥ 3.20（这台机器是 4.2.1）
cmake -S . -B build                                    # 新目录，必须从零 configure
cmake --build build --config Debug                     # 退出码 0，/W4 下 0 warning
ctest --test-dir build -C Debug --output-on-failure    # 1/1 passed
.\build\bin\Debug\mini_tests.exe                       # 259 passed, 0 failed
.\build\bin\Debug\MiniEngine.exe                       # 能起、能玩（人眼一条）
```

⚠️ **必须在空目录里 clone**：把旧 `build/` 或 `CMakeCache.txt` 拷进去，等于自带缓存，验不出「漏提交」。

---

## 5. 验收口径

### 5.1 机器门

| 检查项 | 期望 |
| --- | --- |
| 正常构建（`build/`） | 退出码 0，`/W4` 0 warning |
| `mini_tests.exe` 尾行 | **`259 passed, 0 failed`**（第 4 步改版本号之后仍是 259，只是 `Core.VersionIsReported` 读新串） |
| `ctest` | **`1/1` passed** |
| 干净克隆（§4） | 6 条命令全过，数字同上 |

### 5.2 人眼门

| # | 项 | 判据 |
| --- | --- | --- |
| 1 | GIF 画面 | 100×30；`@` / `E` / `*` / `█▓▒░` 都认得出；**3D 走廊里看得见 `E`** |
| 2 | GIF 分镜 | 六段都在：标题屏 → 开局 → 打怪/开火 → 暂停（画面**冻住**）→ 死亡 → 重开 |
| 3 | GIF 体积 | ≤ **3 MB**；浏览器里能循环播放 |
| 4 | 干净克隆的 exe | 起来就能玩（不依赖本机的 `dist/`、日志、预设） |

### 5.3 本阶段为什么没有反向验证

S14 **不加任何逻辑**（预期 0 行生产代码改动）⇒ 没有「改坏一处看用例响不响」的靶子。替代它的两场检查是：

| 场 | 它防的是什么 |
| --- | --- |
| **干净克隆**（§4） | 「**漏提交**」——本机永远绿、别人 clone 下来编译不过（`tests/CMakeLists.txt` 是手写清单，漏一行就少一个 suite；本项目禁用 `file(GLOB)`） |
| **GIF 分镜核对**（§5.2 第 2 条） | 「**对外演示与真实游戏不一致**」——README 的宣传必须能在 exe 里复现 |

---

## 6. 坑表（按「会出什么现象」排）

| # | 坑 | 症状 / 判据 | 对策 |
| --- | --- | --- | --- |
| 1 | **GIF 体积失控** | 全屏录 20 秒能到 10+ MB | 只录 ≈10 秒、画面 100×30、录制工具里降帧率/降色数；目标 **≤3 MB** |
| 2 | 在**错误的 commit** 上录 | 画面与 tag 对不上 | 录之前跑 `git diff --stat v0.6.0..HEAD -- game/ engine/ tests/`，**输出为空**才录 |
| 3 | 用 **Windows Terminal / VS Code 终端** 录 | 「按住移动」退化 ⇒ 画面一顿一顿 | 用经典控制台（`开始试玩.cmd` 那套思路：`conhost.exe`） |
| 4 | 终端字体不认 `█▓▒░` | 3D 墙变成 `?` 或实心方块 | 换 Consolas / 换字号；**先按 `V` 看一眼再录** |
| 5 | **提交 GIF 不触发 CI** | badge 停在上一次的结论 | 已知行为（`ci.yml` 的 `paths-ignore`）；要补验就 `workflow_dispatch` 手动跑一次 |
| 6 | 拿旧 `build/` 冒充干净克隆 | 假绿 | 目标目录必须由 `git clone` 新建（或 `rm -rf` 后重建） |
| 7 | 版本号**四处不同步** | `Core.VersionIsReported` 红（`lhs` 是旧串） | 字符串 + `VersionMajor/Minor` + `test_smoke.cpp` 两行 + `Version.h` 举例注释，**一起改** |
| 8 | `pack_demo.ps1 -Version` **手填错** | zip 名与真实版本不符，且无人发现 | 打包前跑一次 exe 看版本；⚠️ 这是**新债候选**（脚本与 `VersionString()` 不同源） |
| 9 | **tag 指错 commit** | 指向「改版本号之前」那一笔 | tag 必须在**版本 `1.0.0` 已落地**的那一笔上；打完 `git rev-parse v1.0^{commit}` 核对 |
| 10 | tag 推完又改 | 远端与本地不一致 | `AGENTS.md` §5：**tag 一旦推送不移动**；改错只能发新 tag |

---

## 7. commit 切分（5 笔，每笔都能构建）

| # | 内容 | 说明 |
| --- | --- | --- |
| 1 | `docs: README 嵌入 demo GIF（S14 第 1 步）` + `doc/media/demo.gif` | 纯文档，不触发 CI（预期内） |
| 2 | `docs: README 技术点补 z-buffer + 已知限制复核` | 可与第 1 笔合并 |
| 3 | `docs: S14 干净克隆验证记录` | 只写结论 + 命令 + 实际输出尾行 |
| 4 | `chore: 版本号 0.6.0-dev → 1.0.0（v1.0 收口）` + 文档同步 | 机器门要重跑一次（259） |
| 5 | `docs: v1.0 已打并推送 —— 回填 tag 状态` | 在 tag **之后**，与 v0.5.0 / v0.6.0 同一套做法 |

---

## 8. 文档同步清单（收口时）

| 文件 | 改什么 |
| --- | --- |
| `doc/MiniEngine_PLAN.md` | G 节 v1.0 行标 ✅（②③ 回填）· S14 里程碑 + 落地偏差 |
| `AGENTS.md` | §2 索引（本文一行）· §5 tag 表加 `v1.0` |
| `README.md` | 状态行（GIF / 版本 `1.0.0`）· 顶部嵌图 · 已知限制（去掉 "TODO 发布前"） |
| `doc/TECH_DEBT.md` | C8/C43/C45 复核 · 新增「`pack_demo.ps1` 版本手填」 |
| `doc/SHOWCASE_PLAN.md` | 「动作 1（GIF）」勾掉；动作 2（CI）勾掉 |
| `doc/S14_CHECKLIST.md` | 本文补「收口记录」 |
| `Version.cpp` / `test_smoke.cpp` / `Version.h` | `1.0.0`（四处落点 + 举例注释） |

---

## 9. 风险与待确认

| # | 项 | 状态 |
| --- | --- | --- |
| **T1** | GIF 里 3D 的 `█▓▒░` 在录制工具的缩放下会糊 | 待录完看；糊就放大字号重录（画布仍是 100×30） |
| **T2** | 干净克隆要不要**再验一台机器** | 建议先只做本机；若 CI 也绿，则「另一台机器」只是加分 |
| **T3** | demo 包 `v0.6.0` 传 GitHub Release | 需要网页操作（本机没有 `gh`）；正文可先落 `doc/RELEASE_NOTES_v0.6.md`（**要不要写，你定**） |
| **T4** | `v1.0` 之后回 `-dev` 吗 | 不回：`v1.0` 是「收得起来」的终态，之后进 v1.x+ 纯玩阶段（可开分支） |

---

> **一句话**：S14 不改一行逻辑 —— 它只做三件事：**把画面录下来**、**证明别人 clone 下来能跑**、**打 `v1.0`**。
