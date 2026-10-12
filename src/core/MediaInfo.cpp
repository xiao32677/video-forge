#include "core/MediaInfo.h"
#include "core/Format.h"

#include <QFileInfo>
#include <QStringList>

namespace vf {

QString MediaInfo::containerText() const
{
    if (!formatLongName.isEmpty())
        return formatLongName;
    // 短名往往是逗号分隔的候选列表，取第一个更有意义
    const QStringList parts = formatName.split(QLatin1Char(','), Qt::SkipEmptyParts);
    return parts.isEmpty() ? QStringLiteral("未知容器") : parts.first().toUpper();
}

QString MediaInfo::codecSummary() const
{
    QStringList parts;
    for (const VideoStreamInfo &v : videos)
        parts << (v.codecName.isEmpty() ? QStringLiteral("?") : v.codecName.toUpper());
    for (const AudioStreamInfo &a : audios)
        parts << (a.codecName.isEmpty() ? QStringLiteral("?") : a.codecName.toUpper());
    if (parts.isEmpty())
        return QStringLiteral("无音视频流");
    return parts.join(QStringLiteral(" + "));
}

QString MediaInfo::summaryLine() const
{
    QStringList parts;
    const VideoStreamInfo *v = primaryVideo();
    if (v)
        parts << QStringLiteral("%1x%2").arg(v->width).arg(v->height);
    if (v && v->fps > 0)
        parts << fmt::fps(v->fps);
    parts << fmt::duration(durationSeconds);
    if (fileSize > 0)
        parts << fmt::size(fileSize);
    return parts.join(QStringLiteral(" · "));
}

QString MediaInfo::baseName() const
{
    return QFileInfo(filePath).fileName();
}

} // namespace vf
