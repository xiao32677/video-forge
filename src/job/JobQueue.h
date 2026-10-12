#pragma once

// =============================================================================
//  VideoForge - 转换任务队列（调度器）
//
//  职责：
//    * 维护任务列表与状态机
//    * 按并发上限（默认 2）自动派发排队中的任务
//    * 全局暂停 / 继续 / 取消 / 重试
//    * 统计信息（完成数、失败数、总进度）供状态栏使用
// =============================================================================

#include "job/ConvertJob.h"

#include <QList>
#include <QObject>
#include <QString>

namespace vf {

class FfmpegLocator;

class JobQueue : public QObject
{
    Q_OBJECT

public:
    explicit JobQueue(FfmpegLocator *locator, QObject *parent = nullptr);
    ~JobQueue() override;

    /// 加入一个任务；返回任务 id。若计划无效（例如源文件没有视频流）任务仍会创建，
    /// 但会在调度时立即失败并给出原因。
    QString enqueue(const MediaInfo &info, const ConvertPreset &preset, const ConvertOptions &options);

    ConvertJob *job(const QString &jobId) const;
    QList<ConvertJob *> jobs() const { return m_jobs; }
    int count() const { return m_jobs.size(); }

    int maxConcurrent() const { return m_maxConcurrent; }
    void setMaxConcurrent(int value);

    /// 开始/继续调度（会立刻派发）
    void startScheduling();
    void stopScheduling();

    void pauseAll();
    void resumeAll();
    void cancelAll();
    void cancelJob(const QString &jobId);
    void pauseJob(const QString &jobId);
    void resumeJob(const QString &jobId);
    void retryJob(const QString &jobId);
    void removeJob(const QString &jobId);
    void clearJobs();
    void clearFinished();

    bool isBusy() const;
    int runningCount() const;
    int queuedCount() const;
    int finishedCount() const;
    int failedCount() const;

    /// 整体进度（0.0 ~ 1.0），按任务数平均
    double overallProgress() const;

signals:
    void jobAdded(vf::ConvertJob *job);
    void jobStateChanged(vf::ConvertJob *job);
    void jobProgressChanged(vf::ConvertJob *job);
    void jobFinished(vf::ConvertJob *job, vf::JobState state);
    void jobRemoved(const QString &jobId);
    void queueChanged();
    void allFinished();

private:
    void pump();

    FfmpegLocator *m_locator = nullptr;
    QList<ConvertJob *> m_jobs;
    int m_maxConcurrent = 2;
    bool m_scheduling = false;
    int m_nextId = 1;
    bool m_notifiedAllFinished = true;
};

} // namespace vf
