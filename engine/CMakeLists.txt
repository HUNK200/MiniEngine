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
# 不要提前把 PLAN C.2 草案里的 Application.cpp / Input.cpp 等写进来 ——
# 那些文件还不存在，写进来等于构建直接挂（PLAN A.4：结构被复杂度逼出来）。
# ============================================================

add_library(engine STATIC
    src/core/Version.cpp
    src/core/Time.cpp
    src/render/FrameBuffer.cpp
    src/platform/ConsoleWindows.cpp
    src/platform/InputWindows.cpp
    src/platform/WaitWindows.cpp
)

target_include_directories(engine PUBLIC include)
target_link_libraries(engine PRIVATE mini_options)

add_library(MiniEngine::engine ALIAS engine)
