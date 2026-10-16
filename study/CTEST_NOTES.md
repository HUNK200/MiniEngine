# CTest 笔记（面向用过一点 GoogleTest 的人）

> 场景：本项目 S0 阶段，用自制 `MiniTest` 做断言、用 `ctest` 跑测试。
> 环境：**Windows + PowerShell + VS 2022 多配置生成器**（本文件命令按 PowerShell 写）。

---

## 1. 一句话定位

**CTest 不是测试框架，是 CMake 自带的测试调度器（test runner）。它和 GoogleTest 不是竞品，是两层。**

```text
CMake（构建系统）
  │  配置期：add_test() 把「要跑的命令」写进 CTestTestfile.cmake
  ▼
ctest（调度器 / 报告器）          ← 本文件讲这一层
  │  逐条起进程 → 看退出码 → 汇总报告
  ▼
测试可执行文件（内含 GoogleTest，或本项目自制的 MiniTest 断言）
  │
  └─ 用例 / 断言                ← GoogleTest 工作在这一层
```

GoogleTest 管「**怎么表达一个断言**」；CTest 管「**跑哪些东西、跑了没有、结果如何**」。

---

## 2. 核心机制只有一条

**`add_test()` 注册的不是「测试用例」，而是一条「命令行」；这条命令的进程退出码决定成败（0 = 通过，非 0 = 失败）。**

由此推出三件事：

1. **CTest 完全不知道你用了什么断言库。** 换 gtest / Catch2 / 手写 `if`，CTest 一行都不用改。
   本项目由此推出一个硬要求：`MiniTest` 的 `main` **必须返回失败用例数**——返回 0 就"全绿"，因为 CTest 只看这个。
2. **CTest 不编译任何东西。** 只运行。所以顺序永远是先 `cmake --build`，再 `ctest`。
3. **CTest 不读 `CMakeLists.txt`。** 它读配置期写下的 `CTestTestfile.cmake`。
   → 这是本项目 S0 那个 `Total Tests: 0` 的根因：`tests/CMakeLists.txt` 从未被配置期读到。

---

## 3. 与 GoogleTest 的分工对照

| 能力 | GoogleTest | CTest |
| --- | --- | --- |
| 写断言 / 期望 | ✅ 本职 | ❌ 完全不管 |
| 发现用例 | ✅ `TEST(A,B)` 宏自注册 | ❌ 只知道 `add_test()` 显式注册的命令 |
| 一个进程里跑很多用例 | ✅ 本职 | ❌ 每条 test 一个**独立进程** |
| 调度多个可执行文件 | ❌ | ✅ 本职 |
| 并行 | ⚠️ 有限（sharding） | ✅ `-j N` |
| 超时 | ❌ | ✅ `TIMEOUT` 属性（默认 1500 秒！） |
| 崩溃隔离 | ❌ 整个进程死，后续用例全不跑 | ✅ 只有那条失败，其余照跑 |
| 设置工作目录 / 环境变量 | 自己写代码 | ✅ `WORKING_DIRECTORY` / `ENVIRONMENT` |
| 按标签筛选 | ⚠️ `--gtest_filter` 按名字 | ✅ `LABELS` + `-L`，`-R` / `-E` 正则 |
| 偶发问题复跑 | ❌ | ✅ `--repeat until-fail:20` |
| CI 报告格式 | 自有文本 | ✅ `--output-junit x.xml` |
| 覆盖率 / 内存检查 / dashboard | ❌ | ✅ gcov / valgrind / CDash 集成 |

**「崩溃隔离」对游戏引擎项目特别重要**：越界、空指针、栈溢出让测试进程挂掉是常态。gtest 里一个用例 segfault，后面所有用例都不跑了；CTest 里只有那一条标成 `***Exception: SegFault`，其余跑完。代价是每条测试多一次进程启动开销。

### 你熟悉的 gtest 世界里，这个取舍长什么样

CMake 配 GoogleTest 时通常用 `gtest_discover_tests()`：构建时运行测试程序拿到用例清单，**给每个 `TEST()` 生成一条独立的 CTest 测试**。

- 好处：逐用例隔离，CTest 面板上一目了然，能单独重跑一个失败用例；
- 代价：300 个用例要起 300 次进程。

想省时间可以用它的 `DISCOVERY_MODE PRE_TEST`，或者干脆只注册一条 `add_test` 跑整个二进制。

> **一句话记住**：进程内快但一崩全崩；每进程隔离但慢。
> **别把 1000 个微用例拆成 1000 条 `add_test`。**

---

## 4. 本项目 S0 的分层

```text
tests/MiniTest.h         ← 断言层（这一层代替 gtest）
tests/test_main.cpp      ← 提供 main，进程内跑全部用例，返回失败数
tests/CMakeLists.txt     ← add_test 注册一条命令，交给 CTest 调度
```

分层干净的好处：以后想换 gtest，只换前两层，`add_test` 那行不用动（或改成 `gtest_discover_tests`）。

---

## 5. 命令速查（PowerShell）

### 看（排查"找不到测试"第一招）

```powershell
ctest --test-dir build -N              # 只列出已注册的测试，不执行 ← 最关键
ctest --test-dir build -N -V           # 连"它打算跑什么命令"一起打出来
ctest --test-dir build -C Debug -N     # 多配置生成器（VS）必带 -C
```

### 跑

```powershell
ctest --test-dir build -C Debug                        # 跑全部
ctest --test-dir build -C Debug --output-on-failure    # 只在失败时打印被测程序输出（日常最常用）
ctest --test-dir build -C Debug -V                     # 显示每条测试的命令行
ctest --test-dir build -C Debug -VV                    # 更啰嗦，连环境变量都打
ctest --test-dir build -C Debug -j 4                   # 并行 4 条（用例多了才划算）
```

### 筛

```powershell
ctest --test-dir build -C Debug -R math            # 只跑名字匹配正则 math 的
ctest --test-dir build -C Debug -E slow            # 排除匹配的
ctest --test-dir build -C Debug -L fast            # 只跑打了 LABELS fast 标签的
ctest --test-dir build -C Debug --repeat until-fail:20   # 重复跑直到失败，专抓时序/偶发 bug
```

### 报告

```powershell
ctest --test-dir build -C Debug --output-junit result.xml   # 给 CI 用的 JUnit 格式
```

---

## 6. 五个坑（按会撞到的概率排序）

| # | 坑 | 说明 |
| --- | --- | --- |
| 1 | 改了 `add_test` 不生效 | `CTestTestfile.cmake` 是**配置期**产物 → 必须重新 `cmake -S . -B build`，光 `--build` 不够 |
| 2 | 测试的**工作目录**不是源码目录 | 默认是"调用 `add_test` 的那个构建目录"（`build/tests/`）。将来读 `map.txt` 会找不到 → 用 `WORKING_DIRECTORY` 属性 |
| 3 | 多配置生成器漏 `-C Debug` | 报"找不到测试"，真实原因是可执行文件路径带配置名（`build\bin\Debug\mini_tests.exe`）。用 `$<TARGET_FILE:mini_tests>` 生成路径可避免手写错 |
| 4 | `enable_testing()` 写在子目录 | 必须在**顶层**调用，否则 CTest 从顶层找不到测试文件 |
| 5 | 默认超时 **1500 秒** | 死循环的测试会把 CI 挂 25 分钟 → 用 `TIMEOUT` 属性收紧（游戏循环相关测试必备） |

**另外一条不算坑但要记住**：`include(CTest)` 比单独调 `enable_testing()` 多给你一个标准的 `BUILD_TESTING` 选项和 CDash 支持。本项目自建了 `MINI_BUILD_TESTS`，两者都行，**不建议为此返工**。

---

## 7. PowerShell 特有的三件事

| 需求 | Git Bash | PowerShell |
| --- | --- | --- |
| 看上一条命令的退出码 | `echo $?` | `$LASTEXITCODE`（原生 exe）／`$?` 是布尔值，**不是退出码** |
| 跑构建产物 | `./build/bin/Debug/MiniEngine.exe` | `.\build\bin\Debug\MiniEngine.exe` |
| 检查路径存在 | `ls build/bin` | `Test-Path build\bin` |

所以验收脚本在 PowerShell 下应该这么写：

```powershell
.\build\bin\Debug\mini_tests.exe
$LASTEXITCODE        # 期望：0
```

---

## 8. 什么时候该上 GoogleTest

不用急。判据（见 `MiniEngine_PLAN.md` 的 E-18）：需要 **fixture**、**参数化测试**、**death test**、**mock**，或 CI 需要标准报告格式时。

现在的规模（几十条断言）自制 `MiniTest` 完全够，而且它顺手覆盖了"静态初始化顺序 + 跨 TU 注册"这个真实工程问题，比 gtest 更值得亲手写一遍。

**但 CTest 不管用哪个都该用**——它解决的是另一层的问题。
