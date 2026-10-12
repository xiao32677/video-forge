#include "ui/UiStyle.h"

#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QStyleOption>

namespace vf {

UiStyle::UiStyle()
    : QProxyStyle(QStringLiteral("Fusion"))
{
}

void UiStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                            QPainter *painter, const QWidget *widget) const
{
    if (!option || !painter)
        return;

    const QRect rect = option->rect;

    switch (element) {
    case PE_IndicatorArrowDown:
    case PE_IndicatorArrowUp:
    case PE_IndicatorArrowLeft:
    case PE_IndicatorArrowRight: {
        icons::Glyph glyph = icons::Glyph::ChevronDown;
        if (element == PE_IndicatorArrowUp)
            glyph = icons::Glyph::ChevronDown;   // 旋转由下面的 transform 处理
        else if (element == PE_IndicatorArrowLeft)
            glyph = icons::Glyph::ChevronRight;
        else if (element == PE_IndicatorArrowRight)
            glyph = icons::Glyph::ChevronRight;

        const QColor color = (option->state & State_Enabled) ? theme::mutedText()
                                                             : theme::mutedText().darker(150);
        const int size = qBound(10, qMin(rect.width(), rect.height()) + 6, 20);
        QPixmap glyphPixmap = icons::pixmap(glyph, color, size);

        painter->save();
        painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
        QPointF center = QRectF(rect).center();
        painter->translate(center);
        if (element == PE_IndicatorArrowUp)
            painter->rotate(180.0);
        else if (element == PE_IndicatorArrowLeft)
            painter->rotate(180.0);
        painter->translate(-center);
        const QRectF target(center.x() - size / 2.0, center.y() - size / 2.0, size, size);
        painter->drawPixmap(target.toRect(), glyphPixmap);
        painter->restore();
        return;
    }

    case PE_IndicatorCheckBox:
    case PE_IndicatorItemViewItemCheck: {
        const bool checked = option->state & State_On;
        const bool hovered = option->state & State_MouseOver;
        const bool enabled = option->state & State_Enabled;
        const int side = qMin(qMin(rect.width(), rect.height()), 18);
        const QRectF box(QRectF(rect).center().x() - side / 2.0,
                         QRectF(rect).center().y() - side / 2.0, side, side);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        QColor accent = theme::accent();
        if (!enabled)
            accent = theme::mutedText().darker(130);

        if (checked) {
            QLinearGradient gradient(box.topLeft(), box.bottomRight());
            gradient.setColorAt(0.0, accent.lighter(118));
            gradient.setColorAt(1.0, accent.darker(108));
            painter->setPen(Qt::NoPen);
            painter->setBrush(gradient);
            painter->drawRoundedRect(box, side * 0.30, side * 0.30);

            QPen checkPen(QColor(255, 255, 255), side * 0.15);
            checkPen.setCapStyle(Qt::RoundCap);
            checkPen.setJoinStyle(Qt::RoundJoin);
            painter->setPen(checkPen);
            painter->setBrush(Qt::NoBrush);
            QPainterPath check;
            check.moveTo(box.left() + side * 0.24, box.top() + side * 0.53);
            check.lineTo(box.left() + side * 0.44, box.top() + side * 0.72);
            check.lineTo(box.left() + side * 0.78, box.top() + side * 0.29);
            painter->drawPath(check);
        } else {
            QColor border = theme::mutedText();
            border.setAlpha(hovered ? 220 : 140);
            painter->setPen(QPen(border, 1.4));
            painter->setBrush(QColor(theme::inputBackground()));
            painter->drawRoundedRect(box.adjusted(0.7, 0.7, -0.7, -0.7), side * 0.30, side * 0.30);
        }
        painter->restore();
        return;
    }

    case PE_IndicatorRadioButton: {
        const bool checked = option->state & State_On;
        const int side = qMin(qMin(rect.width(), rect.height()), 18);
        const QRectF box(QRectF(rect).center().x() - side / 2.0,
                         QRectF(rect).center().y() - side / 2.0, side, side);
        const QColor accent = theme::accent();

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(checked ? accent : theme::mutedText(), checked ? 5.2 : 1.4));
        painter->setBrush(QColor(theme::inputBackground()));
        painter->drawEllipse(box.adjusted(2.6, 2.6, -2.6, -2.6));
        painter->restore();
        return;
    }

    default:
        break;
    }

    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

int UiStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    switch (metric) {
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
        return 17;   // 复选框/单选框的方块尺寸，比 Fusion 默认的 13px 更易点、更好看
    case PM_ScrollBarExtent:
        return 12;
    default:
        break;
    }
    return QProxyStyle::pixelMetric(metric, option, widget);
}

} // namespace vf
