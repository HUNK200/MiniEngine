# ============================================================
# engine —— 引擎静态库（不认识任何游戏概念）
#
# 源文件列表按阶段增长（手写，禁用 file(GLOB)）：
#   S0  src/core/Version.cpp
#   S1  src/render/FrameBuffer.cpp（纯数据，可单测）
#   S2  src/platform/ConsoleWindows.cpp（全项目**唯一** include <windows.h> 的文件）
#       由 S1 的 render/TerminalPresenter.cpp 归位而来（PLAN A.3：平台 API 只准出现在 platform/）
#       src/platform/InputWindows.cpp（Win32 事件 → InputState；四态逻辑在 InputState.h）
#   S3  src/platform/WaitWindows.cpp（高精度可等待计时器：Sleep 的 15.6ms 粒度
#                                      会把 60fps 的 16.67ms 目标帧时间睡过头一整跳）
#       src/core/Time.cpp（Time 里碰 steady_clock / 等待的部分；
#                          纯逻辑的 ClampDelta / FrameStats 留在 Time.h，可离线单测）
#       src/render/RowDiff.cpp（脏行 diff：只发变化的行。纯逻辑、可离线单测）
#       src/core/Application.cpp（主循环：时间/输入推进/尺寸对齐/提交/顶栏。
#                                不认识具体游戏，也不认识终端 —— 走 Console 抽象）
# 不要提前把**后面阶段**的文件写进来 —— 那些文件还不存在，写进来等于构建直接挂
# （PLAN A.4：结构被复杂度逼出来）。比如 S3 之后才有的 src/render/Terminal.cpp、
# src/game/*。S3 已经把 PLAN C.2 草案里的 Application.cpp 落实了，所以它在列表里。
#
#   S4  **不新增源文件** —— math/ 是 header-only（纯值类型 + 纯函数 + constexpr），
#       头文件放在 engine/include/mini/math/ 下，靠下面的 PUBLIC include 目录暴露。
#       ⚠️ header-only 的头**只有被 include 才会被编译**，所以 S4 的编译验证
#          完全靠 tests/test_math.cpp 去 include 它们 —— 别以为"在列表里没看到 S4"
#          就是漏了（这是故意的）。
#
#   S5  src/world/TileMap.cpp（格子表：世界坐标 → tile 下标用 floor、越界一律当 Wall。
#                              纯数据 + 纯逻辑，可离线单测）
#       src/world/World.cpp（实体容器：Spawn / Find / 索引 / dt 积分 / 延迟删除。
#                            一份扫描 IndexOf 服务 Find 两版 + FlushRemovals）
#       后面 S6/S7 会往 world/ 加文件时，同样**在这里手写一行**。
#
#   S11 src/core/Logger.cpp（分级日志：级别过滤 + 前缀 + 追加写文件 + 进程级 current 指针。
#                            ⚠️ 它碰的是**文件系统**、不是终端 ⇒ 能被离线单测
#                            （tests/test_logger.cpp 用临时文件，跑完自己删））
# ============================================================

add_library(engine STATIC
    src/core/Version.cpp
    src/core/Time.cpp
    src/core/Application.cpp
    src/core/Logger.cpp
    src/render/FrameBuffer.cpp
    src/render/RowDiff.cpp
    src/platform/ConsoleWindows.cpp
    src/platform/InputWindows.cpp
    src/platform/WaitWindows.cpp
    src/world/TileMap.cpp
    src/world/World.cpp
)

target_include_directories(engine PUBLIC include)
target_link_libraries(engine PRIVATE mini_options)

add_library(MiniEngine::engine ALIAS engine)
