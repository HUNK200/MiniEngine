#include "mini/core/Version.h"

namespace mini::core {

const char* VersionString()
{
    // 中文注释不是装饰：它和 tests/test_main.cpp 里的 UTF-8 断言一起，
    // 用来确认 /utf-8 生效（风险 E-1，本机代码页是 936）。
    //
    // ⚠️ 版本号的**变更史**（只有两处地方记着它：这里 + `PLAN` G 节的版本表）：
    //    · 2026-10-10（`TECH_DEBT` **C3** 关闭）：`0.1.0-dev` → `0.2.0-dev`
    //      —— 依据 **v0.2「走得动」**（200×200 地图 + 相机跟随 + 不能穿墙 + 顶栏 FPS）在 S3 / S6 / S7 达成。
    //    · **2026-10-13：`0.2.0-dev` → `0.3.0-dev`**
    //      —— 依据 **v0.3「玩得起来」**（① 敌人会追会打 S8 · ② 能射击能击杀 S9 ·
    //      ③ HP/Ammo/Kills/Wave + 死亡重开 S10）于 **2026-10-12** 达成，
    //      且当天走完了反向验证 12 场 + 人眼 28 条（`S10_CHECKLIST` 步骤 8）。
    //    · **2026-10-15：`0.3.0-dev` → `0.4.0-dev`**
    //      —— 依据 **v0.4「看得清」**（① Debug Overlay `T` S11 · ② `PROFILE_SCOPE` 分段耗时表 S11 ·
    //      ③ 测试覆盖 math/collision/gamestate —— S4 起就有）于 **2026-10-15** 达成（S11 收口）。
    //      ⚠️ 与 `PLAN` 字面的偏差：`PLAN` G 节写的是"`F1~F4` Debug Overlay"，实际是
    //        **`T`=overlay / `F2`=几何层 / `F4`=写快照**（`F1`/`F3` 在本机被别的软件抢占，
    //        见 `S11_CHECKLIST` 分叉 A）。
    //    保留 `-dev` 后缀（沿用最初 `0.1.0-dev` 的那个约定）：**已达成 v0.4，但树还在往 v0.5 走**
    //    （v0.5「像 3D」= S12 Ray Casting）。
    //
    // ⚠️ 它的调用者只有 `tests/test_smoke.cpp` 一处（**exe 早就不打印版本了** ——
    //    `PLAN:320` / `S0_CHECKLIST` 里那句"exe 打印 `MiniEngine 0.1.0-dev`"是 **S0 的历史行为**，
    //    现在 `main.cpp` 里没有这行）。
    //    留着它的理由**不是**"以后要用"，而是它是 S0 那条链子的**活化石**：
    //    `main.cpp → engine 静态库 → 公共 include 目录 → CTest` —— 挂在 `Core.VersionIsReported` 上，
    //    链子哪一环断了它就会红。
    //    ⚠️ **改版本号 = 四处功能落点一起改**（本文件字符串 + `VersionMajor()` + `VersionMinor()`
    //    + `test_smoke.cpp` 的两行断言 —— 那条用例就是这四个落点的机器守卫）。
    //    ⚠️ 另有**一处举例注释**也要顺手改：`Version.h` 里那句 `形如 "…"`。
    //    它**没有机器守卫**（编译器和用例都不管它）⇒ 最容易漏，改完看一眼。
    return "0.4.0-dev";
}

int VersionMajor() { return 0; }

int VersionMinor() { return 4; }

} // namespace mini::core
