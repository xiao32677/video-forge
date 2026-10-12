// =============================================================================
//  VideoForge - core 层单元测试
//
//  覆盖：格式化工具 / ffprobe JSON 解析 / ffmpeg 命令行构建 / 进度解析 /
//        硬件编码器识别 / 体积估算 / 输出路径生成
//
//  运行：ctest --output-on-failure  或  直接执行 vf_tests.exe
// =============================================================================

#include "core/CommandBuilder.h"
#include "core/ConvertPreset.h"
#include "core/Format.h"
#include "core/HwAccelDetector.h"
#include "core/MediaProbe.h"
#include "core/ProgressParser.h"
#include "core/SizeEstimator.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>

#include <cmath>

using namespace vf;

namespace {

int g_checks = 0;
int g_failures = 0;
QString g_currentSection;

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

void section(const QString &name)
{
    g_currentSection = name;
    out() << "\n── " << name << " ──\n";
    out().flush();
}

void check(bool condition, const QString &expression, int line, const QString &detail = QString())
{
    ++g_checks;
    if (condition) {
        out() << "  [PASS] " << expression << "\n";
    } else {
        ++g_failures;
        out() << "  [FAIL] " << expression;
        if (!detail.isEmpty())
            out() << "  →  " << detail;
        out() << "  (第 " << line << " 行)\n";
    }
    out().flush();
}

#define CHECK(cond) check((cond), QStringLiteral(#cond), __LINE__)
#define CHECK_EQ(actual, expected)                                                        \
    do {                                                                                  \
        const auto _a = (actual);                                                         \
        const auto _e = (expected);                                                       \
        check(_a == _e, QStringLiteral(#actual " == " #expected), __LINE__,               \
              QStringLiteral("实际=%1 期望=%2").arg(_a).arg(_e));                          \
    } while (false)
#define CHECK_NEAR(actual, expected, tolerance)                                           \
    do {                                                                                  \
        const double _a = static_cast<double>(actual);                                     \
        const double _e = static_cast<double>(expected);                                   \
        check(std::abs(_a - _e) <= (tolerance),                                            \
              QStringLiteral(#actual " ≈ " #expected), __LINE__,                           \
              QStringLiteral("实际=%1 期望=%2").arg(_a).arg(_e));                           \
    } while (false)

/// 一段真实的 ffprobe JSON（含视频/音频/字幕流、章节、旋转信息）
const char *kSampleProbeJson = R"JSON(
{
    "streams": [
        {
            "index": 0,
            "codec_name": "h264",
            "codec_long_name": "H.264 / AVC / MPEG-4 AVC / MPEG-4 part 10",
            "profile": "High",
            "codec_type": "video",
            "width": 1920,
            "height": 1080,
            "pix_fmt": "yuv420p",
            "color_space": "bt709",
            "color_transfer": "bt709",
            "color_primaries": "bt709",
            "r_frame_rate": "30000/1001",
            "avg_frame_rate": "30000/1001",
            "duration": "83.417000",
            "bit_rate": "5000000",
            "bits_per_raw_sample": "8",
            "disposition": { "default": 1 },
            "tags": { "language": "und", "title": "主视频" }
        },
        {
            "index": 1,
            "codec_name": "aac",
            "codec_type": "audio",
            "profile": "LC",
            "sample_fmt": "fltp",
            "channel_layout": "stereo",
            "sample_rate": "48000",
            "channels": 2,
            "bit_rate": "192000",
            "disposition": { "default": 1 },
            "tags": { "language": "zho" }
        },
        {
            "index": 2,
            "codec_name": "subrip",
            "codec_type": "subtitle",
            "disposition": { "default": 0, "forced": 1 },
            "tags": { "language": "eng" }
        }
    ],
    "chapters": [ { "id": 0 }, { "id": 1 } ],
    "format": {
        "format_name": "mov,mp4,m4a,3gp,3g2,mj2",
        "format_long_name": "QuickTime / MOV",
        "duration": "83.417000",
        "size": "53000000",
        "bit_rate": "5082400"
    }
}
)JSON";

/// 构造一份可用的 MediaInfo（用于命令行构建测试）
MediaInfo sampleInfo()
{
    return MediaProbe::parseJson(QByteArray(kSampleProbeJson), QStringLiteral("C:/videos/测试 视频.mp4"));
}

void testFormat()
{
    section(QStringLiteral("格式化工具 (Format)"));

    CHECK_EQ(fmt::size(0), QStringLiteral("0 B"));
    CHECK_EQ(fmt::size(1023), QStringLiteral("1023 B"));
    CHECK_EQ(fmt::size(1024), QStringLiteral("1.00 KB"));
    CHECK_EQ(fmt::size(1536), QStringLiteral("1.50 KB"));
    CHECK_EQ(fmt::size(5 * 1024 * 1024), QStringLiteral("5.00 MB"));

    CHECK_EQ(fmt::duration(0), QStringLiteral("00:00"));
    CHECK_EQ(fmt::duration(59.5), QStringLiteral("01:00"));
    CHECK_EQ(fmt::duration(83.417), QStringLiteral("01:23"));
    CHECK_EQ(fmt::duration(3725), QStringLiteral("01:02:05"));
    CHECK_EQ(fmt::duration(-1), QStringLiteral("--:--"));

    CHECK_EQ(fmt::bitrate(5000000), QStringLiteral("5.00 Mb/s"));
    CHECK_EQ(fmt::bitrate(192000), QStringLiteral("192 kb/s"));
    CHECK_EQ(fmt::bitrate(0), QStringLiteral("--"));

    CHECK_EQ(fmt::fps(29.970029), QStringLiteral("29.97 fps"));
    CHECK_EQ(fmt::fps(30.0), QStringLiteral("30 fps"));
    CHECK_EQ(fmt::aspectRatio(1920, 1080), QStringLiteral("16:9"));
    CHECK_EQ(fmt::aspectRatio(1920, 1440), QStringLiteral("4:3"));
    CHECK_EQ(fmt::aspectRatio(1080, 1920), QStringLiteral("9:16"));

    CHECK_EQ(fmt::ffmpegTimestamp(0.0), QStringLiteral("00:00:00.000"));
    CHECK_EQ(fmt::ffmpegTimestamp(83.417), QStringLiteral("00:01:23.417"));
    CHECK_EQ(fmt::ffmpegTimestamp(3725.5), QStringLiteral("01:02:05.500"));

    CHECK_NEAR(fmt::parseTimestamp(QStringLiteral("00:01:23.417")), 83.417, 0.001);
    CHECK_NEAR(fmt::parseTimestamp(QStringLiteral("01:02:05")), 3725.0, 0.001);
    CHECK_NEAR(fmt::parseTimestamp(QStringLiteral("45")), 45.0, 0.001);
    CHECK(fmt::parseTimestamp(QStringLiteral("abc")) < 0);

    CHECK(fmt::isSupportedMediaFile(QStringLiteral("C:/a/b.MP4")) == false); // 文件不存在 -> false
    CHECK(fmt::supportedMediaExtensions().contains(QStringLiteral("mkv")));
    CHECK(fmt::mediaFileDialogFilter().contains(QStringLiteral("*.mp4")));
}

void testParseJson()
{
    section(QStringLiteral("ffprobe JSON 解析 (MediaProbe)"));

    const MediaInfo info = sampleInfo();
    CHECK(info.ok);
    CHECK_EQ(info.containerText(), QStringLiteral("QuickTime / MOV"));
    CHECK_NEAR(info.durationSeconds, 83.417, 0.001);
    CHECK_EQ(info.fileSize, 53000000LL);
    CHECK_EQ(info.chapterCount, 2);
    CHECK_EQ(info.videos.size(), 1);
    CHECK_EQ(info.audios.size(), 1);
    CHECK_EQ(info.subtitles.size(), 1);
    CHECK(info.hasVideo());
    CHECK(info.hasAudio());
    CHECK(info.hasSubtitles());

    const VideoStreamInfo *video = info.primaryVideo();
    CHECK(video != nullptr);
    if (video) {
        CHECK_EQ(video->codecName, QStringLiteral("h264"));
        CHECK_EQ(video->width, 1920);
        CHECK_EQ(video->height, 1080);
        CHECK_NEAR(video->fps, 29.970029, 0.001);
        CHECK_EQ(video->pixelFormat, QStringLiteral("yuv420p"));
        CHECK_EQ(video->bitDepth, 8);
        CHECK_EQ(video->title, QStringLiteral("主视频"));
    }

    const AudioStreamInfo &audio = info.audios.first();
    CHECK_EQ(audio.codecName, QStringLiteral("aac"));
    CHECK_EQ(audio.sampleRate, 48000);
    CHECK_EQ(audio.channels, 2);
    CHECK_EQ(audio.language, QStringLiteral("zho"));

    CHECK_EQ(info.subtitles.first().isForced, true);
    CHECK_EQ(info.codecSummary(), QStringLiteral("H264 + AAC"));
    CHECK(info.summaryLine().contains(QStringLiteral("1920x1080")));

    // 空 JSON / 无流文件要给出人类可读错误
    const MediaInfo empty = MediaProbe::parseJson(QByteArray("{}"), QStringLiteral("x.mp4"));
    CHECK(!empty.ok);
    CHECK(!empty.errorMessage.isEmpty());

    const MediaInfo broken = MediaProbe::parseJson(QByteArray("{ not json"), QStringLiteral("x.mp4"));
    CHECK(!broken.ok);

    // 10bit 位深从 pix_fmt 推断
    const QByteArray tenBit = R"({"streams":[{"index":0,"codec_type":"video","codec_name":"hevc",
        "width":3840,"height":2160,"pix_fmt":"yuv420p10le","avg_frame_rate":"24/1"}],
        "format":{"format_name":"matroska","duration":"10.0"}})";
    const MediaInfo hdr = MediaProbe::parseJson(tenBit, QStringLiteral("x.mkv"));
    CHECK(hdr.ok);
    CHECK_EQ(hdr.primaryVideo()->bitDepth, 10);
}

void testCommandBuilder()
{
    section(QStringLiteral("ffmpeg 命令行构建 (CommandBuilder)"));

    const MediaInfo info = sampleInfo();
    const QString ffmpeg = QStringLiteral("C:/ffmpeg/bin/ffmpeg.exe");

    // ---- 默认 MP4 / H.264 ----
    {
        const ConvertPreset preset = presetById(QStringLiteral("mp4-h264"));
        ConvertOptions options;
        options.outputPath = QStringLiteral("C:/out/out.mp4");
        const CommandPlan plan = CommandBuilder::build(ffmpeg, info, preset, options);

        CHECK(plan.valid);
        CHECK(plan.arguments.contains(QStringLiteral("-progress")));
        CHECK(plan.arguments.contains(QStringLiteral("pipe:1")));
        CHECK(plan.arguments.contains(QStringLiteral("-c:v")));
        CHECK(plan.arguments.contains(QStringLiteral("libx264")));
        CHECK(plan.arguments.contains(QStringLiteral("aac")));
        CHECK(plan.arguments.contains(QStringLiteral("-movflags")));
        CHECK(plan.arguments.contains(QStringLiteral("+faststart")));
        CHECK(plan.arguments.last() == QStringLiteral("C:/out/out.mp4"));
        CHECK(plan.displayCommand.contains(QStringLiteral("ffmpeg.exe")));
        // -i 必须出现在输出之前
        CHECK(plan.arguments.indexOf(QStringLiteral("-i")) < plan.arguments.size() - 1);
    }

    // ---- 无损换壳：视频音频都必须"原样复制" ----
    {
        const ConvertPreset preset = presetById(QStringLiteral("mkv-remux"));
        ConvertOptions options;
        options.copyVideo = true;
        options.copyAudio = true;
        options.outputPath = QStringLiteral("C:/out/out.mkv");
        const CommandPlan plan = CommandBuilder::build(ffmpeg, info, preset, options);
        CHECK(plan.valid);
        // 关键回归：remux 预设的 videoCodec 是空的（表示"原样复制"），
        // 但它绝不能被当成"纯音频预设"——否则视频流既不被映射也不被复制，
        // 输出会退化成用默认编码器重新编码。
        CHECK(!preset.isAudioOnly());
        CHECK(plan.arguments.contains(QStringLiteral("-map")));
        CHECK(plan.arguments.contains(QStringLiteral("0:v:0?")));
        CHECK_EQ(plan.arguments.count(QStringLiteral("copy")), 2);   // 视频 + 音频各一次
        CHECK(!plan.arguments.contains(QStringLiteral("libx264")));
    }

    // ---- 纯音频预设必须被正确识别 ----
    {
        CHECK(presetById(QStringLiteral("mp3")).isAudioOnly());
        CHECK(presetById(QStringLiteral("m4a")).isAudioOnly());
        CHECK(presetById(QStringLiteral("wav")).isAudioOnly());
        CHECK(presetById(QStringLiteral("flac")).isAudioOnly());
        CHECK(presetById(QStringLiteral("m4a-copy")).isAudioOnly());
        CHECK(!presetById(QStringLiteral("mkv-remux")).isAudioOnly());
        CHECK(!presetById(QStringLiteral("gif")).isAudioOnly());
        CHECK(!presetById(QStringLiteral("avi-xvid")).isAudioOnly());
    }

    // ---- 裁剪 + 缩放 + 帧率 ----
    {
        const ConvertPreset preset = presetById(QStringLiteral("mp4-h264"));
        ConvertOptions options;
        options.outputPath = QStringLiteral("C:/out/trim.mp4");
        options.trimEnabled = true;
        options.trimStart = 10.0;
        options.trimEnd = 40.0;
        options.scaleHeight = 720;
        options.fps = 30.0;
        const CommandPlan plan = CommandBuilder::build(ffmpeg, info, preset, options);
        CHECK(plan.valid);
        CHECK_EQ(plan.arguments.count(QStringLiteral("-ss")), 1);
        CHECK(plan.arguments.contains(QStringLiteral("-t")));
        CHECK(plan.arguments.contains(QStringLiteral("00:00:30.000"))); // 40 - 10
        const QString filter = plan.arguments.at(plan.arguments.indexOf(QStringLiteral("-vf")) + 1);
        CHECK(filter.contains(QStringLiteral("scale=-2:720")));
        CHECK(filter.contains(QStringLiteral("fps=30.000")));
        CHECK_NEAR(options.effectiveDuration(info.durationSeconds), 30.0, 0.001);
    }

    // ---- 音频-only 预设 ----
    {
        const ConvertPreset preset = presetById(QStringLiteral("mp3"));
        ConvertOptions options;
        options.outputPath = QStringLiteral("C:/out/out.mp3");
        const CommandPlan plan = CommandBuilder::build(ffmpeg, info, preset, options);
        CHECK(plan.valid);
        CHECK(plan.arguments.contains(QStringLiteral("-vn")));
        CHECK(plan.arguments.contains(QStringLiteral("libmp3lame")));
        CHECK(!plan.arguments.contains(QStringLiteral("libx264")));
    }

    // ---- 硬件加速映射 ----
    {
        const ConvertPreset preset = presetById(QStringLiteral("mp4-h264"));
        ConvertOptions options;
        options.outputPath = QStringLiteral("C:/out/hw.mp4");
        options.hardwareAccel = true;
        options.accel = HardwareAccel::Nvenc;
        const CommandPlan plan = CommandBuilder::build(ffmpeg, info, preset, options);
        CHECK(plan.valid);
        CHECK(plan.arguments.contains(QStringLiteral("h264_nvenc")));
        CHECK(!plan.arguments.contains(QStringLiteral("libx264")));
        CHECK(!plan.warnings.isEmpty());
    }

    // ---- 预设不支持硬件加速时给出提示并回退 ----
    {
        const ConvertPreset preset = presetById(QStringLiteral("gif"));
        ConvertOptions options;
        options.outputPath = QStringLiteral("C:/out/a.gif");
        options.hardwareAccel = true;
        options.accel = HardwareAccel::Nvenc;
        const CommandPlan plan = CommandBuilder::build(ffmpeg, info, preset, options);
        CHECK(plan.valid);
        CHECK(plan.arguments.contains(QStringLiteral("gif")));
        CHECK(!plan.warnings.isEmpty());
    }

    // ---- 错误场景：无视频流却要求转视频 ----
    {
        const QByteArray audioOnly = R"({"streams":[{"index":0,"codec_type":"audio",
            "codec_name":"mp3","sample_rate":"44100","channels":2}],
            "format":{"format_name":"mp3","duration":"180.0"}})";
        const MediaInfo audioInfo = MediaProbe::parseJson(audioOnly, QStringLiteral("song.mp3"));
        const ConvertPreset preset = presetById(QStringLiteral("mp4-h264"));
        ConvertOptions options;
        options.outputPath = QStringLiteral("C:/out/out.mp4");
        const CommandPlan plan = CommandBuilder::build(ffmpeg, audioInfo, preset, options);
        CHECK(!plan.valid);
        CHECK(!plan.errorMessage.isEmpty());
    }

    // ---- 不覆盖时使用 -n ----
    {
        const ConvertPreset preset = presetById(QStringLiteral("mp4-h264"));
        ConvertOptions options;
        options.outputPath = QStringLiteral("C:/out/out.mp4");
        options.overwrite = false;
        const CommandPlan plan = CommandBuilder::build(ffmpeg, info, preset, options);
        CHECK(plan.arguments.contains(QStringLiteral("-n")));
        CHECK(!plan.arguments.contains(QStringLiteral("-y")));
    }

    // ---- 中文路径与空格必须被正确转义（用于"复制命令行"）----
    {
        CHECK_EQ(CommandBuilder::quoteArgument(QStringLiteral("a b.mp4")),
                 QStringLiteral("\"a b.mp4\""));
        CHECK_EQ(CommandBuilder::quoteArgument(QStringLiteral("简单.mp4")),
                 QStringLiteral("简单.mp4"));
    }

    // ---- 编码器映射表 ----
    CHECK_EQ(CommandBuilder::mapCodecForAccel(QStringLiteral("libx265"), HardwareAccel::Qsv),
             QStringLiteral("hevc_qsv"));
    CHECK_EQ(CommandBuilder::mapCodecForAccel(QStringLiteral("libx264"), HardwareAccel::Amf),
             QStringLiteral("h264_amf"));
    CHECK_EQ(CommandBuilder::mapCodecForAccel(QStringLiteral("libx264"), HardwareAccel::None),
             QStringLiteral("libx264"));
}

void testOutputPath()
{
    section(QStringLiteral("输出路径生成"));

    QTemporaryDir temp;
    CHECK(temp.isValid());
    if (!temp.isValid())
        return;

    const QString input = temp.path() + QStringLiteral("/我的视频.mp4");
    QFile file(input);
    file.open(QIODevice::WriteOnly);
    file.write("x");
    file.close();

    const QString first = CommandBuilder::defaultOutputPath(input, QStringLiteral("mp4"));
    CHECK(first.endsWith(QStringLiteral("我的视频_转换后.mp4")));
    CHECK(QDir::fromNativeSeparators(first).startsWith(QDir::fromNativeSeparators(temp.path())));

    // 制造重名，必须自动加序号而不是覆盖
    QFile existing(first);
    existing.open(QIODevice::WriteOnly);
    existing.write("x");
    existing.close();

    const QString second = CommandBuilder::defaultOutputPath(input, QStringLiteral("mp4"));
    CHECK(second != first);
    CHECK(second.contains(QStringLiteral("(2)")));
}

void testProgressParser()
{
    section(QStringLiteral("进度解析 (ProgressParser)"));

    ProgressParser parser;

    // 模拟 ffmpeg -progress 的连续输出（含被切断的半行）
    const QByteArray chunk1 =
        "frame=100\nfps=25.4\nbitrate=1234.5kbits/s\ntotal_size=4567890\n"
        "out_time_us=4000000\nout_time_ms=4000000\nspeed=1.85x\nprogress=continue\n";
    CHECK(parser.feed(QString::fromUtf8(chunk1)));
    CHECK_EQ(parser.info().frame, 100);
    CHECK_NEAR(parser.info().fps, 25.4, 0.01);
    CHECK_EQ(parser.info().totalSize, 4567890LL);
    CHECK_NEAR(parser.info().outTimeSeconds(), 4.0, 0.001);
    CHECK_NEAR(parser.info().speed, 1.85, 0.01);
    CHECK(parser.info().bitrate >= 1234000 && parser.info().bitrate <= 1235000);
    CHECK(!parser.info().finished);

    // 半行 + 续行：必须能跨块正确拼接
    CHECK(!parser.feed(QStringLiteral("frame=2")));
    CHECK_EQ(parser.info().frame, 100);
    CHECK(parser.feed(QStringLiteral("00\nspeed=2.00x\n")));
    CHECK_EQ(parser.info().frame, 200);
    CHECK_NEAR(parser.info().speed, 2.0, 0.01);

    // 结束标记
    CHECK(parser.feed(QStringLiteral("progress=end\n")));
    CHECK(parser.info().finished);

    // 脏数据不能崩
    ProgressInfo info;
    CHECK(!ProgressParser::parseLine(QStringLiteral("garbage"), &info));
    CHECK(!ProgressParser::parseLine(QString(), &info));
    CHECK_EQ(ProgressParser::parseBitrate(QStringLiteral("N/A")), 0LL);
    CHECK_EQ(ProgressParser::parseBitrate(QStringLiteral("2.5Mbits/s")), 2500000LL);
}

void testHwAccel()
{
    section(QStringLiteral("硬件编码器识别 (HwAccelDetector)"));

    // 模拟 `ffmpeg -encoders` 的真实输出片段
    const QString encoderList = QStringLiteral(
        "Encoders:\n"
        " V..... = Video\n"
        " ------\n"
        " V....D libx264              libx264 H.264 / AVC (codec h264)\n"
        " V....D h264_nvenc           NVIDIA NVENC H.264 encoder (codec h264)\n"
        " V....D hevc_nvenc           NVIDIA NVENC hevc encoder (codec hevc)\n"
        " V....D h264_qsv             H.264 video encoder (Intel Quick Sync Video) (codec h264)\n"
        " A....D aac                  AAC (Advanced Audio Coding)\n");

    const QStringList encoders = HwAccelDetector::parseEncoders(encoderList);
    CHECK(encoders.contains(QStringLiteral("h264_nvenc")));
    CHECK(encoders.contains(QStringLiteral("libx264")));
    CHECK(encoders.contains(QStringLiteral("aac")));

    const QList<HardwareCapability> capabilities = HwAccelDetector::buildCapabilities(encoders);
    CHECK_EQ(capabilities.size(), 3);
    CHECK(capabilities.at(0).accel == HardwareAccel::Nvenc);
    // "编译进来了" 与 "本机能用" 是两件事：
    // buildCapabilities 只负责标记 present，available 必须经过真实试编码才置位
    CHECK(capabilities.at(0).present);
    CHECK(!capabilities.at(0).available);
    CHECK_EQ(capabilities.at(0).encoders.size(), 2);
    CHECK(capabilities.at(1).present);    // QSV
    CHECK(!capabilities.at(2).present);   // AMF 不在列表里
    CHECK_EQ(hardwareAccelName(HardwareAccel::Nvenc), QStringLiteral("NVIDIA NVENC"));

    // 没有硬件编码器时，bestAccel 必须安全返回 None
    HwAccelDetector detector;
    CHECK(detector.bestAccel() == HardwareAccel::None);

    // 试编码接口在参数非法时必须安全返回 false，不能崩
    CHECK(!HwAccelDetector::verifyEncoder(QString(), QStringLiteral("h264_nvenc")));
    CHECK(!HwAccelDetector::verifyEncoder(QStringLiteral("C:/nope/ffmpeg.exe"),
                                          QStringLiteral("h264_nvenc")));
}

void testSizeEstimator()
{
    section(QStringLiteral("输出体积预估 (SizeEstimator)"));

    const MediaInfo info = sampleInfo();
    const ConvertPreset preset = presetById(QStringLiteral("mp4-h264"));

    ConvertOptions options;
    options.outputPath = QStringLiteral("C:/out/out.mp4");
    options.crf = 23;
    const SizeEstimate estimate = SizeEstimator::estimate(info, preset, options);
    CHECK(estimate.bytes > 0);
    CHECK(!estimate.explanation.isEmpty());
    CHECK(!estimate.comparison.isEmpty());
    CHECK(!SizeEstimator::describe(estimate).isEmpty());

    // 质量越好（CRF 越小）体积应该越大
    ConvertOptions better = options;
    better.crf = 18;
    const SizeEstimate highQuality = SizeEstimator::estimate(info, preset, better);
    CHECK(highQuality.bytes > estimate.bytes);

    // 缩放应该显著减小体积
    ConvertOptions scaled = options;
    scaled.scaleHeight = 480;
    const SizeEstimate smallEstimate = SizeEstimator::estimate(info, preset, scaled);
    CHECK(smallEstimate.bytes < estimate.bytes);

    // 固定码率模式应该是精确值：5 Mb/s * 83.417s / 8 + 音频
    ConvertOptions fixed = options;
    fixed.videoBitrateKbps = 5000;
    fixed.audioBitrateKbps = 192;
    const SizeEstimate fixedEstimate = SizeEstimator::estimate(info, preset, fixed);
    CHECK(fixedEstimate.reliable);
    const double expectedBytes = (5000000.0 + 192000.0) / 8.0 * 83.417 * 1.01;
    CHECK_NEAR(static_cast<double>(fixedEstimate.bytes), expectedBytes, expectedBytes * 0.02);

    // 直接复制：应等于源文件大小量级
    ConvertOptions copy = options;
    copy.copyVideo = true;
    copy.copyAudio = true;
    const SizeEstimate copyEstimate = SizeEstimator::estimate(info, preset, copy);
    CHECK(copyEstimate.bytes > 0);
    CHECK(copyEstimate.reliable);
}

void testFreeSpaceGuard()
{
    section(QStringLiteral("磁盘空间判定 (SizeEstimator::evaluateFreeSpace)"));

    const qint64 mb = 1024LL * 1024LL;

    // 空间充足：10MB 需求 * 1.1 = 11MB，给 50MB
    {
        const FreeSpaceVerdict verdict = SizeEstimator::evaluateFreeSpace(10 * mb, 50 * mb);
        CHECK(verdict.enough);
        CHECK_EQ(verdict.missingBytes, 0LL);
        CHECK_EQ(verdict.requiredBytes, static_cast<qint64>(10 * mb * 1.10));
    }

    // 刚好等于估算值：因为安全余量 1.1，仍然应该判定为"不够"
    {
        const FreeSpaceVerdict verdict = SizeEstimator::evaluateFreeSpace(10 * mb, 10 * mb);
        CHECK(!verdict.enough);
        CHECK(verdict.missingBytes > 0);
        CHECK_EQ(verdict.missingBytes, verdict.requiredBytes - verdict.freeBytes);
    }

    // 严重不足：必须精确报出差额
    {
        const FreeSpaceVerdict verdict = SizeEstimator::evaluateFreeSpace(100 * mb, 30 * mb);
        CHECK(!verdict.enough);
        CHECK_EQ(verdict.requiredBytes, static_cast<qint64>(100 * mb * 1.10));
        CHECK_EQ(verdict.freeBytes, 30 * mb);
        CHECK_EQ(verdict.missingBytes, static_cast<qint64>(100 * mb * 1.10) - 30 * mb);
    }

    // 完全没空间
    {
        const FreeSpaceVerdict verdict = SizeEstimator::evaluateFreeSpace(5 * mb, 0);
        CHECK(!verdict.enough);
        CHECK_EQ(verdict.missingBytes, verdict.requiredBytes);
    }

    // 估不出体积时不能拦人（否则会误伤）
    {
        const FreeSpaceVerdict verdict = SizeEstimator::evaluateFreeSpace(0, 1 * mb);
        CHECK(verdict.enough);
        CHECK_EQ(verdict.requiredBytes, 0LL);
    }

    // 安全系数设为 1.0 时，刚好够就应该放行
    {
        const FreeSpaceVerdict verdict = SizeEstimator::evaluateFreeSpace(10 * mb, 10 * mb, 1.0);
        CHECK(verdict.enough);
    }

    // 负数/异常输入不能崩，也不能误判成"够"
    {
        const FreeSpaceVerdict verdict = SizeEstimator::evaluateFreeSpace(10 * mb, -5);
        CHECK(!verdict.enough);
        CHECK_EQ(verdict.freeBytes, 0LL);
    }
}

void testPresets()
{
    section(QStringLiteral("预设定义 (ConvertPreset)"));

    const QList<ConvertPreset> &presets = builtinPresets();
    CHECK(presets.size() >= 10);

    QSet<QString> ids;
    for (const ConvertPreset &preset : presets) {
        CHECK(!preset.id.isEmpty());
        CHECK(!preset.displayName.isEmpty());
        CHECK(!preset.extension.isEmpty());
        CHECK(!preset.description.isEmpty());
        CHECK(!ids.contains(preset.id));
        ids.insert(preset.id);
    }

    CHECK(presetById(QStringLiteral("mp4-h264")).videoCodec == QStringLiteral("libx264"));
    CHECK(presetById(QStringLiteral("mp3")).isAudioOnly());
    CHECK(presetById(QStringLiteral("不存在的id")).id == defaultPresetId());
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    out() << "============================================\n"
          << " VideoForge 单元测试 (core 层)\n"
          << "============================================\n";
    out().flush();

    testFormat();
    testParseJson();
    testCommandBuilder();
    testOutputPath();
    testProgressParser();
    testHwAccel();
    testSizeEstimator();
    testFreeSpaceGuard();
    testPresets();

    out() << "\n============================================\n"
          << QStringLiteral(" 共 %1 项断言，失败 %2 项\n").arg(g_checks).arg(g_failures)
          << (g_failures == 0 ? " 结果：全部通过 ✔\n" : " 结果：存在失败 ✘\n")
          << "============================================\n";
    out().flush();

    return g_failures == 0 ? 0 : 1;
}
