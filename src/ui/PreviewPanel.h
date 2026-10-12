#pragma once

// =============================================================================
//  VideoForge - 预览面板（中间主区域）
//
//  包含四块内容：
//    1. 视频画面：未播放时显示封面缩略图，播放时切到真实画面
//    2. 播放控制：播放/暂停、进度条、音量、倍速，以及"设为裁剪起点/终点"
//    3. 胶片条：按时间轴铺开的缩略图，可点击跳转
//    4. 信息表：概览 / 视频流 / 音频流 / 字幕与章节
// =============================================================================

#include "core/MediaInfo.h"

#include <QImage>
#include <QList>
#include <QWidget>

class QLabel;
class QPushButton;
class QSlider;
class QComboBox;
class QStackedWidget;
class QTabWidget;
class QTableWidget;

#if VF_ENABLE_PLAYER
class QMediaPlayer;
class QAudioOutput;
class QVideoWidget;
#endif

namespace vf {

class FilmstripWidget;

class PreviewPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewPanel(QWidget *parent = nullptr);
    ~PreviewPanel() override;

    /// 载入一份新的媒体信息：显示信息表、重置缩略图区、把播放器指向该文件
    void showMedia(const MediaInfo &info);

    /// 清空（无选中文件时）
    void clearMedia();

    void setCoverImage(const QImage &image);
    void setFilmstrip(const QList<QImage> &frames, double durationSeconds);
    void setHint(const QString &text);

    /// 外部（胶片条/进度条）同步播放位置
    void setPosition(double seconds);

    const MediaInfo &mediaInfo() const { return m_info; }

signals:
    /// 用户请求把当前时间设为裁剪起点/终点
    void trimStartRequested(double seconds);
    void trimEndRequested(double seconds);
    /// 播放位置变化（秒）
    void positionChanged(double seconds);
    /// 有文件被载入播放器（用于外部同步时长）
    void mediaLoaded(double durationSeconds);

private slots:
    void onPlayPauseClicked();
    void onStopClicked();
    void onSliderMoved(int value);
    void onDurationChanged(qint64 durationMs);
    void onPositionChanged(qint64 positionMs);
    void onVolumeChanged(int value);

private:
    void buildPlayerArea(QWidget *container);
    void buildInfoArea(QWidget *container);
    void fillTables();
    void showCoverPage();
    void showVideoPage();
    void updateTimeLabels();
    void setPlayerEnabled(bool enabled);

    MediaInfo m_info;
    QImage m_cover;
    double m_durationSeconds = 0.0;
    double m_positionSeconds = 0.0;
    bool m_userSeeking = false;

    QStackedWidget *m_stack = nullptr;
    QLabel *m_coverLabel = nullptr;
    QLabel *m_hintLabel = nullptr;
    FilmstripWidget *m_filmstrip = nullptr;

    QPushButton *m_playButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QPushButton *m_trimStartButton = nullptr;
    QPushButton *m_trimEndButton = nullptr;
    QSlider *m_positionSlider = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QComboBox *m_speedCombo = nullptr;
    QLabel *m_timeLabel = nullptr;

    QTabWidget *m_infoTabs = nullptr;
    QTableWidget *m_overviewTable = nullptr;
    QTableWidget *m_videoTable = nullptr;
    QTableWidget *m_audioTable = nullptr;
    QTableWidget *m_extraTable = nullptr;

#if VF_ENABLE_PLAYER
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QVideoWidget *m_videoWidget = nullptr;
#endif
};

} // namespace vf
