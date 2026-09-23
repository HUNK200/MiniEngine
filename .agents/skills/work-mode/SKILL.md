---
name: work-mode
description: 把 MiniEngine 游戏引擎项目的阶段切碎成 5~10 分钟、随时可中断、且能立刻验证的单个微任务。用于上班/上课间隙的碎片时间：说「上班模式」「摸一下鱼」「给我个 10 分钟的活」「碎片任务」「来个小活」或输入 /w5 /w10 时，先看 git log 与排期表判断当前阶段，再只给出一个任务——包含打开哪个文件哪一行改成什么、增量构建命令、一条可执行的完成标志、被打断怎么办；并硬性拒绝「实现某个系统」「学习某个原理」「跨文件重构」「给几个选项让用户挑」这类伪任务。也用于「做完了」时轮换下一种任务类型、「太简单/太难」时同赛道升降档、「只有 3 分钟」时降到最小、「今天不方便编译」时切纯推演模式。触发词：上班模式, 摸鱼, 摸一下鱼, 碎片任务, 碎片时间, 上班间隙, 空档, 给我个活, 10分钟的活, 5分钟的活, 来个小活, 小任务, 微任务, 再来一个, 做完了, 太简单, 太难, 降级, /w5, /w10, /work, work mode, micro task, 5-minute task. Use when the user wants exactly one small interruptible coding task for a work break.
---

# 上班模式（入口转发）

本 skill 的正文**不在这个目录**，而在项目的 `doc/` 下，以避免同一份内容维护两处：

- 相对本文件的路径：`../../../doc/WORK_MODE.md`
- 绝对路径：`d:/Project/fish/c++_simple_engine/doc/WORK_MODE.md`

同目录下的配套文件：`MiniEngine_PLAN.md`（阶段定义、H.1 排期表、E 风险表）、`S0_CHECKLIST.md`。

## 执行方式

1. **先读取 `d:/Project/fish/c++_simple_engine/doc/WORK_MODE.md`**，按其中的"触发后的固定动作""输出模板""微任务准入标准""禁止的 6 类伪任务""类型轮换表"执行。
2. 判断当前阶段时读 `d:/Project/fish/c++_simple_engine/doc/MiniEngine_PLAN.md`，并以 `git log --oneline -5` 为准；若 `doc/S0_CHECKLIST.md` 的勾选进度显示当前仍在 S0，以它为准。
3. 所有相对路径以 `d:/Project/fish/c++_simple_engine/` 为基准解析。
4. 本文件**不含任何正文**，不要在只读了本文件的情况下直接开始给任务。

## 四条最容易违反的铁律（提前提醒）

1. **只给一个任务。** 不要给 3~5 个选项让用户挑。
2. **触发后不要反问。** 只有在完全判断不出当前阶段时才问一句。
3. **任务必须自带起手式**：打开哪个文件、哪一行、改成什么。没有这个就不算合格任务。
4. **只发任务卡，不代写实现。** 代码一律由使用者自己敲（`AGENTS.md` 核心约定）。明说"你写"才动手。

## 为什么要有这个转发文件

Agent Skill 的发现路径是 `.agents/skills/<name>/SKILL.md`，而文档被统一放在 `doc/` 下（可见、可直接编辑、和其他文档并排），两者对不上。转发文件是调和：**正文一份，入口一个。**

如果不想要这个结构，二选一：

```bash
# 方案 A：彻底改成标准位置（正文搬进来，删掉 doc/ 里那份）
cd /d/Project/fish/c++_simple_engine
mv doc/WORK_MODE.md .agents/skills/work-mode/SKILL.md
# 然后删掉本文件的前 4 行之后的所有"执行方式"内容，并把 frontmatter 保留

# 方案 B：改成个人级，所有项目可用
mkdir -p ~/.agents/skills/work-mode
cp .agents/skills/work-mode/SKILL.md ~/.agents/skills/work-mode/SKILL.md
# 注意：方案 B 下正文里的绝对路径仍然指向本项目，跨项目使用需要自行调整
```

## 安装 / 校验命令

```bash
# 确认文件结构
ls -la .agents/skills/work-mode/SKILL.md
head -n 5 .agents/skills/work-mode/SKILL.md

# 确认名字与目录名一致（name 必须是 work-mode）
grep '^name:' .agents/skills/work-mode/SKILL.md
```

安装到 `.agents/skills/` 后，客户端可能**需要重载或重启**才会发现这个 skill。
