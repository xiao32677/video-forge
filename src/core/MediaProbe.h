#pragma once

// =============================================================================
//  VideoForge - 媒体信息探测（ffprobe）
//  逻辑与 UI 解耦：parseJson() 是纯函数，可脱离 ffprobe 做单元测试。
// =============================================================================

#include "core/MediaInfo.h"

#include <QByteArray>
#include <QFutureWatcher>
#include <QObject>
#include <QString>

namespace vf {

class FfmpegLocator;

class MediaProbe : public QObject
{
    Q_OBJECT

public:
    explicit MediaProbe(FfmpegLocator *locator, QObject *parent = nullptr);

    /// 后台线程探测，完成后通过 probed()/failed() 通知（UI 线程）
    void probeAsync(const QString &filePath);

    /// 同步探测（测试与命令行走这条路径）
    static MediaInfo probeSync(const QString &ffprobePath, const QString &filePath);

    /// 解析 ffprobe -print_format json 的输出（纯函数，单元测试入口）
    static MediaInfo parseJson(const QByteArray &json, const QString &filePath);

    /// 触发一次 raw ffprobe 调用并返回原始 JSON（诊断用）
    static QByteArray runFfprobe(const QString &ffprobePath, const QString &filePath, QString *errorOut);

signals:
    void probed(const vf::MediaInfo &info);
    void failed(const QString &filePath, const QString &errorMessage);

private:
    FfmpegLocator *m_locator = nullptr;
};

} // namespace vf
