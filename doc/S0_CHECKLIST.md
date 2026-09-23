# S0 实现清单与复习阶梯

> **用法**：在"进度板"里勾进度；从 `R1` 往下走复习阶梯，每步都有"预期现象 + 通过标志"。
> 本文件只描述**要什么、怎么验证**，不含实现代码（`AGENTS.md` 约定：AI 默认只出方案）。
>
> 相关文件：`doc/MiniEngine_PLAN.md`（阶段定义、附录 A 命令速查）、`AGENTS.md`（项目约定）。

---

## 进度板

- [x] 建目录结构，写出三份 CMakeLists 雏形
- [x] `main.cpp` 能编译（hello world）
- [x] `cmake -S . -B build` 能配置通过、`engine` 能构建
- [ ] **R1** 观察：看清 ctest 为什么一个测试都找不到
- [ ] **R2** 加 `option()` + `enable_testing()`，故意撞出第一个配置期报错
- [ ] **R3** 加别名，撞出"静默降级"的链接期报错
- [ ] **R4** 定义 `mini_options`，让链接链通
- [ ] **R5** 写 MiniTest，拿到第一次 `1/1 passed`
- [ ] **R6** 加可执行目标与输出目录，让 `MiniEngine.exe` 首次跑出来
- [ ] **R7** 填 Version，验证静态库真的被链上
- [ ] **R8** 反向验证 + `CMakePresets.json` + 提交

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
- ❌ Sanitizer 预设（跟 S4 的 clang 预设一起做）
- ❌ Logger、Config、任何引擎功能

**时间盒**：首次做完约 30~40 分钟（不含复习阅读）。分成 R1~R8 后，每步 5~20 分钟。

---

## 2. 交付物清单

| 文件 | 现状 | 责任 | 不要放什么 |
| --- | --- | --- | --- |
| `CMakeLists.txt` | ⚠️ 半成品 | 定标准、定义编译选项接口库、统一输出目录、开 ctest、建可执行目标 | 具体 target 的源文件 |
| `CMakePresets.json` | ❌ 缺 | 让 `doc/MiniEngine_PLAN.md` 附录 A 的 `cmake --preset` 真能跑 | 绝对路径 |
| `main.cpp` | ✅ 能编译 | 打印版本号，证明库可用 | 任何"预留的主循环" |
| `engine/CMakeLists.txt` | ⚠️ 缺别名 | 造 `engine` 静态库 + `MiniEngine::engine` 别名 | 编译选项的重复粘贴 |
| `engine/include/mini/core/Version.h` | ❌ **空文件** | 唯一 public 头，是"库能对外暴露"的最小证明 | 任何实现 |
| `engine/src/core/Version.cpp` | ❌ **空文件** | 版本信息的定义 | 任何真实功能 |
| `tests/CMakeLists.txt` | ⚠️ 依赖缺失 | 造测试可执行文件 + 向 CTest 注册 | 测试逻辑本身 |
| `tests/MiniTest.h` | ❌ **空文件** | 断言宏 + 自注册机制（header-only） | 依赖任何引擎头 |
| `tests/test_main.cpp` | ❌ **空文件** | 注册表定义 + 参数解析 + 退出码 | 具体用例 |
| `tests/test_smoke.cpp` | ❌ **空文件** | 第一个用例 | — |

> **重要认知**：`Version.cpp` 现在是空的，但它**照样能编译成 `engine.lib`**——CMake 只要求源文件存在，不要求有内容。所以"能构建"完全不等于"有东西"。这正是 S0 必须写 `Version` 的原因：它是唯一能验证"库真的被使用"的手段。

---

## 3. 当前缺口诊断（证据级）

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

| 写法 | 目标不存在时 | 时机 |
| --- | --- | --- |
| `mini_options`（无 `::`） | 当成普通库名，**静默接受** | 直到链接期才失败 |
| `MiniEngine::engine`（有 `::`） | **配置期立即硬报错** | 立刻 |

**规律**：想早期发现拼写错误，就**总是用带 `::` 的别名**。这也是现代 CMake 推荐 ALIAS 的原因之一。R2/R3 会亲眼看到这个对比。

---

## 4. 复习阶梯 R1~R8

每一步都是"**改一点 → 立刻跑 → 看到明确现象**"。R2、R3 是**故意撞错误**，不是走弯路。

### R1 观察：先看清坏在哪（10 分钟，不改代码）

**做什么**

```bash
ctest --test-dir build -N              # -N = 只列出已注册的测试，不执行
ctest --test-dir build -C Debug -N
ls build/                              # 有没有 tests/ 子目录？
ls build/bin 2>&1                      # 有没有统一输出目录？
```

**预期看到**：前两条都输出 `No tests were found!!!`；`build/` 里没有 `tests/`；`build/bin` 不存在。

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

**通过标志**：`ctest` 报 1/1；且 `./build/bin/Debug/mini_tests`（或它实际所在路径）直接跑也返回 0。

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
- 统一输出目录五件套（见 R6）
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

**必须想清楚的三件事**：见 R5 的三条关键约束。

**为什么不用更简单的方案**：手写 `{name, fn}` 数组也完全可以，还少 30 行。选自注册是为了顺手覆盖"静态初始化 + 跨 TU"这个真实工程问题。**不想碰就手写数组，别为此纠结。**

**留个后患意识**（S0 不用解决）：`test_main.cpp` 和 `test_smoke.cpp` 现在都在可执行目标里，链接器不会丢东西。**哪天你把测试辅助移进静态库，未被引用的注册对象会被链接器直接丢弃，测试静默变成 0 个**，那时需要 `WHOLE_ARCHIVE`。

### 5.5 `main.cpp`

- 只允许 include `mini/...` 下的头。出现 `#include "../engine/src/..."` 就说明 include 设计错了
- 不写任何"预留的主循环"

---

## 6. 验收（正向 + 反向）

**正向**

```bash
cmake -S . -B build
cmake --build build --config Debug
ls build/bin/Debug/                                   # MiniEngine.exe  mini_tests.exe
./build/bin/Debug/MiniEngine.exe                      # 打印版本号
./build/bin/Debug/mini_tests ; echo $?                # 1 passed, 0 failed / 退出码 0
ctest --test-dir build -C Debug --output-on-failure    # 100% tests passed, out of 1
```

无 C4819 警告、无 CMake Warning。

**反向验证（别跳）**

1. **证明测试不是永远绿**：把 `test_smoke.cpp` 的断言改成故意失败 → 重新构建 → ctest 必须报失败、退出码非 0
2. **证明警告真的开着**：故意加一个未使用变量 → 重新构建 → 应出现 C4101
3. **证明库真的被链上**：改一下 `Version.cpp` 里的版本串 → 重新构建 → exe 输出必须跟着变（没变说明你跑的不是刚构建的产物）

三条都符合预期，S0 才算真的完成。

---

## 7. 十个坑

| # | 现象 | 原因与对策 |
| --- | --- | --- |
| 1 | 配置直接报错，提到 compatibility | `cmake_minimum_required` 写了 3.5 以下 → 最低 3.20 |
| 2 | 构建报"没有可用的配置" | 多配置生成器必须 `--config Debug` |
| 3 | `ctest` 说 No tests were found | 三个原因按序排查：`tests/` 没进配置 → 没 `enable_testing()` → 漏 `-C Debug` |
| 4 | `add_library` 报 No SOURCES given | **CMake 不接受没有源文件的库**。所以 S0 不建空的 `game/`；要建就用 `INTERFACE` 类型，等有源文件再改 STATIC |
| 5 | exe 跑到 `build/Debug/` 或 `build/x64/Debug/` | 只设了 `CMAKE_RUNTIME_OUTPUT_DIRECTORY`，漏了 `_DEBUG`/`_RELEASE`（且必须全大写） |
| 6 | 一堆 C4819 / 中文注释乱码 | 漏 `/utf-8`；源文件必须 UTF-8 **无 BOM** |
| 7 | 找不到 `<mini/core/Version.h>` | `target_include_directories` 漏了/PUBLIC 写成 PRIVATE/路径少 `include` 一层 |
| 8 | 改了头文件但增量构建没生效 | 检查是否误用 `file(GLOB)` 收集源文件（**S0 明确禁止 GLOB**，手写源文件列表） |
| 9 | 编译期找不到 `std::max`/`min` | 以后引入 `<windows.h>` 时没定义 `NOMINMAX`（R4 已提前加上） |
| 10 | CMake 报路径过长 | 已设 `core.longpaths true`；仍失败就 `cmake -S . -B D:/b/mini` 把 build 移出仓库 |

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

---

## 9. 文档同步

- [ ] PLAN 附录 A 的 `cmake --preset ms-msvc-debug` 依赖 `CMakePresets.json` 存在 → 做到了才不改文档
- [ ] PLAN 的 S0 小节写的是"`ctest` 1/1 passed" → 与第 6 节一致
- [ ] 若实际 target 名/输出路径与 PLAN C.2 不同（**大概率不同**，C.2 是阶段 1 的样子）→ 在 PLAN 的 S0 小节补一行实际结构，别让 C.2 误导三天后的你
