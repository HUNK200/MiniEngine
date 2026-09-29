# ============================================================
# MiniEngine 顶层 CMakeLists —— S0「骨架与构建基线」
#
# 验收标准（PLAN B 节 S0）：
#   cmake -S . -B build
#   cmake --build build --config Debug
#   ctest --test-dir build -C Debug --output-on-failure   → 1/1 passed
# ============================================================

cmake_minimum_required(VERSION 3.20)   # 风险 E-7：CMake 4.x 已移除 <3.5 的兼容写法

project(MiniEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 多配置生成器(VS)下统一 exe 输出位置。
# 只设这一个变量、**不**设 CMAKE_RUNTIME_OUTPUT_DIRECTORY_<CFG>：
# 后者会按原样使用，导致 exe 落在 build/bin/，而 PLAN 附录 A 的运行命令是
# ./build/bin/Debug/MiniEngine.exe —— 让生成器自动补 Debug/Release 子目录才对得上。
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(MINI_BUILD_TESTS "Build unit tests" ON)
option(MINI_SANITIZE    "Enable MSVC AddressSanitizer (/fsanitize=address)" OFF)

# 编译选项做成 INTERFACE 库，避免每个 target 复制粘贴一遍。
# NOMINMAX / WIN32_LEAN_AND_MEAN 现在还用不上，但等 S2 引入 <windows.h> 时，
# 少了这两个宏 std::max/std::min 会被宏打爆（风险 E-17）。
add_library(mini_options INTERFACE)
target_compile_definitions(mini_options INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN)

# 只做 MSVC（2026-09-29 决定）：不保留 clang / ClangCL / gcc 的编译分支。
# 原先这里有个 `else()` 给非 MSVC 编译器加 `-Wall -Wextra -Wpedantic -Wshadow`，
# 它在本机**永远不会执行**（ClangCL 也会让 CMake 把 MSVC 判为真），已删除。
# 依据：AGENTS.md 第 4 节 —— 只用 MSVC，不写也不跑别的编译器分支。
#
# /utf-8 是本机代码页 936 下的必需品，不是可选优化（风险 E-1）
target_compile_options(mini_options INTERFACE /W4 /permissive- /utf-8 /Zc:__cplusplus)

if(MINI_SANITIZE)
    # MSVC 的 ASan（VS2019 16.9+ 起支持）。**不需要 clang** —— 2026-09-29 之前
    # 预设写成继承 ClangCL 是多余的。
    #
    # 编译与链接**都要带**这个开关：只写编译选项会在链接期报 __asan_* 未解析。
    target_compile_options(mini_options INTERFACE /fsanitize=address)
    target_link_options(mini_options INTERFACE /fsanitize=address)

    # MSVC 的 ASan 与 /RTC1 互斥，而 CMake 的 Debug 配置默认带 /RTC1。
    # 显式关掉它，让 `--config Debug` 就能跑 ASan（否则得改用 RelWithDebInfo）。
    target_compile_options(mini_options INTERFACE /RTC-)
endif()

add_subdirectory(engine)

# game/ 留到 S5 之后再建：现在没有任何源文件，
# 空的 add_library(game STATIC) 会直接报 "no SOURCES given to target"。
# 引擎与游戏分家的时机是"游戏侧第一次有真东西"的时候（PLAN A.4 第 2 条）。

add_executable(MiniEngine main.cpp)
target_link_libraries(MiniEngine PRIVATE mini_options MiniEngine::engine)

if(MINI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
