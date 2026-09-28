# MiniEngine 技术债清单与执行计划

> 建立于 **2026-09-28**（S3 结束时）。前置约定见 `AGENTS.md` 1.4：
> **编译与运行由本人执行，AI 只做代码编辑。**
> 所以本文件的每一条都写成「谁做什么 → 跑什么命令 → 期望看到什么」。
>
> 相关文档：`doc/MiniEngine_PLAN.md`（PLAN）、`doc/S3_CHECKLIST.md`（S3 施工图与坑表）。

---

## 0. 这份清单怎么用

### 0.1 分类判据

| 组 | 含义 | 判据 |
| --- | --- | --- |
| **A** | 现在就能关掉，**不改任何生产代码** | 只需要跑命令 / 抄数据 |
| **B** | 现在就能关掉，**要写代码** | 不依赖后面阶段的结构，改动 ≤ 20 行 |
| **C** | 明确推迟，附**触发条件** | 现在做就是提前替后续阶段做决定（违反 PLAN A.4「结构被复杂度逼出来」） |
| **D** | 已关闭，留档 | 供以后查"这个问题当时是怎么解决的" |

### 0.2 为什么要有"触发条件"

推迟不是遗忘。C 组每一条都必须写清**什么信号出现时该动它**，
否则它会从"有意推迟"变成"真的忘了"。

---

## 1. 总表

| # | 事项 | 组 | 成本 | 价值 |
| --- | --- | --- | --- | --- |
| **B1** | `Time::LimitFPS` 没有自动化守卫（60fps 只被人眼看过） | B | ~15 行 + 1 个用例 | ⭐⭐⭐ 把"看起来稳"变成机器查 |
| **B4** | 文档里"float 累加会漂成 59.94"**没有依据**，反向验证 1 的预期结果也随之写错 | B | 改 4 处文字 | ⭐⭐⭐ 错的知识比没有知识更坏 |
| **A3** | **Release 构建从未做过** | A | 3 分钟 | ⭐⭐⭐ 可能挖出 `/O2` 下的新警告 |
| **A4b** | `CMakeLists` 的 `-Wall -Wextra -Wshadow` 分支**本机不可达**（ClangCL 走 MSVC 分支 + 没有 ninja） | A/B | 装 ninja 0.5h | ⭐⭐⭐ 那 4 个开关从没生效过 |
| **B2** | 两个接口 **0 调用者**：`Time::FrameCount()`、`RowDiff::Resize()` | B | 删 ~15 行 | ⭐⭐ 少一个会误导人的接口 |
| **A4** | `ms-clang-debug` 预设从未实测（clang 17 在机） | A | 5 分钟 | ⭐⭐ 交叉验证 MSVC 漏掉的警告 |
| **A5** | `ms-asan` 预设从未实测；且**可能链接失败**（缺 `target_link_options`），UBSan 其实没开 | A | 5 分钟 | ⭐⭐ S3 新增了不少下标/缓冲区逻辑 |
| **B3** | `MaxFrameMs` 等 4 个 API 只有测试调用；且我收口时**静默删掉**了一条引用它的验收项 | B | 1~20 行 | ⭐⭐ 接口与验收要对得上 |
| **A1** | `mini_tests.exe Nope` 门闩未复验 | A | 10 秒 | ⭐ |
| **A2** | 顶栏量化数据没留记录（FPS 波动范围、典型 `Rows`） | A | 3 分钟 | ⭐ 以后没法对比 |
| **B5** | 6 个"只有测试调用"的接口没有注释说明 | B | 6 行注释 | ⭐ 防止后人误删 |
| **A6** | 反向验证 2/4（挪 `BeginFrame`、放大 clamp） | A（可选） | 需改代码 | ⭐ 证明设计必要性 |
| **C1** | `main.cpp` 359 行（PLAN 说 <20 行） | C | → S5 | — |
| **C2** | `game/` 静态库未建 | C | → S5 | — |
| **C3** | `Version` 还是 `0.1.0-dev` | C | → S7 后 | — |
| **C4** | `DemoGame` 自缓存 `const Input*`（S3 决策 D13） | C | 触发即改 | — |
| **C5** | 没有 CI | C | → S14 | — |
| **C6** | `ctest` 没有 `LABELS` / `TIMEOUT` | C | 需要时 | — |
| **C7** | `ConsoleWindows` / `Application` 无法离线单测 | C | 不做 | — |

---

## 2. A 组：现在就能关掉（不改生产代码）

### A1 `mini_tests.exe Nope` 门闩复验

- **现状**：S2 验过；S3 期间没动 `tests/test_main.cpp` 的过滤逻辑，但也没复验。
- **为什么算债**：它是防"过滤不存在的 suite 时静默通过"的门闩 —— 一旦坏了，
  将来 `ctest` 里少跑一个 suite 也看不出来。这类"看起来绿其实没跑"的假绿最危险。
- **步骤**：
  ```powershell
  .\build\bin\Debug\mini_tests.exe Nope
  $LASTEXITCODE          # 注意：不是 $?（见 AGENTS.md 第 4 节）
  ```
- **期望**：打印 `no case in suite 'Nope'` 之类的提示，且 `$LASTEXITCODE -ne 0`。

### A2 顶栏量化数据抄录

- **现状**：S3 验收第 1~6 条是"整体通过"，没有数字。
- **为什么算债**：S4 之后要动 `FrameStats` / 限帧的人，需要一条基线来对比"是不是更慢了"。
- **步骤**：
  ```powershell
  .\build\bin\Debug\MiniEngine.exe
  ```
  1. 静止看 30 秒，记 `FPS` 的最小/最大值、`Work` 的典型值、静止时 `Rows` 的典型值
  2. 按住 W 让它连点 10 次，记 `Rows` 的峰值
  3. 拖窗口到最窄再拖回，记 `Rows` 分母（应等于画布行数）与有没有残影
  4. `.\build\bin\Debug\MiniEngine.exe 300` 记 `FPS` 与 `Work`
- **期望**：FPS 58~61；静止 `Rows` 1~2；`300` 时 FPS 200+ 而 `Work` 不变。
- **落点**：抄进 `doc/S3_CHECKLIST.md` 10.2，或在 S4_CHECKLIST 里建"基线"一节。

### A3 Release 构建 + 跑测试（**从未做过**）

- **证据**：`build/bin/` 下**只有 `Debug/`**；而配置类型本来就是
  `Debug;Release;MinSizeRel;RelWithDebInfo`（VS 多配置生成器）——
  所以 **Release 不需要新的构建目录，在现有 `build/` 里直接就能构建**。
- **为什么算债**：`/O2` 下 MSVC 会报出 Debug 下不报的警告（尤其
  "变量可能未初始化"、"有符号/无符号比较"这一类）。而且 `NDEBUG` 会关掉断言 ——
  S1 的 `FrameBuffer` 与 S3 的 `RowDiff` 都靠"静默忽略"而不是断言，风险不大，
  但**没跑过就不能说没有**。
- **步骤**（用现有 `build/`，**不要**新建目录 —— 两个构建目录混用才是坑）：
  ```powershell
  cmake --build build --config Release
  ctest --test-dir build -C Release --output-on-failure
  .\build\bin\Release\mini_tests.exe
  .\build\bin\Release\MiniEngine.exe
  ```
  也可以走预设：`cmake --build --preset release`（`CMakePresets.json` 里已定义，
  且它指向的就是 `build/`）。
- **期望**：0 warning / 0 error；`ctest` 1/1；34 passed；exe 画面与 Debug 一致。
  ⚠️ 警告数量与 Debug 不一致时**先别改代码**，把输出贴回来。
- **注意**：`ctest` 必须带 `-C Release`（多配置生成器，漏了会找不到配置）。

### A4 `ms-clang-debug` 预设实测

- **现状**：`CMakePresets.json` 里已定义（`binaryDir = build-clang`，`toolset = ClangCL`），
  从未配置过。本机有 LLVM clang 17。
- **为什么算债**：同一套告警级别下，clang 报的东西和 MSVC 不一样。
  S3 新写的代码里有几处值得交叉验：聚合体的引用成员（`FrameContext`）、
  成员初始化列表顺序（`Application` 的 `m_size` → `m_frameBuffer`）、
  `[[nodiscard]]` 的忽略点。
- **步骤**：
  ```powershell
  cmake --preset ms-clang-debug
  cmake --build --preset clang-debug
  ctest --test-dir build-clang -C Debug --output-on-failure
  ```
  ⚠️ **没有 clang 的 testPreset**（`testPresets` 里只有 `debug`），
  所以 `ctest` 必须像上面那样手写 `--test-dir` + `-C`。
- **期望**：0 warning / 0 error；`ctest` 1/1；34 passed。

### A4b ⚠️ **`-Wall -Wextra -Wshadow` 那一支在本机根本不可达**（本次盘点新发现）

- **现状**：`CMakeLists.txt:37-39` 有个 `else()` 分支给非 MSVC 编译器加
  `-Wall -Wextra -Wpedantic -Wshadow`。但是：
  1. **ClangCL 会让 CMake 把 `MSVC` 判为真** → A4 走的是 `/W4 /permissive-` 那一支，
     **不会**应用到 `-Wextra -Wshadow`；
  2. 想真正走 `else()` 需要 `-G Ninja -DCMAKE_CXX_COMPILER=clang++`，
     而**本机没有 ninja / make**（`AGENTS.md` 第 4 节）。
- **结论**：那 4 个更严格的警告开关**从建立以来一次都没生效过**。
  典型的"写了但从没跑过"的配置 —— 和 B1 那条是同一种病。
- **选项**：

  | 选项 | 做法 | 代价 / 收益 |
  | --- | --- | --- |
  | **① 装 ninja**（推荐） | `winget install Ninja-build.Ninja`，然后<br>`cmake -S . -B build-clang-ninja -G Ninja -DCMAKE_CXX_COMPILER=clang++`<br>`cmake --build build-clang-ninja` | 0.5h（含下载）。**一次装好两个收益**：① 真跑一次 `-Wextra -Wshadow`；② 构建提速（PLAN H.5 扩展池的第一条就是这个） |
  | ② 暂时接受 | 在 `CMakeLists.txt` 那几行加注释说明"本机不可达，等装了 ninja 再验" | 0 成本，但那 4 个开关继续是摆设 |

- **推荐 ①**。装上之后 `-Wextra` 很可能**立刻**报出几条现在没人知道的问题
  （`-Wshadow` 对 `DrawHudRow` 那种短局部名尤其敏感）。

### A5 `ms-asan` 预设实测

- **现状**：`binaryDir = build-asan`，`inherits = ms-clang-debug`，
  `cacheVariables = { MINI_SANITIZE ON }`。`MINI_SANITIZE` 在
  `CMakeLists.txt:40-47` **确实被消费了**（不是空设变量）。
- **为什么算债**：S3 新增的代码里有大量**手写下标**：
  `FrameBuffer::Index()`、`RowDiff::Compose()` 的 `m_prevRows[y]`、
  `AppendRowTo()` 的 `m_cells.data() + y * m_w`、`DrawStatsBar()` 的 `text[x]`。
  这些在 Debug 下靠"我推过没问题"，ASan 能直接证伪。
- **步骤**（**没有 `asan` 这个 buildPreset**，只有 `debug` / `release` / `clang-debug`）：
  ```powershell
  cmake --preset ms-asan
  cmake --build build-asan --config Debug
  ctest --test-dir build-asan -C Debug --output-on-failure
  ```
- **期望**：0 error。**期望是干净的** —— 有报告就是真 bug，优先修。
- **两个提前知道的坑**（跑出来别慌，都在预期内）：
  1. **可能链接失败**（`__asan_*` 未解析）。原因：`CMakeLists.txt:42` 只写了
     `target_compile_options(... /fsanitize=address)`，**没有配套的 `target_link_options`**
     （下面那个 `else()` 分支才是编译/链接两个都写）。真报这个错，
     就在那一行旁边补一句链接选项 —— **这是 A5 要发现的东西之一，不是意外**。
  2. **UBSan 其实没开**。预设的 `displayName` 写着 "ClangCL + ASan/UBSan"，
     但 ClangCL 让 `MSVC` 为真 → 走的是只有 `/fsanitize=address` 那一支。
     要么把预设名改准，要么补上 `/fsanitize=undefined`。

### A6（可选）反向验证 2 / 4

- **现状**：`doc/S3_CHECKLIST.md` 10.3。第 3 条已在步骤 2 做过并还原。
- **为什么排在最后**：这两条的价值是"证明某个设计选择是必要的"（教学价值），
  不是"发现 bug"。而且**都要临时改生产代码** —— 你之前说过"需要改代码就算了"，
  所以列为可选。
- **步骤**：
  1. 第 4 条：把 `ClampDelta` 的上界从 `0.05f` 改成 `100.0f`，拖一下窗口，
     看 `@` 是否瞬移一大段 → 证明 clamp 不是洁癖 → **改回来**
  2. 第 2 条：把 `m_time.BeginFrame()` 从循环第 ① 步挪到 `LimitFPS` 之后，
     看顶栏 FPS 是否暴涨到几百 → 证明"帧首"是硬约束 → **改回来**
- **期望**：两次都能看到预期现象，且**改回来之后** `mini_tests` 仍是 34 passed。
- **纪律**：改完必须 `git diff` 确认现场干净（这两个文件都在版本控制里，`git checkout --` 能还原）。

---

## 3. B 组：现在就能加（要写代码）

> 全部由 AI 改代码、本人构建验证。每条都标了预计改动的行数上界，
> 超过就说明它不该现在做。

### B1 `Time::LimitFPS` 的自动化守卫 ⭐ 最高价值

- **现状**：`tests/test_time.cpp` 里 `PlatformWaitBeatsSleepGranularity` 测的是
  `platform::Wait()` **本身**；而 `LimitFPS()` 的**组合逻辑**
  （用哪些边界算 `remain` 与 `spin`、早退条件、`BeginFrame` 与 `LimitFPS` 共享
  同一个 `m_begin`）只被**肉眼看过**（"FPS 稳定在 58~61"）。
- **为什么算债**：**步骤 1 那个 bug 正是这里漏掉的**——当时的 `LimitFPS` 用
  `sleep_for`，`platform::Wait` 的用例根本照不到它，是靠"跑 exe 看 FPS 抖"才发现的。
  也就是说：**就算把 `LimitFPS` 改回 `sleep_for`，现在 34 个用例全绿**。
  这是本项目到目前为止最值得补的一条守卫。
- **做法**：在 `tests/test_time.cpp` 加 1 个用例（`Time` suite 从 8 → 9），
  量"`BeginFrame()` 之后调 `LimitFPS(60)`，到函数返回为止花了多久"。
  断言只需两个界：

  | 界 | 值 | 为什么卡在这 |
  | --- | --- | --- |
  | 下界 | ≥ 16 ms | 目标帧时间是 16.667ms。早醒说明 `remain`/`spin` 算错，或根本没等 |
  | 上界 | < 25 ms | 正确实现约 16.7ms；`sleep_for` 版会被 15.625ms 的调度器 tick 撞到 **31ms**。25ms 卡在两者中间，同时容忍机器负载 |

- **验收**：
  ```powershell
  .\build\bin\Debug\mini_tests.exe Time     # 期望 9 passed
  # 连跑 10 次确认不抖（带上界就是怕这个）
  1..10 | % { .\build\bin\Debug\mini_tests.exe Time | Select-Object -Last 1 }
  ```
- **反向验证（可选，改一行）**：把 `platform::Wait` 换回 `std::this_thread::sleep_for`
  → 这条**必须变红**。这是它存在的全部意义。
- **成本**：套件多约 17ms（当前全集 0.13s，可忽略）。

### B2 删两个 0 调用接口

| 接口 | 位置 | 调用者 | 处置 |
| --- | --- | --- | --- |
| `Time::FrameCount()` | `engine/include/mini/core/Time.h:133` | **全仓库 0 处**（连测试都没有） | **删**。将来要"第几帧"时加回来是 1 行 |
| `RowDiff::Resize(int)` | `RowDiff.h` 声明 + `RowDiff.cpp:42` 定义 | **全仓库 0 处** | **删**。`Compose()` 自己会发现行数变化；留着它反而会让人以为"必须手工调" |

- **为什么算债**：S3 决策 D11 里我刚写过"刻意不返回 `bytes`——加了就是没人用的接口"，
  结果自己又留了这两个。**没人用的接口是一条谎**：读代码的人会假设它在某处被用着。
- **注意**：`Time::m_frames`（成员）**不是**死代码 —— `EndFrame()` 用它跳过首帧，保留。
  删的只是访问器。
- **验收**：构建 0 warning；34 passed 不变；`RowDiff` suite 仍 10 passed。
- **连带**：`doc/S3_CHECKLIST.md` 里"`RowDiff::Resize`"提到过（2.2 与 5 节），
  要一并注明"实现时删掉了，`Compose` 自动发现"。

### B3 `MaxFrameMs` 的去向（顺带修一条被我静默删掉的验收项）

- **现状**：`Time::MaxFrameMs()` / `Time::Stats()` / `FrameStats::AvgFrameMs()` /
  `FrameStats::SampleCount()` 四个只在测试里有调用者，生产代码没人用。
- **额外的诚实说明**：PLAN 的 S3 验收原文有一条
  "**`MaxFrameMs()` 长时间运行不出现尖峰**"。我在收口时把它**悄悄删了**——
  因为顶栏根本没有显示 `Max`，这条验收**没法验**。这是我的疏漏，记在这。
- **两个选项，选一个**：

  | 选项 | 做法 | 代价 |
  | --- | --- | --- |
  | **① 顶栏加一格 `Max %5.1fms`**（推荐） | `Application::DrawStatsBar` 的格式串加一个字段 | 顶栏变长；且因为 `Reset()` 没有 UI 入口，"Max"= **自启动以来最差的一帧**（拖过一次窗口就会一直显示那次尖峰） |
  | ② 删掉这 4 个接口 + 对应的 3 条用例 | 回归"没人用就删" | 丢掉 `MaxFrameMsSurvivesWindow` 这条有价值的用例 |

- **推荐 ① 的理由**：PLAN 坑 #24 明确写了"resize 那一帧的尖峰 `MaxFrameMs()` **会**记下它
  —— 这是对的，别当 bug 去消"。所以"启动以来最差一帧"正是我们**想要**观察的量。
  这也把那条被我删掉的验收项**改写成可验的形式**：
  "启动后不做任何操作，`Max` 应稳定在 20ms 以内；拖一次窗口后 `Max` 会跳一次，**之后不再变**"。
- **验收**：顶栏出现 `Max`；静止 30 秒它不涨；拖窗口时跳一次然后停住。

### B4 修文档里那句**没有依据**的话（"float 累加会漂成 59.94"）

- **现状**：这句话出现在 4 处：
  `engine/src/core/Time.cpp:67-68`（代码注释）、`doc/S3_CHECKLIST.md:555`（伪代码注）、
  `:677`（坑表第 4 条）、`:818`（反向验证 1 的预期结果）。
- **为什么站不住**：
  1. `LimitFPS()` 每帧都以**当帧的 `m_begin`** 重新计算 `elapsed`，`BeginFrame()` 每帧重置
     基准 —— 所以 `target` 的浮点误差**不可能跨帧累积**，压根没有"漂移"的机制。
  2. 量级也对不上：`1.0f/60.0f` 在 float 里的误差约 **1 纳秒/帧**，
     60 帧累计 60ns，顶栏那一格是 0.1ms —— 差 1000 倍，肉眼永远看不见。
  3. **`59.94` 是 NTSC 的 1000/1001 系数**，和浮点累加没关系。原句是把一个熟悉
     数字安到了一个不相关的原因上。
- **真实的陷阱是什么**：**毫秒截断**。若把目标帧时间写成
  `duration_cast<milliseconds>(1/60 秒)` 会得到 **16ms**（而不是 16.667ms）→
  顶栏会稳定停在 **61 左右**（不是 60.0，也不是"漂移"）。这才是一条能一眼看出来的错。
- **处置**：
  1. 4 处文字改成上面的准确说法（整数纳秒的收益是**避免毫秒截断 + 可复现**，
     不是"防漂移"）
  2. `doc/S3_CHECKLIST.md` 10.3 反向验证 1 **改写成可验证的形式**：
     把目标帧时间改成 `milliseconds(1000/60)`，期望看到顶栏稳定在 61 左右（而不是漂移）
  3. 10.3 第 1 条的状态从"❌ 未做"改成"**预期结果原有错误，已改写**"
- **验收**：文档里搜 `59.94` 应只剩"这个说法是错的"这一段本身。

### B5 给"只有测试调用"的接口加一行注释

- **现状**：以下接口在 `engine/src/` 与 `main.cpp` 里 0 调用，但在 `tests/` 里有：

  | 接口 | 现状 | 建议注释 |
  | --- | --- | --- |
  | `FrameBuffer::DrawRect()` | 只有测试调用 | "S1 画布原语，S12+ 画 HUD/边框会用；目前无生产调用者" |
  | `FrameBuffer::At()` | 只有测试调用 | "读回画布，S6 相机/S7 碰撞要用" |
  | `Input::IsDown()` | 无生产调用者 | "= Pressed \|\| Held 的便捷写法；游戏侧用，引擎不用" |
  | `FrameStats::AvgFrameMs()` / `SampleCount()` | 只有测试调用 | 见 B3 |

- **为什么算债**：没有注释时，下一个读到它的人只有两个选择：猜，或去 grep。
  一行注释把这个成本降到 0。
- **验收**：不需要构建（纯注释），但要确认 34 passed 不受影响。

---

## 4. C 组：明确推迟（附触发条件）

| # | 事项 | 触发条件（**出现这个信号就动它**） |
| --- | --- | --- |
| C1 | `main.cpp` 359 行，其中 `DemoGame` 约 250 行 | **S5 建 `game/` 静态库时**。它是那时要搬到 `game/src/Game.cpp` 的东西，现在拆只是把同样的代码换个地方放 |
| C2 | `game/` 目录与静态库未建 | **S5**。PLAN 与顶层 `CMakeLists.txt` 的注释都写死了"S5 才建"，提前建会让 `engine` 的依赖边界变灰 |
| C3 | `Version` 字面量是 `0.1.0-dev` | **PLAN G 节的 v0.2「走得动」达成时**（S6 相机 + S7 碰撞之后）。现在改只是一个数字 |
| C4 | `DemoGame` 自缓存 `const Input*`（S3 决策 D13） | **出现第 2 个需要在 `OnRender` 里读输入的游戏时**。到那时把 `FrameContext` 也传给 `OnRender`，而不是让每个游戏各自缓存一遍 |
| C5 | 没有 CI | **S14 打磨与收口**，或假期排期的 Day7（PLAN 已经把 CI 放在那天）。现在加会在每次 push 时跑一套还没稳定的构建 |
| C6 | `ctest` 没有 `LABELS` / `TIMEOUT` | **用例总数 > 50，或出现单条 > 1s 的慢用例**（B1 加的 17ms 还不够成理由）。PLAN H.5 已经把它排在 Day7 |
| C7 | `ConsoleWindows` / `Application` 无法离线单测 | **不打算动**。它们本质是"和终端打交道"，能离线测的部分（`InputState` / `RowDiff` / `FrameStats` / `ClampDelta`）**已经全部拆出去了** —— 这本身就是 S2/S3 的设计成果，不是缺口 |

### 4.1 关于 C7 的一个例外

`Application::Run()` 里有**一段**是真逻辑、且可以离线测的：**resize 的接受条件**
（`now.width >= 2 && now.height >= 2` 才重建，否则跳过）。但它现在写在循环体里，
要测就得把它抽成自由函数。

**暂不做**，理由是那条分支的失败模式已经被"拖到极窄不崩"这条肉眼验收覆盖了，
抽函数是纯为测试而拆。**触发条件**：如果 S6 的相机边界也依赖这个尺寸协商，
那时把"尺寸协商"整体抽出来再一并测。

---

## 5. D 组：已关闭（留档）

| # | 问题 | 关在哪 | 怎么验的 |
| --- | --- | --- | --- |
| D1 | 关控制台后进程成孤儿，锁住 exe 让构建全挂（`LNK1168`） | S3 / `a6deebb` | 杀 `conhost.exe` → 进程 3 秒内自己退出（recipe 在 `S3_CHECKLIST` 10.2 第 7 条） |
| D2 | 顶栏被游戏内容盖掉 | S3 / `1d45702` | 顶栏在 `game.OnRender` **之后**画 |
| D3 | 每帧 `ESC[2J` 导致全屏闪烁（PLAN E-3） | S3 / `a6deebb` | 只有"尺寸变化且非首帧"才清屏；其余走脏行定位 |
| D4 | 每帧重发整屏（PLAN E-3） | S3 / `dce6b5e` | 脏行 diff：80×25 实测 **2059 B → 197 B**，且等价性用例钉住"全量帧字节 == 老实现" |
| D5 | 窗口尺寸变化画面不跟手（PLAN E-12） | S3 / `1d45702` | 每帧比对 `Console::Size()`，在 `OnUpdate` **之前**重建画布 |
| D6 | `Sleep` 的 15.6ms 粒度毁掉 60fps 限帧 | S3 / `9f0a375` | `platform::Wait`（高精度计时器），`Wait(7ms)` 实测 7.05~7.56ms |
| D7 | `Pressed` 状态可能被吞（S2 的 `m_pulse` 认知） | S2 / `f4ea7a8` | `DownAndUpInSameFrameIsNotLost` 常驻用例 |
| D8 | 判"exe 被占用"用改名法会误判 | S2 文档 | 结论写进 PLAN 坑表：**能改名 ≠ 没被占用，要试着写** |

---

## 6. 建议的执行顺序

按「价值 ÷ 成本」排，一次做一组：

| 轮次 | 内容 | 谁做 | 预计 |
| --- | --- | --- | --- |
| **第 1 轮** | **B1**（LimitFPS 守卫）+ **A1**（门闩） | AI 写 / 你构建；A1 你自己跑 | 20 分钟 |
| **第 2 轮** | **B4**（修那句错话）+ **B2**（删死接口） | AI 改代码 + 改文档 | 20 分钟 |
| **第 3 轮** | **A3**（Release）+ **A4**（clang）+ **A5**（asan）+ **A4b**（决定装不装 ninja） | **你跑**（AI 只根据输出改代码） | 15 分钟（A4b 若装 ninja 另加 0.5h） |
| **第 4 轮** | **B3**（顶栏加 `Max`）+ **A2**（抄量化数据） | AI 改 / 你跑并抄数 | 15 分钟 |
| **第 5 轮** | **B5**（补注释）+ **A6**（可选反向验证） | AI 改 / 你跑 | 15 分钟 |

**关于顺序的三点说明**：

1. **A3/A4/A5 放在第 3 轮**：它们可能挖出真 bug。如果先做 B 组，B 组的新代码
   又成了没被 asan 查过的新债 —— 先让"查错的工具"跑起来，再往上加东西。
2. **B4 排在第 2 轮而不是最后**：修文档不影响任何构建，但它**纠正的是一条假的因果**。
   留着一个"我记得 float 会漂"的错误认知，比少一个用例危险得多。
3. **A4b 本质上不是"债的清理"，是"把一盏没接电的灯接上"**。
   如果决定装 ninja，建议放在**这一轮的最前面** —— 装完之后 A4 才有意义
   （否则 clang 分支跑的还是 `/W4`），而且后续所有构建都变快。

**做到哪一轮算够**：第 1~3 轮做完，S3 的债就只剩"有意推迟"的 C 组了。
第 4~5 轮属于"锦上添花"，什么时候做都行 —— 但**第 3 轮别跳**，
因为它是唯一能发现"我们还不知道的问题"的一步。

---

## 7. 这次盘点的三个额外收获

盘点时从代码里查出、而不是凭印象列的：

### 7.1 一条"零调用者"的接口是上一步自己留的

S3 决策 D11 里写了"刻意不返回 `bytes` —— 加了就是没人用的接口"，
结果同一次提交里又留了 `Time::FrameCount()` 与 `RowDiff::Resize()` 两个零调用接口
（见 B2）。**说明"没人用就删"这条纪律需要在提交前 grep 一次才能落地**，
光靠写决策是记不住的。

### 7.2 步骤 1 的那个 bug 现在仍然可以"无声无息地回来"

`LimitFPS` 从 `sleep_for` 改成 `platform::Wait`，是**靠肉眼看 FPS 抖**发现的。
**当时的 23 个用例全绿**。今天把它改回去，**34 个用例依然全绿**。
这是 B1 排在第一位的全部理由：**能被自动化抓住的错，就不该指望眼睛。**

### 7.3 有两条"从来没跑过"的构建配置（不是缺功能，是缺执行）

盘点时翻了 `CMakePresets.json` 与 `CMakeLists.txt` 才发现的，两条同源：

| 配置 | 写了多久 | 状态 |
| --- | --- | --- |
| `Release` 配置类型 | 从 S0 起（`Debug;Release;...`） | **一次没构建过**（`build/bin/` 下只有 `Debug/`） |
| `-Wall -Wextra -Wpedantic -Wshadow` 的 `else()` 分支 | 从 S0 起 | **一次没生效过**（ClangCL 走 MSVC 分支；没有 ninja 走不到这） |
| `ms-clang-debug` / `ms-asan` 预设 | 从 S0 起 | **一次没配置过** |

**共同点**：它们都"看起来是配好的"，所以没人会去动它。这类债没有报错、没有警告，
只有"某天发现问题时，才发现那道防线一直是空的"。

**这也是 `doc/TECH_DEBT.md` 这份文件本身存在的理由** —— 一个只存在于"当年想起来写了一下"
的地方的配置，等于不存在。**能跑一次的配置，才算配置。**
