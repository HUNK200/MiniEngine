# S2 实现清单与验收

> **用法**：在"进度板"里勾进度。第 6 节的接口设计是**施工图**，第 8 节的坑表是**避雷地图**，第 9 节是**验收证据**。
> 本文件只描述**要什么、怎么验证**，不含可实现代码（`AGENTS.md` 约定：AI 默认只出方案；定义与伪代码是方案的一部分，`.cpp` 的函数体不是）。
>
> 相关文件：`doc/MiniEngine_PLAN.md`（阶段定义、A.3 依赖规则、C.3 类关系草案）、`doc/S0_CHECKLIST.md`（CMake / CTest 与 MiniTest 规格）、`tests/MiniTest.h`（断言宏）。
>
> **环境提醒**：默认 shell 是 **PowerShell**。退出码用 `$LASTEXITCODE`（不是 `echo $?`），路径用反斜杠。

---

## 当前状态（2026-09-28：**S2 验收完成**）

- **实现完成**：7 个 commit（`2c15f8b` 文档 → `14b60d0` Console → `a3a9b09` Input → `f4ea7a8` 测试 → `feaa06c` 演示 → `2a6e578`/`118bb57` 文档与措辞）。
- **自动验收全过**：构建 **0 warning**、`ctest` **1/1**、`mini_tests` **9 → 16 passed**、`> out.txt` 抛异常 + exit 1。
- **交互验收已过**：9.1 的 8 条现象在真机 Windows Terminal 上确认通过（2026-09-28）。
- **遗留一条已知问题**（不在 S2 范围）：关控制台窗口可能留下锁住 exe 的孤儿进程 —— 见 8.5，建议并进 S3 修。
- 接口决策与实现偏差见第 7 节与进度板下方的"实现偏差"表（D1~D6）。

> S2 的实际规模：净新增约 **450 行**（含 HUD 与演示），另有 **165 行搬迁**（S1 的 `render/TerminalPresenter.cpp` 归位到 `platform/`）。
> 比 PLAN 原文估的 ~250 行多出不少，主要花在"可离线单测的状态机"、"抽象基类 + 工厂"、"三行 HUD + `@` 演示"这三处。

---

## 进度板

| # | 任务 | 对应 commit | 状态 |
| --- | --- | --- | --- |
| **S2-1** | `platform::Console` 以 RAII 接管终端模式，`render/TerminalPresenter.*` 删除 | `14b60d0` | ✅ |
| **S2-2** | `platform/KeyCode.h`：`Key` + `KeyState` + `KeyFromVirtualKey()` + `KeyIndex()` | `a3a9b09` | ✅ |
| **S2-3** | `platform/InputState.h`：四态状态机（header-only，可离线单测） | `a3a9b09` | ✅ |
| **S2-4** | `platform/Input.h` + `src/platform/InputWindows.cpp`：Win32 事件 → `InputState` | `a3a9b09` | ✅ |
| **S2-5** | `tests/test_input_state.cpp`：7 个用例 + 注册进 `tests/CMakeLists.txt` | `f4ea7a8` | ✅（实际 16 passed） |
| **S2-6** | `main.cpp` 改为按键四态 HUD + `@` 瞬时移动 + Q 退出 | `feaa06c` | ✅ |
| **S2-7** | 正向验收（9.1 节）8 条现象 | — | ✅ **2026-09-28 真机确认通过** |
| **S2-8** | 反向验证（9.2 节）四条实跑并**还原现场** | — | 第 3、4 条已实跑；第 1、2 条未单独实跑（随 S2 一起确认完成） |
| **S2-9** | 文档同步（第 11 节差异 + 本节落地记录） | commit 5 | ✅ |

- [x] S2-1 → [x] S2-2 → [x] S2-3 → [x] S2-4 → [x] S2-5 → [x] S2-6 → [x] S2-7 → [x] S2-8 → [x] S2-9

> **S2 于 2026-09-28 验收完成**，可以进 S3。
> 遗留一条**已知问题**没修（不在 S2 范围内）：关控制台窗口可能留下锁住 exe 的孤儿进程，见 8.5。

### 实现偏差（写代码时偏离本清单的地方，逐条记下来）

| # | 清单原计划 | 实际做法 | 为什么改 |
| --- | --- | --- | --- |
| D1 | 内部头 `src/platform/QuitFlag.h` 放 `detail::QuitRequested()` | 公共头 `Console.h` 里的自由函数 `platform::QuitRequested()` | 步骤 1 时 `main` 就要读它（那时 `Input` 还没写），而 `main` include 不到 `engine/src/`。做成公共自由函数反而**少一个文件**，且头里没有 Windows 依赖 |
| D2 | `KeyState` 定义在 `InputState.h` | 挪到 `KeyCode.h`（和 `Key` 放一起） | `Input.h` 需要 `KeyState`，但抽象接口用不到状态机。放进"按键词汇表"更合身，`Input.h` 也就不用 include `InputState.h` |
| D3 | `KeyFromVirtualKey(std::uint8_t)` | `KeyFromVirtualKey(std::uint16_t)` | `wVirtualKeyCode` 是 WORD。截成 8 位会把 `0x0100` 这样的码**悄悄折到 `0x00`**，等于凭空多出一个键 |
| D4 | 未映射 VK 在 `Poll()` 里 `if (key == Key::Unknown) break;` | 收口到 `InputState::OnKeyDown`（`KeyIndex` 返回哨兵即拒写） | 单一收口 + **可离线单测**（`KeyCode.InvalidKeyIndexIsIgnored`）。写在 `Poll()` 里就测不到了 |
| D5 | 演示只显示按键状态 | 三行 HUD（15 个键）+ `@` 每次按下走一格 | PLAN S2 的目标写的是"`@` 能响应按键移动（先用瞬时移动验证输入通路）"，只显示 HUD 不满足这条 |
| D6 | `~ConsoleWindows` 只还原终端 | 额外 `SetConsoleCtrlHandler(CtrlHandler, FALSE)` 卸掉 handler | 终端都还回去了，再让 handler 往正常屏幕写转义序列没有意义 |

### 一处认知修正（比原计划更值钱）

写测试时以为 `m_pulse` 只救"一帧内 down+up 的极速点击"。跑反向验证（删掉那个分支）才发现**正常按下也走这一支**：
`prev=false, now=true` 时 `was && now` 不成立，于是落到 `if (m_pulse)` 才返回 `Pressed`。
换句话说 **`m_pulse` 是 `Pressed` 的必要条件，不是边角补丁** —— 删掉它 `Pressed` 整个消失，红的用例比预期多一个。
这条已经写进 6.3 的判定表和 PLAN 的 S2 坑表。

---

## 1. S2 目标与不做什么

**目标（一句话）**：`@` 能响应按键，屏幕上实时显示每个键的 `Pressed / Held / Released`，按 Q 退出且终端干净还原。

**不做什么**（出现它们就是范围蔓延）：

| 不做 | 该归谁 | 理由 |
| --- | --- | --- |
| `Application` / `IGame` / `GameLoop` / `Time` / FPS | **S3**（PLAN C.3 / C.4 已定稿） | 现在只有一个实体，建出来就是空壳 |
| `@` 在格子间移动、任何游戏逻辑 | **S5** | S2 只验证"输入通路通不通" |
| `FrameBuffer::Resize` + 窗口变化重排画面 | **S3 / S6** | S2 只做到"收到事件不崩、不误当按键" |
| 鼠标事件、修饰键（Shift / Ctrl）状态、按键文本输入 | 永远不做 | 终端游戏用不上 |
| Linux / macOS 分支、`#ifdef __linux__` | 永远不做 | `AGENTS.md` 硬约束：只做 Windows |
| 顶层 CMake 改动 | — | `NOMINMAX` / `WIN32_LEAN_AND_MEAN` / `/W4 /utf-8` 在 S0 就已进 `mini_options`，S2 直接用（已核对 `CMakeLists.txt:31-38`） |

**时间盒**：约 2~3 小时（PLAN 原文估 1.5h，按 ~300 行新代码 + 165 行搬迁 + 7 个新用例重估）。

---

## 2. 交付物清单

### 2.1 新增

| 文件 | 职责 | 不要放什么 |
| --- | --- | --- |
| `engine/include/mini/platform/KeyCode.h` | `enum class Key` + `enum class KeyState` + VK 映射 + 下标换算。header-only、零依赖 | `<windows.h>`（只能用裸数字 + 注释） |
| `engine/include/mini/platform/InputState.h` | 四态状态机。header-only、零依赖、**可离线单测** | 任何 Win32 调用、任何 IO |
| `engine/include/mini/platform/Input.h` | `class Input` 抽象基类 + `MakeInput()` 工厂 | HANDLE、`<windows.h>` |
| `engine/include/mini/platform/Console.h` | `class Console` 抽象基类 + `ConsoleSize` + `MakeConsole()` 工厂 + 自由函数 `QuitRequested()`（见 D1） | HANDLE、`<windows.h>` |
| `engine/src/platform/ConsoleWindows.cpp` | 终端模式（in + out）、`Present()`、`Size()`、Ctrl+C handler、`platform::QuitRequested()` 的定义 | 任何游戏概念 |
| `engine/src/platform/InputWindows.cpp` | `ReadConsoleInputW` 事件 → `InputState`；`MakeInput()` | 改终端模式（那是 Console 的事） |
| `tests/test_input_state.cpp` | 7 个离线用例（suite 是 `InputState` 5 个 + `KeyCode` 2 个） | 任何需要终端的用例 |

> **原来的 `src/platform/QuitFlag.h` 没有建**（实现偏差 D1）：进程级退出标志做成了公共头 `Console.h` 里的自由函数
> `platform::QuitRequested()`。`main` 在步骤 1 就要读它，而 `main` include 不到 `engine/src/`。

### 2.2 删除

| 文件 | 原因 |
| --- | --- |
| `engine/include/mini/render/TerminalPresenter.h` | 职责拆成 `platform::Console`（模式+推帧）与 `platform::Input`（Ctrl+C 标志）。S1 头里那个 ⚠️ TODO 到此兑现 |
| `engine/src/render/TerminalPresenter.cpp` | 主体搬进 `ConsoleWindows.cpp`；`render::QuitRequested()` 作废，改由 `Input::QuitRequested()` 提供 |

### 2.3 修改

| 文件 | 改什么 |
| --- | --- |
| `engine/CMakeLists.txt` | 源文件列表：删 `render/TerminalPresenter.cpp`，加 `platform/ConsoleWindows.cpp`、`platform/InputWindows.cpp`（**手写，禁用 `file(GLOB)`**，S0 坑 #8） |
| `main.cpp` | 改用 `MakeConsole()` / `MakeInput()`；删掉 `try/catch(...) { EndTerminal(); throw; }` 那段补丁（RAII 接管） |
| `tests/CMakeLists.txt` | 源文件列表加 `test_input_state.cpp` |
| `doc/MiniEngine_PLAN.md` | 第 11 节的 6 处（3 处措辞修正 + 状态行 + A.2 目录树 + E-12） |
| `AGENTS.md` | 第 2 节的文档表加一行 `doc/S2_CHECKLIST.md` |

> **不需要改的**：`engine/include/mini/render/FrameBuffer.h`（`Console::Present` 只用它的公开接口）、顶层 `CMakeLists.txt`、`CMakePresets.json`。

---

## 3. 依赖规则（本阶段唯一新增的边界）

```
main ──► platform ──► render ──► stdlib
           │
           └──► stdlib
```

| 规矩 | 说明 |
| --- | --- |
| `platform` 公共头里**一个 `<windows.h>` / `#ifdef` 都不出现** | A.3 硬规矩。HANDLE 一律锁在 `.cpp` 里 |
| `platform` 只 include `mini/render/FrameBuffer.h` | 因为 `Present()` 要收它。不认识 Cell 之外的渲染概念，更不认识 game |
| `render` **不反向依赖** platform | S1 那条"阶段 1 临时例外"到此终结，A.3 的例外条款删除（第 11 节差异 #5） |
| `InputState.h` / `KeyCode.h` 零依赖 | 只 include `<array>` / `<cstdint>` / `<cstddef>`，所以能离线单测 |
| `tests/` 只链 `MiniEngine::engine` 与 `mini_options` | 不变。`InputState.h` 是 header-only，测试侧不需要新库 |

---

## 4. 数据结构与所有权

| 对象 | 持有者 | 生命周期与顺序 |
| --- | --- | --- |
| `std::unique_ptr<Console>` | `main`（S3 起是 `Application`） | 构造**先**于 Input（改终端模式）；析构**后**于 Input → 模式最后才恢复 |
| `std::unique_ptr<Input>` | 同上 | 不持有 Console、不引用它，两者只共享"输入模式已设好"这个前提 |
| Ctrl+C 标志 | `ConsoleWindows.cpp` 文件级 `volatile LONG` + `Interlocked*` | 进程级（`SetConsoleCtrlHandler` 没有 per-object 上下文，这个全局躲不掉）。能做的只是把它关进内部头 |
| `InputState` | `InputWindows` 的成员 | 值语义，无指针 |
| 每个键的状态 | `InputState` 里三个 `std::array<bool, Key::Count>` | 合计 3 × ~20 = **60 字节**，无动态分配 |
| 事件读取 | `Poll()` 内的栈上 `INPUT_RECORD` | 零分配，热路径无堆操作 |

**为什么用 `unique_ptr` 而不是值成员**：抽象基类不可值持有；且 `main`（S2 演示）与 `Application`（S3）两种组装方式能共用同一个工厂。

---

## 5. 实施步骤（每步都立刻跑一次，看到明确现象）

### 步骤 S2-1（commit 1）：Console 归位

1. 新建 `engine/include/mini/platform/Console.h`（只有抽象基类 + `ConsoleSize` + 工厂声明）
2. 把 `render/TerminalPresenter.cpp` 的**全部内容**搬进 `engine/src/platform/ConsoleWindows.cpp`：
   - `BeginTerminal()` / `EndTerminal()` → 构造函数 / 析构函数
   - `Present(const FrameBuffer&)` → 虚函数实现
   - `render::QuitRequested()` → `platform::QuitRequested()`（**实际做法**：公共头 `Console.h` 里的自由函数，定义在此。
     原计划是内部头 `src/platform/QuitFlag.h`，没建 —— 见实现偏差 D1）
   - 新增 `Size()`（`GetConsoleScreenBufferInfo` 取 **`srWindow` 的宽高**，不是缓冲区尺寸）
   - 新增**输入侧**模式设置（`~ENABLE_QUICK_EDIT_MODE` + `ENABLE_EXTENDED_FLAGS` + `ENABLE_WINDOW_INPUT`）
3. 删掉 `render/TerminalPresenter.h` / `.cpp`
4. `main.cpp` 最小改动：`BeginTerminal/EndTerminal` 换成 `MakeConsole()` 的 RAII 作用域，删掉 `catch(...)` 补丁。**演示内容（边框 + `@` + `#`）保持不变**
5. `engine/CMakeLists.txt` 同步源文件列表

**立刻跑**

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
.\build\bin\Debug\MiniEngine.exe
```

**应看到**：0 warning、`ctest` 仍 1/1、`mini_tests` 仍 **9 passed**（没加用例）、exe 画面与 S1 完全一致。**这一步是纯搬迁，画面有任何变化就说明搬错了。**

### 步骤 S2-2 ~ S2-4（commit 2）：输入状态机与 Input

1. `KeyCode.h`：`enum class Key : std::uint8_t { Unknown = 0, ..., Count }` + `KeyFromVirtualKey()` + `KeyIndex()`
2. `InputState.h`：三个 bool 数组 + `OnKeyDown/OnKeyUp/OnFocusLost/BeginFrame/Get/IsDown`
3. `Input.h`：`enum class KeyState` + 抽象基类 + `MakeInput()`
4. `InputWindows.cpp`：`Poll()` / `BeginFrame()` / `Get()` / `IsDown()` / `QuitRequested()`

**立刻跑**

```powershell
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

**应看到**：0 warning、`ctest` 仍 1/1（用例数**还没涨**，因为测试在 commit 3）。这一步唯一的产出是"能编译且不破坏既有行为"。

### 步骤 S2-5（commit 3）：补测试

写 `tests/test_input_state.cpp` 的 7 个用例（第 9.3 节），注册进 CMake。

**立刻跑**

```powershell
cmake -S . -B build
cmake --build build --config Debug
.\build\bin\Debug\mini_tests.exe InputState
```

**应看到**：新增 **7 个** `InputState.*` 用例全 OK，总数 9 → **16**。

### 步骤 S2-6（commit 4）：main 演示 + 验收

1. `main.cpp` 改成第 6.6 节的帧序：Poll → 重画状态行 → Present → BeginFrame
2. 跑完 9.1 节的 8 条正向验收
3. 跑完 9.2 节的 4 条反向验证，**每条都要还原现场**

### 步骤 S2-9（commit 5）：文档同步

改第 11 节的 6 处。

---

## 6. 接口设计（施工图）

### 6.1 `platform/KeyCode.h`

```cpp
namespace mini::platform {

enum class Key : std::uint8_t {
    Unknown = 0,          // 0 必须是 Unknown：数组默认初始化即为"无效"
    W, A, S, D, Q, E, R,
    Space, Enter, Escape, Tab,
    ArrowUp, ArrowDown, ArrowLeft, ArrowRight,
    F1, F2, F3, F4,
    Count                 // 哨兵，= 数组长度
};

// VK 码 → Key。公共头不能 include <windows.h>，所以用裸数字 + 注释（0x57 == 'W' 等）
constexpr Key KeyFromVirtualKey(std::uint8_t vk) noexcept;

// Unknown / Count → Count（越界哨兵）
constexpr std::size_t KeyIndex(Key k) noexcept;

}
```

映射表用 `constexpr std::array<Key, 256>`（或 `switch`），只填项目要用的键，其余 `Unknown`：

| VK（裸数字） | `VK_*` 名 | `Key` |
| --- | --- | --- |
| `0x57` `0x41` `0x53` `0x44` | `'W' 'A' 'S' 'D'` | `W` `A` `S` `D` |
| `0x51` `0x45` `0x52` | `'Q' 'E' 'R'` | `Q` `E` `R` |
| `0x20` `0x0D` `0x1B` `0x09` | `VK_SPACE` `VK_RETURN` `VK_ESCAPE` `VK_TAB` | `Space` `Enter` `Escape` `Tab` |
| `0x26` `0x28` `0x25` `0x27` | `VK_UP` `VK_DOWN` `VK_LEFT` `VK_RIGHT` | `ArrowUp` `ArrowDown` `ArrowLeft` `ArrowRight` |
| `0x70`~`0x73` | `VK_F1`~`VK_F4` | `F1`~`F4` |

修饰键（`VK_SHIFT` / `VK_CONTROL` 及其左右变体）**不映射**。方向键的 `VK_UP/DOWN/LEFT/RIGHT` 是"扩展键"，靠 `dwControlKeyState` 的 `ENHANCED_KEY` 位区分小键盘——S2 **不区分**，小键盘方向键和主方向键当同一个键。

### 6.2 `platform/InputState.h` —— S2 的核心，也是最值得写的东西

```cpp
namespace mini::platform {

class InputState {
public:
    void OnKeyDown(Key k) noexcept;
    void OnKeyUp(Key k) noexcept;
    void OnFocusLost() noexcept;      // 失焦时把所有键放掉

    void BeginFrame() noexcept;       // 帧尾：推进状态

    KeyState Get(Key k) const noexcept;
    bool     IsDown(Key k) const noexcept;   // == Pressed || Held

private:
    std::array<bool, kKeyCount> m_downAtEnd{};  // 本帧事件流结束后的物理状态
    std::array<bool, kKeyCount> m_downPrev{};   // 上一帧的 m_downAtEnd
    std::array<bool, kKeyCount> m_pulse{};      // 本帧内发生过 down（不管之后是否 up）
};

}
```

**为什么要有 `m_pulse`**：一帧内 down 又 up（用户飞快敲一下，或某一帧特别长）时，只看 `m_downPrev` / `m_downAtEnd` 会让这个按键**被完全吃掉**。表现为"偶尔按 W 不动"——在 S5 移动 `@` 时会变成极难查的隐性 bug。

> **写完代码后的认知修正**：`m_pulse` 的作用比上面说的还大 —— 它不只是救"极速点击"，
> **正常按下（`prev=false, now=true`）也走 `if (m_pulse)` 这一支**（因为 `was && now` 判不成立）。
> 所以删掉它 `Pressed` 会**整个消失**，不是只丢边角情况。详见进度板下方"一处认知修正"。

### 6.3 `KeyState` 语义与判定顺序

> `KeyState` 实际定义在 **`KeyCode.h`**（和 `Key` 放一起），不在本节的 `InputState.h` 里 —— 见实现偏差 D2。
> 理由：`Input.h` 需要 `KeyState`，但抽象接口用不到状态机。

```cpp
enum class KeyState : std::uint8_t { Up, Pressed, Held, Released };
```

| `m_downPrev` | `m_downAtEnd` | `m_pulse` | `Get()` | `IsDown()` |
| --- | --- | --- | --- | --- |
| false | false | false | `Up` | false |
| false | true | true | `Pressed` | true |
| true | true | true | `Held` | true |
| true | false | true | `Released` | false |
| false | **false** | **true** | **`Pressed`** | **true** | ← 极速点击：显示**一帧** Pressed |

判定**顺序不能换**（先判 `Held` 再判 `pulse`）：

```
if (prev && now)  → Held      // 必须先判，否则长按会被 pulse 误判成 Pressed
else if (prev)    → Released
else if (pulse)   → Pressed   // !prev && pulse
else              → Up
```

`BeginFrame()` 只有两行语义：`m_downPrev = m_downAtEnd; m_pulse 全部清零;`
`OnFocusLost()`：三个数组全部清零。

> **为什么四态而不是三态**：PLAN C.3 原本写的是三态 `{Released, Pressed, Held}`，但那样**无法区分"本帧刚抬起"和"一直没按过"**，而 S2 的验收要求"松开显示 `Released` 仅一帧"——三态下这条**物理上做不到**。加一个 `Up` 让状态转移图依然是一张直白的表（见第 7 节决策 2）。

### 6.4 `platform/Input.h`

```cpp
namespace mini::platform {

class Input {
public:
    virtual ~Input() = default;
    virtual void     Poll() = 0;                  // 非阻塞，读到事件队列为空
    virtual void     BeginFrame() = 0;
    virtual KeyState Get(Key k) const = 0;
    virtual bool     IsDown(Key k) const = 0;
    virtual bool     QuitRequested() const = 0;   // 只反映"外部强制退出"
};

std::unique_ptr<Input> MakeInput();

}
```

**`QuitRequested()` 只含 Ctrl+C / 关窗口 / 关控制台，不含 ESC**（第 7 节决策 4）。

### 6.5 `platform/Console.h`

```cpp
namespace mini::platform {

struct ConsoleSize { int width = 0; int height = 0; };

class Console {
public:
    virtual ~Console() = default;                  // 析构里恢复终端（RAII）
    virtual void        Present(const render::FrameBuffer& fb) = 0;
    virtual ConsoleSize Size() const = 0;
};

std::unique_ptr<Console> MakeConsole();

}
```

**为什么保留抽象基类 + 工厂**（第 7 节决策 1）：HANDLE 必须作为成员存在于某处，如果放在公共头里就必须 `#include <windows.h>` → **直接违反 A.3**。两个出路是抽象基类或 PIMPL，选前者是因为 PLAN C.3 已经这么定，且不用给每个类写 Rule of Five。

**构造即 `BeginTerminal`、析构即 `EndTerminal`**：S1 那对"必须成对调用"的全局函数到此变成 RAII，`main` 里的 `try/catch(...) { EndTerminal(); throw; }` 补丁可以删掉。

### 6.6 帧序（S2 的 `main`）

```
console = MakeConsole()          // 构造：改终端模式（in + out）
input   = MakeInput()
fb(80, 25)                       // S2 仍用固定尺寸；Console::Size() 留给 S3

while (!input->QuitRequested()):
    input->Poll()                // 1. 收事件进状态
    fb.Clear()
    DrawKeyHud(fb, *input)       // 2. 每帧重画一行状态文本
    console->Present(fb)         // 3. 一次性提交
    input->BeginFrame()          // 4. 帧尾推进 ← 必须在读状态之后
    sleep_for(16ms)
```

**这个顺序写反（`BeginFrame` 放到 `Poll` 之前，或省略），`Pressed` 会永远显示不出来。** 与 PLAN C.4 的定稿帧序一致。

---

## 7. 接口决策表（2026-09-28 已拍板）

| # | 决策 | 落成 | 代价 / 备注 |
| --- | --- | --- | --- |
| 1 | **平台层用抽象基类 + 工厂** | `class Console` / `class Input` 是抽象基类；Windows 实现全藏在 `.cpp` 里；`MakeConsole()` / `MakeInput()` 创建 | 多一层虚调用 + 一组文件。换来"公共头零 Windows 痕迹"，符合 A.3 硬规矩。**PLAN S2 正文的 `IConsole` 措辞作废**（差异 #1） |
| 2 | **按键四态 `Up/Pressed/Held/Released`** | 见 6.3。`Up` = 一直没按，`Released` = 本帧刚抬起 | "松开仅一帧"这条验收得以保留。**PLAN C.3 与 S2 正文的"三态"措辞作废**（差异 #2、#3） |
| 3 | **窗口尺寸事件只消费不处理** | `Poll()` 里 `case WINDOW_BUFFER_SIZE_EVENT:` **直接丢**，旁边留一行注释指向 S3 | 不存 `m_pendingResize`（死代码；且 Clang 的 `-Wall` 含 `-Wunused-private-field`，两个 `ms-clang` 预设以后会报）。S3 建议**每帧比较自己的 FrameBuffer 尺寸与 `Console::Size()`**，压根不需要这个事件（差异 #6 顺便改 PLAN 的 E-12） |
| 4 | **`QuitRequested()` 不含 ESC**（本人定，非上述三问） | 只反映 Ctrl+C / `CTRL_CLOSE_EVENT` / 关窗口 | ESC 从 S10 起要用来关菜单，引擎硬编码它退出到时必然要拆。**PLAN C.3 原句"Ctrl+C / ESC / 窗口关闭"要改**（差异 #4） |
| 5 | 输入模式由 **Console** 设，Input 只读事件 | `ConsoleWindows` 构造里存/改 in+out 两套模式，析构里恢复 | 边界清楚：**Console = 终端模式，Input = 事件→状态**。避免两个类各自保存一份模式、重复恢复 |
| 6 | S2 的 `main` 仍用固定 80×25 | `Console::Size()` 接口先落地（供 S3 用），但 S2 不用它建 FrameBuffer | S2 的焦点是输入通路，不顺手改渲染。S1 的演示行为保持不变 |

---

## 8. 坑表

### 8.1 构造期与终端模式

| # | 现象 | 原因与对策 |
| --- | --- | --- |
| 1 | **stdout 被重定向**（`> out.txt`、管道、或从 VS Code 的"输出"面板跑） | `GetConsoleMode` 失败。构造函数必须**抛 `std::runtime_error` 并打印人话**（"需要一个真实终端；请用 Windows Terminal 直接运行"）。**绝不静默降级**——静默降级的结果是你对着一个没画面的 exe 发呆半小时 |
| 2 | 点一下终端后**输入卡死** | `ENABLE_QUICK_EDIT_MODE` 默认是开的，点击会进"选择模式"。对策：`mode \|= ENABLE_EXTENDED_FLAGS; mode &= ~ENABLE_QUICK_EDIT_MODE`。**两者必须成对写**，只写后者不生效（Win32 的怪癖，PLAN 风险 E-4） |
| 3 | 切走窗口再切回，键**卡在 `Held`** | 切走时收不到 `KEY_UP`。对策：置 `ENABLE_WINDOW_INPUT` 才能收到 `FOCUS_EVENT`，在 `bSetFocus == FALSE` 时调 `OnFocusLost()` 清空全部键 |
| 4 | 改窗口大小后**事件乱** | S2 只丢弃（决策 3）。**不允许**把它当按键处理 |
| 5 | 退出后终端不正常（光标不显示 / 码页没还原 / 留在备用屏） | 三条路径都要覆盖：① 正常退出走 `~Console`；② Ctrl+C 走 handler（**异步信号上下文里只准用 `WriteConsoleA`**，禁止碰 `std::cout` / `printf`，会死锁）；③ `CTRL_CLOSE_EVENT` 拦不住，至少把终端还原 |
| 6 | 码页被硬写成 936 | 必须存**原值**再恢复（用户可能本来就在 65001） |

> **#2 与 #3 是同一个开关的两面**：为了让 `FOCUS_EVENT` 能收到，必须设 `ENABLE_WINDOW_INPUT` —— 而**正是它**让 `WINDOW_BUFFER_SIZE_EVENT` 也开始进队列。所以"要收 focus"和"要丢 resize"必须一起做，不能只做一半。

### 8.2 事件处理

| # | 现象 | 原因与对策 |
| --- | --- | --- |
| 7 | 未映射的键互相干扰 | 若 `KeyFromVirtualKey` 返回 `Unknown` 而你没丢，多个不同 VK 会**一起写 `Unknown` 槽位**。对策：`if (key == Key::Unknown) return;`，且 `Unknown = 0` 让数组默认值即为"无效" |
| 8 | 按键字符乱码 | 用 `ReadConsoleInputW`。`A` 版本在 UTF-8 码页下 `AsciiChar` 只给单字节。我们只取 VK，但用 W 更干净 |
| 9 | 一帧内 down+up 的按键**被吃掉** | 见 6.2 的 `m_pulse`。**这是本阶段最容易埋下的隐性 bug**，第 9.3 节有专门的守卫用例 |
| 10 | 输入有延迟 | `Poll()` 必须**读到队列为空**（`GetNumberOfConsoleInputEvents` 循环 + `ReadConsoleInput`），不要一帧只读固定条数——积压会让输入越来越滞后 |
| 11 | 长按被误判成 `Pressed` | Win32 的自动重复是**一串 `bKeyDown = TRUE`，没有 `KEY_UP`**，所以 `m_downAtEnd` 保持 true → `Get()` 正确返回 `Held`，**无需特判**。但 `Get()` 的判定顺序不能把 `pulse` 放在 `prev && now` 前面（见 6.3） |

### 8.3 工程与构建

| # | 现象 | 原因与对策 |
| --- | --- | --- |
| 12 | CMake 配置挂了 | 新增 `.cpp` 必须**手写**进 `engine/CMakeLists.txt`（禁用 `file(GLOB)`，S0 坑 #8）。`InputState.h` 是 header-only，测试侧不需要新库 |
| 13 | 链接期报 `TerminalPresenter.obj` 找不到 | 删了 `render/TerminalPresenter.cpp` 但忘了改 `engine/CMakeLists.txt` 的源文件列表 |
| 14 | `Key::Count` 当数组下标 | `KeyIndex()` 对 `Unknown` 和 `Count` 都返回 `Count` 哨兵，`OnKeyDown` 里 `if (idx >= kKeyCount) return;` |
| 15 | clang-cl 预设报 `-Wunused-private-field` | 决策 3 的直接原因：不要留"写了没人读"的成员。`m_pendingResize` 这种一律不留 |
| 16 | `NOMINMAX` 缺了导致 `std::max` 编译失败 | S0 已进 `mini_options`（`CMakeLists.txt:31`），**不要**再在每个 `.cpp` 里重复 `#define` |

### 8.4 驱动不了的那部分（写代码时实测）

想给 S2 做"自动交互验收"——喂一个 `q` 进去看程序退不退——试过 winpty，**走不通**：

```bash
printf 'q' | winpty -- ./build/bin/Debug/MiniEngine.exe     # → stdin is not a tty
```

`winpty` 自己会拒绝非 tty 的 stdin，所以程序根本没被启动。结论：

- **能自动测的部分**：四态状态机、VK 映射、`KeyIndex` 边界（`mini_tests` 16 个用例）、
  重定向 stdio 的失败路径（`> out.txt` 抛异常 + exit 1）。
- **只能手工的部分**：`Present()` 的画面、`ENABLE_QUICK_EDIT_MODE` 的效果（要鼠标点）、
  `FOCUS_EVENT`（要切窗口）、`Released` 只闪一帧（要肉眼）。

**这正是把四态逻辑从 `InputWindows.cpp` 拆到 `InputState.h` 的全部回报**：输入逻辑里唯一值得自动验证的部分
现在可测了，剩下必须肉眼看的只有终端本身的行为。

### 8.5 已知问题：关窗口会留下锁住 exe 的孤儿进程（**未修，建议并进 S3**）

**现象**（2026-09-28 真机实测）：不用 Q 退出、而是**直接关掉控制台窗口**，然后重建：

```
LINK : fatal error LNK1168: 无法打开 ...\build\bin\Debug\MiniEngine.exe 进行写入
```

`tasklist` 里 `MiniEngine.exe` 还在跑（父进程已不存在）—— **shell 被杀了，子进程成了孤儿**，
它的控制台也没了，于是它永远等不到 `CTRL_CLOSE_EVENT`，一直在
`while (!quit && !input->QuitRequested())` 里对着死控制台反复 `Present()`。

**两个诊断结论**（都反直觉，记下来）：

| 以为 | 实际 |
| --- | --- |
| "文件被占用时不能改名" | **能改名，但不能写**。Windows 加载器允许 rename 一个正在运行的 exe，只禁止 write。**判占用要试着写，不是试着改名** |
| "关掉控制台窗口 = 进程结束" | 不一定。父进程被硬杀 / conhost 消失时，事件的投递路径也一起没了 |

**根因**：`ConsoleWindows::Present()` 用 `std::cout` 写帧，**从不检查写是否成功**；
控制台死了之后这些调用只是静静置 `failbit`，主循环毫无察觉。`InputWindows::Poll()` 里
`ReadConsoleInputW` 失败也只是 `break`，不置退出标志。

**兜底**（现在就能用）：

```powershell
taskkill /F /IM MiniEngine.exe     # 收尸
cmake --build build --config Debug # 之后构建就正常了
```

**建议的修法**（留给 S3，因为 S3 本来就要管"该退出了"这件事）：
`Present()` 检测写失败 → 调一个 `platform::RequestQuit()`，主循环下一轮自然退出。
S3 的 `Application` 会有统一的退出入口，放在那里比现在零敲碎打更好。
（这条已经同时记进 PLAN 的 S2 坑表，别让它只活在这份清单里。）

> **2026-09-28 更新**：S3 方案已定稿，这条**已排进 S3** —— `Console::Present` 改成返回 `bool`，
> 写失败由 `Application::RequestQuit()` 收口（见 `doc/S3_CHECKLIST.md` 决策 7、6.4 节与验收项 7）。
> 本节的两个诊断结论（"能改名 ≠ 没被占用"、孤儿进程的成因）**继续有效，别删**。

---

## 9. 验收

### 9.1 正向（8 条现象，缺一不可）

```powershell
cmake -S . -B build
cmake --build build --config Debug                                   # 期望 0 warning
ctest --test-dir build -C Debug --output-on-failure                  # 期望 1/1，内部用例 16
.\build\bin\Debug\mini_tests.exe InputState                          # 期望 7 passed
.\build\bin\Debug\MiniEngine.exe                                     # 手动验收
```

| # | 操作 | 必须看到 | 结果 |
| --- | --- | --- | --- |
| 1 | 启动 exe | 顶部**三行 HUD** 实时刷新（`W: -  A: -  S: -  D: -  SPACE: -` / `Q: -  ESC: -  UP: -  DOWN: -  LEFT: -` / `RIGHT: -  F1: -  F2: -  TAB: -  ENTER: -`），中间是个黄色 `@`，底部一行操作提示 | ✅ |
| 2 | **长按 W** | `W: Held`（绿色） | ✅ |
| 3 | **松开 W** | `W: Released`（红色）**恰好一帧**，然后回 `-` | ✅ |
| 4 | **飞快点一下 Space** | `SPACE: Pressed`（黄色）一帧（验证 `m_pulse` 那条路径） | ✅ |
| 5 | 按 Q | 退出，回原屏，光标可见，`$LASTEXITCODE` = 0 | ✅ |
| 6 | **鼠标在终端里拖选一下**（出现选区）→ 再按 W | 仍然响应（`ENABLE_QUICK_EDIT_MODE` 已关的证据） | ✅ |
| 7 | 切到别的窗口停 2 秒再切回来 | **没有任何键卡在 `Held`** | ✅ |
| 8 | 改终端窗口大小 | 不崩、不误报按键 | ✅ |
| 9 | 按 W/A/S/D 或方向键 | `@` **每按一次走一格**；**按住不放只走一格**（不连走） | ✅ |

另外：`ctest` 全绿、**0 warning**（`/W4` 下 C4819 / C4127 / C4101 一个都不能有）。

> **2026-09-28：以上 9 条在真机 Windows Terminal 上确认通过**（原计划是 8 条，第 9 条是 `@` 移动的验收，
> PLAN S2 的目标里有"`@` 能响应按键移动"，所以补成 9 条）。

### 9.2 反向验证（照 S0 的传统）

| # | 怎么破坏 | 必须看到 | 还原 | 结果 |
| --- | --- | --- | --- | --- |
| 1 | 去掉 `~ENABLE_QUICK_EDIT_MODE` | 点一下终端 → 输入卡死。**证明这条改动是必要的，不是抄来的** | 加回 | ⏳ 未单独实跑（随 S2 确认完成） |
| 2 | 去掉 `ENABLE_WINDOW_INPUT` | 切窗口回来 → W 卡在 `Held`。证明 `FOCUS_EVENT` 的作用 | 加回 | ⏳ 未单独实跑（随 S2 确认完成） |
| 3 | 把 `m_pulse` 那条分支删掉 | `InputState.DownAndUpInSameFrameIsNotLost` 变红。证明测试真的在守这条边界 | 加回 | ✅ 实跑（比预期多红一条，见"一处认知修正"） |
| 4 | `MiniEngine.exe > out.txt` | 构造抛 `runtime_error` + 人话提示，**不是**静默无画面、也不是崩溃 | 不用改代码 | ✅ 实跑（exit=1，未进备用屏） |

### 9.3 测试 `tests/test_input_state.cpp`（全部离线，不需要终端；已实跑 16 passed）

| 用例（实际名） | 断言 |
| --- | --- |
| `InputState.InitialIsUp` | 初始所有键 `Up`，`IsDown` 全 false；连续 `BeginFrame` 仍是 `Up` |
| `InputState.DownThenFrameGivesPressedThenHeld` | down → `Pressed`；`BeginFrame` → `Held`；**自动重复（再次 OnKeyDown）仍是 `Held`** |
| `InputState.UpGivesReleasedForOneFrame` | up → `Released`；`BeginFrame` → `Up` |
| `InputState.DownAndUpInSameFrameIsNotLost` | down + up 同帧 → `Pressed`；`BeginFrame` → `Up`；down→up→down 也算按下 ← **坑 #9 的守卫** |
| `InputState.FocusLostReleasesEverything` | `Held` 中 `OnFocusLost` → 全 `Up`，且下一帧不会"冒回来" |
| `KeyCode.KeyFromVirtualKeyMapsUsedKeys` | `0x57→W`、`0x26→ArrowUp`、`0x1B→Escape`、`0x70→F1`、`0x42→Unknown`、`0x10→Unknown`（Shift）、`0x0100`/`0xFFFF→Unknown`（高位码不被截断） |
| `KeyCode.InvalidKeyIndexIsIgnored` | `KeyIndex(Unknown)` / `KeyIndex(Count)` 都等于哨兵 `kKeyCount`；所有合法键下标 `< kKeyCount` 且互不相同；灌完无效键后 `W` 仍是 `Up` |

`tests/CMakeLists.txt` 加一行 `test_input_state.cpp`（手写，无 GLOB）。
**实跑结果：9 → 16 passed**，其中 `InputState` 5 个、`KeyCode` 2 个。

---

## 10. commit 切分（每个都能构建）

| # | 信息 | 内容 | 实际结果 |
| --- | --- | --- | --- |
| 1 | `feat: platform::Console 以 RAII 接管终端模式，TerminalPresenter 退休`<br>`14b60d0` | 新建 `Console.h` / `ConsoleWindows.cpp`（原计划的 `QuitFlag.h` 没建，见 D1）；删 `render/TerminalPresenter.*`；**main 必须同时改调用点，否则编译不过**；`engine/CMakeLists.txt` 同步 | ✅ 0 warning、`ctest` 1/1、`mini_tests` 9 passed、画面与 S1 一致 |
| 2 | `feat: 加入 platform::Input 与四态按键状态机`<br>`a3a9b09` | `KeyCode.h` / `InputState.h` / `Input.h` / `InputWindows.cpp` | ✅ 0 warning、`ctest` 1/1（用例数**没涨**） |
| 3 | `test: InputState 状态机与 VK 映射单测（含极速点击守卫）`<br>`f4ea7a8` | `test_input_state.cpp` + `tests/CMakeLists.txt` | ✅ `mini_tests` 9 → **16 passed** |
| 4 | `feat: main 改为按键四态 HUD，WASD/方向键瞬时移动 @`<br>`feaa06c` | 三行 HUD + `@` 每按一次走一格 + Q 退出。**9.1 / 9.2 的交互验收未做** | ⏳ **待手工跑 9.1 全部 + 9.2 第 1、2 条** |
| 5 | `docs: PLAN S2 落地记录与平台依赖规则更新` | 本文件 + PLAN 的 S2 落地记录 | ✅ |

**"每个 commit 都能构建"比切得漂亮重要**——构建不过就合并到下一个。commit 1 是大头，别试图再拆细（拆到"删旧文件但 main 还用"就必然有一个编译不过的中间态）。

> 另有一个**不在原计划里的第 0 个 commit** `2c15f8b docs: S2 方案落地为 S2_CHECKLIST，同步 PLAN 的 A.3/C.3/S2/E-12`：
> 那是本清单本身落地时产生的（含 PLAN 的 6 处措辞修正），比实现早一步进仓库。

---

## 11. 与 PLAN 原文的差异（**6 条全部已改完**，`2c15f8b`）

| # | 位置 | 原文 | 改成 | 状态 |
| --- | --- | --- | --- | --- |
| 1 | PLAN S2 正文"实现"行 | "接口类 `IConsole`" | `platform::Console`（抽象基类 + 工厂） | ✅ |
| 2 | PLAN S2 正文"实现"行 | "三态 `Pressed/Held/Released`" | 四态 `Up/Pressed/Held/Released` | ✅ |
| 3 | PLAN C.3 | `enum class KeyState { Released, Pressed, Held };` | `{ Up, Pressed, Held, Released }`，并补 `IsDown()` 的定义 | ✅ |
| 4 | PLAN C.3 | `virtual bool QuitRequested() const = 0; // Ctrl+C / ESC / 窗口关闭` | 去掉 ESC：只反映"外部强制退出" | ✅ |
| 5 | PLAN A.3 | "**这条规则的唯一例外是阶段 1**（还没抽象平台时允许临时写在 `render/Console` 里），阶段 2 必须把它挪走" | 例外已终结，删掉这半句，改成"`<windows.h>` 只允许出现在 `engine/src/platform/`" | ✅ |
| 6 | PLAN E-12 | 对策含 `WINDOW_BUFFER_SIZE_EVENT`，阶段标 `S2/S6` | 阶段改 `S3/S6`，对策改成"每帧比较 FrameBuffer 尺寸与 `Console::Size()`；事件在 S2 已被安全丢弃" | ✅ |

顺手还改掉了 C.3 里两处会被四态决策带歪的注释：`BeginFrame`（原"把 Pressed 降级为 Held"是三态的说法）、
`MakeInput`（原"`.cpp` 里 `#ifdef` 分派"——本项目只做 Windows，没有分派文件）。


**另外三处结构性同步**（不算差异，属例行更新）：

| 位置 | 改什么 | 状态 |
| --- | --- | --- |
| PLAN A.2 目录树 | `platform/` 下改成实际文件名（`Console.h` `Input.h` `InputState.h` `KeyCode.h`）；`doc/` 下加 `S2_CHECKLIST.md` | ✅ `2c15f8b` |
| PLAN 第 0 节状态行（`MiniEngine_PLAN.md:29`） | `S1` 完成 → **S2 实现完成 + 待手工验收** | ✅ |
| PLAN S2 小节末尾 | 补"**S2 落地记录**"（6 条接口决策 + 实际结构 + 4 条新坑） | ✅ commit 5 |
| `AGENTS.md` 第 2 节文档表 | 加一行 `doc/S2_CHECKLIST.md` | ✅ `2c15f8b` |

上面 6 条**全部已改完**（`2c15f8b` 一次改掉 1~6 + A.2 + AGENTS.md；状态行与落地记录随后补）。

---

## 12. 交付后的自检（收尾用）

- [x] `git status` 里没有 `build/`、没有 `CMakeUserPresets.json`、没有含绝对路径的文件
      （`git ls-files | grep -iE "^build/|CMakeUserPresets"` 为空）
- [x] 新增源文件全部是 UTF-8 **无 BOM**、行尾 **LF**
      （字节级核对：`od` 数 `0x0d` 全部为 0；`file` 判定为 "UTF-8 text"；`git ls-files --eol` 全部 `i/lf w/lf`）
      ⚠️ 别用 `grep -c $'\r'` 判 CRLF —— 在 Git Bash 里它会匹配**整行**，给出"每行都有 CR"的假阳性
- [x] `mini_tests.exe Nope` 仍然报错 + 退出码非 0
      （实测输出 `no case in suite 'Nope' (16 case(s) registered)`，`$LASTEXITCODE` = 1，S0 的门闩没被 S2 破坏）
- [x] 两个 `ms-clang-debug` / `ms-asan` 预设**仍标"未实测"**——计划与 S4 的 Math 测试一起验，没在 S2 顺手改状态

