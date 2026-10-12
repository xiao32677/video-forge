#include "job/ConvertJob.h"
#include "core/Format.h"
#include "core/ProcessUtil.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>
#include <QtGlobal>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace vf {

namespace {

#ifdef Q_OS_WIN
using NtProcessControlFn = LONG(NTAPI *)(HANDLE);

/// 通过 ntdll 挂起/恢复整个进程（含 ffmpeg 可能创建的子线程）
bool setProcessSuspended(qint64 pid, bool suspend)
{
    if (pid <= 0)
        return false;
    HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
    if (!ntdll)
        return false;
    const char *functionName = suspend ? "NtSuspendProcess" : "NtResumeProcess";
    auto function = reinterpret_cast<NtProcessControlFn>(
        reinterpret_cast<void *>(::GetProcAddress(ntdll, functionName)));
    if (!function)
        return false;

    constexpr DWORD kProcessSuspendResume = 0x0800;
    HANDLE handle = ::OpenProcess(kProcessSuspendResume, FALSE, static_cast<DWORD>(pid));
    if (!handle)
        return false;
    function(handle);
    ::CloseHandle(handle);
    return true;
}
#endif

QString tailOf(const QString &text, int maxLines = 8)
{
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (lines.size() <= maxLines)
        return lines.join(QLatin1Char('\n')).trimmed();
    return lines.mid(lines.size() - maxLines).join(QLatin1Char('\n')).trimmed();
}

/// 把 ffmpeg 的英文报错翻译成用户能看懂的中文提示
QString friendlyError(const QString &rawError, int exitCode)
{
    const QString lower = rawError.toLower();
    if (lower.contains(QLatin1String("no space left")))
        return QStringLiteral("磁盘空间不足，请清理后重试。");
    if (lower.contains(QLatin1String("permission denied")) || lower.contains(QLatin1String("access is denied")))
        return QStringLiteral("没有写入权限，请换一个输出目录，或关闭正在占用该文件的程序。");
    if (lower.contains(QLatin1String("invalid data found")) || lower.contains(QLatin1String("moov atom not found")))
        return QStringLiteral("源文件已损坏或不完整，无法读取。");
    if (lower.contains(QLatin1String("unknown encoder")))
        return QStringLiteral("当前 ffmpeg 不支持所选编码器，请换一个预设，或更新 ffmpeg。");
    if (lower.contains(QLatin1String("does not support")) && lower.contains(QLatin1String("codec")))
        return QStringLiteral("该容器不支持所选编码组合，请改用 MKV 或换一个预设。");
    if (lower.contains(QLatin1String("cannot open")))
        return QStringLiteral("无法打开文件，请确认文件没有被其他程序占用。");
    if (rawError.isEmpty())
        return QStringLiteral("ffmpeg 异常退出（代码 %1）。").arg(exitCode);
    return QStringLiteral("ffmpeg 转换失败（代码 %1）：\n%2").arg(exitCode).arg(tailOf(rawError));
}

} // namespace

QString jobStateText(JobState state)
{
    switch (state) {
    case JobState::Queued:    return QStringLiteral("排队中");
    case JobState::Running:   return QStringLiteral("转换中");
    case JobState::Paused:    return QStringLiteral("已暂停");
    case JobState::Completed: return QStringLiteral("已完成");
    case JobState::Failed:    return QStringLiteral("失败");
    case JobState::Canceled:  return QStringLiteral("已取消");
    }
    return QStringLiteral("未知");
}

ConvertJob::ConvertJob(const QString &jobId, const MediaInfo &info,
                       const ConvertPreset &preset, const ConvertOptions &options,
                       const QString &ffmpegPath, QObject *parent)
    : QObject(parent)
    , m_jobId(jobId)
    , m_mediaInfo(info)
    , m_preset(preset)
    , m_options(options)
    , m_ffmpegPath(ffmpegPath)
{
    m_duration = options.effectiveDuration(info.durationSeconds);
    m_plan = CommandBuilder::build(ffmpegPath, info, preset, options);
}

ConvertJob::~ConvertJob()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

void ConvertJob::start()
{
    if (!m_plan.valid) {
        m_errorMessage = m_plan.errorMessage;
        setState(JobState::Failed);
        emit finished(m_jobId, JobState::Failed, m_errorMessage);
        return;
    }
    if (m_state == JobState::Running)
        return;

    // 确保输出目录存在（用户可能选了不存在的目录）
    const QFileInfo outputInfo(m_options.outputPath);
    QDir().mkpath(outputInfo.absolutePath());

    // 重试场景：回收上一次的 QProcess，避免句柄堆积
    if (m_process) {
        if (m_process->state() != QProcess::NotRunning) {
            m_process->kill();
            m_process->waitForFinished(1000);
        }
        m_process->deleteLater();
        m_process = nullptr;
    }
    m_logText.clear();

    m_process = new QProcess(this);
    suppressConsoleWindow(*m_process);
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &ConvertJob::handleStdout);
    connect(m_process, &QProcess::readyReadStandardError, this, &ConvertJob::handleStderr);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &ConvertJob::handleProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &ConvertJob::handleProcessError);

    m_parser.reset();
    m_cancelRequested = false;
    m_pausedByUser = false;
    m_pausedDurationMs = 0.0;
    m_pauseStartedAtMs = 0;
    m_progress = 0.0;
    m_timer.start();

    appendLog(QStringLiteral("$ %1").arg(m_plan.displayCommand));
    for (const QString &warning : m_plan.warnings)
        appendLog(QStringLiteral("提示：%1").arg(warning));

    m_process->start(m_plan.program, m_plan.arguments);
    setState(JobState::Running);
    emit started(m_jobId);
}

void ConvertJob::handleStdout()
{
    if (!m_process)
        return;
    const QString chunk = QString::fromUtf8(m_process->readAllStandardOutput());
    if (m_parser.feed(chunk))
        updateProgressFromParser();
}

void ConvertJob::handleStderr()
{
    if (!m_process)
        return;
    const QString text = QString::fromUtf8(m_process->readAllStandardError()).trimmed();
    if (!text.isEmpty())
        appendLog(text);
}

void ConvertJob::updateProgressFromParser()
{
    m_lastProgress = m_parser.info();

    double progress = 0.0;
    if (m_duration > 0.0) {
        progress = m_lastProgress.outTimeSeconds() / m_duration;
    } else if (m_lastProgress.finished) {
        progress = 1.0;
    }
    progress = qBound(0.0, progress, 1.0);

    // 只在变化足够大时通知界面，避免刷爆事件循环
    if (progress - m_progress >= 0.002 || progress >= 1.0) {
        m_progress = progress;
        emit progressChanged(m_jobId, m_progress, m_lastProgress.speed, m_lastProgress.totalSize);
    }
}

void ConvertJob::handleProcessFinished(int exitCode, QProcess::ExitStatus status)
{
    // 收尾：把还留在管道里的输出读干净
    if (m_process) {
        const QString remainingOut = QString::fromUtf8(m_process->readAllStandardOutput());
        if (!remainingOut.isEmpty())
            m_parser.feed(remainingOut);
        handleStderr();
    }

    if (m_cancelRequested) {
        m_errorMessage = QStringLiteral("已取消");
        setState(JobState::Canceled);
        emit finished(m_jobId, JobState::Canceled, m_errorMessage);
        return;
    }

    if (status == QProcess::CrashExit) {
        m_errorMessage = QStringLiteral("ffmpeg 进程意外终止（可能被杀毒软件拦截或内存不足）。");
        setState(JobState::Failed);
        emit finished(m_jobId, JobState::Failed, m_errorMessage);
        return;
    }

    if (exitCode == 0) {
        m_progress = 1.0;
        emit progressChanged(m_jobId, 1.0, m_lastProgress.speed, m_lastProgress.totalSize);
        setState(JobState::Completed);
        emit finished(m_jobId, JobState::Completed, QStringLiteral("转换完成"));
        return;
    }

    m_errorMessage = friendlyError(m_logText, exitCode);
    setState(JobState::Failed);
    emit finished(m_jobId, JobState::Failed, m_errorMessage);
}

void ConvertJob::handleProcessError(QProcess::ProcessError error)
{
    if (m_cancelRequested)
        return;
    if (error == QProcess::FailedToStart) {
        m_errorMessage = QStringLiteral("无法启动 ffmpeg，请检查 ffmpeg 路径是否正确：%1").arg(m_ffmpegPath);
        setState(JobState::Failed);
        emit finished(m_jobId, JobState::Failed, m_errorMessage);
    }
}

void ConvertJob::cancel()
{
    if (isFinished())
        return;
    m_cancelRequested = true;
    if (m_process && m_process->state() != QProcess::NotRunning) {
        // 先温和终止，给 ffmpeg 机会写完文件头；1.5 秒后强杀
        m_process->terminate();
        if (!m_process->waitForFinished(1500))
            m_process->kill();
    } else {
        setState(JobState::Canceled);
        emit finished(m_jobId, JobState::Canceled, QStringLiteral("已取消"));
    }
}

void ConvertJob::pause()
{
    if (m_state != JobState::Running || !m_process)
        return;
#ifdef Q_OS_WIN
    if (setProcessSuspended(m_process->processId(), true)) {
        m_pausedByUser = true;
        m_pauseStartedAtMs = m_timer.elapsed();
        setState(JobState::Paused);
        appendLog(QStringLiteral("任务已暂停"));
        return;
    }
#endif
    appendLog(QStringLiteral("当前系统不支持暂停，请使用取消。"));
}

void ConvertJob::resume()
{
    if (m_state != JobState::Paused || !m_process)
        return;
#ifdef Q_OS_WIN
    if (setProcessSuspended(m_process->processId(), false)) {
        if (m_pausedByUser && m_pauseStartedAtMs > 0)
            m_pausedDurationMs += static_cast<double>(m_timer.elapsed() - m_pauseStartedAtMs);
        m_pausedByUser = false;
        m_pauseStartedAtMs = 0;
        setState(JobState::Running);
        appendLog(QStringLiteral("任务已继续"));
    }
#endif
}

double ConvertJob::elapsedSeconds() const
{
    if (!m_timer.isValid())
        return 0.0;
    const double total = static_cast<double>(m_timer.elapsed()) - m_pausedDurationMs;
    return qMax(0.0, total / 1000.0);
}

double ConvertJob::etaSeconds() const
{
    if (m_state == JobState::Completed)
        return 0.0;
    const double elapsed = elapsedSeconds();
    if (m_progress <= 0.02 || elapsed <= 0.5)
        return -1.0;
    const double total = elapsed / m_progress;
    return qMax(0.0, total - elapsed);
}

void ConvertJob::setState(JobState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit stateChanged(m_jobId, state);
}

void ConvertJob::appendLog(const QString &text)
{
    if (text.isEmpty())
        return;
    if (!m_logText.isEmpty())
        m_logText += QLatin1Char('\n');
    m_logText += text;
    if (m_logText.size() > 200000)
        m_logText = m_logText.right(100000);
    emit logAppended(m_jobId, text);
}

} // namespace vf
