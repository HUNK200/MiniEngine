# MiniEngine 实施方案

> 本文件是对 `input_plan.md` 的落地回答，按 A~G 结构组织。
> 定位：**C++ + Game Engine Playground**，不是一个必须完成的工程。
> 原则：可运行优先 / 不过度设计 / 每阶段有"好玩的结果"。
>
> **想先看路线？直接跳到 H 节**（长期路线：可停在三条里程碑的哪一站、有空再挑的扩展池）。
>
> ⚠️ **项目约定**：AI 默认只产出**方案**、不写实现代码，具体实现由我自己敲（判定标准与例外见 `AGENTS.md`）。

---

## 0. 环境基线（家机 / 公司机 / 新机分列）

> ⚠️ 本节原文只有一套路径，而且被写成了"本机"。**2026-09-23 家机、2026-09-24 公司机各逐条实测一遍，
> 结果两台机器连工具版本都不一样**（VS 家机装在 E 盘、CMake 一个 4.4.3 一个 4.2.1），
> 照一份基线敲命令会直接找不到工具。**以后每台机器只信自己这一列，动手前先跑一次 `cmake --version`。**
>
> **2026-10-02 第三台机器（下表"新机"列）加入，又是一套版本**：CMake **4.0.0-rc3**、
> MSVC 只有 **14.43.34808（cl 19.43）**、VS 装在 **C 盘**（Community 17.13.2）。
> 这台 clone 后按 A.2 设完 local config，直接 `cmake -S . -B build` 即成功；
> 构建 0 warning、`ctest` 1/1、`mini_tests` **90 passed**。
> 结论：**"CMake ≥ 3.20 + MSVC"这条最低要求在三套版本上都成立** —— 版本号只记事实，不参与判断。

| 项 | 家机（2026-09-23 实测） | 公司机（2026-09-24 实测） | 新机（2026-10-02 实测） | 对方案的影响 |
| --- | --- | --- | --- | --- |
| CMake | **4.4.3** @ `C:\Program Files\CMake\bin`（当天用 `winget install Kitware.CMake` 补装，**装之前家机完全没有 CMake**） | **4.2.1** @ `C:\Program Files\CMake\bin` | **4.0.0-rc3** @ `C:\Program Files\CMake\bin`（**RC 版**；2026-10-02 决定先不升级、先用它跑通全绿 —— 触发条件：报出真实构建错误时再换正式版） | **CMake 4.x 已移除 `cmake_minimum_required(VERSION < 3.5)` 的兼容**，最低写 `3.20` |
| MSVC | VS 2022 Community @ **`E:\Program Files\VisualStudio`**（**不在** C 盘）；装了 14.38.33130 / 14.44.35207 两套 toolset；CMake 生成器实测选中 **19.38.33145.0** | VS 2022 Community，cl 14.44 (19.44) | VS 2022 Community **17.13.2** @ **`C:\Program Files\Microsoft Visual Studio\2022\Community`**（**在** C 盘）；**只装 14.43.34808 一套 toolset（cl 19.43）**；生成器自动选中，未硬编码 | 主力编译器，默认生成器 `Visual Studio 17 2022`；生成器选哪套 toolset 由 VS 自己定，别在文档里硬编码 |
| clang | **19.1.5** @ `…\VisualStudio\VC\Tools\Llvm\bin`（**不在** `C:\Program Files\LLVM`） | 17.0.6（原记录，**未复测**） | 未探测（本项目不碰） | ⛔ **不使用**（2026-09-29 决定）：不做 clang 交叉验证，构建脚本里也不留 ClangCL 分支。**它装着，但本项目不碰** |
| Windows SDK | 10.0.22621.0 | **10.0.26100.0**（配置期自动选中，目标 10.0.26200） | **10.0.22621.0**（只装了这一个） | — |
| g++ / gcc | **未安装** | **未安装** | **未安装** | **只做 Windows**（已确认）：不写也不跑 Linux / macOS 分支。编译器**只有 MSVC 一条路** |
| ninja / make | **未安装**（不阻塞，VS 生成器可用） | **未安装** | **未安装** | 多配置（VS）生成器，所以**必须带 `--config`／`-C`** |
| Git | 2.52.0；仓库 local 身份 `HUNK <wuunj@126.com>` | 2.52.0；**仓库级身份未设**，当前只吃全局 `Wuyangzhi <wu.yangzh@aerok.com.cn>`（补设见 A.2） | **2.36.1**（比另两台旧）；A.2 四步**已设齐**（`autocrlf=false` / `longpaths=true` / `quotepath=false` / `pull.rebase=true`），仓库级身份 `HUNK200 <wuunj@126.com>` ← **这台是个人机** | **身份按机器各用各的**（见 A.2），不要求两机一致 |
| Shell / 终端 | Git Bash，`TERM=xterm-256color`，代码页 **936 (GBK)** | **PowerShell（主力）**，Git Bash 也能用；VS Code 内置终端是 UTF-8，传统控制台代码页 **936 (GBK)** | PowerShell 可用；本次实测走 Git Bash；代码页 **936 (GBK)** | **给用户的命令一律按 PowerShell 写**：退出码用 `$LASTEXITCODE`（不是 `echo $?`）、路径用反斜杠；终端输出必须切 UTF-8，否则 `┌─┐@#*` 全乱码 |

> 项目当前阶段以 **B 节**为准，本节不再记状态 —— 这里原来有一行"项目当前状态：S2 验收完成"，
> 到 S5 就自动过期了（2026-10-02 发现）。同理，本表只记"这台机器上有什么"，不记"项目做到哪"。

### 标准命令（PowerShell + 多配置生成器）

```powershell
# 配置 + 构建（Debug）
cmake -S . -B build
cmake --build build --config Debug

# 运行（S0 起把 exe 统一输出到 build/bin/）
.\build\bin\Debug\MiniEngine.exe
.\build\bin\Debug\MiniEngine.exe 300    # S3 起：第一个参数 = 目标帧率（300 = 不限帧观察）

# 测试（-C 不能漏，否则 CTest 找不到配置）
ctest --test-dir build -C Debug --output-on-failure
.\build\bin\Debug\mini_tests.exe           # 全部用例
.\build\bin\Debug\mini_tests.exe RowDiff   # 只跑某个 suite（S3 起有 suite 过滤）
```

可选（**ASan**）：`cmake --preset ms-asan` → `cmake --build build-asan --config Debug` → 用来查内存错误。
MSVC 原生支持 `/fsanitize=address`，**不需要 clang**（2026-09-29 之前预设写成继承 ClangCL，已改正）。

> ⛔ **不做 clang 交叉验证**（2026-09-29 决定）：原先这里给过一条 `-T ClangCL` 的命令。
> 现在编译器只有 MSVC 一条路 —— 放弃的是"clang 与 MSVC 告警不一致"那层网，**这是已知且接受的风险**。

> 想让构建快一点再装 Ninja（`winget install Ninja-build.Ninja`），届时 `-G Ninja -DCMAKE_BUILD_TYPE=Debug`，命令回到单配置风格。**这是可选优化，不是前置依赖。**

> 🔧 **家机 2026-09-23 才补装上 CMake**：已经开着的 Git Bash 不会自动拿到新的 PATH，**重开一个终端** `cmake` 就能用了；旧终端里临时用全路径 `"/c/Program Files/CMake/bin/cmake.exe"`。

### 平台策略（已确认：只做 Windows）

- **只做 Windows**：不写 Linux / macOS 分支，不写 `termios` / `fcntl` / `ioctl` / `#ifdef __linux__`（见 `AGENTS.md`）。等哪天真要跨平台再说——那时的成本是重构 `platform/` 一层，不是现在。
- **但 `engine/platform/` 这层抽象保留**：它的目的不是跨平台，而是"**把 `<windows.h>` 关在一个目录里**"。终端模式、键盘事件、窗口尺寸这些 Windows API 只允许出现在这里，其余代码保持干净、可移植的写法。
- 不为了"跨平台好看"提前抽象，也不为了快而把 `SetConsoleCursorPosition` 散落各处。

---

## A. 项目总体架构

### A.1 模块图

```text
                          ┌──────────────┐
                          │   main.cpp   │   只负责构造并 Run
                          └──────┬───────┘
                                 │
                    ┌────────────▼────────────┐
                    │  engine::core::Application │  ← 生命周期 + Game Loop
                    │  Time / Logger / Config    │
                    └────────────┬────────────┘
        ┌───────────────┬────────┼─────────┬───────────────┐
        ▼               ▼        ▼         ▼               ▼
  ┌───────────┐  ┌───────────┐ ┌────────┐ ┌─────────┐ ┌──────────┐
  │ platform  │  │  render   │ │ world  │ │ physics │ │    ai    │
  │ Console   │  │FrameBuffer│ │ World  │ │ Collide │ │ FSM      │
  │ Input     │  │Renderer2D │ │ Entity │ │ AABB    │ │ EnemyAI  │
  │ (Win/Lin) │  │Camera     │ │ Map    │ │ Move    │ │          │
  └─────┬─────┘  │Renderer3D │ └───┬────┘ └────┬────┘ └────┬─────┘
        │        └─────┬─────┘     │           │           │
        │              │           │           │           │
        │        ┌─────▼───────────▼───────────▼───────────▼─────┐
        │        │                 math (header-only)            │
        │        │      Vec2 / Vec3 / Rect / AABB / Ray           │
        │        └───────────────────────────────────────────────┘
        │
        ▼
  终端 (Windows Terminal / conhost)

                    ┌──────────────────────┐
                    │   game (上层游戏逻辑)  │
                    │  Player Enemy Weapon  │
                    │  Projectile HUD Wave  │
                    └──────────┬───────────┘
                               │ 只能向下依赖 engine
                               ▼
                             engine

                    ┌──────────────────────┐
                    │  debug                │
                    │  Profiler / Overlay   │  ← 被 core/game 使用，自身不反向依赖
                    └──────────────────────┘
```

### A.2 目录结构

```text
c++_simple_engine/
├── AGENTS.md                   # 项目约定（必须留根目录：会话级自动加载）
├── CMakeLists.txt              # 顶层：标准/选项/子目录
├── CMakePresets.json           # 预设：ms-msvc-debug（默认）/ ms-asan；生成/构建/测试三组都有
├── .gitattributes              # 行尾统一 LF（两机同步用）
├── .gitignore
├── main.cpp                    # 唯一入口。S3 起含临时 DemoGame（IGame 实现）→ **S5 已把游戏侧
│                               #   全部搬去 `game/`**：现在 47 行（40 行非空），只做 argv + 造对象 + Run
│                               #   （A.2 原估算"<20 行"没达成，多出来的全是"为什么这么排"的注释）
│                               #   游戏的版面从**第 1 行**开始 —— 第 0 行留给引擎顶栏
├── README.md                   # 阶段 14 再写，别提前写
│
├── doc/                        # 全部文档（2026-09-23 起集中于此）
│   ├── input_plan.md
│   ├── MiniEngine_PLAN.md      # 本文件
│   ├── S0_CHECKLIST.md         # S0 清单 + CMake 补丁清单 + 勾选进度
│   ├── S2_CHECKLIST.md         # S2 施工图：接口决策 + 坑表 + 测试计划 + 正/反向验收
│   ├── S3_CHECKLIST.md         # S3 施工图：Application/Time/RowDiff + 验收
│   ├── S4_CHECKLIST.md         # S4 施工图：math/ 四个头 + 33 个用例 + 与 PLAN 的 9 条偏差
│   ├── S5_CHECKLIST.md         # S5 施工图：world/ + game/ + 13 条落地偏差 + 4 条反向验证实测
│   ├── TECH_DEBT.md            # 技术债：A/B/C/D 四组，**每阶段收口时更新**
│   ├── MINITEST_SPEC.md        # 测试框架 MiniTest 的规格
│   ├── CTEST_NOTES.md          # CTest 入门与 GoogleTest 分工
│   └── WORK_MODE.md            # 上班模式开关正文
│
├── .agents/skills/work-mode/    # 上班模式开关的 skill 入口（转发到 doc/WORK_MODE.md）
│
├── engine/
│   ├── CMakeLists.txt          # add_library(engine STATIC ...)
│   ├── include/mini/           # 公共头（对外只暴露这层）
│   │   ├── core/       Application.h  Time.h  Logger.h  Config.h  NonCopyable.h
│   │   ├── platform/   Console.h  Input.h  InputState.h  KeyCode.h  Wait.h    # 只做 Windows
│   │   ├── math/       Vec2.h  Vec3.h  Rect.h  AABB.h  Ray.h  MathUtils.h
│   │   │               # ↑ 树是全项目**目标态**；S4 落地的字母序是 MathUtils.h Vec2.h AABB.h Ray.h
│   │   │               #   Vec3.h / Rect.h 只有契约还没建（理由见 S4_CHECKLIST 差异 2）
│   │   ├── render/     Cell.h  Color.h  FrameBuffer.h  RowDiff.h  Renderer2D.h
│   │   │               Camera.h  Viewport.h  Renderer3D.h  RayCaster.h
│   │   │               # ↑ 目标态；**S6 落地了 `Camera.h`**（header-only，118 行，不需要 .cpp）
│   │   │               #   `Viewport.h` **本阶段不建**：S6 只需要"四个 int"，0 调用者
│   │   │               #   （S6_CHECKLIST 差异 4）。这条 render → math 是本项目第一条跨界边
│   │   ├── world/      World.h  Entity.h  Transform.h  Velocity.h  Health.h
│   │   │               TileMap.h  Scene.h
│   │   │               # ↑ 同样是目标态；**S5 落地了 Entity.h / TileMap.h / World.h 三个**
│   │   │               #   （Transform / Velocity 并进 Entity.h —— S5 决策 4；Health/Scene 未建）
│   │   │               #   **S7 又落地了 Collision.h**（header-only，241 行：1×1 盒的分轴 + 夹紧 + 子步进）
│   │   │               #   ⚠️ 它按下面那行本该属于 `physics/` 层，S7 落进了 `world/` —— 见 S7_CHECKLIST 差异 7
│   │   ├── physics/    Collision.h  PhysicsSystem.h
│   │   │               # ↑ 目标态，**S7 没有建这一层**（Collision.h 落在 world/）
│   │   ├── ai/         StateMachine.h  EnemyController.h
│   │   │               # ↑ 目标态；**S8 落地的是 `EnemyBrain.h`**（header-only，249 行：五态枚举 + Decide）
│   │   │               #   ⚠️ **没有建 `StateMachine.h`**：5 个状态用 `switch` 更直白，
│   │   │               #   建通用模板属于"提前造抽象"（S8_CHECKLIST 差异 2）
│   │   └── debug/      DebugOverlay.h  Profiler.h
│   └── src/                    # 与 include 镜像的 .cpp；S2 起 platform/ 下放 Windows 实现
│                               #   （唯一允许出现 <windows.h> 的地方，公共头一个都不能有）
│
├── game/                       # **S5 新建**：游戏侧静态库（main → game → engine，engine 绝不反向）
│   ├── CMakeLists.txt          # add_library(game STATIC ...)；PUBLIC 链 engine、PRIVATE 链 mini_options
│   ├── include/game/
│   │   ├── Game.h  GameState.h  Player.h  Enemy.h
│   │   │               # ↑ 目标态；**S8 落地了 `Enemy.h`**（`EnemyData`：敌人的"状态+血量+定时器"，
│   │   │               #   刻意**不**塞进 `mini::world::Entity` —— S8 分叉 ① / 差异 1）
│   │   │               #   GameState / Player 仍未建（S10 / 玩家没有独立类型，就是那个 `@` 实体）
│   │   ├── Weapon.h  Projectile.h  Pickup.h  WaveManager.h  Hud.h
│   │   │               # ↑ 目标态；**S5 落地的只有 Game.h**（其余等 S9/S10）
│   └── src/                    # S5：Game.cpp
│
└── tests/
    ├── CMakeLists.txt
    ├── MiniTest.h              # 自制极简断言框架（~80 行）
    ├── test_main.cpp           # runner：注册表 + suite 过滤 + 汇总（退出码 = 失败用例数）
    ├── test_smoke.cpp          # S0（3 个）
    ├── test_frame_buffer.cpp   # S1（6 个）
    ├── test_input_state.cpp    # S2（7 个，四态状态机 + VK 映射）
    ├── test_time.cpp           # S3（clamp / FPS 算法 / platform::Wait 精度）8 个
    ├── test_row_diff.cpp       # S3（脏行格式 + 收益守卫 + 字节等价性）10 个
    ├── test_math.cpp           # S4（MathUtils 7 / Vec2 9 / AABB 7 / Ray 9 = 32 个）
    ├── test_world.cpp          # **S5**（TileMap 8 / EntityId 2 / World 10 = 20 个）
    ├── test_game.cpp           # **S5**（Game 3 个，靠假 Input 替身离线跑 —— 决策 9）
    │                           #   2026-10-02 S6：量速度那条改用 256×256 画布，
    │                           #   让相机退化成空操作（见 S6_CHECKLIST 落地偏差 3）
    ├── test_camera.cpp         # **S6**（Camera 14 个，纯函数、不需要终端）
    ├── test_collision.cpp      # S7 之后
    ├── test_tilemap.cpp
    └── test_gamestate.cpp
```

### A.3 依赖规则（一条硬规矩）

```text
main.cpp  →  game  →  engine  →  标准库
                ↘        ↙
                 debug（谁都能用，它不反向依赖任何人）
```

- `engine` **绝不 include `game`**；`game` 绝不直接 include `<windows.h>`。
- `math` 是 header-only、零依赖、`constexpr`，可以被任何层包含。
- `render` 不认识的"游戏概念"（Player、Enemy 一律不认识），只认坐标和字符。
- 只有 `platform/` 里允许出现 `#ifdef _WIN32` / `#include <windows.h>`，且只出现在 `engine/src/platform/` 的 `.cpp` 里——**公共头一个都不能有**。（S1 曾把终端代码临时放在 `render/TerminalPresenter.*`，**该例外已于 S2 终结**；`render` 从此不反向依赖任何一层。）

### A.4 为什么这么设计

1. **按"变化频率"分层，而不是按"技术名词"分层。** `math` 几乎不变，`render` 每阶段都变，`game` 天天变。把易变的放上层、稳定的放下层，改游戏逻辑时不会牵动渲染代码。
2. **`engine` 与 `game` 物理分家**，是为了强制回答一个问题："这个东西到底是引擎能力，还是游戏逻辑？"——这个判断力比代码本身值钱。同时让 `engine` 可以被 `tests` 单独链接。
3. **`platform` 单独一层，而不是用宏散落各处**，因为这是全项目唯一真正要碰 Windows API 的地方（终端模式、键盘事件、窗口尺寸）。集中在一处，`<windows.h>` 就只出现在一个目录里，其余代码保持干净的 C++ 写法。**这层抽象的目的不是跨平台，而是隔离平台 API**——本项目只做 Windows（见 `AGENTS.md`）。
4. **`debug` 从阶段 1 就存在**（哪怕只是一个 FPS 数字），因为"能看见内部状态"这件事必须一开始就有，否则后面会被迫用 `std::cout` 打补丁，然后忘了删。
5. **不上 ECS、不上 EventBus、不上 ResourceManager**（见 D 节）。理由：这个项目的实体数量是几十，不是几十万；简单 `std::vector<Entity>` 完全够用，而 ECS 会带来"为了改一个字段要写三处"的摩擦，直接违背"让我想打开 VS Code"的初衷。

### A.5 坐标系统约定（必须先定死，否则后面到处是 +1/-1 bug）

| 空间 | 单位 | 原点 | 说明 |
| --- | --- | --- | --- |
| World | 格 (float) | 地图左上角，X 右 Y 下 | 实体位置存这里，`(0.5, 0.5)` 表示第 1 格中心 |
| Camera | 格 (float) | 视口左上角对应的世界坐标 | `camera.pos` 就是"世界原点在屏幕上的偏移" |
| Viewport | 格 (int) | 视口内左上角 | 夹取/裁剪用，等于终端的宽高 |
| Screen | 字符格 (int) | 屏幕左上角 (0,0) | FrameBuffer 索引 |

```text
screen_cell = floor(world - camera.pos)          // 1 格 = 1 字符：取"这个点所在的那一格"
world       = camera.pos + screen_cell + 0.5     // 反算回去拿到的是那格的中心（不是精确逆运算）
map_tile    = floor(world)                       // 与 screen_cell 是同一个概念 → 用同一个函数
```

⚠️ **2026-10-02（S6 落地）修正**：第一行**只在 `camera.pos` 是整数时才和渲染一致**。
`camera.pos` 带小数时它与"画面按整数格平移"的渲染**差一格**（S6 实测：`pos = 10.7`、实体在
`63.2` 时，`floor(63.2 − 10.7) = 52`，而渲染把那个实体画在第 `63 − 10 = 53` 列）。
S6 起统一为**整数原点**：

```text
OriginCol   = floor(camera.pos.x)                // 相机**唯一**一次取整
OriginRow   = floor(camera.pos.y)
screen_col  = ToIndex(world.x) - OriginCol       // 地图与实体**共用**这一个原点
screen_row  = ToIndex(world.y) - OriginRow + kHudRow
```

"两条取整路径各自漂移"就是 E-8 本身 —— 理由与复现见 `doc/S6_CHECKLIST.md` 6.4 / 7.2。
上面第三行 `world = camera.pos + screen_cell + 0.5` **仍然成立**：S9 的 `ScreenToWorld`
必须用**浮点的 `camera.pos`**（换成整数原点会丢掉小数部分）。

四条必须写进注释的规矩：
- **实体位置用 float，画的时候才取整**；直接在 int 上做移动，斜向速度会被吃掉。
- **"落在哪一格"和"画在哪一列"用同一个 `floor`**。混用 `round`/`floor` 的症状就是下面 E-8 的
  "画面整体偏移半格 / 走位比视觉慢一格"。
  ⚠️ 2026-09-30 修正：本块原写 `screen = round((world - camera.pos) * 1.0)`，
  它与 `doc/S5_CHECKLIST.md` 用例 23 的期望值（`(5.5, 3.5)` → `At(5, 4)`）**差一整格**，已统一为 floor。
- **`floor` 而不是 `(int)`** 转 tile 索引；负数坐标下截断会指向错误的格。
- **相机只取整一次**（2026-10-02 S6 加）：屏幕格 ↔ 世界格走"整数原点"
  （`Camera::OriginCol/OriginRow`），地图与实体**共用**它 —— 见上面的 S6 修正。
- **碰撞也共用这同一个 `ToIndex`**（2026-10-08 S7 加）：`world/Collision.h` 里"这一格是不是墙"、
  "实体占的是哪一格"走的都是它 —— 到 S7 为止，**全项目仍然只有这一条取整路径**。
  ⚠️ 2026-10-08 收口后这条变得**更硬**：碰撞模型从"1×1 盒"改成**点模型**（占格 = `ToIndex(pos)`），
  于是**渲染用的那一格和碰撞用的那一格是同一个表达式** —— "看见的 = 占的"不再是巧合（见 `doc/S7_CHECKLIST.md` 顶部「模型变更」）。

### A.6 容量与生命周期约定（2026-10-10 加；**设计任何"会增殖的东西"之前先看这一节**）

> 起因：S9 讨论"子弹也是 `Entity` 的话，实体会不会爆炸"。结论是"**现在的内存很小，但上界算法和直觉不一样**"，
> 而且**第一个撞到的墙不是内存，是 CPU**。这一节是全局约定，不只管子弹。

**① 代码里现在没有任何数量上限。** `World::m_entities`（`std::vector<Entity>`）、`m_pendingRemove`、
`Game::m_bullets` 都**没有 cap、没有 reserve、没有 assert**；`m_nextId` 是 `std::uint32_t`、**只增不减**
（→ 42.9 亿次生成才可能绕回 0，不构成实际风险）。

**② 存量上界 = 到达率 × 平均停留时间**（不是"屏幕有多大"）。
⚠️ **屏幕不构成上界**：同屏多个东西**可以重叠**（`TECH_DEBT` C12 的实体间分离解算没做），
所以"80×24 格"只是**可见位置**的上界，不是**数量**的上界。

**③ 到达率的结构性上界来自输入层**（S9 的 `SPACE` 是"按一下一发"）：
`InputState::m_pulse` 是**每键一个 `bool`**，`BeginFrame` 每帧清零 ⇒ **每键每帧最多一次 `Pressed`** ⇒ 到达率 ≤ 帧率。
⚠️ **键盘自动重复不会产生 `Pressed`**：`Get()` 的判据是 `if (was && now) return Held;` —— 重复的 `bKeyDown`
在下一帧 `was == true`，落在 `Held` 上（`InputWindows.cpp:81` 那条注释的推理是对的）。

**④ 停留时间由"死亡条件"决定**：撞实心格（含地图边界，越界 = 实心）或命中目标 ⇒
最长 ≈ **地图对角线 ÷ 速度**（200×200 地图、45 格/秒 ⇒ **6.3 秒**）。

**⑤ 今天的实测数字**（80×24 画布 / 200×200 地图 / 单发武器）：

| 项 | 数字 |
| --- | --- |
| 整个程序内存 | **≈ 100 KB**（大头是固定的两块：ANSI 序列化缓冲 **38.4 KB** + `TileMap` **40 KB**） |
| 实体侧最坏（378 发 + 6 个固定实体，含 `vector` 1.5 倍容量余量） | **≈ 16 KB（约 15%）** |
| 60fps 下同时在飞的子弹上界 | **≈ 378 发** |

⚠️ `capacity ≠ size`：MSVC 的 `std::vector` 按 **1.5 倍**增长（实现定义，非标准；libstdc++ 是 2 倍）→ 算内存时要留这个余量。

**⑥ ③ 那条不变量会被武器设计打破**（这是本节存在的真正理由）：

| 武器形态 | 上界公式 | 60fps 下 | 内存 | 先撞到哪面墙 |
| --- | --- | --- | --- | --- |
| 单发（S9） | `fps × (对角线 ÷ 速度)` | 378 | 16 KB | 没有 |
| 霰弹（一发 N 颗） | `N × fps × 寿命` | ≈ 3,000（N=8） | 126 KB | ⚠️ **CPU** |
| 分裂（每 T 秒裂成 2） | `fps × 寿命 × 2^(寿命/T)` | ≈ 200 万（T=0.3s） | 56 MB | ⚠️⚠️ **CPU 早就死了** |

**⑦ ⚠️ 第一个撞到的墙是 CPU 的 O(B²)，不是内存。**
`World::Find(id)` 是**线性扫**（`World.h:43`），而"对每发子弹调一次 `Find`" ⇒ O(B²)：

| B | 每帧比较次数 ≈ 2B² | 量级 | 60fps 预算 16.7 ms |
| --- | --- | --- | --- |
| 378 | 286 k | ≈ 0.3 ms | ✅ 2% |
| 3,000 | 18 M | ≈ **18 ms** | ❌ 一帧就超预算 |
| 20,000 | 800 M | ≈ 0.8 s | ❌ 卡死 |

⇒ **要设计"指数增殖"的武器，必须先定数量上限（由 CPU 定，不是内存）**；出口有两条：
**(a) 加硬 cap**（+ 分裂代数上限）；**(b) 把 O(B²) 降成 O(n)** —— 一次遍历 `m_entities` 挑出子弹即可，
但那需要"一眼认出哪个是子弹"，也就是给 `Entity` 加一个**吃 2 字节 padding 的 `kind` 字段**
（`sizeof(Entity)` 仍是 24，`static_assert` 不会响）。

**⑧ ⛔ 加 cap 时的禁令：绝不许用"回收 id 再复用"的对象池。**
`EntityId` 的核心契约是"**id 永不复用**"（`Entity.h` 的注释）—— 它保证"悬空 id"只表现为
**找不到**（`Find` 返回 `nullptr`），而**不会静默指向另一个实体**。对象池复用 id 会把这个最难查的 bug 请回来。
正确做法只有两种：**拒绝生成**，或**回收最老的（id 依然不复用）**。

**待办去向**：`TECH_DEBT` **C19**（无上限 → S10 刷怪时定 cap）· **C20**（`Find` 的 O(B²)）。

---

## B. 开发路线（15 个阶段）

时间预算：**每阶段 30 分钟 ~ 1.5 小时**，任何超过 2 小时还没看到画面的阶段都说明拆得不够细。

### 阶段依赖图

```text
S0 骨架 ──► S1 FrameBuffer 出画面 ──► S2 输入 ──► S3 GameLoop/Time ──► S4 Math+测试
                                                                          │
                        ┌─────────────────────────────────────────────────┘
                        ▼
                    S5 World/Entity 移动 ──► S6 Camera ──► S7 碰撞 ──► S8 AI ──► S9 射击 ──► S10 HUD/Wave
                                                                                              │
                                        ┌─────────────────────────────────────────────────────┘
                                        ▼
                    S11 Debug/Profiler ──► S12 Ray Casting 伪3D ──► S13 Sprite/Z-buffer ──► S14 打磨
```

---

### S0 骨架与构建基线

| 项 | 内容 |
| --- | --- |
| 目标 | `cmake --build build --config Debug` 通过，`ctest` 有 1 个用例是绿的 |
| 实现 | 顶层 CMakeLists、`engine` 静态库、`tests`、自制 `MiniTest.h`、`.gitignore`、`CMakePresets.json` |
| C++ 知识 | 现代 CMake target model、`INTERFACE` 库传编译选项、CTest、多配置生成器 |
| 引擎知识 | 库/可执行/测试三者的链接关系 |
| 规模 | ~120 行（几乎全是 CMake） |
| 可运行效果 | 运行 exe 打印一行版本号；`ctest` 1/1 passed |
| 验收 | 上面两条命令 + `ctest --test-dir build -C Debug` 全绿；`git commit` 完成 |
| 坑 | 漏 `/utf-8`（家机实测：源码里有中文注释时直接 `error C4716: 必须返回一个值`，构建过不去，见 E-1）；漏 `--config` 会报"没有可用配置"；额外设 `CMAKE_RUNTIME_OUTPUT_DIRECTORY_<CFG>` 会让 exe 少掉 Debug 这一层目录（见 C.1） |

**实际结构（2026-09-24 两台机器各自实测，与 C.1/C.2 草案的差异以这里为准）**

| 项 | 实际值 |
| --- | --- |
| target | `MiniEngine`（exe）、`mini_tests`（测试 exe）、`engine`（STATIC）+ 别名 `MiniEngine::engine`、`mini_options`（INTERFACE，只装编译选项） |
| 产物路径 | `build/bin/Debug/MiniEngine.exe`、`build/bin/Debug/mini_tests.exe`；静态库在 `build/engine/Debug/engine.lib` |
| CTest | **1 个条目** `mini_tests`（`ctest` 报 1/1），内部是自制框架的 **3 个用例**（suite：`Core` / `Build` / `TestHarness`），`mini_tests --list` 可列、`mini_tests <suite>` 可单跑 |
| 源文件 | 引擎侧 S0 只有 `engine/src/core/Version.cpp`——**C.2 草案里的 `FrameBuffer.cpp` / `ConsoleWindows.cpp` 是阶段 1 结束时的样子，S0 不该有** |
| 测试框架 | 接口按 `doc/MINITEST_SPEC.md` 落地：`MINI_TEST(suite, name)` + `MINI_CHECK` / `MINI_CHECK_EQ` / `MINI_CHECK_NEAR` / `MINI_FAIL`；注册表与 runner 在 `tests/test_main.cpp`，用例在 `tests/test_smoke.cpp` |
| 验收记录 | 配置通过、构建 **0 warning**、`ctest` 1/1、exe 打印 `MiniEngine 0.1.0-dev`；反向验证（断言故意失败、0 用例）都实跑过 |

> **2026-09-29 收口**：原先这里记着"`ms-clang-debug` 与 `ms-asan` 两个预设尚未实测"。
> 现在 —— `ms-clang-debug` 已**删除**（决定不用 clang）；`ms-asan` 已改成**继承 MSVC 预设**，
> 并把 `MINI_SANITIZE` 那条路收成 MSVC 一路（补了缺失的链接选项与 `/RTC-`）。
> 详见 `doc/TECH_DEBT.md` A4 / A5。

### S1 FrameBuffer —— 屏幕上出现 `@`

| 项 | 内容 |
| --- | --- |
| 目标 | 一个 80×25 的 FrameBuffer，`Clear/Draw/Present` 三步出一个带边框的静态画面 |
| 实现 | `Cell{char ch; Color fg; Color bg;}`、`FrameBuffer`（`std::vector<Cell>`）、`Present()` 一次写出；初始化时切 UTF-8 + 打开 VT 模式 |
| C++ 知识 | `vector` 索引线性化（`y*w+x`）、`string::reserve`、`ostream::write`、`std::string_view` |
| 引擎知识 | 后缓冲/前缓冲、Present、"绘制与提交分离" |
| 规模 | ~200 行 |
| 可运行效果 | 终端出现带边框的盒子，里面一个 `@`；光标消失；Ctrl+C 后终端恢复正常 |
| 验收 | 连续运行 10 秒不闪烁；`echo "退出后终端正常"`；画面内容与代码里的坐标一致 |
| 坑 | 代码页 936 必须切 UTF-8；老 conhost 对 VT 支持不全（用 Windows Terminal）；退出时必须 `ESC[?25h` 恢复光标 |

**S1 落地记录（2026-09-24 公司机，含 5 条接口决策）**

| # | 决策 | 落成 | 代价 / 备注 |
| --- | --- | --- | --- |
| 1 | 颜色按 256 来 | `enum class Color : std::uint8_t`；`0~254` = ANSI 256 索引，**`0xFF` = Default 哨兵**（输出 `ESC[39m`/`ESC[49m`）；另给 16 个基础色命名 | 256 色里 **255 号（最亮灰阶）用不了**。真要用时底层换 `uint16_t`，改动只在 `Color.h` 与 `Serialize` 的两处色码 |
| 2 | 尽量单字节 | `Cell{ char ch; Color fg; Color bg; }` → `sizeof == 3`，用 `static_assert` 钉死；边框走 ASCII `+ - |` | **画不了中文 / UTF-8 制表符**。S13 想画 `█▓▒░` 时把 `ch` 换 `char32_t`（Cell 变 8 字节），使用点只有 Set/Draw/Serialize |
| 3 | Present 只负责 IO | `FrameBuffer::Serialize()` 产字符串（纯数据、可单测），`Present(fb)` 才碰终端 | 多一层调用，换来 FrameBuffer 能被完整离线测试 |
| 4 | 终端模式 S1 临时放 render | `engine/src/render/TerminalPresenter.cpp`，全项目**唯一** include `<windows.h>` 的文件；**S2 搬到 `platform/`** | 有一小段"注定要搬家"的代码，头文件里写了 ⚠️ TODO |
| 5 | Ctrl+C 装 handler | `SetConsoleCtrlHandler`，handler 内只用 `WriteConsoleA`（异步安全）；第一次 Ctrl+C → 还原终端 + 让 main 正常退出，第二次 → 放行让系统直接杀 | — |

**实际结构**

| 项 | 实际值 |
| --- | --- |
| 新文件 | `engine/include/mini/render/{Color.h, Cell.h, FrameBuffer.h, TerminalPresenter.h}`、`engine/src/render/{FrameBuffer.cpp, TerminalPresenter.cpp}`、`tests/test_frame_buffer.cpp` |
| 单测 | `FrameBuffer.*` 6 个用例（**不需要终端**）：Cell 3 字节、越界写忽略、索引线性化、DrawRect 裁剪、越界读、Serialize 颜色压缩 |
| `ctest` | 仍是 **1 个条目**（`mini_tests`），内部用例 3 → **9** |
| 帧数据（实测） | 每帧 2411 字节 / 51 个 SGR（2000 格）；25 行 × 80 列；`@` 在 (1,1)、`#` 在 (78,23)、内框角在 (10,5)；逐帧字节完全一致 |

**S1 新踩到的坑（S0 坑表之外）**

| 现象 | 原因与对策 |
| --- | --- |
| 帧数据里行尾是 `\r\r\n` | MSVC 的 `std::cout` 是**文本模式**，会把显式写的 `"\r\n"` 再翻译成 `\r\r\n`（屏幕上无害，但字节不再精确、每行白扔 1 字节）。对策：`BeginTerminal` 里 `_setmode(_fileno(stdout), _O_BINARY)`，退出时还原 |
| 每帧实际 ~37ms（写死 600 帧 = 15 秒以上） | 序列化 + 写终端 + `sleep_for` 的开销叠加。对策：阶段 1 改成按**真实时间**（`steady_clock`）驱动 10 秒，不写死帧数 |
| 断言里写 `find("ABC")` 会失败 | 同一行里 `A`/`B` 是红色而 `C` 回默认色，中间必然夹着 `ESC[39m`。断言要按"色彩段"写（`find("AB")` + `find("\x1b[39mC")`）—— 这条是 S1 第一次跑测试就撞到的，说明测试确实在干活 |
| 老 conhost 上 VT 可能部分失效 | 运行前置：用 Windows Terminal。先 `echo "$([char]27)[38;5;196mRED"` 验证终端本身支持 |

### S2 Platform 抽象层 + 键盘输入

| 项 | 内容 |
| --- | --- |
| 目标 | `platform::Console` + `platform::Input`，`@` 能响应按键移动（先用瞬时移动验证输入通路） |
| 实现 | `platform::Console` / `platform::Input` 是**抽象基类 + 工厂**（`MakeConsole()` / `MakeInput()`），Windows 实现全藏在 `.cpp` 里；用 `ReadConsoleInputW` 轮询事件，关闭 `ENABLE_QUICK_EDIT_MODE`（须**同时**置 `ENABLE_EXTENDED_FLAGS`）并打开 `ENABLE_WINDOW_INPUT` 才能收到 `FOCUS_EVENT`；`KeyCode` 枚举；**四态** `Up/Pressed/Held/Released`。**不写 Linux 分支**（`AGENTS.md` 约定：只做 Windows） |
| C++ 知识 | 抽象接口、`std::unique_ptr` + 工厂函数、RAII（构造开终端模式，析构恢复）、`enum class`、位标志 |
| 引擎知识 | 输入轮询（poll）模型 vs 回调模型、按键边沿与持续状态 |
| 规模 | 净新增约 300 行，另有 165 行搬迁（S1 的 `TerminalPresenter.cpp` 归位到 `platform/`）；仍然只有 Windows 一条路径。比原估多出的部分是"可离线单测的状态机"与"抽象基类 + 工厂"两笔包装 |
| 可运行效果 | 三行 HUD 实时显示 15 个键的四态（`W: Held  SPACE: Pressed  Q: Released`，Pressed/Released 各只存在一帧，靠颜色分辨）；W/A/S/D 与方向键让 `@` **每次按下走一格**；按 Q 退出 |
| 验收 | 长按 W 显示 Held；松开显示 Released 仅一帧；**鼠标在终端里点一下之后输入仍然正常**；切走窗口 2 秒再切回无任何键卡在 Held；`MiniEngine.exe > out.txt` 抛异常 + 人话提示，**不是**静默无画面 |
| 坑 | ① 必须 `&~ENABLE_QUICK_EDIT_MODE` 并配合 `ENABLE_EXTENDED_FLAGS`，否则点一下控制台就"卡死"；② `WINDOW_BUFFER_SIZE_EVENT` 在 S2 **只安全丢弃**（自适应画面归 S3/S6，见 E-12）；③ 退出路径要恢复终端状态（显示光标、还原代码页），否则一次崩溃就让终端一直不正常；④ **一帧内 down+up 的极速按键**必须用 `m_pulse` 兜住，否则会静默漏键；⑤ 未映射的 VK 必须丢弃，否则多个键一起写 `Unknown` 槽位互相干扰 |

> **S2 详细施工图见 `doc/S2_CHECKLIST.md`** —— 接口签名与四态判定表、16 条坑表、7 个离线测试用例、正向 8 条 + 反向 4 条验收、五个 commit 的切分与勾选进度。
> 本节只留阶段定义与决策，实现清单不在两处重复，避免改了一处漏另一处。

**S2 落地记录（2026-09-28 公司机，含 6 条与原文不同的决策）**

| # | 决策 | 落成 | 代价 / 备注 |
| --- | --- | --- | --- |
| 1 | 平台层用**抽象基类 + 工厂** | `Console` / `Input` 是抽象基类，Windows 实现全在 `.cpp`；`MakeConsole()` / `MakeInput()` 创建 | 换"公共头零 Windows 痕迹"。**PLAN S2 正文原先写的 `IConsole` 作废** |
| 2 | 按键**四态** | `Up / Pressed / Held / Released`；`Up` = 一直没按，`Released` = 本帧刚抬起 | 三态无法区分这两者，S2 验收要求的"松开仅一帧"就做不到。**C.3 原三态作废** |
| 3 | 窗口尺寸事件**只消费不处理** | `Poll()` 里 `case WINDOW_BUFFER_SIZE_EVENT:` 直接丢，注释指向 S3 | S3 每帧比较 `Console::Size()` 更简单、不会漏，不需要这个事件（E-12 的触发阶段随之从 S2/S6 改成 S3/S6） |
| 4 | `QuitRequested()` **不含 ESC** | 只反映 Ctrl+C / 关窗口 | ESC 从 S10 起要用来关菜单，引擎硬编码它退出到时必然要拆。**C.3 原句"Ctrl+C / ESC / 窗口关闭"作废** |
| 5 | 进程级退出标志放在**公共头** `Console.h` 的自由函数里 | `platform::QuitRequested()`；`Input::QuitRequested()` 转发给它 | 原计划是内部头 `src/platform/QuitFlag.h`，但 `main` 在步骤 1 就需要读它（那时 Input 还没写），做成公共自由函数反而更少一个文件 |
| 6 | 未映射 VK 的丢弃点放在 `InputState::OnKeyDown` | `KeyIndex()` 对 `Unknown`/`Count` 返回哨兵，写入被拒 | 单一收口，**可离线单测**（`KeyCode.InvalidKeyIndexIsIgnored`）。原计划写在 `Poll()` 里，那样测不到 |

**实际结构**

| 项 | 实际值 |
| --- | --- |
| 新文件 | `engine/include/mini/platform/{Console.h, KeyCode.h, InputState.h, Input.h}`、`engine/src/platform/{ConsoleWindows.cpp, InputWindows.cpp}`、`tests/test_input_state.cpp` |
| 删除 | `engine/include/mini/render/TerminalPresenter.h`、`engine/src/render/TerminalPresenter.cpp`（S1 的临时房客，到期搬走） |
| 单测 | `ctest` 仍是 **1 个条目**（`mini_tests`），内部用例 9 → **16**（新增 suite：`InputState` 5 个、`KeyCode` 2 个） |
| 自动验收 | 构建 **0 warning**；`ctest` 1/1；`mini_tests` 16 passed；`MiniEngine.exe > out.txt` 抛 `runtime_error` + exit 1（**没进备用屏**）；反向验证第 3 条（删 `m_pulse` → 测试变红）已实跑并还原现场 |
| 交互验收 | ✅ **2026-09-28 真机 Windows Terminal 确认通过**：9.1 的 9 条现象（三行 HUD、`Held`、`Released` 闪一帧、`Pressed`、Q 干净退出、拖选后仍响应、切窗口不卡键、改尺寸不崩、`@` 每按一次走一格且按住不连走），详表见 `doc/S2_CHECKLIST.md` 9.1 |

**S2 新踩到的坑（S0/S1 坑表之外）**

| 现象 | 原因与对策 |
| --- | --- |
| 删掉 `m_pulse` 分支后，**`Pressed` 整个消失**（不只是极速点击丢键） | 一开始以为 `m_pulse` 只救"一帧内 down+up"。实际上正常按下（`prev=false, now=true`）也走 `if (m_pulse)` 这一支，因为 `was && now` 判不成立。所以 `m_pulse` 是 `Pressed` 的**必要条件**，不是边角补丁 —— 反向验证时这个用例比预期多红了一个 |
| 长按不走飞，正好是判定顺序带来的 | 自动重复会不断把 `m_pulse` 置回 true，但 `Get()` 里 `was && now`（Held）排在 `pulse` 前面，所以按住不放**不会**被打回 `Pressed`。把顺序换掉，按住 W 会让 `@` 一直走 |
| `KeyFromVirtualKey` 的参数必须是 16 位 | `wVirtualKeyCode` 是 WORD。先截成 `uint8_t` 再查表，会把高位非零的码**悄悄折到别的键**上（`0x0100` → 查到 `0x00`）。改成 `uint16_t` + 范围判断 |
| `winpty` 驱动不了这个程序做自动验收 | `winpty` 拒绝把管道当 tty（报 `stdin is not a tty`），所以"喂一个 q 进去看它退不退"这条自动化路子走不通。**结论：交互验收只能手工做** —— 这也正是把四态状态机拆成 `InputState` 的价值：能自动测的部分已经全部自动测了 |
| **关窗口会留下锁住 exe 的孤儿进程**（`LNK1168`，构建直接失败）—— **已在 S3 修复 ✅** | 不用 Q 退出而是直接关控制台窗口时，父 shell 被杀、子进程成孤儿，控制台也没了 → 它**永远等不到 `CTRL_CLOSE_EVENT`**，一直对着死控制台 `Present()`。根因是 `Present()` 从不检查 `std::cout` 写是否成功（控制台死后只是静静置 `failbit`）。**兜底**（只在老版本上还需要）：`taskkill /F /IM MiniEngine.exe`。<br>**S3 的修法**：`Present()` 返回 `bool` + `Application::RequestQuit()`。**但不能只查写失败** —— 脏行 diff 让"内容没变的帧一个字节都不发"，**没写就没有失败信号**，所以每帧先用 `GetConsoleMode` 判终端存活（`S3_CHECKLIST` 坑 #29）。<br>**正确的验证方式**：关窗口会发 `CTRL_CLOSE_EVENT`，那从来不是这个 bug；要"给它一个独立控制台 + 杀掉属于它的 `conhost.exe`"，recipe 见 `S3_CHECKLIST` 10.2 第 7 条 |
| 诊断时最容易被带偏的一条 | **"能改名"不等于"没被占用"**：Windows 加载器允许 rename 一个正在运行的 exe，只禁止 write。所以判断文件被占用要**试着写**（重链接一次），不是试着改名 |

### S3 Game Loop / Time / FPS（✅ 2026-09-28 完成）

| 项 | 内容 |
| --- | --- |
| 目标 | `Application` 类 + 主循环 + 顶栏实时 FPS/FrameTime |
| 实现 | `Time::BeginFrame/EndFrame/LimitFPS`（`steady_clock`）、滑动窗口 FPS、`platform::Wait` 高精度限帧、`render::RowDiff` 脏行 diff 提交 |
| C++ 知识 | `std::chrono`、`duration_cast`、RAII 计时、静态局部变量、聚合体初始化、`[[nodiscard]]` |
| 引擎知识 | variable dt vs fixed dt、dt clamp、帧率限制、为什么"限帧"和"垂直同步"不是一回事、脏行 diff 与"没写就没有失败信号" |
| 规模 | 实际净新增约 **500 行** + 两组测试 **230 行**（原估 450 行；多出来的是**高精度计时器** `platform::Wait` 与两个守卫用例） |
| 可运行效果 | 顶栏 `FPS  60.0 \| Frame  16.7ms \| Max  16.7ms \| Work  0.3ms \| Rows  1/24`（`Max` 那一格是 **2026-09-29 B3 补的**，S3 当时没有）；`MiniEngine.exe 300` 能看到 FPS 上升而 `Work` 不变；拖窗口画面跟手重排 |
| 验收 | 限帧 60 时 FPS 落在 58~61；拖大/拖小窗口无残影且**不上滚**；**控制台被销毁后进程自己退出**（S2 遗留问题，见下方注明）；**`Max` 静止时不再增长**（B3 恢复的验收项，见下方） |
| 坑 | **Windows 上 `Sleep(1)` 实际睡 ~15.6ms**，必须混合睡眠（先睡到差 `spin`，再自旋）。**`spin = min(2ms, targetFrame/4)`，写死 2ms 会在高帧率下把 CPU 烧穿**。⚠️ **2026-09-28 写步骤 1 时实测推翻了"混合睡眠就够"这个前提**：粒度是调度器 tick **15.625ms**，而 60fps 目标帧时间 16.667ms 只差 1.04ms → 用 `sleep_for` 会**每 15 帧左右睡过头一整跳**（`Sleep(16)` 实测 31.26ms），FPS 在 40~60 抖。**必须用 `platform::Wait`（高精度可等待计时器，精度 <1ms，不需链额外库）**，见 S3 决策 8 |
| 设计结论 | **用 variable dt + `dt = min(dt, 0.05)`**。本项目没有需要固定步长的物理（无重力、无弹性），引入 fixed update 只会让输入手感变差。等 S9 子弹高速穿透问题真的出现时，再考虑"子弹用子步进"这种局部方案，而不是全引擎改结构 |

> **验收里那一条要按实际情况理解**：原写"直接关控制台窗口进程立刻消失"，
> 但关窗口会发 `CTRL_CLOSE_EVENT`，S2 的 handler 早就处理了 —— **它从来不是那个 bug**。
> 真正的坑是"**控制台被销毁但事件没送到**"。所以验证方式改成：
> 给它一个独立控制台，然后杀掉属于它的 `conhost.exe`，进程应**在 3 秒内自己退出**。
> recipe 在 `doc/S3_CHECKLIST.md` 10.2 第 7 条（2026-09-28 已跑通 ✅）。

> **`Max` 那一格：一条被静默删掉的验收项，2026-09-29（B3）补回来了。**
>
> 原文在 S3 验收里写过"**`MaxFrameMs()` 长时间运行不出现尖峰**"。收口时我把它**悄悄删了**——
> 因为顶栏根本没显示 `Max`，这条**没法验**。这是我的疏漏（记在 `doc/TECH_DEBT.md` B3）。
> 现在顶栏加了这一格，它变成可验的，**但预期的写法要改**：
>
> | | |
> | --- | --- |
> | 操作 | 启动后**不做任何事**，挂 30 秒 |
> | 必须看到 | `Max` 的读数**完全不再变化**（这是"长时间运行不出现尖峰"的可验形式） |
> | 然后 | **拖一次窗口** → `Max` **跳一次**（resize 那一帧要重建 `vector` + 全量重画），**之后又停住** |
> | 参考区间 | 静止值大致落在 **17~25ms**。**不要强求 <20ms** —— 见下 |
>
> **为什么把界放宽到 25ms**（这是我这次才想清楚的）：
> `Max` 是**自启动以来**的最差值，**没有任何代码会重置它**（`FrameStats::Reset()` 只有测试在调）。
> 而启动那一两帧要做的事特别多（控制台初始化 + 第一次全量重绘整屏），
> 那一次的开销会被**永久烙进 `Max`**，之后无论跑多久都不会降下来。
> 所以正确的验收是"**停住不动**"，不是"小于某个数"。
> 这正好呼应坑 #24：resize 的尖峰**本来就该被记下来，别当 bug 去消**。

> **从 S2 带过来的一件事 —— 已在 S3 收口 ✅**：
> `Console::Present()` 现在返回 `[[nodiscard]] bool`（控制台已死时返回 false），
> 由 `Application::RequestQuit()` 统一收口。**注意实现比原计划多了一层**：
> 只查写失败不够，因为脏行 diff 让"内容没变的帧一个字节都不发"——
> **没写就没有失败信号**，所以每帧先用 `GetConsoleMode` 判终端存活。见 `S3_CHECKLIST` 坑 #29。
> 背景见 S2 坑表最后两条与 `doc/S2_CHECKLIST.md` 8.5。

**S3 方案定稿（2026-09-28，四条决策）** —— 完整施工图见 **`doc/S3_CHECKLIST.md`**

| # | 决策 | 落成 | 代价 / 备注 |
| --- | --- | --- | --- |
| 1 | **脏行 diff 做，且抽成可单测的 `render::RowDiff`** | `RowDiff.h/.cpp` + `FrameBuffer::SerializeRow()` + 8 个离线用例 | 多一对文件 + 约 100 行，换来"省了多少行/字节"**可自动验证**，并关掉 E-3。代价：它活在一个前提上（同一行两次序列化必须逐字节相同），所以必须有专门守这条的用例 |
| 2 | **`DemoGame` 放 `main.cpp` 的匿名 namespace** | `main.cpp` 240 → 约 120 行 | 仍超过 A.2 写的"<20 行"；**不提前建 `game/` 静态库**（PLAN 与顶层 CMakeLists 都定了 S5 才建） |
| 3 | **resize 做：检测 + `FrameBuffer::Resize` + 一次 `ESC[2J` + 全量重画** | `Run` 第 ③ 步 | 相机边界 clamp 仍归 S6。代价：多约 20 行，且"最后一行/最后一列会不会滚屏"必须肉眼实测 |
| 4 | 新建 **`doc/S3_CHECKLIST.md`** | — | 与 S0/S2 同格式。S3 跨 5 个 commit 且要改 S2 的两个接口，比 S1 更值得单独存档 |
| 5 | **限帧用高精度可等待计时器**，封成 `platform::Wait()` | `platform/Wait.h` + `src/platform/WaitWindows.cpp`；`Time::LimitFPS` 不用 `sleep_for` | **写步骤 1 时实测发现的**（不是方案阶段想到的）：本机 `Sleep` 粒度 15.625ms vs 60fps 目标帧时间 16.667ms，只差 1.04ms → 每 15 帧睡过头一整跳。高精度计时器**不需链额外库**，也不用 `timeBeginPeriod`（那个会改全系统定时器精度）。`Wait(7ms)` 实测 7.05~7.56ms |

**实现进度（→ 2026-09-28：S3 完成）**

`S3 已完成并通过验收`（本人构建 + 运行确认）。6 个 commit：

| # | commit | 内容 |
| --- | --- | --- |
| 1 | `17f3059` | `core::Time` + `FrameStats`（dt clamp + 滑动窗口 FPS） |
| 1b | `9f0a375` | `platform::Wait` 高精度等待（决策 8 的产物，见上表第 5 条） |
| 2 | `dce6b5e` | `render::RowDiff` + `FrameBuffer::SerializeRow` |
| 3 | `a6deebb` | `Console::Present` 返回终端存活，内部接入 `RowDiff` |
| 4 | `1d45702` | `core::Application` + `IGame`，`main` 退化为 `DemoGame` |
| 5 | （本次收口） | 文档同步：本节的落地记录 + `S2_CHECKLIST` 8.5 标已修 + `AGENTS.md` 1.4 |

**可自动验证的数据**：用例 16 → **34 passed**，`ctest` 1/1，构建 0 warning；
脏行帧比全量帧省 **10.5 倍**字节（80×25 实测 2059 B → 197 B）。

**实测与本节原估不一致的三处**（都是落地后才知道的，别按原文理解）：

| 项 | 原文估 | 实际 |
| --- | --- | --- |
| 顶栏格式 | `FPS: 59.9  Frame: 16.7ms  Work: 0.3ms  Rows: 2/25` | `FPS  60.0 \| Frame  16.7ms \| Work  0.3ms \| Rows  1/24`（字段用 `\|` 分隔；**总行数是画布行数 24**，因为第 0 行被顶栏占掉） |
| **顶栏加 `Max`**（2026-09-29 B3） | 原计划没有这一格 | 现在是 `FPS \| Frame \| **Max** \| Work \| Rows`。`Max` 紧挨 `Frame`（**同一个口径**：都是未 clamp 的帧间隔）。**动机**：`MaxFrameMs()` 一直只有测试在调，而"长时间运行不出现尖峰"那条验收因为顶栏看不到 `Max` 而**无法验**，收口时被我静默删了 —— 加这一格把那条验收恢复成**可验**的形式（见上文 S3 节的说明） |
| 规模 | 净新增约 450 行 | 约 **500 行** + 两组测试 ~230 行（多出的是高精度计时器与两个守卫用例） |
| 退出方式 | "直接关控制台窗口进程立刻消失" | 关窗口本来就能退（`CTRL_CLOSE_EVENT` 早就处理了）。**真正的坑是"控制台被销毁但事件没送到"**，验证要用"杀掉属于它的 `conhost.exe`"，见 `S3_CHECKLIST` 10.2 第 7 条 |

**S3 没做完的两件小事**（不影响结项，留给需要时补）：
`S3_CHECKLIST` 10.3 的反向验证 1/2/4（改代码才能验，要 FPS 顶栏观察）；
`mini_tests.exe Nope` 那道 S0 门闩的复验。

**分工约定（2026-09-28 追加，已固化进 `AGENTS.md` 1.4）**：
**编译与运行由本人执行，AI 只做代码编辑** —— 所以本节的"已通过"都是本人回报的结果，
AI 不会自己跑构建或 exe。一个步骤的 commit 由本人在验证通过后决定何时打。

决策表**之外**还有三处与 C.3/C.4 草案不同，已直接改进原文：`FrameContext` 去掉 `Time&`、`IGame` 从 `unique_ptr` 改成引用、
顶栏由 `Application` 在 `game.OnRender` **之后**画。


### S4 Math 库 + 单元测试（✅ 2026-09-29 完成）

| 项 | 内容 |
| --- | --- |
| 目标 | `Vec2/Vec3/Rect/AABB/Ray` 全 `constexpr` 头文件 + 测试覆盖 → **实测落 4 个文件**（`Vec3`/`Rect` 只留契约，见 S4_CHECKLIST 差异 2） |
| 实现 | 运算符重载、`Length/LengthSq/Normalized`（零向量 + NaN 保护）、`Clamp/Lerp/DegToRad/Sign/ApproxEquals/FloorToInt/RoundToInt`、`AABB`（`Contains` **闭**区间 / `Intersects` **开**区间 / `FromCorners` 自动排序 / `IsValid`）、`Ray::IntersectsAABB`（slab 法 + **显式 `d == 0` 分支**） |
| C++ 知识 | `constexpr` **以及它的边界**（哪些标准库函数不是 constexpr：`sqrt`/`floor`/`abs(float)`；判断"值不值得保"见 S4_CHECKLIST 第 8 节原则 5）、运算符重载、`[[nodiscard]]`、值语义、`std::optional` 当返回值 |
| 引擎知识 | 为什么要 `LengthSq`（避免 sqrt）、AABB 的 slab 法、法线方向、**"查询用闭区间 / 碰撞用开区间"** |
| 规模 | ~300 行 + ~200 行测试 → **实测：4 个头文件 517 行 + `test_math.cpp` 663 行**（33 个用例，约估值的 2.5 倍） |
| 可运行效果 | ctest 全绿；可选 demo 打印"两个盒子相交/不相交" → **不做 demo**（决策 3）。改为 4 个 suite 全绿：`MathUtils` 8 / `Vec2` 9 / `AABB` 7 / `Ray` 9；用例总数 35 → **68** |
| 验收 | ✅ 覆盖齐，且**反向验证 3 条都实测看到红**（`doc/S4_CHECKLIST.md` 10.3）：零向量归一化保护、NaN 输入、AABB 边界相接（`<=` vs `<` 已定为**开区间**）、Ray 起点在盒内 → t=0 |
| 坑 | ① 归一化零向量产生 NaN 并污染整个物理循环 → 用 `!(len2 > 0.0f)` 挡住（`len2 <= 0` **挡不住 NaN**），反向验证已确认；② 浮点相等判断 → 全项目禁 `==`，一律 `ApproxEquals` / `MINI_CHECK_NEAR`；③ **贴边算不算相交** → 用 `<`（用 `<=` 的话贴墙实体每帧被判碰撞、被推开一格）；④ **`Ray` 的 `d == 0` 必须显式分支** → `1/d` 写法在平行输入上会漏出 `t = +inf`，且常量表达式里**直接 C2131 编不过**；⑤ **`MINI_CHECK(Vec2{1,2}...)` 编译不过** → 函数宏按顶层逗号拆参数、**花括号不保护逗号**，已把 `MINI_CHECK` 改成可变参数（`MINITEST_SPEC` 第 1 节） |

### S5 World / Entity —— `@` 可以移动（✅ 2026-09-30 完成）

| 项 | 内容 |
| --- | --- |
| 目标 | `TileMap`(200×200) + 简单 `Entity` 结构体 + `World` 容器 + 移动逻辑 |
| 实现 | `struct EntityId`（`uint32` **值句柄**，0 = 无效、永不复用）；`struct Entity { EntityId id; Transform transform; Velocity velocity; char symbol; bool active; }`；`World` 持有 `std::vector<Entity>` + **延迟删除**（`MarkForRemoval` / `FlushRemovals`，swap-erase）；`TileMap` 越界 `At` → `Wall`。**游戏侧第一次独立成 `game/` 静态库**（`game::Game : mini::core::IGame`，`main.cpp` 只留 argv + 造对象 + `Run`）→ 实测 `Entity.h` `TileMap.h/.cpp` `World.h/.cpp` 5 个文件、`game/` 3 个文件 |
| C++ 知识 | 聚合初始化、`std::span`（**非拥有视图**：任何 `Spawn` 之后立即失效）、引用 vs 值语义、`erase-remove` vs `swap-erase`、默认化的 `friend operator==`、`static_assert` 钉结构体布局 |
| 引擎知识 | 实体标识（id vs 指针 vs 索引 → **选值句柄**：可存、可比较、不随容器搬迁失效）、迭代顺序、更新顺序的重要性、**"登记删除 ≠ 立即失效"**（`Find` 还找得到、`Update` 照样积分，直到 `FlushRemovals`） |
| 规模 | ~300 行 → **实测：engine 侧 ~430 行 + game 侧 ~340 行 + 23 条新用例（用例总数 68 → 91）** |
| 可运行效果 | `@` **按住** WASD 连续移动（速度 × dt，默认 20 格/秒，可用 `MiniEngine.exe 60 30` 覆盖）；HUD 显示 `pos / tile / sum dt / v`；地图 = 外沿墙 + **每 10 格一根的规则柱阵** + 一个不对称地标 |
| 验收 | 100 FPS 和 300 FPS 下走过同一段距离耗时相同 → 机器形式是 `World.MovementIsDtIndependent`（1×1.0s vs 100×0.01s）与 `Game` 的 3 条用例；**4 条反向验证全部实测看到红**（`doc/S5_CHECKLIST.md` 10.3.1）。⚠️ HUD 那条自证 `pos ≈ v × sum dt` **只在提供真 key-up 的终端里成立**（VS Code/WT 是 ConPTY，见 `TECH_DEBT` C10） |
| 坑 | ① 遍历 `vector` 时删除元素导致迭代器失效（S9 会撞上，这里先埋好概念）→ **S5 就落地了 `MarkForRemoval`/`FlushRemovals`**，S9 直接复用；② `ToIndex` 必须 `floor`（`(int)` 截断会让负数指到上一格）；③ 斜向必须 `Normalized()`（否则快 √2 倍）；④ **`minitest` 里裸写类型名会找不到**（`Game` 在 `game` 命名空间里，测试文件要 `using game::Game;`）；⑤ 帧序：**写速度必须在 `Update` 之前** |
| 落地偏差 | 13 条，都记在 `doc/S5_CHECKLIST.md`（布局"没规律"、手感"必须按住"、ConPTY 无 key-up 三条是**跑起来之后**才发现并处置的） |


### S6 Camera / Viewport —— 世界大于屏幕（✅ 2026-10-02 完成）

| 项 | 内容 |
| --- | --- |
| 目标 | World→Camera→Screen 转换，Camera 跟随 + 边界夹取 |
| 实现 | `render/Camera.h`（**header-only，118 行**）：`struct CameraLimits{viewW,viewH,mapW,mapH}` + `class Camera{ Vec2 m_pos; }`；`Follow()` 逐轴 `clamp(world − view/2, 0, map − view)`（**snap，无平滑**）；`OriginCol/OriginRow()` = `floor(pos)`（渲染用的整数原点）；`Pos()`；`ScreenToWorld` **只留契约不实现**（0 调用者 → `TECH_DEBT` C11）。`game` 侧两处接入：`OnUpdate` 末尾 `Follow`（**在 `World::Update` 之后**）+ `DrawMap`/`DrawEntities` 改用整数原点。**不做 `target` 字段、不建 `Viewport.h`** |
| C++ 知识 | `clamp`、`[[nodiscard]]`、整数/浮点边界计算、`static_assert` 钉布局、**"同一个事实只能有一个来源"**（相机不持有尺寸，尺寸每帧由调用方给） |
| 引擎知识 | 视口、滚动、边界处理、"只画看得见的"（剔除）、**取整路径必须唯一**（两条各自漂移就是 E-8） |
| 规模 | ~250 行 → **实测：`Camera.h` 118 行 + `tests/test_camera.cpp` 301 行（14 条用例）+ `game/`·`tests/` 改动约 +110/−28 行（含大量注释）；用例总数 90 → **104**，C7 收窄时再 +1 → **105** |
| 可运行效果 | **走进第 41 格画面开始滚动**，`@` 恒定在画面正中（第 40 列）；走到地图右下角镜头被顶住、**不出现错位的墙**；继续走**出地图**（x ≥ 200 / y ≥ 200）`@` 才离开画面 —— S7 之前能做到，属预期 |
| 验收 | 机器形式：`Camera` suite **14 条**（含全图扫描 `PlayerStaysOnScreenInsideMap`、E-8 守卫 `PlayerColumnMatchesItsTileColumn`、退化 `ZeroViewportIsSafe`、resize `ResizeThenFollowUsesNewViewport`）；**2026-10-02 本人回报测试全绿**；**2026-10-08 本机复验 105 passed（0 warning / `ctest` 1/1）**。人眼 7 条见 `doc/S6_CHECKLIST.md` 10.2 —— ✅ **本人 2026-10-02 确认"可以了"**。⚠️ 这些用例**只在"1 格 = 1 字符"的整数格渲染下成立** |
| 坑 | ① 屏幕坐标半格偏移（取整方向）→ 由"**整数原点**"从根上消掉，不是靠再补一层 floor；② **`view >= map` 时 `Clamp` 的前置条件被违反**（`lo > hi`，`MathUtils.h:33`）→ 必须自己早退；③ **HUD 占视口第 0 行** → 视口高 = 画布高 − 1，写成整屏高会让底部多露一行；④ **相机必须在 `World::Update` 之后**，否则 `@` 与地图差一帧相位；⑤ 奇数视口宽时居中偏移必须用**整数除法** |
| 落地偏差 | 4 条，都记在 `doc/S6_CHECKLIST.md`：用例 8 的"负数 pos"不可达、施工图 6.6 与 7.1 的帧序自相矛盾、**S5 的 `Game` 用例拿屏幕位移当世界位移**（相机一介入就测不出归一化）、**"相机的权威尺寸是 `ctx` 而不是 `fb`"** |
| 与 PLAN 的差异 | **6 条**，见 `doc/S6_CHECKLIST.md` 第 12 节（不做平滑、不建 `target`、不提供 `WorldToScreen`、不建 `Viewport.h`、`ScreenToWorld` 只留契约、遍历结构未改） |

### S7 Collision —— 不能穿墙（✅ 2026-10-08 完成）

| 项 | 内容 |
| --- | --- |
| 目标 | 基于 tile 的碰撞 + **分轴移动**；`@` 撞墙停住、沿墙能滑、**永远走不出地图** |
| 实现 | `engine/include/mini/world/Collision.h`（**header-only**，原 241 行 / 点模型后 **165 行**）：`kMaxStepPerTick = 0.5f` + `kContactEpsilon = 1e-4f`（各配 `static_assert`）；公开三个纯函数 —— `IsSolidAt(map,tx,ty)`（**越界 = 实心**）/ `IsSolidAt(map,center)`（**中心所在的那一格**实心吗）/ `MoveAndCollide(map,pos,delta)`（**外层子步 + 内层先 X 后 Y + 夹紧到格边界前 ε**）。`detail::StepAxis` 是 **±x/±y 四个方向共用**的一份实现（`movingIsX` 只切坐标）。⚠️ **2026-10-08 收口后模型改过一次**：从"1×1 盒"改成"占格 = `ToIndex(pos)` 的**点模型**"（原因与完整对照表见 `doc/S7_CHECKLIST.md` 顶部的「模型变更」节）—— 于是 `kEntityHalfSize` / `kMaxTilesPerAxis` / `OverlapsSolid` **都已删除**。接入点只有 `World::Update` 一行（`World.cpp:97`）—— **`game/` 的移动代码一行没改** |
| C++ 知识 | 早退、`[[nodiscard]]`、`constexpr`、`std::max/ceil`、**用 `static_assert` 把几何不变量写进编译期**（子步长 < 1 格、盒半宽 == 0.5）。⚠️ 这里**改过两次**：先按差异 3 去掉 ε（"开区间判定 + 精确半数接触面"），2026-10-08 换成点模型后 **ε 又回来了** —— `kContactEpsilon`，只用于"停在格边界前"，语义与第一版不同（见 `doc/S7_CHECKLIST.md` 顶部「模型变更」） |
| 引擎知识 | 碰撞**检测** vs 碰撞**响应**、AABB 分轴解算、**离散采样 vs 连续扫掠**（子步进 = 把一次采样拆成多次）、"默认参数下不穿"是**巧合**而不是设计 |
| 规模 | ~250 行 → **实测：`Collision.h` 241 行（点模型后 **165 行**）+ `tests/test_collision.cpp` 326 行（点模型后 **381 行**，仍 17 条用例）+ `World.cpp`/`World.h` 改 ~10 行 + `test_game.cpp`/`test_world.cpp` 的口径修正（含 +1 条 `Game` 用例）；用例总数 105 → **123** |
| 可运行效果 | 按住 `W`/`A` **只走 1 格就贴住外沿墙**；斜向撞柱子会**沿柱面滑行**；**走不出地图**（`x ∈ [0.5, 198.5]`）。⚠️ PLAN 原写的"F2 显示碰撞盒"**没做**（差异 5） |
| 验收 | 机器形式：`Collision` suite **17 条**（穿刺守卫 `DoesNotTunnelThroughWallAtHighSpeed`、边界 `CannotLeaveMapThroughEdge`、自愈 `AlreadyInsideSolidDoesNotFreeze`（点模型前叫 `AlreadyOverlappingDoesNotFreeze`）、纯函数 `MoveAndCollideIsPureFunctionOfInputs`…）+ `Game.PlayerStopsAtTopWall`；**2026-10-08 本人回报 `123 passed / 0 failed`**（反向验证 R1 那次的 `112 + 11` 也印证了总数 123）。人眼见 `doc/S7_CHECKLIST.md` 10.2 —— **✅ 2026-10-08 完成**（观察点 1~7、9~10 通过；斜向那条因本机键盘硬件 ghosting **跳过**，替代做法：先 `S` 再 `D` 分两步、或用方向键）；反向验证 6 条见 10.3 —— **已做 R1 / R2 / R4 并实测看到红，R3 / R5 / R6 登记跳过**（代价与触发条件在 10.3） |
| 坑 | ① **一帧位移 ≥ 1 格就可能跳过一整格墙**（点模型：从格边界附近起步时盲区 = D − 1 格；旧盒模型是 ≥ 2 格）→ 子步长必须 < 1 格。⚠️ **这不是"留余量"的问题**：`argv[2]` 的默认速度在 dt clamp 后**正好**是 1 格/帧，一点余量都没有 —— 点模型让子步进从"保险"变成了"必需"；② **不分子步会让"冲出地图"变成永久卡死**（世界外恒为墙、四方向全 solid）—— 这是 S7 自己引入的失败态，也是子步进提前到 S7 的真正理由；③ ⚠️ **2026-10-08 点模型改写**：实体占格 = `ToIndex(pos)`，"**跨格边界本身不拦**"，只有**目标格实心**才拦（对角缝隙因此能穿）；④ 撞墙停在**格边界 ∓ `kContactEpsilon`**（不留 ε 就正好落在实心格里）；⑤ 取整只走 `TileMap::ToIndex`；⑥ 外层子步 / 内层先 X 后 Y 的**顺序定死**（换序在拐角差一格）；⑦ 一轴夹紧后**不许 `break` 整帧**（否则贴墙滑行消失）；⑧ 子步长 < 1 格 ⇒ 一步最多跨一个边界 ⇒ 撞上的格就是 tentative 所在格（**不必扫描候选格**） |
| 落地偏差 | **10 条**（点模型前 7 条 + 收口后 3 条），都记在 `doc/S7_CHECKLIST.md`：候选格局部性依赖子步长 · 用例 11 的"结果一致"不成立 · 用例 8/9 改成等价可观察形式 · 荒谬速度不设上限 · **坑 13 的"1 格"估算算错了（实际 2 格 → `dy` 14→12）** · 用例 18 复用 `RunHolding` · **漏核 `World` suite 使 4 条既有用例真红** · **收口后本人报"看着脱墙却动不了"（模型 vs 显示错配）** · **F4 调试快照** · **碰撞模型整体换成点模型** |
| 与 PLAN 的差异 | **7 条**，见 `doc/S7_CHECKLIST.md` 第 12 节：子步进提前到 S7 · `IsSolidAt` 的位置 · ε（**改过两次**：先去掉、2026-10-08 换点模型后又回来） · 实体推挤不做 · F2 不做 · 规模 · **`Collision.h` 落在 `world/` 而不是目标树的 `physics/`**。⚠️ 另外收口后**模型从"1×1 盒"换成"点模型"**（见 `doc/S7_CHECKLIST.md` 顶部「模型变更」） |

### S8 Enemy + AI 状态机（✅ 2026-10-09 完成）

> 施工图 + 落地记录：`doc/S8_CHECKLIST.md`（含 **9 条落地偏差**、**9 条与本节原文的差异**、用例清单、反向验证）。
> 本表记的是**实际落地**的样子。

| 项 | 内容 |
| --- | --- |
| 目标 | `Idle → Patrol → Chase → Attack → Dead` 五态，`E` 会追 `@` |
| 实现 | **决策与接线分家**（本阶段的核心结构决定）：<br>· `engine/include/mini/ai/EnemyBrain.h`（新增，header-only，**249 行**）= `EnemyState` 五态 + `EnemyTuning`（9 个参数，配 **6 条 `static_assert`** 把迟滞区间钉进编译期）+ `BrainInput` / `BrainOutput` + **纯函数 `Decide`**（同输入同输出、不抛、不分配、无静态状态）。它**只碰 `Vec2` + 枚举** —— 不认识 `Entity` / `World` / `TileMap` / 输入 / 屏幕。<br>· `game/include/game/Enemy.h`（新增）= `EnemyData`（`id / state / patrolPath / waypointIdx / attackTimer / idleTimer / hp`）。<br>· `game/`：`SpawnEnemies` / `DriveEnemyAi`（每帧跑一次 `Decide`，**只写速度**）/ `DrawAiPanel` / `KillNearestEnemy`。<br>⚠️ **`Entity` 零改动、`World.h/.cpp` 零改动、`Collision.h` 零改动** —— 位移仍然只有一条路径（`World::Update` → `MoveAndCollide`），所以敌人**自动**会撞墙、会沿墙滑。整个 S8 只碰了 `engine/` 里的 **`KeyCode.h`**（加 `Key::F7` 与 `Key::K` 两个映射）。 |
| C++ 知识 | `enum class`、**`switch` 的穷尽性**（面板的 `StateName` 不写 `default`：加第六态编译器会提醒）、**纯函数 + 聚合体入参**（`BrainInput` 仿 `CameraLimits`）、`std::span`（把巡逻段交给 `Decide`）、`static_assert` 钉不变量、`std::max` 夹下限 |
| 引擎知识 | 有限状态机、**感知**（只有欧氏距离，**不做视线遮挡**）、**更新顺序**（AI 读"本帧积分前"的位置 = 上一帧末；**决策在积分之前**，与"写速度必须在积分前"是同一条约束）、**迟滞**（`sight < lose` / `attackEnter < attackExit` 是"不抖"的**全部**机制） |
| 规模 | `EnemyBrain.h` **249** 行 + `test_enemy.cpp` **457** 行（18 用例）+ `Enemy.h` 56 行 + `Game.h/.cpp` 约 +150 行 + `test_game.cpp` +257 行（4 条）。**用例总数 123 → 145**（原估 ~350 行，实际落在量级内） |
| 可运行效果 | 5 个 `E` 各守一条**往返段**巡逻（`kPatrolPaths[5][2]`，每行 2 个点 → `(idx+1)%2` 就是往返）；`@` 进 10 格 → 直线追；贴到 1.5 格 → 停下、每 0.8s 扣 1 HP；`@` 跑出 14 格 → 回巡逻。**面板键是 `F7`**（原定 `F3`，本机实测冲突 → 差异 6），面板打开时 `K` 杀"离玩家最近的那个敌人"（→ `Dead` → 实体停更 → **从屏幕上消失**，但 `EnemyData` 留着，所以**面板还看得见 `state=Dead`**） |
| 验收 | 机器形式：`Enemy` suite **18 条**（纯 `Decide`：迟滞、冷却、零距离 NaN、`Dead` 终止、越界路点……）+ `Game` suite 新增 **5 条**（巡逻不追 / 靠近会追 / 攻击扣血 / HP 停 0 / **击杀后从屏幕上消失**）→ 全集 **123 → 146**，**`146 passed / 0 failed` + `ctest` 1/1**（本人 2026-10-09 回报"用例全绿"，2026-10-10 照抄尾行）。<br>⚠️ 第 5 条是**反向验证 R6 逼出来的**（注掉 `entity->active = false;` 时测试**全绿**、exe 里敌人却留在屏幕上 → 说明"死亡后不绘制"当时零覆盖）；它自己又做了一次 R6′ 复验（`:697` 红、`:701` 绿）。详见 `doc/S8_CHECKLIST.md` 偏差 10 / §10.3。<br>**反向验证 R1~R6**：已跑完（R1 首轮"改 tuning 参数"是空转 → 改成打代码逻辑后命中）。**人眼 8 条**：✅ **2026-10-10 本人回报全部通过**（含"隔墙卡墙蹭"——S14 上 A\* 的动机来源）。<br>→ **S8 全部收口。** |
| 坑 | ① **零距离归一化 → NaN**：`self == target` 时速度可能是 NaN，而 `ToIndex(NaN)` 是 UB → `Decide` 里显式判 `LengthSq() > 0`；② **状态抖动**：单阈值必抖，靠双迟滞挡；③ **帧序**：AI 必须在 `World::Update` **之前**（否则敌人慢一帧）；④ **跨过路点**：到点判据必须是"本帧会到达或越过"（`dist <= moveSpeed*dt + arriveRadius`），写成"落进半径内"会让敌人卡在路点两侧来回抽；⑤ **两套真相**：敌人的位置在 `Entity`、状态在 `EnemyData`，靠 `id` join —— S8 **没有删除路径**所以还不出事，真删留到 S9；⑥ **`Dead` 不改容器**（只置 `active=false`）→ 会累积"僵尸 `EnemyData`"；✅ **S9 步骤 4（2026-10-10）已兑现真删** —— `ResolveBullets`（⑤d）收 id 名单 → 循环外 `MarkForRemoval` + `std::erase_if(m_enemies)`，`TECH_DEBT` **C16** 已关；被真删取代的那个 `Dead → active=false` 分支登记为 **C22**（不可达但保留当防御） |
| 设计结论 | **状态机够了，不要上行为树**（同原文）。本阶段另得一条同等重要的结论：**把"纯决策"和"改动世界"分开** —— 决策放 `ai/`（可离线测、零依赖），接线放 `game/`，于是 `engine/` 几乎没动。这是 S7"把碰撞做成纯函数"的**第二次兑现**。 |

### S9 Weapon / Projectile —— 可以射击

| 项 | 内容 |
| --- | --- |
| 目标 | `SPACE` 发射子弹，子弹飞行、撞墙消失、命中敌人扣血、击杀计数 |
| 实现 | `Projectile` 组件 + 简单对象池（`std::vector<Projectile>` + `active` 标记 + 定期压缩）；命中检测用 AABB |
| C++ 知识 | `swap-erase`、迭代器失效、`std::vector::reserve`、生命周期、`std::optional` 返回值 |
| 引擎知识 | 生成/销毁（spawn/destroy）请求、延迟删除队列、事件通知 |
| 规模 | ~300 行 |
| 可运行效果 | `@ ---> * ---> E`，敌人掉血、死亡、`Kills` +1 |
| 验收 | 连按 SPACE 100 次不崩溃、不重复占用内存（任务管理器看内存稳定）；子弹不会穿墙 |
| 坑 | 遍历中 `push_back` 导致迭代器失效；子弹速度过快在低帧率下跳过敌人（如出现→ 子步进，见 S3 结论） |

> ⚠️ **本表是立项时的原始设想，实际落地与它有多处不同**（`Projectile` 组件 + 对象池 + AABB 命中 →
> 最终是 **纯 `Entity` + id 成员表 + 沿线段等距采样**；`PLAN` 点名的"遍历中 `push_back`"这个坑
> 被架构消掉了）。**以 `doc/S9_CHECKLIST.md` 为准**（4 个分叉 + 12 条落地偏差都在那里）。
> ⚠️ **2026-10-10 进度**：步骤 1/2/3/4 已实测绿（**168**，本表对应的"可运行效果"与"验收"
> 已在 10.2/10.3 逐条落地）；**步骤 5（HUD 两行）代码已落地，尚未构建验证**。
> ⚠️ 本表的"全部回填"（含 `A.2` 的 `game/Bullet.h`、`G` 节、`H` 节）仍然安排在
> **步骤 6 收口**时一次性做完 —— 别在这里零散地改。

### S10 HUD / GameState / Wave —— 一个可玩的完整循环

| 项 | 内容 |
| --- | --- |
| 目标 | `HP/Ammo/Kills/Wave` 面板 + `Title/Playing/Paused/GameOver` 状态机 + 刷怪波次 |
| 实现 | `GameState` 枚举状态机，`switch` 分派 Update/Render；`WaveManager` 按波次刷怪并递增难度；HUD 文本布局（左对齐 + 数值右对齐） |
| C++ 知识 | 状态机、字符串格式化（`std::format` 或手写 padding）、常量配置抽到 `Config` |
| 引擎知识 | 游戏状态与引擎循环的关系、暂停即"跳过 Update，只做 Render" |
| 规模 | ~350 行 |
| 可运行效果 | 完整的：开局提示 → 按 Enter → 刷怪 → 射击 → 掉血 → 死亡 → 按 R 重开 |
| 验收 | 从启动到死亡再重开，循环 3 次不泄漏、不卡死；HP 归零后不再响应移动 |
| 坑 | 暂停时仍累积 `dt`，恢复瞬间角色瞬移（应对 `dt` 清零或跳过时间更新） |
| **里程碑** | **到这里 v0.3 达成：一个真正能玩的终端游戏** |

### S11 Debug Overlay + Logger + Profiler

| 项 | 内容 |
| --- | --- |
| 目标 | F1~F4 开关、`PROFILE_SCOPE("Physics")` RAII 计时、最近 120 帧的环形缓冲 + 表格输出 |
| 实现 | `ScopedTimer`（构造取时间、析构累加）、`thread_local` 当前作用域栈、`std::array<TraceEvent,120>` 环形缓冲、Logger 分级 + 写文件 |
| C++ 知识 | **RAII**、`thread_local`、`std::chrono`、环形缓冲、变参模板/`std::format`、`std::source_location`（C++20） |
| 引擎知识 | 性能分析的最小可用形态、scope 计时的实现原理（这就是 Tracy/Chrome Tracing 的雏形） |
| 规模 | ~400 行 |
| 可运行效果 | 终端右侧显示 `Input/Game/Physics/AI/Renderer/Total/FPS` 表格，F4 切换 |
| 验收 | Renderer 是最耗时的项这一结论能被数据验证；Profiler 本身开销 < 0.2ms |
| 坑 | 计时器本身被编译器优化掉；每帧分配内存污染测量结果（用固定缓冲） |

### S12 Ray Casting —— 伪 3D

| 项 | 内容 |
| --- | --- |
| 目标 | DDA 网格步进 + 每列一条射线 → 墙高 + 距离 → 第一人称走廊画面 |
| 实现 | `RayCaster::CastColumn(cameraX) -> {distance, side, tile}`；投影 `lineHeight = screenH / distance`；`cos` 校正鱼眼；按距离选字符 `█ ▓ ▒ ░` |
| C++ 知识 | 固定大小数组、`std::clamp`、数值稳定性、`constexpr` 常量表 |
| 引擎知识 | 光栅化思想、FOV/相机平面、透视投影、**鱼眼校正的原理**（为什么乘 `cos(rayAngle - camDir)`）、深度 |
| 规模 | ~350 行 |
| 可运行效果 | 转动方向键，走廊随之旋转；前进时墙逼近；没有明显鱼眼扭曲 |
| 验收 | 走到墙正前方，中心列墙占满屏；转身 180° 看到反方向；直线走廊不会变成弧线 |
| 坑 | 鱼眼校正漏掉 → 画面中间凸出；DDA 的 `sideDistX/Y` 初值算错 → 墙面出现缝隙或错位；`rayDir` 未归一化 |

### S13 Sprite + Z-buffer

| 项 | 内容 |
| --- | --- |
| 目标 | 敌人以精灵形式出现在 3D 视角里，被墙正确遮挡 |
| 实现 | 把敌人从世界坐标变换到相机空间 → 屏幕列区间；每列与 z-buffer 比较深度；支持按距离变暗 |
| C++ 知识 | 矩阵/向量变换（手写 2×2 就够）、裁剪 |
| 引擎知识 | 精灵渲染、深度缓冲、遮挡排序、为什么 2D 精灵要用 1D z-buffer |
| 规模 | ~300 行 |
| 可运行效果 | 走廊里能看到 `E`，走到墙后它就消失 |
| 验收 | 敌人半身被墙挡住时遮挡边界正确（不是整只消失）；敌人不在 FOV 内不渲染 |

### S14 打磨与收口

| 项 | 内容 |
| --- | --- |
| 目标 | 颜色/光照、简单粒子、A* 寻路（可选）、README、CI、打 tag |
| 实现 | 256 色渐变做距离雾；命中小方块粒子；A* 让 S8 的"卡墙"问题真正有解；GitHub Actions（**只跑 MSVC** —— clang 已于 2026-09-29 弃用） |
| 规模 | 弹性，随时停 |
| 可运行效果 | 录制一个 GIF 放 README —— 这是这个项目唯一需要"对外"的东西 |
| 验收 | `git tag v1.0`；在另一台机器上克隆后三条命令能跑起来 |

---

## C. 第一阶段详细方案（S0 ~ S2 落地）

> 只给**接口和配置的骨架**，不写实现。"第一阶段"= 从空目录到"`@` 能被键盘控制"。

### C.1 顶层 CMakeLists.txt（草案）

```cmake
cmake_minimum_required(VERSION 3.20)   # CMake 4.x 不支持 <3.5 的兼容写法
project(MiniEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 多配置生成器(VS)下统一输出位置，方便直接运行。
# ⚠️ 只设这一个变量，**不要**再照着老写法设 CMAKE_RUNTIME_OUTPUT_DIRECTORY_<CFG>：
# 那个变量会被按原样使用（不补配置子目录），exe 会落到 build/bin/MiniEngine.exe，
# 和附录 A 的 ./build/bin/Debug/MiniEngine.exe 对不上 —— E-6 的坑就埋在这。
# 实测（2026-09-23 家机）：只设这一条时，exe 正确落在 build/bin/Debug/ 下。
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(MINI_BUILD_TESTS "Build unit tests" ON)
option(MINI_SANITIZE    "Enable MSVC AddressSanitizer (/fsanitize=address)" OFF)

# 编译选项做成 INTERFACE 库，避免到处复制粘贴
add_library(mini_options INTERFACE)
target_compile_definitions(mini_options INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_compile_options(mini_options INTERFACE /W4 /permissive- /utf-8 /Zc:__cplusplus)
else()
    target_compile_options(mini_options INTERFACE -Wall -Wextra -Wpedantic -Wshadow)
endif()

add_subdirectory(engine)
add_subdirectory(game)
if(MINI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

> `NOMINMAX` + `WIN32_LEAN_AND_MEAN` 现在不写，等 S2 引入 `<windows.h>` 时就会收到 `max/min` 宏把 `std::max` 打爆的报错。
> `/utf-8` 是本机 936 代码页下的必需品，不是可选优化。

### C.2 engine/CMakeLists.txt（草案）

```cmake
add_library(engine STATIC
    src/platform/ConsoleWindows.cpp
    src/platform/Input.cpp
    src/render/FrameBuffer.cpp
    src/core/Application.cpp
    src/core/Time.cpp
    src/core/Logger.cpp
)
target_include_directories(engine PUBLIC include)
target_link_libraries(engine PRIVATE mini_options)
add_library(MiniEngine::engine ALIAS engine)
```

`game/CMakeLists.txt` 同理，`target_link_libraries(game PUBLIC MiniEngine::engine)`。
`main.cpp` 所在的可执行目标只要 `target_link_libraries(MiniEngine PRIVATE game)`，**不要**手动写 `include_directories`。

### C.3 核心类关系（第一阶段）

```text
main.cpp
  └─ core::Application               // 组装 + 拥有所有子系统
        ├─ platform::Console&       // 终端模式：UTF-8 / VT / 隐藏光标 / 尺寸 / Present
        ├─ platform::Input&         // Poll() → 按键四态
        ├─ render::FrameBuffer&     // 后缓冲：w*h 个 Cell（尺寸随终端变）
        ├─ core::Time&              // dt / FrameMs / WorkMs / fps（**不传给游戏**）
        └─ IGame&                   // 游戏侧实现，Application 不认识具体游戏
```

关键点：**`Application` 只持 `IGame&`（不拥有），`Game` 拥有游戏内容，`Application` 不认识 Player。** 这样 `engine` 库不会反向依赖 `game`，游戏对象的生命周期由调用方（S3 里是 `main`）管。

> **注意两处与最初草案不同**（2026-09-28 S3 定稿，完整理由见 `doc/S3_CHECKLIST.md` 第 4、8 节）：
> ① `IGame` 从 `unique_ptr<IGame>` 改成 `IGame&`——`Application` 不需要拥有它，少一层所有权；
> ② `FrameContext` **不含 `Time&`**——游戏拿到可变 `Time&` 就能调 `EndFrame()`/`LimitFPS()` 把循环时序搞乱，而且没有编译器拦得住。

```cpp
// engine/include/mini/core/Application.h   —— S3 定稿（2026-09-28 已落地，与代码一致）
namespace mini::core {

struct AppConfig {
    int  width     = 0;      // 0 = 用终端当前尺寸（推荐）
    int  height    = 0;
    int  targetFps = 60;     // <=0 = 不限帧
    bool showStats = true;   // 顶栏。打开时**第 0 行归 Application**，游戏从第 1 行开始画
};

// 一帧的输入快照。只有值和 const 引用，**没有 Time&**。
struct FrameContext {
    float deltaSeconds = 0.0f;   // 已 clamp（kMaxDelta = 0.05）
    float frameMs      = 0.0f;   // 帧间隔，未 clamp
    float workMs       = 0.0f;   // 本帧干活耗时
    float fps          = 0.0f;
    int   screenWidth  = 0;
    int   screenHeight = 0;
    const platform::Input& input;
};

class IGame {
public:
    virtual ~IGame() = default;
    virtual void OnStart() = 0;
    virtual void OnUpdate(const FrameContext& ctx) = 0;
    virtual void OnRender(render::FrameBuffer& fb) = 0;
    [[nodiscard]] virtual bool ShouldQuit() const = 0;
};

class Application {
public:
    explicit Application(AppConfig cfg = {});   // 构造顺序：Console → Input → FrameBuffer
    ~Application();                             // RAII：恢复终端、显示光标、离开备用屏

    void Run(IGame& game);

    void RequestQuit() noexcept;                // **统一退出入口**（S2 的孤儿进程问题靠它收口）
    [[nodiscard]] bool QuitRequested() const noexcept;
private:
    AppConfig                          m_cfg;
    std::unique_ptr<platform::Console> m_console;
    std::unique_ptr<platform::Input>   m_input;
    platform::ConsoleSize              m_size{};   // 声明在 FrameBuffer 之前：画布尺寸的来源
    render::FrameBuffer                m_frameBuffer;
    Time                               m_time;
    bool                               m_quit = false;
};
// 实落地的两处补充（2026-09-28）：
//   ① 拷贝/移动四个特殊成员显式 = delete（持有 unique_ptr + 独占终端）
//   ② m_size 声明在 m_frameBuffer **之前** —— 初始化列表里 m_frameBuffer 用 m_size 构造

// Console 侧新增的唯一接口（顶栏要显示 "Rows n/总行数"）：
//     [[nodiscard]] virtual int LastFrameRowsSent() const = 0;   // 上一帧发出去的行数
// 刻意**不**返回 RowDiff::Stats —— 那会让 platform 的公共头去 include render/RowDiff.h，
// 为一个数字把两层耦合起来不划算；也刻意不返回 bytes（顶栏不显示，加了就是死接口）。
// 也刻意是"上一帧"：本帧的要等 Present() 之后才有，而顶栏在 Present() **之前**画。

} // namespace mini::core
```

```cpp
// engine/include/mini/render/FrameBuffer.h  —— 骨架
namespace mini::render {

struct Cell { char ch = ' '; Color fg = Color::Default; Color bg = Color::Default; };

class FrameBuffer {
public:
    FrameBuffer(int w, int h);
    void Resize(int w, int h);                 // 终端尺寸变化时

    void Clear(Cell fill = {});
    void Set(int x, int y, char c, Color fg = Color::Default);  // 越界直接忽略
    void Draw(int x, int y, std::string_view text);
    void DrawRect(int x, int y, int w, int h, char c);

    int  Width()  const;  int Height() const;
    const Cell& At(int x, int y) const;

    std::string_view Serialize() const;        // 内部一次性拼好的字符串
private:
    int              m_w, m_h;
    std::vector<Cell> m_cells;                 // 线性化：index = y*m_w + x
    mutable std::string m_scratch;             // reserve(w*h*8)
};
} // namespace mini::render
```

```cpp
// engine/include/mini/platform/Input.h  —— 骨架
namespace mini::platform {

enum class Key : std::uint8_t { Unknown = 0, W, A, S, D, Q, E, R, Space, Enter, Escape, Tab,
                                ArrowUp, ArrowDown, ArrowLeft, ArrowRight, F1, F2, F3, F4,
                                Count };   // Count = 哨兵，等于数组长度

// 四态（2026-09-28 定稿；原三态 {Released, Pressed, Held} 无法区分"刚抬起"与"一直没按过"）：
//   Up       一直没按
//   Pressed  本帧刚按下，含"本帧内按下又抬起"的极速点击；只维持一帧
//   Held     持续按住
//   Released 本帧刚抬起；只维持一帧
enum class KeyState : std::uint8_t { Up, Pressed, Held, Released };
// IsDown() == (Pressed || Held)；Get() 的判定顺序必须 Held 先于 Pressed
// （否则长按会被"本帧见过 down"的 pulse 误判成 Pressed）—— 见 doc/S2_CHECKLIST.md 6.3

class Input {
public:
    virtual ~Input() = default;
    virtual void BeginFrame() = 0;          // 帧尾推进：m_downPrev = m_downAtEnd，本帧的 down 脉冲清零
    virtual void Poll()       = 0;          // 非阻塞读事件
    virtual bool IsDown(Key k) const = 0;   // Pressed || Held
    virtual KeyState Get(Key k) const = 0;
    virtual bool QuitRequested() const = 0; // 只反映"外部强制退出"：Ctrl+C / 关窗口（**不含 ESC**，见 S2 决策 4）
};

std::unique_ptr<Input> MakeInput();         // 平台工厂。只做 Windows，所以没有 ifdef 分派文件：
                                            // 工厂函数直接定义在 src/platform/InputWindows.cpp 里
} // namespace mini::platform
```

### C.4 Game Loop（**2026-09-28 S3 定稿**）

```cpp
void Application::Run(IGame& game)
{
    game.OnStart();
    while (!QuitRequested())
    {
        m_time.BeginFrame();                 // ① 帧首：算 dt / FrameMs（第一帧为 0）

        m_input->Poll();                     // ② 把键盘事件收进内部状态表

        // ③ 对齐画布尺寸（E-12：不走事件驱动，每帧轮询更简单也不会漏）
        const ConsoleSize size = m_console->Size();
        if (size != m_size && size.width >= 2 && size.height >= 2) {
            m_frameBuffer.Resize(size.width, size.height);
            m_size = size;
        }

        FrameContext ctx{ m_time.Delta(), m_time.FrameMs(), m_time.WorkMs(), m_time.Fps(),
                          m_frameBuffer.Width(), m_frameBuffer.Height(), *m_input };
        game.OnUpdate(ctx);                  // ④ 只有 Game 层更新

        m_frameBuffer.Clear();               // 后缓冲
        game.OnRender(m_frameBuffer);        // 全部绘制进后缓冲
        if (m_cfg.showStats) {
            DrawStatsBar(m_frameBuffer, ctx);   // ⑤ 顶栏由 Application 画，且在游戏**之后**
        }

        if (!m_console->Present(m_frameBuffer)) {   // ⑥ 一次性提交（唯一的终端写入）
            RequestQuit();                          //    终端已死 → 退出（S2 孤儿进程的根治）
        }

        m_input->BeginFrame();               // ⑦ 帧尾：四态状态机推进
        m_time.EndFrame();                   //    算 WorkMs 并塞进 FrameStats
        m_time.LimitFPS(m_cfg.targetFps);    //    混合睡眠限帧

        if (m_input->QuitRequested() || game.ShouldQuit()) {
            RequestQuit();
        }
    }
}
```

**五个顺序硬约束**（写反就出怪现象，方向见 `doc/S3_CHECKLIST.md` 7.1）：

| 约束 | 写反的后果 |
| --- | --- |
| `BeginFrame` 在循环**最前**，不是 `LimitFPS` 之后 | dt 只度量干活耗时 → 顶栏报 3000fps，"性能完美"的假绿 |
| `Poll` 在 `OnUpdate` 前、`BeginFrame` 在 `OnUpdate` 后 | `Pressed` 永远看不到（S2 已踩过） |
| resize 检测在 `OnUpdate` 之前 | 游戏这一帧拿到旧尺寸，按旧尺寸算的位置画到画布外 |
| 顶栏在 `game.OnRender` **之后**画 | 游戏能把 FPS 数字盖掉 |
| `Present` 在 `Clear` + `OnRender` 之后 | 提交的是上一帧残留 |

第一阶段**故意不拆 Physical/AI 子阶段**：现在只有一个实体，拆了就是空函数。等 S7/S8 真正把 `PhysicsSystem` / `AiSystem` 写出来时，再把 `OnUpdate` 拆成：

```cpp
Game::OnUpdate(dt) {
    PlayerInputSystem::Update(world, input, dt);
    PhysicsSystem::Update(world, map, dt);
    AiSystem::Update(world, player, dt);
    ProjectileSystem::Update(world, dt);
    hud.Update(world, time);
}
```

这就是 `input_plan.md` 里那个伪代码的"更合理版本"：**它是阶段性的产物，不是起点。** 具体差异与理由见 E.4。

### C.5 Terminal Renderer 方案

**结论：ANSI Escape Sequence 值得用，而且必须用。**

理由是这四项能力没有 ANSI 就没有替代品（或替代品更麻烦）：

| 需求 | ANSI 方案 | 无 ANSI 的替代 | 评价 |
| --- | --- | --- | --- |
| 光标归位 | `ESC[H` | `SetConsoleCursorPosition` | ANSI 更简单 |
| 隐藏/显示光标 | `ESC[?25l` / `ESC[?25h` | Windows API | ANSI 更简单 |
| 清屏 | `ESC[2J`（**尽量别用**，见下） | Windows API | — |
| 颜色 | `ESC[38;5;{n}m` 256 色 | Windows API 逐格属性 | ANSI 简单得多 |
| 备用屏 | `ESC[?1049h` / `ESC[?1049l` | 无 | 退出后恢复 shell 历史 |

Windows 上唯一的成本是**启用 VT 处理**（Windows 10 1511+ / Windows Terminal 默认已开）：

```cpp
// 阶段 1 先放在 render 里，阶段 2 挪到 platform::ConsoleWindows
SetConsoleOutputCP(CP_UTF8);                     // 936 → UTF-8，否则框线字符全乱
SetConsoleCP(CP_UTF8);                           // 输入侧同理
HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
DWORD  mode = 0;
GetConsoleMode(hOut, &mode);
SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
```

**如何避免闪烁**——三层措施，从便宜到昂贵：

1. **一帧只写一次终端。** `Present()` 把整个画面拼进一个 `std::string`，然后一次 `std::cout.write(p, n)`。这是最大的一笔收益，不是"双缓冲"本身。
2. **不要每帧 `ESC[2J` 清屏。** 清屏 + 重画之间那一瞬就是闪烁的来源。正确做法是 `ESC[H` 回到左上角，然后逐行覆写（行间用 `"\r\n"`，不要用单独的 `"\n"`）。
3. **脏行 diff（放到 S3）。** 内容没变的行不重发，只发 `ESC[{row};1H` 跳到变的行。在 25 行的小画面上收益不大，但在 60 行以上 + 256 色时能省一半以上输出量。

**关于"双缓冲"的概念澄清**：终端游戏里的 `FrameBuffer` 本身就是后缓冲，屏幕是前缓冲。你不需要两份 `FrameBuffer`（除非要做"上一帧对比"来算脏行，那时才需要第二份）。`Present()` 就是"交换"（形式上是一次拷贝）。

**终端尺寸变化**：监听 `WINDOW_BUFFER_SIZE_EVENT`（`ReadConsoleInput` 会收到），变化时 `FrameBuffer::Resize()` + `ESC[2J` 清屏重画 + 相机边界重新 clamp。

**平台隔离**：所有 Windows API 集中在 `platform/ConsoleWindows.*`，公共头里**一个 `#ifdef` 都不出现**（见 A.3 的硬规矩）。本项目只做 Windows，所以不写任何 Linux 分支——真要跨平台时的成本是"新增一个 `ConsoleXxx.cpp`"，不是现在就把两套都写出来。

---

## D. 架构演进

### D.1 实体模型：什么时候升级到 ECS

```text
D-0  单结构体            S5     struct Entity { Transform; Velocity; char symbol; bool active; };
      ↓ 当"不是所有实体都有所有字段"开始烦人
D-1  Entity + 可选字段     S8~S9  加 Health / Collider / AiState 字段，用 active/enabled 标记区分
      ⚠️ **S8 实际没走这条路**：敌人特有的字段（状态/血量/定时器/巡逻）**没进 `Entity`**，
         而是走下面 D-2 提到的"**按类型分容器**"—— `game/Enemy.h` 的 `EnemyData` + `std::vector<EnemyData>`。
         结果：`Entity` 保持 24 字节、`static_assert` 一次都没响、**整个 `engine/` 只碰了 `KeyCode.h` 两行**。
         代价是"两套真相靠 id join"（见 `doc/S8_CHECKLIST.md` 分叉 ① 与差异 1）。
         S9 的 `Projectile` 打算照抄这个形状。
      ↓ 当字段组合出现"要么全有要么全无"的耦合（子弹不需要 Health，敌人不需要 Projectile）
D-2  Entity + Component 指针  S10   std::vector<std::unique_ptr<Component>> + GetComponent<T>()
      ↓ 当 System 开始跨实体按类型遍历，且 cache miss 成为实测瓶颈
D-3  真正的 ECS           ——     Archetype / SoA 存储
```

**给这个项目的明确建议：停在 D-1，S10 之后按需局部到 D-2，永远不到 D-3。**

理由：
- 实体数量级是 **50 个以内**。D-3 的性能优势只有在万级实体时才出现，在此之前它是纯负债。
- D-2/D-3 的代价是"改一个字段要改三处"，直接对上 `input_plan.md` 的自我要求"不要看到计划表就不想做"。
- **D-1 的一个小技巧就够撑到结束**：用 `std::variant` 或按类型分容器（`std::vector<Enemy>`、`std::vector<Projectile>`），比"万能 Entity"更简单也更快，而且是最贴合本项目的做法。
  > 换句话说：**S10 之后如果觉得 `Entity` 变臃肿了，优先考虑拆成 `Player`/`Enemy`/`Projectile` 三种具体类型 + 各自的 `vector`，而不是去做 ECS。** 这就是"数据结构适配需求"，不是"抄一个 ECS 框架"。

**什么时候它真的成立？** 当出现下面任一条件：
1. 同类实体的数量级达到 1 万以上，且 Profiler 明确指出遍历/缓存是瓶颈；
2. 需要运行时动态组合大量正交能力（如 Roguelike 的物品词条系统）。

本项目两个都不满足。

### D.2 渲染：2D → 伪 3D

```text
S1   FrameBuffer：只有 "把字符放到 (x,y)"                     —— 屏幕空间
S6   + Camera：世界坐标 → 屏幕坐标，只有平移                  —— 世界空间
S12  + RayCaster：每列一条射线 → 距离 → 墙高                  —— 透视
S13  + Z-buffer：精灵与墙的遮挡关系                           —— 深度
```

这个演进的关键在于**每一步只加一个概念**：

1. **S1→S6 加的是"坐标变换"**。此时渲染器仍然只会画点/矩形/文本，它不认识 3D。这也是为什么 `Renderer2D` 不该接收 `Player`，只该接收 `x, y, char`。
2. **S6→S12 加的是"列"这个维度**。整个变化其实很小：原来是"遍历可见 tile，把每个画到屏幕"，现在是"遍历屏幕每一列，向后发射线，遇到墙就在这一列画竖线"。**世界数据（`TileMap`）完全不用改**——这正是 S5 选择 tile map 而不是任意几何体的回报。
3. **S12→S13 加的是"深度比较"**。墙的渲染天然产生了一个按列的深度数组，精灵渲染就是"每列比较深度，近了才画"。这个 z-buffer 有 320 项，比 GPU 的 2D 缓冲区小 5 个数量级，但算法一模一样。

**关于"Fish-eye Correction"**（容易搞错的重点）：DDA 得到的距离是沿射线方向的**斜距**，不是到相机平面的**垂距**。同一面正前方的墙，两侧的斜距比中心大，直接用就会让墙看起来向后弯成一个球面。修正是乘 `cos(rayAngle - cameraDir)`，把斜距投影回垂直于相机方向的距离。**第一次看到的画面如果"世界像鱼眼镜头"，90% 是这个乘法漏了。**

**演进时的兼容策略**：`Renderer3D` 不改 `Renderer2D`，两者都以 `FrameBuffer` 为唯一输出目标。加一个 `AppConfig::renderMode { Mode2D, Mode3D }` 就能用 `F5` 在两种视角间来回切换——这个"同一个世界，两种看法"的演示效果非常值，而且成本几乎为零（因为分层干净）。

---

## E. 技术风险与对策

| # | 风险 | 症状 | 对策 | 触发阶段 |
| --- | --- | --- | --- | --- |
| E-1 | **代码页 936** | 框线字符变乱码，MSVC 报 C4819；**2026-09-23 家机实测更狠：只要源文件里有中文注释，漏 `/utf-8` 就直接 `error C4716: 必须返回一个值`** —— UTF-8 字节被按 GBK 解析后吃掉了行尾换行，把下一行代码吸进注释里，**构建根本过不去，不只是警告** | `SetConsoleOutputCP(CP_UTF8)` + 编译加 `/utf-8` + 源文件存 UTF-8；`tests/test_main.cpp` 的 `source_is_compiled_as_utf8` 用例长期守着这条（并已做过去掉 `/utf-8` 的负向验证） | S0 起（已提前命中）/ S1 视觉验证 |
| E-2 | **VT 模式未启用** | 出现字面的 `←[H` 而不是光标归位 | `SetConsoleMode(...ENABLE_VIRTUAL_TERMINAL_PROCESSING)`；用 Windows Terminal 而非老 conhost | S1 |
| E-3 | **终端闪烁** | 画面撕裂/闪动 | 一帧一次 `write`、`ESC[H` 覆写而非 `ESC[2J`、S3 加脏行 diff。**2026-09-28 S3 方案已定稿**：落地为 `render::RowDiff`（只发变化的行，`ESC[{row};1H` 定位 + `ESC[0K` 擦尾；**只有尺寸变化后的第一次**才允许 `ESC[2J`）。见 `doc/S3_CHECKLIST.md` 6.2 / 9.2 | S1/S3 |
| E-4 | **Windows 输入卡死** | 鼠标在终端里点一下后无响应 | 关闭 `ENABLE_QUICK_EDIT_MODE`（需同时置 `ENABLE_EXTENDED_FLAGS`） | S2 |
| E-5 | **Sleep 精度** | 限帧 60 实测 32 FPS | 混合睡眠：`sleep_for(remain - 2ms)` 后自旋；或 `timeBeginPeriod(1)`（可选，需链 `winmm`） | S3 |
| E-6 | **多配置生成器** | `ctest` 找不到测试、exe 路径猜错 | 所有构建命令带 `--config Debug`，ctest 带 `-C Debug`；统一 `CMAKE_RUNTIME_OUTPUT_DIRECTORY` | S0 |
| E-7 | **CMake 4.x 兼容** | `cmake_minimum_required(VERSION 3.5)` 直接报错 | 最低写 3.20 | S0 |
| E-8 | **坐标系混乱** | 画面整体偏移半格、走位比视觉慢一格 | 定死 A.5 的四层约定；实体浮点、tile 用 `floor`；写一个坐标转换的单元测试。**✅ 2026-10-02 S6 已落**：A.5 补了"**整数原点**"这条（相机只取整一次、地图与实体共用），`Camera` suite 14 条里 `PlayerColumnMatchesItsTileColumn` 就是这条的守卫（实体与它脚下的 tile 必须同列） | S4/S6 **✅ 已收口** |
| E-9 | **迭代器失效** | 射击时随机崩溃 | 禁止"遍历中增删"；用延迟删除队列（`toRemove` 列表）或 `swap-erase` 倒序遍历 | S9 |
| E-10 | **穿透** | 子弹跳过敌人、玩家高速穿墙 | 分轴移动（S7）；子弹子步进（S9）；`dt` clamp（S3）。**✅ 2026-10-08 S7 已落**：`world/Collision.h` 的 `MoveAndCollide` = 外层**子步进**（`kMaxStepPerTick = 0.5f`，另有 `static_assert(< 1.0f)` 把"盲区长度 < 0"这条**几何不变量**钉进编译期）+ 内层分轴**夹紧到接触面**。⚠️ **子步进从 S7 就做了**（原分配是 S9 的子弹）—— 理由：`argv[2]` 能把速度开到 5~10 格/帧（阈值 2 格/帧），且不分子步会让 S7 **引入**"冲出地图后永久卡死"。**S9 的子弹沿用同一套**。守卫用例：`Collision.DoesNotTunnelThroughWallAtHighSpeed`（离墙 1.5 格、一帧推 5 格）、`Collision.CannotLeaveMapThroughEdge` | S7 **✅ 已收口**，S9 沿用 |
| E-11 | **Game Loop 设计错误** | 高刷屏上游戏速度变快、暂停恢复瞬移 | variable dt 且 `dt = min(dt, 0.05)`；暂停时不累积时间 | S3/S10 |
| E-12 | **终端改尺寸** | 画面错位、越界写 | **S2 只把 `WINDOW_BUFFER_SIZE_EVENT` 安全丢弃**（不崩、不误当按键）；S3 起每帧比较 `FrameBuffer` 尺寸与 `Console::Size()`，变了才 `FrameBuffer::Resize` + 重画。**不走事件驱动**——S3 本来就有帧循环，轮询更简单且不会漏。**2026-09-28 S3 方案已定稿**：落在 `Application::Run` 第 ③ 步，`width < 2 \|\| height < 2` 时**跳过**（别拿 `Resize` 抛的异常当控制流）；相机边界 clamp 仍归 S6。见 `doc/S3_CHECKLIST.md` 6.3 / 9.3。**✅ 2026-10-02 S6 已落**：相机每帧用 `ctx` 的尺寸重新夹取（`ctx` 与 `fb` 同帧一致，`Application.cpp:89-118`），用例 `Camera.ResizeThenFollowUsesNewViewport` 钉住；⚠️ 但"尺寸协商本身的接受条件"（`w >= 2 && h >= 2`）**仍未抽函数、无法离线测** → ✅ **2026-10-02 当天办完**：抽成 `IsUsableCanvasSide/IsUsableCanvasSize`（`core/Application.h`）+ `Core.CanvasSizeAcceptanceIsBoundarySafe` 用例（`TECH_DEBT` C7 因此**收窄**为"只剩 `ConsoleWindows` 的真终端 IO"） | S3/S6 **✅ 已收口** |
| E-13 | **Ray Casting 鱼眼/裂缝** | 墙呈弧形、墙面有竖缝 | `cos` 校正；DDA 的 `sideDistX/Y` 初值按方向取 `(1-delta)` 或 `delta`；`rayDir` 归一化 | S12 |
| E-14 | **ECS 过度设计** | 花两天改架构，游戏没进展 | 见 D.1：停在 D-1，需要时按具体类型分容器 | S10+ |
| E-15 | **Profiler 失真** | 计时器被优化掉、被测代码因分配而变慢 | `std::atomic_signal_fence`/`DoNotOptimize`、固定大小环形缓冲、零分配 | S11 |
| E-16 | **平台 API 泄漏** | 半年后 `#include <windows.h>` 散落各处，想改也改不动 | 只有 `platform/` 允许出现 `<windows.h>` 与 `#ifdef _WIN32`（见 A.3 硬规矩）；其余代码只 include 标准库和 `mini/` 头。本项目只做 Windows，所以不为跨平台做任何预留 | S2+ |
| E-17 | **Windows 头文件污染** | `std::max` 编译失败、`min` 宏 | `NOMINMAX`、`WIN32_LEAN_AND_MEAN`；`<windows.h>` 只出现在 `.cpp` 里 | S2 |
| E-18 | **测试框架膨胀** | 为了写 3 个断言引入 GoogleTest + 网络配置 | 先用自制 `MiniTest.h`（80 行）；**当出现**"参数化测试 / fixture / 死亡测试 / CI 需要 JUnit 报告"时（约 S10 之后、断言数 > 200）再 `FetchContent` 引入 GoogleTest | S4/S10 |
| E-19 | **一开始就想 3D** | 基础不牢，伪 3D 变成调 bug 地狱 | 严格按 S0→S11 顺序；S12 之前不碰 3D | 全程 |
| E-20 | **动力衰减**（最大的风险） | 计划表太长，一周不看就不想打开 | 每阶段 ≤1.5 小时且必有可见结果；文档只记"下一小步"；S10 就有一个能玩的游戏兜底 | 全程 |

### E.3 没选固定时间步长的理由（补充 S3）

`input_plan.md` 里的循环把 `Physics::Update()` 和 `Game::Update()` 并列，这暗示了固定步长。但本项目的物理是**纯运动学 + AABB**（无重力、无弹簧、无迭代约束求解），固定步长能带来的唯一好处是"确定性复现"，而我们不做录像回放也不做联机同步。代价是引入累加器、插值、以及让输入手感变差（输入采样与物理步不对齐）。

**结论：variable dt + clamp。** 如果将来证据出现（如高速子弹穿透），在子弹这一个子系统里做子步进，而不是全引擎改结构。

### E.4 关于那个"看起来很专业"的 Game Loop 伪代码

```cpp
while (!shouldQuit) {
    Time::Update(); Input::Poll(); Game::Update();
    Physics::Update(); AI::Update(); Renderer::Render(); Time::LimitFPS(60);
}
```

它的问题是：**如果一上来就照抄，你会得到 5 个空函数和一堆全局状态。** 更实际的过程是反过来的——先写 `Game::OnUpdate`，等它长到 200 行时再抽出 `PhysicsSystem` / `AiSystem`；等 `Time`/`Input` 真正变成"每个阶段都要用"时再做成独立模块。**结构应该是被复杂度逼出来的，而不是提前预留的。** 阶段 S7/S8 就是这两个 System 自然出现的时刻（见 C.4）。

---

## F. 推荐的"研究顺序"

### 只有 30 分钟

| 想干什么 | 做什么 | 产出 |
| --- | --- | --- |
| 想立刻看到东西 | 打开项目 → 改 `Hud` 里的字符/颜色 → 重新构建 | 画面变了 |
| 想学点 C++ | 给 `Vec2` 加一个 `constexpr` 方法，补一个测试用例 | `ctest` 从 12 个变 13 个 |
| 想调参数 | 改 `AppConfig::targetFps` / 敌人速度 / 子弹速度，感受手感变化 | 游戏体验变了 |
| 想改手感 | 调相机 `lerp` 系数、碰撞 epsilon、AI 距离阈值 | 手感变化 |

> 30 分钟的场景**不应该是"开始一个新阶段"**。阶段是打包好的 1 小时以上单元；碎片时间用来调参数和补测试，这两件事的反馈速度最快。

### 有 1 小时

按 B 节的阶段顺序取下一个 **S**，做完并 commit。若中途卡住 → 把阶段拆成两半，前半必须先能运行。

### 突然对 Renderer 感兴趣

```text
1. 在 FrameBuffer 里加 256 色支持（ESC[38;5;n m）        ← 30 分钟，视觉回报极高
2. 加脏行 diff，用 Profiler 量 Present() 的耗时变化      ← 1 小时
3. 把正方形像素换成 ▀ / ▄（半块）→ 纵向分辨率翻倍         ← 1 小时，惊喜级效果
4. 用 Braille 字符做亚字符分辨率（2×4 点阵）             ← 一下午，纯好玩
5. 对照 S12 的 RayCaster，试着不用 cos 校正看鱼眼         ← 30 分钟
6. 读 sokol/notcurses 的源码，看别人怎么做终端渲染        ← 参考
```
推荐顺序 **1 → 3 → 2 → 4**。第 3 项（半块字符）是投入产出比最高的一个：终端"像素"从方形变成 1:2 长方形，伪 3D 的观感会跳一个档次。

### 突然对 ECS 感兴趣

```text
1. 先读 D.1，理解为什么本项目停在 D-1
2. 实验（在分支上做，不污染主线）：把 std::vector<Entity> 换成
   按类型分容器 + 一个 free function 遍历 —— 量一量性能差多少
3. 手写一个最小 ECS 原型（只需 Entity=id, Component 池, System 遍历），
   在 tests/ 里验证，不接进游戏
4. 读 EnTT 的 README 与 Archetype 存储的介绍，对照自己的实现
5. 结论通常是"本项目不需要" —— 这个结论本身就是收获
```
**关键纪律：ECS 实验必须在独立分支或 `tests/` 里做，绝不在主线中途重构。** 否则会重现 E-14。

### 突然想研究 3D

```text
不是从 3D 开始，而是从 S12 的 RayCaster 开始。
1. 单条射线 + 一个方块地图，把 DDA 的每一步画出来（用 Debug Overlay 显示格子）
2. 一列 → 全部列  → 得到墙体
3. 加 cos 校正 → 加 FOV → 加移动/转向
4. 加 z-buffer → 加精灵
5. 到这里，你已经实现过"透视投影 + 深度缓冲"了。
   然后才去对比真实的 4x4 矩阵 / 齐次坐标 / 透视除法，你会发现
   你现在缺的是"任意旋转"和"任意几何体"，而不是"3D"这个概念本身。
```

### 一张"随时可做"的清单（不用想，直接挑一个）

- [ ] 给 `@` 加颜色（256 色里找一个顺眼的）
- [ ] 敌人死亡时闪一下
- [ ] 顶栏加一个真实时钟
- [ ] 把地图从硬编码字符串改成文件加载
- [ ] 加一个"上一帧/本帧"切换（按 `P` 逐帧推进）
- [ ] 写一个 `--frames N` 命令行参数，跑 N 帧后自动退出（给 CI 用）
- [ ] 把 `Config` 从编译期常量改成运行时读 `mini.cfg`
- [ ] 录一个 GIF 放 README

---

## G. 版本规划

每个版本只定义 **2~3 个目标**。版本之间不存在"必须做完"的承诺。

| 版本 | 目标（不超过 3 条） | 对应阶段 | 判定标准 |
| --- | --- | --- | --- |
| **v0.1 "看得见"** | ① 现代 CMake 骨架 + ctest 能跑<br>② 终端出现一个带边框的 2D 画面<br>③ WASD 能控制 `@` 移动 | S0~S5 | 三条命令构建成功；终端里有 `@` 且能用 WASD 推着走 |
| **v0.2 "走得动"** | ① 200×200 地图 + 相机跟随<br>② 不能穿墙<br>③ 顶栏 FPS/FrameTime 实时显示 | S3,S6,S7 | 在比屏幕大得多的地图上自由走动，撞墙不穿模，FPS 数字稳定 |
| **v0.3 "玩得起来"** | ① 敌人会追会打（**S8 ✅ 2026-10-09**）<br>② 能射击、能击杀<br>③ HP/Ammo/Kills/Wave + 死亡重开 | S8~S10 | **一个外人可以上手玩 5 分钟的完整游戏**（这是全项目最重要的一次验收） |
| **v0.4 "看得清"** | ① F1~F4 Debug Overlay<br>② `PROFILE_SCOPE` 各子系统耗时表<br>③ 单元测试覆盖 math/collision/gamestate | S4,S11 | 能说出"渲染占了多少 ms"；`ctest` 全绿 |
| **v0.5 "像 3D"** | ① DDA 射线投射的走廊画面<br>② 无鱼眼扭曲<br>③ `F5` 在 2D/3D 视角间切换 | S12 | 同一个世界两种视角自由切换，走位在两个视角下一致 |
| **v1.0 "收得起来"** | ① 精灵 + z-buffer 遮挡正确<br>② README + GIF + CI（**只跑 MSVC**）<br>③ 干净克隆后三条命令能跑 | S13,S14 | 别人按 README 能复现；`git tag v1.0` |
| **v1.x+（不收口，纯玩）** | 颜色/雾、粒子、A*、行为树、Braille 渲染、关卡编辑器、Chrome Trace 导出…… | 弹性 | 想玩就开分支，不做完也不算失败 |

### 最后一句

> 这个项目的成功标准**不是** v1.0 那行字，而是：
> **某天晚上你打开 VS Code，只为了看那个 `@` 换成 `▓` 之后是什么样子。**
>
> 如果哪一步让你觉得"这是任务不是玩"，那一步就是设计错了 —— 砍掉它，回到上一版。

---

## H. 长期路线与扩展池（**无固定排期**）

> **2026-09-30 起本项目按「长期任务」推进**：不设按天的排期、不设截止日、不按假期切段。
> 推进顺序只由 `B` 节的 15 个阶段决定；一次做多少、隔多久做一次，由当时的精力决定。
> 本节只保留两样长期有用的东西：**里程碑路径**（做到哪算哪）和**扩展池**（有空再挑）。
> 原先的「7 天冲刺 / 42h 时间账 / Day1~Day7 排期 / 每天固定节奏」**已整节删除** ——
> 那种表格只在"和假期对齐"时才有意义，长期任务下它只会变成一份没人遵守的假计划。

### H.1 里程碑路径：三条，随时可以停

> 这不是排期，而是"**可以停在哪一站**"；每个终点的判定标准在 `G` 节的版本表里。

| 路径 | 覆盖阶段 | 终点 |
| --- | --- | --- |
| **保底** | S0 ~ S10 | **v0.3「玩得起来」：一个能玩的 2D 游戏**（能刷怪 / 能射击 / 能死能重开） |
| 标准 | + S11、S12 | v0.4「看得清」+ v0.5「像 3D」：Debug Overlay + 伪 3D 走廊 |
| 完整 | + S13、S14 | v1.0「收得起来」：精灵遮挡 + README/GIF/CI + `git tag v1.0` |

**降速 / 卡住时怎么砍（按此顺序，不要反过来）：**

1. 先冻结扩展池（H.2）—— 它本来就是"有富余才动"的东西；
2. 把当前阶段砍小到仍满足 **PLAN 原则 1**（能构建、能运行、能看到结果），砍掉的部分写进
   `doc/TECH_DEBT.md` 的 C 组并**附上触发条件**；
3. 最后才考虑砍掉尾部阶段（S13/S14）—— 反正是游乐场项目，砍了不算失败。

> **唯一值得守的锚点是 v0.3 的「能玩」。** 在它达成之前**禁止任何重构冲动** ——
> 想重构就写 `TODO` 丢进扩展池或 `TECH_DEBT`，等 v0.3 之后再评估。
> 大概率那时会发现根本不需要（这也是 D.1 想说的那件事的另一面）。

### H.2 扩展池（有空才挑，按性价比排序）

> **计划外的新点子一律扔进这里**，不要在主线阶段里插队 —— 想玩扩展池里的东西是合法的
> （那本来就叫玩），主线该走到哪一站还是走到哪一站。

| 想做的事 | 耗时 | 什么时候做 | 备注 |
| --- | --- | --- | --- |
| 256 色 + 按距离变暗/雾 | 1h | S6 之后的任一空档 | 视觉回报最高的一项 |
| `▀` / `▄` 半块字符（纵向分辨率翻倍）| 1h | S6 相机前后皆可 | 伪 3D 观感直接跳一档 |
| 命中/死亡小方块粒子 | 1h | S9 射击之后 | 廉价但很提气 |
| 地图从 `map.txt` 加载 | 1h | **S7 已完成（2026-10-08）→ 触发条件满足，随时可挑** | 之后调关卡不用重新编译 |
| A* 寻路（解决 S8 的卡墙）| 2.5h | **S14**（原写"S8 之后"；S8 已完成 → "卡墙"现在**亲眼可见**了，但按 S8 的设计意留到 S14 再解） | BFS 打底，顺手在 Debug Overlay 里画路径 |
| 装 Ninja 提速构建 | 0.5h | 任一空档（与阶段无关） | `winget install Ninja-build.Ninja`，一条命令，别折腾 |
| 给 `mini_tests` 加 `LABELS` 与 `TIMEOUT` 属性 | 1h | 用例总数 > 50 之后 | 让 `ctest -L fast` 与超时保护可用，见 `doc/CTEST_NOTES.md` |
| Profiler 导出 Chrome Trace JSON | 1.5h | S11 之后 | `chrome://tracing` 里看火焰图，成就感很强 |
| Braille 字符渲染（2×4 点阵）| 3h | 任一空档 | 纯玩，风险高收益也高，做不完就当实验 |
| 行为树替换 FSM | 2h | **不建议** | D.1 已说明：5 个状态的 FSM 更好维护 |

---

## 附录 A：命令速查

```powershell
# ---- 首次配置（家：本机，已完成，保留备查）----
git init -b main
cmake -S . -B build

# ---- 日常循环 ----
cmake -S . -B build
cmake --build build --config Debug
.\build\bin\Debug\MiniEngine.exe
ctest --test-dir build -C Debug --output-on-failure

# ---- 只跑一个测试 suite（**精确匹配**，不是前缀；见 doc/MINITEST_SPEC.md 第 3 节）----
.\build\bin\Debug\mini_tests.exe --list        # 先看有哪些 suite.name
.\build\bin\Debug\mini_tests.exe MathUtils     # S4 的四个：MathUtils / Vec2 / AABB / Ray
.\build\bin\Debug\mini_tests.exe AABB
# ⚠️ `mini_tests.exe Math` **不存在**（没有叫 Math 的 suite）；不带参数才是跑全部

# ---- Release ----
cmake --build build --config Release
.\build\bin\Release\MiniEngine.exe
ctest --test-dir build -C Release --output-on-failure      # 别忘了 -C

# ---- AddressSanitizer（MSVC 原生，不需要 clang）----
cmake --preset ms-asan
cmake --build --preset asan
ctest --preset asan

# ⛔ 不再有 clang 交叉验证（2026-09-29 决定：编译器只有 MSVC 一条路）
```

### 附录 A.1 首次推送到 GitHub（私有仓库）

```bash
# 1. 先在 GitHub 网页上建一个空仓库（不要勾 README / .gitignore / LICENSE）
#    建议名：mini-engine
# 2. 关联远程并推送（SSH 或 HTTPS 二选一）
git remote add origin https://github.com/<你的用户名>/mini-engine.git   # HTTPS，凭据管理器会弹浏览器登录
# git remote add origin git@github.com:<你的用户名>/mini-engine.git     # SSH
git push -u origin main
```

### 附录 A.2 任一台新机器首次 clone 后的必做四步（**关键**）

> **第 0 步（2026-10-02 新机实测后补）：动手前先 `cmake --version`。**
> 三台机器三个版本（家机 4.4.3 / 公司机 4.2.1 / 新机 4.0.0-rc3），照别台的报错查会查错方向。
> 新机实测到的两件事：① clone 后第一次 `cmake -S . -B build` 直接成功 ——
> `build/CMakeFiles/<版本>/CompilerIdCXX/CompilerIdCXX.exe` 就是"MSVC 工具链可用"的证据；
> ② `build/` 里若已留着别人/别次留下的配置，先 `Remove-Item -Recurse -Force build` 再配，
> 缓存里的生成器与绝对路径不会跟着机器走。

`.git/config` **不在版本控制里**，所以下面这些设置**不会跟着仓库同步**，每台新机器都要重设一次：

```bash
git clone https://github.com/<你的用户名>/mini-engine.git
cd mini-engine

# 1. 身份：**按机器各用各的**（2026-09-23 决定，不再要求两机一致）——
#    公司机用公司邮箱，家机用个人邮箱，各自设成**仓库 local** config 即可，
#    不会互相覆盖，也不会写进仓库。（家机：HUNK <wuunj@126.com>）
git config user.name  "Wuyangzhi"
git config user.email "wu.yangzh@aerok.com.cn"

# 2. 换行与路径：免疫 Git for Windows 默认的 core.autocrlf=true
git config core.autocrlf false
git config core.longpaths true      # build 目录层级深，避开 Windows 260 字符限制
git config core.quotepath false     # 中文文件名正常显示

# 3. 两机交替开发用 rebase，保持线性历史
git config pull.rebase true

# 4. 验证：列出的每个文件都应是 i/lf w/lf（不应出现 i/crlf 或 w/crlf）
git ls-files --eol
```

> ⚠️ **`build/` 绝不能跨机同步**（已在 `.gitignore` 里）。两台机器的 CMake 缓存、生成器、绝对路径都不同，到公司后第一件事是**重新 configure**，不要复制家里的 `build` 目录：
> ```bash
> cmake -S . -B build
> ```
> 另外，别把仓库放在 OneDrive / 坚果云等网盘同步目录里——网盘同步 `.git/` 会损坏仓库。

> 🌐 **还有个"第 5 步"，但只在 push / clone 超时时才需要**：新机器的代理端口**大概率不一样**。
> 症状→根因对照表、一条命令的修法、怎么找端口 → 见 **附录 F：网络 / 代理排障（GitHub 超时）**。

### 附录 A.3 两机交接的日常动作

```bash
# 离开一台机器前（务必 push，否则另一台看不到）
git add -A && git commit -m "wip: 离开前保存" && git push

# 到另一台机器后（先 pull 再动手，避免分叉）
git pull --rebase
```

> Git Bash（代码页 936）里 `git log` 的中文可能显示成乱码。看日志请用 **VS Code 的终端 / Git 面板**（UTF-8），或临时 `chcp 65001`。这只是显示问题，仓库内容与 GitHub 页面均正常。
>
> 关于作者邮箱：**2026-09-23 起按机器各用各的**（家机个人邮箱、公司机公司邮箱）。历史里那 2 个 commit 仍保留公司身份，**不打算重写** —— 重写历史会让两机 rebase 变麻烦，而作者名不同不影响任何功能。

## 附录 B：CMakePresets.json（**2026-09-29 已按实际落地更新**）

```json
{
  "version": 6,
  "configurePresets": [
    { "name": "ms-msvc-debug", "generator": "Visual Studio 17 2022",
      "binaryDir": "${sourceDir}/build",      "cacheVariables": { "CMAKE_CONFIGURATION_TYPES": "Debug;Release" } },
    { "name": "ms-asan",       "inherits": "ms-msvc-debug",
      "binaryDir": "${sourceDir}/build-asan", "cacheVariables": { "MINI_SANITIZE": "ON" } }
  ],
  "buildPresets": [
    { "name": "debug",   "configurePreset": "ms-msvc-debug", "configuration": "Debug" },
    { "name": "release", "configurePreset": "ms-msvc-debug", "configuration": "Release" },
    { "name": "asan",    "configurePreset": "ms-asan",       "configuration": "Debug" }
  ],
  "testPresets": [
    { "name": "debug",   "configurePreset": "ms-msvc-debug", "configuration": "Debug",   "output": { "outputOnFailure": true } },
    { "name": "release", "configurePreset": "ms-msvc-debug", "configuration": "Release", "output": { "outputOnFailure": true } },
    { "name": "asan",    "configurePreset": "ms-asan",       "configuration": "Debug",   "output": { "outputOnFailure": true } }
  ]
}
```

> 与原草案的差异：**删掉了 `ms-clang-debug`**（`toolset: ClangCL`）与它的 buildPreset；
> `ms-asan` 改为继承 MSVC 预设（ASan 用 MSVC 原生的 `/fsanitize=address`，不需要 clang）；
> 补上了 `release` / `asan` 两组 build/testPreset（原草案只有一个 `debug` testPreset，
> 导致 `ctest --preset` 在 clang/asan 下没法用）。

## 附录 C：.gitignore

```gitignore
build*/
out/
.vs/
.vscode/*.json
*.user
CMakeUserPresets.json
*.log
```

## 附录 D：Commit 节奏

- **一个阶段 = 3~6 个 commit**，每个 commit 都要能编译。
- 分支：主线 `main` 始终可运行；阶段在 `stage/07-collision` 上做，能跑通再合；**纯实验（ECS/Ray casting 试验）另起 `exp/` 前缀分支，不合并也不删除**。
- 信息风格（不用 Conventional Commits 也完全可以）：

```text
feat: add terminal framebuffer with ansi present
feat: add windows console abstraction
feat: add keyboard input with pressed/held/released
feat: add player movement and tile map
fix:  disable quick-edit mode so mouse click does not freeze input
feat: add aabb tile collision with per-axis resolution
feat: add enemy fsm with patrol/chase/attack
feat: add projectile system and kills counter
perf: skip unchanged rows when presenting
docs: record coordinate convention in plan
```

- 打 tag 的时机就是 G 表的版本号：`v0.1`、`v0.2`…… **打 tag 那天别写代码，只写 README。**

## 附录 E：每阶段收工时问自己三句话

1. `cmake --build build --config Debug` 绿了吗？exe 跑起来了吗？（**可运行优先**）
2. 我今天新加的这个类/层，是**问题逼出来的**，还是我觉得"应该有个"？（**避免过度设计**）
3. 有哪个"好玩的结果"能一眼看见？（**保持动力**）

三个都是"是"，就 commit 并收工。

---

## 附录 F：网络 / 代理排障（GitHub 超时）

> **2026-09-29 实测记录。** 这节跟引擎无关，但它**下次换机器几乎必然还要撞一次**
> （本项目就是家 / 公司两台机器交替开发），所以记在最容易被翻到的地方。

### F.1 症状 → 根因对照

| 症状 | 根因 | 去哪修 |
| --- | --- | --- |
| `git push` / `clone` **卡住，然后是超时** | git **没走本机代理**，直连 `github.com` 被挡 | **F.2** 一条命令 |
| 卡很久才报错，不是"立刻拒绝" | `http.connecttimeout 120`（本机 global 就是这个值）| **F.3** |
| 换了机器 / 换了代理软件，又开始超时 | 端口变了（见 F.4 的表）| **F.4** 先找端口 |

### F.2 修：一条命令（**只让 GitHub 走代理**）

```powershell
git config --global http.https://github.com.proxy http://127.0.0.1:7897
```

**为什么用这个长写法，而不是裸的 `http.proxy`**：
`http.<url>.proxy` 是**按 URL 前缀生效**的 —— 只有 `github.com` 走代理，国内站点保持直连。
写成裸的 `http.proxy` 会把**所有**仓库、所有域名都强制塞进代理
（以后加 Gitee、连公司内网 git 时就会被它坑）。

### F.3 顺手把"假死"改回"快速失败"

本机 global 里原本有这三条，**本意是"防超时"，实际效果是把"连不上"伪装成"假死"**：

| 配置 | 原值 | 实际后果 |
| --- | --- | --- |
| `http.connecttimeout` | 120 | 光"建连接"就肯等 **120 秒** |
| `http.lowspeedlimit` | 0 | **慢速守卫被禁用**（0 = 关）→ 卡住时永远不会主动放弃 |
| `http.lowspeedtime` | 600 | 配合上一条，完全失效 |

```powershell
git config --global http.connecttimeout 15      # 连不上就 15 秒报错，别对着屏幕猜
```

（`lowspeed*` 两条可以留着，也可以 `git config --global --unset http.lowspeedlimit` 删掉。）

### F.4 先找端口：本机代理到底在哪

```powershell
netstat -ano | Select-String "LISTENING" | Select-String ":7897|:7890|:10809|:1080"
# 拿最后一列的 PID 看是谁：
tasklist /FI "PID eq <PID>"
```

本次实测就是这么找到的：**PID 31032 = `verge-mihomo.exe`** → Clash Verge 的内核 → 端口 **7897**。

| 代理软件 | 常见端口 |
| --- | --- |
| **Clash Verge / mihomo**（本机就是它）| **7897**（混合端口，HTTP/SOCKS 都吃）|
| Clash for Windows | 7890（混合）/ 7891 |
| v2rayN | 10809（HTTP）/ 10808（SOCKS）|
| 通用 SOCKS5 | 1080 → 写法是 `socks5://127.0.0.1:1080` |

### F.5 验证 & 备用梯子

```powershell
git ls-remote origin main        # 只读；能列出 ref 说明网络+认证都通
git push -v                      # 卡的时候看卡在哪个阶段
```

按概率排的备用招：

1. `git config --global http.version HTTP/1.1` —— HTTP/2 在**跨境劣化链路**上容易挂；
2. **SSH over 443**（22 端口常被封、443 常通）：`~/.ssh/config` 写
   `Host github.com` / `HostName ssh.github.com` / `Port 443` / `User git`，
   再把 remote 换成 `git@github.com:HUNK200/MiniEngine.git`（前提：GitHub 上已配 SSH key）；
3. **换网络**（手机热点）—— 跨境链路抖动很常见，**有时候就是等一会儿**。

### F.6 两条纪律

1. **这些都是 `--global` 配置**：跟着**这台机器**，不属于仓库 → **换机器要重配**。
   反过来，在 `git config --global --list` 里看到 `<url>.proxy` 时要知道它是干嘛的。
2. **push 之后拿 hash 验证，别靠感觉**：
   ```powershell
   git rev-parse --short HEAD
   git rev-parse --short origin/main     # 两个相等才算真的上去了
   ```
   remote-tracking ref **只在成功同步之后才更新**，所以它本身是可信的判据 ——
   比"终端里没报错"可靠。（2026-09-29 实测：`885c861` == `885c861`，10 个 commit 全部落地。）
