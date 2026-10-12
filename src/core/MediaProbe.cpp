#include "core/MediaProbe.h"
#include "core/FfmpegLocator.h"
#include "core/Format.h"
#include "core/ProcessUtil.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QRegularExpression>
#include <QtConcurrent/QtConcurrentRun>

namespace vf {

namespace {

qint64 toInt64(const QJsonValue &value)
{
    if (value.isString()) {
        bool ok = false;
        const qint64 v = value.toString().toLongLong(&ok);
        return ok ? v : 0;
    }
    return static_cast<qint64>(value.toDouble(0));
}

double toDouble(const QJsonValue &value)
{
    if (value.isString()) {
        bool ok = false;
        const double v = value.toString().toDouble(&ok);
        return ok ? v : 0.0;
    }
    return value.toDouble(0.0);
}

/// "30000/1001" -> 29.970029...
double parseRational(const QString &text)
{
    const QStringList parts = text.split(QLatin1Char('/'));
    bool okNum = false;
    const double numerator = parts.value(0).toDouble(&okNum);
    if (!okNum)
        return 0.0;
    if (parts.size() == 1)
        return numerator;
    bool okDen = false;
    const double denominator = parts.value(1).toDouble(&okDen);
    if (!okDen || denominator == 0.0)
        return 0.0;
    return numerator / denominator;
}

QString streamLanguage(const QJsonObject &stream)
{
    const QJsonObject tags = stream.value(QStringLiteral("tags")).toObject();
    QString lang = tags.value(QStringLiteral("language")).toString();
    if (lang == QLatin1String("und"))
        lang.clear();
    return lang;
}

QString streamTitle(const QJsonObject &stream)
{
    return stream.value(QStringLiteral("tags")).toObject().value(QStringLiteral("title")).toString();
}

bool streamIsDefault(const QJsonObject &stream)
{
    return stream.value(QStringLiteral("disposition")).toObject().value(QStringLiteral("default")).toInt(0) == 1;
}

int streamRotation(const QJsonObject &stream)
{
    const QJsonArray sideData = stream.value(QStringLiteral("side_data_list")).toArray();
    for (const QJsonValue &entry : sideData) {
        const QJsonObject object = entry.toObject();
        if (object.contains(QStringLiteral("rotation")))
            return static_cast<int>(object.value(QStringLiteral("rotation")).toDouble());
    }
    return 0;
}

} // namespace

MediaProbe::MediaProbe(FfmpegLocator *locator, QObject *parent)
    : QObject(parent)
    , m_locator(locator)
{
}

QByteArray MediaProbe::runFfprobe(const QString &ffprobePath, const QString &filePath, QString *errorOut)
{
    if (ffprobePath.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("ffprobe 不可用，请先在设置中指定 ffmpeg 路径。");
        return {};
    }

    QProcess process;
    suppressConsoleWindow(process);
    process.start(ffprobePath,
                  {QStringLiteral("-v"), QStringLiteral("error"),
                   QStringLiteral("-print_format"), QStringLiteral("json"),
                   QStringLiteral("-show_format"),
                   QStringLiteral("-show_streams"),
                   QStringLiteral("-show_chapters"),
                   filePath});

    if (!process.waitForStarted(5000)) {
        if (errorOut)
            *errorOut = QStringLiteral("无法启动 ffprobe：%1").arg(process.errorString());
        return {};
    }
    // 大文件 + 网络盘可能较慢，给足 30 秒
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished(2000);
        if (errorOut)
            *errorOut = QStringLiteral("解析超时（30 秒）。文件可能位于不可访问的位置，或已损坏。");
        return {};
    }

    const QByteArray out = process.readAllStandardOutput();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0 || out.isEmpty()) {
        QString stderrText = QString::fromUtf8(process.readAllStandardError()).trimmed();
        const QStringList lines = stderrText.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        if (lines.size() > 3)
            stderrText = lines.mid(lines.size() - 3).join(QStringLiteral(" "));
        if (errorOut)
            *errorOut = stderrText.isEmpty()
                ? QStringLiteral("ffprobe 未能读取该文件（可能不是有效的媒体文件）。")
                : stderrText;
        return {};
    }
    return out;
}

MediaInfo MediaProbe::parseJson(const QByteArray &json, const QString &filePath)
{
    MediaInfo info;
    info.filePath = filePath;

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        info.errorMessage = QStringLiteral("ffprobe 输出解析失败：%1").arg(parseError.errorString());
        return info;
    }

    const QJsonObject root = document.object();
    const QJsonObject format = root.value(QStringLiteral("format")).toObject();

    info.formatName = format.value(QStringLiteral("format_name")).toString();
    info.formatLongName = format.value(QStringLiteral("format_long_name")).toString();
    info.durationSeconds = toDouble(format.value(QStringLiteral("duration")));
    info.bitrate = toInt64(format.value(QStringLiteral("bit_rate")));
    info.fileSize = toInt64(format.value(QStringLiteral("size")));

    const QFileInfo fileInfo(filePath);
    if (info.fileSize <= 0 && fileInfo.exists())
        info.fileSize = fileInfo.size();

    const QJsonArray streams = root.value(QStringLiteral("streams")).toArray();
    for (const QJsonValue &entry : streams) {
        const QJsonObject stream = entry.toObject();
        const QString type = stream.value(QStringLiteral("codec_type")).toString();

        if (type == QLatin1String("video")) {
            VideoStreamInfo video;
            video.index = stream.value(QStringLiteral("index")).toInt(-1);
            video.codecName = stream.value(QStringLiteral("codec_name")).toString();
            video.codecLongName = stream.value(QStringLiteral("codec_long_name")).toString();
            video.profile = stream.value(QStringLiteral("profile")).toString();
            video.pixelFormat = stream.value(QStringLiteral("pix_fmt")).toString();
            video.colorSpace = stream.value(QStringLiteral("color_space")).toString();
            video.colorTransfer = stream.value(QStringLiteral("color_transfer")).toString();
            video.colorPrimaries = stream.value(QStringLiteral("color_primaries")).toString();
            video.width = stream.value(QStringLiteral("width")).toInt();
            video.height = stream.value(QStringLiteral("height")).toInt();
            video.bitrate = toInt64(stream.value(QStringLiteral("bit_rate")));
            video.durationSeconds = toDouble(stream.value(QStringLiteral("duration")));
            video.rotationDegrees = streamRotation(stream);
            video.language = streamLanguage(stream);
            video.title = streamTitle(stream);
            video.isDefault = streamIsDefault(stream);

            double fps = parseRational(stream.value(QStringLiteral("avg_frame_rate")).toString());
            if (fps <= 0.0)
                fps = parseRational(stream.value(QStringLiteral("r_frame_rate")).toString());
            video.fps = fps;

            // 位深：优先 bits_per_raw_sample，其次从 pix_fmt 推断（yuv420p10le -> 10）
            const QString bitsText = stream.value(QStringLiteral("bits_per_raw_sample")).toString();
            video.bitDepth = bitsText.toInt();
            if (video.bitDepth <= 0) {
                const QRegularExpression re(QStringLiteral("p(\\d{1,2})(le|be)$"));
                const QRegularExpressionMatch match = re.match(video.pixelFormat);
                video.bitDepth = match.hasMatch() ? match.captured(1).toInt() : 8;
            }

            info.videos.append(video);
        } else if (type == QLatin1String("audio")) {
            AudioStreamInfo audio;
            audio.index = stream.value(QStringLiteral("index")).toInt(-1);
            audio.codecName = stream.value(QStringLiteral("codec_name")).toString();
            audio.codecLongName = stream.value(QStringLiteral("codec_long_name")).toString();
            audio.profile = stream.value(QStringLiteral("profile")).toString();
            audio.sampleFormat = stream.value(QStringLiteral("sample_fmt")).toString();
            audio.channelLayout = stream.value(QStringLiteral("channel_layout")).toString();
            audio.sampleRate = stream.value(QStringLiteral("sample_rate")).toString().toInt();
            audio.channels = stream.value(QStringLiteral("channels")).toInt();
            audio.bitrate = toInt64(stream.value(QStringLiteral("bit_rate")));
            audio.language = streamLanguage(stream);
            audio.title = streamTitle(stream);
            audio.isDefault = streamIsDefault(stream);
            info.audios.append(audio);
        } else if (type == QLatin1String("subtitle")) {
            SubtitleStreamInfo subtitle;
            subtitle.index = stream.value(QStringLiteral("index")).toInt(-1);
            subtitle.codecName = stream.value(QStringLiteral("codec_name")).toString();
            subtitle.language = streamLanguage(stream);
            subtitle.title = streamTitle(stream);
            subtitle.isDefault = streamIsDefault(stream);
            subtitle.isForced = stream.value(QStringLiteral("disposition")).toObject()
                                    .value(QStringLiteral("forced")).toInt(0) == 1;
            info.subtitles.append(subtitle);
        }
    }

    info.chapterCount = root.value(QStringLiteral("chapters")).toArray().size();

    if (info.durationSeconds <= 0.0) {
        // 某些流式容器只在流里带 duration
        if (const VideoStreamInfo *v = info.primaryVideo())
            info.durationSeconds = v->durationSeconds;
    }

    if (info.videos.isEmpty() && info.audios.isEmpty()) {
        info.errorMessage = QStringLiteral("该文件中没有找到音视频流，无法转换。");
        return info;
    }

    info.ok = true;
    return info;
}

MediaInfo MediaProbe::probeSync(const QString &ffprobePath, const QString &filePath)
{
    MediaInfo info;
    info.filePath = filePath;

    const QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        info.errorMessage = QStringLiteral("文件不存在：%1").arg(filePath);
        return info;
    }

    QString error;
    const QByteArray json = runFfprobe(ffprobePath, filePath, &error);
    if (json.isEmpty()) {
        info.fileSize = fileInfo.size();
        info.errorMessage = error.isEmpty() ? QStringLiteral("读取失败") : error;
        return info;
    }
    return parseJson(json, filePath);
}

void MediaProbe::probeAsync(const QString &filePath)
{
    const QString ffprobePath = m_locator ? m_locator->tools().ffprobePath : QString();

    auto *watcher = new QFutureWatcher<MediaInfo>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, filePath]() {
        const MediaInfo info = watcher->result();
        watcher->deleteLater();
        if (info.ok)
            emit probed(info);
        else
            emit failed(filePath, info.errorMessage.isEmpty() ? QStringLiteral("未知错误") : info.errorMessage);
    });
    watcher->setFuture(QtConcurrent::run([ffprobePath, filePath]() {
        return probeSync(ffprobePath, filePath);
    }));
}

} // namespace vf
