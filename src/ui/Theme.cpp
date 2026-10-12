#include "ui/Theme.h"

#include <QApplication>
#include <QFont>
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
    QString border;
    QString hover;
    QString muted;
    QString accent;
    QString accentHover;
    QString accentText;
    QString disabled;
};

Palette paletteFor(Mode mode)
{
    Palette p;
    if (mode == Mode::Dark) {
        p.window        = QStringLiteral("#1B1D21");
        p.windowText    = QStringLiteral("#E6E8EB");
        p.base          = QStringLiteral("#141518");
        p.alternateBase = QStringLiteral("#1F2125");
        p.surface       = QStringLiteral("#232629");
        p.border        = QStringLiteral("#33373D");
        p.hover         = QStringLiteral("#2C3036");
        p.muted         = QStringLiteral("#8B939C");
        p.accent        = QStringLiteral("#4C8DFF");
        p.accentHover   = QStringLiteral("#6BA1FF");
        p.accentText    = QStringLiteral("#FFFFFF");
        p.disabled      = QStringLiteral("#5A6068");
    } else {
        p.window        = QStringLiteral("#F4F6F9");
        p.windowText    = QStringLiteral("#1D2126");
        p.base          = QStringLiteral("#FFFFFF");
        p.alternateBase = QStringLiteral("#F7F9FC");
        p.surface       = QStringLiteral("#FFFFFF");
        p.border        = QStringLiteral("#D8DEE6");
        p.hover         = QStringLiteral("#E9EEF5");
        p.muted         = QStringLiteral("#69707A");
        p.accent        = QStringLiteral("#2F6FED");
        p.accentHover   = QStringLiteral("#4A85F5");
        p.accentText    = QStringLiteral("#FFFFFF");
        p.disabled      = QStringLiteral("#A8AEB6");
    }
    return p;
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
    palette.setColor(QPalette::ToolTipBase, QColor(p.surface));
    palette.setColor(QPalette::ToolTipText, QColor(p.windowText));
    palette.setColor(QPalette::Highlight, QColor(p.accent));
    palette.setColor(QPalette::HighlightedText, QColor(p.accentText));
    palette.setColor(QPalette::PlaceholderText, QColor(p.muted));
    palette.setColor(QPalette::Link, QColor(p.accent));
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

QColor accent()
{
    return QColor(paletteFor(g_mode).accent);
}

QColor mutedText()
{
    return QColor(paletteFor(g_mode).muted);
}

QColor cardBackground()
{
    return QColor(paletteFor(g_mode).surface);
}

QColor successColor()
{
    return QColor(QStringLiteral("#2FA84F"));
}

QColor warningColor()
{
    return QColor(QStringLiteral("#E08A17"));
}

QColor dangerColor()
{
    return QColor(QStringLiteral("#E44A4A"));
}

QString styleSheet(Mode mode)
{
    const Palette p = paletteFor(mode);
    QString qss = QStringLiteral(R"QSS(
* { outline: none; }

QWidget {
    font-size: 13px;
    color: %TEXT%;
}

QMainWindow, QDialog {
    background: %WINDOW%;
}

QToolTip {
    background: %SURFACE%;
    color: %TEXT%;
    border: 1px solid %BORDER%;
    padding: 6px 8px;
    border-radius: 6px;
}

/* ---------------------------------------------------------------- 菜单栏 */
QMenuBar {
    background: %WINDOW%;
    border-bottom: 1px solid %BORDER%;
    padding: 2px 6px;
}
QMenuBar::item {
    padding: 6px 12px;
    border-radius: 6px;
    background: transparent;
}
QMenuBar::item:selected { background: %HOVER%; }
QMenuBar::item:pressed  { background: %ACCENT%; color: %ACCENT_TEXT%; }

QMenu {
    background: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 6px;
}
QMenu::item {
    padding: 7px 26px 7px 14px;
    border-radius: 6px;
}
QMenu::item:selected { background: %ACCENT%; color: %ACCENT_TEXT%; }
QMenu::item:disabled { color: %MUTED%; }
QMenu::separator { height: 1px; background: %BORDER%; margin: 6px 10px; }

/* ---------------------------------------------------------------- 工具栏 */
QToolBar {
    background: %WINDOW%;
    border-bottom: 1px solid %BORDER%;
    padding: 6px 8px;
    spacing: 4px;
}
QToolBar::separator {
    width: 1px;
    background: %BORDER%;
    margin: 4px 8px;
}
QToolButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 7px;
    padding: 6px 12px;
    color: %TEXT%;
}
QToolButton:hover   { background: %HOVER%; border-color: %BORDER%; }
QToolButton:pressed { background: %ACCENT%; color: %ACCENT_TEXT%; }
QToolButton:checked { background: %ACCENT%; color: %ACCENT_TEXT%; }
QToolButton:disabled { color: %MUTED%; }

/* ---------------------------------------------------------------- 按钮 */
QPushButton {
    background: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 7px;
    padding: 7px 16px;
    min-height: 18px;
}
QPushButton:hover   { background: %HOVER%; border-color: %ACCENT%; }
QPushButton:pressed { background: %ACCENT%; color: %ACCENT_TEXT%; border-color: %ACCENT%; }
QPushButton:disabled { color: %MUTED%; border-color: %BORDER%; background: %WINDOW%; }

QPushButton[accent="true"] {
    background: %ACCENT%;
    color: %ACCENT_TEXT%;
    border: 1px solid %ACCENT%;
    font-weight: 600;
}
QPushButton[accent="true"]:hover   { background: %ACCENT_HOVER%; }
QPushButton[accent="true"]:disabled { background: %BORDER%; border-color: %BORDER%; color: %MUTED%; }

QPushButton[danger="true"]:hover { border-color: %DANGER%; color: %DANGER%; }

/* ---------------------------------------------------------------- 输入控件 */
QLineEdit, QPlainTextEdit, QTextEdit, QSpinBox, QDoubleSpinBox, QComboBox, QTimeEdit {
    background: %BASE%;
    border: 1px solid %BORDER%;
    border-radius: 7px;
    padding: 6px 9px;
    selection-background-color: %ACCENT%;
    selection-color: %ACCENT_TEXT%;
}
QLineEdit:focus, QPlainTextEdit:focus, QTextEdit:focus,
QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus, QTimeEdit:focus {
    border-color: %ACCENT%;
}
QLineEdit:disabled, QComboBox:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled {
    color: %MUTED%;
    background: %WINDOW%;
}

QComboBox::drop-down {
    border: none;
    width: 22px;
}
QComboBox QAbstractItemView {
    background: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 8px;
    padding: 4px;
    selection-background-color: %ACCENT%;
    selection-color: %ACCENT_TEXT%;
    outline: none;
}

QSpinBox::up-button, QSpinBox::down-button,
QDoubleSpinBox::up-button, QDoubleSpinBox::down-button {
    width: 16px;
    border: none;
    background: transparent;
}

/* ---------------------------------------------------------------- 滑块 */
QSlider::groove:horizontal {
    height: 5px;
    border-radius: 3px;
    background: %BORDER%;
}
QSlider::sub-page:horizontal {
    height: 5px;
    border-radius: 3px;
    background: %ACCENT%;
}
QSlider::handle:horizontal {
    width: 15px;
    height: 15px;
    margin: -6px 0;
    border-radius: 8px;
    background: %SURFACE%;
    border: 2px solid %ACCENT%;
}
QSlider::handle:horizontal:hover { background: %ACCENT%; }

/* ---------------------------------------------------------------- 列表与表格 */
QListWidget, QTreeWidget, QTableWidget, QListView, QTreeView, QTableView {
    background: %BASE%;
    border: 1px solid %BORDER%;
    border-radius: 9px;
    alternate-background-color: %ALT%;
}
QListWidget::item, QTreeWidget::item {
    padding: 6px 8px;
    border-radius: 6px;
    margin: 1px 3px;
}
QListWidget::item:hover, QTreeWidget::item:hover { background: %HOVER%; }
QListWidget::item:selected, QTreeWidget::item:selected {
    background: %ACCENT%;
    color: %ACCENT_TEXT%;
}

QHeaderView::section {
    background: %SURFACE%;
    color: %MUTED%;
    border: none;
    border-bottom: 1px solid %BORDER%;
    padding: 7px 8px;
    font-weight: 600;
}
QTableWidget::item, QTableView::item { padding: 4px 6px; }
QTableCornerButton::section { background: %SURFACE%; border: none; }

/* ---------------------------------------------------------------- 标签页 */
QTabWidget::pane {
    border: 1px solid %BORDER%;
    border-radius: 9px;
    background: %SURFACE%;
    top: -1px;
}
QTabBar::tab {
    background: transparent;
    color: %MUTED%;
    padding: 7px 14px;
    margin-right: 3px;
    border: 1px solid transparent;
    border-top-left-radius: 8px;
    border-top-right-radius: 8px;
}
QTabBar::tab:hover { color: %TEXT%; background: %HOVER%; }
QTabBar::tab:selected {
    color: %TEXT%;
    background: %SURFACE%;
    border-color: %BORDER%;
    border-bottom-color: %SURFACE%;
    font-weight: 600;
}

/* ---------------------------------------------------------------- 分组框 */
QGroupBox {
    border: 1px solid %BORDER%;
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

/* ---------------------------------------------------------------- 分隔条 */
QSplitter::handle { background: %BORDER%; }
QSplitter::handle:horizontal { width: 2px; }
QSplitter::handle:vertical { height: 2px; }
QSplitter::handle:hover { background: %ACCENT%; }

/* ---------------------------------------------------------------- 滚动条 */
QScrollBar:vertical {
    background: transparent;
    width: 11px;
    margin: 2px;
}
QScrollBar::handle:vertical {
    background: %BORDER%;
    border-radius: 5px;
    min-height: 34px;
}
QScrollBar::handle:vertical:hover { background: %MUTED%; }
QScrollBar:horizontal {
    background: transparent;
    height: 11px;
    margin: 2px;
}
QScrollBar::handle:horizontal {
    background: %BORDER%;
    border-radius: 5px;
    min-width: 34px;
}
QScrollBar::handle:horizontal:hover { background: %MUTED%; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }

/* ---------------------------------------------------------------- 进度条 */
QProgressBar {
    background: %BASE%;
    border: 1px solid %BORDER%;
    border-radius: 7px;
    text-align: center;
    height: 18px;
    color: %TEXT%;
}
QProgressBar::chunk {
    background: %ACCENT%;
    border-radius: 6px;
}

/* ---------------------------------------------------------------- 状态栏 */
QStatusBar {
    background: %WINDOW%;
    border-top: 1px solid %BORDER%;
    color: %MUTED%;
}
QStatusBar::item { border: none; }

/* ---------------------------------------------------------------- 复选框 */
QCheckBox, QRadioButton { spacing: 7px; padding: 3px 0; }
QCheckBox::indicator, QRadioButton::indicator {
    width: 16px;
    height: 16px;
    border: 1px solid %BORDER%;
    background: %BASE%;
}
QCheckBox::indicator { border-radius: 5px; }
QRadioButton::indicator { border-radius: 9px; }
QCheckBox::indicator:hover, QRadioButton::indicator:hover { border-color: %ACCENT%; }
QCheckBox::indicator:checked, QRadioButton::indicator:checked {
    background: %ACCENT%;
    border-color: %ACCENT%;
}
QCheckBox::indicator:disabled, QRadioButton::indicator:disabled { background: %WINDOW%; }

/* ---------------------------------------------------------------- 自定义对象 */
QLabel[role="hint"]   { color: %MUTED%; font-size: 12px; }
QLabel[role="title"]  { font-size: 15px; font-weight: 700; }
QLabel[role="badge"]  {
    background: %ACCENT%;
    color: %ACCENT_TEXT%;
    border-radius: 9px;
    padding: 2px 10px;
    font-size: 11px;
    font-weight: 600;
}
QLabel[role="ok"]     { color: %SUCCESS%; }
QLabel[role="warn"]   { color: %WARN%; }
QLabel[role="error"]  { color: %DANGER%; }
QLabel[role="card"] {
    background: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 10px;
}

QFrame[role="card"] {
    background: %SURFACE%;
    border: 1px solid %BORDER%;
    border-radius: 10px;
}
QFrame[role="separator"] { background: %BORDER%; max-height: 1px; }

/* 空状态占位 */
QLabel[role="empty"] {
    color: %MUTED%;
    font-size: 14px;
}
)QSS");

    struct Token { const char *key; QString value; };
    // 顺序很重要：长 key 必须先替换，否则 %TEXT% 会先把 %ACCENT_TEXT% 拆坏
    const Token tokens[] = {
        {"%ACCENT_TEXT%",  p.accentText},
        {"%ACCENT_HOVER%", p.accentHover},
        {"%ACCENT%",       p.accent},
        {"%SUCCESS%",      QStringLiteral("#3FBF63")},
        {"%WARN%",         QStringLiteral("#E8A33D")},
        {"%DANGER%",       QStringLiteral("#FF6B6B")},
        {"%TEXT%",         p.windowText},
        {"%WINDOW%",       p.window},
        {"%SURFACE%",      p.surface},
        {"%BASE%",         p.base},
        {"%ALT%",          p.alternateBase},
        {"%BORDER%",       p.border},
        {"%HOVER%",        p.hover},
        {"%MUTED%",        p.muted},
    };
    for (const Token &token : tokens)
        qss.replace(QString::fromLatin1(token.key), token.value);
    return qss;
}

void apply(QApplication &app, Mode mode)
{
    setCurrentMode(mode);
    app.setStyle(QStringLiteral("Fusion"));
    app.setPalette(buildQPalette(paletteFor(mode)));

    QFont font = app.font();
    font.setPointSizeF(9.75);
    app.setFont(font);

    app.setStyleSheet(styleSheet(mode));
}

} // namespace theme
} // namespace vf
