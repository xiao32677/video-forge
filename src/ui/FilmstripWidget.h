#pragma once

// =============================================================================
//  VideoForge - 胶片条控件
//
//  把 N 张缩略图按时间轴横排展示，点击某帧即跳转到对应时间；
//  播放时会在对应位置画一条播放头。这是"看得到内容"的核心体验。
// =============================================================================

#include <QImage>
#include <QList>
#include <QWidget>

namespace vf {

class FilmstripWidget : public QWidget
{
    Q_OBJECT

public:
    explicit FilmstripWidget(QWidget *parent = nullptr);

    /// 设置帧序列（按时间顺序）
    void setFrames(const QList<QImage> &frames, double durationSeconds);

    /// 清空（切换文件时调用）
    void clearFrames();

    /// 是否已经加载了帧
    bool hasFrames() const { return !m_frames.isEmpty(); }

    /// 当前播放位置（秒），用于绘制播放头
    void setPosition(double seconds);
    double position() const { return m_position; }

    /// 点击帧 -> 该帧对应的时间（秒）
    double timeForIndex(int index) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /// 用户点击了某一帧
    void frameClicked(double timeSeconds);
    /// 用户悬停到某一帧（用于主窗口显示预览）
    void frameHovered(int index);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    int indexAt(const QPoint &position) const;
    QRect frameRect(int index) const;
    int visibleFrameCount() const;

    QList<QImage> m_frames;
    double m_duration = 0.0;
    double m_position = 0.0;
    int m_hoveredIndex = -1;
};

} // namespace vf
