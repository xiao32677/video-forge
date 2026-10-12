#pragma once

// =============================================================================
//  VideoForge - 通用界面小部件
//
//  GlyphLabel : 用矢量图标绘制的小标签（跟随主题换色）
//  Chip       : 圆角小徽章（用于状态栏与面板标题旁的信息块）
//  SectionCard: 带图标标题的卡片容器（替代朴素的 QGroupBox）
// =============================================================================

#include "ui/Icons.h"

#include <QColor>
#include <QFrame>
#include <QWidget>

class QHBoxLayout;
class QLabel;
class QVBoxLayout;

namespace vf {

/// 只画一个矢量图标的轻量控件（颜色跟随主题，主题切换时无需重建）
class GlyphLabel : public QWidget
{
    Q_OBJECT

public:
    explicit GlyphLabel(icons::Glyph glyph, int size = 18,
                        const QColor &color = QColor(), QWidget *parent = nullptr);

    void setGlyph(icons::Glyph glyph);
    void setGlyphColor(const QColor &color);
    void setGlyphSize(int size);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    icons::Glyph m_glyph;
    int m_size;
    QColor m_color;   ///< 无效颜色表示"用主题强调色"
};

/// 圆角小徽章
class Chip : public QWidget
{
    Q_OBJECT

public:
    explicit Chip(const QString &text, const QColor &color,
                  icons::Glyph glyph = icons::Glyph::Info, QWidget *parent = nullptr);

    void setText(const QString &text);
    void setColor(const QColor &color);
    void setGlyph(icons::Glyph glyph);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_text;
    QColor m_color;
    icons::Glyph m_glyph;
};

/// 带图标标题的卡片
class SectionCard : public QFrame
{
    Q_OBJECT

public:
    explicit SectionCard(const QString &title, icons::Glyph glyph, QWidget *parent = nullptr);

    /// 往卡片正文里添加控件
    QVBoxLayout *body() const { return m_bodyLayout; }

    /// 标题右侧的附加控件（例如"重置"按钮或说明文字）
    void addHeaderWidget(QWidget *widget);

    void setTitle(const QString &title);
    void setSubtitle(const QString &subtitle);
    void setGlyph(icons::Glyph glyph);

    /// 标题栏高度（子类/外部画分隔线时用得到）
    static int headerHeight();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    GlyphLabel *m_glyphLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_subtitleLabel = nullptr;
    QWidget *m_headerWidget = nullptr;
    QHBoxLayout *m_headerLayout = nullptr;
    QVBoxLayout *m_bodyLayout = nullptr;
};

} // namespace vf
