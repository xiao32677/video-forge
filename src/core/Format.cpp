#include "core/Format.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>
#include <numeric>

namespace vf {
namespace fmt {

QString size(qint64 bytes)
{
    if (bytes < 0)
        return QStringLiteral("--");
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    if (unit == 0)
        return QStringLiteral("%1 B").arg(bytes);
    return QStringLiteral("%1 %2").arg(QString::number(value, 'f', value < 10 ? 2 : 1), QString::fromLatin1(units[unit]));
}

QString duration(double seconds)
{
    if (seconds < 0 || !std::isfinite(seconds))
        return QStringLiteral("--:--");
    const qint64 total = static_cast<qint64>(std::llround(seconds));
    const qint64 h = total / 3600;
    const qint64 m = (total % 3600) / 60;
    const qint64 s = total % 60;
    if (h > 0)
        return QStringLiteral("%1:%2:%3")
            .arg(h, 2, 10, QLatin1Char('0'))
            .arg(m, 2, 10, QLatin1Char('0'))
            .arg(s, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(m, 2, 10, QLatin1Char('0')).arg(s, 2, 10, QLatin1Char('0'));
}

QString durationLong(double seconds)
{
    if (seconds < 0 || !std::isfinite(seconds))
        return QStringLiteral("--");
    const qint64 total = static_cast<qint64>(std::llround(seconds));
    const qint64 h = total / 3600;
    const qint64 m = (total % 3600) / 60;
    const qint64 s = total % 60;
    QStringList parts;
    if (h > 0)
        parts << QStringLiteral("%1 小时").arg(h);
    if (m > 0)
        parts << QStringLiteral("%1 分").arg(m);
    parts << QStringLiteral("%1 秒").arg(s);
    return parts.join(QLatin1Char(' '));
}

QString bitrate(qint64 bitsPerSecond)
{
    if (bitsPerSecond <= 0)
        return QStringLiteral("--");
    if (bitsPerSecond >= 1000000)
        return QStringLiteral("%1 Mb/s").arg(QString::number(bitsPerSecond / 1000000.0, 'f', 2));
    return QStringLiteral("%1 kb/s").arg(QString::number(bitsPerSecond / 1000.0, 'f', 0));
}

QString fps(double value)
{
    if (value <= 0 || !std::isfinite(value))
        return QStringLiteral("--");
    const double rounded = std::round(value * 100.0) / 100.0;
    if (std::abs(rounded - std::round(rounded)) < 0.001)
        return QStringLiteral("%1 fps").arg(static_cast<int>(std::round(rounded)));
    return QStringLiteral("%1 fps").arg(QString::number(rounded, 'f', 2));
}

QString aspectRatio(int width, int height)
{
    if (width <= 0 || height <= 0)
        return QStringLiteral("--");
    const int g = std::gcd(width, height);
    int w = width / g;
    int h = height / g;
    // 常见比例的近似（例如 1918x1080 应显示 16:9）
    const double ratio = static_cast<double>(width) / height;
    struct Known { double value; int w; int h; };
    static const Known known[] = {
        {16.0 / 9.0, 16, 9}, {4.0 / 3.0, 4, 3}, {21.0 / 9.0, 21, 9},
        {1.85, 37, 20}, {2.35, 47, 20}, {1.0, 1, 1}, {9.0 / 16.0, 9, 16},
    };
    for (const Known &k : known) {
        if (std::abs(ratio - k.value) < 0.02)
            return QStringLiteral("%1:%2").arg(k.w).arg(k.h);
    }
    if (w > 40 || h > 40)
        return QStringLiteral("%1x%2").arg(width).arg(height);
    return QStringLiteral("%1:%2").arg(w).arg(h);
}

QString ffmpegTimestamp(double seconds)
{
    if (seconds < 0)
        seconds = 0;
    const qint64 ms = static_cast<qint64>(std::llround(seconds * 1000.0));
    const qint64 h = ms / 3600000;
    const qint64 m = (ms % 3600000) / 60000;
    const qint64 s = (ms % 60000) / 1000;
    const qint64 milli = ms % 1000;
    return QStringLiteral("%1:%2:%3.%4")
        .arg(h, 2, 10, QLatin1Char('0'))
        .arg(m, 2, 10, QLatin1Char('0'))
        .arg(s, 2, 10, QLatin1Char('0'))
        .arg(milli, 3, 10, QLatin1Char('0'));
}

double parseTimestamp(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty())
        return -1.0;
    const QStringList parts = trimmed.split(QLatin1Char(':'));
    if (parts.size() > 3)
        return -1.0;
    double total = 0.0;
    for (const QString &part : parts) {
        bool ok = false;
        const double value = part.trimmed().toDouble(&ok);
        if (!ok || value < 0)
            return -1.0;
        total = total * 60.0 + value;
    }
    return total;
}

QString speed(double value)
{
    if (value <= 0 || !std::isfinite(value))
        return QStringLiteral("--");
    return QStringLiteral("%1x").arg(QString::number(value, 'f', 2));
}

QStringList supportedMediaExtensions()
{
    static const QStringList extensions = {
        // 视频
        QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("mov"), QStringLiteral("avi"),
        QStringLiteral("webm"), QStringLiteral("flv"), QStringLiteral("wmv"), QStringLiteral("m4v"),
        QStringLiteral("mpg"), QStringLiteral("mpeg"), QStringLiteral("ts"), QStringLiteral("m2ts"),
        QStringLiteral("mts"), QStringLiteral("3gp"), QStringLiteral("3g2"), QStringLiteral("ogv"),
        QStringLiteral("rmvb"), QStringLiteral("rm"), QStringLiteral("vob"), QStringLiteral("asf"),
        QStringLiteral("mxf"), QStringLiteral("dv"), QStringLiteral("f4v"), QStringLiteral("m2v"),
        // 音频
        QStringLiteral("mp3"), QStringLiteral("wav"), QStringLiteral("flac"), QStringLiteral("aac"),
        QStringLiteral("m4a"), QStringLiteral("ogg"), QStringLiteral("oga"), QStringLiteral("wma"),
        QStringLiteral("opus"), QStringLiteral("aiff"), QStringLiteral("aif"), QStringLiteral("ape"),
        QStringLiteral("mka"), QStringLiteral("amr"), QStringLiteral("ac3"), QStringLiteral("dts"),
        // 动图
        QStringLiteral("gif"),
    };
    return extensions;
}

bool isSupportedMediaFile(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists() || !info.isFile())
        return false;
    return supportedMediaExtensions().contains(info.suffix().toLower());
}

QString mediaFileDialogFilter()
{
    QStringList patterns;
    const QStringList extensions = supportedMediaExtensions();
    for (const QString &extension : extensions)
        patterns << QStringLiteral("*.") + extension;
    return QStringLiteral("媒体文件 (%1);;视频 (*.mp4 *.mkv *.mov *.avi *.webm *.flv *.wmv *.m4v *.ts *.mpg *.mpeg *.3gp *.rmvb *.vob);;"
                          "音频 (*.mp3 *.wav *.flac *.aac *.m4a *.ogg *.wma *.opus *.ape *.mka);;"
                          "所有文件 (*.*)")
        .arg(patterns.join(QLatin1Char(' ')));
}

QString saveFileDialogFilter(const QString &container, const QString &extension)
{
    QString label = container.toUpper();
    if (label == QLatin1String("MATROSKA"))
        label = QStringLiteral("MKV");
    else if (label == QLatin1String("IPOD"))
        label = QStringLiteral("M4A");
    return QStringLiteral("%1 文件 (*.%2);;所有文件 (*.*)").arg(label, extension.toLower());
}

QString number(double value, int decimals)
{
    QString text = QString::number(value, 'f', decimals);
    if (text.contains(QLatin1Char('.'))) {
        while (text.endsWith(QLatin1Char('0')))
            text.chop(1);
        if (text.endsWith(QLatin1Char('.')))
            text.chop(1);
    }
    return text;
}

} // namespace fmt
} // namespace vf
