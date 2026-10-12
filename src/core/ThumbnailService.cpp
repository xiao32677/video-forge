#include "core/ThumbnailService.h"
#include "core/FfmpegLocator.h"
#include "core/Format.h"
#include "core/ProcessUtil.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

#include <cmath>

namespace vf {

namespace {

constexpr int kThumbCacheVersion = 2;

QByteArray buildCacheKeyMaterial(const QString &filePath, double timeSeconds, const QSize &size)
{
    const QFileInfo info(filePath);
    return QStringLiteral("v%1|%2|%3|%4|%5x%6")
        .arg(kThumbCacheVersion)
        .arg(QDir::toNativeSeparators(info.absoluteFilePath()))
        .arg(info.lastModified().toMSecsSinceEpoch())
        .arg(info.size())
        .arg(QString::number(timeSeconds, 'f', 3))
        .arg(size.width())
        .arg(size.height())
        .toUtf8();
}

} // namespace

ThumbnailService::ThumbnailService(FfmpegLocator *locator, QObject *parent)
    : QObject(parent)
    , m_locator(locator)
{
}

QString ThumbnailService::cacheDirectory()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString dir = (base.isEmpty()
                             ? QDir::tempPath() + QStringLiteral("/VideoForge")
                             : base) + QStringLiteral("/thumbnails");
    QDir().mkpath(dir);
    return dir;
}

void ThumbnailService::clearCache()
{
    QDir dir(cacheDirectory());
    const QStringList files = dir.entryList({QStringLiteral("*.png")}, QDir::Files);
    for (const QString &file : files)
        dir.remove(file);
}

QString ThumbnailService::cacheKeyFor(const QString &filePath, double timeSeconds, const QSize &size) const
{
    const QByteArray material = buildCacheKeyMaterial(filePath, timeSeconds, size);
    return QString::fromLatin1(QCryptographicHash::hash(material, QCryptographicHash::Sha1).toHex());
}

QImage ThumbnailService::loadFromCache(const QString &key)
{
    const QString path = cacheDirectory() + QLatin1Char('/') + key + QStringLiteral(".png");
    if (!QFile::exists(path))
        return {};
    QImage image;
    if (!image.load(path))
        return {};
    return image;
}

void ThumbnailService::saveToCache(const QString &key, const QImage &image)
{
    if (image.isNull())
        return;
    const QString path = cacheDirectory() + QLatin1Char('/') + key + QStringLiteral(".png");
    image.save(path, "PNG");
}

QImage ThumbnailService::extractFrame(const QString &ffmpegPath, const QString &filePath,
                                     double timeSeconds, const QSize &size, QString *errorOut)
{
    if (ffmpegPath.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("ffmpeg 不可用");
        return {};
    }
    if (!QFile::exists(filePath)) {
        if (errorOut)
            *errorOut = QStringLiteral("文件不存在");
        return {};
    }

    if (timeSeconds < 0)
        timeSeconds = 0;

    // -ss 放在 -i 之前：利用关键帧快速定位，速度提升一个数量级
    QStringList args{
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-ss"), fmt::ffmpegTimestamp(timeSeconds),
        QStringLiteral("-i"), filePath,
        QStringLiteral("-frames:v"), QStringLiteral("1"),
        QStringLiteral("-an"), QStringLiteral("-sn"),
    };
    if (size.isValid() && size.width() > 0)
        args << QStringLiteral("-vf") << QStringLiteral("scale=%1:-2:flags=bicubic").arg(size.width());
    args << QStringLiteral("-f") << QStringLiteral("image2pipe")
         << QStringLiteral("-vcodec") << QStringLiteral("png")
         << QStringLiteral("pipe:1");

    QProcess process;
    suppressConsoleWindow(process);
    process.start(ffmpegPath, args);
    if (!process.waitForStarted(5000)) {
        if (errorOut)
            *errorOut = QStringLiteral("无法启动 ffmpeg：%1").arg(process.errorString());
        return {};
    }
    if (!process.waitForFinished(20000)) {
        process.kill();
        process.waitForFinished(2000);
        if (errorOut)
            *errorOut = QStringLiteral("抽帧超时");
        return {};
    }

    const QByteArray data = process.readAllStandardOutput();
    QImage image;
    if (!data.isEmpty())
        image = QImage::fromData(data, "PNG");

    if (image.isNull()) {
        if (errorOut) {
            QString stderrText = QString::fromUtf8(process.readAllStandardError()).trimmed();
            if (stderrText.size() > 300)
                stderrText = stderrText.right(300);
            *errorOut = stderrText.isEmpty() ? QStringLiteral("该时间点没有可用画面") : stderrText;
        }
        return {};
    }
    return image;
}

double ThumbnailService::imageInterest(const QImage &image)
{
    if (image.isNull())
        return 0.0;
    const QImage gray = image.convertToFormat(QImage::Format_Grayscale8);
    const int width = gray.width();
    const int height = gray.height();
    if (width <= 0 || height <= 0)
        return 0.0;

    // 每 2 行/列采样一次，速度足够且稳定
    const int stepY = std::max(1, height / 96);
    const int stepX = std::max(1, width / 96);
    double sum = 0.0;
    double sumSq = 0.0;
    qint64 count = 0;
    for (int y = 0; y < height; y += stepY) {
        const uchar *line = gray.constScanLine(y);
        for (int x = 0; x < width; x += stepX) {
            const double v = line[x];
            sum += v;
            sumSq += v * v;
            ++count;
        }
    }
    if (count == 0)
        return 0.0;
    const double mean = sum / count;
    const double variance = std::max(0.0, sumSq / count - mean * mean);
    return std::sqrt(variance);
}

double ThumbnailService::pickCoverTimestamp(const QString &ffmpegPath, const QString &filePath,
                                            double durationSeconds, const QSize &probeSize)
{
    if (durationSeconds <= 0.0)
        durationSeconds = 1.0;

    // 候选时间点：避开最开头的黑场与广告位，也避免正好踩在片尾名单上
    const double ratios[] = {0.18, 0.35, 0.08, 0.55, 0.75};
    double bestTime = durationSeconds * ratios[0];
    double bestScore = -1.0;

    for (double ratio : ratios) {
        const double t = std::max(0.0, durationSeconds * ratio);
        if (t >= durationSeconds)
            continue;
        QString error;
        const QImage frame = extractFrame(ffmpegPath, filePath, t, probeSize, &error);
        if (frame.isNull())
            continue;
        const double score = imageInterest(frame);
        if (score > bestScore) {
            bestScore = score;
            bestTime = t;
        }
        // 足够有内容就不用继续试了，节省时间
        if (score > 26.0)
            break;
    }
    return bestTime;
}

void ThumbnailService::invalidate()
{
    m_generation.fetch_add(1);
}

void ThumbnailService::requestCover(const QString &filePath, double durationSeconds, const QSize &size)
{
    const quint64 generation = m_generation.load();
    const QString ffmpegPath = m_locator ? m_locator->tools().ffmpegPath : QString();

    // 先用 18% 处作为"快速封面"：命中缓存就立即出图，体验最好
    const double quickTime = durationSeconds > 0 ? durationSeconds * 0.18 : 1.0;
    const QString quickKey = cacheKeyFor(filePath, quickTime, size);
    const QImage cached = loadFromCache(quickKey);
    if (!cached.isNull()) {
        emit coverReady(filePath, cached, true);
        return;
    }

    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher, filePath, generation]() {
        const QImage image = watcher->result();
        watcher->deleteLater();
        if (generation != m_generation.load())
            return; // 已被更新的请求取代
        if (image.isNull())
            emit thumbnailFailed(filePath, QStringLiteral("无法生成缩略图"));
        else
            emit coverReady(filePath, image, false);
    });

    watcher->setFuture(QtConcurrent::run([this, ffmpegPath, filePath, durationSeconds, size, generation]() {
        const double t = pickCoverTimestamp(ffmpegPath, filePath, durationSeconds);
        QString error;
        QImage image = extractFrame(ffmpegPath, filePath, t, size, &error);

        // 兜底：如果智能挑帧失败，尝试直接取第 1 秒
        if (image.isNull() && durationSeconds > 1.0)
            image = extractFrame(ffmpegPath, filePath, 1.0, size, &error);

        if (!image.isNull() && generation == m_generation.load())
            saveToCache(cacheKeyFor(filePath, t, size), image);
        return image;
    }));
}

void ThumbnailService::requestFilmstrip(const QString &filePath, double durationSeconds, int frames,
                                        const QSize &frameSize)
{
    if (frames <= 0)
        return;
    const quint64 generation = m_generation.load();
    const QString ffmpegPath = m_locator ? m_locator->tools().ffmpegPath : QString();

    auto *watcher = new QFutureWatcher<QList<QImage>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this,
            [this, watcher, filePath, durationSeconds, generation]() {
                const QList<QImage> images = watcher->result();
                watcher->deleteLater();
                if (generation != m_generation.load())
                    return;
                if (images.isEmpty())
                    emit thumbnailFailed(filePath, QStringLiteral("无法生成胶片条"));
                else
                    emit filmstripReady(filePath, images, durationSeconds);
            });

    watcher->setFuture(QtConcurrent::run([this, ffmpegPath, filePath, durationSeconds, frames, frameSize, generation]() {
        QList<QImage> result;
        const double duration = durationSeconds > 0 ? durationSeconds : static_cast<double>(frames);
        for (int i = 0; i < frames; ++i) {
            if (generation != m_generation.load())
                return QList<QImage>();
            // 均匀分布，且两侧各留 2% 余量避免抓到纯黑首尾帧
            const double t = duration * (0.02 + 0.96 * (static_cast<double>(i) + 0.5) / frames);
            const QString key = cacheKeyFor(filePath, t, frameSize);
            QImage image = loadFromCache(key);
            if (image.isNull()) {
                image = extractFrame(ffmpegPath, filePath, t, frameSize);
                if (!image.isNull())
                    saveToCache(key, image);
            }
            result.append(image); // 允许个别帧为空（占位），保证索引与时间轴对齐
        }
        return result;
    }));
}

} // namespace vf
