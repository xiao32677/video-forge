#include "ui/LogPanel.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/UiWidgets.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTabWidget>
#include <QTime>
#include <QVBoxLayout>

namespace vf {

namespace {

QPlainTextEdit *makeOutput(QWidget *parent, bool monospace)
{
    auto *edit = new QPlainTextEdit(parent);
    edit->setReadOnly(true);
    edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    edit->setMaximumBlockCount(4000);
    edit->setFrameShape(QFrame::NoFrame);
    if (monospace) {
        QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        font.setPointSizeF(9.0);
        edit->setFont(font);
    } else {
        QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        font.setPointSizeF(9.0);
        edit->setFont(font);
    }
    return edit;
}

/// 给日志行按语义着色：成功/失败/命令/系统提示各有各的颜色
QString colorize(const QString &prefix, const QString &text)
{
    const QString escaped = text.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    QColor color = theme::textColor();
    bool bold = false;

    if (escaped.startsWith(QStringLiteral("✅"))) {
        color = theme::successColor();
    } else if (escaped.startsWith(QStringLiteral("❌"))) {
        color = theme::dangerColor();
    } else if (escaped.startsWith(QStringLiteral("⚠"))) {
        color = theme::warningColor();
    } else if (escaped.startsWith(QStringLiteral("$"))) {
        color = theme::accent();
    } else if (escaped.startsWith(QStringLiteral("提示")) || escaped.startsWith(QStringLiteral("已"))
               || escaped.startsWith(QStringLiteral("任务"))) {
        color = theme::mutedText();
    }

    QString html = QStringLiteral("<span style='color:%1;'>").arg(color.name());
    if (!prefix.isEmpty()) {
        html += QStringLiteral("<span style='color:%1; font-weight:600;'>%2</span>")
                    .arg(theme::mutedText().name(), prefix.toHtmlEscaped());
    }
    html += escaped;
    html += QStringLiteral("</span>");
    Q_UNUSED(bold);
    return html;
}

} // namespace

LogPanel::LogPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 4, 10, 10);
    layout->setSpacing(6);

    auto *headerRow = new QHBoxLayout;
    headerRow->setSpacing(7);
    headerRow->addWidget(new GlyphLabel(icons::Glyph::Terminal, 16, QColor(), this));
    auto *title = new QLabel(QStringLiteral("运行日志 / 命令行"), this);
    title->setProperty("role", "panelTitle");
    headerRow->addWidget(title);

    m_logStatus = new QLabel(QStringLiteral("等待任务"), this);
    m_logStatus->setProperty("role", "hint");
    headerRow->addWidget(m_logStatus);
    headerRow->addStretch();

    m_autoScrollCheck = new QCheckBox(QStringLiteral("自动滚动"), this);
    m_autoScrollCheck->setChecked(true);
    headerRow->addWidget(m_autoScrollCheck);

    auto *copyLogButton = new QPushButton(QStringLiteral("复制日志"), this);
    copyLogButton->setIcon(icons::icon(icons::Glyph::Copy, theme::mutedText(), 14));
    copyLogButton->setCursor(Qt::PointingHandCursor);
    connect(copyLogButton, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_logEdit->toPlainText());
        m_logStatus->setText(QStringLiteral("日志已复制到剪贴板"));
    });
    headerRow->addWidget(copyLogButton);

    auto *copyCommandButton = new QPushButton(QStringLiteral("复制命令"), this);
    copyCommandButton->setIcon(icons::icon(icons::Glyph::Terminal, theme::accent(), 14));
    copyCommandButton->setCursor(Qt::PointingHandCursor);
    copyCommandButton->setToolTip(QStringLiteral("复制当前任务的完整 ffmpeg 命令，可自行到终端执行"));
    connect(copyCommandButton, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_commandEdit->toPlainText());
        m_logStatus->setText(QStringLiteral("命令行已复制到剪贴板"));
    });
    headerRow->addWidget(copyCommandButton);

    auto *clearButton = new QPushButton(this);
    clearButton->setIcon(icons::icon(icons::Glyph::Trash, theme::mutedText(), 14));
    clearButton->setToolTip(QStringLiteral("清空日志"));
    clearButton->setFixedWidth(38);
    clearButton->setCursor(Qt::PointingHandCursor);
    connect(clearButton, &QPushButton::clicked, this, &LogPanel::clearLog);
    headerRow->addWidget(clearButton);

    layout->addLayout(headerRow);

    m_tabs = new QTabWidget(this);
    m_logEdit = makeOutput(m_tabs, false);
    m_logEdit->setPlaceholderText(QStringLiteral("转换开始后，这里会实时显示进度与 ffmpeg 输出"));
    m_commandEdit = makeOutput(m_tabs, true);
    m_commandEdit->setPlaceholderText(
        QStringLiteral("选中队列中的某个任务后，这里会显示它真正执行的 ffmpeg 命令"));
    m_tabs->addTab(m_logEdit, icons::icon(icons::Glyph::List, theme::mutedText(), 14),
                   QStringLiteral("运行日志"));
    m_tabs->addTab(m_commandEdit, icons::icon(icons::Glyph::Terminal, theme::mutedText(), 14),
                   QStringLiteral("ffmpeg 命令行"));
    m_tabs->setMinimumHeight(80);

    layout->addWidget(m_tabs, 1);
}

void LogPanel::appendLog(const QString &jobId, const QString &text)
{
    if (text.isEmpty())
        return;
    const QString prefix = jobId.isEmpty() ? QString() : QStringLiteral("[%1] ").arg(jobId);
    m_logEdit->appendHtml(colorize(prefix, text));
    if (m_autoScrollCheck->isChecked()) {
        QScrollBar *bar = m_logEdit->verticalScrollBar();
        bar->setValue(bar->maximum());
    }
    m_logStatus->setText(QStringLiteral("最后更新：%1")
                             .arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
}

void LogPanel::appendSystemMessage(const QString &text)
{
    appendLog(QStringLiteral("系统"), text);
}

void LogPanel::setCommand(const QString &jobId, const QString &command)
{
    m_commandEdit->setPlainText(command);
    if (!jobId.isEmpty())
        m_tabs->setTabToolTip(1, QStringLiteral("任务 %1 的命令行").arg(jobId));
}

void LogPanel::clearLog()
{
    m_logEdit->clear();
    m_commandEdit->clear();
    m_logStatus->setText(QStringLiteral("已清空"));
}

QString LogPanel::logText() const
{
    return m_logEdit->toPlainText();
}

QString LogPanel::commandText() const
{
    return m_commandEdit->toPlainText();
}

} // namespace vf
