# CI 施工图与进度板 —— GitHub Actions + MSVC + ctest

> 本文件是 `SHOWCASE_PLAN` **动作 2（CI + badge）** 的施工图 + 进度板 + 落地记录，
> 格式对齐 `doc/S10_CHECKLIST.md`。⚠️ 与 `AGENTS.md` 冲突时**以 `AGENTS.md` 为准**。
> ⛔ **它不改任何 C++ / CMake / 测试代码** —— 只新增 1 个 workflow 文件 + 文档同步。

---

## 当前状态

| 项 | 状态 |
| --- | --- |
| 本文件 | ✅ **2026-10-13 落纸**（方案 + 已落地的 workflow + 公开前审计） |
| `ci.yml` | ✅ **已写**（`.github/workflows/ci.yml`，97 行）· ⬜ **未推、未首跑** |
| 验收 | ⬜ 待本人 `git push` 后看 Actions 页面（见 §9） |
| 反向验证 | ⬜ 待跑（R1：故意改坏一条用例 ⇒ CI 必须红） |
| 仓库可见性 | ⬜ **本人 2026-10-13 决定：改为公开** —— ⚠️ **翻转前先看 §11 的审计** |

## 分工约定（沿用 `AGENTS.md` §1.4）

| 谁 | 做什么 |
| --- | --- |
| **AI** | 只编辑 / 新建文件（workflow + 文档）。**不 push、不看 Actions、不跑构建** |
| **本人** | `git push`、看 Actions 页面、回报绿/红与实际数字 |

> ⚠️ **CI 与本地验证最大的不同**：CI 只有**推上去才存在**，因此它天然属于"本人执行"那一侧。

---

## 1. 目标 / 明确不做

| | 内容 |
| --- | --- |
| **目标** | 在 GitHub 托管的 `windows-2022` 上，用**与 README 逐字一致**的命令构建 + 跑 197 条用例；结果变成 badge；从今往后 S11~S14 的每一步都有机器守门 |
| **明确不做** | ⛔ 不跑 `MiniEngine.exe`（需要真控制台、无 TTY —— 人眼验收不可自动化）· ⛔ 不装任何工具（镜像自带 CMake + VS 2022）· ⛔ **不做缓存**（零第三方依赖，没有可缓存的东西）· ⛔ 不做格式检查 / clang-tidy（本项目不用 clang）· ⛔ 不做覆盖率 / 多 OS 矩阵 / 发布产物 / 定时任务 · ⛔ **不改 `CMakeLists.txt` 去迁就 CI** |
| **验收锚点** | 故意改坏一条用例 ⇒ **CI 必须红，且 Actions 页面能指出是哪条**（`SHOWCASE_PLAN` 动作 2 的原话） |

## 2. 前置事实（逐条核过，带出处）

| # | 事实 | 出处 | 对方案的意义 |
| --- | --- | --- | --- |
| 1 | ★★ **`windows-latest` / `windows-2025` 自 2026-06-08 起默认换成 Visual Studio 2026**（6-15 完成迁移）；要留在 VS 2022 必须显式写 `windows-2022` | GitHub changelog 2026-05-14 | **决定性**：本项目生成器是 `Visual Studio 17 2022` |
| 2 | `windows-2022` 镜像预装 **VS 2022 Enterprise 17.14.x** + 较新 CMake + PowerShell 7 | `actions/runner-images` 的 win22 版本说明 | 与三台机器同代 ⇒ CI = **第 4 台机器** |
| 3 | 本项目三条命令**不带 `-G`** ⇒ 生成器是**隐式**的（CMake 选本机已装最高版本 VS） | `README.md` / `AGENTS.md` §4 | 环境假设只能靠 runner 标签承担；需要"观察面"确认它选对了 |
| 4 | `option(MINI_BUILD_TESTS "Build unit tests" **ON**)`；`enable_testing()` + `add_subdirectory(tests)` 都在该条件内 | `CMakeLists.txt:24,65-68` | 裸命令**会**建测试 ⇒ CI 不必传任何 `-D`。⚠️ 默认值若变 OFF，`ctest` 会**静默 0/0** |
| 5 | 生成器是**多配置** ⇒ `--build` 要 `--config`、`ctest` 要 `-C` | `AGENTS.md` §4 | 漏了 = "找不到可执行文件" |
| 6 | `mini_tests.exe` **返回失败用例数**；空选择返回 1 | `tests/test_main.cpp:113-117,91-99` | 天然适配 CI（非 0 即红）；空注册表**不会**假绿 |
| 7 | `add_test(NAME mini_tests COMMAND $<TARGET_FILE:mini_tests>)` ⇒ **一个 exe = 1 个 CTest 用例** | `tests/CMakeLists.txt:50` | `ctest` 期望是 **1/1** 而不是 197 ⇒ **两条都要跑**（见 §6 step 5/6 的分工） |
| 8 | 仓库 `github.com/HUNK200/MiniEngine`；`PLAN` 附录 A.1 记为**私有**，本人 2026-10-13 决定**转公开** | 本人决定 + `PLAN` | 公开后：CI **完全免费无限** + badge 才真正"别人可验证"；⚠️ 但**历史里的字串永久可见** ⇒ 先看 §11 |
| 9 | 源文件 UTF-8 **无 BOM** + LF（`.gitattributes` 强制） | `AGENTS.md` §5 | CI 上加一行 `core.autocrlf=false` 消除一个"本地/CI 不一致"维度 |
| 10 | 本地有 `CMakePresets.json`，但 README 用的是**裸命令** | `CMakePresets.json` | CI 用裸命令（理由见 §5 D8） |

## 3. 交付物

| 类别 | 文件 | 规模 |
| --- | --- | --- |
| **新增** | `.github/workflows/ci.yml` | **97 行**（含注释） |
| **新增** | `doc/CI_CHECKLIST.md`（本文件） | — |
| 改 | `README.md`（badge + 一句说明） | +3 行 |
| 改 | `doc/SHOWCASE_PLAN.md`（动作 2 状态 + ★ 坑） | ~8 行 |
| 改 | `doc/TECH_DEBT.md`（**关 C5** + 新增 1 条） | ~8 行 |
| 改 | `AGENTS.md` §2（补本文件一行） | +1 行 |
| **不动** | `CMakeLists.txt` · `engine/` · `game/` · `tests/` · `CMakePresets.json` | **0 行** |

## 4. 依赖与边界（`AGENTS.md` §3 的判定）

| 问题 | 判定 | 写在哪 |
| --- | --- | --- |
| GitHub Actions 违反"不引入第三方库"吗？ | **不违反**：它是**构建服务**，不进产物、不改 CMake、不新增编译依赖 | `ci.yml` 文件头 + README |
| `uses:` 呢？ | ⚠️ **确实是把别人的代码拉进流水线**（两个都是 GitHub 官方维护）⇒ 所以 **pin 到 40 位 commit SHA** | 同上 |
| 只跑 MSVC？ | ✅ 必须（`PLAN` v1.0 原话；clang 已于 2026-09-29 全线移除） | — |

## 5. 接口设计（= `ci.yml` 的"公开面"）

| 决策 | 取法 | 理由 / 代价 |
| --- | --- | --- |
| **D1 触发** | `push(main)` + `pull_request(main)` + `workflow_dispatch` | 手动补跑：被 `paths-ignore` 跳过的 run 靠它 |
| **D2 跳过文档** | `paths-ignore: doc/**, **/*.md, AGENTS.md, .agents/**` | 本仓库一半 commit 是文档。⚠️ 副作用：这类 push 不跑 CI ⇒ badge 停在旧颜色 |
| **D3 `pull_request` 不过滤路径** | 见 `ci.yml` | 避免"必需检查被跳过"那一类麻烦（虽然目前没开 required checks） |
| **D4 权限** | `permissions: { contents: read }` | 最小权限；私有/公开仓库 checkout 都用内置 `GITHUB_TOKEN`，不需要 PAT |
| **D5 并发** | `concurrency` + `cancel-in-progress: true` | 连推多个 commit 时只跑最后一个 |
| **D6 超时** | `timeout-minutes: 20` | 防挂死（GitHub 硬上限 6 小时） |
| **D7 job 结构** | **v1 只一个 job（Debug）** | 首跑要一次成功；Release 走 v2（§12） |
| **D8 生成器** | **不传 `-G`** —— 保住"CI 命令 = README 命令"这条命根子 | 代价：生成器是隐式的 ⇒ 用 step 3 的日志当观察面 |
| **D9 action 取法** | **两个都 pin SHA**：`actions/checkout@df4cb1c0…`（v6.0.3）· `actions/upload-artifact@cf430e03…`（v7.0.2） | 供应链纪律；代价：升级要手动（§8 给了取 SHA 的命令） |

## 6. 逐步骤（每步：命令 / 作用 / 对应本地哪一条）

| # | 步骤 | 命令 | 它做什么 · 不写会怎样 |
| --- | --- | --- | --- |
| 1 | Checkout | `actions/checkout@<sha>`（`fetch-depth: 1`） | 取源码；浅克隆就够（CI 不需要历史） |
| 2 | **核版本** | `git config --global core.autocrlf false`<br>`cmake --version`<br>`vswhere -latest -property installationVersion` | ★ `AGENTS.md` §4「每台机器第一步：核版本」在 CI 上的落点。⚠️ **只打印不断言**（三台机器 cl 版本本来就不同）。vswhere 找不到时**不失败**（它只是环境探测） |
| 3 | 配置 | `cmake -S . -B build` | 与 README 逐字一致。★ 输出的 `Building for: Visual Studio 17 2022` 就是**生成器的观察面** |
| 4 | 构建 | `cmake --build build --config Debug` | `--config` 不能漏（多配置生成器） |
| 5 | **跑用例（拿尾行）** | `.\build\bin\Debug\mini_tests.exe` | ★ 给出本项目口径的 `197 passed, 0 failed`；**并且**它是"测试有没有被编译进来"的**响亮**守卫（事实 4） |
| 6 | **跑 ctest** | `ctest --test-dir build -C Debug --output-on-failure` | 验证 CTest 集成（`-C` 不能漏）。期望 `1/1` |
| 7 | 失败留证 | `if: failure()` → `upload-artifact`（`LastTest.log`） | 日志几 KB，不占配额；失败时能直接在页面看 CTest 原始输出 |

> ⚠️ **step 5 与 step 6 都在是故意的**：`ctest` 只会说 `1/1`（一个 exe = 一个用例，事实 7），
> **说不出 `197`**；而直接跑 exe 看不到 CTest 集成。**两个都要，各管一件事。**

## 7. 坑表

| # | 坑 | 症状 / 判据 | 是不是代码错 |
| --- | --- | --- | --- |
| 1 | ★★ **写了 `windows-latest`** | 它现在是 **VS 2026**（2026-06 起）⇒ step 3 报生成器找不到，**或**静默用上与你三台机器都不同的工具链。判据：step 3 的 `Building for:` 必须是 **`Visual Studio 17 2022`** | ❌ 配置错 |
| 2 | 漏 `--config` / `-C` | step 4 报找不到配置；step 6 报 "Unable to find executable" 或 `0 tests` | ❌ 命令错 |
| 3 | `ctest` 报 **0/0** 而非 1/1 | `MINI_BUILD_TESTS` 变 OFF ⇒ 测试**根本没建**。⇒ 这就是 step 5 必须在的原因（它会红） | ⚠️ 配置变更 |
| 4 | PowerShell 语法 | runner 默认 shell 是 **pwsh**：不能用 `./a/b`、`echo $?`；`\` 路径可原样写。判据：报错是不是"命令未找到" | ❌ 语法错 |
| 5 | **首次运行的小坑** | 路径 / 引号 / 转义。**预留一次迭代是正常的** —— 首跑红 ≠ 方案错 | ⚠️ 预期内 |
| 6 | 在 CI 里装工具（choco / winget） | 一装，CI 与本地就不再是同一环境 ⇒ "验证 README"的价值归零。**本方案一个工具都不装** | ❌ 纪律 |
| 7 | `paths-ignore` 的副作用 | 纯文档 push 不触发 ⇒ badge 停在旧颜色，**不代表"刚验过"** | ⚠️ 语义 |
| 8 | ★ **`windows-2022` 标签也会过期** | VS 2022 未来从镜像撤出 ⇒ CI 会在**没改代码**的情况下红。⇒ **已登记成债**（`TECH_DEBT` **C36**） | ⚠️ 外部变化 |
| 9 | 行尾差异 | `.gitattributes` 强制 LF；runner 上 `core.autocrlf` 若为真会改写 ⇒ step 2 一行消除 | ⚠️ 环境 |
| 10 | vswhere 位置变了 | step 2 打不出 VS 版本（但**不会**让 CI 红，因为写了 `Test-Path` 分支） | ⚠️ 环境 |
| 11 | **计费口径我不确定** | GitHub 在 2025-12 的计费改版里**撤掉了 1/2/10 那张倍率表**，改成按 SKU 的美元价；"跨 OS 怎么扣免费分钟"官方**没明说**（我查到的两份资料互相矛盾）⇒ 按 §10 的上界估 | ⚠️ 资料冲突 |

## 8. `SHA pin` 记录与升级流程

| action | pin 的 SHA | 版本 | 来源 |
| --- | --- | --- | --- |
| `actions/checkout` | `df4cb1c069e1874edd31b4311f1884172cec0e10` | **v6.0.3** | `git ls-remote` 的 `refs/tags/v6.0.3^{}`（**权威**） |
| `actions/upload-artifact` | `cf430e030ddbb5b0abf93d22962f4752f3646cd9` | **v7.0.2** | `git ls-remote` 的 `refs/tags/v7.0.2`（**权威**） |

**升级 / 复核用这一条命令**（用 `git` 就行，本项目不装 `gh`）：

```bash
git ls-remote --tags https://github.com/actions/checkout.git | grep -E 'refs/tags/v[0-9.]+(\^\{\})?$'
```

⚠️ 两条纪律：① **带 `^{}` 的那个才是 commit SHA**（不带的是 annotated tag 对象）；② 换 SHA 时**注释里的版本号必须跟着改**（`# v6.0.3` 是给人看的唯一线索）。
⚠️ 已知未取：`actions/checkout` 已有 **v7.0.0**（`9c091bb2…`），但它的变更说明我没读到 ⇒ **先留在成熟的 v6.0.3**，要升是一行的事。

## 9. 验收

**正向**

| # | 看什么 | 期望 |
| --- | --- | --- |
| 1 | Actions 页面出现 run，与 badge 同名 | ✅ 绿 |
| 2 | step 2 输出 | `cmake version` **≥ 3.20**；VS 版本 **17.x** |
| 3 | step 3 输出 | 含 **`Building for: Visual Studio 17 2022`** |
| 4 | step 5 输出 | 尾行 **`197 passed, 0 failed`** |
| 5 | step 6 输出 | **`100% tests passed, 1 tests passed out of 1`** |
| 6 | README 顶部 badge | 显示 `passing`（肉眼） |

**反向验证**（同 S10 §10.3 的纪律：**预测 → 实测 → 比对 → 还原**）

| 场次 | 破坏方式 | 预期 |
| --- | --- | --- |
| **R1** | 把 `tests/test_smoke.cpp:28` 的期望值改成 `"0.3.1-dev"` | step 5 红，日志指出 **`[FAIL] Core.VersionIsReported`**；step 6 也红（同一个 exe） |
| **R2**（可选） | 删掉 step 4 的 `--config Debug` | 期望红在**构建**（证明坑表 2 是真的） |

⚠️ R1 跑完必须 `git checkout -- tests/test_smoke.cpp` **并再推一次**，确认回到绿。

**首跑记录（本人抄录）**

| # | 项 | 实测 |
| --- | --- | --- |
| 1 | run 结论 | ⬜ 待抄 |
| 2 | step 3 的 `Building for:` | ⬜ 待抄 |
| 3 | step 5 尾行 | ⬜ 待抄 |
| 4 | step 6 结果 | ⬜ 待抄 |
| 5 | 整轮实际耗时（分钟） | ⬜ 待抄（用来核 §10 的估算） |

## 10. 成本

| 情况 | 数字 |
| --- | --- |
| **仓库转公开后** | ✅ **标准 runner 上完全免费、无额度上限** ⇒ 成本不再是任何决策因素（`SHOWCASE_PLAN` 动作 2 的"越早越好"因此更成立） |
| 若仍私有 | Free 计划 2,000 分钟/月；一次 job ≈ 2~3 分钟 ⇒ 每周 5 推 ≈ 130 配额分钟/月（**≈ 6.5%**）；超了 Windows 约 $0.010/分钟 |
| ⚠️ 不确定项 | 见坑表 11（跨 OS 扣分钟的官方口径在 2025-12 改版后不明确） |

## 11. ★ 公开前的审计（2026-10-13 AI 用 `git grep` 扫的）

> ⚠️ **为什么必须在翻转可见性之前做完**：一旦公开，**这些字串在 git 历史里永久可见** ——
> 事后再删文件也不管用（除非 rewrite history）。所以这一步是**先审计、后公开**。

| # | 命中 | 内容 | 建议 |
| --- | --- | --- | --- |
| 1 | `doc/MiniEngine_PLAN.md:33` | 🔴🔴 **公司邮箱 + 雇主**：`Wuyangzhi <wu.yangzh@aerok.com.cn>` | **必须处理**：改成 `公司机（身份略）` 或 `<work-email>`。⚠️ 这条最敏感 |
| 2 | `doc/MiniEngine_PLAN.md:33,1354` | 🟡 **个人邮箱** `wuunj@126.com` | 你定：公开个人邮箱通常可接受（它是 git commit 的 author 信息，公开仓库里本来也能看到）⇒ 但要知道它会被搜索引擎收录 |
| 3 | `.agents/skills/work-mode/SKILL.md:13` | 🟠 `d:/Project/fish/c++_simple_engine/…` + `D:\WorkPlace\MiniEngine` | **违反 `AGENTS.md` §5"绝不提交任何含绝对路径的文件"** ⇒ 改成 `<仓库根>` 占位 |
| 4 | `doc/MiniEngine_PLAN.md:1481` | 🟠 **本机代理**：`http://127.0.0.1:7897` | 建议删掉这一行（属本机环境，与项目无关） |
| 5 | `doc/MiniEngine_PLAN.md:27-29` | 🟡 家机 VS 装在 **`E:\Program Files\VisualStudio`**、CMake 装在 `C:\Program Files\CMake\bin` | 无害但属本机细节 ⇒ 可脱敏为 `VS 2022（自选路径）` |
| 6 | `doc/S0_CHECKLIST.md:417` | 🟢 `D:/b/mini`（示例路径） | 无需处理 |
| 7 | 家目录 / 用户名 | ✅ **`C:\Users\…` 与用户名 `wuyan` 一处都没有命中** | 无需处理 |

⭐ 顺带确认的好消息：**`build/`、`*.log`、`CMakeUserPresets.json` 都没进仓库**（`.gitignore` 生效）；
已跟踪文件里**没有别的绝对路径泄漏**（`git grep -nIE "[A-Za-z]:[\\/]"` 的其余命中都在上表里）。

## 12. 明确推迟（附触发条件，沿用 `TECH_DEBT` 的 C 组纪律）

| 事项 | 触发条件 | 备注 |
| --- | --- | --- |
| **Release job**（v2） | v1 首跑绿之后**立刻**做 | `TECH_DEBT` A3 曾记"Release 构建从未做过" ⇒ 这个 job 能永久挡住它 |
| **ASan job** | 真出现内存/越界类 bug 时；或 S11 的 Profiler 进场后 | 本地方案已验过（A5/A7）；多一个 job = 多一份分钟（公开后免费，条件已放宽） |
| **required status checks**（分支保护） | 有人协作 / 开始用 PR 时 | 单人直推 `main` 会被自己卡住 ⇒ **现在不开** |
| **依赖 / 构建缓存** | **永远不需要**（零第三方依赖） | 已写进 `ci.yml` 注释，防后人"顺手加" |
| **`windows-2022` 之外的 runner** | 镜像撤掉 VS 2022 时（坑表 8） | 那时要动生成器名 + 三台机器一起升 |
| **`checkout` 升 v7** | 需要 v7 的特性，或 v6 停止维护时 | 升级是一行 + 注释里的版本号 |

## 13. 需要同步的文档

| # | 文档 | 改什么 |
| --- | --- | --- |
| 1 | `README.md` | 顶部加 badge + 一句"CI 只跑 Windows + MSVC、命令与本地一致、纯文档改动不触发" |
| 2 | `doc/SHOWCASE_PLAN.md` | 动作 2 状态 → 已落盘（待首跑）；把坑表 1 那条记进去；§5 状态表更新 |
| 3 | `doc/TECH_DEBT.md` | **关 C5**（没有 CI —— 提前于 S14 关闭）+ 新增 **C36**（runner 标签 ↔ VS 版本绑定） |
| 4 | `AGENTS.md` §2 | 补本文件一行 |
| 5 | `doc/S10_CHECKLIST.md` / `S*_CHECKLIST.md` | **不改**（历史留档） |
