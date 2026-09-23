# MiniEngine 实施方案

> 本文件是对 `input_plan.md` 的落地回答，按 A~G 结构组织。
> 定位：**C++ + Game Engine Playground**，不是一个必须完成的工程。
> 原则：可运行优先 / 不过度设计 / 每阶段有"好玩的结果"。
>
> **时间有限？直接跳到 H 节**（7 天冲刺计划：每天做什么、掉队怎么砍、富余时间去哪）。
>
> ⚠️ **项目约定**：AI 默认只产出**方案**、不写实现代码，具体实现由我自己敲（判定标准与例外见 `AGENTS.md`）。

---

## 0. 环境基线（家机 / 公司机分列）

> ⚠️ 本节原文只有一套路径，而且被写成了"本机"。**2026-09-23 在家机逐条实测，发现两台机器
> 的工具路径根本不同**（VS 家机装在 E 盘、clang 版本也不同），照一份基线敲命令会直接找不到工具。
> **以后每台机器只信自己这一列，动手前先跑一次 `cmake --version`。**

| 项 | 家机（2026-09-23 实测） | 公司机 | 对方案的影响 |
| --- | --- | --- | --- |
| CMake | **4.4.3** @ `C:\Program Files\CMake\bin`（当天用 `winget install Kitware.CMake` 补装，**装之前家机完全没有 CMake**） | 4.2.1（原记录，未复测） | **CMake 4.x 已移除 `cmake_minimum_required(VERSION < 3.5)` 的兼容**，最低写 `3.20` |
| MSVC | VS 2022 Community @ **`E:\Program Files\VisualStudio`**（**不在** C 盘）；装了 14.38.33130 / 14.44.35207 两套 toolset；CMake 生成器实测选中 **19.38.33145.0** | cl 14.44 (19.44)（原记录，未复测） | 主力编译器，默认生成器 `Visual Studio 17 2022`；生成器选哪套 toolset 由 VS 自己定，别在文档里硬编码 |
| clang | **19.1.5** @ `…\VisualStudio\VC\Tools\Llvm\bin`（**不在** `C:\Program Files\LLVM`） | 17.0.6（原记录，未复测） | 第二编译器，走 VS 生成器 `-T ClangCL`，不用单独装 Ninja |
| Windows SDK | 10.0.22621.0 | 未记录 | — |
| g++ / gcc | **未安装** | **未安装** | 不验证 Linux 分支，Linux 代码只写不跑 |
| ninja / make | **未安装**（不阻塞，VS 生成器可用） | **未安装** | 多配置（VS）生成器，所以**必须带 `--config`／`-C`** |
| Git | 2.52.0；仓库 local 身份 `HUNK <wuunj@126.com>` | 仓库 local 身份 `Wuyangzhi <wu.yangzh@aerok.com.cn>` | **身份按机器各用各的**（见 A.2），不要求两机一致 |
| 终端 | Git Bash，`TERM=xterm-256color`，代码页 **936 (GBK)** | 同左 | 必须切 UTF-8，否则 `┌─┐@#*` 全乱码 |
| 项目当前状态 | **S0 完成**（2026-09-23）：`ctest` 1/1 passed | — | 下一步进 S1 FrameBuffer |

### 标准命令（多配置生成器下）

```bash
# 配置 + 构建（Debug）
cmake -S . -B build
cmake --build build --config Debug

# 运行（阶段 0 起就把 exe 统一输出到 build/bin/）
./build/bin/Debug/MiniEngine.exe

# 测试（-C 不能漏，否则 CTest 找不到配置）
ctest --test-dir build -C Debug --output-on-failure
```

可选：`cmake -S . -B build-clang -G "Visual Studio 17 2022" -T ClangCL` 得到 clang 构建，用来交叉验证 MSVC 没抓到的警告。

> 想让构建快一点再装 Ninja（`winget install Ninja-build.Ninja`），届时 `-G Ninja -DCMAKE_BUILD_TYPE=Debug`，命令回到单配置风格。**这是可选优化，不是前置依赖。**

> 🔧 **家机 2026-09-23 才补装上 CMake**：已经开着的 Git Bash 不会自动拿到新的 PATH，**重开一个终端** `cmake` 就能用了；旧终端里临时用全路径 `"/c/Program Files/CMake/bin/cmake.exe"`。

### 平台策略（已确认）

**Windows 优先 + Linux 预留抽象层**：
- 本机只验证 Windows Terminal + MSVC；
- 从阶段 2 起所有平台相关代码进 `engine/platform/`，Linux 分支（termios/fcntl/ioctl）写好、能通过编译检查，但暂不在本机运行；
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
  终端 (Windows Terminal / conhost / Linux tty)

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
├── CMakeLists.txt              # 顶层：标准/选项/子目录
├── CMakePresets.json           # 预设 ms-msvc / ms-clang，避免手敲参数
├── .gitignore
├── .editorconfig               # 统一 UTF-8 / LF-CRLF / 缩进
├── input_plan.md
├── MiniEngine_PLAN.md          # 本文件
├── README.md                   # 阶段 14 再写，别提前写
├── main.cpp                    # 唯一入口，<20 行
│
├── engine/
│   ├── CMakeLists.txt          # add_library(engine STATIC ...)
│   ├── include/mini/           # 公共头（对外只暴露这层）
│   │   ├── core/       Application.h  Time.h  Logger.h  Config.h  NonCopyable.h
│   │   ├── platform/   Console.h  ConsoleWindows.h  ConsoleLinux.h  Input.h  KeyCode.h
│   │   ├── math/       Vec2.h  Vec3.h  Rect.h  AABB.h  Ray.h  MathUtils.h
│   │   ├── render/     Cell.h  Color.h  FrameBuffer.h  Renderer2D.h
│   │   │               Camera.h  Viewport.h  Renderer3D.h  RayCaster.h
│   │   ├── world/      World.h  Entity.h  Transform.h  Velocity.h  Health.h
│   │   │               TileMap.h  Scene.h
│   │   ├── physics/    Collision.h  PhysicsSystem.h
│   │   ├── ai/         StateMachine.h  EnemyController.h
│   │   └── debug/      DebugOverlay.h  Profiler.h
│   └── src/                    # 与 include 镜像的 .cpp
│
├── game/
│   ├── CMakeLists.txt          # add_library(game STATIC ...)
│   ├── include/game/
│   │   ├── Game.h  GameState.h  Player.h  Enemy.h
│   │   ├── Weapon.h  Projectile.h  Pickup.h  WaveManager.h  Hud.h
│   └── src/
│
└── tests/
    ├── CMakeLists.txt
    ├── MiniTest.h              # 自制极简断言框架（~80 行）
    ├── test_main.cpp
    ├── test_math.cpp
    ├── test_collision.cpp
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
- 只有 `platform/` 里允许出现 `#ifdef _WIN32` / `#include <windows.h>`。**这条规则的唯一例外是阶段 1**（还没抽象平台时允许临时写在 `render/Console` 里），阶段 2 必须把它挪走。

### A.4 为什么这么设计

1. **按"变化频率"分层，而不是按"技术名词"分层。** `math` 几乎不变，`render` 每阶段都变，`game` 天天变。把易变的放上层、稳定的放下层，改游戏逻辑时不会牵动渲染代码。
2. **`engine` 与 `game` 物理分家**，是为了强制回答一个问题："这个东西到底是引擎能力，还是游戏逻辑？"——这个判断力比代码本身值钱。同时让 `engine` 可以被 `tests` 单独链接。
3. **`platform` 单独一层，而不是用宏散落各处**，因为这是全项目唯一真正"必须分平台"的地方（终端模式、键盘、窗口尺寸）。集中在一处，Windows/Linux 的差异就只有一眼能看完的 200 行。
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
screen = round((world - camera.pos) * 1.0)      // 1 格 = 1 字符
world  = camera.pos + screen
map_tile = floor(world)                          // 注意是 floor，不是 (int) 截断（负数会错）
```

两条必须写进注释的规矩：
- **实体位置用 float，画的时候才 `round`**；直接在 int 上做移动，斜向速度会被吃掉。
- **`floor` 而不是 `(int)`** 转 tile 索引；负数坐标下截断会指向错误的格。

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

### S2 Platform 抽象层 + 键盘输入

| 项 | 内容 |
| --- | --- |
| 目标 | `platform::Console` + `platform::Input`，`@` 能响应按键移动（先用瞬时移动验证输入通路） |
| 实现 | 接口类 `IConsole`；Windows 用 `ReadConsoleInput`/`PeekConsoleInput`，Linux 用 `termios` raw + `fcntl(O_NONBLOCK)`；`KeyCode` 枚举；三态 `Pressed/Held/Released` |
| C++ 知识 | 抽象接口、`std::unique_ptr` + 工厂函数、RAII（构造开终端模式，析构恢复）、`enum class`、位标志 |
| 引擎知识 | 输入轮询（poll）模型 vs 回调模型、按键边沿与持续状态 |
| 规模 | ~350 行（含 Linux 分支） |
| 可运行效果 | 屏幕上实时显示 `W: Held  SPACE: Pressed  Q: Released`；按 Q 退出 |
| 验收 | 长按 W 显示 Held；松开显示 Released 仅一帧；**鼠标在终端里点一下之后输入仍然正常** |
| 坑 | ① 必须 `&~ENABLE_QUICK_EDIT_MODE` 并配合 `ENABLE_EXTENDED_FLAGS`，否则点一下控制台就"卡死"；② Linux 忘恢复 termios，退出后终端不回显；③ 忘了处理 `WINDOW_BUFFER_SIZE_EVENT`，改窗口大小后画面错位 |

### S3 Game Loop / Time / FPS

| 项 | 内容 |
| --- | --- |
| 目标 | `Application` 类 + 主循环 + 顶栏实时 FPS/FrameTime |
| 实现 | `Time::Update()`（`steady_clock`）、滑动窗口 FPS、混合睡眠限帧、脏行 diff 提交 |
| C++ 知识 | `std::chrono`、`duration_cast`、`std::this_thread::sleep_for`、RAII 计时、静态局部变量 |
| 引擎知识 | variable dt vs fixed dt、dt clamp、帧率限制、为什么"限帧"和"垂直同步"不是一回事 |
| 规模 | ~250 行 |
| 可运行效果 | 顶栏 `FPS: 59.9  Frame: 16.7ms`；把限帧改成 300 能看到 FPS 上升；dt 数值稳定不抖 |
| 验收 | 限帧 60 时 FPS 落在 58~61；`dt` 长时间运行不出现尖峰（打印 max dt 验证） |
| 坑 | **Windows 上 `Sleep(1)` 实际睡 ~15.6ms**，必须混合睡眠（先睡到差 2ms，再自旋） |
| 设计结论 | **用 variable dt + `dt = min(dt, 0.05)`**。本项目没有需要固定步长的物理（无重力、无弹性），引入 fixed update 只会让输入手感变差。等 S9 子弹高速穿透问题真的出现时，再考虑"子弹用子步进"这种局部方案，而不是全引擎改结构 |

### S4 Math 库 + 单元测试

| 项 | 内容 |
| --- | --- |
| 目标 | `Vec2/Vec3/Rect/AABB/Ray` 全 `constexpr` 头文件 + 测试覆盖 |
| 实现 | 运算符重载、`Length/LengthSq/Normalized`、`AABB::Intersects`、`Ray::IntersectsAABB`(slab 法) |
| C++ 知识 | `constexpr`、运算符重载、`[[nodiscard]]`、值语义、模板（`Vec2` 用 `float` 特化即可，别一上来就模板化 `T`） |
| 引擎知识 | 为什么要 `LengthSq`（避免 sqrt）、AABB 的 slab 法、法线方向 |
| 规模 | ~300 行 + ~200 行测试 |
| 可运行效果 | ctest 全绿；可选 demo 打印"两个盒子相交/不相交" |
| 验收 | 覆盖：零向量归一化保护、NaN 输入、AABB 边界相接（`<=` vs `<` 要明确）、Ray 起点在盒内 |
| 坑 | 归一化零向量产生 NaN 并污染整个物理循环（必须返回 `{0,0}`）；浮点相等判断 |

### S5 World / Entity —— `@` 可以移动

| 项 | 内容 |
| --- | --- |
| 目标 | `TileMap`(200×200) + 简单 `Entity` 结构体 + `World` 容器 + 移动逻辑 |
| 实现 | `struct Entity { Transform transform; Velocity velocity; char symbol; EntityId id; bool active; }`；`World` 持有 `std::vector<Entity>` |
| C++ 知识 | 聚合初始化、`std::optional`、`std::span`、引用 vs 值语义、`erase-remove` vs `swap-erase` |
| 引擎知识 | 实体标识（id vs 指针 vs 索引）、迭代顺序、更新顺序的重要性 |
| 规模 | ~300 行 |
| 可运行效果 | `@` 按 WASD 在地图上移动，速度受 `dt` 影响（改 dt 速度不变） |
| 验收 | 100 FPS 和 300 FPS 下，`@` 走过同一段距离耗时相同 |
| 坑 | 在遍历 `vector` 时删除元素导致迭代器失效（S9 会撞上，这里先埋好概念） |

### S6 Camera / Viewport —— 世界大于屏幕

| 项 | 内容 |
| --- | --- |
| 目标 | World→Camera→Screen 转换，Camera 平滑跟随 + 边界夹取 |
| 实现 | `Camera{ Vec2 pos; Vec2 target; }`、`WorldToScreen/ ScreenToWorld`、`lerp` 跟随、只遍历可见 tile |
| C++ 知识 | `clamp`、`lerp`、`[[nodiscard]]`、整数/浮点边界计算 |
| 引擎知识 | 视口、滚动、边界处理、"只画看得见的"（剔除） |
| 规模 | ~250 行 |
| 可运行效果 | 大地图上镜头跟随 `@`，走到地图角落时镜头停住、不出现黑边 |
| 验收 | 地图比终端大得多时，屏幕上永远不出现"世界之外"的空白；`@` 始终在画面内 |
| 坑 | 屏幕坐标出现半格偏移（`k - 0.5` 的取整方向）；相机移动导致整屏抖动（应对相机做 `floor`） |

### S7 Collision —— 不能穿墙

| 项 | 内容 |
| --- | --- |
| 目标 | 基于 tile 的碰撞 + **分轴移动**，实体间 AABB 推挤 |
| 实现 | `MoveAndCollide(entity, delta)`：先移 X 轴单独检测、再移 Y 轴；tile 查询 `TileMap::IsSolidAt(Vec2)` |
| C++ 知识 | 早退、`epsilon` 比较、`std::array` vs `vector`、const 正确性 |
| 引擎知识 | 碰撞检测 vs 碰撞响应、AABB 分轴解算、为什么不能一次移完再解算（会穿角） |
| 规模 | ~250 行 |
| 可运行效果 | 贴着墙走很顺，斜向顶墙角不会卡住也不会穿过去；F2 显示碰撞盒 |
| 验收 | 疯狂按键撞墙 30 秒不穿模；沿墙滑行（wall slide）手感存在 |
| 坑 | 一次移动整个 delta 后再检测 → 高速下穿墙；用"拒绝移动"而不是"分轴"→ 贴墙卡顿 |

### S8 Enemy + AI 状态机

| 项 | 内容 |
| --- | --- |
| 目标 | `Idle → Patrol → Chase → Attack → Dead` 五态，`E` 会追 `@` |
| 实现 | 表驱动 FSM（`switch` 或 `std::function` 表都行，先 `switch`）；距离阈值切换；Patrol 用预置路径点往返 |
| C++ 知识 | `enum class`、`switch` 穷尽性、`std::function`、状态与数据分离 |
| 引擎知识 | 有限状态机、感知（视距/距离）、更新顺序（AI 用上一帧位置还是最新位置） |
| 规模 | ~350 行 |
| 可运行效果 | `E` 沿直线追 `@`，距离 < 1.5 开始攻击扣血，`@` 跑远后 `E` 返回 Patrol |
| 验收 | F3 打开后能实时看到 `State / Target / Distance`；5 个敌人同时追不出现抖动打转 |
| 坑 | 相邻敌人互相推挤导致"抖动"（先忽略实体间碰撞解算的停止条件）；Chase 直接朝玩家走会卡在墙后（**这一阶段故意不做寻路**，把 A* 留到 S14，让"卡墙"成为亲眼看到的动机） |
| 设计结论 | **状态机够了，不要上行为树。** 5 个状态的行为树只会更难看懂 |

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
| 实现 | 256 色渐变做距离雾；命中小方块粒子；A* 让 S8 的"卡墙"问题真正有解；GitHub Actions 矩阵 MSVC+clang |
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
option(MINI_SANITIZE    "Enable ASan/UBSan (clang/gcc)" OFF)

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
    src/platform/ConsoleLinux.cpp
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
  └─ Engine::Application            // 组装 + 拥有所有子系统
        ├─ Logger&                  // 静态/单例皆可，先简单
        ├─ platform::Console&       // 终端模式：UTF-8 / VT / 隐藏光标 / 尺寸
        ├─ platform::Input&         // Poll() → 按键三态
        ├─ render::FrameBuffer&     // 后缓冲：w*h 个 Cell
        ├─ core::Time&              // deltaTime / fps / frameTime
        └─ std::unique_ptr<IGame>   // 游戏侧实现，Application 不认识具体游戏
```

关键点：**`Application` 拥有 `IGame`，`Game` 拥有游戏内容，`Application` 不认识 Player。** 这样 `engine` 库不会反向依赖 `game`。

```cpp
// engine/include/mini/core/Application.h   —— 骨架，仅声明
namespace mini::core {

struct AppConfig { int width = 80; int height = 25; int targetFps = 60; };

class IGame {
public:
    virtual ~IGame() = default;
    virtual void OnStart()  = 0;
    virtual void OnUpdate(const struct FrameContext& ctx) = 0;  // dt / input / time
    virtual void OnRender(render::FrameBuffer& fb) = 0;
    virtual bool ShouldQuit() const = 0;
};

class Application {
public:
    explicit Application(AppConfig cfg);
    ~Application();                       // RAII：恢复终端、显示光标、离开备用屏

    void Run(IGame& game);
private:
    AppConfig                     m_cfg;
    std::unique_ptr<platform::Console> m_console;   // 工厂：Win/Lin 各一份实现
    std::unique_ptr<platform::Input>   m_input;
    render::FrameBuffer                m_frameBuffer;
    core::Time                         m_time;
};

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

enum class Key { Unknown, W, A, S, D, Q, E, Space, Escape, F1, F2, F3, F4, ArrowUp, ArrowDown, ArrowLeft, ArrowRight, Enter, R, /*...*/ };

enum class KeyState { Released, Pressed, Held };   // Pressed 只维持一帧

class Input {
public:
    virtual ~Input() = default;
    virtual void BeginFrame() = 0;          // 把 Pressed 降级为 Held，处理 Released
    virtual void Poll()       = 0;          // 非阻塞读事件
    virtual bool IsDown(Key k) const = 0;   // Held || Pressed
    virtual KeyState Get(Key k) const = 0;
    virtual bool QuitRequested() const = 0; // Ctrl+C / ESC / 窗口关闭
};

std::unique_ptr<Input> MakeInput();         // 平台工厂（.cpp 里 #ifdef 分派）
} // namespace mini::platform
```

### C.4 Game Loop（第一阶段定稿）

```cpp
void Application::Run(IGame& game)
{
    game.OnStart();
    while (!game.ShouldQuit() && !m_input->QuitRequested())
    {
        m_time.BeginFrame();                 // 记录帧起点，算 dt / fps

        m_input->Poll();                     // 把键盘事件收进内部状态表

        FrameContext ctx{ m_time.Delta(), *m_input, m_time };
        game.OnUpdate(ctx);                  // ← 目前只有 Game 层更新

        m_frameBuffer.Clear();               // 后缓冲
        game.OnRender(m_frameBuffer);        // 全部绘制进后缓冲
        m_console->Present(m_frameBuffer);   // 一次性提交（唯一的终端写入）

        m_input->BeginFrame();               // 三态状态机推进
        m_time.EndFrame();
        m_time.LimitFPS(m_cfg.targetFps);    // 混合睡眠限帧
    }
}
```

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
| 光标归位 | `ESC[H` | `SetConsoleCursorPosition` / `ioctl` | ANSI 更简单 |
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

**终端尺寸变化**：Windows 监听 `WINDOW_BUFFER_SIZE_EVENT`，Linux 用 `ioctl(TIOCGWINSZ)` + 每帧比较；变化时 `FrameBuffer::Resize()` + `ESC[2J` 清屏重画 + 相机边界重新 clamp。

**Windows / Linux 兼容**：全部差异集中在 `ConsoleWindows.cpp` / `ConsoleLinux.cpp`，公共头里**一个 `#ifdef` 都不出现**。Linux 侧除了 VT 序列（Linux tty 原生支持）还要额外做 `termios` raw 模式，这是唯一比 Windows 麻烦的地方。

---

## D. 架构演进

### D.1 实体模型：什么时候升级到 ECS

```text
D-0  单结构体            S5     struct Entity { Transform; Velocity; char symbol; bool active; };
      ↓ 当"不是所有实体都有所有字段"开始烦人
D-1  Entity + 可选字段     S8~S9  加 Health / Collider / AiState 字段，用 active/enabled 标记区分
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
| E-3 | **终端闪烁** | 画面撕裂/闪动 | 一帧一次 `write`、`ESC[H` 覆写而非 `ESC[2J`、S3 加脏行 diff | S1/S3 |
| E-4 | **Windows 输入卡死** | 鼠标在终端里点一下后无响应 | 关闭 `ENABLE_QUICK_EDIT_MODE`（需同时置 `ENABLE_EXTENDED_FLAGS`） | S2 |
| E-5 | **Sleep 精度** | 限帧 60 实测 32 FPS | 混合睡眠：`sleep_for(remain - 2ms)` 后自旋；或 `timeBeginPeriod(1)`（可选，需链 `winmm`） | S3 |
| E-6 | **多配置生成器** | `ctest` 找不到测试、exe 路径猜错 | 所有构建命令带 `--config Debug`，ctest 带 `-C Debug`；统一 `CMAKE_RUNTIME_OUTPUT_DIRECTORY` | S0 |
| E-7 | **CMake 4.x 兼容** | `cmake_minimum_required(VERSION 3.5)` 直接报错 | 最低写 3.20 | S0 |
| E-8 | **坐标系混乱** | 画面整体偏移半格、走位比视觉慢一格 | 定死 A.5 的四层约定；实体浮点、tile 用 `floor`；写一个坐标转换的单元测试 | S4/S6 |
| E-9 | **迭代器失效** | 射击时随机崩溃 | 禁止"遍历中增删"；用延迟删除队列（`toRemove` 列表）或 `swap-erase` 倒序遍历 | S9 |
| E-10 | **穿透** | 子弹跳过敌人、玩家高速穿墙 | 分轴移动（S7）；子弹子步进（S9）；`dt` clamp（S3） | S7/S9 |
| E-11 | **Game Loop 设计错误** | 高刷屏上游戏速度变快、暂停恢复瞬移 | variable dt 且 `dt = min(dt, 0.05)`；暂停时不累积时间 | S3/S10 |
| E-12 | **终端改尺寸** | 画面错位、越界写 | `WINDOW_BUFFER_SIZE_EVENT`/`TIOCGWINSZ` + `FrameBuffer::Resize` + 重画 | S2/S6 |
| E-13 | **Ray Casting 鱼眼/裂缝** | 墙呈弧形、墙面有竖缝 | `cos` 校正；DDA 的 `sideDistX/Y` 初值按方向取 `(1-delta)` 或 `delta`；`rayDir` 归一化 | S12 |
| E-14 | **ECS 过度设计** | 花两天改架构，游戏没进展 | 见 D.1：停在 D-1，需要时按具体类型分容器 | S10+ |
| E-15 | **Profiler 失真** | 计时器被优化掉、被测代码因分配而变慢 | `std::atomic_signal_fence`/`DoNotOptimize`、固定大小环形缓冲、零分配 | S11 |
| E-16 | **Linux 分支腐烂** | 半年后发现 Linux 编译不过 | 每次改 platform 后跑一次 `-T ClangCL` 构建（至少保证语法/头文件正确）；本机没 g++，别假装双平台 | S2+ |
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
| **v0.3 "玩得起来"** | ① 敌人会追会打<br>② 能射击、能击杀<br>③ HP/Ammo/Kills/Wave + 死亡重开 | S8~S10 | **一个外人可以上手玩 5 分钟的完整游戏**（这是全项目最重要的一次验收） |
| **v0.4 "看得清"** | ① F1~F4 Debug Overlay<br>② `PROFILE_SCOPE` 各子系统耗时表<br>③ 单元测试覆盖 math/collision/gamestate | S4,S11 | 能说出"渲染占了多少 ms"；`ctest` 全绿 |
| **v0.5 "像 3D"** | ① DDA 射线投射的走廊画面<br>② 无鱼眼扭曲<br>③ `F5` 在 2D/3D 视角间切换 | S12 | 同一个世界两种视角自由切换，走位在两个视角下一致 |
| **v1.0 "收得起来"** | ① 精灵 + z-buffer 遮挡正确<br>② README + GIF + CI（MSVC & clang）<br>③ 干净克隆后三条命令能跑 | S13,S14 | 别人按 README 能复现；`git tag v1.0` |
| **v1.x+（不收口，纯玩）** | 颜色/雾、粒子、A*、行为树、Braille 渲染、关卡编辑器、Chrome Trace 导出…… | 弹性 | 想玩就开分支，不做完也不算失败 |

### 最后一句

> 这个项目的成功标准**不是** v1.0 那行字，而是：
> **某天晚上你打开 VS Code，只为了看那个 `@` 换成 `▓` 之后是什么样子。**
>
> 如果哪一步让你觉得"这是任务不是玩"，那一步就是设计错了 —— 砍掉它，回到上一版。

---

## H. 国庆 7 天冲刺计划（10/1 ~ 10/7，每天 6h+，目标：做着玩）

### H.0 先算清时间账：看着富余，其实是"刚好打平"

| 项 | 小时 |
| --- | --- |
| 可用 | 7 × 6 = **42h**（按 38h 规划，留"今天没状态"的余量）|
| 主线 S0~S14 | B 节按每阶段 ≤1.5h 的理想值是 ~22.5h，**实测含调试/踩坑要 32h** |
| 每天 1h 自由块 × 7 | 7h（"做着玩"这个目标靠它兑现）|
| 合计需求 | **约 40h ≈ 可用 42h** |

**结论：没有可以随便挥霍的空间。** 富余时间只有三个指定用途，不许挪走：

1. 补掉某天没做完的部分（最可能发生在 Day 5、Day 6）；
2. Day 7 的收口缓冲；
3. Day 7 上午的扩展池自由探索。

> ⚠️ **富余时间最大的风险，是被第一天拿去买 CMake 和架构的完美主义。**
> **Day 1 的"骨架 + 抽象层"时间硬上限 60 分钟**，超了就是在过度设计（见 E-14 / E-20）。
> 记住：这个假期真正富余的不是时间，是"保底路径只要 22h"（见 H.3）——你随时可以少做两天而不算失败。

### H.1 七天排期

| 天 | 日期 | 主线阶段 | 主线 | 自由 | 当天结束时**必须能跑出来**的东西 |
| --- | --- | --- | --- | --- | --- |
| Day1 | 10/1 | S0 骨架 · S1 画面 · S2 输入 | 5.0h | 1.0h | 终端里 `@` 出现并被 WASD 推着走；按下/按住/松开三态在屏幕上可见；退出后终端不出坏 |
| Day2 | 10/2 | S3 GameLoop/Time · S4 Math + 测试 | 4.5h | 1.5h | 顶栏 FPS≈60 稳定、dt 不抖；200×200 地图上移动且速度与 FPS 无关；`ctest` 全绿 |
| Day3 | 10/3 | S5 Entity · S6 Camera | 4.5h | 1.5h | 相机平滑跟随 `@`；走到地图边缘不露出"世界之外"的空白 |
| Day4 | 10/4 | S7 碰撞 · S8 AI（**轻量日**）| 4.0h | 2.0h | 撞墙不穿模、贴墙不卡顿；`E` 会追会打、`@` 跑远后回 Patrol；F2/F3 可视化能开 |
| Day5 | 10/5 | S9 射击 · S10 HUD/Wave | 4.5h | 1.5h | **v0.3：一个能玩的完整游戏**（刷怪 → 射击 → 掉血 → 死亡 → 按 R 重开）|
| Day6 | 10/6 | S11 Debug/Profiler · S12 Ray Casting | 5.5h | 0.5h | F4 显示各子系统耗时表；第一人称走廊且无鱼眼扭曲；F5 在 2D/3D 视角间切换 |
| Day7 | 10/7 | S13 Sprite/z-buffer · S14 收口 | 4.0h | 2.0h | 敌人被墙正确遮挡；README + GIF + CI；`git tag v1.0`（**这一天不写新功能**）|

合计：主线 32h + 自由 10h = 42h。Day4 故意做轻量日，Day6 最重但放在已经熟练之后。

### H.2 每天的固定节奏

```text
[0:00-0:15]  热身：跑一遍昨天的 exe，确认没坏                ← 别省，这是"每天都能玩"的保障
[0:15-1:45]  主线块 1（90 min）
[1:45-2:00]  离开椅子                                        ← 7 天 × 6h 最容易在这里烂尾
[2:00-3:30]  主线块 2（90 min）
[3:30-4:30]  自由块：1 小时纯玩，不设目标，想到什么做什么      ← "做着玩"靠它兑现
[4:30-6:00]  主线块 3 / 补昨天的坑 / 当天收口
[6:00-6:10]  commit + 在 PROJECT_LOG.md 写下"明天第一件事"     ← 3 行以内
```

**三条硬规则：**

1. **40 分钟规则**：一个问题卡超过 40 分钟没有进展 → 写 `// TODO(hack): 原因` + 用最蠢的办法绕过去，继续推进。
   假期里"停摆半天"是唯一真正不可恢复的损失。烂代码不是问题（S11 的 Profiler 会帮你找回来），**停摆才是**。
2. **每天必须以"能跑的 commit"收尾**：功能没做完就加开关关掉或注释掉再提交。目标是"checkout 任意一天的 commit 都能跑起来"。
3. **计划外的新点子一律扔进 H.5 扩展池**。想玩扩展池里的东西是合法的（那本来就叫玩），但不要在主线里插队。

### H.3 分叉点：三条路径，随时可以停

| 路径 | 天数 | 累计 | 终点 |
| --- | --- | --- | --- |
| **保底** | Day1~Day5 | ~22h | **v0.3：一个能玩的 2D 游戏** |
| 标准 | + Day6 | ~28h | v0.5：伪 3D 走廊 + Debug Overlay |
| 完整 | + Day7 | ~32h | v1.0：精灵遮挡 + README/GIF/CI + tag |

**掉队处理顺序（不要反过来）：**

1. 砍当天的自由块；
2. 顺延到 Day7 的 2h 缓冲；
3. 砍掉最后一个阶段（比如 S13 推到假期之后，反正是游乐场项目）。

> **Day 5 结束时的"能玩"是整个假期最重要的心理锚点。
> 所以 Day 5 之前禁止任何重构冲动**——想重构就写 TODO，Day5 之后再评估。
> 大概率你会发现根本不需要（这也是 D.1 想说的那件事的另一面）。

### H.4 假期时间怎么分配更省力

- **整块 2h+（上午、状态最好）** 留给需要连续上下文的事：S3 的限帧调试、S12 的 DDA、S13 的遮挡边界。这些一旦被打断就要重新加载上下文。
- **碎片 / 状态差（晚上、饭后）** 留给机械活：S0/S1 抄 CMake、S11 的 Overlay 文本排版、补测试、录 GIF、写 README。机械活做得慢但不影响正确性。
- 建议把 **S12（最难）放 Day6 上午**（全假期精力峰值），把 **Day7 只留给文档/CI/录屏**（状态最差的一天做最不需要创造力的事）。
- **自由块的内容不要提前规划**——它的全部价值就在于"不需要计划"。

### H.5 扩展池（有富余时间才动，按性价比排序）

| 想做的事 | 耗时 | 建议插哪 | 备注 |
| --- | --- | --- | --- |
| 256 色 + 按距离变暗/雾 | 1h | Day2 自由块 | 视觉回报最高的一项 |
| `▀` / `▄` 半块字符（纵向分辨率翻倍）| 1h | Day3 自由块 | 伪 3D 观感直接跳一档 |
| 命中/死亡小方块粒子 | 1h | Day5 自由块 | 廉价但很提气 |
| 地图从 `map.txt` 加载 | 1h | Day4 自由块 | 之后调关卡不用重新编译 |
| A* 寻路（解决 S8 的卡墙）| 2.5h | Day6 / Day7 | BFS 打底，顺手在 Debug Overlay 里画路径 |
| 装 Ninja 提速构建 | 0.5h | Day1 | `winget install Ninja-build.Ninja`，一条命令，别折腾 |
| 开 WSL 真验证 Linux 分支 | 2h | Day7 | 本机没有 g++，这是唯一的真验证手段 |
| Profiler 导出 Chrome Trace JSON | 1.5h | Day7 | `chrome://tracing` 里看火焰图，成就感很强 |
| Braille 字符渲染（2×4 点阵）| 3h | Day7 | 纯玩，风险高收益也高，做不完就当实验 |
| 行为树替换 FSM | 2h | **不建议** | D.1 已说明：5 个状态的 FSM 更好维护 |

### H.6 三个"今天状态不对"的替代剧本

| 状态 | 怎么办 |
| --- | --- |
| 累，只想坐着敲 | 自由块延长到 3h；主线只做当天最机械的那部分（抄 CMake、排 HUD 文本、写测试）|
| 精神特别好 | 主线提前，把后一天的一个阶段拉过来做，Day7 的缓冲会变宽 |
| 完全不想写代码 | 录 GIF、写 README、读 sokol / notcurses 源码、把 `PROJECT_LOG.md` 补完整 —— 这些都算项目进展，不算掉队 |

### H.7 一句话

> 6h × 7 天看着很多，但主线实测 32h + 自由块 7~10h，**基本打平**。
> 所以这个假期的正确心态是：**每天按时收口，别想着"后面还有六天呢"。**
> 真正富余的东西不是时间，而是"保底路径只要 22h"——你随时可以少做两天而不算失败。

---

## 附录 A：命令速查

```bash
# ---- 首次配置（家：本机，已完成，保留备查）----
git init -b main
cmake --preset ms-msvc-debug          # 或：cmake -S . -B build

# ---- 日常循环 ----
cmake --build build --config Debug
./build/bin/Debug/MiniEngine.exe
ctest --test-dir build -C Debug --output-on-failure

# ---- Release ----
cmake --build build --config Release
./build/bin/Release/MiniEngine.exe

# ---- clang 交叉验证（不需要装 Ninja）----
cmake -S . -B build-clang -G "Visual Studio 17 2022" -T ClangCL
cmake --build build-clang --config Debug

# ---- Sanitizer（clang/gcc）----
cmake -S . -B build-asan -DMINI_SANITIZE=ON
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

### 附录 A.2 公司电脑首次 clone 后的必做四步（**关键**）

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

# 4. 验证：应输出 6 个文件，且全部是 i/lf w/lf
git ls-files --eol
```

> ⚠️ **`build/` 绝不能跨机同步**（已在 `.gitignore` 里）。两台机器的 CMake 缓存、生成器、绝对路径都不同，到公司后第一件事是**重新 configure**，不要复制家里的 `build` 目录：
> ```bash
> cmake -S . -B build
> ```
> 另外，别把仓库放在 OneDrive / 坚果云等网盘同步目录里——网盘同步 `.git/` 会损坏仓库。

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

## 附录 B：CMakePresets.json（草案）

```json
{
  "version": 6,
  "configurePresets": [
    { "name": "ms-msvc-debug",   "generator": "Visual Studio 17 2022",
      "binaryDir": "${sourceDir}/build",       "cacheVariables": { "CMAKE_CONFIGURATION_TYPES": "Debug;Release" } },
    { "name": "ms-clang-debug",  "generator": "Visual Studio 17 2022", "toolset": "ClangCL",
      "binaryDir": "${sourceDir}/build-clang", "cacheVariables": { "CMAKE_CONFIGURATION_TYPES": "Debug" } },
    { "name": "ms-asan",         "inherits": "ms-clang-debug",
      "binaryDir": "${sourceDir}/build-asan",  "cacheVariables": { "MINI_SANITIZE": "ON" } }
  ],
  "buildPresets": [
    { "name": "debug",       "configurePreset": "ms-msvc-debug",  "configuration": "Debug" },
    { "name": "release",     "configurePreset": "ms-msvc-debug",  "configuration": "Release" },
    { "name": "clang-debug", "configurePreset": "ms-clang-debug", "configuration": "Debug" }
  ],
  "testPresets": [
    { "name": "debug", "configurePreset": "ms-msvc-debug", "configuration": "Debug", "output": { "outputOnFailure": true } }
  ]
}
```

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
