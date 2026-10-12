// =============================================================================
//  VideoForge - 测试用的"假 ffmpeg"
//
//  为什么需要它：
//    job 层测试要验证的是"进程 → 进度解析 → 状态机"这条链路，
//    但真实 ffmpeg 的行为不可控（耗时、会不会失败、输出什么都不确定）。
//    这个可执行文件刻意模仿 ffmpeg 的几个关键行为，并支持通过环境变量切换：
//
//      VF_FAKE_MODE=ok        打印进度 → 写一个非空输出文件 → 退出 0
//      VF_FAKE_MODE=nooutput  打印进度 → 退出 0，但**不产出文件**（测产物校验）
//      VF_FAKE_MODE=fail      打印错误 → 退出 1
//      VF_FAKE_MODE=slow      停 3 秒再退出（用来测取消/暂停）
//
//  它只在测试里被使用，不参与正式程序。
// =============================================================================

#include <QCoreApplication>
#include <QFile>
#include <QString>
#include <QTextStream>
#include <QThread>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QString mode = qEnvironmentVariable("VF_FAKE_MODE", QStringLiteral("ok"));

    // ffmpeg 的参数里，最后一个不以 '-' 开头的参数就是输出文件
    QString outputPath;
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (!argument.startsWith(QLatin1Char('-')))
            outputPath = argument;
    }

    QTextStream out(stdout);
    auto emitProgress = [&out](const QString &text) {
        out << text;
        out.flush();
    };

    if (mode == QLatin1String("fail")) {
        emitProgress(QStringLiteral("Error: fake ffmpeg failed on purpose\n"));
        return 1;
    }

    if (mode == QLatin1String("slow")) {
        // 给测试留出取消的时间窗口
        QThread::sleep(3);
        return 0;
    }

    // ok / nooutput：模仿 -progress pipe:1 的输出节奏
    emitProgress(QStringLiteral("frame=30\nfps=30.0\ntotal_size=1024\n"
                                "out_time_us=1000000\nspeed=1.0x\nprogress=continue\n"));
    QThread::msleep(150);
    emitProgress(QStringLiteral("frame=300\nfps=33.0\ntotal_size=4096\n"
                                "out_time_us=10000000\nspeed=1.8x\nprogress=continue\n"));
    QThread::msleep(150);
    emitProgress(QStringLiteral("progress=end\n"));

    if (mode == QLatin1String("ok") && !outputPath.isEmpty()) {
        QFile file(outputPath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QByteArray(4096, 'x'));
            file.close();
        }
    }
    return 0;
}
