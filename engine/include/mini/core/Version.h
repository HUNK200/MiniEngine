#pragma once

// ============================================================
// S0 唯一的引擎头文件。
// 它存在的理由不是"要有个版本号"，而是要把这条链子接通：
//     main.cpp → engine 静态库 → 公共 include 目录 → CTest
// 真正的引擎模块从 S1（FrameBuffer）开始。
// ============================================================

namespace mini::core {

// 形如 "0.1.0-dev"。版本号里程碑见 PLAN G 节（v0.1 看得见 / v0.2 走得动 / v0.3 玩得起来）。
const char* VersionString();

int VersionMajor();

int VersionMinor();

} // namespace mini::core
