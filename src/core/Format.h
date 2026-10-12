#pragma once

// =============================================================================
//  VideoForge - 通用格式化工具（人类可读文本）
//  纯函数，可单元测试，不依赖 Qt GUI。
// =============================================================================

#include <QString>
#include <QtGlobal>

namespace vf {

namespace fmt {

/// 字节数 -> "1.23 GB" / "845.2 MB" / "12.0 KB"
QString size(qint64 bytes);

/// 秒 -> "01:23:45" 或 "12:03"（不足一小时省略小时段）
QString duration(double seconds);

/// 秒 -> "1小时23分45秒"（用于预估等自然语言场景）
QString durationLong(double seconds);

/// 比特率 -> "5.4 Mb/s" / "192 kb/s"
QString bitrate(qint64 bitsPerSecond);

/// 帧率 -> "29.97 fps" / "30 fps"（自动去掉多余小数）
QString fps(double value);

/// 比例 -> "16:9"，无法识别时回退为 "1280x720" 形式
QString aspectRatio(int width, int height);

/// 把秒转为 ffmpeg 可接受的 "HH:MM:SS.mmm" 时间戳
QString ffmpegTimestamp(double seconds);

/// 解析 "HH:MM:SS(.mmm)" / "MM:SS" / "SS" 为秒；失败返回 -1
double parseTimestamp(const QString &text);

/// 速度倍率 -> "1.85x"
QString speed(double value);

/// 支持导入的媒体扩展名（小写、不带点）
QStringList supportedMediaExtensions();

/// 是否是可导入的媒体文件
bool isSupportedMediaFile(const QString &filePath);

/// 供 QFileDialog 使用的过滤器字符串
QString mediaFileDialogFilter();

/// 供输出另存对话框使用的过滤器（按容器）
QString saveFileDialogFilter(const QString &container, const QString &extension);

/// 带单位缩放数字（用于帧率选择等）
QString number(double value, int decimals = 2);

} // namespace fmt

} // namespace vf
