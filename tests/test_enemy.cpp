#include "MiniTest.h"

#include <mini/ai/EnemyBrain.h>

#include <span>

// ------------------------------------------------------------
// ai/EnemyBrain.h 的单元测试（S8 步骤 1：18 个用例，suite `Enemy`）
//
// 【为什么这些用例这么便宜】
//   `Decide` 是纯函数、只碰 `Vec2` 和 `EnemyState` —— 不需要地图、不需要 `Entity`、
//   不需要 `World`、不需要终端、不需要假输入。这是 S7 把碰撞做成纯函数之后，
//   "能离线测的部分全部拆出去"这条纪律的第二次兑现。
//
// 【⚠️ 期望值一律由 `kDefaultTuning` 算出来，不许写死数字】
//   理由与 `test_game.cpp` 里"不许写死 kMoveSpeed 算出的格数"完全相同：
//   调手感（视距 / 速度 / 冷却）时，用例必须跟着自动适配，否则测试会慢慢变成"测空气"。
//
// 【⚠️ 枚举不能用 MINI_CHECK_EQ】
//   `MINI_CHECK_EQ` 失败时要 `ToString` 两个参数（`ostringstream <<`），而 `enum class`
//   没有 `operator<<` —— 用了就是**编译不过**（不是运行期失败）。
//   所以枚举一律用 `MINI_CHECK(a.state == EnemyState::X)`：它只打印表达式字符串，不转值。
//
// 【⚠️ 浮点比较用 MINI_CHECK_NEAR，不写 `==`】（S4 起的纪律）
//   顺带一个好性质：这条宏是 `((lhs)-(rhs)) <= eps && ((rhs)-(lhs)) <= eps`，
//   而 **NaN 参与任何比较都是 false** → `MINI_CHECK_NEAR(v, 0.0f, 0.0f)` 会连 NaN 一起抓住。
//   用例 16 就是靠这条来钉"零距离不许产生 NaN"。
// ------------------------------------------------------------

namespace {

using mini::ai::BrainInput;
using mini::ai::BrainOutput;
using mini::ai::Decide;
using mini::ai::EnemyState;
using mini::ai::kDefaultTuning;
using mini::math::Vec2;

// 每帧固定 1/60 秒（与游戏里的 dt 同量级）
constexpr float kDt = 1.0f / 60.0f;

// 逻辑上"每个用例都该自己算"的三个距离。由 kDefaultTuning **推导**，绝不写死数字：
//   · kBetweenSightAndLose —— 严格夹在 sightRange 与 loseRange 之间的"迟滞带"
//   · kBeyondLose         —— 明显超出脱战距离
//   · kInsideAttackEnter  —— 严格小于 attackEnter（贴脸）
constexpr float kBetweenSightAndLose = (kDefaultTuning.sightRange + kDefaultTuning.loseRange) * 0.5f;
constexpr float kBeyondLose          = kDefaultTuning.loseRange + 5.0f;
constexpr float kInsideAttackEnter   = kDefaultTuning.attackEnter * 0.5f;

// 造一个"活着、目标活着、dt = 1/60"的基础输入。
// ⚠️ `BrainInput` 是聚合体、有默认值：忘了填 `selfAlive` 会**默认 true**（安全，但不报错），
//    所以每个用例构造完都要自己核对一遍"我到底设了哪几项"。
BrainInput MakeInput(EnemyState state, Vec2 self, Vec2 target)
{
    BrainInput in{};
    in.state       = state;
    in.self        = self;
    in.target      = target;
    in.selfAlive   = true;
    in.targetAlive = true;
    in.dt          = kDt;
    return in;
}

// 一个"离得很远的目标"：保证不会触发进战逻辑，用来隔离巡逻/build状态机
Vec2 FarTarget(Vec2 self)
{
    return Vec2{self.x + kBeyondLose, self.y};
}

} // namespace

// ------------------------------------------------------------
// 1. 纯函数性：同输入两遍 → 逐字段相同（无隐藏状态）。
//    同 S6 的 Camera.FollowIsPureFunctionOfInputs、S7 的
//    Collision.MoveAndCollideIsPureFunctionOfInputs 一个套路。
// ------------------------------------------------------------
MINI_TEST(Enemy, DecideIsPureFunctionOfInputs)
{
    const Vec2 path[] = {Vec2{3.5f, 2.5f}, Vec2{9.5f, 4.5f}};

    BrainInput in{};
    in.self        = Vec2{2.5f, 2.5f};
    in.target      = Vec2{2.5f + kBetweenSightAndLose, 2.5f};
    in.waypoints   = std::span<const Vec2>{path};
    in.waypointIdx = 1;
    in.state       = EnemyState::Patrol;
    in.attackTimer = 0.3f;
    in.idleTimer   = 0.25f;
    in.dt          = kDt;

    const BrainOutput a = Decide(in, kDefaultTuning);
    const BrainOutput b = Decide(in, kDefaultTuning);

    MINI_CHECK(a.state == b.state);
    MINI_CHECK_EQ(static_cast<int>(a.waypointIdx), static_cast<int>(b.waypointIdx));
    MINI_CHECK_NEAR(a.velocity.x, b.velocity.x, 0.0f);
    MINI_CHECK_NEAR(a.velocity.y, b.velocity.y, 0.0f);
    MINI_CHECK_NEAR(a.attackTimer, b.attackTimer, 0.0f);
    MINI_CHECK_NEAR(a.idleTimer, b.idleTimer, 0.0f);
    MINI_CHECK_EQ(a.damage, b.damage);

    // 顺带钉"推进而不是原样返回"：两个定时器都必须变小（不能是 in 的副本）
    MINI_CHECK(a.attackTimer < in.attackTimer);
    MINI_CHECK(a.idleTimer < in.idleTimer);
}

// ------------------------------------------------------------
// 2. Idle 只活 idleSeconds 那么久
// ------------------------------------------------------------
MINI_TEST(Enemy, IdleBecomesPatrolAfterIdleSeconds)
{
    const Vec2 self = Vec2{2.5f, 2.5f};

    // (a) 剩余时间还没耗完 → 仍是 Idle，且原地不动
    BrainInput in  = MakeInput(EnemyState::Idle, self, FarTarget(self));
    in.idleTimer   = kDefaultTuning.idleSeconds;
    const BrainOutput mid = Decide(in, kDefaultTuning);

    MINI_CHECK(mid.state == EnemyState::Idle);
    MINI_CHECK_NEAR(mid.velocity.x, 0.0f, 0.0f);
    MINI_CHECK_NEAR(mid.velocity.y, 0.0f, 0.0f);

    // (b) 剩余时间 < dt → 夹到 0 → 转 Patrol
    in.idleTimer = kDt * 0.5f;
    MINI_CHECK(Decide(in, kDefaultTuning).state == EnemyState::Patrol);
}

// ------------------------------------------------------------
// 3. Patrol：朝当前路点走，速度大小 == moveSpeed、方向指向路点
// ------------------------------------------------------------
MINI_TEST(Enemy, PatrolMovesTowardWaypoint)
{
    const Vec2 self   = Vec2{2.5f, 2.5f};
    const Vec2 path[] = {Vec2{10.5f, 2.5f}};   // 正右方

    BrainInput in  = MakeInput(EnemyState::Patrol, self, FarTarget(self));
    in.waypoints   = std::span<const Vec2>{path};

    const BrainOutput out = Decide(in, kDefaultTuning);

    MINI_CHECK(out.state == EnemyState::Patrol);
    MINI_CHECK_NEAR(out.velocity.Length(), kDefaultTuning.moveSpeed, 1e-4f);
    MINI_CHECK_NEAR(out.velocity.x, kDefaultTuning.moveSpeed, 1e-4f);
    MINI_CHECK_NEAR(out.velocity.y, 0.0f, 1e-4f);
}

// ------------------------------------------------------------
// 4. Patrol：站在当前路点上 → 推进到下一个，并朝它走
// ------------------------------------------------------------
MINI_TEST(Enemy, PatrolAdvancesWaypointOnArrival)
{
    const Vec2 path[] = {Vec2{3.5f, 2.5f}, Vec2{10.5f, 2.5f}};
    const Vec2 self   = path[0];   // 就站在第 0 个路点上

    BrainInput in  = MakeInput(EnemyState::Patrol, self, FarTarget(self));
    in.waypoints   = std::span<const Vec2>{path};
    in.waypointIdx = 0;

    const BrainOutput out = Decide(in, kDefaultTuning);

    MINI_CHECK(out.state == EnemyState::Patrol);
    MINI_CHECK_EQ(static_cast<int>(out.waypointIdx), 1);         // 换点
    MINI_CHECK_NEAR(out.velocity.x, kDefaultTuning.moveSpeed, 1e-4f);   // 朝新路点（+x）
    MINI_CHECK_NEAR(out.velocity.y, 0.0f, 1e-4f);
}

// ------------------------------------------------------------
// 5. Patrol：走到最后一个路点后**环绕**回第一个
//
// ⚠️ 用例名从施工图里的 `PatrolPingPongsAtPathEnds` 改成了 `PatrolWrapsAroundAtPathEnds`：
//    `Decide` 是无状态的，只拿得到 `waypointIdx`，**推不出"上次是往 +1 还是 -1"** ——
//    所以真正的"往返"只能靠**调用方把路径表本身写成回文**（A,B,C,B）来实现，
//    而 `Decide` 这一层只保证"环形前进"。
//    这条已登记为 doc/S8_CHECKLIST.md 的落地偏差 1（施工图 7.4 预判了这个分叉）。
// ------------------------------------------------------------
MINI_TEST(Enemy, PatrolWrapsAroundAtPathEnds)
{
    const Vec2 path[] = {Vec2{3.5f, 2.5f}, Vec2{10.5f, 2.5f}};
    const Vec2 self   = path[1];   // 站在**最后一个**路点上

    BrainInput in  = MakeInput(EnemyState::Patrol, self, FarTarget(self));
    in.waypoints   = std::span<const Vec2>{path};
    in.waypointIdx = 1;

    const BrainOutput out = Decide(in, kDefaultTuning);

    MINI_CHECK_EQ(static_cast<int>(out.waypointIdx), 0);                 // 环绕回第 0 个
    MINI_CHECK_NEAR(out.velocity.x, -kDefaultTuning.moveSpeed, 1e-4f);   // 于是回头走（-x）
    MINI_CHECK_NEAR(out.velocity.y, 0.0f, 1e-4f);
}

// ------------------------------------------------------------
// 6. 空路点表：不崩、不动（game/ 侧理论上不会发生，但纯函数必须自己扛住）
// ------------------------------------------------------------
MINI_TEST(Enemy, EmptyWaypointsDoesNotCrash)
{
    const Vec2 self = Vec2{2.5f, 2.5f};

    BrainInput in  = MakeInput(EnemyState::Patrol, self, FarTarget(self));
    in.waypointIdx = 7;    // 空表 + 脏下标，两个坑一起踩

    const BrainOutput out = Decide(in, kDefaultTuning);

    MINI_CHECK(out.state == EnemyState::Patrol);
    MINI_CHECK_EQ(static_cast<int>(out.waypointIdx), 0);
    MINI_CHECK_NEAR(out.velocity.x, 0.0f, 0.0f);
    MINI_CHECK_NEAR(out.velocity.y, 0.0f, 0.0f);
}

// ------------------------------------------------------------
// 7. 越界的 waypointIdx：先取模再使用（防御脏输入）
// ------------------------------------------------------------
MINI_TEST(Enemy, WaypointIndexOutOfRangeIsClamped)
{
    const Vec2 path[] = {Vec2{3.5f, 2.5f}, Vec2{10.5f, 2.5f}};

    BrainInput in  = MakeInput(EnemyState::Patrol, Vec2{6.5f, 2.5f}, FarTarget(Vec2{6.5f, 2.5f}));
    in.waypoints   = std::span<const Vec2>{path};
    in.waypointIdx = 7;    // 只有 2 个点却传 7

    const BrainOutput out = Decide(in, kDefaultTuning);

    MINI_CHECK_EQ(static_cast<int>(out.waypointIdx), 1);   // 7 % 2 == 1
}

// ------------------------------------------------------------
// 8. 看见目标 → 进战（从 Idle 进、从 Patrol 进，两条边都要走通）
// ------------------------------------------------------------
MINI_TEST(Enemy, SightTriggersChase)
{
    const Vec2 self = Vec2{2.5f, 2.5f};
    const Vec2 near = Vec2{self.x + kDefaultTuning.sightRange * 0.5f, self.y};

    // (a) 从 Idle 进战 —— **Idle 还没到期也要追**（⑥ 在 ⑩ 之前）
    BrainInput fromIdle = MakeInput(EnemyState::Idle, self, near);
    fromIdle.idleTimer  = kDefaultTuning.idleSeconds;
    MINI_CHECK(Decide(fromIdle, kDefaultTuning).state == EnemyState::Chase);

    // (b) 从 Patrol 进战
    BrainInput fromPatrol = MakeInput(EnemyState::Patrol, self, near);
    MINI_CHECK(Decide(fromPatrol, kDefaultTuning).state == EnemyState::Chase);
}

// ------------------------------------------------------------
// 9. Chase：朝目标直线走，速度大小 == moveSpeed
//    ⚠️ 这条是反向验证 R2 的目标（把方向改成常量 / 不归一化 → 变红）
// ------------------------------------------------------------
MINI_TEST(Enemy, ChaseMovesTowardTarget)
{
    const Vec2 self   = Vec2{2.5f, 2.5f};
    const Vec2 target = Vec2{self.x, self.y + kBetweenSightAndLose};   // 正下方

    BrainInput in = MakeInput(EnemyState::Chase, self, target);
    const BrainOutput out = Decide(in, kDefaultTuning);

    MINI_CHECK(out.state == EnemyState::Chase);
    MINI_CHECK_NEAR(out.velocity.Length(), kDefaultTuning.moveSpeed, 1e-4f);
    MINI_CHECK_NEAR(out.velocity.x, 0.0f, 1e-4f);
    MINI_CHECK_NEAR(out.velocity.y, kDefaultTuning.moveSpeed, 1e-4f);   // 指向 +y
}

// ------------------------------------------------------------
// 10. 迟滞：sightRange 与 loseRange 之间那一段**保持原态**
//     ⚠️ 这条是反向验证 R1 的目标（把 loseRange 改成 == sightRange → 变红）。
//        它守卫的是"敌人不抖"这条核心决策 —— 抖动正是 PLAN:573 点名的那个坑。
// ------------------------------------------------------------
MINI_TEST(Enemy, HysteresisKeepsChaseUntilLoseRange)
{
    const Vec2 self   = Vec2{2.5f, 2.5f};
    const Vec2 inBand = Vec2{self.x + kBetweenSightAndLose, self.y};

    // (a) 已经在追 → 中间那一段**继续追**（不脱战）
    BrainInput chasing = MakeInput(EnemyState::Chase, self, inBand);
    MINI_CHECK(Decide(chasing, kDefaultTuning).state == EnemyState::Chase);

    // (b) 还在巡逻 → 中间那一段**不进去**（不追）
    BrainInput patrolling = MakeInput(EnemyState::Patrol, self, inBand);
    MINI_CHECK(Decide(patrolling, kDefaultTuning).state == EnemyState::Patrol);

    // (c) 超过 loseRange → 脱战回巡逻
    BrainInput escaped = MakeInput(EnemyState::Chase, self, FarTarget(self));
    MINI_CHECK(Decide(escaped, kDefaultTuning).state == EnemyState::Patrol);
}

// ------------------------------------------------------------
// 11. 贴脸 → 进 Attack，而且**停下**（速度 {0,0}）
// ------------------------------------------------------------
MINI_TEST(Enemy, AttackEntersWithinAttackEnter)
{
    const Vec2 self   = Vec2{2.5f, 2.5f};
    const Vec2 target = Vec2{self.x + kInsideAttackEnter, self.y};

    BrainInput in  = MakeInput(EnemyState::Chase, self, target);
    in.attackTimer = kDefaultTuning.attackCooldown;   // 冷却没到 → 隔离"进攻击"这一步

    const BrainOutput out = Decide(in, kDefaultTuning);

    MINI_CHECK(out.state == EnemyState::Attack);
    MINI_CHECK_EQ(out.damage, 0);
    MINI_CHECK_NEAR(out.velocity.x, 0.0f, 0.0f);
    MINI_CHECK_NEAR(out.velocity.y, 0.0f, 0.0f);
}

// ------------------------------------------------------------
// 12. 冷却到点 → 打出伤害，并把冷却重置
// ------------------------------------------------------------
MINI_TEST(Enemy, AttackFiresDamageWhenCooldownElapsed)
{
    const Vec2 self   = Vec2{2.5f, 2.5f};
    const Vec2 target = Vec2{self.x + kInsideAttackEnter, self.y};

    BrainInput in  = MakeInput(EnemyState::Attack, self, target);
    in.attackTimer = 0.0f;   // 冷却好了

    const BrainOutput out = Decide(in, kDefaultTuning);

    MINI_CHECK(out.state == EnemyState::Attack);
    MINI_CHECK_EQ(out.damage, kDefaultTuning.attackDamage);
    MINI_CHECK_NEAR(out.attackTimer, kDefaultTuning.attackCooldown, 0.0f);
}

// ------------------------------------------------------------
// 13. 冷却没到 → 本帧伤害为 0
//     ⚠️ 这条是反向验证 R3 的目标（去掉冷却判定 → 变红）
// ------------------------------------------------------------
MINI_TEST(Enemy, AttackRespectsCooldown)
{
    const Vec2 self   = Vec2{2.5f, 2.5f};
    const Vec2 target = Vec2{self.x + kInsideAttackEnter, self.y};

    BrainInput in  = MakeInput(EnemyState::Attack, self, target);
    in.attackTimer = kDefaultTuning.attackCooldown * 0.5f;   // 还在冷却里

    const BrainOutput out = Decide(in, kDefaultTuning);

    MINI_CHECK(out.state == EnemyState::Attack);
    MINI_CHECK_EQ(out.damage, 0);
    MINI_CHECK(out.attackTimer > 0.0f);
}

// ------------------------------------------------------------
// 14. 冷却计时：每帧减 dt，且**下限夹 0**（不许变负）
//
// ⚠️ 这里用 `Chase` 态而不是 `Attack` 态来隔离计时逻辑：`Attack` 态下冷却一旦到 0
//    就会立刻开火并重置成 attackCooldown（那是用例 12 的事），看不见"夹到 0"。
// ------------------------------------------------------------
MINI_TEST(Enemy, AttackTimerCountsDownAndClampsAtZero)
{
    const Vec2 self   = Vec2{2.5f, 2.5f};
    const Vec2 target = Vec2{self.x + kBetweenSightAndLose, self.y};   // 够远，不会进攻击

    BrainInput in  = MakeInput(EnemyState::Chase, self, target);
    in.attackTimer = 0.5f;

    const BrainOutput counted = Decide(in, kDefaultTuning);
    MINI_CHECK_NEAR(counted.attackTimer, 0.5f - kDt, 1e-6f);

    // 剩余时间 < dt → 夹到 0，而不是负数
    in.attackTimer = kDt * 0.5f;
    MINI_CHECK_NEAR(Decide(in, kDefaultTuning).attackTimer, 0.0f, 0.0f);
}

// ------------------------------------------------------------
// 15. 退出攻击：距离超过 attackExit → 回 Chase，并重新朝目标走
// ------------------------------------------------------------
MINI_TEST(Enemy, LeavingAttackRangeReturnsToChase)
{
    const Vec2 self = Vec2{2.5f, 2.5f};
    // 严格大于 attackExit、又不超过 loseRange 的一个距离
    constexpr float kOutside = (kDefaultTuning.attackExit + kDefaultTuning.loseRange) * 0.5f;
    const Vec2 target = Vec2{self.x + kOutside, self.y};

    BrainInput in = MakeInput(EnemyState::Attack, self, target);
    const BrainOutput out = Decide(in, kDefaultTuning);

    MINI_CHECK(out.state == EnemyState::Chase);
    MINI_CHECK_NEAR(out.velocity.Length(), kDefaultTuning.moveSpeed, 1e-4f);
    MINI_CHECK_NEAR(out.velocity.x, kDefaultTuning.moveSpeed, 1e-4f);   // 朝目标（+x）
}

// ------------------------------------------------------------
// 16. 零距离 → 速度必须是 {0,0}，**而且不能是 NaN**
//     ⚠️ 这条是反向验证 R4 的目标（去掉"长度平方 > 0"那道判断 → 变红）。
//        NaN 为什么会要命：`pos` 变 NaN 之后 `ToIndex(NaN)` 是 UB
//        （TileMap.h:69-71 明写"去产生的地方查"）。
//        `MINI_CHECK_NEAR(v, 0.0f, 0.0f)` 同时抓"非零"与"NaN"（见文件头说明）。
// ------------------------------------------------------------
MINI_TEST(Enemy, ZeroDistanceDoesNotProduceNaN)
{
    const Vec2 self = Vec2{2.5f, 2.5f};

    // (a) Chase 且自己与目标**完全重合**
    BrainInput overlapping = MakeInput(EnemyState::Chase, self, self);
    const BrainOutput a    = Decide(overlapping, kDefaultTuning);

    MINI_CHECK(a.state == EnemyState::Attack);   // 距离 0 必然 <= attackEnter
    MINI_CHECK_NEAR(a.velocity.x, 0.0f, 0.0f);
    MINI_CHECK_NEAR(a.velocity.y, 0.0f, 0.0f);

    // (b) Patrol 且**路点与自己完全重合** —— 这条才真正走进"朝某点走"的零向量分支
    const Vec2 path[] = {self};
    BrainInput onWaypoint = MakeInput(EnemyState::Patrol, self, FarTarget(self));
    onWaypoint.waypoints  = std::span<const Vec2>{path};

    const BrainOutput b = Decide(onWaypoint, kDefaultTuning);
    MINI_CHECK(b.state == EnemyState::Patrol);
    MINI_CHECK_NEAR(b.velocity.x, 0.0f, 0.0f);
    MINI_CHECK_NEAR(b.velocity.y, 0.0f, 0.0f);
}

// ------------------------------------------------------------
// 17. 自己死了 → 任何态都立刻变 Dead、速度 {0,0}、不打人
// ------------------------------------------------------------
MINI_TEST(Enemy, SelfDeadForcesDeadAndStops)
{
    const Vec2 self   = Vec2{2.5f, 2.5f};
    const Vec2 target = Vec2{self.x + kInsideAttackEnter, self.y};   // 贴脸（最容易"还在打"）

    const EnemyState states[] = {EnemyState::Idle, EnemyState::Patrol,
                                 EnemyState::Chase, EnemyState::Attack};

    for (const EnemyState s : states) {
        BrainInput in = MakeInput(s, self, target);
        in.selfAlive  = false;

        const BrainOutput out = Decide(in, kDefaultTuning);
        MINI_CHECK(out.state == EnemyState::Dead);
        MINI_CHECK_EQ(out.damage, 0);
        MINI_CHECK_NEAR(out.velocity.x, 0.0f, 0.0f);
        MINI_CHECK_NEAR(out.velocity.y, 0.0f, 0.0f);
    }
}

// ------------------------------------------------------------
// 18. 目标死了 → 从交战态回到 Patrol，而且**这一帧不许再结算伤害**
// ------------------------------------------------------------
MINI_TEST(Enemy, TargetDeadReleasesChase)
{
    const Vec2 self   = Vec2{2.5f, 2.5f};
    const Vec2 target = Vec2{self.x + kInsideAttackEnter, self.y};

    // (a) 正在追 → 目标死了 → 回巡逻
    BrainInput chasing  = MakeInput(EnemyState::Chase, self, target);
    chasing.targetAlive = false;
    MINI_CHECK(Decide(chasing, kDefaultTuning).state == EnemyState::Patrol);

    // (b) 正在打 → 目标死了 → 回巡逻，且伤害为 0
    //     （这条钉的是**顺序**：⑤ 脱战必须发生在 ⑨ 开火之前）
    BrainInput attacking = MakeInput(EnemyState::Attack, self, target);
    attacking.targetAlive = false;
    attacking.attackTimer = 0.0f;   // 冷却已到 —— 若顺序写反，这里就会打出伤害

    const BrainOutput out = Decide(attacking, kDefaultTuning);
    MINI_CHECK(out.state == EnemyState::Patrol);
    MINI_CHECK_EQ(out.damage, 0);
}
