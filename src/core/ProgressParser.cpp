#include "core/ProgressParser.h"

#include <QStringList>

namespace vf {

qint64 ProgressParser::parseBitrate(const QString &text)
{
    QString value = text.trimmed();
    if (value.isEmpty() || value.startsWith(QLatin1String("N/A")))
        return 0;

    double multiplier = 1.0;
    if (value.contains(QLatin1String("kbit"))) {
        multiplier = 1000.0;
        value.remove(QLatin1String("kbits/s"));
        value.remove(QLatin1String("kbit/s"));
    } else if (value.contains(QLatin1String("Mbit"))) {
        multiplier = 1000000.0;
        value.remove(QLatin1String("Mbits/s"));
        value.remove(QLatin1String("Mbit/s"));
    } else if (value.contains(QLatin1String("bits/s"))) {
        value.remove(QLatin1String("bits/s"));
    }

    bool ok = false;
    const double number = value.trimmed().toDouble(&ok);
    if (!ok)
        return 0;
    return static_cast<qint64>(number * multiplier);
}

bool ProgressParser::parseLine(const QString &line, ProgressInfo *info)
{
    if (!info)
        return false;
    const int separator = line.indexOf(QLatin1Char('='));
    if (separator <= 0)
        return false;

    const QString key = line.left(separator).trimmed();
    const QString value = line.mid(separator + 1).trimmed();

    if (key == QLatin1String("frame")) {
        bool ok = false;
        const int frame = value.toInt(&ok);
        if (ok) {
            info->frame = frame;
            info->valid = true;
            return true;
        }
    } else if (key == QLatin1String("fps")) {
        bool ok = false;
        const double fps = value.toDouble(&ok);
        if (ok) {
            info->fps = fps;
            info->valid = true;
            return true;
        }
    } else if (key == QLatin1String("bitrate")) {
        info->bitrate = parseBitrate(value);
        return true;
    } else if (key == QLatin1String("total_size")) {
        bool ok = false;
        const qint64 size = value.toLongLong(&ok);
        if (ok) {
            info->totalSize = size;
            info->valid = true;
            return true;
        }
    } else if (key == QLatin1String("out_time_us") || key == QLatin1String("out_time_ms")) {
        // 注意：ffmpeg 的 out_time_ms 实际单位也是微秒（历史遗留命名）
        bool ok = false;
        const qint64 time = value.toLongLong(&ok);
        if (ok && time >= 0) {
            info->outTimeUs = time;
            info->valid = true;
            return true;
        }
    } else if (key == QLatin1String("speed")) {
        QString speedText = value;
        speedText.remove(QLatin1Char('x'));
        bool ok = false;
        const double speed = speedText.trimmed().toDouble(&ok);
        if (ok) {
            info->speed = speed;
            info->valid = true;
            return true;
        }
    } else if (key == QLatin1String("progress")) {
        info->finished = (value == QLatin1String("end"));
        info->valid = true;
        return true;
    }
    return false;
}

bool ProgressParser::feedLine(const QString &line)
{
    if (line.isEmpty())
        return false;
    return parseLine(line, &m_info);
}

bool ProgressParser::feed(const QString &chunk)
{
    m_buffer += chunk;
    bool updated = false;

    int newlineIndex = m_buffer.indexOf(QLatin1Char('\n'));
    while (newlineIndex >= 0) {
        QString line = m_buffer.left(newlineIndex);
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        m_buffer.remove(0, newlineIndex + 1);
        if (parseLine(line, &m_info))
            updated = true;
        newlineIndex = m_buffer.indexOf(QLatin1Char('\n'));
    }

    // 防止异常输出把缓冲撑爆
    if (m_buffer.size() > 65536)
        m_buffer.clear();
    return updated;
}

} // namespace vf
