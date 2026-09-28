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
    void DrawRect(int x, int y, int w, int h, char c);

    bool InBounds(int x, int y) const;
    int  Width() const { return m_w; }
    int  Height() const { return m_h; }

    // 越界返回 Cell{}（空白 + 默认色），**不返回引用**：
    // 引用版在越界时只能 UB 或断言，而 S6 之后到处要拿屏幕外的格子做判断
    Cell At(int x, int y) const;

    // 把整屏拼成一个字符串（不含光标归位，那是 Present 的事）。
    // 返回值指向内部缓冲：有效期到下一次 Serialize() / Resize() / 析构。
    std::string_view Serialize() const;

private:
    // 线性化索引：index = y * m_w + x（y 是行、x 是列）
    std::size_t Index(int x, int y) const;

    // 重建 m_cells 与 m_scratch 容量（ctor 与 Resize 共用）
    void Reallocate(int w, int h);

    int               m_w = 0;
    int               m_h = 0;
    std::vector<Cell> m_cells;     // 线性化：index = y * m_w + x
    mutable std::string m_scratch; // 序列化缓冲，mutable 以便 Serialize() 是 const
};

} // namespace mini::render
