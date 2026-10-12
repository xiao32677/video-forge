#include "ui/ConvertSettingsPanel.h"
#include "core/Format.h"
#include "core/SizeEstimator.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/UiWidgets.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

namespace vf {

namespace {

QLabel *hintLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setProperty("role", "hint");
    label->setWordWrap(true);
    return label;
}

/// 下拉框的宽度策略：默认 QComboBox 会按"最长的那一项"要宽度，
/// 在窄侧栏里这会把整个面板撑宽导致右侧被裁掉。
/// 改成"按最少 14 个字符"计算，超出部分自动省略（完整内容在 tooltip / 下方说明里）。
void tuneCombo(QComboBox *combo, int minimumCharacters = 14)
{
    combo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    combo->setMinimumContentsLength(minimumCharacters);
}

} // namespace

ConvertSettingsPanel::ConvertSettingsPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 10, 10, 10);
    outer->setSpacing(8);

    auto *titleRow = new QHBoxLayout;
    titleRow->setSpacing(7);
    titleRow->addWidget(new GlyphLabel(icons::Glyph::Sliders, 16, QColor(), this));
    auto *title = new QLabel(QStringLiteral("转换设置"), this);
    title->setProperty("role", "panelTitle");
    titleRow->addWidget(title);
    titleRow->addStretch();
    outer->addLayout(titleRow);

    // 内容较多，放进滚动区，小窗口下也不会挤爆
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    // 只允许纵向滚动：内容宽度始终跟随面板宽度，避免出现横向滚动条
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto *content = new QWidget(scroll);
    auto *contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 6, 0);
    contentLayout->setSpacing(10);
    contentLayout->addWidget(buildOutputGroup());
    contentLayout->addWidget(buildVideoGroup());
    contentLayout->addWidget(buildAudioGroup());
    contentLayout->addWidget(buildTrimGroup());
    contentLayout->addWidget(buildAdvancedGroup());
    contentLayout->addStretch();

    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    // ------------------------------------------------------------ 预估 + 按钮
    // "预计输出多少"是普通用户最关心的一句话，值得单独做成一张信息卡
    auto *estimateCard = new QFrame(this);
    estimateCard->setProperty("role", "card");
    auto *estimateLayout = new QHBoxLayout(estimateCard);
    estimateLayout->setContentsMargins(12, 10, 12, 10);
    estimateLayout->setSpacing(11);
    estimateLayout->addWidget(new GlyphLabel(icons::Glyph::Gauge, 22, QColor(), estimateCard), 0,
                              Qt::AlignTop);

    auto *estimateColumn = new QVBoxLayout;
    estimateColumn->setContentsMargins(0, 0, 0, 0);
    estimateColumn->setSpacing(2);
    m_estimateMetric = new QLabel(QStringLiteral("等待选择文件"), estimateCard);
    m_estimateMetric->setProperty("role", "metric");
    m_estimateLabel = new QLabel(QStringLiteral("导入文件后会自动估算输出体积"), estimateCard);
    m_estimateLabel->setProperty("role", "hint");
    m_estimateLabel->setWordWrap(true);
    estimateColumn->addWidget(m_estimateMetric);
    estimateColumn->addWidget(m_estimateLabel);
    estimateLayout->addLayout(estimateColumn, 1);
    outer->addWidget(estimateCard);

    m_startButton = new QPushButton(QStringLiteral("开始转换"), this);
    m_startButton->setProperty("accent", true);
    m_startButton->setIcon(icons::icon(icons::Glyph::PlayCircle,
                                       QColor(QStringLiteral("#FFFFFF")), 17));
    m_startButton->setMinimumHeight(42);
    m_startButton->setCursor(Qt::PointingHandCursor);
    m_startButton->setToolTip(QStringLiteral("把媒体库里的所有文件按当前设置加入转换队列（F5）"));
    connect(m_startButton, &QPushButton::clicked, this, &ConvertSettingsPanel::startRequested);
    outer->addWidget(m_startButton);

    m_stopButton = new QPushButton(QStringLiteral("取消全部任务"), this);
    m_stopButton->setProperty("danger", true);
    m_stopButton->setIcon(icons::icon(icons::Glyph::Stop, theme::dangerColor(), 14));
    m_stopButton->setCursor(Qt::PointingHandCursor);
    m_stopButton->setEnabled(false);
    connect(m_stopButton, &QPushButton::clicked, this, &ConvertSettingsPanel::stopRequested);
    outer->addWidget(m_stopButton);

    onPresetChanged();
}

SectionCard *ConvertSettingsPanel::buildOutputGroup()
{
    auto *group = new SectionCard(QStringLiteral("输出格式"), icons::Glyph::Convert, this);
    auto *layout = group->body();
    layout->setSpacing(7);

    m_presetCombo = new QComboBox(group);
    QString currentGroup;
    for (const ConvertPreset &preset : builtinPresets()) {
        if (preset.group != currentGroup) {
            if (!currentGroup.isEmpty())
                m_presetCombo->insertSeparator(m_presetCombo->count());
            currentGroup = preset.group;
        }
        m_presetCombo->addItem(QStringLiteral("[%1] %2").arg(preset.group, preset.displayName), preset.id);
    }
    m_presetCombo->setCurrentIndex(m_presetCombo->findData(defaultPresetId()));
    tuneCombo(m_presetCombo, 18);
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConvertSettingsPanel::onPresetChanged);
    layout->addWidget(m_presetCombo);

    m_presetHint = hintLabel(QString(), group);
    layout->addWidget(m_presetHint);

    m_sameAsSourceCheck = new QCheckBox(QStringLiteral("输出到源文件目录"), group);
    m_sameAsSourceCheck->setChecked(true);
    m_sameAsSourceCheck->setToolTip(QStringLiteral("勾选时输出文件与源文件放在同一个目录；\n取消勾选可指定统一的输出目录"));
    layout->addWidget(m_sameAsSourceCheck);

    auto *dirRow = new QHBoxLayout;
    dirRow->setSpacing(6);
    m_outputDirEdit = new QLineEdit(group);
    m_outputDirEdit->setPlaceholderText(QStringLiteral("默认与源文件同目录"));
    m_outputDirEdit->setEnabled(false);
    dirRow->addWidget(m_outputDirEdit, 1);

    auto *browseButton = new QPushButton(QStringLiteral("浏览…"), group);
    browseButton->setEnabled(false);
    dirRow->addWidget(browseButton);
    connect(browseButton, &QPushButton::clicked, this, [this]() {
        const QString directory = QFileDialog::getExistingDirectory(
            this, QStringLiteral("选择输出目录"), m_outputDirEdit->text());
        if (!directory.isEmpty())
            m_outputDirEdit->setText(QDir::toNativeSeparators(directory));
    });
    connect(m_sameAsSourceCheck, &QCheckBox::toggled, this, [this, browseButton](bool sameAsSource) {
        m_outputDirEdit->setEnabled(!sameAsSource);
        browseButton->setEnabled(!sameAsSource);
    });

    layout->addLayout(dirRow);
    m_outputPreview = hintLabel(QString(), group);
    // 路径不做换行，只做中间省略。
    // 注意：不换行的 QLabel 其 minimumSizeHint 就等于文本宽度，会把侧栏永久撑宽，
    // 所以这里必须把宽度方向的 sizeHint 直接置为 Ignored，让它能被布局压缩。
    m_outputPreview->setWordWrap(false);
    m_outputPreview->setMinimumWidth(0);
    m_outputPreview->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(m_outputPreview);
    return group;
}

SectionCard *ConvertSettingsPanel::buildVideoGroup()
{
    auto *group = new SectionCard(QStringLiteral("画质与尺寸"), icons::Glyph::Sliders, this);
    auto *layout = new QFormLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
    layout->setSpacing(7);

    // ---- 质量 ----
    auto *qualityRow = new QHBoxLayout;
    m_qualitySlider = new QSlider(Qt::Horizontal, group);
    m_qualitySlider->setRange(0, 51);
    m_qualitySlider->setValue(23);
    m_qualitySlider->setToolTip(QStringLiteral("CRF 值：越小越清晰、文件越大。23 是通用最佳平衡点"));
    m_qualitySpin = new QSpinBox(group);
    m_qualitySpin->setRange(0, 51);
    m_qualitySpin->setValue(23);
    m_qualitySpin->setFixedWidth(58);
    connect(m_qualitySlider, &QSlider::valueChanged, m_qualitySpin, &QSpinBox::setValue);
    connect(m_qualitySpin, QOverload<int>::of(&QSpinBox::valueChanged), m_qualitySlider, &QSlider::setValue);
    connect(m_qualitySpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ConvertSettingsPanel::refreshEstimate);
    qualityRow->addWidget(m_qualitySlider, 1);
    qualityRow->addWidget(m_qualitySpin);
    layout->addRow(QStringLiteral("质量"), qualityRow);

    m_qualityHint = hintLabel(QStringLiteral("数值越小越清晰，文件也越大。18=高画质，23=推荐，28=体积优先"), group);
    layout->addRow(QString(), m_qualityHint);

    // ---- 编码速度 ----
    m_speedCombo = new QComboBox(group);
    const QList<QPair<QString, QString>> speeds{
        {QStringLiteral("ultrafast"), QStringLiteral("极快（体积最大）")},
        {QStringLiteral("veryfast"), QStringLiteral("很快")},
        {QStringLiteral("fast"), QStringLiteral("快")},
        {QStringLiteral("medium"), QStringLiteral("均衡（推荐）")},
        {QStringLiteral("slow"), QStringLiteral("慢（更省体积）")},
        {QStringLiteral("veryslow"), QStringLiteral("极慢（体积最小）")},
    };
    for (const auto &entry : speeds)
        m_speedCombo->addItem(entry.second, entry.first);
    m_speedCombo->setCurrentIndex(3);
    m_speedCombo->setToolTip(QStringLiteral("编码耗时与体积的权衡，不确定就用“均衡”"));
    tuneCombo(m_speedCombo);
    connect(m_speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConvertSettingsPanel::refreshEstimate);
    layout->addRow(QStringLiteral("编码速度"), m_speedCombo);

    // ---- 分辨率 ----
    auto *resolutionRow = new QHBoxLayout;
    m_resolutionCombo = new QComboBox(group);
    m_resolutionCombo->addItem(QStringLiteral("保持原始分辨率"), 0);
    m_resolutionCombo->addItem(QStringLiteral("4K · 2160p"), 2160);
    m_resolutionCombo->addItem(QStringLiteral("2K · 1440p"), 1440);
    m_resolutionCombo->addItem(QStringLiteral("全高清 · 1080p"), 1080);
    m_resolutionCombo->addItem(QStringLiteral("高清 · 720p"), 720);
    m_resolutionCombo->addItem(QStringLiteral("标清 · 480p"), 480);
    m_resolutionCombo->addItem(QStringLiteral("小屏 · 360p"), 360);
    m_resolutionCombo->addItem(QStringLiteral("自定义…"), -1);
    auto *customLabel = new QLabel(QStringLiteral("宽"), group);
    m_customWidth = new QSpinBox(group);
    m_customWidth->setRange(16, 7680);
    m_customWidth->setValue(1280);
    m_customWidth->setSingleStep(2);
    m_customWidth->setPrefix(QStringLiteral("宽 "));
    m_customHeight = new QSpinBox(group);
    m_customHeight->setRange(16, 4320);
    m_customHeight->setValue(720);
    m_customHeight->setSingleStep(2);
    m_customHeight->setPrefix(QStringLiteral("高 "));
    m_customWidth->setVisible(false);
    m_customHeight->setVisible(false);
    customLabel->setVisible(false);

    tuneCombo(m_resolutionCombo);
    resolutionRow->addWidget(m_resolutionCombo, 1);
    resolutionRow->addWidget(customLabel);
    resolutionRow->addWidget(m_customWidth);
    resolutionRow->addWidget(m_customHeight);
    layout->addRow(QStringLiteral("分辨率"), resolutionRow);

    connect(m_resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, customLabel](int) {
                const int data = m_resolutionCombo->currentData().toInt();
                const bool custom = (data == -1);
                customLabel->setVisible(custom);
                m_customWidth->setVisible(custom);
                m_customHeight->setVisible(custom);
                refreshEstimate();
            });
    connect(m_customHeight, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ConvertSettingsPanel::refreshEstimate);

    // ---- 帧率 ----
    m_fpsCombo = new QComboBox(group);
    m_fpsCombo->addItem(QStringLiteral("保持原始帧率"), 0.0);
    m_fpsCombo->addItem(QStringLiteral("60 fps"), 60.0);
    m_fpsCombo->addItem(QStringLiteral("50 fps"), 50.0);
    m_fpsCombo->addItem(QStringLiteral("30 fps"), 30.0);
    m_fpsCombo->addItem(QStringLiteral("25 fps"), 25.0);
    m_fpsCombo->addItem(QStringLiteral("24 fps（电影感）"), 24.0);
    m_fpsCombo->addItem(QStringLiteral("15 fps"), 15.0);
    tuneCombo(m_fpsCombo);
    connect(m_fpsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConvertSettingsPanel::refreshEstimate);
    layout->addRow(QStringLiteral("帧率"), m_fpsCombo);

    m_copyVideoCheck = new QCheckBox(QStringLiteral("直接复制视频流"), group);
    m_copyVideoCheck->setToolTip(
        QStringLiteral("不重新编码，只换容器：几秒完成、画质零损失。\n"
                       "仅在换封装格式时可用；勾选后上面的画质/分辨率设置不再生效"));
    layout->addRow(QString(), m_copyVideoCheck);

    group->body()->addLayout(layout);
    return group;
}

SectionCard *ConvertSettingsPanel::buildAudioGroup()
{
    auto *group = new SectionCard(QStringLiteral("音频"), icons::Glyph::Volume, this);
    auto *layout = new QFormLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(7);

    m_audioBitrateCombo = new QComboBox(group);
    const QList<int> bitrates{64, 96, 128, 160, 192, 256, 320};
    for (int bitrate : bitrates)
        m_audioBitrateCombo->addItem(QStringLiteral("%1 kbps%2")
                                         .arg(bitrate)
                                         .arg(bitrate == 192 ? QStringLiteral("（推荐）") : QString()),
                                     bitrate);
    m_audioBitrateCombo->setCurrentIndex(bitrates.indexOf(192));
    tuneCombo(m_audioBitrateCombo);
    connect(m_audioBitrateCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ConvertSettingsPanel::refreshEstimate);
    layout->addRow(QStringLiteral("音频码率"), m_audioBitrateCombo);

    m_channelsCombo = new QComboBox(group);
    m_channelsCombo->addItem(QStringLiteral("保持原始声道"), 0);
    m_channelsCombo->addItem(QStringLiteral("立体声（2 声道）"), 2);
    m_channelsCombo->addItem(QStringLiteral("单声道"), 1);
    tuneCombo(m_channelsCombo);
    layout->addRow(QStringLiteral("声道"), m_channelsCombo);

    m_copyAudioCheck = new QCheckBox(QStringLiteral("直接复制音频流"), group);
    m_copyAudioCheck->setToolTip(QStringLiteral("音频不重新编码，零损失、速度最快"));
    layout->addRow(QString(), m_copyAudioCheck);

    m_removeAudioCheck = new QCheckBox(QStringLiteral("移除音频"), group);
    m_removeAudioCheck->setToolTip(QStringLiteral("输出的视频没有声音"));
    layout->addRow(QString(), m_removeAudioCheck);

    group->body()->addLayout(layout);
    return group;
}

SectionCard *ConvertSettingsPanel::buildTrimGroup()
{
    auto *group = new SectionCard(QStringLiteral("裁剪（只转换片段）"), icons::Glyph::Scissors, this);
    m_trimGroup = group;
    auto *layout = group->body();
    layout->setSpacing(7);

    m_trimCheck = new QCheckBox(QStringLiteral("启用裁剪"), group);
    layout->addWidget(m_trimCheck);

    m_trimStartSpin = new QDoubleSpinBox(group);
    m_trimStartSpin->setRange(0.0, 100000.0);
    m_trimStartSpin->setDecimals(2);
    m_trimStartSpin->setSuffix(QStringLiteral(" 秒"));
    m_trimStartSpin->setMinimumWidth(90);
    m_trimEndSpin = new QDoubleSpinBox(group);
    m_trimEndSpin->setRange(0.0, 100000.0);
    m_trimEndSpin->setDecimals(2);
    m_trimEndSpin->setSuffix(QStringLiteral(" 秒"));
    m_trimEndSpin->setMinimumWidth(90);

    // 起止时间竖排：横排两个带单位的输入框会把侧栏顶宽
    auto *form = new QFormLayout;
    form->setSpacing(7);
    form->addRow(QStringLiteral("起点"), m_trimStartSpin);
    form->addRow(QStringLiteral("终点"), m_trimEndSpin);
    layout->addLayout(form);

    layout->addWidget(hintLabel(
        QStringLiteral("播放视频时点「设为起点 / 设为终点」可自动填入时间"), group));

    connect(m_trimCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_trimStartSpin->setEnabled(checked);
        m_trimEndSpin->setEnabled(checked);
        refreshEstimate();
    });
    connect(m_trimEndSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ConvertSettingsPanel::refreshEstimate);

    m_trimStartSpin->setEnabled(false);
    m_trimEndSpin->setEnabled(false);
    return group;
}

SectionCard *ConvertSettingsPanel::buildAdvancedGroup()
{
    auto *group = new SectionCard(QStringLiteral("高级（可选）"), icons::Glyph::Gear, this);
    m_advancedGroup = group;
    auto *layout = group->body();
    layout->setSpacing(7);

    m_hardwareCheck = new QCheckBox(QStringLiteral("使用显卡硬件加速"), group);
    m_hardwareCheck->setToolTip(QStringLiteral("需要 NVIDIA / Intel / AMD 显卡支持；\n"
                                               "编码更快，同体积下画质略低于软件编码。\n"
                                               "不可用时会自动回退到软件编码"));
    layout->addWidget(m_hardwareCheck);

    m_hardwareHint = hintLabel(QStringLiteral("正在检测硬件编码能力…"), group);
    layout->addWidget(m_hardwareHint);

    m_fastStartCheck = new QCheckBox(QStringLiteral("优化网络播放"), group);
    m_fastStartCheck->setChecked(true);
    m_fastStartCheck->setToolTip(QStringLiteral("把 MP4 的索引信息（moov）放到文件开头，\n"
                                                "让视频可以边下边播，几乎不影响转换速度"));
    layout->addWidget(m_fastStartCheck);

    m_stripMetadataCheck = new QCheckBox(QStringLiteral("清除元数据"), group);
    m_stripMetadataCheck->setToolTip(QStringLiteral("去掉拍摄设备、拍摄时间、GPS 等信息，保护隐私"));
    layout->addWidget(m_stripMetadataCheck);

    layout->addWidget(new QLabel(QStringLiteral("自定义 ffmpeg 参数"), group));
    m_extraArgsEdit = new QLineEdit(group);
    m_extraArgsEdit->setPlaceholderText(QStringLiteral("例如： -metadata title=\"我的视频\""));
    m_extraArgsEdit->setToolTip(QStringLiteral("高级用户使用；会被追加到命令行末尾（输出文件之前）"));
    layout->addWidget(m_extraArgsEdit);

    return group;
}

void ConvertSettingsPanel::setMediaInfo(const MediaInfo &info)
{
    m_info = info;
    if (info.ok && info.durationSeconds > 0) {
        m_trimStartSpin->setMaximum(info.durationSeconds);
        m_trimEndSpin->setMaximum(info.durationSeconds);
        if (m_trimEndSpin->value() <= 0.0 || m_trimEndSpin->value() > info.durationSeconds)
            m_trimEndSpin->setValue(info.durationSeconds);
    }
    refreshEstimate();
}

void ConvertSettingsPanel::clearMedia()
{
    m_info = MediaInfo();
    m_trimStartSpin->setValue(0.0);
    m_trimEndSpin->setValue(0.0);
    if (m_estimateMetric)
        m_estimateMetric->setText(QStringLiteral("等待选择文件"));
    if (m_estimateLabel)
        m_estimateLabel->setText(QStringLiteral("导入文件后会自动估算输出体积"));
}

void ConvertSettingsPanel::setOutputDirectory(const QString &directory)
{
    m_outputDirEdit->setText(directory);
}

QString ConvertSettingsPanel::outputDirectory() const
{
    return m_outputDirEdit->text().trimmed();
}

void ConvertSettingsPanel::setHardwareCapabilities(const QList<HardwareCapability> &capabilities)
{
    m_capabilities = capabilities;
    QStringList available;
    for (const HardwareCapability &capability : capabilities) {
        if (capability.available)
            available << capability.name;
    }
    if (available.isEmpty()) {
        m_hardwareCheck->setEnabled(false);
        m_hardwareCheck->setChecked(false);
        m_hardwareHint->setText(QStringLiteral("未检测到可用的硬件编码器，将使用 CPU 软件编码（兼容性最好）"));
    } else {
        m_hardwareCheck->setEnabled(true);
        m_hardwareHint->setText(QStringLiteral("检测到：%1。勾选后速度可提升数倍，画质略低于软件编码")
                                    .arg(available.join(QStringLiteral(" / "))));
    }
}

HardwareAccel ConvertSettingsPanel::selectedAccel() const
{
    for (const HardwareCapability &capability : m_capabilities) {
        if (capability.available)
            return capability.accel;
    }
    return HardwareAccel::None;
}

void ConvertSettingsPanel::setTrimStart(double seconds)
{
    m_trimCheck->setChecked(true);
    m_trimStartSpin->setValue(qMax(0.0, seconds));
    if (m_trimEndSpin->value() <= m_trimStartSpin->value() && m_info.durationSeconds > seconds)
        m_trimEndSpin->setValue(m_info.durationSeconds);
    refreshEstimate();
}

void ConvertSettingsPanel::setTrimEnd(double seconds)
{
    if (seconds <= 0.0)
        return;
    m_trimCheck->setChecked(true);
    m_trimEndSpin->setValue(seconds);
    if (m_trimStartSpin->value() >= m_trimEndSpin->value())
        m_trimStartSpin->setValue(0.0);
    refreshEstimate();
}

void ConvertSettingsPanel::setBusy(bool busy)
{
    m_startButton->setEnabled(!busy);
    m_startButton->setText(busy ? QStringLiteral("正在转换…") : QStringLiteral("开始转换"));
    m_stopButton->setEnabled(busy);
}

ConvertPreset ConvertSettingsPanel::currentPreset() const
{
    return presetById(m_presetCombo->currentData().toString());
}

QString ConvertSettingsPanel::currentPresetId() const
{
    return m_presetCombo->currentData().toString();
}

bool ConvertSettingsPanel::setPresetById(const QString &presetId)
{
    const int index = m_presetCombo->findData(presetId);
    if (index < 0)
        return false;
    m_presetCombo->setCurrentIndex(index);
    return true;
}

ConvertOptions ConvertSettingsPanel::currentOptions() const
{
    ConvertOptions options;
    options.copyVideo = m_copyVideoCheck->isChecked();
    options.copyAudio = m_copyAudioCheck->isChecked();
    options.removeAudio = m_removeAudioCheck->isChecked();
    options.crf = m_qualitySpin->value();
    options.speedPreset = m_speedCombo->currentData().toString();
    options.fps = m_fpsCombo->currentData().toDouble();

    const int resolution = m_resolutionCombo->currentData().toInt();
    if (resolution == -1) {
        options.scaleWidth = m_customWidth->value();
        options.scaleHeight = m_customHeight->value();
    } else if (resolution > 0) {
        options.scaleHeight = resolution;
    }

    options.audioBitrateKbps = m_audioBitrateCombo->currentData().toInt();
    options.audioChannels = m_channelsCombo->currentData().toInt();

    options.trimEnabled = m_trimCheck->isChecked();
    options.trimStart = m_trimStartSpin->value();
    options.trimEnd = m_trimEndSpin->value() > 0 ? m_trimEndSpin->value()
                                                : m_info.durationSeconds;

    options.hardwareAccel = m_hardwareCheck->isChecked() && m_hardwareCheck->isEnabled();
    options.accel = selectedAccel();
    options.fastStart = m_fastStartCheck->isChecked();
    options.stripMetadata = m_stripMetadataCheck->isChecked();
    options.extraArguments = m_extraArgsEdit->text();
    return options;
}

void ConvertSettingsPanel::onPresetChanged()
{
    m_preset = currentPreset();

    m_presetHint->setText(m_preset.description);
    // 下拉框会按宽度省略，把完整名称与说明挂到 tooltip 上，信息不丢
    m_presetCombo->setToolTip(QStringLiteral("%1\n%2")
                                  .arg(m_preset.displayName, m_preset.description));

    const bool audioOnly = m_preset.isAudioOnly();
    const bool isGif = m_preset.id == QLatin1String("gif");
    const bool noVideoEncode = isGif ? false : !m_preset.videoCopyable;

    // 视频相关控件
    const bool videoRelevant = !audioOnly;
    m_qualitySlider->setEnabled(videoRelevant && !isGif);
    m_qualitySpin->setEnabled(videoRelevant && !isGif);
    m_speedCombo->setEnabled(videoRelevant && !isGif);
    m_resolutionCombo->setEnabled(videoRelevant);
    m_fpsCombo->setEnabled(videoRelevant);
    m_copyVideoCheck->setEnabled(videoRelevant && m_preset.videoCopyable);
    if (!m_preset.videoCopyable)
        m_copyVideoCheck->setChecked(false);

    // 无损换壳预设：默认就打开"直接复制视频流"
    if (m_preset.id == QLatin1String("mkv-remux")) {
        m_copyVideoCheck->setChecked(true);
        m_copyAudioCheck->setChecked(true);
        if (m_preset.audioCodec.isEmpty()) {
            // 该预设没有音频编码器 -> 复制音频
        }
    } else {
        m_copyVideoCheck->setChecked(false);
    }
    Q_UNUSED(noVideoEncode);

    // 音频相关控件
    const bool audioCodecKnown = !m_preset.audioCodec.isEmpty();
    m_audioBitrateCombo->setEnabled(audioCodecKnown);
    m_copyAudioCheck->setEnabled(!audioOnly ? true : m_preset.audioCopyable);
    if (!m_preset.audioCopyable)
        m_copyAudioCheck->setChecked(false);
    m_removeAudioCheck->setEnabled(!audioOnly);

    // 质量滑块的合适默认值
    if (m_preset.defaultCrf > 0) {
        m_qualitySpin->setValue(m_preset.defaultCrf);
        m_qualitySlider->setValue(m_preset.defaultCrf);
    }
    if (!m_preset.defaultSpeedPreset.isEmpty()) {
        const int index = m_speedCombo->findData(m_preset.defaultSpeedPreset);
        if (index >= 0)
            m_speedCombo->setCurrentIndex(index);
    }
    if (m_preset.defaultAudioBitrateKbps > 0) {
        const int index = m_audioBitrateCombo->findData(m_preset.defaultAudioBitrateKbps);
        if (index >= 0)
            m_audioBitrateCombo->setCurrentIndex(index);
    }

    updateEnabledStates();
    refreshEstimate();
}

void ConvertSettingsPanel::updateEnabledStates()
{
    const bool audioOnly = m_preset.isAudioOnly();
    m_trimGroup->setVisible(true);
    Q_UNUSED(audioOnly);
}

void ConvertSettingsPanel::refreshEstimate()
{
    if (!m_info.ok) {
        if (m_estimateMetric)
            m_estimateMetric->setText(QStringLiteral("等待选择文件"));
        if (m_estimateLabel)
            m_estimateLabel->setText(QStringLiteral("导入文件后会自动估算输出体积"));
        return;
    }

    const ConvertOptions options = currentOptions();
    const SizeEstimate estimate = SizeEstimator::estimate(m_info, m_preset, options);

    if (m_estimateMetric) {
        m_estimateMetric->setText(estimate.bytes > 0
            ? QStringLiteral("预计输出 %1%2").arg(fmt::size(estimate.bytes),
                                                  estimate.reliable ? QString() : QStringLiteral("（估算）"))
            : QStringLiteral("无法预估体积"));
    }

    QStringList details;
    if (!estimate.comparison.isEmpty())
        details << estimate.comparison;
    if (!estimate.explanation.isEmpty())
        details << estimate.explanation;
    if (m_estimateLabel)
        m_estimateLabel->setText(details.join(QStringLiteral("　·　")));

    // 输出文件名预览，避免用户不知道文件会落在哪儿。
    // 长路径必须做省略，否则会把整个面板撑宽（中文/长路径场景下尤其明显）。
    const QString directory = m_sameAsSourceCheck->isChecked()
        ? QFileInfo(m_info.filePath).absolutePath()
        : (m_outputDirEdit->text().trimmed().isEmpty() ? QStringLiteral("（未选择输出目录）")
                                                       : m_outputDirEdit->text().trimmed());
    const QString nativeDirectory = QDir::toNativeSeparators(directory);
    // 用带上限的宽度预算做省略：布局早期 width() 还不可靠，不能直接拿它算
    const int budget = qBound(150, m_outputPreview->width() - 6, 300);
    m_outputPreview->setText(QFontMetrics(m_outputPreview->font())
                                 .elidedText(QStringLiteral("输出目录：%1").arg(nativeDirectory),
                                             Qt::ElideMiddle, budget));
    m_outputPreview->setToolTip(nativeDirectory);
}

} // namespace vf
