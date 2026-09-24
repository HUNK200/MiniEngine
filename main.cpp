// 尖括号 + `mini/` 前缀：引擎头只能从 PUBLIC include 目录找，
// 不允许用相对路径穿透到 engine/src/（S0_CHECKLIST 第 5.5 节 / R7）
#include <mini/core/Version.h>

#include <cstdio>

// S0 阶段唯一的职责：证明"能构建、能跑、能链接 engine 库"。
// 从 S1 起这里会被 Application 取代（PLAN C.3）。

int main()
{
    std::printf("MiniEngine %s\n", mini::core::VersionString());
    std::printf("S0 skeleton: engine static lib linked, CTest wired.\n");
    return 0;
}
