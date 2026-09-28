# ============================================================
# engine —— 引擎静态库（不认识任何游戏概念）
#
# 源文件列表按阶段增长（手写，禁用 file(GLOB)）：
#   S0  src/core/Version.cpp
#   S1  src/render/FrameBuffer.cpp（纯数据，可单测）
#   S2  src/platform/ConsoleWindows.cpp（全项目**唯一** include <windows.h> 的文件）
#       由 S1 的 render/TerminalPresenter.cpp 归位而来（PLAN A.3：平台 API 只准出现在 platform/）
# 不要提前把 PLAN C.2 草案里的 Application.cpp / Input.cpp 等写进来 ——
# 那些文件还不存在，写进来等于构建直接挂（PLAN A.4：结构被复杂度逼出来）。
# ============================================================

add_library(engine STATIC
    src/core/Version.cpp
    src/render/FrameBuffer.cpp
    src/platform/ConsoleWindows.cpp
)

target_include_directories(engine PUBLIC include)
target_link_libraries(engine PRIVATE mini_options)

add_library(MiniEngine::engine ALIAS engine)
