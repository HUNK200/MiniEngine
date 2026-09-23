# ============================================================
# tests —— 一个 exe 就是一个 CTest 用例
# 自制框架（MiniTest.h）不做 per-case 拆分：退出码非 0 即失败，
# 一个 exe 覆盖多个 MINI_TEST 用例，CTest 侧显示为 1/1。
# ============================================================

add_executable(mini_tests
    test_main.cpp
)

target_include_directories(mini_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(mini_tests PRIVATE mini_options MiniEngine::engine)

add_test(NAME mini_tests COMMAND mini_tests)
