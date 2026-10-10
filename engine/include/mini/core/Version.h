#pragma once

// ============================================================
// S0 唯一的引擎头文件。
// 它存在的理由不是"要有个版本号"，而是要把这条链子接通：
//     main.cpp → engine 静态库 → 公共 include 目录 → CTest
// 真正的引擎模块从 S1（FrameBuffer）开始。
// ============================================================

namespace mini::core {

// 形如 "0.2.0-dev"。版本号里程碑见 PLAN G 节（v0.1 看得见 / v0.2 走得动 / v0.3 玩得起来）。
// ⚠️ 改版本号要**三处一起**（`Version.cpp` 的字符串 + 下面两个函数 + `test_smoke.cpp` 的断言）；
//    "这个函数为什么还留着（只有一个测试调用者）"也写在 `Version.cpp` 的注释里。
const char* VersionString();

int VersionMajor();

int VersionMinor();

} // namespace mini::core
