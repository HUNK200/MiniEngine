# MiniEngine：纯命令行 2D/3D 游戏引擎实验项目

## 1. 项目定位

我想做一个**纯命令行（Terminal/Console）的 C++ 小型游戏引擎实验项目**。

项目的目的不是做一个商业级引擎，也不是为了最终发布，而是：

1. 通过一个有趣、可运行、可持续扩展的项目重新建立 C++ 编程兴趣。
2. 实际理解游戏引擎的基本结构和 Game Loop。
3. 研究 Entity / Component / System、Input、Physics、AI、Camera、Renderer 等概念。
4. 练习 C++、CMake、调试、性能分析、跨平台开发等能力。
5. 最终能够在纯命令行中实现一个可以玩的 2D 游戏，并进一步尝试实现简单的伪 3D / Ray Casting。
6. 项目最好能够作为个人技术研究项目保存在 GitHub。

---

# 2. 核心限制

请严格考虑以下限制：

### 2.1 技术限制

第一阶段只允许使用：

* C++
* CMake
* 标准库
* Windows Terminal / Linux Terminal
* VS Code

**尽量不要引入第三方库。**

如果某个功能确实非常适合使用第三方库，可以作为后续可选方案提出，但不要让第三方库成为第一阶段的必要依赖。

---

### 2.2 渲染限制

项目不使用：

* Unreal Engine
* Unity
* OpenGL
* DirectX
* Vulkan
* SDL
* SFML

第一阶段所有画面都通过终端字符实现。

例如：

```text
##############################
#                            #
#          E                 #
#                            #
#                  @         #
#                            #
##############################
```

其中：

```text
@ = Player
E = Enemy
# = Wall
. = Floor
* = Projectile
+ = Item
```

后续可以研究 ASCII / Unicode 字符、终端颜色、ANSI Escape Sequence 等。

---

# 3. 最终愿景

希望最终能够实现一个类似下面的纯命令行游戏：

```text
┌──────────────────────────────────────────────┐
│ MiniEngine   FPS: 59.8   Frame: 16.7ms      │
├──────────────────────────────────────────────┤
│                                              │
│       ##########                             │
│       #        #                             │
│       #   E    #                             │
│       #    @   #       E                     │
│       #        #                             │
│       ##########                             │
│                                              │
├──────────────────────────────────────────────┤
│ HP: 80       Ammo: 12       Wave: 3          │
└──────────────────────────────────────────────┘
```

最终可以包含：

* Player
* Enemy
* 地图
* 碰撞
* 输入
* 武器
* Projectile
* HP
* AI
* Camera
* UI
* Game State
* 简单粒子效果
* 性能统计
* Debug Overlay

进一步研究后，希望尝试：

```text
2D World
   ↓
Ray Casting
   ↓
ASCII / Terminal Renderer
   ↓
伪 3D
```

实现类似早期 Wolfenstein 3D 风格的终端伪 3D 效果。

---

# 4. 游戏循环

希望核心结构能够围绕 Game Loop 展开，例如：

```cpp
while (!shouldQuit)
{
    Time::Update();

    Input::Poll();

    Game::Update();

    Physics::Update();

    AI::Update();

    Renderer::Render();

    Time::LimitFPS(60);
}
```

但不要机械照搬这个结构。

请分析：

* Game Loop 应该如何设计
* Fixed Update 和 Variable Update 是否需要
* Delta Time 如何处理
* FPS 如何统计
* Frame Time 如何统计
* 如何避免终端刷新导致性能问题
* Windows / Linux 下是否存在差异

---

# 5. 计划实现的模块

希望逐步研究以下模块：

## Core

* Application
* GameLoop
* Time
* Logger
* Configuration

## Input

* Keyboard Input
* Key State
* Pressed / Released / Held
* Windows / Linux 差异

## World

* World
* Entity
* Component
* Transform
* Scene

## Math

* Vec2
* Vec3
* Rect
* AABB
* Ray
* Distance
* Normalization

## Physics

* Collision
* AABB
* Trigger
* Simple Movement
* Gravity（可选）

## AI

* Enemy Controller
* State Machine
* Patrol
* Chase
* Attack
* Dead

后续可以研究：

* Behavior Tree
* A*
* Path Finding

## Renderer

第一阶段：

```text
Terminal
 ↓
FrameBuffer
 ↓
Renderer2D
 ↓
Console
```

需要考虑：

* Clear
* Draw
* Present
* Double Buffer
* Camera
* Viewport

后续：

```text
World
 ↓
Camera
 ↓
Ray Casting
 ↓
Renderer3D
 ↓
Terminal
```

## Game

例如：

```text
Player
Enemy
Weapon
Projectile
Pickup
GameState
Wave
```

---

# 6. 2D 阶段

希望第一阶段最终实现一个简单的 2D Terminal 游戏。

地图：

```text
####################
#..................#
#..................#
#......E...........#
#..................#
#..........@.......#
#..................#
####################
```

要求：

### Player

* WASD 移动
* 不能穿墙
* 有位置和速度
* 有简单状态

### Enemy

* 自动寻找 Player
* 距离较远时 Chase
* 距离较近时 Attack
* Player 离开后可以返回 Patrol

### Weapon

例如：

```text
SPACE
```

发射：

```text
@ ---> * ---> E
```

### Game State

显示：

```text
HP: 80
Ammo: 12
Kills: 5
Wave: 2
```

---

# 7. Camera

地图不能限制在终端大小。

例如：

```text
World = 200 x 200
Terminal = 80 x 30
```

需要实现：

```text
World
  ↓
Camera
  ↓
Viewport
  ↓
FrameBuffer
  ↓
Terminal
```

Camera 可以跟随 Player。

请解释：

* World Coordinate
* Camera Coordinate
* Screen Coordinate
* 坐标转换
* Viewport
* 边界处理

---

# 8. Renderer

希望设计一个简单的 FrameBuffer：

```cpp
class FrameBuffer
{
public:
    void Clear();
    void Set(int x, int y, char c);
    void Present();
};
```

例如：

```cpp
renderer.Draw(player.x, player.y, '@');
renderer.Draw(enemy.x, enemy.y, 'E');
```

然后统一：

```cpp
renderer.Present();
```

避免大量：

```cpp
std::cout << ...
```

导致终端闪烁。

请设计一个合理的 Terminal Renderer，并解释：

* ANSI Escape Sequence 是否值得使用
* 如何避免闪烁
* 如何实现双缓冲
* 如何处理终端尺寸变化
* Windows Terminal 与 Linux Terminal 的兼容问题

---

# 9. Entity / Component

不要一开始就设计一个复杂 ECS。

希望从简单结构开始，例如：

```cpp
struct Entity
{
    Transform transform;
    Velocity velocity;

    char symbol;

    bool active;
};
```

然后逐渐研究：

```text
Entity
 ├── Transform
 ├── Velocity
 ├── Health
 ├── Collider
 ├── AI
 └── Renderer
```

请分析什么时候应该从这种简单设计升级到真正的 ECS。

不要为了“架构漂亮”而过度设计。

---

# 10. AI

希望至少实现：

```text
Idle
 ↓
Patrol
 ↓
Chase
 ↓
Attack
 ↓
Dead
```

例如：

```text
Enemy
State: Chase
Target: Player
Distance: 8.32
```

后续可以研究：

```text
State Machine
      ↓
Behavior Tree
      ↓
A*
```

请设计一个适合这个项目规模的 AI 方案。

---

# 11. Physics

第一阶段只需要简单碰撞：

```text
Player
 ↓
Calculate New Position
 ↓
Collision Detection
 ↓
Blocked?
 ├── Yes → Reject / Resolve
 └── No  → Apply
```

先实现：

* AABB
* Wall Collision
* Entity Collision

后续再考虑：

* Circle Collision
* Trigger
* Gravity
* Collision Response
* Continuous Collision Detection

请控制复杂度。

---

# 12. 伪 3D / Ray Casting

这是项目后期的重要研究方向。

地图仍然是：

```text
####################
#..................#
#......#...........#
#......#.....E.....#
#......#...........#
#..........@.......#
####################
```

但最终终端显示：

```text
          ||||||||||||
      ||||||||||||||||||
   ||||||||        ||||||||
|||||||||            |||||||
||||||||              ||||||
|||||||       E        |||||||
||||||||              |||||||
|||||||||            |||||||
   ||||||||        ||||||||
      ||||||||||||||||||
          ||||||||||||
```

希望研究：

* Ray Casting
* FOV
* Wall Distance
* Perspective
* Fish-eye Correction
* Depth
* Sprite / Enemy Rendering

请解释如何从 2D 地图逐步演进到这个系统。

---

# 13. 调试能力

希望项目从一开始就有 Debug 功能。

例如：

```text
FPS: 59.8
Frame: 16.72 ms

Entities: 12
Enemies: 5
Projectiles: 3

Player:
  Position: (31.2, 14.8)
  Velocity: (1.0, 0.0)
  HP: 80
```

最好能够：

```text
F1 = Toggle Debug
F2 = Toggle Collision
F3 = Toggle AI State
F4 = Toggle Profiler
```

---

# 14. Profiler

希望后期实现：

```text
============================
 Frame Profile
============================

Input        0.05 ms
Game         0.31 ms
Physics      0.82 ms
AI           1.24 ms
Renderer     4.82 ms

Total        7.24 ms
FPS          138
============================
```

请设计一个简单的：

```cpp
PROFILE_SCOPE("Physics");
```

机制。

后续可以研究：

* RAII
* chrono
* thread_local
* ring buffer
* trace
* Chrome Trace Format

---

# 15. C++ 学习目标

希望这个项目能够顺便覆盖：

* C++17 / C++20
* RAII
* Smart Pointer
* STL
* Move Semantics
* Templates
* Lambda
* Thread
* Mutex
* Atomic
* Memory Model
* Coroutine（可选）
* Allocator（实验）
* Cache / Data Layout
* Profiling

但不要为了覆盖知识点而强行加入。

核心原则：

> **遇到实际问题再研究 C++ 特性。**

---

# 16. 跨平台

希望至少支持：

```text
Windows
Linux
```

例如：

```bash
cmake -S . -B build
cmake --build build
```

然后：

```bash
./MiniEngine
```

Windows：

```powershell
.\MiniEngine.exe
```

请尽量让代码保持跨平台。

如果 Terminal Input 在 Windows/Linux 上实现方式不同，请设计一个平台抽象层，例如：

```text
Platform
 ├── Windows
 └── Linux
```

---

# 17. 测试

希望至少使用：

```text
Unit Test
```

测试：

* Vec2
* AABB
* Collision
* Ray
* Map
* Game State

如果不引入第三方测试框架，也可以先自己实现简单测试。

请分析什么时候值得加入 GoogleTest。

---

# 18. CMake

希望项目使用现代 CMake。

例如：

```text
MiniEngine
 ├── Engine
 ├── Game
 ├── Tests
 └── main.cpp
```

希望最终支持：

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

并考虑：

* Debug
* Release
* Sanitizer
* Warning
* Clang / GCC / MSVC

---

# 19. Git

希望项目适合长期迭代。

请设计合理的 commit 节奏。

例如：

```text
feat: add terminal framebuffer
feat: add keyboard input
feat: add player movement
feat: add collision detection
feat: add enemy chase
feat: add projectile system
```

不要要求每个 commit 都非常正式，但希望每个阶段都可以运行。

---

# 20. 最重要的开发原则

请严格遵守：

### 原则 1：可运行优先

每个阶段结束都应该能够：

```text
Build
 ↓
Run
 ↓
看到实际结果
```

---

### 原则 2：不要过度设计

不要一开始就：

```text
ECS
Reflection
Serialization
Plugin System
Resource Manager
Dependency Injection
Event Bus
```

除非项目真的遇到了对应问题。

---

### 原则 3：每个阶段都应该有“好玩的结果”

例如：

```text
阶段 1：
终端里出现 @

阶段 2：
@ 可以移动

阶段 3：
@ 不能穿墙

阶段 4：
E 开始追 @

阶段 5：
@ 可以射击 E

阶段 6：
出现 Wave

阶段 7：
Camera 跟随

阶段 8：
出现简单光照 / 伪 3D
```

---

# 21. 希望 DeepSeek 输出的内容

请你不要直接开始写完整代码。

先帮我设计完整方案。

请按照以下结构回答：

## A. 项目总体架构

给出：

```text
整体模块图
目录结构
模块之间的依赖关系
```

并解释为什么这么设计。

---

## B. 开发路线

把项目拆成大约 **10～15 个阶段**。

每个阶段说明：

```text
目标
需要实现什么
涉及的 C++ 知识
涉及的游戏引擎知识
预计代码规模
完成后的可运行效果
验收标准
```

要求每个阶段都尽量能够在较短时间内完成，不要出现一个阶段需要几天才能看到效果的情况。

---

## C. 第一阶段详细方案

重点给出第一阶段：

```text
目录结构
CMakeLists.txt
核心类
类之间关系
Game Loop
Terminal Renderer
```

但**不要一次生成整个项目的所有代码**。

我希望可以按照阶段逐步实现。

---

## D. 架构演进

重点解释：

```text
简单 Entity
     ↓
Component
     ↓
System
     ↓
ECS（如果真的有必要）
```

以及：

```text
2D Renderer
     ↓
Camera
     ↓
Ray Casting
     ↓
Pseudo 3D
```

应该如何逐步演进。

---

## E. 技术风险

列出这个项目最容易踩的坑，例如：

* Terminal Input
* ANSI
* Windows/Linux 差异
* Console Rendering 性能
* Frame Timing
* Terminal Flickering
* Coordinate System
* Game Loop
* ECS 过度设计
* Ray Casting
* CMake 跨平台

并给出解决策略。

---

## F. 推荐的“研究顺序”

我希望这个项目更像一个：

> C++ + Game Engine Playground

而不是一个必须完成的大项目。

因此请给出：

```text
如果今天只想玩 30 分钟
应该做什么？

如果有 1 小时
应该做什么？

如果突然对 Renderer 感兴趣
应该研究什么？

如果突然对 ECS 感兴趣
应该研究什么？

如果突然想研究 3D
应该从哪里开始？
```

---

## G. 最终推荐

最后请给出你认为最合理的：

```text
MiniEngine v0.1
MiniEngine v0.2
MiniEngine v0.3
MiniEngine v0.4
MiniEngine v1.0
```

每个版本只定义少量目标。

重点是：

> **这个项目应该让我想打开 VS Code，而不是让我看到计划表就不想做。**

不要把它设计成一个庞大、严肃、需要长期坚持才能获得反馈的工程。

我更希望它是一个可以随时打开、随时实验、随时获得结果的 C++ 游戏引擎游乐场。
