#pragma once

// =============================================================================
//  VideoForge - 缩略图服务
//
//  能力：
//    * 封面帧：智能挑帧（跳过纯黑/纯色片头），失败自动尝试多个时间点
//    * 胶片条：按时间轴均匀抽 N 帧，拼成一条时间轴预览
//    * 磁盘缓存：key = 路径 + 修改时间 + 文件大小 + 时间点 + 尺寸
//    * 全部在后台线程执行，UI 不卡顿；切换文件时旧任务自动作废
// =============================================================================

#include <QFutureWatcher>
#include <QImage>
#include <QList>
#include <QObject>
#include <QSize>
#include <QString>

#include <atomic>

namespace vf {

class FfmpegLocator;

class ThumbnailService : public QObject
{
    Q_OBJECT

public:
    explicit ThumbnailService(FfmpegLocator *locator, QObject *parent = nullptr);

    /// 请求封面帧（异步）。durationSeconds <= 0 时会退化为取第 1 秒。
    void requestCover(const QString &filePath, double durationSeconds, const QSize &size = QSize(480, 270));

    /// 请求胶片条（异步）。frames 数量建议 8~16。
    void requestFilmstrip(const QString &filePath, double durationSeconds, int frames = 12,
                          const QSize &frameSize = QSize(160, 90));

    /// 丢弃当前所有未完成任务的结果（用户已切换到别的文件）
    void invalidate();

    /// 同步抽帧（测试 / 命令行导出用）
    static QImage extractFrame(const QString &ffmpegPath, const QString &filePath,
                              double timeSeconds, const QSize &size, QString *errorOut = nullptr);

    /// 计算"最佳封面时间点"：在若干候选中挑选信息量最大的一帧
    static double pickCoverTimestamp(const QString &ffmpegPath, const QString &filePath,
                                     double durationSeconds, const QSize &probeSize = QSize(192, 108));

    /// 图片"信息量"评分（灰度标准差，越大越有内容，纯黑/纯色接近 0）
    static double imageInterest(const QImage &image);

    static QString cacheDirectory();
    static void clearCache();

signals:
    void coverReady(const QString &filePath, const QImage &image, bool fromCache);
    void filmstripReady(const QString &filePath, const QList<QImage> &frames, double durationSeconds);
    void thumbnailFailed(const QString &filePath, const QString &errorMessage);

private:
    QString cacheKeyFor(const QString &filePath, double timeSeconds, const QSize &size) const;
    QImage loadFromCache(const QString &key);
    void saveToCache(const QString &key, const QImage &image);

    FfmpegLocator *m_locator = nullptr;
    std::atomic<quint64> m_generation{0};
};

} // namespace vf
