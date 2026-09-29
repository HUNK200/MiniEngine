#pragma once

#include "mini/render/Cell.h"

#include <string>
#include <string_view>
#include <vector>

// ============================================================
// FrameBuffer —— 屏幕后缓冲（S1）
//
// 职责：**只管像素，不碰 IO**。
//   - 它是"后缓冲"，终端屏幕是"前缓冲"，Present() 就是交换（见 platform::Console）
//   - 所有终端 API 都在 platform::Console 里，所以这个类**可以离线单测**
//
// 依赖方向：render 内部自洽，不依赖 platform，不认识 game。
//
// 坐标：x = 列（向右增大），y = 行（向下增大），原点在左上角。
//       与 PLAN A.5 的 Screen 空间一致（单位 = 字符格）。
// ============================================================

namespace mini::render {

class FrameBuffer {
public:
    // w / h 非正时抛 std::invalid_argument：宽高为 0 的画面没有意义，
    // 而且要挡住"从终端读到 0 就一路传下来"这种情况（读尺寸是 S2 的事）
    FrameBuffer(int w, int h);

    // 终端尺寸变化时重建。S1 不监听变化事件（那是 S2 的 Input），
    // 但接口先落地并被单测覆盖。
    void Resize(int w, int h);

    void Clear(Cell fill = {});

    // 越界**静默忽略**：S5 之后实体天天走出屏幕边缘，靠的就是这条（不报错、不裁剪调用方）
    void Set(int x, int y, char c, Color fg = Color::Default, Color bg = Color::Default);

    // 逐格写一串文本（用默认颜色），逐格越界忽略
    void Draw(int x, int y, std::string_view text);

    // 只画**四条边**（不填充内部），按矩形自身的边坐标裁剪：
    // 边落在画布外的部分直接不出现，不会因为裁剪而"折回来"画到另一侧
    //
    // ⚠️ **生产代码 0 调用**（单测 7 处）—— 别当死代码删掉：
    //    它服务的是 S12+ 的边框 / HUD 面板，那时才第一次有生产调用者。
    void DrawRect(int x, int y, int w, int h, char c);

    // 点是否在画布内。**不是纯测试用**：`Set()` 与 `At()` 内部都调它。
    //
    // ⚠️ 生产代码不**直接**调（单测 3 处）—— 别因为"没人调"就收回 private：
    //    收回去之后，边界用例只能**通过 Set/At 间接**验证，
    //    一旦失败，报错会指向 Set/At 而不是边界判定本身。
    bool InBounds(int x, int y) const;
    int  Width() const { return m_w; }
    int  Height() const { return m_h; }

    // 越界返回 Cell{}（空白 + 默认色），**不返回引用**：
    // 引用版在越界时只能 UB 或断言，而 S6 之后到处要拿屏幕外的格子做判断
    //
    // ⚠️ **生产代码 0 调用**（单测 4 处）—— 它的角色是"**验证 `Set`/`Draw` 真写对了**的手段"：
    //    只有它能**读回含颜色**的格子。删掉它，那一类断言只能退化成比对 Serialize 的字节。
    Cell At(int x, int y) const;

    // 把整屏拼成一个字符串（不含光标归位，那是 Present 的事）。
    // 返回值指向内部缓冲：有效期到下一次 Serialize() / SerializeRow() / Resize() / 析构。
    //
    // ⚠️ **生产代码 0 调用**（单测 4 处）：生产路径已改走 `SerializeRow()`（脏行 diff，S3）。
    //    但**别删它** —— 它现在的角色是"**全量帧的参考实现**"，
    //    单测靠它证明脏行 diff **字节等价**（`RowDiff.FullFrameMatchesLegacySerializeBytes`）。
    //    而且它与 `SerializeRow()` 共用 `AppendRowTo()` ——
    //    那是"两条路径颜色规则一致"的**唯一**保证（删了就得把颜色逻辑抄第二遍）。
    std::string_view Serialize() const;

    // 单独取第 y 行的字节（**不含行尾、不含行首的 SGR 重置**）。越界返回空。
    //
    // 给 render::RowDiff 用（脏行重发时先发 ESC[{y};1H + ESC[39;49m，再发这段）。
    //
    // 契约（RowDiff 的脏行比较活在这上面）：
    //   1. 颜色的处理规则与 Serialize() 完全一致（同一份实现），
    //      但**假定这一行开头终端是默认色** —— 因为绝对定位之后终端的 SGR
    //      是上一行留下的，调用方必须先发重置
    //   2. 这一行的字节**只取决于这一行的内容**，别的行怎么变都不影响它
    //
    // ⚠️ 与 Serialize() 共用同一个 m_scratch：拿到的 view 在下一次
    //    Serialize()/SerializeRow() 调用后就失效了（要先 copy 再取下一行）。
    std::string_view SerializeRow(int y) const;

private:
    // 线性化索引：index = y * m_w + x（y 是行、x 是列）
    std::size_t Index(int x, int y) const;

    // 重建 m_cells 与 m_scratch 容量（ctor 与 Resize 共用）
    void Reallocate(int w, int h);

    // 把第 y 行追加到 out。curFg/curBg 是**进出参数**：
    // 进来表示"这一行开头终端已经是什么颜色"，出去更新成行尾颜色。
    // Serialize() 与 SerializeRow() 共用它 —— 这是"两条路径颜色规则一致"的唯一保证。
    void AppendRowTo(std::string& out, int y, Color& curFg, Color& curBg) const;

    int               m_w = 0;
    int               m_h = 0;
    std::vector<Cell> m_cells;     // 线性化：index = y * m_w + x
    mutable std::string m_scratch; // 序列化缓冲，mutable 以便 Serialize() 是 const
};

} // namespace mini::render
