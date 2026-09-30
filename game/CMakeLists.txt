# ============================================================
# game —— 本项目的游戏侧（S5 第一次有"游戏"这个东西）
#
# 为什么 S5 才建：在这之前游戏侧只有 main.cpp 里一个 ~250 行的 DemoGame，
# 没有第二个消费者 —— 建库只是把同样的代码挪个地方（PLAN A.4：结构被复杂度逼出来）。
# 到 S5，"游戏侧开始有真东西了"（世界 / 输入翻译 / 渲染），分家才有意义。
#
# 依赖方向（PLAN A.3 的硬规矩）：
#     main → game → engine → 标准库          （engine **绝不** include game）
#
# ⚠️ 两处 CMake 细节，都不是风格问题：
#   1. engine 必须是 **PUBLIC**：Game.h 的基类 `mini::core::IGame` 来自 engine，
#      消费者（main / tests）要能顺着 game 看见 engine 的头与库。
#   2. mini_options 必须**自己再链一次**：engine 是 PRIVATE 链它的，
#      /utf-8 不会传递给消费者 —— 而 Game.cpp 里有中文注释，漏了就是编码事故。
#
# ⚠️ 空的 add_library 会直接报 "no SOURCES given to target"，
#    所以 Game.cpp 与这个文件必须**同一步**落地（顶层 CMakeLists 的旧注释记的就是这件事）。
# ============================================================

add_library(game STATIC
    src/Game.cpp
)

target_include_directories(game PUBLIC include)

target_link_libraries(game PUBLIC MiniEngine::engine)
target_link_libraries(game PRIVATE mini_options)

add_library(MiniEngine::game ALIAS game)
