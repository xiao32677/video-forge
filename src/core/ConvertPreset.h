#pragma once

// =============================================================================
//  VideoForge - 转换预设
//  预设 = 目标容器 + 编码器组合 + 默认质量参数，面向"不懂参数的人"。
// =============================================================================

#include <QList>
#include <QString>

namespace vf {

/// 硬编码器类型
enum class HardwareAccel {
    None = 0,   ///< 软件编码
    Nvenc,      ///< NVIDIA
    Qsv,        ///< Intel Quick Sync
    Amf,        ///< AMD
    VideoToolbox ///< macOS（预留）
};

QString hardwareAccelName(HardwareAccel accel);

struct ConvertPreset
{
    QString id;                 ///< 稳定标识，用于持久化
    QString displayName;        ///< 面向用户的名字，如 "MP4 · 通用（H.264 + AAC）"
    QString group;              ///< 分组："视频" / "音频" / "图片"
    QString description;        ///< 一句话说明适用场景
    QString container;          ///< 目标容器/格式：mp4 / mkv / webm / mov / avi / gif / mp3 / m4a / wav / flac
    QString extension;          ///< 输出扩展名（不带点）
    QString videoCodec;         ///< ffmpeg 编码器名；留空表示无视频流
    QString audioCodec;         ///< ffmpeg 编码器名；留空表示无音频流
    int defaultCrf = 23;        ///< 默认质量（越小越清晰）
    QString defaultSpeedPreset; ///< x264/x265 的 -preset 值
    int defaultAudioBitrateKbps = 192;
    bool videoCopyable = true;  ///< 是否允许"直接复制视频流"（无损快速）
    bool audioCopyable = true;  ///< 是否允许"直接复制音频流"
    bool supportsHardware = true; ///< 是否支持硬件编码加速
    bool subtitleCapable = false; ///< 该容器能否承载字幕轨（mp4/mkv/mov 可以；avi/gif/音频不行）
    /// 是否是"纯音频"预设（只输出音频，没有视频轨）。
    /// 注意：不能用"videoCodec 是否为空"来判断——无损换壳（remux）预设的 videoCodec
    /// 也是空的（因为它的意思是"原样复制"），但它是视频预设。这里踩过坑，
    /// 导致 MKV 无损换壳实际上没有映射/复制视频流。
    bool audioOnly = false;

    bool isValid() const { return !id.isEmpty(); }
    bool isAudioOnly() const { return audioOnly; }
};

/// 内置预设全集（顺序即界面展示顺序）
const QList<ConvertPreset> &builtinPresets();

/// 按 id 查找；找不到时返回通用 MP4 预设
ConvertPreset presetById(const QString &id);

/// 默认预设 id
QString defaultPresetId();

} // namespace vf
