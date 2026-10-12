#include "core/CommandBuilder.h"
#include "core/Format.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>

namespace vf {

double ConvertOptions::effectiveDuration(double sourceDuration) const
{
    double duration = sourceDuration;
    if (trimEnabled) {
        const double start = qMax(0.0, trimStart);
        const double end = trimEnd > start ? trimEnd : sourceDuration;
        duration = qMax(0.0, end - start);
    }
    return duration;
}

QString CommandBuilder::mapCodecForAccel(const QString &softwareCodec, HardwareAccel accel)
{
    if (accel == HardwareAccel::None)
        return softwareCodec;

    struct Mapping { const char *software; const char *nvenc; const char *qsv; const char *amf; };
    static const Mapping table[] = {
        {"libx264", "h264_nvenc", "h264_qsv", "h264_amf"},
        {"libx265", "hevc_nvenc", "hevc_qsv", "hevc_amf"},
        {"mpeg4",   "h264_nvenc", "h264_qsv", "h264_amf"},
    };

    for (const Mapping &m : table) {
        if (softwareCodec == QLatin1String(m.software)) {
            switch (accel) {
            case HardwareAccel::Nvenc: return QString::fromLatin1(m.nvenc);
            case HardwareAccel::Qsv:   return QString::fromLatin1(m.qsv);
            case HardwareAccel::Amf:   return QString::fromLatin1(m.amf);
            default: break;
            }
        }
    }
    return softwareCodec;
}

QString CommandBuilder::quoteArgument(const QString &argument)
{
    if (argument.isEmpty())
        return QStringLiteral("\"\"");
    const bool needsQuote = argument.contains(QLatin1Char(' '))
        || argument.contains(QLatin1Char('"'))
        || argument.contains(QLatin1Char('\''))
        || argument.contains(QLatin1Char('&'))
        || argument.contains(QLatin1Char('('))
        || argument.contains(QLatin1Char(')'))
        || argument.contains(QLatin1Char(';'))
        || argument.contains(QLatin1Char('|'))
        || argument.contains(QLatin1Char('<'))
        || argument.contains(QLatin1Char('>'))
        || argument.contains(QLatin1Char('^'));
    if (!needsQuote)
        return argument;
    QString escaped = argument;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

QString CommandBuilder::joinForDisplay(const QString &program, const QStringList &arguments)
{
    QStringList parts;
    parts << quoteArgument(program);
    for (const QString &argument : arguments)
        parts << quoteArgument(argument);
    return parts.join(QLatin1Char(' '));
}

QString CommandBuilder::defaultOutputPath(const QString &inputPath, const QString &extension,
                                          const QString &outputDirectory)
{
    const QFileInfo info(inputPath);
    QDir dir = outputDirectory.isEmpty() ? info.absoluteDir() : QDir(outputDirectory);
    if (!dir.exists())
        QDir().mkpath(dir.absolutePath());

    const QString suffix = extension.startsWith(QLatin1Char('.')) ? extension.mid(1) : extension;
    const QString baseName = info.completeBaseName();

    QString path = dir.absoluteFilePath(QStringLiteral("%1_转换后.%2").arg(baseName, suffix));
    int counter = 2;
    while (QFileInfo::exists(path) && counter < 10000) {
        path = dir.absoluteFilePath(
            QStringLiteral("%1_转换后(%2).%3").arg(baseName).arg(counter).arg(suffix));
        ++counter;
    }
    return path;
}

CommandPlan CommandBuilder::build(const QString &ffmpegPath,
                                  const MediaInfo &info,
                                  const ConvertPreset &preset,
                                  const ConvertOptions &options)
{
    CommandPlan plan;
    plan.program = ffmpegPath;

    if (ffmpegPath.isEmpty()) {
        plan.errorMessage = QStringLiteral("ffmpeg 路径为空，请先在设置中指定 ffmpeg。");
        return plan;
    }
    if (options.outputPath.trimmed().isEmpty()) {
        plan.errorMessage = QStringLiteral("输出路径为空。");
        return plan;
    }
    if (!preset.isAudioOnly() && !info.hasVideo()) {
        plan.errorMessage = QStringLiteral("源文件没有视频流，无法转换为「%1」。请改用音频预设。")
                                .arg(preset.displayName);
        return plan;
    }
    if (preset.isAudioOnly() && !info.hasAudio()) {
        plan.errorMessage = QStringLiteral("源文件没有音频流，无法提取音频。");
        return plan;
    }

    const bool trimming = options.trimEnabled && options.trimEnd > options.trimStart;

    // ------------------------------------------------------------------ 全局
    QStringList args{
        QStringLiteral("-hide_banner"),
        QStringLiteral("-nostdin"),
        QStringLiteral("-loglevel"), QStringLiteral("warning"),
        QStringLiteral("-stats_period"), QStringLiteral("0.4"),
        QStringLiteral("-progress"), QStringLiteral("pipe:1"),
        QStringLiteral("-nostats"),
        options.overwrite ? QStringLiteral("-y") : QStringLiteral("-n"),
    };

    // ------------------------------------------------------------------ 输入
    // -ss 放在 -i 前：关键帧快速定位，长视频上快一个数量级
    if (trimming && options.trimStart > 0.0)
        args << QStringLiteral("-ss") << fmt::ffmpegTimestamp(options.trimStart);
    args << QStringLiteral("-i") << info.filePath;
    if (trimming)
        args << QStringLiteral("-t") << fmt::ffmpegTimestamp(options.trimEnd - options.trimStart);

    // ------------------------------------------------------------------ 流映射
    const bool wantVideoTrack = !preset.isAudioOnly() && info.hasVideo();
    if (wantVideoTrack)
        args << QStringLiteral("-map") << QStringLiteral("0:v:0?");
    if (!options.removeAudio && info.hasAudio())
        args << QStringLiteral("-map") << QStringLiteral("0:a:0?");

    // ------------------------------------------------------------------ 字幕
    // 字幕要么被明确保留，要么被明确告知会丢弃 —— 不能像以前那样"悄悄没了"。
    const bool wantSubtitles = (options.subtitleMode == QLatin1String("copy"))
        && info.hasSubtitles() && !preset.isAudioOnly();
    if (wantSubtitles) {
        if (!preset.subtitleCapable) {
            plan.warnings << QStringLiteral("「%1」这种容器放不下字幕轨，字幕不会被保留。")
                                 .arg(preset.displayName);
        } else {
            args << QStringLiteral("-map") << QStringLiteral("0:s?");
            // MP4/MOV 只认 mov_text，需要转一道（字幕很小，几乎不耗时）；
            // MKV 可以直接复制原字幕轨，保持零损失。
            const QString container = preset.container;
            if (container == QLatin1String("mp4") || container == QLatin1String("mov")
                || container == QLatin1String("ipod")) {
                args << QStringLiteral("-c:s") << QStringLiteral("mov_text");
            } else {
                args << QStringLiteral("-c:s") << QStringLiteral("copy");
            }
        }
    }

    // ------------------------------------------------------------------ 视频
    bool videoStreamCopied = false;
    if (wantVideoTrack) {
        const bool wantCopy = options.copyVideo && preset.videoCopyable;
        if (wantCopy) {
            args << QStringLiteral("-c:v") << QStringLiteral("copy");
            videoStreamCopied = true;
        } else {
            QString codec = preset.videoCodec;
            if (codec.isEmpty()) {
                codec = QStringLiteral("libx264");
                plan.warnings << QStringLiteral("该预设未指定视频编码器，已回退为 H.264。");
            }
            const bool isGif = (codec == QLatin1String("gif"));
            bool hardwareApplied = false;

            if (options.hardwareAccel) {
                if (!preset.supportsHardware || isGif) {
                    plan.warnings << QStringLiteral("预设「%1」不支持硬件加速，已使用软件编码。")
                                         .arg(preset.displayName);
                } else {
                    const QString mapped = mapCodecForAccel(codec, options.accel);
                    if (mapped != codec) {
                        codec = mapped;
                        hardwareApplied = true;
                        plan.warnings << QStringLiteral("已启用 %1 硬件编码。")
                                             .arg(hardwareAccelName(options.accel));
                    }
                }
            }

            args << QStringLiteral("-c:v") << codec;

            if (isGif) {
                // GIF 无法承载高质量渐变，强制调色板友好的参数
                QStringList gifFilters;
                gifFilters << QStringLiteral("fps=%1")
                                  .arg(QString::number(options.fps > 0.0 ? options.fps : 12.0, 'f', 3));
                if (options.scaleHeight > 0)
                    gifFilters << QStringLiteral("scale=-2:%1:flags=lanczos").arg(options.scaleHeight);
                else if (options.scaleWidth > 0)
                    gifFilters << QStringLiteral("scale=%1:-2:flags=lanczos").arg(options.scaleWidth);
                else
                    gifFilters << QStringLiteral("scale=480:-2:flags=lanczos");
                gifFilters << QStringLiteral("split[a][b];[a]palettegen=stats_mode=diff[p];[b][p]paletteuse=dither=bayer");

                args << QStringLiteral("-filter_complex")
                     << gifFilters.join(QLatin1Char(','))
                     << QStringLiteral("-loop") << QStringLiteral("0");
            } else {
                // ---- 质量参数：码率模式 / 硬件 CQ / 软件 CRF ----
                if (options.videoBitrateKbps > 0) {
                    args << QStringLiteral("-b:v") << QStringLiteral("%1k").arg(options.videoBitrateKbps)
                         << QStringLiteral("-maxrate") << QStringLiteral("%1k").arg(options.videoBitrateKbps * 2)
                         << QStringLiteral("-bufsize") << QStringLiteral("%1k").arg(options.videoBitrateKbps * 4);
                } else if (hardwareApplied) {
                    args << QStringLiteral("-rc") << QStringLiteral("vbr")
                         << QStringLiteral("-cq") << QString::number(options.crf);
                } else {
                    args << QStringLiteral("-crf") << QString::number(options.crf);
                }

                if (!options.speedPreset.isEmpty()
                    && (codec.startsWith(QLatin1String("libx26"))
                        || codec == QLatin1String("libvpx-vp9"))) {
                    args << QStringLiteral("-preset") << options.speedPreset;
                }

                // ---- 缩放 / 帧率 ----
                QStringList filters;
                if (options.scaleHeight > 0 && options.scaleWidth > 0)
                    filters << QStringLiteral("scale=%1:%2:flags=lanczos")
                                   .arg(options.scaleWidth).arg(options.scaleHeight);
                else if (options.scaleHeight > 0)
                    filters << QStringLiteral("scale=-2:%1:flags=lanczos").arg(options.scaleHeight);
                else if (options.scaleWidth > 0)
                    filters << QStringLiteral("scale=%1:-2:flags=lanczos").arg(options.scaleWidth);
                if (options.fps > 0.0)
                    filters << QStringLiteral("fps=%1").arg(QString::number(options.fps, 'f', 3));
                if (!filters.isEmpty())
                    args << QStringLiteral("-vf") << filters.join(QLatin1Char(','));

                // ---- 兼容性：老设备只吃 yuv420p，10bit/444 源会黑屏 ----
                const QString pixelFormat = info.primaryVideo() ? info.primaryVideo()->pixelFormat : QString();
                const bool needsPixelFormatFix = (codec.startsWith(QLatin1String("libx26"))
                                                  || codec == QLatin1String("mpeg4"))
                    && !pixelFormat.isEmpty()
                    && !pixelFormat.startsWith(QLatin1String("yuv420p"))
                    && !pixelFormat.startsWith(QLatin1String("yuvj420p"));
                if (needsPixelFormatFix)
                    args << QStringLiteral("-pix_fmt") << QStringLiteral("yuv420p");
            }
        }
    }

    // ------------------------------------------------------------------ 音频
    bool audioStreamCopied = false;
    if (options.removeAudio) {
        args << QStringLiteral("-an");
    } else if (info.hasAudio()) {
        if (preset.audioCodec.isEmpty()) {
            if (options.copyAudio) {
                args << QStringLiteral("-c:a") << QStringLiteral("copy");
                audioStreamCopied = true;
            } else {
                args << QStringLiteral("-an");
            }
        } else if (options.copyAudio && preset.audioCopyable) {
            args << QStringLiteral("-c:a") << QStringLiteral("copy");
            audioStreamCopied = true;
        } else {
            args << QStringLiteral("-c:a") << preset.audioCodec;
            const bool isPcm = preset.audioCodec.startsWith(QLatin1String("pcm_"));
            if (!isPcm && options.audioBitrateKbps > 0)
                args << QStringLiteral("-b:a") << QStringLiteral("%1k").arg(options.audioBitrateKbps);
            if (options.audioChannels > 0)
                args << QStringLiteral("-ac") << QString::number(options.audioChannels);
            if (options.audioSampleRate > 0)
                args << QStringLiteral("-ar") << QString::number(options.audioSampleRate);
        }
    }

    if (preset.isAudioOnly() && !videoStreamCopied)
        args << QStringLiteral("-vn");

    // ------------------------------------------------------------------ 容器
    const QString container = preset.container;
    const bool isMp4Family = (container == QLatin1String("mp4")
                              || container == QLatin1String("mov")
                              || container == QLatin1String("ipod"));
    if (isMp4Family && options.fastStart)
        args << QStringLiteral("-movflags") << QStringLiteral("+faststart");

    if (container == QLatin1String("webm")) {
        if (videoStreamCopied || audioStreamCopied)
            plan.warnings << QStringLiteral("WebM 容器对编码有严格限制，直接复制流可能导致播放失败。");
    }

    // ------------------------------------------------------------------ 元数据与高级参数
    if (options.stripMetadata)
        args << QStringLiteral("-map_metadata") << QStringLiteral("-1");

    if (!options.extraArguments.trimmed().isEmpty()) {
        // 用户自定义参数：禁止覆盖关键控制参数，保证进度解析与输入输出不被破坏
        static const QStringList protectedKeys{
            QStringLiteral("-i"), QStringLiteral("-progress"), QStringLiteral("-y"),
            QStringLiteral("-n"), QStringLiteral("-nostats"), QStringLiteral("-hide_banner"),
        };
        const QStringList extra = QProcess::splitCommand(options.extraArguments.trimmed());
        for (const QString &piece : extra) {
            if (protectedKeys.contains(piece))
                continue;
            args << piece;
        }
        plan.warnings << QStringLiteral("已附加自定义参数，请自行确认参数正确性。");
    }

    args << options.outputPath;

    if (videoStreamCopied && options.scaleHeight > 0)
        plan.warnings << QStringLiteral("视频流为直接复制，缩放/帧率设置不会生效。");
    if (audioStreamCopied && options.audioBitrateKbps > 0 && preset.audioCodec.isEmpty())
        plan.warnings << QStringLiteral("音频流为直接复制，码率设置不会生效。");

    plan.arguments = args;
    plan.displayCommand = joinForDisplay(plan.program, args);
    plan.valid = true;
    return plan;
}

} // namespace vf
