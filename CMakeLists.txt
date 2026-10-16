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

# ── S11.5「送得出去」：静态 CRT（可分发构建）────────────────────────────────
#
# 为什么要有这个开关：默认的 MSVC 构建是 **动态 CRT（/MD）**，exe 依赖
#   VCRUNTIME140.dll / MSVCP140.dll（来自"VC++ 2015-2022 可再发行组件"）。
# 别人机器上**没装**那个运行库时，双击 exe 会直接报"缺少 DLL" —— 这是把 demo
# 送出去时的**头号风险**。静态 CRT（/MT）把这些代码链进 exe，换来自包含。
#
# ⚠️ 三条硬约束（写下来是因为踩了都不报错）：
#   1. **默认 OFF** ⇒ 现有 Debug / Release / CI / asan 四条路径行为一字不变；
#   2. 必须在 `add_subdirectory()` **之前**设置 —— 这个变量在**目标创建时**被
#      捕获进 `MSVC_RUNTIME_LIBRARY` 目标属性；放在后面就是**静默无效**
#      （症状：dumpbin 里照样有 VCRUNTIME140.dll）；
#   3. 依赖策略 **CMP0091 = NEW**（由 `cmake_minimum_required(VERSION 3.20)` 保证）——
#      否则 CMake 会往 `CMAKE_CXX_FLAGS_*` 里塞 `/MD`/`/MDd`，把这里的设置盖掉。
#      ⇒ 别把最低版本改回 3.15 以下。
#
# 生成器表达式让"单配置 + 多配置"两种生成器都对：Debug 用静态调试 CRT，
# 其余配置（Release / RelWithDebInfo / MinSizeRel）用静态发布 CRT。
option(MINI_STATIC_CRT "Link the MSVC runtime statically (/MT) for a redistributable binary" OFF)

if(MINI_STATIC_CRT AND MINI_SANITIZE)
    # ASan 需要动态 CRT（runtime 要能替换分配器）⇒ 组合起来链接期会报一堆奇怪的话。
    # 与其让人去猜，不如在**配置期**给一句人话。
    message(FATAL_ERROR "MINI_STATIC_CRT 与 MINI_SANITIZE 不能同时开：ASan 需要动态 CRT（/MD）。")
endif()

if(MINI_STATIC_CRT)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# 把"这一份 build 用的是哪种 CRT"打在配置输出里 —— 它是**可观察的验证点**
#（比"我记得我开了"可靠；S11.5 的验收里 dumpbin 那一步靠它定位）。
if(MSVC)
    if(MINI_STATIC_CRT)
        message(STATUS "MiniEngine: MSVC 运行时 = **静态 /MT**（可分发：不依赖 VC++ 可再发行组件）")
    else()
        message(STATUS "MiniEngine: MSVC 运行时 = 动态 /MD（默认：运行目标机需要 VC++ 2015-2022 可再发行组件）")
    endif()
endif()

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

# game/ 在 S5 建起来了 —— 这里原来写着"留到 S5 之后再建"，S5 到了。
# 顺序：engine 必须在前面（game 要链 MiniEngine::engine）。
add_subdirectory(game)

add_executable(MiniEngine main.cpp)
# 显式写 engine：main 现在只 include <game/Game.h>，引擎头其实由 game PUBLIC 传递过来，
# 但把直接依赖写出来，读的人不用去 game 的 CMake 里查"为什么这里能编译"。
target_link_libraries(MiniEngine PRIVATE mini_options MiniEngine::engine MiniEngine::game)

if(MINI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
