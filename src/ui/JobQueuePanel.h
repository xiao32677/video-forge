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
enum class JobState;   // 定义在 job/ConvertJob.h，这里只需要类型名

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
    /// 只重绘某一行的可视区域。
    /// 之前每次进度回调都 update() 整个 viewport，任务一多就明显掉帧。
    void repaintRow(int row);
    /// 增量计数：state 状态进入(+1)或离开(-1)
    void countStateChange(JobState state, bool leaving);
    void recountFromTable();

    QTableWidget *m_table = nullptr;
    QLabel *m_summary = nullptr;
    QPushButton *m_clearFinishedButton = nullptr;

    // 增量维护的任务计数（避免每次进度回调都 O(n) 扫全表）
    int m_queuedCount = 0;
    int m_runningCount = 0;
    int m_completedCount = 0;
    int m_failedCount = 0;
    int m_canceledCount = 0;
};

} // namespace vf
