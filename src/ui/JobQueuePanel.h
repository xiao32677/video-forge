#pragma once

// =============================================================================
//  VideoForge - 任务队列面板（底部）
//
//  用表格呈现每个任务的：文件名 / 状态 / 进度 / 速度 / 剩余时间 / 输出路径
//  进度列用自绘代理（QStyledItemDelegate）画圆角进度条，比塞 QProgressBar 控件更轻。
// =============================================================================

#include <QHash>
#include <QStyledItemDelegate>
#include <QWidget>

class QTableWidget;
class QLabel;
class QPushButton;

namespace vf {

class ConvertJob;

/// 进度条绘制代理
class ProgressBarDelegate : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit ProgressBarDelegate(QObject *parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
};

class JobQueuePanel : public QWidget
{
    Q_OBJECT

public:
    explicit JobQueuePanel(QWidget *parent = nullptr);

    void addJob(ConvertJob *job);
    void updateJob(ConvertJob *job);
    void removeJob(const QString &jobId);
    void clearAll();
    void clearFinished();

    /// 当前选中的任务 id（用于日志面板显示对应命令行）
    QString selectedJobId() const;

    int rowCount() const;

signals:
    void cancelRequested(const QString &jobId);
    void pauseRequested(const QString &jobId);
    void resumeRequested(const QString &jobId);
    void retryRequested(const QString &jobId);
    void removeRequested(const QString &jobId);
    void openOutputRequested(const QString &jobId);
    void playOutputRequested(const QString &jobId);
    void selectionChanged(const QString &jobId);

private:
    int rowForJob(const QString &jobId) const;
    void refreshSummary();

    QTableWidget *m_table = nullptr;
    QLabel *m_summary = nullptr;
    QPushButton *m_clearFinishedButton = nullptr;
};

} // namespace vf
