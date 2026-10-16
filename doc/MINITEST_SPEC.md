# MiniTest 设计规格（S0 要写的测试框架）

> 用途：S0 阶段要手写的三份文件（`tests/MiniTest.h`、`tests/test_main.cpp`、`tests/test_smoke.cpp`）的**方案层规格**。
> 本文只描述接口、机制、行为与验收，**不含实现代码**（`AGENTS.md` 约定）。
> 配套：`study/CTEST_NOTES.md`（CTest 是调度器，不懂断言）、`doc/S0_CHECKLIST.md`（S0 缺口与 CMake 补丁清单）。
>
> **实现状态（2026-09-24）：已按本规格落地**，代码在 `tests/MiniTest.h`（宏与声明）、`tests/test_main.cpp`（注册表 + runner）、`tests/test_smoke.cpp`（用例）。落地时补了两处规格没写到的细节，已在第 1、3、6 节标注（`ToString` 模板、"suite 名不存在"要报错、C4127 对策）。

---

## 0. 先对齐一个概念

**CTest 本身没有"逻辑"可写。** 它是调度器：只认 `add_test` 注册的命令和该进程的退出码。

所以"实现 ctest 的基础逻辑"实际上等于**写一个测试框架**，CTest 只是最后那一步的消费者：

```text
test_smoke.cpp                       ← 你以后天天写的文件（用例）
  MINI_TEST(Math, AddWorks) { MINI_CHECK_EQ(1 + 1, 2); }
        │ 宏展开 = 一个 static 函数 + 一个 static 对象
        │ static 对象在构造时把自己塞进注册表
        ▼
test_main.cpp                        ← 注册表的唯一实例 + main()
  main(): 解析参数 → 遍历注册表 → 逐个执行 → 统计 → return 失败数
        │ exit code
        ▼
ctest  add_test(NAME smoke COMMAND ...) → 只看退出码 → 报告
```

---

## 1. `MiniTest.h` 对外接口

### 注册

```text
MINI_TEST(suite, name)        // 定义一个用例并自注册
```

### 断言（全部是"失败不中断"语义）

```text
MINI_CHECK(...)               // 表达式为假 → 记一次失败，继续跑
MINI_CHECK_EQ(a, b)           // 失败时打印两侧的值
MINI_CHECK_NEAR(a, b, eps)    // 浮点比较；禁写 ==
MINI_FAIL(msg)                // 无条件失败（留给未实现分支）
```

> **⚠️ 为什么 `MINI_CHECK` 用可变参数**（2026-09-29 改，踩坑后修的）：
>
> 函数宏的参数按**顶层逗号**切分，而"哪一层算顶层"**只数圆括号 —— 花括号不保护逗号**。
> 于是这条本来很自然的断言写不出来：
>
> ```cpp
> MINI_CHECK(Vec2{1.0f, 2.0f}.LengthSq() > 0.0f);   // ← 被拆成两个参数
> ```
>
> 报的是 **C4002「参数过多」**，后面还会跟一串**完全看不懂**的下游语法错误
> （`应为 }` / `miniOk 必须初始化` / `ReportFailure 重定义` …）——
> 因为宏展开后 `static_cast<bool>(Vec2{1.0f` 是个半截表达式，把后面整段代码都带歪了。
> **查语法是白费力气，唯一的真线索是 C4002。**
>
> 修法是让宏**不做参数拆分**：`MINI_CHECK` 只需要一个表达式，用 `...` 原样接收即可
> （消息里的字符串化也换成 `#__VA_ARGS__`，展开后照样是表达式原文）。
>
> **但 `MINI_CHECK_EQ` / `MINI_CHECK_NEAR` 做不到** —— 它们必须**区分** `lhs` / `rhs` / `eps`，
> 可变参数拆不开。所以：
>
> | 写法 | 是否安全 |
> | --- | --- |
> | `MINI_CHECK(Vec2{1, 2}.x > 0)` | ✅ `...` 原样接收 |
> | `MINI_CHECK_EQ((AABB{1, 2, 3, 4}.min.x), 1.0f)` | ✅ **自己包的圆括号是必需的** |
> | `MINI_CHECK_EQ(AABB{1, 2, 3, 4}.min.x, 1.0f)` | ❌ 被拆成 4 个参数 |
> | `MINI_CHECK_NEAR(v.Dot(Vec2{0, 1}), 0.0f, 1e-5f)` | ✅ 花括号已在函数调用的括号里（第 2 层），本来就安全 |
>
> **判据**：不是"有没有花括号"，而是"**花括号有没有出现在参数的最外层**"。

### 头里只有两个函数**声明**（定义在 `test_main.cpp`）

```text
void RegisterTest(const TestCase&);                                // 注册表入口
void ReportFailure(const char* file, int line, const char* expr);  // 断言失败时调用
```

**外加一个 header-only 的 inline 模板**（2026-09-24 落地时补）：

```text
template <typename T> std::string ToString(const T&);              // 把断言两侧的值变成字符串
```

为什么需要它：`MINI_CHECK_EQ` / `MINI_CHECK_NEAR` 要求"失败时打印两侧的值"，但 `ReportFailure` 的签名里没有放值的位置，所以消息只能**在宏里拼好**再传进去，拼消息就要转字符串。它**无状态、不碰注册表**，放头里不会造成下面第 3 条那种"每个 TU 一份"的问题。

### 两个设计决定与理由

| 决定 | 理由 |
| --- | --- |
| 断言**只有"不中断"版**（不做 abort 版） | abort 版要在宏里 `return`，会把用例函数锁死成 `void()`，且与局部 RAII 交互变复杂。gtest 的 `EXPECT_*` / `ASSERT_*` 就是这个取舍，S0 只做前者够了 |
| 失败走 `ReportFailure(file, line, expr)` 函数，而不是宏里直接改全局变量 | 宏里塞语句容易写出"在 `if` 里没加大括号就崩"的东西；调用函数则一行搞定，文件名行号由 `__FILE__` / `__LINE__` 传进去 |

---

## 2. 注册机制：唯一有技巧的地方

### 宏的展开形态（概念）

一个 `static` 用例函数 + 一个 `static` 对象，后者的构造函数调用 `RegisterTest`。

### 三个必须想清楚的点

1. **命名唯一性**：宏内部要把 `suite`、`name`、`__LINE__` 拼进标识符，否则同一文件里多个用例会产生重复定义。
2. **注册表要用"函数内静态局部变量"**：如果写成全局 `std::vector` 且它在另一个 TU 里，就会出现**跨 TU 的静态初始化顺序问题**——注册对象的构造函数可能比那个 vector 先跑，注册直接落空。函数内静态局部变量（C++11 起）保证"首次使用时初始化"，正好绕开。
3. **绝对不能定义在头文件里**：头被 N 个 `.cpp` 包含 → 每个 TU 各得一份注册表 → 现象是"测试数量为 0"或计数乱掉，而且**不报错**。

> 第 3 条和 CMake 侧的 G1（`if(未定义变量)` 恒为假）属于同一类错误：**静默失败**。

---

## 3. runner 行为规格（`test_main.cpp`）

| 输入 | 行为 |
| --- | --- |
| 无参数 | 跑全部用例 |
| `argv[1]` = suite 名 | 只跑该 suite（精确匹配） |
| `argv[1]` = suite 名但**没有对应用例** | 打印 `no case in suite 'X' (N case(s) registered)`，**返回非 0**（手滑打错 suite 名不该拿到假绿；2026-09-24 落地时补的） |
| `--list` | 列出所有 `suite.name`，**返回 0**（注册表为空时也只打印 `0 case(s)` 并返回 0——它是"列清单"，不是"跑测试"） |
| 未知参数（`-` 开头但不是 `--list`）或多于 1 个参数 | 打印用法，**返回非 0**（拼错参数要立刻失败，不能静默跑全部） |

**输出格式**：每条一行 `[ OK ] suite.name` 或 `[FAIL] suite.name`；失败时多打 `file:line` 和表达式原文；末尾一行 `N passed, M failed`。

**返回值 = 失败用例数**（0 = 全过）。

### 一个必须做的边界

注册表为**空**（0 个用例）时要报错并返回非 0。

> 这条是防"假绿"的。如果 0 个用例返回 0，CTest 会报告 100% passed——你明明什么都没有，却得到绿灯。

---

## 4. 建议的写法顺序（每步都能编译、都能看到现象）

不要一次写三份文件。按这个顺序，每步留一个可验证的中间态：

| 步 | 做什么 | 立刻能看到什么 |
| --- | --- | --- |
| 1 | `test_main.cpp`：注册表 + `main` 遍历（此时 0 个用例） | 打印"0 个用例"且**退出码非 0**；`ctest` 报失败 → **证明 runner → 退出码 → CTest 这条链路通了** |
| 2 | `MiniTest.h` 加 `MINI_TEST` 宏；`test_smoke.cpp` 写一个**空用例** | `1 passed, 0 failed`；`ctest` 第一次报绿 |
| 3 | 加 `MINI_CHECK`，用例里写一个必然通过的断言 | 断言机制生效 |
| 4 | 故意让断言失败一次 | `[FAIL]` 打印出 `file:line` 与表达式原文，退出码非 0 |
| 5 | 加 `--list` 与 suite 过滤 | `ctest -N` 与 `mini_tests Math` 可用 |

**第 1 步的价值最大**：它在还没有任何断言的情况下，就证明了整条链路是真的通的。别跳过。

---

## 5. 验收标准

```powershell
.\build\bin\Debug\mini_tests.exe --list          # 列出用例；$LASTEXITCODE = 0
.\build\bin\Debug\mini_tests.exe ; $LASTEXITCODE  # 期望 0
ctest --test-dir build -C Debug -N                # 应列出 mini_tests（不再是 Total Tests: 0）
ctest --test-dir build -C Debug --output-on-failure
# 反向验证：故意让一个断言失败 → ctest 必须报失败
```

---

## 6. 设计层面的坑（写之前先看一遍）

| # | 坑 | 后果 |
| --- | --- | --- |
| 1 | 注册表定义在头文件里 | 每个 TU 一份 → 用例数不对，**且不报错** |
| 2 | 注册表用全局变量而非函数内静态局部变量 | 跨 TU 静态初始化顺序问题 → 注册随机丢失 |
| 3 | 宏里没用 `__LINE__` 拼名字 | 同文件多用例重复定义 |
| 4 | 0 个用例也返回 0 | CTest 报假绿 |
| 5 | `--list` 返回失败数而不是 0 | CI 里"列出测试"被当成失败 |
| 6 | 失败信息不换行 / 不刷新 | 用 `--output-on-failure` 时看不到重点 |
| 7 | 以后把注册辅助移进静态库 | 未引用的注册对象被链接器丢弃 → 静默 0 个测试，那时需要 `WHOLE_ARCHIVE` |
| 8 | 断言条件整个是**常量**（如 `MINI_CHECK_NEAR(0.1 + 0.2, 0.3, 1e-9)`） | MSVC `/W4` 会对 `if (常量)` 报 **C4127**。对策：宏里先把条件存进 `const bool` 再 `if (!miniOk)`（2026-09-24 实测，`(void)miniOk;` 顺带挡住 C4189） |

---

## 7. 与 GoogleTest 的关系

现在的规模（几十条断言）自制 `MiniTest` 完全够，而且它顺手覆盖了"**静态初始化顺序 + 跨 TU 注册**"这个真实工程问题，比直接上 gtest 更值得亲手写一遍。

换 gtest 的判据（见 `doc/MiniEngine_PLAN.md` 的 E-18）：需要 fixture、参数化测试、death test、mock，或 CI 需要标准报告格式时，再用 `FetchContent` 引入。

**届时 CTest 那层不用改**——它只管调度，不管断言库。详见 `study/CTEST_NOTES.md`。
