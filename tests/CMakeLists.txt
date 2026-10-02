# ============================================================
# tests —— 一个 exe 就是一个 CTest 用例
# 自制框架（MiniTest.h）不做 per-case 拆分：退出码非 0 即失败，
# 一个 exe 覆盖多个 MINI_TEST 用例，CTest 侧显示为 1/1。
#
# 源文件分两拨：
#   test_main.cpp        —— 注册表 + runner（机制，S0 之后基本不用动）
#   test_smoke.cpp       —— S0 的用例
#   test_frame_buffer.cpp—— S1 的用例（不需要终端：FrameBuffer 不碰 IO）
#   test_input_state.cpp —— S2 的用例（不需要终端：四态状态机与 VK 映射是纯逻辑）
#   test_time.cpp        —— S3 的用例（不需要时钟：ClampDelta / FrameStats 是纯逻辑）
#   test_row_diff.cpp    —— S3 的用例（不需要终端：RowDiff 只产字符串）
#   test_math.cpp        —— S4 的用例（不需要任何外部条件：math 是纯函数 + 纯值类型）
#   test_world.cpp       —— S5 的用例（不需要终端：world 不认识 platform/render。步骤 1/2 各加了 10 个）
#   test_game.cpp        —— S5 的用例（game 层：靠**假 Input 替身**离线跑，见决策 9）
#   test_camera.cpp      —— S6 的用例（不需要终端：Camera 是纯函数，不认识 TileMap/FrameBuffer）
# 新加用例文件必须在这里手写进源文件列表（S0 明确禁用 file(GLOB)）。
# ============================================================

add_executable(mini_tests
    test_main.cpp
    test_smoke.cpp
    test_frame_buffer.cpp
    test_input_state.cpp
    test_time.cpp
    test_row_diff.cpp
    test_math.cpp
    test_world.cpp
    test_game.cpp
    test_camera.cpp
)

target_include_directories(mini_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# 链 game 是为了让 Game::OnUpdate/OnRender 可离线测（S5 决策 9）。
# 依赖无环：tests → game → engine，而 engine 绝不反向 include game（PLAN A.3）。
target_link_libraries(mini_tests PRIVATE mini_options MiniEngine::engine MiniEngine::game)

# 用 generator expression 拿路径，别写 exe 的相对路径：
# 多配置生成器下路径含配置名，硬编码会在 Debug/Release 切换时找错文件
add_test(NAME mini_tests COMMAND $<TARGET_FILE:mini_tests>)
