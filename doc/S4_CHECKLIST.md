# S4 实现清单与验收

> **用法**：在"进度板"里勾进度。第 6 节是**施工图**，第 9 节是**避雷地图**，第 10 节是**验收证据**。
> 本文件只描述**要什么、怎么验证**，不含可实现代码（`AGENTS.md` 约定：AI 默认只出方案；定义与伪代码是方案的一部分，`.cpp` 的函数体不是）。
>
> 相关文件：`doc/MiniEngine_PLAN.md`（S4 小节、A.5 坐标约定、E-8 风险）、`doc/S3_CHECKLIST.md`（上一阶段的格式与教训）、`doc/TECH_DEBT.md`（B5 那条纪律在这里继续用）。
>
> **环境提醒**：默认 shell 是 **PowerShell**。退出码用 `$LASTEXITCODE`（不是 `echo $?`），路径用反斜杠。

---

## 当前状态（2026-09-29：**方案已定，未开工**）

- **S0 / S1 / S2 / S3 均已完成并验收。** 当前 43 个 commit，用例 **35**，工作区干净。
- 本阶段的接口决策已拍板（第 8 节共 **4 条**，全部由本人于 2026-09-29 拍定）。
- **本阶段的特殊之处：S4 是第一个"没有任何画面"的阶段。** 交付物是纯库 + 测试，
  所以第 10 节的验收里**没有"跑 exe 看现象"这一项** —— 可观察结果是
  `mini_tests MathUtils` / `Vec2` / `AABB` / `Ray` **四个 suite 全绿** + 退出码 0。这是决策 3 的直接后果。
- ⚠️ **没有 `mini_tests Math` 这条命令**：runner 的 suite 过滤是**精确匹配**（`MINITEST_SPEC` 第 3 节），
  而本阶段按"被测单元"分了 4 个 suite。想一次跑完 S4 就**不带参数**（跑全部 68 个用例）。
  见落地偏差 **D4** —— 这条命令是文档先写错、实现时才发现的。
- **S4 之后**：`math` 会立刻在 S5 的 `World` 里获得真实调用者，那时它才"看得见"。

### ⚠️ 分工约定（`AGENTS.md` 1.4；本阶段沿用）

**编译与运行由本人执行，AI 只做代码编辑。**

| 谁 | 做什么 |
| --- | --- |
| AI | 只改/建源文件。**不跑 `cmake`、不跑 `ctest`、不跑 exe** |
| 本人 | 构建、跑测试、看现象，把结果回报 |
| AI | 按回报结果决定下一步（改 bug / 收尾 / 继续） |

推论：**一个步骤的 commit 由本人在验证通过后决定何时打**；在验证之前改动只留在工作区。
报告里会明确写出"待你执行什么、期望看到什么"。

### 两件被推迟的事（**不是忘了，是有意排后**）

| 事项 | 状态 | 说明 |
| --- | --- | --- |
| **技术债 B5**（11 处"只有测试调用"的接口注释）+ **A2**（抄量化数据） | ⏸ 退后 | 本人定：技术债退后。B5 的**准确清单**已在 `TECH_DEBT.md` 里重查过 |
| **`git push`** | ⏸ 等 S4 完成 | 本地已有 6 个未推送 commit。**本阶段不碰网络**（这台机器的 GitHub 通路时通时不通，见下方"已知环境问题"） |

**已知环境问题**：`github.com` 的 IP 段存在 **SNI 层间歇性过滤**（TCP 能连、TLS 却被丢），
所以 `hosts` 钉 IP 的办法**每天都要重扫**。`ssh.github.com:443` 一直稳定可用（已验证握手），
是长期解法。这与本阶段代码**完全无关**，只是记录在这里免得再花时间重复诊断。

> S4 的预估规模：**源文件 ~280 行**（`MathUtils.h` ~55 / `Vec2.h` ~95 / `AABB.h` ~70 / `Ray.h` ~60）
> + **测试 ~250 行**（约 **33 个**用例）。
> PLAN 原文估 ~300 行 + ~200 行测试，基本吻合（差异见第 12 节）。

---

## 进度板

> 状态：⬜ 未开始 · 📝 代码已写待验 · ✅ 已验并提交

| # | 任务 | 对应 commit | 状态 |
| --- | --- | --- | --- |
| **S4-1** | `math/MathUtils.h`：标量工具 + `FloorToInt`/`RoundToInt` | commit 1 | 📝 |
| **S4-2** | `math/Vec2.h`：值类型 + 运算符 + `Normalized` 保护 | commit 1 | 📝 |
| **S4-3** | `tests/test_math.cpp`：MathUtils 8 个 + Vec2 9 个用例；`tests/CMakeLists.txt` 加一行 | commit 1 | 📝 |
| **S4-4** | `math/AABB.h`：含**贴边不算相交**的开区间语义 | commit 2 | 📝 |
| **S4-5** | `tests/test_math.cpp`：AABB 7 个用例 | commit 2 | 📝 |
| **S4-6** | `math/Ray.h`：slab 法 + **显式 `d == 0` 分支** | commit 3 | ⬜ |
| **S4-7** | `tests/test_math.cpp`：Ray 9 个用例 | commit 3 | ⬜ |
| **S4-8** | 反向验证 3 条实跑并**还原现场**（见 10.3） | commit 3 | ⬜ |
| **S4-9** | docs 收口：PLAN S4 标 ✅ / A.2 目录树 / 差异表 / 命令速查 | commit 4 | ⬜ |

### 落地偏差（实现时发现的问题，逐步追加）

| # | 发现在 | 偏差 | 处置 |
| --- | --- | --- | --- |
| **D1** | 步骤 1 | **本文档 7.1 的对照表把 `{inf,0}` 那一行写错了**：原来写"走除法：`1/inf = 0` → 返回 `{0,0}`"，但实现是 `x / len`（不是 `1/len`），所以 `inf / inf = **NaN**`，**会漏出去** | 表格已改准。这属于"输入本身是 inf"这种**超出文档范围**的输入（约定 1 不防御）；`{1e30,0}`（LengthSq 溢出）那一档**是**被接住的，用例 15 覆盖了它 |
| **D2** | 步骤 1 | 6.2 的施工图没规定 `operator==` / `operator<<` 的有无，实现时**两个都不提供** | 写进第 8 节原则 7 —— 这是有意的：`==` 会诱导浮点精确比较，`<<` 目前只有测试想用 |
| **D3** | 步骤 1 **第一次编译失败**（本人回报） | `test_math.cpp` 有 **7 处** `MINI_CHECK(Vec2{1.0f, 2.0f}...)` 编译不过：`MINI_CHECK` 是**函数宏**，参数里的逗号会被当分隔符，而**花括号不保护逗号**（只有圆括号保护）→ C4002「参数过多」+ 一串看不懂的下游语法错误 | **按要求"按最正确的方式"从根上修 `MiniTest.h`**：`MINI_CHECK` 改成可变参数 `(...)`，那 7 处**不再需要任何括号**（先写的绕过手法已收回）。`MINI_CHECK_EQ`/`MINI_CHECK_NEAR` 必须区分参数、可变参数拆不开 → 仍要求调用方自己包圆括号（只剩 1 处，注释写明"这对括号不能省"）。规格同步到 `MINITEST_SPEC` 第 1 节，并进坑表 #27 |
| **D4** | 步骤 1（本人回报 `no case in suite 'Math'`） | **本文档多处写了 `mini_tests Math`，但这条命令根本不存在**：runner 的 suite 过滤是 `strcmp` **精确匹配**（`test_main.cpp:84`，`MINITEST_SPEC` 第 3 节也这么规定），而本阶段按被测单元分了 `MathUtils`/`Vec2`/`AABB`/`Ray` 四个 suite | 文档里 8 处命令改成真实 suite 名，`Math` 这条去掉（想一次跑完 S4 就不带参数）。<br>**根因是我照抄了 `MINITEST_SPEC` 的示例** —— 那份规格第 1 节的示例 suite 就叫 `Math`（`MINI_TEST(Math, AddWorks)`），我顺着写了 `mini_tests Math`，却没想到真实 suite 名是 `MathUtils`/`Vec2`。**还是"写了没跑过"**：**照抄一个示例，不如去 grep 一遍真的名字** |
| **D5** | 步骤 2 | **本文档 10.1 的用例 23 描述有错**：原文写"`min > max` → `IsValid()` false **且 `Intersects` 恒 false**"。写用例时才发现后半句**不成立** —— 反盒上的 `Intersects` 是普通公式算出来的，可能 true 也可能 false，取决于对方盒子在哪（例：`{min={5,0},max={1,10}}` 与 `{0,0}..{10,10}` 相交判 true） | 描述改成"`IsValid()` false；**故意不测**反盒的 `Intersects`"（没意义的东西不写进验收）。用例里加一句注释说明"反盒应该在构造阶段避免，走 `FromCorners()` 永远造不出反盒" |
| **D6** | 步骤 2（本人回报 `AABB.PointBoxContainsButDoesNotIntersect` **FAIL**） | **我用例的期望写错了，代码是对的。** 我把"两个相同退化盒不相交"外推成了"退化点盒**不与任何东西**相交"，多推了一步。真实行为：点盒在另一个盒**内部**时 `Intersects` 是 **true**（`1 < 2 && 0 < 1` 都成立 —— 公式只看不等式，不看重叠长度） | ① 用例改名 `DegeneratePointBoxSemantics`（原名在断言一件假事）并**把三种情况分开钉住**（vs 自己 / 在内部 / 在边界）；② `AABB.h` 的 `Intersects` 注释与文件头各加一段"退化盒 ≠ 闭区间的点"；③ 用例 24 的描述同步。**结论**：`Intersects` 的公式没错，**是我的规格文字错了两处**（D5、D6）—— 两条都是"没推到底就写进文档" |
| **D7** | 步骤 2 收尾（本人问"介绍下 AABB 做了什么"，我回头核对 PLAN 时自查发现） | **我在头注释里给 `AABB` 编了两个未来的调用者**：写了"S11 的 Debug 绘制、S12 的 DDA 步进"。核对 PLAN 后：**S11 是 Debug Overlay + Logger + Profiler**（耗时表格，全篇没有 AABB）、**S12 是 DDA 格子步进**（不走 `AABB::Intersects`）。同一句里的 S7/S9 那两条**是真的**（PLAN:493 / PLAN:521）。**清理时又查出两处同款**：`Vec2.h` 的 `Perp()` 写了"S12 的墙面朝向"、`LengthSq()` 写了"S7 的谁更近会大量用它"、`MathUtils.h` 的 `Lerp` 写了"S9 的子弹落点预测会需要" —— PLAN 里**都没有** | 全部改成**只写核对过的**：`AABB.h` 只留 S7/S9 并注明 `PLAN:983` 的定性；`Vec2.h`/`MathUtils.h` 只讲函数自身的道理（`LengthSq` 引 `PLAN:457` 的"避免 sqrt"），**不再指认具体阶段**；`AABB.h` 明写"**S4 阶段它还没有任何调用者**"。<br>**教训：写"谁会用"必须去 grep PLAN，不能凭印象。** 这和 D4（照抄示例）是同一毛病的两种表现：**都是让记忆代替检索** |


---

## 1. S4 目标与不做什么

**目标**：`engine/include/mini/math/` —— **header-only、零引擎依赖、`mini::math` 命名空间**。
给 S5+ 提供向量、轴对齐盒、射线。

| 不做 | 理由（不是"以后再说"，是"**为什么现在不该做**"） |
| --- | --- |
| **矩阵**（`Mat3`/`Mat4`） | S12 射线投射是 **2D DDA**，终端渲染是 2D 投影 —— **整条路线没有一处需要矩阵**。加它只会有零调用者 |
| **四元数 / 欧拉角** | 没有 3D 旋转 |
| **模板化 `Vec2T<T>`** | PLAN 明确写了"别一上来就模板化 `T`"。运算符全要变模板、报错信息变难读，换来 0 收益（本项目只有 `float` 一种精度） |
| **`Vec3` / `Rect` 的实体** | 决策 1：**只预留接口契约，不建半成品文件**。见 6.6 |
| **SIMD / 手写快速 sqrt / 三角函数查表** | 本项目瓶颈是终端 I/O（一次 `WriteConsole` 几毫秒），不在算术 |
| **坐标变换全流程**（world→camera→screen） | 需要相机（S6）。S4 只做**其中不可再拆的那一小块**：`FloorToInt`/`RoundToInt`（决策 4，服务 PLAN E-8） |
| **改 `main.cpp`** | 决策 3：S4 不加 demo。演示逻辑塞进 `DemoGame` 会在 S5 拆 `game/` 时再挪一次 |
| **任何 `render` / `platform` / `core` 的 include** | `math` 必须在依赖图**最底层**（见第 3 节） |

---

## 2. 交付物清单

| 文件 | 内容 | 估行 |
| --- | --- | --- |
| `engine/include/mini/math/MathUtils.h` | `kPi` `Clamp` `Lerp` `DegToRad` `Sign` `ApproxEquals` **`FloorToInt` `RoundToInt`** | ~55 |
| `engine/include/mini/math/Vec2.h` | `struct Vec2` + 成员运算符 + 自由运算符 + `LengthSq/Length/Normalized/Dot/Perp/Min/Max/ApproxEquals` | ~95 |
| `engine/include/mini/math/AABB.h` | `FromCenterSize/FromCorners/Center/Size/Contains/Intersects/IsValid` | ~70 |
| `engine/include/mini/math/Ray.h` | `struct Ray` + `IntersectsAABB`（slab 法） | ~60 |
| `tests/test_math.cpp` | suite `Math`，**约 33 个用例** | ~250 |

**CMake 改动（只有两处）**：

1. `tests/CMakeLists.txt`：源文件列表**手写**加一行 `test_math.cpp`（`AGENTS.md` 禁用 `file(GLOB)`）；
   顶部那段"哪个文件是哪一阶段的用例"注释也加一行。
2. `engine/CMakeLists.txt`：**不新增源文件**（math 是 header-only）。
   但**要改那段头注释**：加一行说明"S4 的 math 是 header-only，故列表中不出现" ——
   否则下一个人翻到这里会以为漏了 S4。

**`engine/CMakeLists.txt` 的 include 已经够用**（已核对）：
`target_include_directories(engine PUBLIC include)` → 任何链接 `MiniEngine::engine` 的目标
都能 `#include <mini/math/Vec2.h>`，不需要新增任何 `target_include_directories`。

---

## 3. 依赖规则（本阶段**不新增边**）

依赖方向**单向向下**，`math` 在最底层：

```
core / render / platform        ← 上层（S4 一行都不改）
        │
        ▼
      math                      ← 本阶段新增，只被上面几层 include
        │
        ▼
     标准库                      ← <cmath> <optional> <cstdint>
```

**规矩（照 PLAN A.3 的精神，比 A.3 更严）**：

1. `math` 里**不许出现** `<windows.h>`、ANSI 转义、`#ifdef`、终端相关任何东西
2. `math` 里**不许 include** `mini/core/*`、`mini/render/*`、`mini/platform/*`
3. `math` **可以被任何层 include** —— 这是它唯一的存在理由
4. 头文件**不许**有全局可变状态、不许 `using namespace std`

> **为什么这条比 S2/S3 更值得写死**：`math` 是唯一"永远不变"的一层（PLAN A.4：按变化频率分层）。
> 它一旦反向依赖了上层，后面所有阶段都会被拖住。

---

## 4. 数据结构与所有权

**全部是值类型。这里没有任何所有权问题 —— 这正是 `math` 与 `platform` 层最大的不同。**

| 类型 | 成员 | 大小 |
| --- | --- | --- |
| `Vec2` | `float x, y` | 8 字节 |
| `AABB` | `Vec2 min, max` | 16 字节 |
| `Ray` | `Vec2 origin, dir` | 16 字节 |

- **trivially copyable、无堆分配、无析构、无指针** → 可以随便按值传、可以放进 `std::vector` 而不用担心失效
- 头文件里放 `static_assert(std::is_trivially_copyable_v<Vec2>)` 之类的自检（**编译期钉住**，
  以后谁想加个 `std::string` 成员会立刻炸）
- 大部分是 `constexpr` → 可以在**编译期**当常量用（`constexpr AABB kBox = ...`）
- **没有 id、没有句柄、没有引用计数**：这些在 S5 的 `World` 里才会出现，`math` 不该提前替它决定

---

## 5. 逐文件实施步骤（每步都立刻跑一次）

> 每一步都是**本人构建、本人回报**。AI 只改文件，报告里写明"待你跑什么、期望看到什么"。

### 步骤 S4-1 / S4-2 / S4-3（commit 1）：`MathUtils` + `Vec2`

1. `engine/include/mini/math/MathUtils.h`：第 6.1 节的 8 个函数
2. `engine/include/mini/math/Vec2.h`：第 6.2 节的类型与运算符
3. `tests/test_math.cpp`：第 10.1 节的 **前 17 个**用例（MathUtils 8 + Vec2 9）
4. `tests/CMakeLists.txt`：加 `test_math.cpp`

**这一步刻意不接线**：`main.cpp`、`engine/CMakeLists.txt` 一行不改，其它层也没人 include `math`。

**立刻跑**

```powershell
cmake --build build --config Debug
.\build\bin\Debug\mini_tests.exe MathUtils    # 期望 8 passed
.\build\bin\Debug\mini_tests.exe Vec2         # 期望 9 passed
.\build\bin\Debug\mini_tests.exe              # 期望 35 → 52 passed
ctest --test-dir build -C Debug --output-on-failure
```

**应看到**：0 warning、`ctest` 仍 1/1、用例 35 → **52**、`MiniEngine.exe` 行为与 S3 **完全一致**。

> ⚠️ **`math` 是 header-only，所以"编译过"这件事完全依赖 `test_math.cpp` 去 include 它。**
> 如果哪个头文件没被 include，编译器**根本不会看它** —— 见坑 #15。

### 步骤 S4-4 / S4-5（commit 2）：`AABB`

1. `engine/include/mini/math/AABB.h`：第 6.3 节
2. `tests/test_math.cpp`：加第 10.1 节的 **中间 7 个**用例

**立刻跑**

```powershell
cmake --build build --config Debug
.\build\bin\Debug\mini_tests.exe AABB      # 期望 7 passed
.\build\bin\Debug\mini_tests.exe          # 期望 35 → 59 passed
```

### 步骤 S4-6 / S4-7（commit 3）：`Ray` + 反向验证

1. `engine/include/mini/math/Ray.h`：第 6.4 节 + 第 7.3 节的 slab 法
2. `tests/test_math.cpp`：加第 10.1 节的 **最后 9 个**用例
3. **跑第 10.3 节的反向验证 3 条**（每条都要看到红，且必须还原现场）

**立刻跑**

```powershell
cmake --build build --config Debug
.\build\bin\Debug\mini_tests.exe Ray       # 期望 9 passed
.\build\bin\Debug\mini_tests.exe          # 期望 35 → 68 passed（= S4 全部）
ctest --test-dir build -C Debug
ctest --preset asan                       # ASan 这条路也要过（S3 已打通）
```

### 步骤 S4-9（commit 4）：docs

1. `doc/MiniEngine_PLAN.md`：S4 小节标 ✅ + 实测规模；A.2 目录树（`math/` 下**实际**只有 4 个文件）；差异表（第 12 节）
2. `doc/S4_CHECKLIST.md`：本节（落地记录、实际用例数、决策变化）
3. `doc/TECH_DEBT.md`：过一遍新接口有没有"零调用者"的

---

## 6. 接口设计（施工图）

> 命名空间一律 `mini::math`。头文件一律 `#pragma once`（与现有头文件保持一致）。
> 下面全是**声明**，函数体只在第 7 节给伪代码 —— 符合 `AGENTS.md` 的"方案边界"。

### 6.1 `MathUtils.h`

```cpp
namespace mini::math {

inline constexpr float kPi = 3.14159265358979323846f;

// 假定 lo <= hi（不检查；调用方负责）。不处理 NaN —— NaN 进 NaN 出。
[[nodiscard]] constexpr float Clamp(float v, float lo, float hi) noexcept;

// **不钳制 t**：t 超出 [0,1] 时按线性外推，这让它也能当"沿线段延长"用。
[[nodiscard]] constexpr float Lerp(float a, float b, float t) noexcept;

[[nodiscard]] constexpr float DegToRad(float deg) noexcept;

// 正/负/零 → +1/-1/0。**-0.0f 返回 0**（不是 -1）。NaN 返回 0。
[[nodiscard]] constexpr float Sign(float v) noexcept;

// 近似相等。**只做绝对误差，不做相对比较** —— 本项目坐标量级 < 1e4，绝对 eps 够用。
// NaN 对任何值（包括 NaN 自己）都返回 false，这是**正确行为**，注释里要写明。
[[nodiscard]] constexpr bool ApproxEquals(float a, float b, float eps = 1e-5f) noexcept;

// world(float) → tile/screen(int)。**serves PLAN E-8 的 S4 那一半。**
// 见 7.2：必须用 floor 语义，不能用 (int) 截断。
[[nodiscard]] constexpr int FloorToInt(float v) noexcept;
[[nodiscard]] constexpr int RoundToInt(float v) noexcept;   // 语义对齐 std::round

} // namespace mini::math
```

### 6.2 `Vec2.h`

```cpp
namespace mini::math {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    // —— 全部 constexpr：纯算术，不碰 <cmath> ——
    constexpr Vec2& operator+=(Vec2 o) noexcept;
    constexpr Vec2& operator-=(Vec2 o) noexcept;
    constexpr Vec2& operator*=(float s) noexcept;
    constexpr Vec2& operator/=(float s) noexcept;   // s == 0 **不检查**（见坑 #17）

    [[nodiscard]] constexpr float LengthSq() const noexcept;
    [[nodiscard]] constexpr float Dot(Vec2 o) const noexcept;
    [[nodiscard]] constexpr Vec2  Perp()     const noexcept;  // 逆时针 90°：(x,y) → (-y,x)
    [[nodiscard]] constexpr Vec2  Min(Vec2 o) const noexcept;  // 逐分量
    [[nodiscard]] constexpr Vec2  Max(Vec2 o) const noexcept;
    [[nodiscard]] constexpr bool  ApproxEquals(Vec2 o, float eps = 1e-5f) const noexcept;

    // —— 这两个**不能 constexpr**：要用 std::sqrt（决策 2，见第 12 节差异 1）——
    [[nodiscard]] float Length() const noexcept;
    [[nodiscard]] Vec2  Normalized() const noexcept;   // 零/NaN/inf → 返回 {0,0}
};

// 二元运算符用**自由函数**（左右对称；以后想加 `2.0f * v` 只需一个重载）
[[nodiscard]] constexpr Vec2 operator+(Vec2 a, Vec2 b) noexcept;
[[nodiscard]] constexpr Vec2 operator-(Vec2 a, Vec2 b) noexcept;
[[nodiscard]] constexpr Vec2 operator-(Vec2 v) noexcept;
[[nodiscard]] constexpr Vec2 operator*(Vec2 v, float s) noexcept;
[[nodiscard]] constexpr Vec2 operator*(float s, Vec2 v) noexcept;
[[nodiscard]] constexpr Vec2 operator/(Vec2 v, float s) noexcept;

// 编译期自检（写在头文件里，谁 include 谁检查）
static_assert(sizeof(Vec2) == 8);

} // namespace mini::math
```

**为什么成员运算符 vs 自由运算符要分开写**：`+=` 这类**复合赋值必须是成员**（语言规定），
而 `+` 这类**用自由函数更对称**（`a + b` 和 `b + a` 都能走同一套规则，也方便将来加标量左乘）。
这不是风格偏好，是 C++ 的惯例写法。

### 6.3 `AABB.h`

```cpp
namespace mini::math {

struct AABB {
    Vec2 min{};
    Vec2 max{};

    // c ± size/2。注意 size 是**全宽全高**，不是半宽。
    [[nodiscard]] static constexpr AABB FromCenterSize(Vec2 c, Vec2 size) noexcept;

    // **自动排序**：传两个角点，谁大谁小无所谓（见坑 #10）
    [[nodiscard]] static constexpr AABB FromCorners(Vec2 a, Vec2 b) noexcept;

    [[nodiscard]] constexpr Vec2 Center() const noexcept;   // (min + max) / 2
    [[nodiscard]] constexpr Vec2 Size()   const noexcept;   // max - min

    // —— 这两个的边界语义**故意不一致**（见坑 #5/#6）——
    // Contains：**闭区间** min <= p <= max —— "点在盒子里"的直觉
    [[nodiscard]] constexpr bool Contains(Vec2 p) const noexcept;
    // Intersects：**开区间** —— 两盒刚好贴边时**不算**相交
    [[nodiscard]] constexpr bool Intersects(const AABB& o) const noexcept;

    [[nodiscard]] constexpr bool IsValid() const noexcept;  // min <= max（逐分量）
};

} // namespace mini::math
```

### 6.4 `Ray.h`

```cpp
namespace mini::math {

struct Ray {
    Vec2 origin{};
    Vec2 dir{1.0f, 0.0f};   // **不必是单位向量**，但必须非零（调用方保证）

    // nullopt = 不相交（含"盒子整个在背后"）
    // 有值   = 沿射线**第一次进入盒子**的参数 t；**起点在盒内 → t = 0**
    [[nodiscard]] std::optional<float> IntersectsAABB(const AABB& box) const noexcept;
};

} // namespace mini::math
```

**为什么返回 `std::optional<float>` 而不是 `bool` + 出参**：

| 方案 | 评价 |
| --- | --- |
| `bool` + `float& outT` | 出参容易忘传、调用点读不出来（`f(b, t)` 里 `t` 是入还是出？） |
| `bool`，t 放进成员 | 引入隐藏状态，`struct Ray` 不再是无状态值类型 |
| **`std::optional<float>`** | 命中信息**就在返回值里**；`std::optional` 是值类型、可 `constexpr` 友好，C++17 起标准 |
| `std::optional<RayHit{float t; Vec2 normal;}>` | **现在不做**：法线只在"反弹"时才需要，而反弹要到 S9 之后。触发条件写进 6.6 |

### 6.5 文件骨架

四个头文件都按同一形状组织（与 `InputState.h` / `Color.h` 的现有风格一致）：

```text
#pragma once
// 文件头注释：这一层是干什么的、依赖谁、被谁用、有哪些"约定死在注释里"的东西

#include <必要的标准库>

namespace mini::math {

// 类型 / 函数，每个公开接口上方一行注释说明"为什么存在、谁会用"

} // namespace mini::math
```

**注释的写法沿用 `TECH_DEBT.md` B5 定下的原则**：
**不写"它是什么"（名字已经说了），写"它为什么存在、什么时候会有人用、哪些约定是故意这样定的"。**

### 6.6 将来加 `Vec3` / `Rect` 的接口契约（**预留，不实现**）

> **决策 1 的落实方式**：**不建 `Vec3.h` / `Rect.h` 半成品文件**，契约写在这里。
>
> **为什么不建文件**：① 没被任何 TU include 的头文件**编译器根本不看**，等于写在纸上；
> ② 一旦被 include 就必须能编译，半成品编译不过；③ 它会让交付物清单里出现一个
> **看起来做完了其实没做**的东西。这个项目**已经踩过两次**同一类坑
> （`ms-clang-debug` 预设、`-Wall` 死分支 —— 都是"写在文件里、看着配好了、其实从没跑过"）。
>
> 同时在 `Vec2.h` 的文件头留一行指路：`// 3D 时同目录加 Vec3.h，形状与本文件逐条对应（见 doc/S4_CHECKLIST.md 6.6）`。

```text
Vec3 —— 触发条件：S12 射线投射若改成真 3D，或出现需要 3D 方向向量的地方（估 ~40 行）

    struct Vec3 { float x = 0.0f, y = 0.0f, z = 0.0f; };

    与 Vec2 **逐条同形**（不要另发明一套）：
      += -= *= /= 用成员；+ - * / 用自由函数（含标量左右乘）
      LengthSq / Length / Normalized / Dot / Min / Max / ApproxEquals
      Vec3 独有 Cross(Vec3)
      **constexpr 划分与 Vec2 一致**：Length / Normalized 照样不标 constexpr

    Vec3::Cross 与 Vec2::Perp 的关系（写进 Perp 的注释）：
      2D 的"叉积"结果是标量（z 分量），拿不到 z 就只能拿它的符号；
      Perp() 就是"叉积那个方向"的具体化 —— 所以 Perp 的**方向约定必须和 Cross 一致**，
      否则将来加 Vec3 时两边会对不上。

Rect —— 触发条件：S6 视口裁剪真需要 (x, y, w, h) 形式时（估 ~30 行）

    struct Rect { float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f; };
    **必须提供显式的 ToAABB() / FromAABB()**，注释写明：
      "两者表达同一个几何量，同时存在只是为了两个使用场景的书写习惯
       （AABB 适合碰撞、Rect 适合视口/UI），转换只许走这两个函数。"
    ⚠️ 若 S6 发现用 AABB 也能写得一样顺，就**不要加 Rect** —— 两套并存是转换 bug 的温床。

Ray 的命中结构 —— 触发条件：S9 之后出现"反弹/反射"需求时

    struct RayHit { float t; Vec2 normal; };
    届时 IntersectsAABB 的返回类型改成 std::optional<RayHit>（**这是一次破坏性改动**，
    所以现在不做；现在只需要 t）
```

---

## 7. 算法与伪代码

### 7.1 `Normalized()` 的零 / NaN / inf 保护

```text
Normalized():
    len2 = LengthSq()
    if (!(len2 > 0.0f)):      # ← 关键：写成 !(x > 0)，**不是** x <= 0
        return {0, 0}
    return (*this) / Sqrt(len2)
```

**为什么是 `!(len2 > 0.0f)` 而不是 `len2 <= 0.0f`**：

| 输入 | `len2` | `len2 <= 0` | `!(len2 > 0)` | 说明 |
| --- | --- | --- | --- | --- |
| `{0,0}` | `0` | true | true | 两种写法都对 |
| `{NaN,0}` | `NaN` | **false** | **true** | **只有 `!(x>0)` 挡得住！** |
| `{1e30,0}` | `inf`（**溢出**） | false | false | `len = sqrt(inf) = inf`，而 `1e30 / inf = 0` → 返回 `{0,0}` ✅ 被接住（用例 15） |
| `{inf,0}` | `inf` | false | false | ⚠️ **会漏出 NaN**：`inf / inf = NaN`。这属于"输入分量本身就是 inf" —— **超出文档范围**（约定 1 不防御）。见落地偏差 **D1** |
| `{1e-30,0}` | `0`（下溢） | true | true | 返回 `{0,0}` |

一行 `!` 同时挡住 0、NaN、负数三种情况。这是 PLAN S4 坑"归一化零向量产生 NaN
并污染整个物理循环"的解药，也是**反向验证 1 的目标**。

### 7.2 `FloorToInt` / `RoundToInt`

```text
FloorToInt(v):
    i = (int)v                              # 向零截断
    if (v < 0.0f && (float)i != v):         # 负数且有余 → 再退一格
        return i - 1
    return i

RoundToInt(v):                              # 语义对齐 std::round：**.5 远离零**
    if (v >= 0.0f): return  FloorToInt(v + 0.5f)
    else:           return -FloorToInt(-v + 0.5f)
```

**为什么手算而不用 `std::floor` / `std::round`**：**它们和 `std::sqrt` 一样，在 C++20 不是 `constexpr`**。

**这里和决策 2 形成一个有意的对照**（这条原则要写进 `MathUtils.h` 的注释）：

> `constexpr` 值不值得保，**看代价**。
> `sqrt` 要 12 行 + 一个得单独测精度的实现 → **放弃**（决策 2）。
> `floor` 只要 2 行纯算术 → **保住**（决策 4）。
> 判据不是"能不能做到"，而是"**多写的东西值不值它的复杂度**"。

**为什么 `RoundToInt` 不写成 `FloorToInt(v + 0.5f)`**：那样在负半轴会变成
"向 +inf 舍入"（`-0.5 → 0`），与 `std::round` 的"远离零"（`-0.5 → -1`）**不一致**。
标准库已经有约定时，**跟着标准库走**比自己发明一套安全。

### 7.3 `Ray::IntersectsAABB` 的 slab 法

```text
IntersectsAABB(box):
    tNear = -INF
    tFar  = +INF

    for axis in {x, y}:
        o = origin[axis]
        d = dir[axis]

        if (d == 0.0f):                                 # ← 显式分支，**不靠** 1/0 = inf
            if (o < box.min[axis] || o > box.max[axis]):
                return nullopt                          # 与这层平行且在外侧 → 永远打不中
            continue                                    # 平行且在内 → 这条轴不限制 t
        else:
            inv = 1.0f / d
            t1  = (box.min[axis] - o) * inv
            t2  = (box.max[axis] - o) * inv
            if (t1 > t2): swap(t1, t2)
            tNear = max(tNear, t1)
            tFar  = min(tFar,  t2)
            if (tNear > tFar): return nullopt           # 早退：两层的交集是空

    if (tFar < 0.0f): return nullopt                    # 盒子整个在射线背后
    return max(tNear, 0.0f)                             # 起点在盒内 → tNear < 0 → 钳成 0
```

**为什么必须显式写 `d == 0` 分支**（这是本阶段最值得记的一条）：

- 教科书做法是 `t1 = (min - o) * (1/d)`，靠 IEEE 的 `inf` 参与 `min/max` 自然收敛。
  **绝大多数情况下它是对的。**
- 但当 `o` 恰好**落在 slab 边界上**且 `d == 0` 时，会算出 **`0 * inf = NaN`**，
  然后 `std::max(NaN, x)` 的结果**是实现定义的**（MSVC 的 `std::max` 是 `a < b ? b : a`，
  NaN 参与时的走向和直觉不一样）→ 不可预测、不可复现。
- 显式分支还有个附带好处：**两条 `nullopt` 的路（平行在外 / 背后）都能单独构造用例**，
  于是"这段逻辑被测到了"是可证的。

**擦角怎么算**：判定分离用的是 `tNear > tFar`（**严格大于**），所以 `tNear == tFar`
（射线恰好擦过盒子角）**算命中**。这是有意的，见 10.1 的 `Ray.GrazesCorner`。

---

## 8. 接口决策表（2026-09-29 已拍板）

| # | 决策 | 选择 | 理由 / 代价 |
| --- | --- | --- | --- |
| **1** | `Vec3` / `Rect` 做不做 | **只预留接口契约，不建文件**（6.6） | 整条路线没有 `Vec3` 的使用者（S12 是 2D DDA）；`Rect(min,max)` 与 `AABB` 是同一个几何量，并存会生转换 bug。**"预留"落在文档而不是半成品文件** —— 理由见 6.6 的引用块 |
| **2** | sqrt 相关函数的 `constexpr` | **不标**（`Length`/`Normalized` 是 `inline`） | `std::sqrt` 在 C++20 不是 `constexpr`（**证据**：本机 MSVC 14.44 `<cmath>` 第 264 行 `inline float sqrt(...)`，`_CONSTEXPR23` 只在 C++23 下展开）。备选方案（`is_constant_evaluated()` + 牛顿迭代，约 12 行）被否：多一个 sqrt 实现要单独测精度 |
| **3** | 要不要可见 demo | **不要**，`ctest` 全绿即可 | S4 是纯库阶段。测试有明确输出与退出码，就是"能运行、能看到结果"。真正的可见效果等 S5 的 `World`。代价：本阶段"看不到东西"，需要接受 |
| **4** | PLAN E-8 的坐标转换在 S4 做吗 | **做**：`FloorToInt` / `RoundToInt` + 测试 | E-8 原文写的就是 "S4/S6"。现在只花约 10 行；等 S6 和相机一起做容易被夹带出错。代价：这两个函数现在**只有测试调用**（属于 B5 那类，注释里已写明用途与调用者） |

**附加原则（本次调研得出，写进注释）**：

| # | 原则 |
| --- | --- |
| 5 | **`constexpr` 值不值得保，看代价**（对照见 7.2）；判据不是"能不能"，是"值不值" |
| 6 | **接口注释写"为什么存在、谁会用它"，不写"它是什么"**（沿用 `TECH_DEBT.md` B5）；S4 新增的每个接口都要能回答"现在谁在调，或者什么时候会有人调" |
| 7 | **`Vec2` 刻意不给 `operator==`，也不给 `operator<<`**（步骤 1 落地时定的，见偏差 D2）。`==` 会诱导"浮点精确比较"这种几乎总是错的写法；少了它，测试里也没法误用 `MINI_CHECK_EQ` 拿两个向量硬比 —— **逼所有人写"多近算相等"**。`<<` 目前唯一的需求方是测试的失败消息，而逐分量断言本来就够查；真需要时再加（那时它有明确的使用者） |

---

## 9. 坑表

| # | 坑 | 怎么避 |
| --- | --- | --- |
| 1 | `Normalized()` 用 `len2 <= 0` 挡不住 **NaN**（NaN 与任何值比较都是 false） | 写成 `!(len2 > 0.0f)`。见 7.1 的对照表 |
| 2 | `std::sqrt` **在 C++20 不是 constexpr** | 决策 2：不标。别试图用 `constexpr` 修饰 `Length`/`Normalized` |
| 3 | `std::floor` / `std::round` / `std::abs(float)` **也不是 constexpr** | 手算 floor（2 行）；`ApproxEquals` 里避开 `abs`，写成 `d < eps && d > -eps` |
| 4 | 用 `std::abs` 写 `ApproxEquals` 会**连带失去 constexpr** | 同上。副作用是好的：NaN 自动返回 false（`NaN < eps` 为假） |
| 5 | `AABB::Intersects` 用 `<=` → **紧贴墙壁的实体每帧都被判碰撞并被推开一格**，永远贴不住墙 | 用严格 `<`。这是 S7 最容易踩的坑 |
| 6 | `Contains`（闭）与 `Intersects`（开）语义**故意不一致** | 两处都要写注释说明"为什么不一致"；各有一个测试钉住。不是一个漏改，是一个决定 |
| 7 | `Perp()` 方向搞反 → S7 的法线全部朝内，碰撞把实体往里推 | 写死**逆时针** `(x,y) → (-y,x)`，并测试 `(1,0) → (0,1)` |
| 8 | `Ray` 的 `d == 0` 靠 IEEE `1/0 = inf` → 边界上出 **`0 * inf = NaN`** | 显式分支（7.3）。不要赌 IEEE 收敛 |
| 9 | `std::max(NaN, x)` 的结果**实现定义** | 所以第 8 条尤其重要：**别让 NaN 参与 min/max**。math 里凡是"防御 NaN"的地方都要用比较式而不是 `min`/`max` |
| 10 | `AABB::FromCorners` 不排序 → 造出反盒（`min > max`），后面所有判定静默失效 | 逐分量取 min/max（自动排序）；`IsValid()` 留给断言和测试 |
| 11 | `LengthSq()` 溢出/下溢 | **不做防御**，但注释写明适用范围（坐标量级 < 1e15 安全，本项目 < 1e4）。溢出会变 `inf`，而 `Normalized` 那条 `!(len2>0)` 恰好接得住 |
| 12 | `Normalized()` 返回 `{0,0}` 会**静默掩盖**"输入本来就不该是零向量"这种 bug | 这是**有意的取舍**（宁可零向量也不要 NaN 污染物理循环）。注释里要写明这层含义，让调用方知道"拿到零向量可能是错误输入" |
| 13 | `Ray` 起点在盒内返回 **0** 而不是负 t | 调用方拿 t 去推进位置时不会倒退。见 7.3 最后一行 |
| 14 | 擦角（`tNear == tFar`）算不算命中没定 | 用 `>` 判定分离 → **算命中**；写一个用例钉住 |
| 15 | **header-only 的头文件只有被 include 才会被编译** —— 没被 include 时编译器根本不看它 | `test_math.cpp` 必须 include **全部四个**头（哪怕某一步还用不到 AABB，也要 include，否则那个头是"没编译过"的） |
| 16 | 以为 `engine/CMakeLists.txt` 要加源文件 | math 是 header-only，**一个源文件都不加**；只改 `tests/CMakeLists.txt`。但要给 `engine/CMakeLists.txt` 的头注释加一行说明，免得后人以为漏了 S4 |
| 17 | `Vec2 / 0.0f` | **不检查**，返回 `inf`/`NaN`；注释写明"调用方负责"。检查会让热路径变慢，而且本项目也没有除零的合理场景 |
| 18 | `sizeof(Vec2) == 8` 被将来加的成员或 padding 破坏 | 头文件里放 `static_assert` —— 加它就是为了**尽早**发现 |
| 19 | `FloorToInt` 里 `(int)v` 在 `\|v\| > 2^31` 时是 **UB**（不是"截断"） | 注释写明输入范围约束（`.tmx` 地图/坐标不会到那里） |
| 20 | `RoundToInt` 写成 `FloorToInt(v + 0.5f)` → 负半轴与 `std::round` 不一致 | 用 7.2 的对称写法（负数取绝对值再取负） |
| 21 | `-0.0f` 的处理 | `Sign(-0.0f)` 必须是 **0**（不是 -1）；`ApproxEquals(0.0f, -0.0f)` 是 **true**。两个都要有用例 |
| 22 | 在头文件里 `using namespace std;` 或放全局变量 | 头文件被你 include 到任何地方，污染所有人。全部包在 `mini::math` 里 |
| 23 | `math` 反向依赖上层（比如为了用 `render::Color` 而 include `render/`） | 第 3 节写死的规矩：`math` 只依赖标准库。这是"按变化频率分层"的底线 |
| 24 | 加了 `Vec3.h` 半成品但没人 include，于是"看起来做完了" | 决策 1：契约写文档（6.6），**不建文件** |
| 25 | **`/W4` 的 C4756（常量算术溢出）可能被用例 15 的 `1e30f` 触发** —— 编译器可能把 `1e30f × 1e30f` 折叠成 `inf` 然后告警 | ⚠️ **这条是预测，不是已确认**。步骤 1 构建时若真报 C4756，**不是代码错了**：把输入改成运行时值即可（**先别动语义** —— 用例要的就是"LengthSq 溢出成 inf"这一个输入） |
| 26 | 忘记 `math` 是 header-only → 以为"`engine/CMakeLists.txt` 的列表里没有 S4"是漏了 | 那是**故意的**，该文件头注释已写明（坑 #16）。真正要改的只有 `tests/CMakeLists.txt` 那一行 |
| 27 | ⭐ **宏参数里的"顶层逗号"会被当成参数分隔符** —— **花括号不保护逗号，只有圆括号保护**。现象是 C4002「参数过多」**外加一串完全看不懂的下游语法错误**（`应为 }`/`miniOk 必须初始化`/`ReportFailure 重定义`…），很容易让人去查语法而不是查宏 | ✅ **已从根上修掉（2026-09-29，D3）**：`MINI_CHECK` 改成可变参数 `(...)`，**任何表达式都能直接传**，不需要括号。<br>⚠️ 但 `MINI_CHECK_EQ` / `MINI_CHECK_NEAR` 要**区分参数**，可变参数拆不开，所以它们仍要求：<br>❌ `MINI_CHECK_EQ(AABB{1,2,3,4}.min.x, 1.0f)` → 被拆成 4 个参数<br>✅ `MINI_CHECK_EQ((AABB{1,2,3,4}.min.x), 1.0f)` ← 自己包的圆括号**必需**<br>✅ `MINI_CHECK_NEAR(v.Dot(Vec2{0, 1}), 0.0f, 1e-5f)` ← 花括号已在函数调用的括号里（第 2 层），**安全**<br>判据：不是"有没有花括号"，是"**花括号有没有出现在某个参数的最外层**"。规格已写进 `MINITEST_SPEC` 第 1 节 |

---

## 10. 验收

> **本阶段没有"跑 exe 看现象"**（决策 3）。可观察结果 = **4 个 suite 全绿**的输出 + 退出码。
> ⚠️ **没有 `mini_tests Math` 这条命令**（suite 过滤是**精确匹配**，见偏差 D4）—— 想一次跑完 S4 就**不带参数**。
> 所有反向验证都要**看到红**，且**必须还原现场**。

### 10.1 用例清单（33 个，**4 个 suite**：`MathUtils` / `Vec2` / `AABB` / `Ray`）

> **按"被测单元"分 suite**，沿用 S2 的先例（`KeyCode` 也在 `test_input_state.cpp` 里单独成一个 suite）。
> 代价：没有一条命令能"只跑 S4 的全部"—— runner 的过滤是**精确匹配**，
> 所以要么用 4 条命令，要么不带参数跑全部 68 个。见偏差 **D4**。

**MathUtils（8）**

| # | 用例 | 钉住什么 |
| --- | --- | --- |
| 1 | `MathUtils.ClampBasic` | 区间内原样、下溢夹到 lo、上溢夹到 hi |
| 2 | `MathUtils.LerpEndpointsAndExtrapolation` | `t=0 → a`、`t=1 → b`、`t=2` **外推**（证明"不钳 t"是刻意的） |
| 3 | `MathUtils.DegToRadKnownValues` | `0→0`、`180→kPi`、`-90→-kPi/2` |
| 4 | `MathUtils.SignThreeWay` | 正/负/零 → +1/-1/0；**`-0.0f` 也是 0**（坑 #21） |
| 5 | `MathUtils.ApproxEqualsEps` | 差 < eps 为真、差 > eps 为假、默认 eps 可用 |
| 6 | `MathUtils.ApproxEqualsNanIsFalse` | `NaN vs NaN`、`NaN vs 0` **都是 false**（正确行为，坑 #4） |
| 7 | `MathUtils.FloorToIntNegative` | `3.7→3`、`-0.5→-1`、`-3.0→-3`、`-3.7→-4`。**注释里对照 `(int)` 截断会得到 3/0/-3/-3**（坑 #19、E-8） |
| 8 | `MathUtils.RoundToIntHalfAwayFromZero` | `+0.5→1`、**`-0.5→-1`**、`2.4→2`、`-2.6→-3`（坑 #20） |

**Vec2（9）**

| # | 用例 | 钉住什么 |
| --- | --- | --- |
| 9 | `Vec2.DefaultIsZero` | 默认构造是 `{0,0}`（不是未初始化） |
| 10 | `Vec2.Arithmetic` | `+ - 一元- *`（标量左右乘）`/`；`+= -= *= /=` 都返回自身引用 |
| 11 | `Vec2.LengthSqAndLength` | `{3,4}` → `LengthSq()==25`、`Length()==5` |
| 12 | `Vec2.NormalizedIsUnit` | 结果长度 ≈ 1（用 `ApproxEquals`），方向不变 |
| 13 | `Vec2.NormalizedZeroReturnsZero` | `{0,0}` → `{0,0}` ← **反向验证 1 的目标之一** |
| 14 | `Vec2.NormalizedNanReturnsZero` | `{NaN,1}` → `{0,0}` ← **反向验证 1 的另一个目标**（这条才是关键） |
| 15 | `Vec2.NormalizedHugeReturnsZero` | `{1e30,0}`（`LengthSq` 溢出成 `inf`）→ `{0,0}`（坑 #11） |
| 16 | `Vec2.DotAndPerp` | 正交向量 `Dot==0`；**`Perp()`：`(1,0)→(0,1)`、`(0,1)→(-1,0)`**（逆时针，坑 #7） |
| 17 | `Vec2.ApproxEqualsAndConstexpr` | `Vec2::ApproxEquals` 的行为；**`static_assert(Vec2{3,4}.LengthSq() == 25.0f)`** 证明 constexpr 真的生效 |

**AABB（7）**

| # | 用例 | 钉住什么 |
| --- | --- | --- |
| 18 | `AABB.FromCenterSizeAndAccessors` | `Center()`/`Size()` 与 `FromCenterSize` 往返一致 |
| 19 | `AABB.FromCornersSortsAutomatically` | 传 `(max, min)` 反序也得到正确的 `min`/`max`（坑 #10） |
| 20 | `AABB.ContainsIncludesEdges` | 四条边上的点、四个角**都算在内**；外面一点不算 |
| 21 | `AABB.TouchingIsNotIntersecting` | 两盒共用一条边 → **false** ← **反向验证 2 的目标**（坑 #5） |
| 22 | `AABB.OverlapAndContainmentIntersects` | 部分重叠 → true；一盒完全含另一盒 → true；**盒与自己** → true |
| 23 | `AABB.ValidityAndDegenerate` | `min > max` → `IsValid()` **false**（单轴反了也算无效）；<br>⚠️ **故意不测反盒的 `Intersects`** —— 那上面它没有意义（公式照样给 true/false，但不对应任何几何事实）。**退化的点盒/线盒**（`min==max`）→ `IsValid` **true** 但 `Intersects` **false**（开区间）| 
| 24 | `AABB.DegeneratePointBoxSemantics` | 退化点盒的**三种**行为一次钉完：<br>① vs 它自己 → `Intersects` **false**（`1<1` 不成立）<br>② 在另一个盒**内部** → `Intersects` **true**（`1<2 && 0<1` 成立，虽然重叠长度为 0）<br>③ 在另一个盒**边界**上 → **false**（与"贴边不算相交"同源）<br>再加一条 `Contains` 与 `Intersects` **在同一判断里答案不同**（查询 vs 碰撞） |

**Ray（9）**

| # | 用例 | 钉住什么 |
| --- | --- | --- |
| 25 | `Ray.HitsFromFront` | 正前方命中，t 等于手算值 |
| 26 | `Ray.MissesWhenBehind` | 盒子整个在射线背后 → `nullopt` |
| 27 | `Ray.OriginInsideReturnsZero` | **起点在盒内 → t = 0**（PLAN S4 验收明文要求） |
| 28 | `Ray.OriginOnBoundaryReturnsZero` | 起点恰在盒边界上 → t = 0 |
| 29 | `Ray.ParallelOutsideReturnsNullopt` | 平行且在外侧 → `nullopt` ← **反向验证 3 的目标**（坑 #8） |
| 30 | `Ray.ParallelInsideHits` | 平行但在 slab 内（沿轴穿盒）→ 命中且 t 正确 |
| 31 | `Ray.ZeroDirectionDegeneratesToPointTest` | `dir == {0,0}`：起点在盒内 → 0；在外 → `nullopt`（退化行为写进注释） |
| 32 | `Ray.DiagonalHitDistanceMatchesManualValue` | 斜向命中，t 与手算值 `ApproxEquals` |
| 33 | `Ray.GrazesCorner` | 恰好擦角（`tNear == tFar`）→ **算命中**（坑 #14） |

> 用例名一律 `套件.用例名` 形式，与现有 5 个 suite 保持一致（`MINI_TEST(MathUtils, ClampBasic)` 这样写）。

### 10.2 正向验收

```powershell
cmake --build build --config Debug                      # 期望 0 warning
.\build\bin\Debug\mini_tests.exe MathUtils              # 期望  8 passed
.\build\bin\Debug\mini_tests.exe Vec2                   # 期望  9 passed
.\build\bin\Debug\mini_tests.exe AABB                   # 期望  7 passed
.\build\bin\Debug\mini_tests.exe Ray                    # 期望  9 passed
                                                        #  ↑ 4 个加起来 = 33
.\build\bin\Debug\mini_tests.exe                        # 期望 35 → 68 passed（不带参数 = 全部）
ctest --test-dir build -C Debug --output-on-failure     # 期望 1/1
ctest --preset asan                                     # 期望同样全绿（ASan 这条 S3 已打通）
cmake --build build --config Release                    # 期望同样 0 warning（S3 已验过这条路）
```

**外加两条"没坏"的确认**：

| 检查 | 期望 |
| --- | --- |
| `.\build\bin\Debug\MiniEngine.exe` | 行为与 S3 **逐字符一致**（S4 不该动任何运行期行为） |
| `mini_tests.exe Nope` | 仍打印 `no case in suite 'Nope' (68 case(s) registered)` 且退出码非 0（门闩 A1 保持有效） |

### 10.3 反向验证（3 条，**每条都必须看到红**）

> 由本人执行（recipe 如下）；也可以叫我改、你跑、我再还原。
> **做完必须还原现场并再跑一次确认全绿。**

| # | 破坏方式 | 必须看到 | 为什么要这条 |
| --- | --- | --- | --- |
| 1 | `Vec2::Normalized()` 里 `!(len2 > 0.0f)` 改成 `len2 <= 0.0f` | **只有 `NormalizedNanReturnsZero` 变红**，`NormalizedZeroReturnsZero` **不变红** | 如果两条都红或都不红，说明这两条用例**测的是同一件事**，没钉住 NaN 这个区别 —— 那 `!` 这个写法就没被验证 |
| 2 | `AABB::Intersects` 的 `<` 全改成 `<=` | `TouchingIsNotIntersecting` 变红 | 证明"贴边不算相交"这条语义真的被测到了 |
| 3 | 删掉 `Ray::IntersectsAABB` 里的 `d == 0` 分支，改成靠 IEEE 的 `1/d` | `ParallelOutsideReturnsNullopt` 变红 | **如果它没红**，说明那个用例是摆设（IEEE 恰好也对），要**重写成边界上 `0*inf` 那种输入**，否则这条守卫等于不存在 |

**第 3 条的风险最高**：教科书式的 `1/d` 在"平行且在外"时**恰好也返回 nullopt**，
所以那个用例很可能本来就不会红。这正是 B1 那次学到的教训 ——
**"加了一条防回归的测试"必须连"它真的会红吗"一起验**（见 `TECH_DEBT.md` 第 6 节）。

### 10.4 交付后的自检（收尾用）

- [ ] 四个头文件**都被 `test_math.cpp` include**（否则没编译过，坑 #15）
- [ ] 依赖规矩 1/2 自检（这台机器没有 grep，用 PowerShell）：
      ```powershell
      Select-String -Path engine\include\mini\math\*.h -Pattern 'windows\.h|#ifdef'   # 期望无输出
      Select-String -Path engine\include\mini\math\*.h -Pattern 'include <mini/'      # 期望无输出
      ```
- [ ] 所有公开接口**都有一行注释**回答"为什么存在 / 谁会用它"（决策 6）
- [ ] 新增接口逐个过一遍：**有生产调用者的**（`AABB` 全部会被 S5 用）/
      **暂时只有测试调用的**（`FloorToInt` `RoundToInt` `Ray::IntersectsAABB`）——
      后者必须在注释里写明用途与触发条件（就是 `TECH_DEBT.md` B5 那条纪律）
- [ ] `static_assert(sizeof(Vec2) == 8)` 在头文件里（坑 #18）
- [ ] 反向验证 3 条**都跑过且都看到红**，现场已还原
- [ ] `doc/MiniEngine_PLAN.md` 的 S4 小节标 ✅、A.2 目录树改对、差异表写完
- [ ] `doc/TECH_DEBT.md` 再看一眼有没有新债
- [ ] `git status` 干净

---

## 11. commit 切分（每个都能构建）

| # | 信息 | 内容 | 跑完应该看到 |
| --- | --- | --- | --- |
| 1 | `feat: 加入 math/MathUtils.h 与 Vec2.h` | `MathUtils.h` + `Vec2.h` + `test_math.cpp`（17 个用例）+ `tests/CMakeLists.txt` 加一行 + `engine/CMakeLists.txt` 头注释加一行 | 0 warning、用例 35 → 52、exe 行为与 S3 一致 |
| 2 | `feat: 加入 math/AABB.h（贴边不算相交）` | `AABB.h` + 7 个用例 | 用例 52 → 59、exe 行为不变 |
| 3 | `feat: 加入 math/Ray.h（slab 法，显式处理平行）` | `Ray.h` + 9 个用例 | 用例 59 → 68、exe 行为不变 |
| 4 | `docs: S4 落地记录与 PLAN 同步` | PLAN 的 S4 小节 + A.2 目录树 + 差异表 + 本文件的落地记录 | — |

**"每个 commit 都能构建"比切得漂亮重要** —— 构建不过就合并到下一个。

**S4 的三个 commit 都是"只加代码、不改任何既有行为"**，所以判据很干净：
**`MiniEngine.exe` 的现象必须逐字符不变；变了就是改错了。**

---

## 12. 与 PLAN 原文的差异（必须同步）

| # | 位置 | 原文 | 改成 | 理由 |
| --- | --- | --- | --- | --- |
| 1 | PLAN S4 目标 | "`Vec2/Vec3/Rect/AABB/Ray` 全 **`constexpr`** 头文件" | `Vec2`/`AABB`/`Ray` 的**纯算术部分**是 `constexpr`；`Length`/`Normalized` **不是** | `std::sqrt` 在 C++20 不是 constexpr。**证据**：本机 MSVC 14.44 `<cmath>` 第 264 行 `inline float sqrt(_In_ float)`，`_CONSTEXPR23` 只在 `__cplusplus > 202002L` 下展开。C++23 后可无痛改回 |
| 2 | PLAN S4 目标 / A.2 目录树 | `math/` 下 6 个文件：`Vec2.h Vec3.h Rect.h AABB.h Ray.h MathUtils.h` | **4 个文件**：`MathUtils.h Vec2.h AABB.h Ray.h` | 决策 1：`Vec3`/`Rect` 只预留契约（6.6），不建半成品文件 |
| 3 | PLAN S4 可运行效果 | "可选 demo 打印'两个盒子相交/不相交'" | **不做 demo**，改为"4 个 suite 全绿、退出码 0" | 决策 3；避免把演示塞进 `main.cpp` 而后在 S5 挪走 |
| 4 | PLAN S4 C++ 知识 | "`constexpr`、运算符重载……" | 补一句：**"`constexpr` 的边界 —— 哪些标准库函数不是 constexpr（`sqrt`/`floor`/`abs(float)`），以及代价原则"** | 这是本阶段真实学到的东西（第 8 节原则 5） |
| 5 | PLAN E-8 | "……写一个坐标转换的单元测试"，阶段标 **S4/S6** | S4 那半落地为 `FloorToInt`/`RoundToInt` + 两个用例；world→screen 的全流程仍留 S6 | 决策 4 |
| 6 | PLAN S4 规模 | "~300 行 + ~200 行测试" | **~280 行 + ~250 行**（33 个用例） | 测试比估计多一点（边界用例列得细） |
| 7 | PLAN A.2 目录树 | `tests/` 下 `test_math.cpp` | **一致，不用改** | — |
| 8 | `doc/MINITEST_SPEC.md` 第 1 节 | `MINI_CHECK(expr)` 单参数 | 改成 **`MINI_CHECK(...)` 可变参数** + 补一段"为什么" | 这次是**动了框架**而不是绕过它（见偏差 D3），规格必须同步 |

**同步时机**：commit 4（收尾）。S4 结束时应确认上表 7 条**全部**落到 PLAN。

---

## 13. 本阶段的"学习点"备忘（给自己看的）

| 主题 | 这一阶段真正学到的 |
| --- | --- |
| `constexpr` | **不是"能标就标"**：先查标准库支持到哪（`sqrt`/`floor` C++20 都没有），再算"手写替代品的代价"，最后决定保不保。两个相反的选择（放弃 sqrt / 保住 floor）**判据是同一条** |
| 浮点 | **NaN 的比较恒为假**这个性质能拿来当守卫（`!(x > 0)` 一行挡三种输入）；反过来，它也让"用 `<=` 挡 NaN"这种写法**静默失效** |
| 几何 | **闭区间 vs 开区间是有语义的**，不是一个笔误：`Contains` 要含边（点在里面），`Intersects` 不能含边（否则贴墙被推开）。同一份代码里两种语义并存，**必须写在注释里** |
| 射线求交 | slab 法的**优雅之处正是它的陷阱**：靠 IEEE 的 `inf` 收敛在 99% 情况下对，剩下 1% 是 `0 * inf = NaN`。**"能跑对"和"可预测"是两件事** |
| 接口设计 | 返回 `std::optional<float>` 而不是 `bool` + 出参：**把命中信息放进返回值**，调用点就没有歧义 |
| 工程习惯 | 反向验证的三条要分别设计成"**只有它能红**"（第 1 条尤其：如果两条用例一起红，说明它们没区分开，守卫是假的） |
