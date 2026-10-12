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

    struct Vendor {
        HardwareAccel accel;
        const char *name;
        const char *candidates[4];
        const char *okNote;
        const char *missingNote;
    };
    static const Vendor vendors[] = {
        {HardwareAccel::Nvenc, "NVIDIA NVENC",
         {"h264_nvenc", "hevc_nvenc", "av1_nvenc", nullptr},
         "适合有 NVIDIA 独显的机器，转换速度可提升 3~10 倍",
         "当前 ffmpeg 构建里没有 NVENC 编码器"},
        {HardwareAccel::Qsv, "Intel Quick Sync",
         {"h264_qsv", "hevc_qsv", "av1_qsv", "vp9_qsv"},
         "核显即可加速，笔记本上更省电、更安静",
         "当前 ffmpeg 构建里没有 QSV 编码器"},
        {HardwareAccel::Amf, "AMD AMF",
         {"h264_amf", "hevc_amf", "av1_amf", nullptr},
         "适合 Radeon 显卡 / Ryzen 核显",
         "当前 ffmpeg 构建里没有 AMF 编码器"},
    };

    QList<HardwareCapability> list;
    for (const Vendor &vendor : vendors) {
        HardwareCapability capability;
        capability.accel = vendor.accel;
        capability.name = QString::fromLatin1(vendor.name);
        for (int i = 0; i < 4 && vendor.candidates[i]; ++i) {
            const QString encoder = QString::fromLatin1(vendor.candidates[i]);
            if (set.contains(encoder))
                capability.encoders << encoder;
        }
        capability.present = !capability.encoders.isEmpty();
        // 注意：这里不设置 available —— 是否真的能用要交给 verifyEncoder() 试编一帧才算数
        capability.note = capability.present ? QString::fromUtf8(vendor.okNote)
                                             : QString::fromUtf8(vendor.missingNote);
        list << capability;
    }
    return list;
}

bool HwAccelDetector::verifyEncoder(const QString &ffmpegPath, const QString &encoder)
{
    if (ffmpegPath.isEmpty() || encoder.isEmpty())
        return false;

    // 用 lavfi 合成 8 帧纯色图，真的走一遍该硬件编码器。
    // 没有对应显卡/驱动时，ffmpeg 会以非 0 退出（例如 "Cannot load nvcuda.dll"）。
    QProcess process;
    suppressConsoleWindow(process);
    process.start(ffmpegPath, {
        QStringLiteral("-hide_banner"),
        QStringLiteral("-loglevel"), QStringLiteral("error"),
        QStringLiteral("-nostdin"),
        QStringLiteral("-f"), QStringLiteral("lavfi"),
        QStringLiteral("-i"), QStringLiteral("color=c=black:s=128x128:r=8:d=1"),
        QStringLiteral("-frames:v"), QStringLiteral("8"),
        QStringLiteral("-c:v"), encoder,
        QStringLiteral("-f"), QStringLiteral("null"),
        QStringLiteral("-"),
    });

    if (!process.waitForStarted(5000))
        return false;
    if (!process.waitForFinished(20000)) {
        process.kill();
        process.waitForFinished(2000);
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
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

    // 逐个真实试编码：只有真的能编出东西来，才敢对用户说"可用"
    for (HardwareCapability &capability : m_capabilities) {
        if (!capability.present)
            continue;
        bool verified = false;
        for (const QString &encoder : capability.encoders) {
            if (verifyEncoder(ffmpegPath, encoder)) {
                verified = true;
                break;
            }
        }
        capability.available = verified;
        if (!verified) {
            capability.note = QStringLiteral("编码器已编译进 ffmpeg，但本机试编码失败"
                                             "（通常是没装对应显卡或驱动），已按不可用处理");
        }
    }

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
