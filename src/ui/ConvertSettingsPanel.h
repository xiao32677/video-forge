#pragma once

// =============================================================================
//  VideoForge - 右侧转换设置面板
//
//  设计原则（"给人用"）：
//    * 默认值就是大多数人的最佳选择，不改任何东西也能一键转换
//    * 每个选项都有中文说明和悬浮提示，术语都翻译成人话
//    * 实时显示预估输出体积，避免"转完才发现 4 个 G"
//    * 高级参数收在"高级"分组里，不干扰普通用户
// =============================================================================

#include "core/CommandBuilder.h"
#include "core/ConvertPreset.h"
#include "core/HwAccelDetector.h"
#include "core/MediaInfo.h"

#include <QList>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSlider;
class QSpinBox;

namespace vf {

class SectionCard;

class ConvertSettingsPanel : public QWidget
{
    Q_OBJECT

public:
    explicit ConvertSettingsPanel(QWidget *parent = nullptr);

    /// 选中了新媒体文件：刷新可用性、裁剪上限与体积预估
    void setMediaInfo(const MediaInfo &info);
    void clearMedia();

    ConvertPreset currentPreset() const;
    ConvertOptions currentOptions() const;

    QString outputDirectory() const;
    void setOutputDirectory(const QString &directory);

    /// 硬件加速探测结果（用于勾选项的可用性与提示）
    void setHardwareCapabilities(const QList<HardwareCapability> &capabilities);
    HardwareAccel selectedAccel() const;

    /// 预留接口：外部（预览面板的"设为起点/终点"）写入裁剪时间
    void setTrimStart(double seconds);
    void setTrimEnd(double seconds);

    /// 按 id 切换预设（自动化测试 / 记忆上次选择时使用）
    bool setPresetById(const QString &presetId);
    QString currentPresetId() const;

    /// 批量转换进行中：禁用"开始"，启用"停止"
    void setBusy(bool busy);

signals:
    void startRequested();
    void stopRequested();

private:
    // 这两个不是槽也可以：全部连接都用新式函数指针语法
    void onPresetChanged();
    void refreshEstimate();

    SectionCard *buildOutputGroup();
    SectionCard *buildVideoGroup();
    SectionCard *buildAudioGroup();
    SectionCard *buildTrimGroup();
    SectionCard *buildAdvancedGroup();
    void updateEnabledStates();

    MediaInfo m_info;
    ConvertPreset m_preset;
    QList<HardwareCapability> m_capabilities;

    QComboBox *m_presetCombo = nullptr;
    QLabel *m_presetHint = nullptr;
    QLineEdit *m_outputDirEdit = nullptr;
    QCheckBox *m_sameAsSourceCheck = nullptr;
    QLabel *m_outputPreview = nullptr;

    QSlider *m_qualitySlider = nullptr;
    QSpinBox *m_qualitySpin = nullptr;
    QLabel *m_qualityHint = nullptr;
    QComboBox *m_speedCombo = nullptr;
    QComboBox *m_resolutionCombo = nullptr;
    QSpinBox *m_customWidth = nullptr;
    QSpinBox *m_customHeight = nullptr;
    QComboBox *m_fpsCombo = nullptr;
    QCheckBox *m_copyVideoCheck = nullptr;

    QComboBox *m_audioBitrateCombo = nullptr;
    QComboBox *m_channelsCombo = nullptr;
    QCheckBox *m_copyAudioCheck = nullptr;
    QCheckBox *m_removeAudioCheck = nullptr;

    SectionCard *m_trimGroup = nullptr;
    QCheckBox *m_trimCheck = nullptr;
    QDoubleSpinBox *m_trimStartSpin = nullptr;
    QDoubleSpinBox *m_trimEndSpin = nullptr;

    SectionCard *m_advancedGroup = nullptr;
    QCheckBox *m_hardwareCheck = nullptr;
    QLabel *m_hardwareHint = nullptr;
    QCheckBox *m_fastStartCheck = nullptr;
    QCheckBox *m_stripMetadataCheck = nullptr;
    QLineEdit *m_extraArgsEdit = nullptr;

    QLabel *m_estimateLabel = nullptr;
    QLabel *m_estimateMetric = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_stopButton = nullptr;
};

} // namespace vf
