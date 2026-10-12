#include "ui/PreviewPanel.h"
#include "core/Format.h"
#include "ui/AppIcon.h"
#include "ui/FilmstripWidget.h"
#include "ui/Theme.h"

#if VF_ENABLE_PLAYER
#  include <QAudioOutput>
#  include <QMediaPlayer>
#  include <QVideoWidget>
#endif

#include <QComboBox>
#include <QFileInfo>
#include <QGridLayout>
#include <QHash>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace vf {

namespace {

/// 编码器名 -> 更友好的中文展示
QString friendlyCodec(const QString &codec)
{
    static const QHash<QString, QString> table{
        {QStringLiteral("h264"), QStringLiteral("H.264 / AVC")},
        {QStringLiteral("hevc"), QStringLiteral("H.265 / HEVC")},
        {QStringLiteral("mpeg4"), QStringLiteral("MPEG-4 Part 2")},
        {QStringLiteral("mpeg2video"), QStringLiteral("MPEG-2")},
        {QStringLiteral("vp8"), QStringLiteral("VP8")},
        {QStringLiteral("vp9"), QStringLiteral("VP9")},
        {QStringLiteral("av1"), QStringLiteral("AV1")},
        {QStringLiteral("prores"), QStringLiteral("Apple ProRes")},
        {QStringLiteral("dnxhd"), QStringLiteral("Avid DNxHD")},
        {QStringLiteral("mjpeg"), QStringLiteral("Motion JPEG")},
        {QStringLiteral("aac"), QStringLiteral("AAC")},
        {QStringLiteral("mp3"), QStringLiteral("MP3")},
        {QStringLiteral("ac3"), QStringLiteral("AC-3 / 杜比")},
        {QStringLiteral("eac3"), QStringLiteral("E-AC-3")},
        {QStringLiteral("flac"), QStringLiteral("FLAC 无损")},
        {QStringLiteral("opus"), QStringLiteral("Opus")},
        {QStringLiteral("vorbis"), QStringLiteral("Vorbis")},
        {QStringLiteral("pcm_s16le"), QStringLiteral("PCM 16bit")},
        {QStringLiteral("pcm_s24le"), QStringLiteral("PCM 24bit")},
        {QStringLiteral("subrip"), QStringLiteral("SubRip 字幕")},
        {QStringLiteral("ass"), QStringLiteral("ASS 字幕")},
        {QStringLiteral("mov_text"), QStringLiteral("MP4 内嵌字幕")},
    };
    const QString key = codec.toLower();
    if (table.contains(key))
        return QStringLiteral("%1 (%2)").arg(table.value(key), codec);
    return codec.isEmpty() ? QStringLiteral("未知") : codec;
}

QString channelText(int channels)
{
    switch (channels) {
    case 1: return QStringLiteral("单声道");
    case 2: return QStringLiteral("立体声");
    case 3: return QStringLiteral("2.1 声道");
    case 6: return QStringLiteral("5.1 环绕");
    case 8: return QStringLiteral("7.1 环绕");
    default: return channels > 0 ? QStringLiteral("%1 声道").arg(channels) : QStringLiteral("未知");
    }
}

QString speedText(const QString &preset)
{
    static const QHash<QString, QString> table{
        {QStringLiteral("ultrafast"), QStringLiteral("极快（体积最大）")},
        {QStringLiteral("superfast"), QStringLiteral("很快")},
        {QStringLiteral("veryfast"), QStringLiteral("快")},
        {QStringLiteral("faster"), QStringLiteral("较快")},
        {QStringLiteral("fast"), QStringLiteral("偏快")},
        {QStringLiteral("medium"), QStringLiteral("均衡（推荐）")},
        {QStringLiteral("slow"), QStringLiteral("慢（更省体积）")},
        {QStringLiteral("slower"), QStringLiteral("更慢")},
        {QStringLiteral("veryslow"), QStringLiteral("极慢（体积最小）")},
    };
    return table.value(preset, preset);
}

void fillTable(QTableWidget *table, const QList<QPair<QString, QString>> &rows)
{
    table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        auto *keyItem = new QTableWidgetItem(rows.at(i).first);
        keyItem->setForeground(theme::mutedText());
        table->setItem(i, 0, keyItem);
        table->setItem(i, 1, new QTableWidgetItem(rows.at(i).second));
    }
    table->resizeColumnToContents(0);
    table->horizontalHeader()->setStretchLastSection(true);
}

QTableWidget *makeInfoTable(QWidget *parent)
{
    auto *table = new QTableWidget(parent);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({QStringLiteral("项目"), QStringLiteral("值")});
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setShowGrid(false);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setHighlightSections(false);
    return table;
}

} // namespace

PreviewPanel::PreviewPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 10, 8, 10);
    layout->setSpacing(8);

    // ------------------------------------------------------------ 播放画面
    auto *videoCard = new QWidget(this);
    auto *videoLayout = new QVBoxLayout(videoCard);
    videoLayout->setContentsMargins(0, 0, 0, 0);
    videoLayout->setSpacing(6);

    m_stack = new QStackedWidget(videoCard);
    m_stack->setMinimumHeight(220);

    m_coverLabel = new QLabel(m_stack);
    m_coverLabel->setAlignment(Qt::AlignCenter);
    m_coverLabel->setText(QStringLiteral("从左侧拖入或添加一个视频文件"));
    m_coverLabel->setStyleSheet(QStringLiteral("background: #000; color: #8B939C; border-radius: 10px;"));
    m_stack->addWidget(m_coverLabel);

#if VF_ENABLE_PLAYER
    m_videoWidget = new QVideoWidget(m_stack);
    m_videoWidget->setMinimumHeight(200);
    m_stack->addWidget(m_videoWidget);
#else
    m_stack->addWidget(new QLabel(QStringLiteral("（本构建未启用内嵌播放器）"), m_stack));
#endif

    m_hintLabel = new QLabel(videoCard);
    m_hintLabel->setProperty("role", "hint");
    m_hintLabel->setAlignment(Qt::AlignCenter);
    m_hintLabel->setVisible(false);

    videoLayout->addWidget(m_stack, 1);
    videoLayout->addWidget(m_hintLabel);

    layout->addWidget(videoCard, 5);

    // ------------------------------------------------------------ 播放控制
    buildPlayerArea(this);

    // ------------------------------------------------------------ 胶片条
    m_filmstrip = new FilmstripWidget(this);
    connect(m_filmstrip, &FilmstripWidget::frameClicked, this, [this](double seconds) {
#if VF_ENABLE_PLAYER
        if (m_player && m_player->source().isValid()) {
            m_player->setPosition(static_cast<qint64>(seconds * 1000.0));
            showVideoPage();
            return;
        }
#endif
        setPosition(seconds);
    });
    layout->addWidget(m_filmstrip);

    // ------------------------------------------------------------ 信息表
    buildInfoArea(this);
}

PreviewPanel::~PreviewPanel() = default;

void PreviewPanel::buildPlayerArea(QWidget *container)
{
    auto *layout = qobject_cast<QVBoxLayout *>(container->layout());
    if (!layout)
        return;

    auto *row = new QHBoxLayout;
    row->setSpacing(8);

    m_playButton = new QPushButton(QStringLiteral("▶  播放"), container);
    m_playButton->setFixedWidth(88);
    m_playButton->setCursor(Qt::PointingHandCursor);
    connect(m_playButton, &QPushButton::clicked, this, &PreviewPanel::onPlayPauseClicked);
    row->addWidget(m_playButton);

    m_stopButton = new QPushButton(QStringLiteral("■"), container);
    m_stopButton->setFixedWidth(34);
    m_stopButton->setToolTip(QStringLiteral("停止播放"));
    m_stopButton->setCursor(Qt::PointingHandCursor);
    connect(m_stopButton, &QPushButton::clicked, this, &PreviewPanel::onStopClicked);
    row->addWidget(m_stopButton);

    m_positionSlider = new QSlider(Qt::Horizontal, container);
    m_positionSlider->setRange(0, 1000);
    m_positionSlider->setToolTip(QStringLiteral("拖动可快速定位"));
    connect(m_positionSlider, &QSlider::sliderPressed, this, [this]() { m_userSeeking = true; });
    connect(m_positionSlider, &QSlider::sliderReleased, this, [this]() { m_userSeeking = false; });
    connect(m_positionSlider, &QSlider::valueChanged, this, &PreviewPanel::onSliderMoved);
    row->addWidget(m_positionSlider, 1);

    m_timeLabel = new QLabel(QStringLiteral("--:-- / --:--"), container);
    m_timeLabel->setMinimumWidth(104);
    m_timeLabel->setAlignment(Qt::AlignCenter);
    m_timeLabel->setProperty("role", "hint");
    row->addWidget(m_timeLabel);

    m_trimStartButton = new QPushButton(QStringLiteral("设为起点"), container);
    m_trimStartButton->setToolTip(QStringLiteral("把当前播放位置设为裁剪起点"));
    m_trimStartButton->setCursor(Qt::PointingHandCursor);
    connect(m_trimStartButton, &QPushButton::clicked, this,
            [this]() { emit trimStartRequested(m_positionSeconds); });
    row->addWidget(m_trimStartButton);

    m_trimEndButton = new QPushButton(QStringLiteral("设为终点"), container);
    m_trimEndButton->setToolTip(QStringLiteral("把当前播放位置设为裁剪终点"));
    m_trimEndButton->setCursor(Qt::PointingHandCursor);
    connect(m_trimEndButton, &QPushButton::clicked, this,
            [this]() { emit trimEndRequested(m_positionSeconds); });
    row->addWidget(m_trimEndButton);

    layout->addLayout(row);

    auto *secondRow = new QHBoxLayout;
    secondRow->setSpacing(8);

    auto *volumeLabel = new QLabel(QStringLiteral("🔊"), container);
    secondRow->addWidget(volumeLabel);
    m_volumeSlider = new QSlider(Qt::Horizontal, container);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(70);
    m_volumeSlider->setFixedWidth(90);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &PreviewPanel::onVolumeChanged);
    secondRow->addWidget(m_volumeSlider);

    secondRow->addSpacing(12);
    secondRow->addWidget(new QLabel(QStringLiteral("倍速"), container));
    m_speedCombo = new QComboBox(container);
    m_speedCombo->addItem(QStringLiteral("0.5x"), 0.5);
    m_speedCombo->addItem(QStringLiteral("1.0x"), 1.0);
    m_speedCombo->addItem(QStringLiteral("1.5x"), 1.5);
    m_speedCombo->addItem(QStringLiteral("2.0x"), 2.0);
    m_speedCombo->setCurrentIndex(1);
    m_speedCombo->setFixedWidth(74);
    secondRow->addWidget(m_speedCombo);

    secondRow->addStretch();
    auto *roleHint = new QLabel(
        QStringLiteral("提示：胶片条可直接点击跳转，滚轮可微调时间"), container);
    roleHint->setProperty("role", "hint");
    secondRow->addWidget(roleHint);

    layout->addLayout(secondRow);

#if VF_ENABLE_PLAYER
    m_player = new QMediaPlayer(this);
    m_audioOutput = new QAudioOutput(this);
    m_player->setAudioOutput(m_audioOutput);
    m_player->setVideoOutput(m_videoWidget);
    m_audioOutput->setVolume(0.7);

    connect(m_player, &QMediaPlayer::durationChanged, this, &PreviewPanel::onDurationChanged);
    connect(m_player, &QMediaPlayer::positionChanged, this, &PreviewPanel::onPositionChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) {
            m_playButton->setText(QStringLiteral("▶  播放"));
            m_player->setPosition(0);
            m_positionSlider->setValue(0);
        }
    });
    connect(m_speedCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        if (m_player)
            m_player->setPlaybackRate(m_speedCombo->currentData().toDouble());
    });
#else
    setPlayerEnabled(false);
#endif
}

void PreviewPanel::buildInfoArea(QWidget *container)
{
    auto *layout = qobject_cast<QVBoxLayout *>(container->layout());
    if (!layout)
        return;

    m_infoTabs = new QTabWidget(container);
    m_overviewTable = makeInfoTable(m_infoTabs);
    m_videoTable = makeInfoTable(m_infoTabs);
    m_audioTable = makeInfoTable(m_infoTabs);
    m_extraTable = makeInfoTable(m_infoTabs);

    m_infoTabs->addTab(m_overviewTable, QStringLiteral("概览"));
    m_infoTabs->addTab(m_videoTable, QStringLiteral("视频流"));
    m_infoTabs->addTab(m_audioTable, QStringLiteral("音频流"));
    m_infoTabs->addTab(m_extraTable, QStringLiteral("字幕 / 章节"));
    m_infoTabs->setMinimumHeight(200);

    layout->addWidget(m_infoTabs, 3);
}

void PreviewPanel::setPlayerEnabled(bool enabled)
{
    m_playButton->setEnabled(enabled);
    m_stopButton->setEnabled(enabled);
    m_positionSlider->setEnabled(enabled);
    m_volumeSlider->setEnabled(enabled);
    m_speedCombo->setEnabled(enabled);
    if (!enabled)
        m_hintLabel->setVisible(!m_hintLabel->text().isEmpty());
}

void PreviewPanel::clearMedia()
{
    m_info = MediaInfo();
    m_cover = QImage();
    m_durationSeconds = 0.0;
    m_positionSeconds = 0.0;
    m_coverLabel->setPixmap(QPixmap());
    m_coverLabel->setText(QStringLiteral("从左侧拖入或添加一个视频文件"));
    m_filmstrip->clearFrames();
    fillTables();
    showCoverPage();
    setPlayerEnabled(false);
    m_timeLabel->setText(QStringLiteral("--:-- / --:--"));
    m_positionSlider->setValue(0);
#if VF_ENABLE_PLAYER
    if (m_player)
        m_player->setSource(QUrl());
#endif
}

void PreviewPanel::showMedia(const MediaInfo &info)
{
    m_info = info;
    m_durationSeconds = info.durationSeconds;
    m_positionSeconds = 0.0;
    m_cover = QImage();

    m_coverLabel->setPixmap(QPixmap());
    m_coverLabel->setText(QStringLiteral("正在生成缩略图…"));
    m_filmstrip->clearFrames();
    fillTables();
    showCoverPage();
    setPlayerEnabled(true);
    m_positionSlider->setValue(0);
    updateTimeLabels();

#if VF_ENABLE_PLAYER
    if (m_player) {
        m_player->setSource(QUrl::fromLocalFile(info.filePath));
        m_player->setPlaybackRate(m_speedCombo->currentData().toDouble());
        m_player->pause();
        m_playButton->setText(QStringLiteral("▶  播放"));
    }
#endif
    setHint(QString());
}

void PreviewPanel::setCoverImage(const QImage &image)
{
    m_cover = image;
    if (image.isNull())
        return;

    const QSize target = m_stack->size().isEmpty() ? QSize(720, 405) : m_stack->size();
    m_coverLabel->setPixmap(QPixmap::fromImage(
        image.scaled(target * 0.92, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    if (m_stack->currentIndex() == 0)
        showCoverPage();
}

void PreviewPanel::setFilmstrip(const QList<QImage> &frames, double durationSeconds)
{
    m_filmstrip->setFrames(frames, durationSeconds > 0 ? durationSeconds : m_durationSeconds);
}

void PreviewPanel::setHint(const QString &text)
{
    m_hintLabel->setText(text);
    m_hintLabel->setVisible(!text.isEmpty());
}

void PreviewPanel::showCoverPage()
{
    m_stack->setCurrentIndex(0);
}

void PreviewPanel::showVideoPage()
{
#if VF_ENABLE_PLAYER
    if (m_videoWidget)
        m_stack->setCurrentIndex(1);
#endif
}

void PreviewPanel::onPlayPauseClicked()
{
#if VF_ENABLE_PLAYER
    if (!m_player || !m_player->source().isValid()) {
        setHint(QStringLiteral("请先在左侧选择一个文件"));
        return;
    }
    if (m_player->playbackState() == QMediaPlayer::PlayingState) {
        m_player->pause();
        m_playButton->setText(QStringLiteral("▶  播放"));
    } else {
        showVideoPage();
        m_player->play();
        m_playButton->setText(QStringLiteral("⏸  暂停"));
    }
#endif
}

void PreviewPanel::onStopClicked()
{
#if VF_ENABLE_PLAYER
    if (!m_player)
        return;
    m_player->stop();
    m_playButton->setText(QStringLiteral("▶  播放"));
    m_positionSlider->setValue(0);
    m_positionSeconds = 0.0;
    m_filmstrip->setPosition(0.0);
    updateTimeLabels();
    showCoverPage();
#endif
}

void PreviewPanel::onSliderMoved(int value)
{
    if (!m_userSeeking)
        return;
    if (m_durationSeconds <= 0.0)
        return;
    const double seconds = m_durationSeconds * value / 1000.0;
    m_positionSeconds = seconds;
#if VF_ENABLE_PLAYER
    if (m_player && m_player->source().isValid())
        m_player->setPosition(static_cast<qint64>(seconds * 1000.0));
#endif
    m_filmstrip->setPosition(seconds);
    updateTimeLabels();
    emit positionChanged(seconds);
}

void PreviewPanel::onDurationChanged(qint64 durationMs)
{
    if (durationMs > 0) {
        m_durationSeconds = durationMs / 1000.0;
        updateTimeLabels();
        emit mediaLoaded(m_durationSeconds);
    }
}

void PreviewPanel::onPositionChanged(qint64 positionMs)
{
    const double seconds = positionMs / 1000.0;
    m_positionSeconds = seconds;
    if (!m_userSeeking && m_durationSeconds > 0.0) {
        const int value = static_cast<int>(1000.0 * seconds / m_durationSeconds);
        QSignalBlocker blocker(m_positionSlider);
        m_positionSlider->setValue(qBound(0, value, 1000));
    }
    m_filmstrip->setPosition(seconds);
    updateTimeLabels();
    emit positionChanged(seconds);
}

void PreviewPanel::onVolumeChanged(int value)
{
#if VF_ENABLE_PLAYER
    if (m_audioOutput)
        m_audioOutput->setVolume(value / 100.0);
#else
    Q_UNUSED(value);
#endif
}

void PreviewPanel::setPosition(double seconds)
{
    m_positionSeconds = seconds;
    m_filmstrip->setPosition(seconds);
    if (m_durationSeconds > 0.0) {
        QSignalBlocker blocker(m_positionSlider);
        m_positionSlider->setValue(qBound(0, static_cast<int>(1000.0 * seconds / m_durationSeconds), 1000));
    }
    updateTimeLabels();
}

void PreviewPanel::updateTimeLabels()
{
    if (m_durationSeconds > 0.0)
        m_timeLabel->setText(QStringLiteral("%1 / %2")
                                 .arg(fmt::duration(m_positionSeconds), fmt::duration(m_durationSeconds)));
    else
        m_timeLabel->setText(QStringLiteral("--:-- / --:--"));
}

void PreviewPanel::fillTables()
{
    // ---------------------------------------------------------------- 概览
    QList<QPair<QString, QString>> overview;
    if (!m_info.ok) {
        overview << qMakePair(QStringLiteral("状态"),
                              m_info.errorMessage.isEmpty() ? QStringLiteral("未选择文件")
                                                            : m_info.errorMessage);
    } else {
        overview << qMakePair(QStringLiteral("文件名"), QFileInfo(m_info.filePath).fileName());
        overview << qMakePair(QStringLiteral("完整路径"), m_info.filePath);
        overview << qMakePair(QStringLiteral("容器格式"), m_info.containerText());
        overview << qMakePair(QStringLiteral("时长"), fmt::duration(m_info.durationSeconds));
        overview << qMakePair(QStringLiteral("文件大小"), fmt::size(m_info.fileSize));
        overview << qMakePair(QStringLiteral("总码率"), fmt::bitrate(m_info.bitrate));
        overview << qMakePair(QStringLiteral("编码概览"), m_info.codecSummary());
        overview << qMakePair(QStringLiteral("流数量"),
                              QStringLiteral("视频 %1 · 音频 %2 · 字幕 %3")
                                  .arg(m_info.videos.size())
                                  .arg(m_info.audios.size())
                                  .arg(m_info.subtitles.size()));
    }
    fillTable(m_overviewTable, overview);

    // ---------------------------------------------------------------- 视频流
    QList<QPair<QString, QString>> videoRows;
    if (m_info.videos.isEmpty()) {
        videoRows << qMakePair(QStringLiteral("视频流"), QStringLiteral("无"));
    } else {
        for (const VideoStreamInfo &video : m_info.videos) {
            const QString prefix = m_info.videos.size() > 1
                ? QStringLiteral("#%1 ").arg(video.index) : QString();
            videoRows << qMakePair(prefix + QStringLiteral("编码器"), friendlyCodec(video.codecName));
            if (!video.profile.isEmpty())
                videoRows << qMakePair(prefix + QStringLiteral("编码档次"), video.profile);
            videoRows << qMakePair(prefix + QStringLiteral("分辨率"),
                                   QStringLiteral("%1 × %2（%3）")
                                       .arg(video.width).arg(video.height)
                                       .arg(fmt::aspectRatio(video.width, video.height)));
            videoRows << qMakePair(prefix + QStringLiteral("帧率"), fmt::fps(video.fps));
            videoRows << qMakePair(prefix + QStringLiteral("像素格式"),
                                   QStringLiteral("%1 · %2 位").arg(video.pixelFormat).arg(video.bitDepth));
            if (!video.colorSpace.isEmpty() || !video.colorPrimaries.isEmpty())
                videoRows << qMakePair(prefix + QStringLiteral("色彩"),
                                       QStringLiteral("%1 / %2 / %3")
                                           .arg(video.colorPrimaries.isEmpty() ? QStringLiteral("?") : video.colorPrimaries,
                                                video.colorTransfer.isEmpty() ? QStringLiteral("?") : video.colorTransfer,
                                                video.colorSpace.isEmpty() ? QStringLiteral("?") : video.colorSpace));
            if (video.bitrate > 0)
                videoRows << qMakePair(prefix + QStringLiteral("码率"), fmt::bitrate(video.bitrate));
            if (video.rotationDegrees != 0)
                videoRows << qMakePair(prefix + QStringLiteral("旋转"), QStringLiteral("%1°").arg(video.rotationDegrees));
        }
    }
    fillTable(m_videoTable, videoRows);

    // ---------------------------------------------------------------- 音频流
    QList<QPair<QString, QString>> audioRows;
    if (m_info.audios.isEmpty()) {
        audioRows << qMakePair(QStringLiteral("音频流"), QStringLiteral("无（视频将静音输出）"));
    } else {
        for (const AudioStreamInfo &audio : m_info.audios) {
            const QString prefix = m_info.audios.size() > 1
                ? QStringLiteral("#%1 ").arg(audio.index) : QString();
            audioRows << qMakePair(prefix + QStringLiteral("编码器"), friendlyCodec(audio.codecName));
            audioRows << qMakePair(prefix + QStringLiteral("声道"),
                                   QStringLiteral("%1（%2）").arg(channelText(audio.channels),
                                                                 audio.channelLayout.isEmpty()
                                                                     ? QStringLiteral("-") : audio.channelLayout));
            audioRows << qMakePair(prefix + QStringLiteral("采样率"),
                                   audio.sampleRate > 0 ? QStringLiteral("%1 Hz").arg(audio.sampleRate)
                                                        : QStringLiteral("未知"));
            if (!audio.sampleFormat.isEmpty())
                audioRows << qMakePair(prefix + QStringLiteral("采样格式"), audio.sampleFormat);
            if (audio.bitrate > 0)
                audioRows << qMakePair(prefix + QStringLiteral("码率"), fmt::bitrate(audio.bitrate));
            if (!audio.language.isEmpty())
                audioRows << qMakePair(prefix + QStringLiteral("语言"), audio.language);
        }
    }
    fillTable(m_audioTable, audioRows);

    // ---------------------------------------------------------------- 字幕 / 章节
    QList<QPair<QString, QString>> extraRows;
    if (m_info.subtitles.isEmpty()) {
        extraRows << qMakePair(QStringLiteral("字幕"), QStringLiteral("无"));
    } else {
        for (const SubtitleStreamInfo &subtitle : m_info.subtitles) {
            QStringList flags;
            if (subtitle.isDefault)
                flags << QStringLiteral("默认");
            if (subtitle.isForced)
                flags << QStringLiteral("强制");
            extraRows << qMakePair(QStringLiteral("字幕 #%1").arg(subtitle.index),
                                   QStringLiteral("%1%2%3")
                                       .arg(friendlyCodec(subtitle.codecName),
                                            subtitle.language.isEmpty() ? QString()
                                                                        : QStringLiteral(" · ") + subtitle.language,
                                            flags.isEmpty() ? QString()
                                                            : QStringLiteral(" · ") + flags.join(QLatin1Char('/'))));
        }
    }
    extraRows << qMakePair(QStringLiteral("章节"), m_info.chapterCount > 0
                                                       ? QStringLiteral("%1 个").arg(m_info.chapterCount)
                                                       : QStringLiteral("无"));
    fillTable(m_extraTable, extraRows);
}

} // namespace vf
