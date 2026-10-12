#pragma once

// =============================================================================
//  VideoForge - 硬件编码器探测
//
//  思路：读取 `ffmpeg -encoders` 列表，判断当前这台机器上编译进去的硬件编码器，
//  再结合显卡厂商做一个便宜的可用性判断。探测失败永远回退软件编码，
//  绝不让用户因为"勾了硬件加速"而转换失败。
// =============================================================================

#include "core/ConvertPreset.h"

#include <QList>
#include <QObject>
#include <QStringList>

namespace vf {

struct HardwareCapability
{
    HardwareAccel accel = HardwareAccel::None;
    QString name;                 ///< 展示名
    QStringList encoders;         ///< 可用的 ffmpeg 编码器名，如 h264_nvenc
    bool available = false;
    QString note;                 ///< 说明，例如"检测到 NVIDIA 显卡"
};

class HwAccelDetector : public QObject
{
    Q_OBJECT

public:
    explicit HwAccelDetector(QObject *parent = nullptr);

    /// 探测（同步，通常 < 300ms；建议在后台线程调用）
    QList<HardwareCapability> detect(const QString &ffmpegPath);

    /// 返回第一个可用的加速方案；都不可用时返回 None
    HardwareAccel bestAccel() const;

    /// 上一次探测到的全部能力
    const QList<HardwareCapability> &capabilities() const { return m_capabilities; }

    /// 从 `ffmpeg -encoders` 文本中提取可用编码器（纯函数，可单测）
    static QStringList parseEncoders(const QString &encoderListText);

    /// 依据编码器列表判断各厂商能力
    static QList<HardwareCapability> buildCapabilities(const QStringList &encoders);

signals:
    void detected();

private:
    QList<HardwareCapability> m_capabilities;
};

} // namespace vf
