#include "core/FfmpegLocator.h"
#include "core/ProcessUtil.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QStringList>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace vf {

namespace {

const char *kSettingsKey = "ffmpeg/executablePath";

QString exeName(const QString &base)
{
#ifdef Q_OS_WIN
    return base + QStringLiteral(".exe");
#else
    return base;
#endif
}

bool isExecutableFile(const QString &path)
{
    QFileInfo info(path);
    return info.exists() && info.isFile();
}

} // namespace

FfmpegLocator::FfmpegLocator(QObject *parent)
    : QObject(parent)
{
}

QString FfmpegLocator::configuredPath()
{
    QSettings settings;
    return settings.value(QString::fromLatin1(kSettingsKey)).toString();
}

void FfmpegLocator::setConfiguredPath(const QString &path)
{
    QSettings settings;
    if (path.isEmpty())
        settings.remove(QString::fromLatin1(kSettingsKey));
    else
        settings.setValue(QString::fromLatin1(kSettingsKey), path);
}

QStringList FfmpegLocator::candidateDirectories()
{
    QStringList dirs;
    const QString appDir = QCoreApplication::applicationDirPath();

    // 1) 随应用分发的内置 ffmpeg（绿色包的主力路径）
    dirs << appDir + QStringLiteral("/ffmpeg/bin");
    dirs << appDir + QStringLiteral("/ffmpeg");
    dirs << appDir + QStringLiteral("/tools/ffmpeg/bin");
    dirs << appDir + QStringLiteral("/tools/ffmpeg");
    dirs << appDir + QStringLiteral("/bin");

    // 2) 开发期：源码树根目录下的 third_party 与临时 _toolchain
    QDir dir(appDir);
    dir.cdUp();
    dirs << dir.absolutePath() + QStringLiteral("/ffmpeg/bin");

    // 3) 常见系统安装位置
#ifdef Q_OS_WIN
    dirs << QStringLiteral("C:/ffmpeg/bin");
    dirs << QStringLiteral("C:/Program Files/ffmpeg/bin");
    const QString localApp = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (!localApp.isEmpty())
        dirs << localApp + QStringLiteral("/Microsoft/WinGet/Links");
    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!home.isEmpty())
        dirs << home + QStringLiteral("/scoop/shims");
    dirs << QStringLiteral("C:/ProgramData/chocolatey/bin");
#endif
    return dirs;
}

QString FfmpegLocator::probeVersion(const QString &executablePath)
{
    if (executablePath.isEmpty() || !isExecutableFile(executablePath))
        return QString();

    QProcess process;
    suppressConsoleWindow(process);
    process.start(executablePath, {QStringLiteral("-version")});
    if (!process.waitForStarted(3000))
        return QString();
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(1000);
        return QString();
    }
    const QString output = QString::fromUtf8(process.readAllStandardOutput());
    const QRegularExpression re(QStringLiteral("version\\s+(\\S+)"));
    const QRegularExpressionMatch match = re.match(output);
    if (!match.hasMatch())
        return QString();
    return match.captured(1);
}

bool FfmpegLocator::useExecutable(const QString &ffmpegPath)
{
    if (ffmpegPath.isEmpty())
        return false;
    const QFileInfo info(ffmpegPath);
    if (!info.exists() || !info.isFile())
        return false;

    const QString dir = info.absolutePath();
    const QString ffprobePath = dir + QLatin1Char('/') + exeName(QStringLiteral("ffprobe"));
    if (!isExecutableFile(ffprobePath))
        return false;

    FfmpegTools tools;
    tools.ffmpegPath = info.absoluteFilePath();
    tools.ffprobePath = ffprobePath;
    tools.version = probeVersion(tools.ffmpegPath);
    if (tools.version.isEmpty())
        return false;

    QProcess process;
    suppressConsoleWindow(process);
    process.start(tools.ffmpegPath, {QStringLiteral("-version")});
    if (process.waitForStarted(3000) && process.waitForFinished(5000)) {
        const QString output = QString::fromUtf8(process.readAllStandardOutput());
        const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        if (lines.size() > 1)
            tools.buildConfig = lines.at(1).trimmed();
    }

    m_tools = tools;
    m_located = true;
    m_failureReason.clear();
    setConfiguredPath(tools.ffmpegPath);
    emit toolsChanged();
    return true;
}

bool FfmpegLocator::validateDirectory(const QString &binDirectory)
{
    const QString ffmpegPath = binDirectory + QLatin1Char('/') + exeName(QStringLiteral("ffmpeg"));
    const QString ffprobePath = binDirectory + QLatin1Char('/') + exeName(QStringLiteral("ffprobe"));
    if (!isExecutableFile(ffmpegPath) || !isExecutableFile(ffprobePath))
        return false;
    return useExecutable(ffmpegPath);
}

FfmpegTools FfmpegLocator::locate()
{
    if (m_located && m_tools.valid())
        return m_tools;

    m_located = true;
    m_failureReason.clear();

    // 1) 用户配置过的路径
    const QString configured = configuredPath();
    if (!configured.isEmpty()) {
        QFileInfo info(configured);
        if (info.exists() && useExecutable(configured))
            return m_tools;
        // 配置已失效：清掉，继续自动查找，避免一直报错
        setConfiguredPath(QString());
    }

    // 2) 内置 / 常见目录
    const QStringList dirs = candidateDirectories();
    for (const QString &dir : dirs) {
        if (validateDirectory(dir))
            return m_tools;
    }

    // 3) 系统 PATH
    const QString fromPath = QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
    if (!fromPath.isEmpty() && useExecutable(fromPath))
        return m_tools;

    // 4) 都没有 —— 给出可执行的解决建议
    m_tools = FfmpegTools();
    m_failureReason = QStringLiteral(
        "未找到 ffmpeg。请点击【设置 ffmpeg 路径】手动指定，"
        "或把 ffmpeg 解压后的 bin 目录放到本程序目录下的 ffmpeg/bin 中。");
    emit toolsChanged();
    return m_tools;
}

} // namespace vf
