#pragma once

// =============================================================================
//  VideoForge - 媒体信息数据模型
//  由 ffprobe 的 JSON 输出解析而来，与 UI 完全解耦。
// =============================================================================

#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>

namespace vf {

struct VideoStreamInfo
{
    int index = -1;
    QString codecName;
    QString codecLongName;
    QString profile;
    QString pixelFormat;
    QString colorSpace;
    QString colorTransfer;
    QString colorPrimaries;
    int width = 0;
    int height = 0;
    double fps = 0.0;
    double durationSeconds = 0.0;
    qint64 bitrate = 0;
    int bitDepth = 0;
    int rotationDegrees = 0;
    QString language;
    QString title;
    bool isDefault = false;

    bool isValid() const { return index >= 0; }
};

struct AudioStreamInfo
{
    int index = -1;
    QString codecName;
    QString codecLongName;
    QString profile;
    QString sampleFormat;
    QString channelLayout;
    int sampleRate = 0;
    int channels = 0;
    qint64 bitrate = 0;
    QString language;
    QString title;
    bool isDefault = false;

    bool isValid() const { return index >= 0; }
};

struct SubtitleStreamInfo
{
    int index = -1;
    QString codecName;
    QString language;
    QString title;
    bool isDefault = false;
    bool isForced = false;
};

/// ffprobe 一次探测的完整结果
struct MediaInfo
{
    QString filePath;
    qint64 fileSize = 0;
    QString formatName;       // 短名，如 "mov,mp4,m4a,3gp,3g2,mj2"
    QString formatLongName;   // 如 "QuickTime / MOV"
    double durationSeconds = 0.0;
    qint64 bitrate = 0;
    int chapterCount = 0;

    QList<VideoStreamInfo> videos;
    QList<AudioStreamInfo> audios;
    QList<SubtitleStreamInfo> subtitles;

    bool ok = false;          ///< 探测是否成功
    QString errorMessage;     ///< 失败原因（人类可读）

    /// 主视频流（取第一条，若无则返回无效对象）
    const VideoStreamInfo *primaryVideo() const
    {
        return videos.isEmpty() ? nullptr : &videos.first();
    }

    bool hasVideo() const { return !videos.isEmpty(); }
    bool hasAudio() const { return !audios.isEmpty(); }
    bool hasSubtitles() const { return !subtitles.isEmpty(); }

    /// 人类可读的容器描述
    QString containerText() const;

    /// 编码信息概览，例如 "H.264 + AAC"
    QString codecSummary() const;

    /// 单行摘要，例如 "1920x1080 · 29.97 fps · 00:01:23 · 12.4 MB"
    QString summaryLine() const;

    /// 文件基名（不含路径）
    QString baseName() const;

    void clear() { *this = MediaInfo(); }
};

} // namespace vf

Q_DECLARE_METATYPE(vf::MediaInfo)
