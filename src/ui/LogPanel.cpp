#include "ui/LogPanel.h"
#include "ui/Theme.h"

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
    if (monospace) {
        QFont font = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        font.setPointSizeF(9.0);
        edit->setFont(font);
    }
    return edit;
}

} // namespace

LogPanel::LogPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 4, 10, 10);
    layout->setSpacing(6);

    auto *headerRow = new QHBoxLayout;
    auto *title = new QLabel(QStringLiteral("运行日志 / 命令行"), this);
    title->setProperty("role", "title");
    headerRow->addWidget(title);

    m_logStatus = new QLabel(QStringLiteral("等待任务"), this);
    m_logStatus->setProperty("role", "hint");
    headerRow->addWidget(m_logStatus);
    headerRow->addStretch();

    m_autoScrollCheck = new QCheckBox(QStringLiteral("自动滚动"), this);
    m_autoScrollCheck->setChecked(true);
    headerRow->addWidget(m_autoScrollCheck);

    auto *copyLogButton = new QPushButton(QStringLiteral("复制日志"), this);
    copyLogButton->setCursor(Qt::PointingHandCursor);
    connect(copyLogButton, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_logEdit->toPlainText());
        m_logStatus->setText(QStringLiteral("日志已复制到剪贴板"));
    });
    headerRow->addWidget(copyLogButton);

    auto *copyCommandButton = new QPushButton(QStringLiteral("复制命令"), this);
    copyCommandButton->setCursor(Qt::PointingHandCursor);
    copyCommandButton->setToolTip(QStringLiteral("复制当前任务的完整 ffmpeg 命令，可自行到终端执行"));
    connect(copyCommandButton, &QPushButton::clicked, this, [this]() {
        QApplication::clipboard()->setText(m_commandEdit->toPlainText());
        m_logStatus->setText(QStringLiteral("命令行已复制到剪贴板"));
    });
    headerRow->addWidget(copyCommandButton);

    auto *clearButton = new QPushButton(QStringLiteral("清空"), this);
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
    m_tabs->addTab(m_logEdit, QStringLiteral("运行日志"));
    m_tabs->addTab(m_commandEdit, QStringLiteral("ffmpeg 命令行"));
    m_tabs->setMinimumHeight(120);

    layout->addWidget(m_tabs, 1);
}

void LogPanel::appendLog(const QString &jobId, const QString &text)
{
    if (text.isEmpty())
        return;
    const QString prefix = jobId.isEmpty() ? QString() : QStringLiteral("[%1] ").arg(jobId);
    m_logEdit->appendPlainText(prefix + text);
    if (m_autoScrollCheck->isChecked()) {
        QScrollBar *bar = m_logEdit->verticalScrollBar();
        bar->setValue(bar->maximum());
    }
    m_logStatus->setText(QStringLiteral("最后更新：%1").arg(QTime::currentTime().toString(QStringLiteral("HH:mm:ss"))));
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
