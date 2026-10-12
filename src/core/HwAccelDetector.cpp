#include "core/HwAccelDetector.h"
#include "core/ProcessUtil.h"

#include <QProcess>
#include <QRegularExpression>
#include <QSet>

namespace vf {

HwAccelDetector::HwAccelDetector(QObject *parent)
    : QObject(parent)
{
}

QStringList HwAccelDetector::parseEncoders(const QString &encoderListText)
{
    // 形如： " V....D h264_nvenc           NVIDIA NVENC H.264 encoder (codec h264)"
    QStringList result;
    const QStringList lines = encoderListText.split(QLatin1Char('\n'));
    const QRegularExpression re(QStringLiteral("^\\s*[A-Z.]{6}\\s+(\\S+)\\s"));
    for (const QString &line : lines) {
        const QRegularExpressionMatch match = re.match(line);
        if (match.hasMatch())
            result << match.captured(1);
    }
    return result;
}

QList<HardwareCapability> HwAccelDetector::buildCapabilities(const QStringList &encoders)
{
    const QSet<QString> set(encoders.begin(), encoders.end());

    QList<HardwareCapability> list;

    HardwareCapability nvenc;
    nvenc.accel = HardwareAccel::Nvenc;
    nvenc.name = QStringLiteral("NVIDIA NVENC");
    for (const char *name : {"h264_nvenc", "hevc_nvenc", "av1_nvenc"}) {
        const QString encoder = QString::fromLatin1(name);
        if (set.contains(encoder))
            nvenc.encoders << encoder;
    }
    nvenc.available = !nvenc.encoders.isEmpty();
    nvenc.note = nvenc.available
        ? QStringLiteral("适合有 NVIDIA 独显的机器，转换速度可提升 3~10 倍")
        : QStringLiteral("未在当前 ffmpeg 构建中找到 NVENC 编码器");
    list << nvenc;

    HardwareCapability qsv;
    qsv.accel = HardwareAccel::Qsv;
    qsv.name = QStringLiteral("Intel Quick Sync");
    for (const char *name : {"h264_qsv", "hevc_qsv", "av1_qsv", "vp9_qsv"}) {
        const QString encoder = QString::fromLatin1(name);
        if (set.contains(encoder))
            qsv.encoders << encoder;
    }
    qsv.available = !qsv.encoders.isEmpty();
    qsv.note = qsv.available
        ? QStringLiteral("核显即可加速，笔记本省电且安静")
        : QStringLiteral("未在当前 ffmpeg 构建中找到 QSV 编码器");
    list << qsv;

    HardwareCapability amf;
    amf.accel = HardwareAccel::Amf;
    amf.name = QStringLiteral("AMD AMF");
    for (const char *name : {"h264_amf", "hevc_amf", "av1_amf"}) {
        const QString encoder = QString::fromLatin1(name);
        if (set.contains(encoder))
            amf.encoders << encoder;
    }
    amf.available = !amf.encoders.isEmpty();
    amf.note = amf.available
        ? QStringLiteral("适合 Radeon 显卡 / Ryzen 核显")
        : QStringLiteral("未在当前 ffmpeg 构建中找到 AMF 编码器");
    list << amf;

    return list;
}

QList<HardwareCapability> HwAccelDetector::detect(const QString &ffmpegPath)
{
    m_capabilities.clear();
    if (ffmpegPath.isEmpty()) {
        emit detected();
        return m_capabilities;
    }

    QProcess process;
    suppressConsoleWindow(process);
    process.start(ffmpegPath, {QStringLiteral("-hide_banner"), QStringLiteral("-encoders")});
    if (!process.waitForStarted(5000) || !process.waitForFinished(10000)) {
        process.kill();
        process.waitForFinished(1000);
        emit detected();
        return m_capabilities;
    }

    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    m_capabilities = buildCapabilities(parseEncoders(output));
    emit detected();
    return m_capabilities;
}

HardwareAccel HwAccelDetector::bestAccel() const
{
    for (const HardwareCapability &capability : m_capabilities) {
        if (capability.available)
            return capability.accel;
    }
    return HardwareAccel::None;
}

} // namespace vf
