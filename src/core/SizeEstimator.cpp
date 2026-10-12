#include "core/SizeEstimator.h"
#include "core/Format.h"

#include <cmath>

namespace vf {

namespace {

/// 音频码率（含容器开销）
qint64 audioBitrateFor(const MediaInfo &info, const ConvertPreset &preset, const ConvertOptions &options)
{
    if (options.removeAudio || !info.hasAudio())
        return 0;
    if (options.copyAudio && preset.audioCopyable) {
        const AudioStreamInfo &audio = info.audios.first();
        return audio.bitrate > 0 ? audio.bitrate : 192000;
    }
    if (preset.audioCodec.isEmpty())
        return 0;
    if (preset.audioCodec.startsWith(QLatin1String("pcm_s16le"))) {
        const int sampleRate = options.audioSampleRate > 0 ? options.audioSampleRate : 48000;
        const int channels = options.audioChannels > 0
            ? options.audioChannels
            : (info.hasAudio() ? info.audios.first().channels : 2);
        return static_cast<qint64>(sampleRate) * 16 * qMax(1, channels);
    }
    if (preset.audioCodec == QLatin1String("flac"))
        return 900000;
    return static_cast<qint64>(qMax(64, options.audioBitrateKbps)) * 1000;
}

} // namespace

double SizeEstimator::crfToBitrateMbps(int crf, const QString &codec, double fps)
{
    // 经验模型：1080p30 下 x264 CRF23 ≈ 4.5 Mb/s，每降低 6 个 CRF 码率约翻倍
    const double base = 4.5;
    const double step = std::pow(2.0, (23.0 - static_cast<double>(crf)) / 6.0);
    double mbps = base * step;

    if (codec.contains(QLatin1String("265")) || codec.contains(QLatin1String("hevc")))
        mbps *= 0.62;                     // H.265 同画质省约 38%
    else if (codec.contains(QLatin1String("vp9")))
        mbps *= 0.70;
    else if (codec == QLatin1String("mpeg4"))
        mbps *= 1.35;                     // 老编码器效率低

    if (fps > 0.0 && fps < 25.0)
        mbps *= 0.8;                      // 低帧率画面变化少
    else if (fps > 45.0)
        mbps *= 1.35;

    return qMax(0.15, mbps);
}

SizeEstimate SizeEstimator::estimate(const MediaInfo &info,
                                     const ConvertPreset &preset,
                                     const ConvertOptions &options)
{
    SizeEstimate result;
    const double duration = options.effectiveDuration(info.durationSeconds);
    if (duration <= 0.0) {
        result.explanation = QStringLiteral("无法预估值（读不到时长）");
        return result;
    }

    const qint64 audioBits = audioBitrateFor(info, preset, options);

    // ---- 音频-only 预设：直接精确计算 ----
    if (preset.isAudioOnly()) {
        result.bytes = static_cast<qint64>(audioBits / 8.0 * duration * 1.02);
        result.reliable = audioBits > 0;
        result.explanation = QStringLiteral("音频 %1 × %2")
                                 .arg(fmt::bitrate(audioBits), fmt::duration(duration));
    } else {
        const VideoStreamInfo *video = info.primaryVideo();
        if (!video) {
            result.explanation = QStringLiteral("无视频流");
            return result;
        }

        // 输出像素总量相对源的变化（缩放会显著影响体积）
        double pixelFactor = 1.0;
        if (options.scaleHeight > 0 && video->height > 0)
            pixelFactor = std::pow(static_cast<double>(options.scaleHeight) / video->height, 2.0);
        else if (options.scaleWidth > 0 && video->width > 0)
            pixelFactor = std::pow(static_cast<double>(options.scaleWidth) / video->width, 2.0);

        const double fps = options.fps > 0.0 ? options.fps : video->fps;

        qint64 videoBits = 0;
        if (options.copyVideo && preset.videoCopyable) {
            videoBits = video->bitrate > 0 ? video->bitrate : info.bitrate;
            result.reliable = true;
            result.explanation = QStringLiteral("视频流直接复制，体积与原文件基本一致");
        } else if (options.videoBitrateKbps > 0) {
            videoBits = options.videoBitrateKbps * 1000;
            result.reliable = true;
            result.explanation = QStringLiteral("视频 %1（固定码率）× %2")
                                     .arg(fmt::bitrate(videoBits), fmt::duration(duration));
        } else if (preset.videoCodec == QLatin1String("gif")) {
            // GIF 体积 ≈ 像素数 × 帧数 × 每像素字节（256 色调色板 + LZW）
            const int height = options.scaleHeight > 0 ? options.scaleHeight : 270;
            const double width = video->height > 0
                ? height * (static_cast<double>(video->width) / video->height) : 480.0;
            const double gifFps = options.fps > 0.0 ? options.fps : 12.0;
            videoBits = static_cast<qint64>(width * height * gifFps * 0.35 * 8.0);
            result.explanation = QStringLiteral("GIF 按 %1x%2 · %3 fps 估算，实际取决于画面复杂度")
                                     .arg(static_cast<int>(width)).arg(height)
                                     .arg(QString::number(gifFps, 'f', 0));
        } else {
            const double mbps = crfToBitrateMbps(options.crf,
                                                 preset.videoCodec,
                                                 fps) * pixelFactor;
            videoBits = static_cast<qint64>(mbps * 1'000'000.0);
            result.explanation = QStringLiteral("按 CRF %1 经验模型推算视频约 %2 × %3")
                                     .arg(options.crf)
                                     .arg(fmt::bitrate(videoBits), fmt::duration(duration));
        }

        result.bytes = static_cast<qint64>((videoBits + audioBits) / 8.0 * duration * 1.01);
    }

    if (result.bytes > 0) {
        result.explanation += QStringLiteral("（含约 1% 容器开销）");
        if (info.fileSize > 0) {
            const double ratio = 100.0 * static_cast<double>(result.bytes) / info.fileSize;
            result.comparison = QStringLiteral("约为原文件的 %1%").arg(QString::number(ratio, 'f', 0));
        }
    }
    return result;
}

QString SizeEstimator::describe(const SizeEstimate &estimate)
{
    if (estimate.bytes <= 0)
        return estimate.explanation.isEmpty() ? QStringLiteral("无法预估") : estimate.explanation;

    QString text = QStringLiteral("预计输出约 %1").arg(fmt::size(estimate.bytes));
    if (!estimate.reliable)
        text += QStringLiteral("（估算值）");
    if (!estimate.comparison.isEmpty())
        text += QStringLiteral("，%1").arg(estimate.comparison);
    return text;
}

FreeSpaceVerdict SizeEstimator::evaluateFreeSpace(qint64 estimatedBytes, qint64 freeBytes,
                                                  double safetyFactor)
{
    FreeSpaceVerdict verdict;
    verdict.freeBytes = qMax<qint64>(0, freeBytes);

    if (estimatedBytes <= 0) {
        // 估不出来就不拦，交给 ffmpeg 自己报错——总比误拦用户好
        verdict.enough = true;
        verdict.requiredBytes = 0;
        return verdict;
    }
    if (safetyFactor < 1.0)
        safetyFactor = 1.0;

    verdict.requiredBytes = static_cast<qint64>(estimatedBytes * safetyFactor);
    if (verdict.freeBytes >= verdict.requiredBytes) {
        verdict.enough = true;
        return verdict;
    }

    verdict.enough = false;
    verdict.missingBytes = verdict.requiredBytes - verdict.freeBytes;
    return verdict;
}

} // namespace vf
