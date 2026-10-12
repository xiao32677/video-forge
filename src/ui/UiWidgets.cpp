#include "ui/UiWidgets.h"
#include "ui/Theme.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

namespace vf {

// ---------------------------------------------------------------------------
//  GlyphLabel
// ---------------------------------------------------------------------------
GlyphLabel::GlyphLabel(icons::Glyph glyph, int size, const QColor &color, QWidget *parent)
    : QWidget(parent)
    , m_glyph(glyph)
    , m_size(size)
    , m_color(color)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFixedSize(size + 2, size + 2);
}

void GlyphLabel::setGlyph(icons::Glyph glyph)
{
    m_glyph = glyph;
    update();
}

void GlyphLabel::setGlyphColor(const QColor &color)
{
    m_color = color;
    update();
}

void GlyphLabel::setGlyphSize(int size)
{
    m_size = size;
    setFixedSize(size + 2, size + 2);
    update();
}

QSize GlyphLabel::sizeHint() const
{
    return QSize(m_size + 2, m_size + 2);
}

void GlyphLabel::paintEvent(QPaintEvent *)
{
    const QColor color = m_color.isValid() ? m_color : theme::accent();
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    const QPixmap pixmap = icons::pixmap(m_glyph, color, m_size);
    const int x = (width() - m_size) / 2;
    const int y = (height() - m_size) / 2;
    painter.drawPixmap(x, y, m_size, m_size, pixmap);
}

// ---------------------------------------------------------------------------
//  Chip
// ---------------------------------------------------------------------------
Chip::Chip(const QString &text, const QColor &color, icons::Glyph glyph, QWidget *parent)
    : QWidget(parent)
    , m_text(text)
    , m_color(color.isValid() ? color : theme::mutedText())
    , m_glyph(glyph)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

void Chip::setText(const QString &text)
{
    m_text = text;
    updateGeometry();
    update();
}

void Chip::setColor(const QColor &color)
{
    m_color = color.isValid() ? color : theme::mutedText();
    update();
}

void Chip::setGlyph(icons::Glyph glyph)
{
    m_glyph = glyph;
    updateGeometry();
    update();
}

QSize Chip::sizeHint() const
{
    const QFontMetrics metrics(font());
    const int textWidth = metrics.horizontalAdvance(m_text);
    return QSize(textWidth + 34, 24);
}

QSize Chip::minimumSizeHint() const
{
    return sizeHint();
}

void Chip::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QColor background = m_color;
    background.setAlpha(30);
    QColor border = m_color;
    border.setAlpha(80);

    painter.setPen(QPen(border, 1.0));
    painter.setBrush(background);
    painter.drawRoundedRect(box, box.height() / 2.0, box.height() / 2.0);

    const int iconSize = 13;
    const QPixmap pixmap = icons::pixmap(m_glyph, m_color, iconSize);
    painter.drawPixmap(QRectF(box.left() + 8, box.center().y() - iconSize / 2.0,
                              iconSize, iconSize).toRect(),
                       pixmap);

    QFont textFont = font();
    textFont.setPointSizeF(qMax(7.5, textFont.pointSizeF() - 1.2));
    textFont.setWeight(QFont::DemiBold);
    painter.setFont(textFont);
    painter.setPen(m_color);
    painter.drawText(QRectF(box.left() + 8 + iconSize + 5, box.top(),
                            box.width() - 8 - iconSize - 5 - 8, box.height()),
                     Qt::AlignVCenter | Qt::AlignLeft, m_text);
}

// ---------------------------------------------------------------------------
//  SectionCard
// ---------------------------------------------------------------------------
int SectionCard::headerHeight()
{
    return 38;
}

SectionCard::SectionCard(const QString &title, icons::Glyph glyph, QWidget *parent)
    : QFrame(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 8, 12, 12);
    outer->setSpacing(10);

    m_headerLayout = new QHBoxLayout;
    m_headerLayout->setContentsMargins(0, 0, 0, 0);
    m_headerLayout->setSpacing(8);

    m_glyphLabel = new GlyphLabel(glyph, 16, QColor(), this);
    m_headerLayout->addWidget(m_glyphLabel);

    m_titleLabel = new QLabel(title, this);
    m_titleLabel->setProperty("role", "cardTitle");
    m_headerLayout->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel(this);
    m_subtitleLabel->setProperty("role", "hint");
    m_subtitleLabel->setVisible(false);
    m_headerLayout->addWidget(m_subtitleLabel);

    m_headerLayout->addStretch();
    outer->addLayout(m_headerLayout);

    m_bodyLayout = new QVBoxLayout;
    m_bodyLayout->setContentsMargins(0, 0, 0, 0);
    m_bodyLayout->setSpacing(8);
    outer->addLayout(m_bodyLayout);
}

void SectionCard::setTitle(const QString &title)
{
    m_titleLabel->setText(title);
}

void SectionCard::setSubtitle(const QString &subtitle)
{
    m_subtitleLabel->setText(subtitle);
    m_subtitleLabel->setVisible(!subtitle.isEmpty());
}

void SectionCard::setGlyph(icons::Glyph glyph)
{
    m_glyphLabel->setGlyph(glyph);
}

void SectionCard::addHeaderWidget(QWidget *widget)
{
    if (!widget)
        return;
    if (m_headerWidget)
        m_headerLayout->removeWidget(m_headerWidget);
    m_headerWidget = widget;
    m_headerLayout->addWidget(widget);
}

void SectionCard::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.setPen(QPen(theme::borderColor(), 1.0));
    painter.setBrush(theme::cardBackground());
    painter.drawRoundedRect(box, 10, 10);

    // 标题与正文之间的分隔线（只画内部，不碰圆角）
    const qreal separatorY = 8 + headerHeight() - 6;
    QColor separator = theme::borderColor();
    separator.setAlpha(150);
    painter.setPen(QPen(separator, 1.0));
    painter.drawLine(QPointF(box.left() + 1, separatorY), QPointF(box.right() - 1, separatorY));
}

} // namespace vf
