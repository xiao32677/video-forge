#include "ui/AppIcon.h"
#include "ui/Theme.h"

#include <QPainter>
#include <QPainterPath>

namespace vf {
namespace appicon {

QIcon applicationIcon()
{
    static QIcon cached;
    if (!cached.isNull())
        return cached;

    // 1) 资源里的 .ico（含 16/24/32/48/64/128/256 多尺寸）
    QIcon fromResource(QStringLiteral(":/assets/videoforge.ico"));
    if (!fromResource.isNull() && !fromResource.availableSizes().isEmpty()) {
        cached = fromResource;
        return cached;
    }

    // 2) 资源里的 png
    QIcon fromPng(QStringLiteral(":/assets/logo.png"));
    if (!fromPng.isNull()) {
        cached = fromPng;
        return cached;
    }

    // 3) 兜底：现场画一个，绝不出现"无图标"
    QIcon fallback;
    for (int size : {16, 24, 32, 48, 64, 128, 256})
        fallback.addPixmap(logo(size));
    cached = fallback;
    return cached;
}

QPixmap logo(int size)
{
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient gradient(0, 0, 0, size);
    gradient.setColorAt(0.0, QColor(0x5B, 0x9B, 0xFF));
    gradient.setColorAt(1.0, QColor(0x28, 0x53, 0xC8));
    QPainterPath rounded;
    rounded.addRoundedRect(QRectF(0, 0, size, size), size * 0.22, size * 0.22);
    painter.fillPath(rounded, gradient);

    // 转换箭头环
    QPen ringPen(QColor(255, 255, 255, 235), size * 0.075, Qt::SolidLine, Qt::FlatCap);
    painter.setPen(ringPen);
    const QRectF ring(size * 0.17, size * 0.17, size * 0.66, size * 0.66);
    painter.drawArc(ring, -58 * 16, 308 * 16);

    // 播放三角
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255));
    QPolygonF triangle;
    triangle << QPointF(size * 0.375, size * 0.325)
             << QPointF(size * 0.685, size * 0.5)
             << QPointF(size * 0.375, size * 0.675);
    painter.drawPolygon(triangle);

    return pixmap;
}

} // namespace appicon
} // namespace vf
