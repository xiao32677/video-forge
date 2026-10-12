#include "job/JobQueue.h"
#include "core/FfmpegLocator.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QUuid>

namespace vf {

JobQueue::JobQueue(FfmpegLocator *locator, QObject *parent)
    : QObject(parent)
    , m_locator(locator)
{
}

JobQueue::~JobQueue()
{
    for (ConvertJob *job : m_jobs)
        job->cancel();
}

QString JobQueue::enqueue(const MediaInfo &info, const ConvertPreset &preset, const ConvertOptions &options)
{
    const FfmpegTools tools = m_locator ? m_locator->tools() : FfmpegTools();
    const QString jobId = QStringLiteral("job-%1-%2")
                              .arg(m_nextId++)
                              .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(6));

    auto *job = new ConvertJob(jobId, info, preset, options,
                               tools.ffmpegPath, tools.ffprobePath, this);

    connect(job, &ConvertJob::stateChanged, this, [this](const QString &, JobState) {
        if (auto *changed = qobject_cast<ConvertJob *>(sender()))
            emit jobStateChanged(changed);
        emit queueChanged();
        pump();
    });
    connect(job, &ConvertJob::progressChanged, this,
            [this](const QString &, double, double, qint64) {
                if (auto *changed = qobject_cast<ConvertJob *>(sender()))
                    emit jobProgressChanged(changed);
            });
    connect(job, &ConvertJob::finished, this,
            [this](const QString &, JobState state, const QString &) {
                auto *finishedJob = qobject_cast<ConvertJob *>(sender());
                if (!finishedJob)
                    return;
                emit jobFinished(finishedJob, state);
                if (state == JobState::Canceled) {
                    // 取消的任务直接从队列移除，列表不会越堆越多
                    removeJob(finishedJob->jobId());
                }
                emit queueChanged();
                pump();
            });

    m_jobs.append(job);
    m_notifiedAllFinished = false;
    emit jobAdded(job);
    emit queueChanged();
    pump();
    return jobId;
}

ConvertJob *JobQueue::job(const QString &jobId) const
{
    for (ConvertJob *job : m_jobs) {
        if (job->jobId() == jobId)
            return job;
    }
    return nullptr;
}

void JobQueue::setMaxConcurrent(int value)
{
    m_maxConcurrent = qBound(1, value, 8);
    pump();
}

void JobQueue::startScheduling()
{
    m_scheduling = true;
    m_notifiedAllFinished = false;
    pump();
}

void JobQueue::stopScheduling()
{
    m_scheduling = false;
}

void JobQueue::pump()
{
    if (!m_scheduling)
        return;

    for (ConvertJob *job : m_jobs) {
        if (runningCount() >= m_maxConcurrent)
            break;
        if (job->state() == JobState::Queued)
            job->start();
    }

    if (!m_notifiedAllFinished && isBusy() == false && !m_jobs.isEmpty()) {
        bool hasPending = false;
        for (ConvertJob *job : m_jobs) {
            if (job->state() == JobState::Queued) {
                hasPending = true;
                break;
            }
        }
        if (!hasPending) {
            m_notifiedAllFinished = true;
            m_scheduling = false;
            emit allFinished();
        }
    }
}

bool JobQueue::isBusy() const
{
    for (ConvertJob *job : m_jobs) {
        if (job->state() == JobState::Running || job->state() == JobState::Queued
            || job->state() == JobState::Paused)
            return true;
    }
    return false;
}

int JobQueue::runningCount() const
{
    int count = 0;
    for (ConvertJob *job : m_jobs) {
        if (job->state() == JobState::Running)
            ++count;
    }
    return count;
}

int JobQueue::queuedCount() const
{
    int count = 0;
    for (ConvertJob *job : m_jobs) {
        if (job->state() == JobState::Queued)
            ++count;
    }
    return count;
}

int JobQueue::finishedCount() const
{
    int count = 0;
    for (ConvertJob *job : m_jobs) {
        if (job->state() == JobState::Completed)
            ++count;
    }
    return count;
}

int JobQueue::failedCount() const
{
    int count = 0;
    for (ConvertJob *job : m_jobs) {
        if (job->state() == JobState::Failed)
            ++count;
    }
    return count;
}

double JobQueue::overallProgress() const
{
    if (m_jobs.isEmpty())
        return 0.0;
    double sum = 0.0;
    for (ConvertJob *job : m_jobs)
        sum += job->progress();
    return sum / m_jobs.size();
}

void JobQueue::pauseAll()
{
    for (ConvertJob *job : m_jobs) {
        if (job->state() == JobState::Running)
            job->pause();
    }
}

void JobQueue::resumeAll()
{
    for (ConvertJob *job : m_jobs) {
        if (job->state() == JobState::Paused)
            job->resume();
    }
}

void JobQueue::cancelAll()
{
    for (ConvertJob *job : m_jobs) {
        if (!job->isFinished())
            job->cancel();
    }
}

void JobQueue::cancelJob(const QString &jobId)
{
    if (ConvertJob *job = this->job(jobId))
        job->cancel();
}

void JobQueue::pauseJob(const QString &jobId)
{
    if (ConvertJob *job = this->job(jobId))
        job->pause();
}

void JobQueue::resumeJob(const QString &jobId)
{
    if (ConvertJob *job = this->job(jobId))
        job->resume();
}

void JobQueue::retryJob(const QString &jobId)
{
    ConvertJob *job = this->job(jobId);
    if (!job || !job->isFinished())
        return;
    // 若输出文件存在且是上一次失败留下的半成品，先清掉
    const QString output = job->outputPath();
    if (QFile::exists(output) && job->state() == JobState::Failed)
        QFile::remove(output);

    m_scheduling = true;
    m_notifiedAllFinished = false;
    job->start();
}

void JobQueue::removeJob(const QString &jobId)
{
    int index = -1;
    for (int i = 0; i < m_jobs.size(); ++i) {
        if (m_jobs.at(i)->jobId() == jobId) {
            index = i;
            break;
        }
    }
    if (index < 0)
        return;

    // 关键：先把任务从列表里摘出去，再 cancel()。
    // cancel() 对"排队中"的任务是同步发出 finished 信号的，而 finished 的处理器
    // 又会回调 removeJob —— 如果不先摘出，就会对同一个位置 removeAt 两次，
    // 造成越界访问（曾经真的在这里崩过一次）。
    ConvertJob *job = m_jobs.takeAt(index);
    if (!job->isFinished())
        job->cancel();
    job->deleteLater();

    emit jobRemoved(jobId);
    emit queueChanged();
}

void JobQueue::clearJobs()
{
    // 同样先清列表再取消，避免 cancel 同步触发的 removeJob 重入
    const QList<ConvertJob *> snapshot = m_jobs;
    m_jobs.clear();
    for (ConvertJob *job : snapshot) {
        if (!job->isFinished())
            job->cancel();
        job->deleteLater();
    }
    emit queueChanged();
}

void JobQueue::clearFinished()
{
    for (int i = m_jobs.size() - 1; i >= 0; --i) {
        ConvertJob *job = m_jobs.at(i);
        if (!job->isFinished())
            continue;
        m_jobs.removeAt(i);
        job->deleteLater();
    }
    emit queueChanged();
}

} // namespace vf
