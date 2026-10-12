#include "ui/FilmstripWidget.h"
#include "core/Format.h"
#include "ui/Theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QWheelEvent>

#include <cmath>

namespace vf {

namespace {
constexpr int kGap = 3;
constexpr int kPadding = 4;
constexpr int kHeight = 76;
} // namespace

FilmstripWidget::FilmstripWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setMinimumHeight(kHeight);
    setToolTip(QStringLiteral("点击任意一帧可跳转到该时间点"));
}

QSize FilmstripWidget::sizeHint() const
{
    return QSize(560, kHeight);
}

QSize FilmstripWidget::minimumSizeHint() const
{
    return QSize(240, 44);   // 小屏下允许压得更矮，绘制仍按实际高度自适应
}

int FilmstripWidget::visibleFrameCount() const
{
    return m_frames.size();
}

void FilmstripWidget::setFrames(const QList<QImage> &frames, double durationSeconds)
{
    m_frames = frames;
    m_duration = durationSeconds;
    m_hoveredIndex = -1;
    update();
}

void FilmstripWidget::clearFrames()
{
    m_frames.clear();
    m_duration = 0.0;
    m_position = 0.0;
    m_hoveredIndex = -1;
    update();
}

void FilmstripWidget::setPosition(double seconds)
{
    if (std::abs(seconds - m_position) < 0.05)
        return;
    m_position = seconds;
    update();
}

double FilmstripWidget::timeForIndex(int index) const
{
    const int count = visibleFrameCount();
    if (count <= 0 || m_duration <= 0.0)
        return 0.0;
    if (index < 0)
        index = 0;
    if (index >= count)
        index = count - 1;
    return m_duration * (0.02 + 0.96 * (static_cast<double>(index) + 0.5) / count);
}

QRect FilmstripWidget::frameRect(int index) const
{
    const int count = visibleFrameCount();
    if (count <= 0)
        return QRect();
    const int available = width() - 2 * kPadding - (count - 1) * kGap;
    const int frameWidth = qMax(24, available / count);
    const int x = kPadding + index * (frameWidth + kGap);
    return QRect(x, kPadding, frameWidth, height() - 2 * kPadding);
}

int FilmstripWidget::indexAt(const QPoint &position) const
{
    const int count = visibleFrameCount();
    if (count <= 0)
        return -1;
    if (position.x() < kPadding || position.x() > width() - kPadding)
        return -1;
    // 按网格均匀反推索引
    const int available = width() - 2 * kPadding - (count - 1) * kGap;
    const int frameWidth = qMax(24, available / count);
    const int index = (position.x() - kPadding) / (frameWidth + kGap);
    return qBound(0, index, count - 1);
}

void FilmstripWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF background(0, 0, width(), height());
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::cardBackground().darker(105));
    painter.drawRoundedRect(background.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);

    if (m_frames.isEmpty()) {
        painter.setPen(theme::mutedText());
        painter.drawText(rect(), Qt::AlignCenter,
                         QStringLiteral("胶片条预览将在读取完成后显示"));
        return;
    }

    for (int i = 0; i < m_frames.size(); ++i) {
        const QRect target = frameRect(i);
        const QImage &image = m_frames.at(i);

        QPainterPath clip;
        clip.addRoundedRect(target, 5, 5);
        painter.save();
        painter.setClipPath(clip);
        if (image.isNull()) {
            painter.fillRect(target, theme::cardBackground().lighter(115));
            painter.setPen(theme::mutedText());
            painter.drawText(target, Qt::AlignCenter, QStringLiteral("—"));
        } else {
            const QImage scaled = image.scaled(target.size(), Qt::KeepAspectRatioByExpanding,
                                               Qt::SmoothTransformation);
            const QRect source((scaled.width() - target.width()) / 2,
                               (scaled.height() - target.height()) / 2,
                               target.width(), target.height());
            painter.drawImage(target, scaled, source);
        }
        painter.restore();

        // 悬停高亮
        painter.setBrush(Qt::NoBrush);
        if (i == m_hoveredIndex)
            painter.setPen(QPen(theme::accent(), 2));
        else
            painter.setPen(QPen(theme::mutedText().darker(160), 1));
        painter.drawRoundedRect(QRectF(target).adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);
    }

    // 播放头
    if (m_duration > 0.0) {
        const double ratio = qBound(0.0, m_position / m_duration, 1.0);
        const int x = kPadding + static_cast<int>(ratio * (width() - 2 * kPadding));
        painter.setPen(QPen(theme::accent(), 2));
        painter.drawLine(x, 2, x, height() - 2);
    }
}

void FilmstripWidget::mousePressEvent(QMouseEvent *event)
{
    const int index = indexAt(event->pos());
    if (index >= 0)
        emit frameClicked(timeForIndex(index));
}

void FilmstripWidget::mouseMoveEvent(QMouseEvent *event)
{
    const int index = indexAt(event->pos());
    if (index != m_hoveredIndex) {
        m_hoveredIndex = index;
        if (index >= 0) {
            setToolTip(QStringLiteral("第 %1 帧 · %2（点击跳转）")
                           .arg(index + 1)
                           .arg(fmt::duration(timeForIndex(index))));
            emit frameHovered(index);
        }
        update();
    }
}

void FilmstripWidget::leaveEvent(QEvent *)
{
    if (m_hoveredIndex != -1) {
        m_hoveredIndex = -1;
        update();
    }
}

void FilmstripWidget::wheelEvent(QWheelEvent *event)
{
    // 在胶片条上滚轮 = 前后拖动播放位置，比拖进度条更符合直觉
    if (m_duration <= 0.0) {
        QWidget::wheelEvent(event);
        return;
    }
    const double step = m_duration * 0.02;
    const double delta = event->angleDelta().y() > 0 ? step : -step;
    const double target = qBound(0.0, m_position + delta, m_duration);
    setPosition(target);
    emit frameClicked(target);
    event->accept();
}

} // namespace vf
