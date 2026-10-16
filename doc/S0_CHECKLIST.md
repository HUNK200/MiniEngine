# S0 实现清单与复习阶梯

> **用法**：在"进度板"里勾进度。复习阶梯 `R1`~`R8` 是**可选路径**（想彻底弄懂 CMake/CTest 时再走）。
> 本文件只描述**要什么、怎么验证**，不含实现代码（`AGENTS.md` 约定：AI 默认只出方案）。
>
> 相关文件：`doc/MiniEngine_PLAN.md`（阶段定义、附录 A 命令速查）、`study/CTEST_NOTES.md`（CTest 入门，与 GoogleTest 的分工）、`doc/MINITEST_SPEC.md`（要手写的测试框架规格）、`AGENTS.md`（项目约定）。
>
> **环境提醒**：默认 shell 是 **PowerShell**。退出码用 `$LASTEXITCODE`（不是 `echo $?`），路径用反斜杠。

---

## 当前状态（2026-09-24：**S0 已完成并通过验收**）

**已完成**：目录结构、三份 CMakeLists、`mini_options` INTERFACE 库、`MiniEngine::engine` 别名、`CMakePresets.json`、`Version`、MiniTest 框架。

| 验收项 | 结果 |
| --- | --- |
| `cmake -S . -B build` | 通过（家机 CMake 4.4.3 / 公司机 4.2.1 各跑一次） |
| `cmake --build build --config Debug` | 通过，**0 warning** |
| `ctest --test-dir build -C Debug` | **1/1 passed** |
| `mini_tests.exe` | `3 passed, 0 failed`（suite：`Core` / `Build` / `TestHarness`） |
| `MiniEngine.exe` | 打印 `MiniEngine 0.1.0-dev` |
| 反向验证 | 断言失败 → ctest 报 0%；0 用例 → 退出码 1；两处现场均已还原 |

**历史**：曾经的阻塞点是 `tests/` 从未进入构建图（第 3、10 节记录了那次诊断），现已全部补齐。

**下一步**：进入 **S1 FrameBuffer**（阶段定义见 `doc/MiniEngine_PLAN.md` B 节）。

---

## 进度板

- [x] 建目录结构，写出三份 CMakeLists 雏形
- [x] `main.cpp` 能编译（hello world）
- [x] `cmake -S . -B build` 能配置通过、`engine` 能构建
- [x] **R1** 观察：看清 ctest 为什么一个测试都找不到
      → 结论：`if(MINI_BUILD_TESTS)` 恒为假 → `tests/` 从未进配置。证据三条：`ctest -N` 报 `Total Tests: 0`；`build/` 下没有 `tests/` 目录；没有 `CTestTestfile.cmake`
- [x] **C1** 顶层：用 `option()` 声明 `MINI_BUILD_TESTS`（默认 ON）
- [x] **C2** 顶层：`enable_testing()`（必须在 `add_subdirectory` 之前）
- [x] **C3** `engine/CMakeLists.txt`：`add_library(MiniEngine::engine ALIAS engine)`
- [x] **C4** 顶层：`mini_options` INTERFACE 库（`/utf-8`、`NOMINMAX`、警告等级）
- [x] **C5** 统一输出目录 → exe 落在 `build\bin\Debug\`（**只设** `CMAKE_RUNTIME_OUTPUT_DIRECTORY` 即可，原因见坑 #5）
- [x] **R5** 写 MiniTest，拿到第一次 `1/1 passed`（规格见 `doc/MINITEST_SPEC.md`；2026-09-24 已按规格落成 `suite.name` 分组 + `test_smoke.cpp`）
- [x] **R6** 加 `add_executable(MiniEngine main.cpp)`，让 exe 首次跑出来
- [x] **R7** 填 Version，验证静态库真的被链上
- [x] **R8** 反向验证 + `CMakePresets.json` + 提交（5 个 commit，2026-09-24 已推送 `87e0d73..00ae8ed`）

> 剩下的**只有可选项**：`R2`~`R4`（"故意撞 CMake 报错"的复习路径）已被 `C1`~`C4` 的实战取代，想弄懂"配置期 vs 链接期报错时机"随时回头看。
> `ms-clang-debug` / `ms-asan` 两个预设当时没实测过 —— **2026-09-29 收口**：
> `ms-clang-debug` 已删除（决定只用 MSVC），`ms-asan` 改成 MSVC 版并补了缺失的链接选项。
> 详见 `doc/TECH_DEBT.md` A4 / A5。

---

## 1. S0 目标与不做什么

**目标（一句话）**：三条命令全绿，且 exe 能跑。

```bash
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

**不做什么**（出现它们就是范围蔓延）：

- ❌ FrameBuffer / Console / Input。注意 `doc/MiniEngine_PLAN.md` 的 C.2 那份 `engine/CMakeLists.txt` 草案列了 `FrameBuffer.cpp`、`ConsoleWindows.cpp`，**那是阶段 1 结束时的样子，不是 S0**
- ❌ `game/` 目录（S5 才建，理由见坑 #4）
- ❌ Sanitizer 预设（当时打算跟 S4 的 clang 预设一起做；**2026-09-29 起 `ms-asan` 已改成 MSVC 版**，与 clang 无关了）
- ❌ Logger、Config、任何引擎功能

**时间盒**：首次做完约 30~40 分钟（不含复习阅读）。分成 R1~R8 后，每步 5~20 分钟。

---

## 2. 交付物清单

| 文件 | 现状 | 责任 | 不要放什么 |
| --- | --- | --- | --- |
| `CMakeLists.txt` | ✅ 完成 | 定标准、定义编译选项接口库、统一输出目录、开 ctest、建可执行目标 | 具体 target 的源文件 |
| `CMakePresets.json` | ✅ 完成 | 让 `doc/MiniEngine_PLAN.md` 附录 A 的 `cmake --preset` 真能跑 | 绝对路径 |
| `main.cpp` | ✅ 完成 | 打印版本号，证明库可用 | 任何"预留的主循环" |
| `engine/CMakeLists.txt` | ✅ 完成 | 造 `engine` 静态库 + `MiniEngine::engine` 别名 | 编译选项的重复粘贴 |
| `engine/include/mini/core/Version.h` | ✅ 完成 | 唯一 public 头，是"库能对外暴露"的最小证明 | 任何实现 |
| `engine/src/core/Version.cpp` | ✅ 完成 | 版本信息的定义 | 任何真实功能 |
| `tests/CMakeLists.txt` | ✅ 完成 | 造测试可执行文件 + 向 CTest 注册（`$<TARGET_FILE:>`） | 测试逻辑本身 |
| `tests/MiniTest.h` | ✅ 完成（110 → 129 行） | 断言宏 + 自注册机制（header-only，只有声明与宏） | 注册表实例、任何引擎头 |
| `tests/test_main.cpp` | ✅ 完成 | 注册表定义 + 参数解析 + 退出码 | 具体用例 |
| `tests/test_smoke.cpp` | ✅ 完成（3 个用例） | 用例本体，按 suite 分组 | runner 逻辑 |

> **保留一条历史认知**：`Version.cpp` 一度是空的，但它**照样能被编译成 `engine.lib`**——CMake 只要求源文件存在，不要求有内容。所以"能构建"从不等于"有东西"。现在 `Version` 有实现，正是为了把它变成"静态库真被链接"的证据（见 R7 的验收方式）。

---

## 3. 缺口诊断（2026-09-23 的快照，**已全部修完**）

> 本节是**历史记录**：当时 `tests/` 还没进构建图，下面每条都是照着构建产物实测出来的。
> G1~G8 现已全部补齐（`ctest -N` 现在报 `Total Tests: 1`，`build/CTestTestfile.cmake` 与 `build/tests/CTestTestfile.cmake` 都在）。
> 保留它的价值是记住那一类"**静默失败**"——尤其 G1 的 `if(未定义变量)`。

下表每一条都有构建产物作为证据，不是推测。

### 3.1 根因链：为什么 `ctest` 一个测试都找不到

```
if(MINI_BUILD_TESTS)        ← 变量从未声明，恒为假
   ↓
tests/ 从未 add_subdirectory            ← 证据：build/ 下没有 tests/ 目录
   ↓
没有 mini_tests 目标                    ← 证据：MiniEngine.sln 里只有 ALL_BUILD / ZERO_CHECK / engine
   ↓
没有 CTestTestfile.cmake                ← 证据：build/CTestTestfile.cmake 不存在
   ↓
ctest 空跑                              ← 证据：LastTest.log 只有 Start/End 两行，中间 0 行
```

**`if()` 里用未定义变量不会报错**，只会当假。这是它最坑的地方。

### 3.2 缺口清单

| # | 位置 | 缺什么 | 后果 | 报错时机 |
| --- | --- | --- | --- | --- |
| G1 | 根 CMakeLists | `MINI_BUILD_TESTS` 从未用 `option()` 声明 | `tests/` 从不参与配置 → ctest 空跑 | **无报错（静默）** |
| G2 | 根 CMakeLists | 没有 `enable_testing()` | 即使 tests 进来也不生成 CTest 文件 | **无报错（静默）** |
| G3 | 根 CMakeLists | `mini_options` 从未定义 | `target_link_libraries` 把它当**普通库名** → 去找 `mini_options.lib` | 链接期 |
| G4 | `engine/CMakeLists.txt` | 没有 `MiniEngine::engine` 别名 | tests 里用的是带 `::` 的名字 | **配置期硬报错** |
| G5 | 根 CMakeLists | 没有 `add_executable(MiniEngine main.cpp)` | 主程序目标不存在 | **无报错（静默）** |
| G6 | 根 CMakeLists | 没有 C++ 标准设置 | 现在看不出，S4 写 `constexpr`/`string_view` 才炸 | 编译期 |
| G7 | 根 CMakeLists | 没有统一输出目录四件套 | exe 落在 `build/Debug/` 或 `build/x64/Debug/`，不是 `build/bin/Debug/` | **无报错（静默）** |
| G8 | 根目录 | 没有 `CMakePresets.json` | 文档里的 `cmake --preset` 是假的 | 运行命令时 |

### 3.3 G3 与 G4 的对照——这是本次最值钱的一课

同样是"目标不存在"，CMake 的两种反应**完全不同**：

| 写法 | 目标不存在时 | 报错时机 |
| --- | --- | --- |
| `mini_options`（无 `::`） | 当成普通库名，**静默接受** | 拖到链接期才失败 |
| `MiniEngine::engine`（有 `::`） | **硬报错** | 配置阶段（不必等到构建） |

**规律**：想早期发现拼写错误，就**总是用带 `::` 的别名**。这也是现代 CMake 推荐 ALIAS 的原因之一。R2/R3 会亲眼看到这个对比。

---

## 4. 复习阶梯 R1~R8

每一步都是"**改一点 → 立刻跑 → 看到明确现象**"。R2、R3 是**故意撞错误**，不是走弯路。

### R1 观察：先看清坏在哪（10 分钟，不改代码）

**做什么**

```powershell
ctest --test-dir build -N              # -N = 只列出已注册的测试，不执行
ctest --test-dir build -C Debug -N
Get-ChildItem build                    # 有没有 tests/ 子目录？
Test-Path build\bin                    # 有没有统一输出目录？（期望 False）
```

**预期看到**（已实测）：前两条都报 `Total Tests: 0`（CMake 4.2 的措辞；旧版本会写 `No tests were found!!!`）；`build/` 里没有 `tests/` 子目录；`Test-Path build\bin` 返回 `False`。

**原理**：CMake 分两段——**配置期**（读 CMakeLists，产出 `.sln` / `.vcxproj` / `CTestTestfile.cmake`）和**构建期**（编译、链接）。`ctest` **不读 CMakeLists**，它只读配置期写出的 `CTestTestfile.cmake`。所以"测试没被注册"永远要去**配置期**找原因，翻构建日志是白费功夫。

**通过标志**：你能自己说出这句话——"因为 `tests/` 根本没进配置，所以没有任何 CTest 文件，所以 ctest 空跑"。

### R2 加 `option()` 与 `enable_testing()`，故意撞出第一个配置期报错（15 分钟）

**做什么**（只改根 `CMakeLists.txt`）

1. 在 `project()` 之后，用 **`option()`** 声明 `MINI_BUILD_TESTS`（默认 ON）。顺带声明 `MINI_SANITIZE`（默认 OFF，S0 不用它）
2. 在 `add_subdirectory()` **之前**调用 **`enable_testing()`**
3. `add_subdirectory(engine)` 之后追加 `add_subdirectory(tests)`（现在 `if` 才真正生效）

**然后立即跑**

```bash
cmake -S . -B build
```

**预期看到（这是好事）**：**配置期硬报错**，大意是 `mini_tests` 链接的目标 `MiniEngine::engine` 不存在。因为带 `::` 的名字不能降级成普通库名。

**原理**：`enable_testing()` 必须在**顶层**调用。它的作用是"从这一层开始生成 CTestTestfile.cmake 并向下传播"；写在 `tests/` 里只会生成在 `tests/` 下，`ctest` 从顶层找就找不到。

**通过标志**：你看到并读懂了那条 `MiniEngine::engine ... not found` 的报错，并且知道它属于**配置期**。

**不要做**：现在**先不要**改 `engine/CMakeLists.txt`，让这个错误完整出现一次。

### R3 加别名，撞出"静默降级"的链接期报错（15 分钟）

**做什么**：在 `engine/CMakeLists.txt` 里用 **`add_library(... ALIAS ...)`** 定义 `MiniEngine::engine` 指向 `engine`。

**然后跑**

```bash
cmake -S . -B build            # 期望：这次配置通过
cmake --build build --config Debug
```

**预期看到**：配置通过，但在**构建期链接**时报错，找不到 `mini_options.lib`（或 MSVC 报 `LNK1181: cannot open input file 'mini_options.lib'`）。

**原理**：这就是 3.3 表格里的第二种反应。`mini_options` 不含 `::`，CMake 按"外部库名"处理，直到链接器才抱怨。**对比 R2 的报错时机**——一个是配置期，一个是构建期，这是本步唯一的收获。

**通过标志**：你能说出"为什么同一个'目标不存在'，一个在配置期就报，一个拖到链接期"。

### R4 定义 `mini_options`，让链接链通（20 分钟）

**做什么**（根 `CMakeLists.txt`）

用 **`add_library(... INTERFACE)`** 建一个**纯接口库**，承载全部编译选项，然后让各 target 链接它：

- 编译期定义：`NOMINMAX`、`WIN32_LEAN_AND_MEAN`（用 `target_compile_definitions`，指定 **INTERFACE** 作用域）
- MSVC 分支：`/W4 /permissive- /utf-8 /Zc:__cplusplus`
- 其他编译器分支：`-Wall -Wextra -Wpedantic -Wshadow`
- 用 `if(MSVC)` 区分，别假设只有一种编译器

**INTERFACE 库是什么**：它不产出 `.lib`，只是一个"装着编译选项的包裹"。谁 `target_link_libraries` 它，谁就继承这些选项。这样选项只写一处。

**放过什么**：`lib`/`exe`/`INTERFACE` 三种库类型先只需知道 INTERFACE 这一种。

**然后跑**

```bash
cmake -S . -B build && cmake --build build --config Debug
```

**预期看到**：配置通过、构建**通过**。但 `mini_tests` 会在链接时报 `unresolved external symbol main`——因为 `tests/*.cpp` 还是空文件，根本没有 `main`。

> 这个报错正是 R5 的入口。**"构建通过"和"链接通过"是两件事**，`engine`（静态库）只有"归档"这一步，所以它早就"通过"了。

**通过标志**：你能解释为什么 `engine` 早就构建成功，而 `mini_tests` 不行。

### R5 写 MiniTest，拿到第一次 `1/1 passed`（30 分钟，本阶段的重头）

**做什么**：填三个空文件。接口设计见第 5 节，实现自己写。

**关键约束（三条，写之前先想清楚）**

1. `main` 必须**返回失败用例数**（0 = 全过）。CTest 只看进程退出码
2. 注册表**只能在一个 `.cpp` 里定义**，头文件里只放 `extern` 声明 + 宏。否则每个 TU 各得一份，"测试数 0"能坑你半小时
3. 自注册靠**静态对象构造函数**。想清楚：为什么注册表本身要用"函数内静态局部变量"而不是全局变量（提示：静态初始化顺序）

**然后跑**

```bash
cmake --build build --config Debug
ctest --test-dir build -C Debug -N          # 期望：列出 smoke（不是 No tests found）
ctest --test-dir build -C Debug --output-on-failure
```

**预期看到**：`100% tests passed, 0 tests failed out of 1`。

**顺便复习 ctest 的五个常用开关**

| 开关 | 作用 |
| --- | --- |
| `-N` | 只列出已注册的测试，不执行 ← 排查"找不到测试"第一招 |
| `-C Debug` | 多配置生成器（VS）**必带**，因为可执行文件路径含配置名 |
| `-R <regex>` | 只跑名字匹配的测试 |
| `-V` / `-VV` | 显示测试的命令行 / 完整输出 |
| `--output-on-failure` | 只在失败时打印被测程序的 stdout |

**通过标志**：`ctest` 报 1/1；且直接跑 `.\build\bin\Debug\mini_tests.exe` 后 `$LASTEXITCODE` 为 0。

### R6 加可执行目标与输出目录，让 exe 首次跑出来（20 分钟）

**做什么**（根 `CMakeLists.txt`）

1. 用 **`add_executable()`** 建主程序目标，源文件 `main.cpp`
2. 让它 **`target_link_libraries`** 到引擎库的别名（用别名，不要用 `engine`）
3. 统一输出目录：设 `CMAKE_RUNTIME_OUTPUT_DIRECTORY`，**并且**逐个设 `_DEBUG` / `_RELEASE` / `_RELWITHDEBINFO` / `_MINSIZEREL` 四个后缀变量（VS 是多配置生成器，只设前者无效；配置名必须**全大写**）
4. 补 C++ 标准设置：`CMAKE_CXX_STANDARD 20` + `CMAKE_CXX_STANDARD_REQUIRED ON` + `CMAKE_CXX_EXTENSIONS OFF`

**然后跑**

```bash
cmake -S . -B build && cmake --build build --config Debug
ls build/bin/Debug/                 # 期望：MiniEngine.exe  mini_tests.exe
./build/bin/Debug/MiniEngine.exe
```

**预期看到**：目录里同时有 exe 和测试程序；exe 还是打印 `hello, world!`（正常，R7 才改它）。

**通过标志**：`build/bin/Debug/` 里两个 exe 都在；**不是**散落在 `build/Debug/`。

### R7 填 Version，验证静态库真的被链上（20 分钟）

**做什么**

1. `Version.h`：声明取版本号/版本串的函数（声明即可，想想 `constexpr` 能不能用在这里）
2. `Version.cpp`：定义它们
3. `main.cpp`：改成 `#include <mini/core/Version.h>`（**尖括号 + `mini/` 前缀**，绝不用相对路径穿透到 `engine/src/`），打印版本号

**然后跑**

```bash
cmake --build build --config Debug
./build/bin/Debug/MiniEngine.exe        # 期望：MiniEngine v0.1.0 之类
```

**原理**：这一步同时验证三件事——`target_include_directories(... PUBLIC include)` 配对了、静态库**真的被链接**了、`main.cpp` 没有用穿透式 include。如果 include 找不到，说明 PUBLIC 写成了 PRIVATE 或路径少了 `include` 这一层。

**通过标志**：exe 打印出版本号，而不是 `hello, world!`。

### R8 反向验证 + 预设 + 提交（25 分钟）

**反向验证三条**（比正向更重要，见第 6 节）→ 然后按 `doc/MiniEngine_PLAN.md` 附录 B 的草案写 `CMakePresets.json` → 按第 7 节切 commit。

---

## 5. 各文件要求速查

### 5.1 顶层 `CMakeLists.txt` 应包含

- `cmake_minimum_required(VERSION 3.20)`（4.x 已不支持 <3.5）
- `project(MiniEngine LANGUAGES CXX)`
- C++ 标准三件套（见 R6）
- C++ 标准件与 `option()` 声明
- `mini_options` INTERFACE 库（见 R4）
- **`enable_testing()` 在 `add_subdirectory()` 之前**（见 R2）
- 统一输出目录：**只设** `CMAKE_RUNTIME_OUTPUT_DIRECTORY`（多配置生成器会自动再补一层 `Debug/`，见 R6 与坑 #5）
- `add_subdirectory(engine)` → `add_subdirectory(tests)` → `add_executable(MiniEngine main.cpp)`
- 禁用：`include_directories()` / `link_libraries()` / `file(GLOB)` 收集源文件

### 5.2 `engine/CMakeLists.txt` 应包含

- `add_library(engine STATIC src/core/Version.cpp)`（S0 只要一个源文件）
- `target_include_directories(engine PUBLIC include)`（**必须 PUBLIC**）
- `target_link_libraries(engine PRIVATE mini_options)`
  - 何时改成 PUBLIC：等某个 **public 头里出现 `<windows.h>`**（S2 的 platform 头）时。现在没有，所以 PRIVATE，在注释里写清判据
- `add_library(MiniEngine::engine ALIAS engine)`（见 R3）
- 可以删掉重复的 `cmake_minimum_required`（顶层已定）

### 5.3 `tests/CMakeLists.txt` 应包含

- `add_executable(mini_tests test_main.cpp test_smoke.cpp)`
- `target_link_libraries(mini_tests PRIVATE MiniEngine::engine mini_options)`（显式链 `mini_options`：`engine` 是 PRIVATE 传递，不会漏给测试）
- `add_test(NAME ... COMMAND $<TARGET_FILE:mini_tests>)`（**generator expression**，路径随配置变化也不会错）
- 建议按 suite 注册多条 CTest 条目，让 CTest 层能单跑某一类
- 不要在这里调 `enable_testing()`

### 5.4 `tests/MiniTest.h` 接口设计

**对外只有这些宏**：

```
MINI_TEST(suite_name, case_name)     // 定义用例并自动注册
MINI_CHECK(expr)                     // expr 为假则失败
MINI_CHECK_EQ(a, b)                  // 失败时打印两侧的值
MINI_CHECK_NEAR(a, b, eps)           // 浮点比较，禁写 ==
MINI_FAIL(msg)                       // 无条件失败
```

**运行期行为**：无参跑全部；`argv[1]` 为 suite 名则只跑该 suite；`--list` 列出用例名后返回 0；输出 `N passed, M failed`；失败行固定三行内：`[FAIL] suite.case` / `file:line` / 表达式原文。

**接口落地情况（2026-09-24）**：已按上面这组签名实现在 `tests/MiniTest.h` / `tests/test_main.cpp` / `tests/test_smoke.cpp`。三处与规格的细化差异，规格文档（`MINITEST_SPEC.md`）里已同步：

- 头里多一个 header-only 的 `inline` 模板 `ToString`（无状态，只负责把断言两侧的值变成字符串）
- runner 对"suite 名不存在"报错并返回非 0（防手滑打错 suite 名拿到假绿）
- 断言宏先把条件存进 `const bool` 再判断：否则 `MINI_CHECK_NEAR(0.1 + 0.2, 0.3, 1e-9)` 这种整条都是常量的写法会撞 MSVC 的 **C4127**，把用例代码搞脏（`/W4` 下是硬约束）

**必须想清楚的三件事**：见 R5 的三条关键约束。

**为什么不用更简单的方案**：手写 `{name, fn}` 数组也完全可以，还少 30 行。选自注册是为了顺手覆盖"静态初始化 + 跨 TU"这个真实工程问题。**不想碰就手写数组，别为此纠结。**

**留个后患意识**（S0 不用解决）：`test_main.cpp` 和 `test_smoke.cpp` 现在都在可执行目标里，链接器不会丢东西。**哪天你把测试辅助移进静态库，未被引用的注册对象会被链接器直接丢弃，测试静默变成 0 个**，那时需要 `WHOLE_ARCHIVE`。

### 5.5 `main.cpp`

- 只允许 include `mini/...` 下的头。出现 `#include "../engine/src/..."` 就说明 include 设计错了
- 不写任何"预留的主循环"

---

## 6. 验收（正向 + 反向）

**正向**

```powershell
cmake -S . -B build
cmake --build build --config Debug
Test-Path build\bin\Debug\MiniEngine.exe              # 期望 True
.\build\bin\Debug\MiniEngine.exe                      # 打印 MiniEngine 0.1.0-dev
.\build\bin\Debug\mini_tests.exe                      # 3 passed, 0 failed
.\build\bin\Debug\mini_tests.exe --list               # 列出 suite.case；$LASTEXITCODE = 0
.\build\bin\Debug\mini_tests.exe Core                 # 只跑 Core 组：1 passed, 0 failed
.\build\bin\Debug\mini_tests.exe Nope                 # suite 名不存在 → 报错，退出码 1（不是假绿）
$LASTEXITCODE                                         # 期望 0
ctest --test-dir build -C Debug --output-on-failure   # 100% tests passed, out of 1
```

无 C4819 / C4127 等任何警告、无 CMake Warning。

**反向验证（别跳）**

1. **证明测试不是永远绿**：把 `test_smoke.cpp` 的断言改成故意失败 → 重新构建 → 必须打印 `[FAIL] suite.case` + `file:line` + 两侧的值，退出码非 0，`ctest` 报 `0% tests passed`
2. **证明"0 个用例"不会假绿**：把 `test_smoke.cpp` 从 `add_executable` 里临时去掉 → 重新构建 → 必须打印 `no case registered` 且退出码非 0（这条是防 CTest 报 100% passed 的门闩）
3. **证明警告真的开着**：故意加一个未使用变量 → 重新构建 → 应出现 C4101
4. **证明库真的被链上**：改一下 `Version.cpp` 里的版本串 → 重新构建 → exe 输出必须跟着变（没变说明你跑的不是刚构建的产物）

四条都符合预期，S0 才算真的完成。**第 1、2 条在 2026-09-24 已实跑过并还原现场**。

---

## 7. 十一个坑

| # | 现象 | 原因与对策 |
| --- | --- | --- |
| 1 | 配置直接报错，提到 compatibility | `cmake_minimum_required` 写了 3.5 以下 → 最低 3.20 |
| 2 | 构建报"没有可用的配置" | 多配置生成器必须 `--config Debug` |
| 3 | `ctest` 说 No tests were found | 三个原因按序排查：`tests/` 没进配置 → 没 `enable_testing()` → 漏 `-C Debug` |
| 4 | `add_library` 报 No SOURCES given | **CMake 不接受没有源文件的库**。所以 S0 不建空的 `game/`；要建就用 `INTERFACE` 类型，等有源文件再改 STATIC |
| 5 | exe 跑到 `build/Debug/`、`build/x64/Debug/` 或**少了 `Debug` 那一层** | 两种相反的错误：① **只设** `CMAKE_RUNTIME_OUTPUT_DIRECTORY` 时才由生成器自动补 `Debug/`（本项目采用，实测 exe 落在 `build\bin\Debug\`）；② 如果再去逐个设 `_DEBUG`/`_RELEASE` 四个后缀变量，这些变量是**按原样使用**，结果反而掉到 `build/bin/`（家机踩过）。别两边都设 |
| 6 | 一堆 C4819 / 中文注释乱码 | 漏 `/utf-8`；源文件必须 UTF-8 **无 BOM** |
| 7 | 找不到 `<mini/core/Version.h>` | `target_include_directories` 漏了/PUBLIC 写成 PRIVATE/路径少 `include` 一层 |
| 8 | 改了头文件但增量构建没生效 | 检查是否误用 `file(GLOB)` 收集源文件（**S0 明确禁止 GLOB**，手写源文件列表） |
| 9 | 编译期找不到 `std::max`/`min` | 以后引入 `<windows.h>` 时没定义 `NOMINMAX`（R4 已提前加上） |
| 10 | CMake 报路径过长 | 已设 `core.longpaths true`；仍失败就 `cmake -S . -B D:/b/mini` 把 build 移出仓库 |
| 11 | 断言里写常量，MSVC 报 **C4127**（条件表达式是常量） | `if (常量)` 触发。断言宏里先把条件存进 `const bool` 再判断即可绕开（2026-09-24 实测，见 5.4） |

**另外一条不在坑表但要记住**：`if(未定义变量)` **不报错、只当假**。G1 就是死在这里，而且完全静默。以后凡是"某个子目录的 target 神秘消失"，第一个怀疑对象就是它。

---

## 8. commit 切分（每个都能构建）

| # | 信息 | 对应 |
| --- | --- | --- |
| 1 | `build: 补齐顶层 CMake 选项、测试开关与输出目录` | R2 + R4 + R6 |
| 2 | `build: 为 engine 增加 MiniEngine::engine 别名` | R3 |
| 3 | `feat: 加入 engine 版本信息与可执行入口` | R7 |
| 4 | `test: 接入 MiniTest 极简断言框架与 ctest` | R5 |
| 5 | `build: 加入 CMakePresets.json 预设` | R8 |

顺序上 3 和 4 可对调。**"每个 commit 都能构建"比切得漂亮重要**——构建不过就合并到下一个。

> **实际落地情况（2026-09-24 记录）**：家机把这批改动合成成了**一个** commit `7115e29 feat: add S0 build skeleton with engine static lib and ctest`（含 CMake、`Version`、`MiniTest.h`、`CMakePresets.json` 全部）；2026-09-24 又按 `MINITEST_SPEC.md` 把测试框架重构了一次（`test: MiniTest 按 suite 分组…`，含新增 `test_smoke.cpp`）。这张表留着当切分参考，不必回头去凑。

---

## 9. 文档同步

- [x] PLAN 附录 A 的 `cmake --preset ms-msvc-debug` 依赖 `CMakePresets.json` 存在 → 预设已在仓库里，名字核对一致
- [x] PLAN 的 S0 小节写的是"`ctest` 1/1 passed" → 与第 6 节一致（`mini_tests` 仍是 1 个 CTest 条目，内部 3 个用例）
- [x] 实际 target 名/输出路径与 PLAN C.2 不同（C.2 是阶段 1 的样子）→ 已在 PLAN 的 S0 小节补"实际结构"一段（2026-09-24），避免 C.2 误导

---

## 10. CMake 4 处补丁清单（阻塞点解法）

> **状态（2026-09-24）：C1~C5 已全部落地**，本节保留为"怎么改、改完该看到什么"的复习材料，不是待办。

按依赖顺序改，**中间会看到两个报错，都是预期的**——它们正好是"改对了没有"的判据。

### C1 顶层：声明测试开关

| 项 | 要求 |
| --- | --- |
| 位置 | `project(...)` 之后、`add_subdirectory(...)` 之前 |
| API | `option()` |
| 语义 | 名字 `MINI_BUILD_TESTS`，一句描述，默认 `ON` |

用 `option()` 而不是 `set()`：前者进 CMake 缓存、有真正的 ON/OFF 布尔语义、可被 `-DMINI_BUILD_TESTS=OFF` 覆盖；后者每次 configure 都重置，命令行覆盖会失效。

**验证**：`build\CMakeCache.txt` 里出现 `MINI_BUILD_TESTS:BOOL=ON`。

### C2 顶层：开 CTest

| 项 | 要求 |
| --- | --- |
| 位置 | **必须在 `add_subdirectory(...)` 之前**，且必须在**顶层** |
| API | `enable_testing()` |

它的作用是"从这一层开始生成 `CTestTestfile.cmake` 并向下传播"。写在 `tests/` 里，`ctest` 从顶层找就找不到。

**验证**：`build\CTestTestfile.cmake` 与 `build\tests\CTestTestfile.cmake` 同时出现。
（可选替代：`include(CTest)` 会额外给一个标准 `BUILD_TESTING` 选项；本项目已用 `MINI_BUILD_TESTS`，不必换。）

**改完 C1 + C2 立刻 configure，会出现第一个报错（预期）**：

```text
CMake Error at tests/CMakeLists.txt:5 (target_link_libraries):
  Target "mini_tests" links to:
    MiniEngine::engine
  but the target was not found.
```

它在 `-- Configuring done` 之后、`-- Generating done` **之前**。反过来说：**看到 `-- Generating done` 就等于整个目标图是自洽的**。

### C3 `engine/CMakeLists.txt`：加别名

| 项 | 要求 |
| --- | --- |
| 位置 | `add_library(engine STATIC ...)` 之后 |
| API | `add_library(... ALIAS ...)`，别名名必须是 **`MiniEngine::engine`** |

带 `::` 的名字必须对应真实存在的目标，否则 generate 阶段硬报错——拼写错误当场暴露，这是好事。

**改完 configure 应通过，但构建会撞第二个报错（预期）**：

```text
LNK1181: cannot open input file 'mini_options.lib'
```

### C4 顶层：定义 `mini_options`

| 项 | 要求 |
| --- | --- |
| 位置 | **必须在 `add_subdirectory(engine)` 之前** |
| API | `add_library(mini_options INTERFACE)` |

INTERFACE 库不产出二进制，只是"装编译选项的包裹"。内容全部用 **INTERFACE** 作用域：

- `target_compile_definitions(... INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN)`
- MSVC 分支：`/W4 /permissive- /utf-8 /Zc:__cplusplus`（`/utf-8` 在 936 代码页下是必需品，不是优化）
- 其他编译器分支：`-Wall -Wextra -Wpedantic -Wshadow`
- 用 `if(MSVC)` 区分，别假设只有一种编译器

**为什么必须放前面**：`engine` 与 `tests` 都已经写了 `target_link_libraries(... mini_options)`。这个名字不含 `::`，CMake 会当普通库名，必须在使用它的 target 之前定义。

**改完 C4 后，构建的最后一步会失败，同样是预期内**：`tests\*.cpp` 还是空文件 → `unresolved external symbol main`。

> **这个报错出现，就说明 C1~C4 全部改对了**——剩下只是把三份文件写出来（规格见 `doc/MINITEST_SPEC.md`）。

### C5 统一输出目录

不做也能跑，只是 exe 路径会变（落在 `build\tests\Debug\` 或 `build\Debug\` 之类的地方）。做了才是文档里写的 `build\bin\Debug\`。

- **只设** `CMAKE_RUNTIME_OUTPUT_DIRECTORY` = `${CMAKE_BINARY_DIR}/bin`
- **不要**再设 `_DEBUG` / `_RELEASE` / `_RELWITHDEBINFO` / `_MINSIZEREL` 四个后缀变量

**为什么"多设反而错"**（2026-09-23 家机踩过，2026-09-24 公司机复验）：多配置生成器下，若**只设** `CMAKE_RUNTIME_OUTPUT_DIRECTORY`，生成器会**自动再补一层配置名** → `build/bin/Debug/`；而 `_<CONFIG>` 后缀变量是**按原样使用**的，一旦设了，`Debug` 那一层就没了 → 变成 `build/bin/`，正好和 PLAN 附录 A 的运行命令 `.\build\bin\Debug\MiniEngine.exe` 对不上。

没做 C5 时找 exe：`Get-ChildItem -Recurse -Filter mini_tests.exe build`

### 报错对照表

| 现象 | 原因 |
| --- | --- |
| `Target ... MiniEngine::engine but the target was not found` | C3 没做，或别名名字拼错 |
| `cannot open input file 'mini_options.lib'` | C4 没做，或放到了 `add_subdirectory` 之后 |
| configure 通过但 `ctest -N` 仍报 `Total Tests: 0` | C2 的 `enable_testing()` 写在子目录里了 |
| `CMakeCache.txt` 里没有 `MINI_BUILD_TESTS` | C1 没做，或用了 `set()`（不进缓存） |
| `unresolved external symbol main` | 正常，等你写 `test_main.cpp` |
