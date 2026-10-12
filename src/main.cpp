// =============================================================================
//  VideoForge - 程序入口
//
//  正常使用：双击即可（可选带文件名参数，支持"用 VideoForge 打开"）
//  自测模式：--screenshot <目录> 会自动载入视频、截图、跑一次真实转换再截图，
//            用于在没有人工点击的情况下验证整个客户端链路是否正常。
// =============================================================================

#include "ui/AppIcon.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QStringList>
#include <QTimer>

using namespace vf;

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("VideoForge"));
    QApplication::setApplicationName(QStringLiteral("VideoForge"));
    QApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QApplication::setWindowIcon(appicon::applicationIcon());

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("VideoForge · 基于 ffmpeg 的视频格式转换桌面客户端"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("files"),
                                 QStringLiteral("要打开的媒体文件（可多个）"),
                                 QStringLiteral("[文件…]"));

    QCommandLineOption themeOption(
        QStringList{QStringLiteral("t"), QStringLiteral("theme")},
        QStringLiteral("启动主题：dark 或 light"), QStringLiteral("主题"));
    QCommandLineOption screenshotOption(
        QStringLiteral("screenshot"),
        QStringLiteral("自测模式：自动载入并截图到指定目录"), QStringLiteral("目录"));
    QCommandLineOption autoConvertOption(
        QStringLiteral("autoconvert"),
        QStringLiteral("自测模式：截图后自动开始一次转换"));
    QCommandLineOption presetOption(
        QStringLiteral("preset"),
        QStringLiteral("启动时使用指定的转换预设 id（例如 mp4-h264 / mp3 / gif）"),
        QStringLiteral("预设id"));
    parser.addOption(themeOption);
    parser.addOption(screenshotOption);
    parser.addOption(autoConvertOption);
    parser.addOption(presetOption);
    parser.process(app);

    const QString themeName = parser.value(themeOption).trimmed().toLower();
    theme::apply(app, themeName == QLatin1String("light") ? theme::Mode::Light : theme::Mode::Dark);

    // 截图自测时忽略上次保存的窗口布局，保证每次截出来的界面一致、可复现
    if (!parser.value(screenshotOption).isEmpty())
        qputenv("VF_RESET_LAYOUT", "1");

    MainWindow window;

    const QString presetId = parser.value(presetOption).trimmed();
    if (!presetId.isEmpty() && !window.setPresetForTest(presetId)) {
        qWarning("未知预设 id：%s", qPrintable(presetId));
    }

    const QString screenshotDirectory = parser.value(screenshotOption);
    const bool screenshotMode = !screenshotDirectory.isEmpty();

    if (screenshotMode) {
        QDir().mkpath(screenshotDirectory);
        window.resize(1500, 1000);
    }
    window.show();

    const QStringList positional = parser.positionalArguments();
    if (!positional.isEmpty()) {
        QStringList absolutePaths;
        for (const QString &path : positional)
            absolutePaths << QFileInfo(path).absoluteFilePath();
        if (absolutePaths.size() == 1)
            window.loadFile(absolutePaths.first());
        else
            window.importPaths(absolutePaths);
    }

    if (screenshotMode) {
        const bool autoConvert = parser.isSet(autoConvertOption);
        const QString directory = screenshotDirectory;

        // 自测时间线：等缩略图与信息就绪 -> 截图 -> 触发转换 -> 截转换中 -> 截完成 -> 退出
        QTimer::singleShot(7000, &window, [&window, directory, autoConvert]() {
            window.grab().save(directory + QStringLiteral("/01-主界面.png"), "PNG");
            // 布局诊断输出：排查控件把侧栏撑宽的问题
            QFile debugFile(directory + QStringLiteral("/layout-debug.txt"));
            if (debugFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&debugFile);
                stream << window.layoutDiagnostics();
            }
            if (!autoConvert) {
                // 只截图模式：抓完就退出，避免自动化脚本挂住
                QTimer::singleShot(1500, qApp, &QApplication::quit);
                return;
            }
            window.triggerConversionForTest();
            QTimer::singleShot(4000, &window, [&window, directory]() {
                window.grab().save(directory + QStringLiteral("/02-转换中.png"), "PNG");
                QTimer::singleShot(30000, &window, [&window, directory]() {
                    window.grab().save(directory + QStringLiteral("/03-转换完成.png"), "PNG");
                    QTimer::singleShot(2000, qApp, &QApplication::quit);
                });
            });
        });
    }

    return app.exec();
}
