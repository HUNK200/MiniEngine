#pragma once

// ============================================================
// S0 唯一的引擎头文件。
// 它存在的理由不是"要有个版本号"，而是要把这条链子接通：
//     main.cpp → engine 静态库 → 公共 include 目录 → CTest
// 真正的引擎模块从 S1（FrameBuffer）开始。
// ============================================================

namespace mini::core {

// 形如 "0.6.0-dev"（**当前值**）。版本号里程碑见 PLAN G 节
// （v0.1 看得见 / v0.2 走得动 / v0.3 玩得起来 / v0.4 看得清 / v0.5 像 3D / S13 精灵+遮挡）。
// ⚠️ 改版本号 = **四处功能落点**（`Version.cpp` 的字符串 + `VersionMajor()` + `VersionMinor()`
//    + `test_smoke.cpp` 的断言）+ **这句举例**。这一句没有机器守卫（编译器和用例都不管它）⇒ 最容易漏。
// ⚠️ "这个函数为什么还留着（只有一个测试调用者）"写在 `Version.cpp` 的注释里；变更史也在那儿。
const char* VersionString();

int VersionMajor();

int VersionMinor();

} // namespace mini::core
