#pragma once

// =============================================================================
//  VideoForge - ffmpeg -progress 输出解析
//
//  ffmpeg 以 key=value 的形式持续输出进度，例如：
//      frame=123
//      fps=25.4
//      bitrate=1234.5kbits/s
//      total_size=4567890
//      out_time_us=5000000
//      speed=1.85x
//      progress=continue
//  本类把逐行文本累积成结构化进度，纯逻辑、可单测。
// =============================================================================

#include <QString>

namespace vf {

struct ProgressInfo
{
    bool valid = false;         ///< 是否解析出任何有效字段
    bool finished = false;      ///< 收到 progress=end
    qint64 outTimeUs = 0;       ///< 已处理时长（微秒）
    qint64 totalSize = 0;       ///< 当前输出文件字节数
    int frame = 0;
    double fps = 0.0;
    double speed = 0.0;         ///< 处理速度倍率
    qint64 bitrate = 0;         ///< bits/s

    double outTimeSeconds() const { return static_cast<double>(outTimeUs) / 1'000'000.0; }
};

class ProgressParser
{
public:
    /// 送入一段原始输出（可以是半行），返回本次是否更新了状态
    bool feed(const QString &chunk);

    /// 送入一行（不含换行）
    bool feedLine(const QString &line);

    const ProgressInfo &info() const { return m_info; }
    void reset() { m_info = ProgressInfo(); m_buffer.clear(); }

    /// 解析单行 key=value（静态，便于单测）
    static bool parseLine(const QString &line, ProgressInfo *info);

    /// 解析 "1234.5kbits/s" / "N/A"
    static qint64 parseBitrate(const QString &text);

private:
    ProgressInfo m_info;
    QString m_buffer;
};

} // namespace vf
