#pragma once

// =============================================================================
//  VideoForge - ffmpeg 命令行构建器
//
//  这是"用命令行而不是 ffmpeg API"的核心：把界面上的选项翻译成参数列表。
//  纯函数实现（不碰 QProcess），因此可以完整单元测试。
// =============================================================================

#include "core/ConvertPreset.h"
#include "core/MediaInfo.h"

#include <QString>
#include <QStringList>

namespace vf {

/// 界面上的全部转换选项
struct ConvertOptions
{
    // ---- 流策略 ----
    bool copyVideo = false;         ///< 视频流直接复制（不重编码）
    bool copyAudio = false;         ///< 音频流直接复制
    bool removeAudio = false;       ///< 丢弃音频

    // ---- 视频参数 ----
    int crf = 23;                   ///< 质量（CRF / QP），越小越清晰
    QString speedPreset;            ///< x264/x265 -preset
    int scaleWidth = 0;             ///< 目标宽度，0=等比只按高度，-1=不变
    int scaleHeight = 0;            ///< 目标高度，0=保持原始
    double fps = 0.0;               ///< 0 = 保持原始帧率
    qint64 videoBitrateKbps = 0;    ///< >0 时使用固定码率而非 CRF
    bool hardwareAccel = false;
    HardwareAccel accel = HardwareAccel::None;

    // ---- 音频参数 ----
    int audioBitrateKbps = 192;
    int audioChannels = 0;          ///< 0 = 保持原始
    int audioSampleRate = 0;        ///< 0 = 保持原始

    // ---- 裁剪 ----
    bool trimEnabled = false;
    double trimStart = 0.0;
    double trimEnd = 0.0;           ///< 相对源文件的时间；<= trimStart 视为无效

    // ---- 输出/杂项 ----
    QString outputPath;
    bool fastStart = true;          ///< MP4/MOV：moov 前置，便于网络播放
    bool overwrite = true;
    bool stripMetadata = false;     ///< -map_metadata -1
    /// 字幕处理策略：
    ///   "drop" —— 丢弃（默认，与旧行为一致，但界面会明确提示）
    ///   "copy" —— 保留原字幕轨（mp4/mov 转成 mov_text，mkv 直接复制）
    QString subtitleMode = QStringLiteral("drop");
    QString extraArguments;         ///< 高级：用户手动追加的参数

    /// 实际输出时长（秒），用于进度与体积估算
    double effectiveDuration(double sourceDuration) const;
};

/// 构建结果
struct CommandPlan
{
    QString program;                ///< ffmpeg 可执行文件路径
    QStringList arguments;          ///< 完整参数列表（不含 program）
    QString displayCommand;         ///< 可直接复制到终端运行的命令行
    QStringList warnings;           ///< 需要提示给用户的降级/调整说明
    bool valid = false;
    QString errorMessage;           ///< valid=false 时的原因

    /// 供界面"复制命令"按钮使用
    QString toDisplayString() const { return displayCommand; }
};

class CommandBuilder
{
public:
    /// 根据预设与选项构建一次转换命令
    static CommandPlan build(const QString &ffmpegPath,
                             const MediaInfo &info,
                             const ConvertPreset &preset,
                             const ConvertOptions &options);

    /// 把 ffmpeg 编码器名按硬件加速类型映射（libx264 -> h264_nvenc 等）
    static QString mapCodecForAccel(const QString &softwareCodec, HardwareAccel accel);

    /// 输出文件默认路径：同目录 + "文件名_转换后.ext"（重名自动加序号）
    static QString defaultOutputPath(const QString &inputPath, const QString &extension,
                                     const QString &outputDirectory = QString());

    /// 给命令行做安全引用（用于展示与复制）
    static QString quoteArgument(const QString &argument);
    static QString joinForDisplay(const QString &program, const QStringList &arguments);
};

} // namespace vf
