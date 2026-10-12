#pragma once

// =============================================================================
//  VideoForge - 日志 / 命令行面板
//
//  两个标签页：
//    * 运行日志：所有任务的实时输出（含 ffmpeg 的报错原文）
//    * 命令行：当前选中任务的完整 ffmpeg 命令，可一键复制到终端自己跑
//  "可复制出完整命令行"是工程可验证性的体现：界面不放黑盒。
// =============================================================================

#include <QWidget>

class QPlainTextEdit;
class QLabel;
class QCheckBox;
class QTabWidget;

namespace vf {

class LogPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LogPanel(QWidget *parent = nullptr);

    void appendLog(const QString &jobId, const QString &text);
    void appendSystemMessage(const QString &text);
    void setCommand(const QString &jobId, const QString &command);
    void clearLog();

    QString logText() const;
    QString commandText() const;

signals:
    void copyRequested(const QString &text);

private:
    QTabWidget *m_tabs = nullptr;
    QPlainTextEdit *m_logEdit = nullptr;
    QPlainTextEdit *m_commandEdit = nullptr;
    QLabel *m_logStatus = nullptr;
    QCheckBox *m_autoScrollCheck = nullptr;
};

} // namespace vf
