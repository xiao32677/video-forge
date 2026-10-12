#include "ui/Theme.h"
#include "ui/UiStyle.h"

#include <QApplication>
#include <QFont>
#include <QLinearGradient>
#include <QPalette>
#include <QSettings>

namespace vf {
namespace theme {

namespace {

Mode g_mode = Mode::Dark;

struct Palette
{
    QString window;
    QString windowText;
    QString base;
    QString alternateBase;
    QString surface;      ///< 卡片/面板
    QString elevated;     ///< 抬升一层（悬停、条带）
    QString border;
    QString borderSoft;
    QString hover;
    QString muted;
    QString accent;
    QString accentHover;
    QString accentAlt;
    QString accentText;
    QString disabled;
    QString success;
    QString warning;
    QString danger;
};

Palette paletteFor(Mode mode)
{
    Palette p;
    if (mode == Mode::Dark) {
        p.window      = QStringLiteral("#0F1116");
        p.windowText  = QStringLiteral("#E9ECF2");
        p.base        = QStringLiteral("#12151B");
        p.alternateBase = QStringLiteral("#171B22");
        p.surface     = QStringLiteral("#1A1F27");
        p.elevated    = QStringLiteral("#222833");
        p.border      = QStringLiteral("#2A3140");
        p.borderSoft  = QStringLiteral("#212734");
        p.hover       = QStringLiteral("#242B36");
        p.muted       = QStringLiteral("#8C95A6");
        p.accent      = QStringLiteral("#4C8DFF");
        p.accentHover = QStringLiteral("#6DA4FF");
        p.accentAlt   = QStringLiteral("#8B5CF6");
        p.accentText  = QStringLiteral("#FFFFFF");
        p.disabled    = QStringLiteral("#5A6270");
        p.success     = QStringLiteral("#34D399");
        p.warning     = QStringLiteral("#FBBF24");
        p.danger      = QStringLiteral("#F87171");
    } else {
        p.window      = QStringLiteral("#EDF0F6");
        p.windowText  = QStringLiteral("#141A24");
        p.base        = QStringLiteral("#FFFFFF");
        p.alternateBase = QStringLiteral("#F6F8FC");
        p.surface     = QStringLiteral("#FFFFFF");
        p.elevated    = QStringLiteral("#F3F6FB");
        p.border      = QStringLiteral("#D9E0EA");
        p.borderSoft  = QStringLiteral("#E6EBF2");
        p.hover       = QStringLiteral("#EAF0F9");
        p.muted       = QStringLiteral("#67707F");
        p.accent      = QStringLiteral("#2563EB");
        p.accentHover = QStringLiteral("#3B82F6");
        p.accentAlt   = QStringLiteral("#7C3AED");
        p.accentText  = QStringLiteral("#FFFFFF");
        p.disabled    = QStringLiteral("#A6AEBB");
        p.success     = QStringLiteral("#0E9F6E");
        p.warning     = QStringLiteral("#D97706");
        p.danger      = QStringLiteral("#DC2626");
    }
    return p;
}

/// 数字色值 -> 带透明度的 rgba() 文本，QSS 里要用
QString rgba(const QString &hex, int alpha)
{
    QColor color(hex);
    return QStringLiteral("rgba(%1, %2, %3, %4)")
        .arg(color.red()).arg(color.green()).arg(color.blue()).arg(alpha);
}

QPalette buildQPalette(const Palette &p)
{
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(p.window));
    palette.setColor(QPalette::WindowText, QColor(p.windowText));
    palette.setColor(QPalette::Base, QColor(p.base));
    palette.setColor(QPalette::AlternateBase, QColor(p.alternateBase));
    palette.setColor(QPalette::Text, QColor(p.windowText));
    palette.setColor(QPalette::Button, QColor(p.surface));
    palette.setColor(QPalette::ButtonText, QColor(p.windowText));
    palette.setColor(QPalette::ToolTipBase, QColor(p.elevated));
    palette.setColor(QPalette::ToolTipText, QColor(p.windowText));
    palette.setColor(QPalette::Highlight, QColor(p.accent));
    palette.setColor(QPalette::HighlightedText, QColor(p.accentText));
    palette.setColor(QPalette::PlaceholderText, QColor(p.muted));
    palette.setColor(QPalette::Link, QColor(p.accent));
    palette.setColor(QPalette::Mid, QColor(p.border));
    palette.setColor(QPalette::Dark, QColor(p.borderSoft));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(p.disabled));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(p.disabled));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(p.disabled));
    return palette;
}

} // namespace

Mode currentMode()
{
    return g_mode;
}

void setCurrentMode(Mode mode)
{
    g_mode = mode;
}

Mode toggle()
{
    g_mode = (g_mode == Mode::Dark) ? Mode::Light : Mode::Dark;
    return g_mode;
}

QColor accent()               { return QColor(paletteFor(g_mode).accent); }
QColor accentAlt()            { return QColor(paletteFor(g_mode).accentAlt); }
QColor textColor()            { return QColor(paletteFor(g_mode).windowText); }
QColor mutedText()            { return QColor(paletteFor(g_mode).muted); }
QColor cardBackground()       { return QColor(paletteFor(g_mode).surface); }
QColor elevatedBackground()   { return QColor(paletteFor(g_mode).elevated); }
QColor inputBackground()      { return QColor(paletteFor(g_mode).base); }
QColor windowBackground()     { return QColor(paletteFor(g_mode).window); }
QColor borderColor()          { return QColor(paletteFor(g_mode).border); }
QColor successColor()         { return QColor(paletteFor(g_mode).success); }
QColor warningColor()         { return QColor(paletteFor(g_mode).warning); }
QColor dangerColor()          { return QColor(paletteFor(g_mode).danger); }

QString styleSheet(Mode mode)
{
    const Palette p = paletteFor(mode);
    const bool dark = (mode == Mode::Dark);

    // 标题栏渐变（hero）
    const QString heroTop = dark ? QStringLiteral("#1B2231") : QStringLiteral("#FFFFFF");
    const QString heroBottom = dark ? QStringLiteral("#141822") : QStringLiteral("#F7F9FD");
    const QString actionBar = dark ? QStringLiteral("#151920") : QStringLiteral("#FFFFFF");

    QString qss = QStringLiteral(R"QSS(
/* ================================================================= 全局 */
* { outline: none; }

QWidget {
    font-size: 13px;
    color: %TEXT%;
}

QMainWindow, QDialog {
    background: qlineargradient(x1:0, y1:0, x2:0.6, y2:1,
                stop:0 %WINDOW_TOP%, stop:1 %WINDOW%);
}

QToolTip {
    background: %ELEVATED%;
    color: %TEXT%;
    border: 1px solid %BORDER%;
    border-radius: 7px;
    padding: 7px 10px;
    font-size: 12px;
}

/* ================================================================= 菜单 */
QMenuBar {
    background: transparent;
    border-bottom: 1px solid %BORDER_SOFT%;
    padding: 4px 10px 0 10px;
}
QMenuBar::item {
    padding: 7px 13px;
    margin-bottom: 3px;
    border-radius: 7px;
    background: transparent;
    color: %MUTED%;
    font-weight: 500;
}
QMenuBar::item:selected {
    background: %HOVER%;
    color: %TEXT%;
}
QMenuBar::item:pressed {
    background: %ACCENT%;
    color: %ACCENT_TEXT%;
}

QMenu {
    background: %ELEVATED%;
    border: 1px solid %BORDER%;
    border-radius: 10px;
    padding: 7px;
}
QMenu::item {
    padding: 8px 28px 8px 14px;
    border-radius: 7px;
    color: %TEXT%;
}
QMenu::item:selected {
    background: %ACCENT%;
    color: %ACCENT_TEXT%;
}
QMenu::item:disabled { color: %DISABLED%; }
QMenu::separator {
    height: 1px;
    background: %BORDER_SOFT%;
    margin: 6px 12px;
}
QMenu::icon { padding-left: 8px; }

/* ================================================================= 顶部栏 */
QToolBar {
    background: %ACTIONBAR%;
    border: none;
    border-bottom: 1px solid %BORDER_SOFT%;
    padding: 7px 10px;
    spacing: 3px;
}
QToolBar#heroBar {
    background: qlineargradient(x1:0, y1:0, x2:0.85, y2:0.4,
                stop:0 %HERO_TOP%, stop:0.55 %HERO_BOTTOM%, stop:1 %WINDOW%);
    border-bottom: 1px solid %BORDER_SOFT%;
    padding: 10px 12px 10px 12px;
}
/* 顶部两行区域：自己控制布局，比 QToolBar 的伸缩行为可靠 */
QFrame#heroRow {
    background: qlineargradient(x1:0, y1:0, x2:0.9, y2:0.7,
                stop:0 %HERO_TOP%, stop:0.55 %HERO_BOTTOM%, stop:1 %WINDOW%);
    border-bottom: 1px solid %BORDER_SOFT%;
}
QFrame#actionRow {
    background: %ACTIONBAR%;
    border-bottom: 1px solid %BORDER_SOFT%;
}
QToolBar::separator {
    width: 1px;
    background: %BORDER%;
    margin: 6px 7px;
}

QToolButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 8px;
    padding: 6px 11px;
    color: %TEXT%;
    font-weight: 500;
}
QToolButton:hover {
    background: %HOVER%;
    border-color: %BORDER%;
}
QToolButton:pressed {
    background: %ACCENT%;
    border-color: %ACCENT%;
    color: %ACCENT_TEXT%;
}
QToolButton:checked {
    background: %ACCENT_SOFT%;
    border-color: %ACCENT_BORDER%;
    color: %ACCENT%;
}
QToolButton:disabled { color: %DISABLED%; }
QToolButton[primary="true"] {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 %ACCENT%, stop:1 %ACCENT_ALT%);
    color: %ACCENT_TEXT%;
    border: 1px solid transparent;
    font-weight: 700;
    padding: 6px 16px;
}
QToolButton[primary="true"]:hover {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 %ACCENT_HOVER%, stop:1 %ACCENT_ALT_LIGHT%);
}
QToolButton[primary="true"]:disabled {
    background: %BORDER%;
    color: %DISABLED%;
}

/* ================================================================= 按钮 */
QPushButton {
    background: %ELEVATED%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 7px 16px;
    min-height: 18px;
    font-weight: 500;
}
QPushButton:hover {
    background: %HOVER%;
    border-color: %ACCENT_BORDER%;
    color: %ACCENT%;
}
QPushButton:pressed {
    background: %ACCENT%;
    border-color: %ACCENT%;
    color: %ACCENT_TEXT%;
}
QPushButton:disabled {
    color: %DISABLED%;
    border-color: %BORDER_SOFT%;
    background: %WINDOW%;
}
QPushButton[accent="true"] {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0.4,
                stop:0 %ACCENT%, stop:1 %ACCENT_ALT%);
    color: %ACCENT_TEXT%;
    border: none;
    font-weight: 700;
    font-size: 14px;
    padding: 10px 18px;
}
QPushButton[accent="true"]:hover {
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0.4,
                stop:0 %ACCENT_HOVER%, stop:1 %ACCENT_ALT_LIGHT%);
}
QPushButton[accent="true"]:disabled {
    background: %BORDER%;
    color: %DISABLED%;
}
QPushButton[danger="true"]:hover {
    border-color: %DANGER%;
    color: %DANGER%;
    background: %DANGER_SOFT%;
}
QPushButton[ghost="true"] {
    background: transparent;
    border-color: %BORDER_SOFT%;
}
/* 状态栏里的胶囊按钮（例如 ffmpeg 状态） */
QPushButton[chip="true"] {
    background: %ELEVATED%;
    border: 1px solid %BORDER%;
    border-radius: 11px;
    padding: 2px 12px;
    font-size: 11.5px;
    font-weight: 600;
    min-height: 18px;
}
QPushButton[chip="true"]:hover {
    border-color: %ACCENT_BORDER%;
    color: %ACCENT%;
}

/* ================================================================= 输入 */
QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTimeEdit {
    background: %BASE%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 6px 9px;
    selection-background-color: %ACCENT%;
    selection-color: %ACCENT_TEXT%;
}
QLineEdit:hover, QSpinBox:hover, QDoubleSpinBox:hover, QComboBox:hover {
    border-color: %BORDER_STRONG%;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus,
QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QComboBox:on, QTimeEdit:focus {
    border-color: %ACCENT%;
    background: %BASE_FOCUS%;
}
QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled,
QDoubleSpinBox:disabled, QPlainTextEdit:disabled {
    color: %DISABLED%;
    background: %WINDOW%;
    border-color: %BORDER_SOFT%;
}

QComboBox::drop-down {
    subcontrol-origin: padding;
    subcontrol-position: center right;
    border: none;
    width: 24px;
}
QComboBox QAbstractItemView {
    background: %ELEVATED%;
    border: 1px solid %BORDER%;
    border-radius: 9px;
    padding: 5px;
    selection-background-color: %ACCENT%;
    selection-color: %ACCENT_TEXT%;
    outline: none;
}

QSpinBox::up-button, QSpinBox::down-button,
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
    width: 17px;
    border: none;
    background: transparent;
    margin-right: 2px;
}

/* ================================================================= 滑块 */
QSlider::groove:horizontal {
    height: 6px;
    border-radius: 3px;
    background: %BORDER_SOFT%;
}
QSlider::sub-page:horizontal {
    height: 6px;
    border-radius: 3px;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 %ACCENT%, stop:1 %ACCENT_ALT%);
}
QSlider::handle:horizontal {
    width: 15px;
    height: 15px;
    margin: -5px 0;
    border-radius: 8px;
    background: #FFFFFF;
    border: 2px solid %ACCENT%;
}
QSlider::handle:horizontal:hover {
    background: %ACCENT%;
    border-color: #FFFFFF;
}
QSlider::handle:horizontal:disabled {
    background: %BORDER%;
    border-color: %DISABLED%;
}
QSlider::sub-page:horizontal:disabled { background: %BORDER%; }

/* ================================================================= 进度条 */
QProgressBar {
    background: %BASE%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    text-align: center;
    height: 18px;
    color: %TEXT%;
    font-size: 11px;
    font-weight: 600;
}
QProgressBar::chunk {
    border-radius: 7px;
    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 %ACCENT%, stop:1 %ACCENT_ALT%);
}

/* ================================================================= 列表表格 */
QListWidget, QTreeWidget, QTableWidget, QListView, QTreeView, QTableView {
    background: %BASE%;
    border: 1px solid %BORDER_SOFT%;
    border-radius: 10px;
    alternate-background-color: %ALT%;
    selection-background-color: %ACCENT_SOFT%;
    selection-color: %TEXT%;
}
QListWidget::item, QTreeWidget::item { border: none; }
QHeaderView::section {
    background: %ELEVATED%;
    color: %MUTED%;
    border: none;
    border-bottom: 1px solid %BORDER%;
    border-right: 1px solid %BORDER_SOFT%;
    padding: 8px 10px;
    font-weight: 600;
    font-size: 11px;
}
QHeaderView::section:first { border-top-left-radius: 0; }
QHeaderView::section:last { border-right: none; }
QTableCornerButton::section { background: %ELEVATED%; border: none; }
QTableWidget::item, QTableView::item { padding: 5px 7px; border: none; }

/* ================================================================= 标签页 */
QTabWidget::pane {
    border: 1px solid %BORDER_SOFT%;
    border-radius: 10px;
    background: %BASE%;
    top: -1px;
}
QTabBar { qproperty-drawBase: 0; }
QTabBar::tab {
    background: transparent;
    color: %MUTED%;
    padding: 7px 15px;
    margin: 0 3px 4px 0;
    border: 1px solid transparent;
    border-radius: 8px;
    font-weight: 500;
}
QTabBar::tab:hover {
    color: %TEXT%;
    background: %HOVER%;
}
QTabBar::tab:selected {
    color: %ACCENT%;
    background: %ACCENT_SOFT%;
    border-color: %ACCENT_BORDER%;
    font-weight: 700;
}

/* ================================================================= 分组框（兼容保留） */
QGroupBox {
    border: 1px solid %BORDER_SOFT%;
    border-radius: 10px;
    margin-top: 14px;
    padding: 12px 10px 10px 10px;
    background: %SURFACE%;
    font-weight: 600;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 12px;
    padding: 0 6px;
    color: %MUTED%;
}

/* ================================================================= 分隔条 */
QSplitter::handle { background: transparent; }
QSplitter::handle:horizontal { width: 3px; }
QSplitter::handle:vertical { height: 3px; }
QSplitter::handle:hover { background: %ACCENT_BORDER%; }

/* ================================================================= 滚动条 */
QScrollBar:vertical {
    background: transparent;
    width: 12px;
    margin: 3px 2px 3px 0;
}
QScrollBar::handle:vertical {
    background: %BORDER%;
    border-radius: 4px;
    min-height: 38px;
}
QScrollBar::handle:vertical:hover { background: %MUTED%; }
QScrollBar:horizontal {
    background: transparent;
    height: 12px;
    margin: 0 3px 2px 3px;
}
QScrollBar::handle:horizontal {
    background: %BORDER%;
    border-radius: 4px;
    min-width: 38px;
}
QScrollBar::handle:horizontal:hover { background: %MUTED%; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* ================================================================= 状态栏 */
QStatusBar {
    background: %ACTIONBAR%;
    border-top: 1px solid %BORDER_SOFT%;
    color: %MUTED%;
    min-height: 30px;
}
QStatusBar::item { border: none; }
QStatusBar QLabel { color: %MUTED%; font-size: 12px; }

/* ================================================================= 复选框 */
QCheckBox, QRadioButton {
    spacing: 8px;
    padding: 3px 0;
    color: %TEXT%;
}
QCheckBox:disabled, QRadioButton:disabled { color: %DISABLED%; }

/* ================================================================= 语义化属性 */
QLabel[role="hint"]     { color: %MUTED%; font-size: 11.5px; }
QLabel[role="cardTitle"]{ font-size: 13px; font-weight: 700; color: %TEXT%; }
QLabel[role="panelTitle"]{ font-size: 14px; font-weight: 700; color: %TEXT%; }
QLabel[role="heroTitle"]{ font-size: 17px; font-weight: 800; color: %TEXT%; }
QLabel[role="heroSub"]  { font-size: 11.5px; color: %MUTED%; }
QLabel[role="section"]  { color: %MUTED%; font-size: 11px; font-weight: 700; }
QLabel[role="empty"]    { color: %MUTED%; font-size: 13px; }
QLabel[role="ok"]       { color: %SUCCESS%; }
QLabel[role="warn"]     { color: %WARN%; }
QLabel[role="error"]    { color: %DANGER%; }
QLabel[role="metric"]   { font-size: 15px; font-weight: 700; color: %TEXT%; }

QFrame[role="separator"] { background: %BORDER_SOFT%; max-height: 1px; border: none; }
/* 预览画面区：视频/封面都落在这块"屏幕"里 */
QStackedWidget#videoSurface {
    background: #0A0C10;
    border: 1px solid %BORDER%;
    border-radius: 12px;
}
QFrame[role="card"] {
    background: %SURFACE%;
    border: 1px solid %BORDER_SOFT%;
    border-radius: 10px;
}
QFrame[role="heroChip"] {
    background: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 10px;
}
)QSS");

    struct Token { const char *key; QString value; };
    // 顺序很重要：长 key 必须先替换，否则会被短 key 拆坏
    const Token tokens[] = {
        {"%ACCENT_TEXT%",      p.accentText},
        {"%ACCENT_HOVER%",     p.accentHover},
        {"%ACCENT_BORDER%",    rgba(p.accent, dark ? 110 : 90)},
        {"%ACCENT_SOFT%",      rgba(p.accent, dark ? 38 : 26)},
        {"%ACCENT_ALT_LIGHT%", QColor(p.accentAlt).lighter(115).name()},
        {"%ACCENT_ALT%",       p.accentAlt},
        {"%ACCENT%",           p.accent},
        {"%SUCCESS%",          p.success},
        {"%WARN%",             p.warning},
        {"%DANGER_SOFT%",      rgba(p.danger, 30)},
        {"%DANGER%",           p.danger},
        {"%BORDER_STRONG%",    QColor(p.border).lighter(125).name()},
        {"%BORDER_SOFT%",      p.borderSoft},
        {"%BORDER%",           p.border},
        {"%ELEVATED%",         p.elevated},
        {"%SURFACE%",          p.surface},
        {"%HOVER%",            p.hover},
        {"%WINDOW_TOP%",       dark ? QStringLiteral("#131722") : QStringLiteral("#F7F9FD")},
        {"%WINDOW%",           p.window},
        {"%BASE_FOCUS%",       dark ? QColor(p.base).lighter(118).name() : QStringLiteral("#FFFFFF")},
        {"%BASE%",             p.base},
        {"%ALT%",              p.alternateBase},
        {"%HERO_TOP%",         heroTop},
        {"%HERO_BOTTOM%",      heroBottom},
        {"%ACTIONBAR%",        actionBar},
        {"%DISABLED%",         p.disabled},
        {"%TEXT%",             p.windowText},
        {"%MUTED%",            p.muted},
    };
    for (const Token &token : tokens)
        qss.replace(QString::fromLatin1(token.key), token.value);
    return qss;
}

void apply(QApplication &app, Mode mode)
{
    setCurrentMode(mode);

    // 自定义代理样式：接管下拉箭头与复选框对勾的绘制
    app.setStyle(new UiStyle);
    app.setPalette(buildQPalette(paletteFor(mode)));

    QFont font = app.font();
    // 中文优先用系统 UI 字体，避免默认字体在中文下偏"宋体感"
    font.setFamilies({QStringLiteral("Microsoft YaHei UI"),
                      QStringLiteral("Segoe UI Variable Text"),
                      QStringLiteral("Segoe UI"),
                      QStringLiteral("PingFang SC"),
                      QStringLiteral("Noto Sans CJK SC")});
    font.setPointSizeF(9.75);
    app.setFont(font);

    app.setStyleSheet(styleSheet(mode));
}

} // namespace theme
} // namespace vf
