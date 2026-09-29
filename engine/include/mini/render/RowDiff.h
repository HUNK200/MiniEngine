#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// ============================================================
// render::RowDiff —— 脏行 diff（S3）
//
// 职责：**只把内容变了的行交给终端**。
//
// 为什么值得单独一个类：
//   80×25 的画面一帧约 2.4KB，60fps 就是 145KB/s；而屏幕上通常只有几行在动
//   （顶栏的 FPS + 那个 @）。省下来的量很大 —— 而且它是**唯一能自动验证
//   "到底省了多少"的地方**，所以做成纯逻辑、不碰 IO、能离线单测。
//
// ⚠️ 它活在一个前提上：`FrameBuffer::SerializeRow(y)` 的输出
//    **只取决于第 y 行的内容**。前提破了的现象是"某一行每帧白重发"——
//    不崩、不报错、只是白干。`tests/test_row_diff.cpp` 里有专门守这条的用例。
//
// 依赖：只 include 标准库 + 前向声明的 FrameBuffer（.cpp 里才要完整定义）。
// 不 include <windows.h>，也不认识 Console —— 把字节写出去是 Console 的事。
// ============================================================

namespace mini::render {

class FrameBuffer;

class RowDiff {
public:
    // 这里原先还有一个 `Resize(int rows)`，2026-09-28 删掉了：
    // **全仓库 0 处调用**（连测试都没有）。`Compose()` 自己会发现行数不符
    // 并丢掉旧快照（见下面那段），多一个公开的 Resize 只会让人以为"必须手工调"，
    // 而且它和数据成员 `m_prevRows` 的关系得靠读实现才能搞清。
    //
    // 换画布尺寸时要做的**只有一件事**：正常调 `Compose()`。要不要全量重发
    // 由它自己判断（行数变了 → 快照作废 → 全量）。

    // 强制下一帧全量重发。用在：清屏（ESC[2J）之后、终端尺寸变化之后。
    void Invalidate() noexcept;

    // 输入新帧，返回**这一帧要写终端的完整字节**（含光标定位与 SGR 重置）。
    //
    // 返回值指向内部缓冲：有效期到下一次 Compose() / 析构
    // （与 FrameBuffer::Serialize 同一规矩）。
    //
    // 全量帧：ESC[H + ESC[39;49m + 逐行 + "\r\n"（**最后一行不写**，否则整屏上滚）
    // 脏行帧：每行 = ESC[{y+1};1H + ESC[39;49m + 行内容 + ESC[0K
    //         （ESC[0K 擦掉"行变短"时右边残留的旧字符）
    std::string_view Compose(const FrameBuffer& fb);

    struct Stats {
        int         rowsTotal = 0;   // 画布行数
        int         rowsSent  = 0;   // 本帧真发出去的行数（顶栏 Rows: 2/25 用它）
        std::size_t bytes     = 0;   // 本帧字节数
    };

    [[nodiscard]] Stats LastStats() const noexcept { return m_stats; }

private:
    std::vector<std::string> m_prevRows;   // 上一帧每行的字节（不含定位序列）
    std::string              m_scratch;    // 本帧要发的完整字节
    bool                     m_dirtyAll = true;   // 首帧就是"全脏"
    Stats                    m_stats{};
};

} // namespace mini::render
