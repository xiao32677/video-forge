#pragma once

// =============================================================================
//  VideoForge - 单个转换任务
//
//  一个 ConvertJob = 一个 ffmpeg 进程的生命周期管理：
//      排队 -> 启动 -> 运行(进度) -> 完成/失败/取消
//  支持暂停（Windows 下挂起进程）与继续。
// =============================================================================

#include "core/CommandBuilder.h"
#include "core/MediaInfo.h"
#include "core/ProgressParser.h"

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QString>

namespace vf {

enum class JobState {
    Queued,     ///< 已加入队列，等待执行
    Running,    ///< 正在执行
    Paused,     ///< 已暂停
    Completed,  ///< 成功完成
    Failed,     ///< 失败
    Canceled,   ///< 用户取消
};

QString jobStateText(JobState state);

class ConvertJob : public QObject
{
    Q_OBJECT

public:
    ConvertJob(const QString &jobId, const MediaInfo &info,
               const ConvertPreset &preset, const ConvertOptions &options,
               const QString &ffmpegPath, QObject *parent = nullptr);
    ~ConvertJob() override;

    QString jobId() const { return m_jobId; }
    const MediaInfo &mediaInfo() const { return m_mediaInfo; }
    const ConvertPreset &preset() const { return m_preset; }
    const ConvertOptions &options() const { return m_options; }
    QString outputPath() const { return m_options.outputPath; }
    QString displayCommand() const { return m_plan.displayCommand; }
    const CommandPlan &plan() const { return m_plan; }

    JobState state() const { return m_state; }
    double progress() const { return m_progress; }        ///< 0.0 ~ 1.0
    double speed() const { return m_lastProgress.speed; }
    qint64 outputBytes() const { return m_lastProgress.totalSize; }
    double elapsedSeconds() const;
    double etaSeconds() const;                            ///< 剩余秒数；未知返回 -1
    QString errorMessage() const { return m_errorMessage; }
    QString logText() const { return m_logText; }
    QStringList warnings() const { return m_plan.warnings; }

    /// 准备阶段发现的问题（例如源文件没有视频流），此时任务不会被调度
    bool isRunnable() const { return m_plan.valid; }
    QString preflightError() const { return m_plan.errorMessage; }

    // ---- 生命周期控制（由 JobQueue 在合适时机调用）----
    void start();
    void cancel();
    void pause();
    void resume();

    /// 是否已经结束（无论成功失败）
    bool isFinished() const
    {
        return m_state == JobState::Completed || m_state == JobState::Failed
            || m_state == JobState::Canceled;
    }

signals:
    void started(const QString &jobId);
    void progressChanged(const QString &jobId, double progress, double speed, qint64 outputBytes);
    void stateChanged(const QString &jobId, vf::JobState state);
    void finished(const QString &jobId, vf::JobState state, const QString &message);
    void logAppended(const QString &jobId, const QString &text);

private slots:
    void handleStdout();
    void handleStderr();
    void handleProcessFinished(int exitCode, QProcess::ExitStatus status);
    void handleProcessError(QProcess::ProcessError error);

private:
    void setState(JobState state);
    void appendLog(const QString &text);
    void updateProgressFromParser();

    QString m_jobId;
    MediaInfo m_mediaInfo;
    ConvertPreset m_preset;
    ConvertOptions m_options;
    QString m_ffmpegPath;
    CommandPlan m_plan;

    QProcess *m_process = nullptr;
    ProgressParser m_parser;
    ProgressInfo m_lastProgress;
    JobState m_state = JobState::Queued;
    double m_progress = 0.0;
    double m_duration = 0.0;
    QString m_errorMessage;
    QString m_logText;
    QElapsedTimer m_timer;
    bool m_cancelRequested = false;
    bool m_pausedByUser = false;
    double m_pausedDurationMs = 0.0;
    qint64 m_pauseStartedAtMs = 0;
};

} // namespace vf

Q_DECLARE_METATYPE(vf::JobState)
