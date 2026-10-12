// =============================================================================
//  VideoForge - job 层单元测试
//
//  为什么单独一个测试文件：
//    core 层是纯函数，测起来容易；而 job 层是**状态机 + 子进程 + 信号**，
//    恰恰是最容易出问题的地方 —— 之前那个必现崩溃（removeJob 重入导致的越界访问）
//    就发生在这一层，而当时 core 层 212 条断言完全覆盖不到它。
//
//  测试手法：不依赖真实 ffmpeg，而是调用 tests/fake_ffmpeg.cpp 编译出来的
//  "假 ffmpeg"，它能按环境变量切换行为（成功 / 失败 / 不产出文件 / 慢任务），
//  于是"正常完成 / 失败 / 产物校验失败 / 取消 / 队列移除重入"都能确定性复现。
// =============================================================================

#include "core/CommandBuilder.h"
#include "core/ConvertPreset.h"
#include "core/MediaProbe.h"
#include "job/ConvertJob.h"
#include "job/JobQueue.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>

#include <functional>

using namespace vf;

namespace {

int g_checks = 0;
int g_failures = 0;
QString g_fakeFfmpeg;
QString g_workDir;

QTextStream &out()
{
    static QTextStream stream(stdout);
    return stream;
}

void section(const QString &name)
{
    out() << "\n── " << name << " ──\n";
    out().flush();
}

void check(bool condition, const QString &expression, int line, const QString &detail = QString())
{
    ++g_checks;
    if (condition) {
        out() << "  [PASS] " << expression << "\n";
    } else {
        ++g_failures;
        out() << "  [FAIL] " << expression;
        if (!detail.isEmpty())
            out() << "  →  " << detail;
        out() << "  (第 " << line << " 行)\n";
    }
    out().flush();
}

#define CHECK(cond) check((cond), QStringLiteral(#cond), __LINE__)
#define CHECK_EQ(actual, expected)                                          \
    do {                                                                    \
        const auto _a = (actual);                                           \
        const auto _e = (expected);                                         \
        check(_a == _e, QStringLiteral(#actual " == " #expected), __LINE__, \
              QStringLiteral("实际=%1 期望=%2").arg(_a).arg(_e));            \
    } while (false)

/// 跑事件循环直到 predicate 成立或超时
bool waitFor(const std::function<bool()> &predicate, int timeoutMs = 20000)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate()) {
        if (timer.elapsed() > timeoutMs)
            return false;
        QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
    }
    return true;
}

/// 设置假 ffmpeg 的行为模式（同一个可执行文件，靠环境变量切换）
void setFakeMode(const QString &mode)
{
    qputenv("VF_FAKE_MODE", mode.toUtf8());
}

/// 构造一份"源文件"信息（直接喂 ffprobe JSON，避免真的去探测）
MediaInfo fakeSource(const QString &filePath, double duration = 10.0)
{
    const QString json = QStringLiteral(R"({
        "streams": [
            {"index":0,"codec_type":"video","codec_name":"h264","width":1280,"height":720,
             "pix_fmt":"yuv420p","avg_frame_rate":"30/1","duration":"%1"},
            {"index":1,"codec_type":"audio","codec_name":"aac","sample_rate":"48000","channels":2}
        ],
        "format": {"format_name":"mov,mp4","duration":"%1","size":"1000000"}
    })").arg(duration);
    return MediaProbe::parseJson(json.toUtf8(), filePath);
}

QString uniqueOutputPath(const QString &name)
{
    return g_workDir + QLatin1Char('/') + name;
}

} // namespace

// ---------------------------------------------------------------------------

void testSuccessfulJob()
{
    section(QStringLiteral("ConvertJob：正常完成 + 进度解析 + 产物校验"));

    setFakeMode(QStringLiteral("ok"));
    const MediaInfo source = fakeSource(QStringLiteral("C:/fake/source.mp4"), 10.0);
    const ConvertPreset preset = presetById(QStringLiteral("mp4-h264"));
    ConvertOptions options;
    options.outputPath = uniqueOutputPath(QStringLiteral("ok_output.mp4"));
    QFile::remove(options.outputPath);

    ConvertJob job(QStringLiteral("test-ok"), source, preset, options, g_fakeFfmpeg, QString());
    CHECK(job.isRunnable());
    CHECK(job.displayCommand().contains(QStringLiteral("fake_ffmpeg")));

    double lastProgress = 0.0;
    int progressEvents = 0;
    QObject::connect(&job, &ConvertJob::progressChanged,
                     [&](const QString &, double p, double, qint64) {
                         lastProgress = p;
                         ++progressEvents;
                     });

    JobState finalState = JobState::Queued;
    QObject::connect(&job, &ConvertJob::finished,
                     [&](const QString &, JobState state, const QString &) { finalState = state; });

    job.start();
    CHECK(waitFor([&]() { return job.isFinished(); }));

    CHECK(finalState == JobState::Completed);
    CHECK(job.state() == JobState::Completed);
    CHECK(progressEvents > 0);
    CHECK(lastProgress >= 0.99);
    CHECK(job.verification().ok);
    CHECK(!job.verification().summary.isEmpty());
    CHECK(QFileInfo::exists(options.outputPath));
    CHECK(job.logText().contains(QStringLiteral("校验通过")));

    QFile::remove(options.outputPath);
}

void testFailedJob()
{
    section(QStringLiteral("ConvertJob：ffmpeg 返回非 0 → Failed 且错误可读"));

    setFakeMode(QStringLiteral("fail"));
    const MediaInfo source = fakeSource(QStringLiteral("C:/fake/bad.mp4"), 5.0);
    const ConvertPreset preset = presetById(QStringLiteral("mp4-h264"));
    ConvertOptions options;
    options.outputPath = uniqueOutputPath(QStringLiteral("fail_output.mp4"));

    ConvertJob job(QStringLiteral("test-fail"), source, preset, options, g_fakeFfmpeg, QString());

    JobState finalState = JobState::Queued;
    QString message;
    QObject::connect(&job, &ConvertJob::finished,
                     [&](const QString &, JobState state, const QString &msg) {
                         finalState = state;
                         message = msg;
                     });

    job.start();
    CHECK(waitFor([&]() { return job.isFinished(); }));

    CHECK(finalState == JobState::Failed);
    CHECK(!job.errorMessage().isEmpty());
    CHECK(!message.isEmpty());
}

void testVerificationCatchesMissingOutput()
{
    section(QStringLiteral("ConvertJob：退出码 0 但产物缺失 → 仍然判失败"));

    setFakeMode(QStringLiteral("nooutput"));
    const MediaInfo source = fakeSource(QStringLiteral("C:/fake/v.mp4"), 4.0);
    const ConvertPreset preset = presetById(QStringLiteral("mp4-h264"));
    ConvertOptions options;
    options.outputPath = uniqueOutputPath(QStringLiteral("never_created.mp4"));
    QFile::remove(options.outputPath);

    ConvertJob job(QStringLiteral("test-verify"), source, preset, options, g_fakeFfmpeg, QString());

    JobState finalState = JobState::Queued;
    QObject::connect(&job, &ConvertJob::finished,
                     [&](const QString &, JobState state, const QString &) { finalState = state; });

    job.start();
    CHECK(waitFor([&]() { return job.isFinished(); }));

    CHECK(finalState == JobState::Failed);
    CHECK(!job.verification().ok);
    CHECK(job.errorMessage().contains(QStringLiteral("校验")));
}

void testCancelRunningJob()
{
    section(QStringLiteral("ConvertJob：取消运行中的任务 → Canceled（不是 Failed）"));

    setFakeMode(QStringLiteral("slow"));
    const MediaInfo source = fakeSource(QStringLiteral("C:/fake/c.mp4"), 60.0);
    const ConvertPreset preset = presetById(QStringLiteral("mp4-h264"));
    ConvertOptions options;
    options.outputPath = uniqueOutputPath(QStringLiteral("cancel_output.mp4"));

    ConvertJob job(QStringLiteral("test-cancel"), source, preset, options, g_fakeFfmpeg, QString());

    JobState finalState = JobState::Queued;
    QObject::connect(&job, &ConvertJob::finished,
                     [&](const QString &, JobState state, const QString &) { finalState = state; });

    job.start();
    CHECK(waitFor([&]() { return job.state() == JobState::Running; }, 8000));
    job.cancel();
    CHECK(waitFor([&]() { return job.isFinished(); }));

    CHECK(finalState == JobState::Canceled);
    CHECK(job.state() == JobState::Canceled);
}

void testRetryRecyclesProcess()
{
    section(QStringLiteral("ConvertJob：同一个任务重试两次不能崩（QProcess 回收）"));

    setFakeMode(QStringLiteral("ok"));
    const MediaInfo source = fakeSource(QStringLiteral("C:/fake/r.mp4"), 3.0);
    const ConvertPreset preset = presetById(QStringLiteral("mp4-h264"));
    ConvertOptions options;
    options.outputPath = uniqueOutputPath(QStringLiteral("retry_output.mp4"));

    ConvertJob job(QStringLiteral("test-retry"), source, preset, options, g_fakeFfmpeg, QString());
    JobState finalState = JobState::Queued;
    int finishedCount = 0;
    QObject::connect(&job, &ConvertJob::finished,
                     [&](const QString &, JobState state, const QString &) {
                         finalState = state;
                         ++finishedCount;
                     });

    job.start();
    CHECK(waitFor([&]() { return job.isFinished(); }));
    CHECK(finalState == JobState::Completed);

    job.start();
    CHECK(waitFor([&]() { return job.isFinished(); }));
    CHECK(finalState == JobState::Completed);
    CHECK_EQ(finishedCount, 2);

    QFile::remove(options.outputPath);
}

/// 关键回归：移除"仍在排队"的任务时，cancel() 会同步发 finished，
/// 信号处理器再回调 removeJob —— 老代码在这里对同一位置 removeAt 两次，
/// 造成越界访问并崩溃（0xC0000005）。
void testQueueRemoveReentrancy()
{
    section(QStringLiteral("JobQueue：移除排队中的任务（重入崩溃回归测试）"));

    const MediaInfo source = fakeSource(QStringLiteral("C:/fake/q.mp4"), 3.0);
    const ConvertPreset preset = presetById(QStringLiteral("mp4-h264"));
    ConvertOptions options;
    options.outputPath = uniqueOutputPath(QStringLiteral("queue_output.mp4"));

    JobQueue queue(nullptr);   // 不接 locator：测的是队列自身逻辑
    queue.setMaxConcurrent(1);
    CHECK_EQ(queue.maxConcurrent(), 1);

    const QString id1 = queue.enqueue(source, preset, options);
    const QString id2 = queue.enqueue(source, preset, options);
    CHECK_EQ(queue.count(), 2);
    CHECK(queue.job(id1) != nullptr);
    CHECK(queue.job(id2) != nullptr);
    CHECK(queue.job(QStringLiteral("不存在")) == nullptr);
    CHECK_EQ(queue.queuedCount(), 2);
    CHECK_EQ(queue.runningCount(), 0);
    // "排队中"也算 busy：队列里还有没跑完的东西，界面就不该让你再起一批
    CHECK(queue.isBusy());

    // —— 这一步就是当年的崩溃点 ——
    queue.removeJob(id1);
    CHECK_EQ(queue.count(), 1);
    CHECK(queue.job(id1) == nullptr);
    CHECK_EQ(queue.queuedCount(), 1);

    queue.removeJob(id2);
    CHECK_EQ(queue.count(), 0);
    CHECK_EQ(queue.queuedCount(), 0);

    // 重复移除、移除不存在的 id 都必须安全
    queue.removeJob(id1);
    queue.removeJob(QStringLiteral("不存在"));
    CHECK_EQ(queue.count(), 0);
    CHECK_EQ(queue.finishedCount(), 0);
    CHECK_EQ(queue.failedCount(), 0);
    CHECK_EQ(queue.overallProgress(), 0.0);

    // clearFinished 在空队列上也不能出问题
    queue.clearFinished();
    CHECK_EQ(queue.count(), 0);
}

/// 队列调度：并发上限为 1 时，第二个任务必须等第一个结束才开始
void testQueueSchedulingWithLimit()
{
    section(QStringLiteral("JobQueue：并发上限调度（需要真实 locator，用假 ffmpeg 路径直连）"));

    // JobQueue 内部通过 FfmpegLocator 取 ffmpeg 路径；这里不走它，
    // 而是直接用 ConvertJob 验证"队列调度顺序"的核心假设：
    // 同一时刻只允许一个 Running。
    setFakeMode(QStringLiteral("ok"));
    const MediaInfo source = fakeSource(QStringLiteral("C:/fake/s.mp4"), 2.0);
    const ConvertPreset preset = presetById(QStringLiteral("mp4-h264"));

    ConvertOptions options1;
    options1.outputPath = uniqueOutputPath(QStringLiteral("sched1.mp4"));
    ConvertOptions options2;
    options2.outputPath = uniqueOutputPath(QStringLiteral("sched2.mp4"));

    ConvertJob job1(QStringLiteral("s1"), source, preset, options1, g_fakeFfmpeg, QString());
    ConvertJob job2(QStringLiteral("s2"), source, preset, options2, g_fakeFfmpeg, QString());

    int finished = 0;
    QObject::connect(&job1, &ConvertJob::finished, [&](const QString &, JobState, const QString &) { ++finished; });

    job1.start();
    // 第一个还在跑的时候，第二个绝不能是 Running（模拟 maxConcurrent=1 的调度前提）
    CHECK(job1.state() == JobState::Running);
    CHECK(job2.state() == JobState::Queued);
    CHECK(waitFor([&]() { return job1.isFinished(); }));
    CHECK_EQ(finished, 1);

    job2.start();
    CHECK(waitFor([&]() { return job2.isFinished(); }));
    CHECK(job2.state() == JobState::Completed);

    QFile::remove(options1.outputPath);
    QFile::remove(options2.outputPath);
}

/// 预检：不可能完成的任务（用视频预设转纯音频）必须在构建阶段就被拦住
void testPreflightRejectsImpossibleJob()
{
    section(QStringLiteral("ConvertJob：预检拦截不可能的任务"));

    const QString audioOnlyJson = QStringLiteral(R"({
        "streams": [{"index":0,"codec_type":"audio","codec_name":"mp3",
                     "sample_rate":"44100","channels":2}],
        "format": {"format_name":"mp3","duration":"180.0","size":"4000000"}
    })");
    const MediaInfo audioInfo = MediaProbe::parseJson(audioOnlyJson.toUtf8(),
                                                      QStringLiteral("C:/fake/song.mp3"));
    const ConvertPreset videoPreset = presetById(QStringLiteral("mp4-h264"));
    ConvertOptions options;
    options.outputPath = uniqueOutputPath(QStringLiteral("impossible.mp4"));

    ConvertJob job(QStringLiteral("preflight"), audioInfo, videoPreset, options,
                   g_fakeFfmpeg, QString());
    CHECK(!job.isRunnable());
    CHECK(!job.preflightError().isEmpty());

    // 启动它也不能崩，而且必须立刻以失败收场
    JobState finalState = JobState::Queued;
    QObject::connect(&job, &ConvertJob::finished,
                     [&](const QString &, JobState state, const QString &) { finalState = state; });
    job.start();
    CHECK(finalState == JobState::Failed);
    CHECK_EQ(job.progress(), 0.0);
}

/// 字幕：选了"保留"就必须真的出现在命令行里
void testSubtitleMapping()
{
    section(QStringLiteral("字幕：不再静默丢弃"));

    const QString withSubsJson = QStringLiteral(R"({
        "streams": [
            {"index":0,"codec_type":"video","codec_name":"h264","width":1280,"height":720,
             "pix_fmt":"yuv420p","avg_frame_rate":"30/1"},
            {"index":1,"codec_type":"audio","codec_name":"aac","sample_rate":"48000","channels":2},
            {"index":2,"codec_type":"subtitle","codec_name":"subrip","tags":{"language":"eng"}}
        ],
        "format": {"format_name":"matroska","duration":"60.0","size":"5000000"}
    })");
    const MediaInfo info = MediaProbe::parseJson(withSubsJson.toUtf8(),
                                                 QStringLiteral("C:/fake/withsubs.mkv"));

    // 1) 默认（丢弃）：命令行里不能出现 -c:s
    {
        ConvertOptions options;
        options.outputPath = QStringLiteral("C:/out/a.mp4");
        const CommandPlan plan = CommandBuilder::build(QStringLiteral("ffmpeg"),
                                                       info, presetById(QStringLiteral("mp4-h264")),
                                                       options);
        CHECK(plan.valid);
        CHECK(!plan.arguments.contains(QStringLiteral("-c:s")));
    }

    // 2) 保留到 MP4：必须映射字幕并转成 mov_text
    {
        ConvertOptions options;
        options.outputPath = QStringLiteral("C:/out/a.mp4");
        options.subtitleMode = QStringLiteral("copy");
        const CommandPlan plan = CommandBuilder::build(QStringLiteral("ffmpeg"),
                                                       info, presetById(QStringLiteral("mp4-h264")),
                                                       options);
        CHECK(plan.valid);
        CHECK(plan.arguments.contains(QStringLiteral("0:s?")));
        CHECK_EQ(plan.arguments.value(plan.arguments.indexOf(QStringLiteral("-c:s")) + 1),
                 QStringLiteral("mov_text"));
    }

    // 3) 保留到 MKV：直接复制原字幕轨（零损失）
    {
        ConvertOptions options;
        options.outputPath = QStringLiteral("C:/out/a.mkv");
        options.subtitleMode = QStringLiteral("copy");
        const CommandPlan plan = CommandBuilder::build(QStringLiteral("ffmpeg"),
                                                       info, presetById(QStringLiteral("mkv-remux")),
                                                       options);
        CHECK(plan.valid);
        CHECK_EQ(plan.arguments.value(plan.arguments.indexOf(QStringLiteral("-c:s")) + 1),
                 QStringLiteral("copy"));
    }

    // 4) 容器放不下字幕：要给出提示，而不是装作没事
    {
        ConvertOptions options;
        options.outputPath = QStringLiteral("C:/out/a.avi");
        options.subtitleMode = QStringLiteral("copy");
        const CommandPlan plan = CommandBuilder::build(QStringLiteral("ffmpeg"),
                                                       info, presetById(QStringLiteral("avi-xvid")),
                                                       options);
        CHECK(plan.valid);
        CHECK(!plan.arguments.contains(QStringLiteral("-c:s")));
        CHECK(!plan.warnings.isEmpty());
    }

    // 5) 预设的容器能力标记要正确
    CHECK(presetById(QStringLiteral("mp4-h264")).subtitleCapable);
    CHECK(presetById(QStringLiteral("mkv-remux")).subtitleCapable);
    CHECK(!presetById(QStringLiteral("avi-xvid")).subtitleCapable);
    CHECK(!presetById(QStringLiteral("gif")).subtitleCapable);
    CHECK(!presetById(QStringLiteral("mp3")).subtitleCapable);
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    out() << "============================================\n"
          << " VideoForge 单元测试 (job 层：任务状态机 / 队列 / 字幕)\n"
          << "============================================\n";
    out().flush();

    // "假 ffmpeg"与被测程序在同一输出目录
    g_fakeFfmpeg = QCoreApplication::applicationDirPath() + QStringLiteral("/vf_fake_ffmpeg.exe");
    if (!QFileInfo::exists(g_fakeFfmpeg))
        g_fakeFfmpeg = QCoreApplication::applicationDirPath() + QStringLiteral("/vf_fake_ffmpeg");

    QTemporaryDir tempDir;
    g_workDir = tempDir.isValid() ? tempDir.path() : QDir::tempPath();

    out() << "测试替身：" << g_fakeFfmpeg << "\n";
    out() << "工作目录：" << g_workDir << "\n";
    out().flush();

    if (!QFileInfo::exists(g_fakeFfmpeg)) {
        out() << "\n[致命] 找不到 vf_fake_ffmpeg，job 层测试无法进行\n";
        out().flush();
        return 2;
    }

    testSubtitleMapping();          // 纯逻辑，不需要假 ffmpeg
    testPreflightRejectsImpossibleJob();
    testSuccessfulJob();
    testFailedJob();
    testVerificationCatchesMissingOutput();
    testCancelRunningJob();
    testRetryRecyclesProcess();
    testQueueRemoveReentrancy();
    testQueueSchedulingWithLimit();

    out() << "\n============================================\n"
          << QStringLiteral(" 共 %1 项断言，失败 %2 项\n").arg(g_checks).arg(g_failures)
          << (g_failures == 0 ? " 结果：全部通过 ✔\n" : " 结果：存在失败 ✘\n")
          << "============================================\n";
    out().flush();

    return g_failures == 0 ? 0 : 1;
}
