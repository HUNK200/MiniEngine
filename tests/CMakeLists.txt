# ============================================================
# tests —— 一个 exe 就是一个 CTest 用例
# 自制框架（MiniTest.h）不做 per-case 拆分：退出码非 0 即失败，
# 一个 exe 覆盖多个 MINI_TEST 用例，CTest 侧显示为 1/1。
#
# 源文件分两拨：
#   test_main.cpp        —— 注册表 + runner（机制，S0 之后基本不用动）
#   test_smoke.cpp       —— S0 的用例
#   test_frame_buffer.cpp—— S1 的用例（不需要终端：FrameBuffer 不碰 IO）
# 新加用例文件必须在这里手写进源文件列表（S0 明确禁用 file(GLOB)）。
# ============================================================

add_executable(mini_tests
    test_main.cpp
    test_smoke.cpp
    test_frame_buffer.cpp
)

target_include_directories(mini_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mini_tests PRIVATE mini_options MiniEngine::engine)

# 用 generator expression 拿路径，别写 exe 的相对路径：
# 多配置生成器下路径含配置名，硬编码会在 Release/ClangCL 下找错文件
add_test(NAME mini_tests COMMAND $<TARGET_FILE:mini_tests>)
