#include "ui/Icons.h"

#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPolygonF>
#include <QtMath>

namespace vf {
namespace icons {

namespace {

constexpr qreal kDesignSize = 24.0;

void strokePath(QPainter &painter, const QPainterPath &path)
{
    painter.drawPath(path);
}

/// 画一个"圆角矩形"
QPainterPath roundedRect(qreal x, qreal y, qreal w, qreal h, qreal r)
{
    QPainterPath path;
    path.addRoundedRect(QRectF(x, y, w, h), r, r);
    return path;
}

void drawGlyph(QPainter &painter, Glyph glyph, const QColor &color)
{
    QPen pen(color, 1.7);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (glyph) {
    case Glyph::Add:
        painter.drawLine(QPointF(12, 5.5), QPointF(12, 18.5));
        painter.drawLine(QPointF(5.5, 12), QPointF(18.5, 12));
        break;

    case Glyph::Folder:
    case Glyph::FolderOpen: {
        QPainterPath path;
        path.moveTo(3, 18);
        path.lineTo(3, 7);
        path.quadTo(3, 5.6, 4.4, 5.6);
        path.lineTo(9.2, 5.6);
        path.lineTo(11.3, 8.2);
        path.lineTo(19.6, 8.2);
        path.quadTo(21, 8.2, 21, 9.6);
        path.lineTo(21, 18);
        path.quadTo(21, 19.4, 19.6, 19.4);
        path.lineTo(4.4, 19.4);
        path.quadTo(3, 19.4, 3, 18);
        strokePath(painter, path);
        if (glyph == Glyph::FolderOpen)
            painter.drawLine(QPointF(7.5, 12.6), QPointF(20.5, 12.6));
        break;
    }

    case Glyph::Stack: {
        painter.drawPath(roundedRect(6.5, 4.5, 11, 11, 2.5));
        painter.drawPath(roundedRect(4.0, 7.0, 11, 11, 2.5));
        break;
    }

    case Glyph::Trash: {
        painter.drawLine(QPointF(4.5, 7), QPointF(19.5, 7));
        painter.drawLine(QPointF(9.5, 4.5), QPointF(14.5, 4.5));
        QPainterPath body;
        body.moveTo(6.5, 7);
        body.lineTo(7.3, 19);
        body.quadTo(7.4, 20.2, 8.6, 20.2);
        body.lineTo(15.4, 20.2);
        body.quadTo(16.6, 20.2, 16.7, 19);
        body.lineTo(17.5, 7);
        strokePath(painter, body);
        painter.drawLine(QPointF(10.4, 10.5), QPointF(10.7, 16.6));
        painter.drawLine(QPointF(13.6, 10.5), QPointF(13.3, 16.6));
        break;
    }

    case Glyph::Play: {
        QPolygonF triangle;
        triangle << QPointF(8, 5.5) << QPointF(18.5, 12) << QPointF(8, 18.5);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPolygon(triangle);
        break;
    }

    case Glyph::PlayCircle: {
        painter.drawEllipse(QPointF(12, 12), 8.4, 8.4);
        QPolygonF triangle;
        triangle << QPointF(10.2, 8.6) << QPointF(15.8, 12) << QPointF(10.2, 15.4);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPolygon(triangle);
        break;
    }

    case Glyph::Pause:
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPath(roundedRect(7.5, 5.5, 3.4, 13, 1.5));
        painter.drawPath(roundedRect(13.1, 5.5, 3.4, 13, 1.5));
        break;

    case Glyph::Stop:
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPath(roundedRect(6.5, 6.5, 11, 11, 2.5));
        break;

    case Glyph::Gear: {
        painter.drawEllipse(QPointF(12, 12), 3.1, 3.1);
        painter.drawEllipse(QPointF(12, 12), 7.4, 7.4);
        for (int i = 0; i < 8; ++i) {
            const qreal angle = i * M_PI / 4.0;
            const QPointF inner(12 + 7.4 * qCos(angle), 12 + 7.4 * qSin(angle));
            const QPointF outer(12 + 9.4 * qCos(angle), 12 + 9.4 * qSin(angle));
            painter.drawLine(inner, outer);
        }
        break;
    }

    case Glyph::Film: {
        painter.drawPath(roundedRect(3.5, 5, 17, 14, 2.4));
        painter.drawLine(QPointF(8, 5), QPointF(8, 19));
        painter.drawLine(QPointF(16, 5), QPointF(16, 19));
        for (int i = 0; i < 3; ++i) {
            const qreal y = 7.6 + i * 3.5;
            painter.drawLine(QPointF(4.8, y), QPointF(6.8, y));
            painter.drawLine(QPointF(17.2, y), QPointF(19.2, y));
        }
        break;
    }

    case Glyph::Music: {
        painter.drawLine(QPointF(9.3, 16.5), QPointF(9.3, 6.2));
        painter.drawLine(QPointF(9.3, 6.2), QPointF(18, 4.6));
        painter.drawLine(QPointF(18, 4.6), QPointF(18, 14.6));
        painter.drawEllipse(QPointF(7.1, 17), 2.3, 2.0);
        painter.drawEllipse(QPointF(15.8, 15.1), 2.3, 2.0);
        break;
    }

    case Glyph::Picture: {
        painter.drawPath(roundedRect(3.5, 5, 17, 14, 2.4));
        painter.drawEllipse(QPointF(8.6, 9.8), 1.6, 1.6);
        QPainterPath hill;
        hill.moveTo(5.0, 17.2);
        hill.lineTo(10.2, 12.2);
        hill.lineTo(13.4, 15.2);
        hill.lineTo(15.8, 13.0);
        hill.lineTo(19.0, 16.4);
        strokePath(painter, hill);
        break;
    }

    case Glyph::Check: {
        QPainterPath path;
        path.moveTo(5, 12.6);
        path.lineTo(9.8, 17.4);
        path.lineTo(19, 6.8);
        strokePath(painter, path);
        break;
    }

    case Glyph::Cross:
        painter.drawLine(QPointF(6.5, 6.5), QPointF(17.5, 17.5));
        painter.drawLine(QPointF(17.5, 6.5), QPointF(6.5, 17.5));
        break;

    case Glyph::ChevronDown: {
        QPainterPath path;
        path.moveTo(7, 10);
        path.lineTo(12, 15);
        path.lineTo(17, 10);
        strokePath(painter, path);
        break;
    }

    case Glyph::ChevronRight: {
        QPainterPath path;
        path.moveTo(10, 7);
        path.lineTo(15, 12);
        path.lineTo(10, 17);
        strokePath(painter, path);
        break;
    }

    case Glyph::Sun: {
        painter.drawEllipse(QPointF(12, 12), 4.0, 4.0);
        for (int i = 0; i < 8; ++i) {
            const qreal angle = i * M_PI / 4.0;
            const QPointF inner(12 + 6.2 * qCos(angle), 12 + 6.2 * qSin(angle));
            const QPointF outer(12 + 8.8 * qCos(angle), 12 + 8.8 * qSin(angle));
            painter.drawLine(inner, outer);
        }
        break;
    }

    case Glyph::Moon: {
        QPainterPath path;
        path.moveTo(20, 14.6);
        path.arcTo(QRectF(4, 4, 16, 16), 60, 250);
        path.quadTo(11.5, 13.2, 14.6, 8.4);
        path.quadTo(18.4, 10.6, 20, 14.6);
        path.closeSubpath();
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPath(path);
        break;
    }

    case Glyph::Info:
        painter.drawEllipse(QPointF(12, 12), 8.2, 8.2);
        painter.drawLine(QPointF(12, 11), QPointF(12, 16.6));
        painter.drawPoint(QPointF(12, 7.8));
        break;

    case Glyph::Sparkle: {
        QPainterPath star;
        star.moveTo(11, 3.4);
        star.quadTo(12.6, 9.4, 17.4, 11);
        star.quadTo(12.6, 12.6, 11, 18.6);
        star.quadTo(9.4, 12.6, 4.6, 11);
        star.quadTo(9.4, 9.4, 11, 3.4);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPath(star);

        QPainterPath small;
        small.moveTo(18.2, 14.4);
        small.quadTo(18.9, 17.0, 21.0, 17.8);
        small.quadTo(18.9, 18.6, 18.2, 21.0);
        small.quadTo(17.5, 18.6, 15.4, 17.8);
        small.quadTo(17.5, 17.0, 18.2, 14.4);
        painter.drawPath(small);
        break;
    }

    case Glyph::Download:
        painter.drawLine(QPointF(12, 4.5), QPointF(12, 15));
        painter.drawPolyline(QPolygonF({QPointF(7.6, 10.8), QPointF(12, 15.2), QPointF(16.4, 10.8)}));
        painter.drawPolyline(QPolygonF({QPointF(4.8, 14.6), QPointF(4.8, 18.4),
                                        QPointF(19.2, 18.4), QPointF(19.2, 14.6)}));
        break;

    case Glyph::Refresh: {
        QPainterPath path;
        path.arcMoveTo(QRectF(4.5, 4.5, 15, 15), 65);
        path.arcTo(QRectF(4.5, 4.5, 15, 15), 65, 280);
        strokePath(painter, path);
        painter.drawPolyline(QPolygonF({QPointF(13.4, 3.4), QPointF(17.4, 6.6), QPointF(13.2, 8.6)}));
        break;
    }

    case Glyph::Clock:
        painter.drawEllipse(QPointF(12, 12), 8.2, 8.2);
        painter.drawPolyline(QPolygonF({QPointF(12, 7.4), QPointF(12, 12.4), QPointF(15.8, 14.2)}));
        break;

    case Glyph::Gauge: {
        QPainterPath path;
        path.arcMoveTo(QRectF(4, 6, 16, 16), 170);
        path.arcTo(QRectF(4, 6, 16, 16), 170, 200);
        strokePath(painter, path);
        painter.drawLine(QPointF(12, 15.4), QPointF(16.2, 10.6));
        painter.drawEllipse(QPointF(12, 15.4), 1.5, 1.5);
        break;
    }

    case Glyph::Scissors: {
        painter.drawLine(QPointF(7.4, 8.2), QPointF(17.6, 16.4));
        painter.drawLine(QPointF(16.6, 8.2), QPointF(6.4, 16.4));
        painter.drawEllipse(QPointF(5.4, 18.2), 2.2, 2.2);
        painter.drawEllipse(QPointF(18.6, 18.2), 2.2, 2.2);
        break;
    }

    case Glyph::Shield: {
        QPainterPath path;
        path.moveTo(12, 3.6);
        path.lineTo(19.4, 6.6);
        path.lineTo(19.4, 12.2);
        path.quadTo(19.4, 17.6, 12, 20.6);
        path.quadTo(4.6, 17.6, 4.6, 12.2);
        path.lineTo(4.6, 6.6);
        path.closeSubpath();
        strokePath(painter, path);
        break;
    }

    case Glyph::Rocket: {
        QPainterPath body;
        body.moveTo(12, 3.4);
        body.quadTo(17.2, 7.2, 17.2, 13.4);
        body.lineTo(17.2, 15.6);
        body.lineTo(6.8, 15.6);
        body.lineTo(6.8, 13.4);
        body.quadTo(6.8, 7.2, 12, 3.4);
        strokePath(painter, body);
        painter.drawLine(QPointF(9.6, 18.0), QPointF(14.4, 18.0));
        painter.drawLine(QPointF(10.6, 20.6), QPointF(13.4, 20.6));
        painter.drawEllipse(QPointF(12, 9.6), 1.7, 1.7);
        break;
    }

    case Glyph::Copy:
        painter.drawPath(roundedRect(8.4, 3.6, 12, 12, 2.4));
        painter.drawPath(roundedRect(3.6, 8.4, 12, 12, 2.4));
        break;

    case Glyph::List:
        for (int i = 0; i < 3; ++i) {
            const qreal y = 7.0 + i * 5.0;
            painter.drawEllipse(QPointF(5.4, y), 1.2, 1.2);
            painter.drawLine(QPointF(9.4, y), QPointF(19, y));
        }
        break;

    case Glyph::Volume: {
        QPainterPath path;
        path.moveTo(4.6, 9.4);
        path.lineTo(8.2, 9.4);
        path.lineTo(12.2, 5.6);
        path.lineTo(12.2, 18.4);
        path.lineTo(8.2, 14.6);
        path.lineTo(4.6, 14.6);
        path.closeSubpath();
        strokePath(painter, path);
        painter.drawArc(QRectF(11.4, 7.4, 9, 9.2), -60 * 16, 120 * 16);
        break;
    }

    case Glyph::Monitor:
        painter.drawPath(roundedRect(3.4, 4.6, 17.2, 12.0, 2.4));
        painter.drawLine(QPointF(9, 20.4), QPointF(15, 20.4));
        painter.drawLine(QPointF(12, 16.6), QPointF(12, 20.4));
        break;

    case Glyph::Convert: {
        QPainterPath upper;
        upper.arcMoveTo(QRectF(4.2, 4.2, 15.6, 15.6), 150);
        upper.arcTo(QRectF(4.2, 4.2, 15.6, 15.6), 150, -240);
        strokePath(painter, upper);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPolygon(QPolygonF({QPointF(19.4, 5.2), QPointF(20.4, 10.6), QPointF(15.2, 8.4)}));
        break;
    }

    case Glyph::Drive:
        painter.drawPath(roundedRect(3.6, 6.0, 16.8, 12.0, 2.6));
        painter.drawLine(QPointF(3.6, 13.4), QPointF(20.4, 13.4));
        painter.drawEllipse(QPointF(17.0, 15.8), 0.9, 0.9);
        break;

    case Glyph::Terminal:
        painter.drawPath(roundedRect(3.4, 4.6, 17.2, 14.8, 2.4));
        painter.drawPolyline(QPolygonF({QPointF(7.4, 9.4), QPointF(10.4, 12.0), QPointF(7.4, 14.6)}));
        painter.drawLine(QPointF(12.6, 15.0), QPointF(16.6, 15.0));
        break;

    case Glyph::Sliders:
        painter.drawLine(QPointF(4.4, 8.0), QPointF(19.6, 8.0));
        painter.drawLine(QPointF(4.4, 16.0), QPointF(19.6, 16.0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(QPointF(9.2, 8.0), 2.3, 2.3);
        painter.drawEllipse(QPointF(15.0, 16.0), 2.3, 2.3);
        break;

    case Glyph::Bolt: {
        QPainterPath path;
        path.moveTo(13.6, 3.2);
        path.lineTo(6.2, 13.4);
        path.lineTo(11.2, 13.4);
        path.lineTo(10.4, 20.8);
        path.lineTo(17.8, 10.6);
        path.lineTo(12.8, 10.6);
        path.closeSubpath();
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawPath(path);
        break;
    }

    case Glyph::Warning: {
        QPainterPath path;
        path.moveTo(12, 4.0);
        path.lineTo(21.0, 19.4);
        path.lineTo(3.0, 19.4);
        path.closeSubpath();
        strokePath(painter, path);
        painter.drawLine(QPointF(12, 10.0), QPointF(12, 14.2));
        painter.drawPoint(QPointF(12, 16.8));
        break;
    }
    }
}

} // namespace

QPixmap pixmap(Glyph glyph, const QColor &color, int size, qreal devicePixelRatio)
{
    if (size <= 0)
        size = 20;
    if (devicePixelRatio <= 0)
        devicePixelRatio = 1.0;

    // 图标会被样式表在每次重绘时反复请求（下拉箭头、复选框对勾等），
    // 必须缓存，否则每帧都要重新走一遍 QPainter 路径绘制。
    static QHash<QString, QPixmap> cache;
    const QString key = QStringLiteral("%1|%2|%3|%4")
                            .arg(static_cast<int>(glyph))
                            .arg(color.rgba())
                            .arg(size)
                            .arg(devicePixelRatio, 0, 'f', 2);
    const auto cached = cache.constFind(key);
    if (cached != cache.constEnd())
        return cached.value();

    const int physical = qMax(1, qRound(size * devicePixelRatio));
    QPixmap result(physical, physical);
    result.fill(Qt::transparent);
    result.setDevicePixelRatio(devicePixelRatio);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.scale(physical / kDesignSize, physical / kDesignSize);
    drawGlyph(painter, glyph, color);
    painter.end();

    if (cache.size() > 512)
        cache.clear();
    cache.insert(key, result);
    return result;
}

QIcon icon(Glyph glyph, const QColor &color, int size)
{
    QIcon result;
    result.addPixmap(pixmap(glyph, color, size, 1.0));
    result.addPixmap(pixmap(glyph, color, size, 2.0));
    return result;
}

} // namespace icons
} // namespace vf
