# S11.5「送得出去」施工图 —— 交付一个别人能试玩的 demo

> **阶段定位**：插在 **S11（已完成）与 S12（Ray Casting）之间**，本人 **2026-10-16** 提出。
> **为什么用 `.5` 而不是塞进 S12**：S12 是 v0.5 的正主（伪 3D），demo 交付是**展示层/交付层**的事，
> 与 `SHOWCASE_PLAN` 的 6 个动作同源（README · CI · 一页技术亮点 · **可试玩产物** · S12 · 第二个项目）。
> **本阶段的性质**：几乎不写 C++（唯一会碰源码的是"默认窗口尺寸"那一处），主体是**构建变体 + 打包脚本 + 一张玩家卡 + 一次发布**。

---

## 1. 前置事实（2026-10-16 实测核查，不是推测）

| # | 事实 | 出处 | 对本任务的含义 |
| --- | --- | --- | --- |
| 1 | **上屏字面量全是 ASCII**（`game/src/Game.cpp` / `Application.cpp` 里没有非 ASCII 进 `fb.Draw`/`snprintf`） | grep 无命中 | 代码页 936 也不会乱码 |
| 2 | 程序自己 `SetConsoleOutputCP(CP_UTF8)` + `ENABLE_VIRTUAL_TERMINAL_PROCESSING`，退出时还原 | `ConsoleWindows.cpp:162/168/82` | 颜色/转义在**新机器开箱可用**，不需要用户改终端设置 |
| 3 | ⚠️ 项目**没有设置 `MSVC_RUNTIME_LIBRARY`** ⇒ 默认 **`/MD` 动态 CRT** | 三个 `CMakeLists.txt` + `cmake/` grep 无命中 | ★ **头号风险**：外人机器没装 VC++ 2015-2022 运行库 ⇒ exe 双击报"缺少 `MSVCP140.dll`" |
| 4 | ⚠️ **CI 只跑 Debug** | `.github/workflows/ci.yml:46/77/89` | demo 发的是 **Release**，而 Release **没有 CI 覆盖** ⇒ 本阶段顺手补一个 Release job |
| 5 | 程序**不设**窗口尺寸（无 `SetConsoleWindowInfo`/`SetConsoleScreenBufferSize`） | `ConsoleWindows.cpp` grep 无命中 | ⇒ 这条要补（见 §3 分叉 B） |
| 6 | **`Console::Size()` 读的是可视窗口（`srWindow`），不是缓冲区** | `ConsoleWindows.cpp:267-277`（注释写明） | "默认大小"要设**窗口**；缓冲区只在不够大时一起放大 |
| 7 | **`ResolveCanvasSize` 在构造函数初始化列表里读 `console.Size()`**，`FrameBuffer` 紧接着按它建 | `Application.cpp:54-71` + `:75-80` | ★ **"默认尺寸"必须在 `MakeConsole()` 当场生效**，否则第一帧的画布仍是旧尺寸（要靠每帧轮询才追上） |
| 8 | `IsUsableCanvasSide/IsUsableCanvasSize` 是 **`constexpr` 自由函数放在公共头**，用例在 `test_smoke.cpp` 的 `Core` 组 | `Application.h:36-59` | 新决策函数**照抄这个形态**⇒ 纯函数、可离线测（这是 `TECH_DEBT` C7 的产物，别退回 `.cpp` 匿名 namespace） |
| 9 | `Console` 接口只有 3 个纯虚（`Present`/`Size`/`LastFrameRowsSent`），**全项目只有一个实现**（`ConsoleWindows`，经 `MakeConsole()` 创建） | `Console.h:38-60` + 全仓 grep | ⇒ 加第 4 个纯虚**不会破任何东西**（没有测试替身） |
| 10 | **`dumpbin` 在 PATH 上**（VS 2022 的 Hostx64 bin 已在 PATH） | `which dumpbin` ✓ | "零依赖"自检**开箱能做**；仍留回退（缺工具时警告而非静默通过） |
| 11 | `.gitattributes` **已把 `*.cmd`/`*.bat`/`*.ps1` 设成 CRLF**、`*.zip` 设成 binary | `.gitattributes` | ⇒ 批处理的换行坑**已经有人管**，本阶段**不用加规则**；剩下的坑是"**中文放进 `.cmd`**"（见 §6 坑 3） |
| 12 | ⚠️ **仓库目前仍非匿名可读**（`api.github.com/.../MiniEngine` → **HTTP 404**，2026-10-16 探测） | 只读 API 探测 | ⇒ **"Release 能被外人下载"这条还没成立**，要在发布前复核（见 §11 待确认 T1） |
| 13 | **本机没有 `gh` CLI** | `which gh` 无输出 | 建 Release + 传资产走**网页 UI**（不用 PAT 走 REST） |
| 14 | 仓库**没有 `tools/` 目录**；`.gitignore` 忽略 `build-*/` 但**没有** `dist/` | `ls`/`cat` | 新目录 `tools/`；`.gitignore` 加 `dist/` |

---

## 2. 目标 / 明确不做什么

**目标**：一个**没装 VS、没装 CMake、不认识这个项目**的人，在 Windows 上 **解压 → 双击 → 5 分钟内玩起来**，
并且**出了问题能把证据带回来**（按 `F4` 生成日志发回）。

| 不做 | 为什么 |
| --- | --- |
| 安装器 / MSI / 自动更新 | 与"终端小 demo"的体量不匹配 |
| 代码签名 | 证书成本 >> 收益；改用玩家卡写明"SmartScreen → 更多信息 → 仍要运行" |
| Linux/macOS 版 | 硬约束：只做 Windows（`AGENTS.md` §3） |
| 改玩法 / 加教程 / 难度选项 | 本阶段只解决"**送得出去**"，不解决"更好玩" |
| 裁剪调试键（`T`/`F2`/`F7`/`F4`） | 分叉 C 取"保留"：同一份 exe，玩家卡只讲玩法 |
| 32 位 / arm64 | 只发 x64 |
| CI 自动建 Release | 本版不做（无 `gh`；人工点一次更快） |

---

## 3. 接口决策（**分叉 A~E 已于 2026-10-16 拍板**）

| 分叉 | 决定 | 落地形态 |
| --- | --- | --- |
| **A** 静态 CRT 怎么给 | **选项 + 预设**（默认 OFF） | 根 `CMakeLists.txt` 加 `option(MINI_STATIC_CRT ... OFF)`；打开时设 `CMAKE_MSVC_RUNTIME_LIBRARY = "MultiThreaded$<$<CONFIG:Debug>:Debug>"`。⚠️ **必须在 `add_subdirectory` 之前设**（变量在目标创建时被捕获） |
| **B** 窗口尺寸谁设 | **程序侧设，默认 100×30** | 见下面的分层表；启动器**不再**设尺寸（避免两个真相） |
| **C** 调试键 | **保留** | 同一份 exe；卡上不宣传调试键，只留一句"按 `F4` 生成反馈日志" |
| **D** 发布 | **git tag + GitHub Release**（不只是 `.gitignore`） | tag `v0.4.0`（annotated）；资产 = 打包好的 zip；⚠️ 前置 = **仓库转 public**（事实 12） |
| **E** CI Release job | **加** | 新增一个 job（Release 构建 + 跑用例 + ctest），与 Debug job 并列 |

### 3.1 分叉 B 的分层（**唯一会碰 C++ 的地方**）

沿用既有先例（`IsUsableCanvasSize` = "**策略在 `core/`、机制在 `platform/`**"，事实 8）：

| 层 | 新增/改动 | 职责 | 签名/形态 |
| --- | --- | --- | --- |
| `engine/include/mini/core/Application.h` | 新增常量 + **纯函数** | **策略与决策** | `inline constexpr platform::ConsoleSize kDefaultCanvas{100, 30};`<br>`[[nodiscard]] constexpr platform::ConsoleSize ComputeSizeRequest(platform::ConsoleSize current, platform::ConsoleSize want) noexcept;`<br>语义：**逐轴取 max**（只放大不缩小；某轴已够就保持） |
| `engine/include/mini/platform/Console.h` | 新增第 4 个纯虚 | **机制契约** | `[[nodiscard]] virtual bool TryResizeWindow(ConsoleSize want) = 0;`（**尽力而为**：`false` = 系统不让改，**不致命**） |
| `engine/src/platform/ConsoleWindows.cpp` | 实现 + 私有 helper | **Windows 实现** | 读 `GetConsoleScreenBufferInfo`；**需要时先** `SetConsoleScreenBufferSize`、**再** `SetConsoleWindowInfo`；任一失败 ⇒ 返回 `false` |
| `engine/src/core/Application.cpp` | 新增匿名 namespace 的自由函数 + **改一处初始化列表** | **在 `FrameBuffer` 之前生效**（事实 7） | `std::unique_ptr<platform::Console> MakeConsoleWithDefaultSize(...)`：`MakeConsole()` → `TryResizeWindow(ComputeSizeRequest(c->Size(), kDefaultCanvas))`（**忽略返回值**）→ 返回 |

**尺寸为什么取 100×30**：HUD 第 2 行 **64 列** + overlay 面板 ~30 列（右对齐）+ 留白 ⇒ 宽要有余量；
行数上：顶栏 1 + HUD 2，而 **`F7` 面板打开时底部要 ~12 行** ⇒ 30 行时世界仍有 ~17 行。
（80×25 是边界可用档；常量只有一处，想换随时换。）

**⚠️ 连带后果（必须写进文档）**：程序一旦自设默认尺寸，`S11_CHECKLIST` **§16.2 那组"小窗口（60×18）基线"就不再可作对照**了
⇒ 要么重采一次基线，要么在 §16 里标注"仅作历史记录"。人眼清单第 16 条仍有效（程序只在**启动时**保证下限，之后可手动拖小）。

### 3.2 分叉 D 的发布决策

- **tag 名**：`v0.4.0`（annotated）。⚠️ 说明里要交代：**版本串仍是 `0.4.0-dev`** —— 按 `Version.cpp` 的约定，`-dev` 表示"**已达成 v0.4、树在往 v0.5 走**"，与 tag 不矛盾。
- **资产名**：`MiniEngine-Demo-v0.4.0-windows-x64.zip`。
- **正文落点**：新建 **`doc/RELEASE_NOTES_v0.4.md`**（本人 2026-10-16 同意），Release 页面粘贴它。
- **发布步骤**（无 `gh`，走网页）：`Settings → 转 public` → `git tag -a v0.4.0` + `git push origin v0.4.0` → `releases/new` 选 tag → 粘正文 → 拖 zip → Publish。

---

## 4. 文件清单

### 新增（7 个）

| 文件 | 作用 |
| --- | --- |
| `tools/pack_demo.ps1` | 打包：构建 → 依赖自检 → 组目录 → 压缩 → 打印 |
| `tools/demo/开始试玩.cmd` | 玩家入口：`cd /d "%~dp0"` → `chcp 65001` → 优先 `conhost.exe` 起 exe → `pause` |
| `tools/demo/玩法与已知限制.txt` | 玩家卡（UTF-8，**中文在这里**，不在 `.cmd` 里） |
| `doc/S11_5_DEMO_CHECKLIST.md` | 本文件 |
| `doc/RELEASE_NOTES_v0.4.md` | Release 正文（**最后一步写**，填真实数字） |
| `dist/`（git 忽略） | 打包产物目录 |
| `.github/workflows/ci.yml` 的 Release job | 在既有文件里加，不新增文件 |

### 改动（5 个）

| 文件 | 改什么 | 为什么 |
| --- | --- | --- |
| `CMakeLists.txt`（根） | `MINI_STATIC_CRT` 选项 | 零运行库依赖 |
| `CMakePresets.json` | `ms-msvc-release-static` 配置预设 + `release-static` 构建预设 | 一条命令拿到可分发的构建 |
| `engine/include/mini/core/Application.h` | `kDefaultCanvas` + `ComputeSizeRequest` | 默认尺寸的策略（纯函数、可测） |
| `engine/include/mini/platform/Console.h` | `TryResizeWindow` 纯虚 | 机制契约 |
| `engine/src/platform/ConsoleWindows.cpp` | `TryResizeWindow` 实现 | Windows 实现 |
| `engine/src/core/Application.cpp` | `MakeConsoleWithDefaultSize` + 初始化列表改用它 | 在 `FrameBuffer` 之前生效 |
| `tests/test_smoke.cpp` | `Core` 组 +4 条用例 | 钉住 `ComputeSizeRequest` 的边界 |
| `README.md` / `doc/MiniEngine_PLAN.md` / `doc/TECH_DEBT.md` / `doc/SHOWCASE_PLAN.md` / `AGENTS.md`§2 表 / `.gitignore` | 见 §10 | 文档一致 |

---

## 5. 步骤切分（6 步，每步都能单独验证）

| 步 | 内容 | 产物 | 谁做 | 验收 |
| --- | --- | --- | --- | --- |
| **1** | 静态 CRT：选项 + Release 静态预设；顺手把 `dumpbin` 自检写成脚本里的一个函数 | **零依赖 exe** | AI 写 / 本步你构建 | `dumpbin /dependents` 里**无** vcruntime/msvcp；`216 passed` 不变 |
| **2** | 分叉 B：`kDefaultCanvas` + `ComputeSizeRequest` + `TryResizeWindow` + 初始化列表接线 + 4 条用例 | 启动即 ≥100×30（⚠️ **只在经典控制台**） | AI 写 / 你构建 | ✅ 已验：新用例 4 条绿（216→220）；**`conhost.exe` 里**窗口变成 100×30 ✓；⚠️ ConPTY 里改不动（坑 5，预期） |
| **3** | `pack_demo.ps1` + `开始试玩.cmd` + 玩家卡 + `.gitignore` 加 `dist/` | **可发的 zip** | AI 写 / 你跑 | ✅ **2026-10-16 脚本跑通**（首次产出 zip，你回报"产物在本机可以运行"）· ⚠️ **跨机验收留到回家**（本人定：本机无第二环境，"暂且认为通过"）|
| **4** | 验收：**换机器 / 换 Windows 账户**实跑；2 场反向验证 | 验收记录 | 你跑 | 见 §8 |
| **5** | 转 public 复核 → `git tag v0.4.0` → 建 Release → 传 zip | 可下载的 demo | 你（tag 我可代跑） | ① Release **匿名可打开** ② 下载包 SHA256 与本地一致 |
| **6** | 文档收口 + `RELEASE_NOTES_v0.4.md`（填真实数字） | 文档一致 | AI 写 | 见 §10 |

---

## 6. 坑表（编号沿用本项目的"判据"写法）

| # | 坑 | 会怎样 | 判据/对策 |
| --- | --- | --- | --- |
| 1 | **`CMAKE_MSVC_RUNTIME_LIBRARY` 设晚了** | 目标已创建 ⇒ 选项**静默无效**，你以为静态了其实还是 `/MD` | `dumpbin /dependents` 里**必然**还能看到 `VCRUNTIME140.dll` ⇒ 就是它 |
| 2 | 静态 CRT 与 **ASan** 不兼容 | `-DMINI_STATIC_CRT=ON` + `ms-asan` 预设 ⇒ 配置期/链接期报错 | 两者**永不组合**（脚本固定用 Release 静态、CI 的 asan 保持默认） |
| 3 | **中文写进 `.cmd`**（936 代码页解析 UTF-8 批处理） | `echo` 出乱码，甚至**命令解析错** | `.cmd` 正文**只写 ASCII**；中文全放 `.txt`（事实 11 已解决换行，**没解决编码**） |
| 4 | `conhost.exe <exe>` 起法未必给到经典控制台 | 长按移动可能仍退化 | ✅ **2026-10-16 实测：给到经典控制台**（`start "" conhost.exe "<exe>"` ⇒ 经典窗口 + 尺寸功能生效）⇒ **启动器优先 conhost 这条从"nice to have"升格为必需**（它一次解决**两个**问题：key-up 长按 + 窗口尺寸） |
| 5 | Windows Terminal / ConPTY 里 `SetConsoleWindowInfo` 失败 | 尺寸不生效（**不是错**） | ✅ **2026-10-16 实测确认**（本人回报：`conhost.exe` 起来**能放大**、**PowerShell 里直接跑不变** —— 那个会话是 ConPTY）。机制：窗口归**客户端**（WT）管，pseudo console 不认这类调用 ⇒ `TryResizeWindow` 返回 `false`，调用方**忽略**（不崩、不报错）。⇒ **兜底三件套**：① 启动器优先 `conhost.exe`（见坑 4）；② 启动器里 `mode con: cols=100 lines=30`（经典控制台有效、ConPTY 下无害）；③ 若画布仍小到放不下 HUD，**标题屏多打一行提示**（"窗口太小：建议 ≥100×30"） |
| 6 | 只放大不缩小的边界 | 用户窗口 200 列时被**拉窄**了 | 用例钉"某轴已够就保持"（§7 第 4 条） |
| 7 | 缓冲区 < 窗口 | `SetConsoleWindowInfo` 返回 `ERROR_INVALID_PARAMETER(87)` | 顺序：**先** `SetConsoleScreenBufferSize`（只在不够时）**再** `SetConsoleWindowInfo` |
| 8 | `dumpbin` 不在 PATH（别人机器/精简环境） | 自检步骤失败或静默跳过 | 脚本里：找不到就**打印警告并继续**，但**不许**当成"通过" |
| 9 | 打包脚本污染开发构建 | 把 `build/` 弄脏、或生成物混进源码目录 | 用**独立** `build-demo/`（`.gitignore` 的 `build-*/` 已覆盖）；只写 `dist/` |
| 10 | zip 里少文件 / 多文件 | 玩家缺 exe 或缺卡 | 脚本**打印文件数与体积**，并对"exe 存在"做硬断言 |
| 11 | `.cmd` 结束不含 `pause` | 崩了窗口一闪就没，玩家以为"双击没反应" | 启动器**必须**以 `pause` 收尾（demo 的第一 UX 要点） |
| 12 | 未签名 exe 被 SmartScreen 拦 | 玩家放弃 | 卡里写"更多信息 → 仍要运行" |
| 13 | **Release 从没在 CI 上跑过** | 发布的那个配置**从没被自动验证** | 分叉 E 的 Release job；发布前必须绿 |
| 14 | 版本串与 tag 不一致的误读 | 有人以为发错了版本 | Release 正文里写明 `-dev` 的约定（§3.2） |
| 15 | **`§16.2` 基线失效** | 拿"插桩前 0.5ms"跟新尺寸下的数比 | 文档里标注"仅作历史记录"或重采（§3.1 连带后果） |

---

## 7. 测试计划

### 7.1 新增用例（4 条，纯函数，放在 `tests/test_smoke.cpp` 的 `Core` 组）

| # | 用例意图 | 期望 |
| --- | --- | --- |
| 1 | 当前**已够大** | 请求 = 当前（**不许缩小**） |
| 2 | 两轴都小 | 请求 = `kDefaultCanvas` |
| 3 | 只有一轴小 | **只放大那一轴**（另一轴原样） |
| 4 | 某轴更大、另一轴更小 | 大的保持、小的补齐（**逐轴 max**） |

（放 `Core` 组的理由：同主题的 `CanvasSizeAcceptanceIsBoundarySafe` 就在那儿，事实 8。若你想单独成文件 ⇒ 记成偏差。）

### 7.2 反向验证（2 场，按 S10/S11 的纪律）

| 场 | 改坏什么 | 预测 |
| --- | --- | --- |
| **R1** | 把 `MINI_STATIC_CRT` 关掉重跑打包 | `dumpbin` 输出里**应当重新出现** `VCRUNTIME140.dll` ⇒ 证明这道自检**真的在看东西** |
| **R2** | 跳过 `MakeConsoleWithDefaultSize` 里的 `TryResizeWindow` 调用 | 小窗口启动 ⇒ **应当看到 HUD 折行 / 画布很小** ⇒ 证明"默认尺寸"不是空话 |

---

## 8. 验收标准

| 类型 | 内容 |
| --- | --- |
| **一条命令** | `powershell -File tools\pack_demo.ps1` ⇒ `dist\MiniEngine-Demo-v0.4.0-windows-x64.zip` + 打印文件数/体积 |
| **一个现象** | **另一台机器 / 另一 Windows 账户**（不装 VS/CMake）：解压 → 双击 `开始试玩.cmd` → 标题屏 → `ENTER` → 能玩 5 分钟（移动/开火/暂停/死亡/重开）；**窗口自动 ≥100×30**（⚠️ 靠启动器的 `conhost.exe` + `mode con` 保证 —— **ConPTY 里程序自己改不动**，见坑 5） |
| **一个数字**（2026-10-16 实测） | exe 依赖表 **0 个 vcruntime/msvcp**（只剩 `KERNEL32.dll` 一类系统 DLL）· 静态 Release exe **352,768 B（344 KB）** · zip **178,705 B（174 KB）/ 3 个文件** · 用例 **220**（216 + 第 2 步的 4 条；`Core` 组 2 → **6**） |
| **发布验收** | ① Release 页面**匿名可打开**、资产可下载（转 public 后）② 下载包 **SHA256 与本地 `dist/` 一致**（证明"发出去的确实是那个包"） |
| **反向验证** | §7.2 的 R1 / R2（预测 → 实测 → 逐条比对 → 还原） |

---

## 9. commit 切分（预计 5 个）

| # | commit | 内容 |
| --- | --- | --- |
| 1 | `build:` | 静态 CRT 选项 + Release 静态预设（+ CI Release job，若同批） |
| 2 | `feat:` | `kDefaultCanvas` + `ComputeSizeRequest` + `TryResizeWindow` + 接线 + 4 条用例 |
| 3 | `build:` | `tools/pack_demo.ps1` + 启动器 + 玩家卡 + `.gitignore` |
| 4 | `ci:` | Release job（若第 1 步没顺手做） |
| 5 | `docs:` | README/PLAN/TECH_DEBT/SHOWCASE_PLAN/AGENTS.md§2 + `RELEASE_NOTES_v0.4.md` |

---

## 10. 需要同步的文档

| 文件 | 改什么 |
| --- | --- |
| `README.md` | 加"**试玩**"一节（下载/打包命令 + 键位 + 三条限制），并说明 zip 在哪取 |
| `doc/MiniEngine_PLAN.md` | 阶段表插入 **S11.5** 一行（目标/产物/验收）；G 节注明"demo 不发新版本号（仍是 `0.4.0-dev`）" |
| `doc/TECH_DEBT.md` | 登记"**不做代码签名 / 不做安装器**"为**有意接受**（带触发条件）；`§16.2` 基线可比性一句话修正 |
| `doc/SHOWCASE_PLAN.md` | 动作清单插一条"**可试玩产物**"（它与 GIF、README 同目的：让外人 3 分钟上手） |
| `AGENTS.md` | §2 的文档表加 `doc/S11_5_DEMO_CHECKLIST.md` 一行（**它是索引，不能漏**） |
| `doc/S11_CHECKLIST.md` | §16.2 加"⚠️ 2026-10-16 起默认窗口 100×30 ⇒ 这组小窗口基线仅作历史记录" |
| `.gitignore` | 加 `dist/` |

---

## 11. 风险与待确认

| # | 项 | 状态 |
| --- | --- | --- |
| **T1** | ~~⚠️ 仓库尚未转 public~~ ⇒ ✅ **2026-10-16 本人已完成**（API 探测：`private=false` / `visibility=public` / `pushed_at=2026-10-16T05:21:39Z`），tag 与 Release 均已对外可见 | 原操作路径留档：`Settings → General → Danger Zone → Change repository visibility → Make public`。<br>⚠️ **公开前审计**：`CI_CHECKLIST` §11 的 7 条已于 2026-10-13 结案（0/1/2 邮箱**本人拍板接受**、3/4/5 已脱敏、6/7 无需处理）；本次又对**新增/改动文件**复扫（盘符 / `C:\Users` / 邮箱 / 代理端口）⇒ 只有 GitHub 用户名命中（属公开面）→ 干净。<br>⚠️ **代价已认**：公开后每个 commit 显示真名 + 公司邮箱（131 个历史 commit 永久可见） |
| T2 | `conhost.exe <exe>` 起法是否真给到经典控制台 + key-up | ✅ **2026-10-16 已实测**：`start "" conhost.exe "<exe>"` 给出经典控制台，**窗口尺寸功能生效**（结论写在 §6 坑 4）；同批还查明 ConPTY 里改不动（坑 5）。⇒ **启动器优先 conhost 这条从"nice to have"升格为必需** |
| T3 | 第二台机器/第二个账户的实测 | ⏳ **待做**（本人 2026-10-16 定：本机无第二环境，"暂且认为通过"，回家再验）。本阶段第 4 步的硬要求 —— demo 的**唯一真验收** |
| T4 | 静态 CRT 会让 exe 变大多少 | ✅ 实测：**352,768 B（344 KB）**（对照：Debug 动态 CRT 327 KB）；zip **178,705 B** ⇒ 已写进 `RELEASE_NOTES_v0.4.md` §6 |
| T5 | `§16.2` 基线失效的处置（重采 or 标注） | ✅ **已标注**（`S11_CHECKLIST` §16.2 顶部加了"自 2026-10-16 起仅作历史记录"）；重采留到 S12 前 |

## 11.1 ★ 发布记录（2026-10-16 · 已核过）

**本人已完成**：转 public → `git tag -a v0.4.0` → 建 Release → 上传 zip。

| # | 检查项 | 实测结果 |
| --- | --- | --- |
| 1 | 仓库可见性 | ✅ `private=false` · `visibility=public` |
| 2 | tag | ✅ `v0.4.0` 是 **annotated**（`f2b065b`），远端有 `refs/tags/v0.4.0` + `^{}` ⇒ **已推**；⚠️ 指向 **`ad7b6ac`（第 2 步）** |
| 3 | Release | ✅ 已发布（`draft=false` · `prerelease=false` · `target_commitish=main`） |
| 4 | 资产 | ✅ `MiniEngine-Demo-v0.4.0-windows-x64.zip` · **178,705 B** |
| 5 | ★ **资产 = 本地产物**（§8 发布验收第 ② 条） | ✅ 下载回来算 SHA256 = **`4af8d1d5489fa24337cc1d41bf46dfd284b452d8e0926a28aeffff75b4946354`**，与 `dist/` 那份**逐字节相同**（⚠️ 与首次打包的 `62b1bfd0…` 不同 ⇒ 正是**重打包**（把卡里的 216 改成 220）的证据） |
| 6 | 包内三件套 | ✅ 3 个文件；玩家卡写着「当前 **220** 条单元用例全绿」；启动器 **CRLF=28 / 非 ASCII 字节 = 0** |

**⚠️ 两项待补（不阻塞"能玩"，阻塞"能看懂"）**

| # | 项 | 处置 |
| --- | --- | --- |
| **A** | **Release 的标题与正文都是空的**（`name=""`、`body_len=0`）⇒ 对外页面没有玩法/键位/已知限制 | 编辑 Release：标题填 `MiniEngine v0.4「看得清」—— 终端里的小游戏 demo`；正文贴 `doc/RELEASE_NOTES_v0.4.md`；§6 的校验行填 **`4af8d1d5…6354`**（= 已上传资产的哈希） |
| **B** | **tag 指向 `ad7b6ac`（第 2 步），不含第 3 / 6 步**（打包脚本、启动器、玩家卡、Release 正文来源、文档收口当时**都还没提交**）⇒ `checkout v0.4.0` 看不到"包是怎么打出来的" | 提交 ③④ 之后：`git tag -f -a v0.4.0 -m "…"` + `git push -f origin v0.4.0`。⚠️ **为什么这次可以动已发布的 tag**：发布仅几分钟 · **`downloads=0`** · 我们自己发现 · **资产哈希不受影响**（tag 只决定源码树）；一般规则仍是"已发布的 tag 不要动"，这次是明确的例外 |


---

## 12. 本阶段"完成的定义"

1. 一条命令产出 zip；**另一台机器**解压双击能玩（现象）
2. exe 依赖表 0 个 vcruntime/msvcp（数字）；2 场反向验证跑完
3. `git tag v0.4.0` + Release 已发布，**匿名可打开、资产可下载、SHA256 一致**
4. 四份文档 + `AGENTS.md` §2 + `.gitignore` 全部同步；`RELEASE_NOTES_v0.4.md` 用**实测数字**（不是占位符）
