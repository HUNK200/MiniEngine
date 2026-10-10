#include "mini/core/Version.h"

namespace mini::core {

const char* VersionString()
{
    // 中文注释不是装饰：它和 tests/test_main.cpp 里的 UTF-8 断言一起，
    // 用来确认 /utf-8 生效（风险 E-1，本机代码页是 936）。
    //
    // ⚠️ 2026-10-10（TECH_DEBT **C3** 关闭）：`0.1.0-dev` → `0.2.0-dev`。
    //    依据 PLAN G 节：**v0.2「走得动」**（200×200 地图 + 相机跟随 + 不能穿墙 + 顶栏 FPS）
    //    在 S3 / S6 / S7 已经达成，触发条件满足。
    //    保留 `-dev` 后缀 —— 沿用原来 `0.1.0-dev` 的同一个约定：
    //    "**已达成 v0.2，但这是一棵还在往 v0.3「玩得起来」走的开发树**"（v0.3 = S8~S10，S8 已完成）。
    //
    // ⚠️ 它的调用者只有 `tests/test_smoke.cpp` 一处（**exe 早就不打印版本了** ——
    //    `PLAN:320` / `S0_CHECKLIST` 里那句"exe 打印 `MiniEngine 0.1.0-dev`"是 **S0 的历史行为**，
    //    现在 `main.cpp` 里没有这行）。
    //    留着它的理由**不是**"以后要用"，而是它是 S0 那条链子的**活化石**：
    //    `main.cpp → engine 静态库 → 公共 include 目录 → CTest` —— 挂在 `Core.VersionIsReported` 上，
    //    链子哪一环断了它就会红。改版本号时**三处一起改**（本文件字符串 + 下面两个函数 + 那条用例）。
    return "0.2.0-dev";
}

int VersionMajor() { return 0; }

int VersionMinor() { return 2; }

} // namespace mini::core
