#include "core/FfmpegInstaller.h"
#include "core/Format.h"
#include "core/ProcessUtil.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

namespace vf {

namespace {

const char *kDefaultUrl = "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip";

/// 程序目录能不能写（便携包场景可以；装在 Program Files 里就不行）
bool directoryIsWritable(const QString &path)
{
    QDir dir(path);
    if (!dir.exists())
        QDir().mkpath(path);
    const QString probe = path + QStringLiteral("/.vf_write_probe");
    QFile file(probe);
    const bool ok = file.open(QIODevice::WriteOnly);
    if (ok) {
        file.close();
        file.remove();
    }
    return ok;
}

} // namespace

FfmpegInstaller::FfmpegInstaller(QObject *parent)
    : QObject(parent)
{
    m_network = new QNetworkAccessManager(this);
}

FfmpegInstaller::~FfmpegInstaller()
{
    cleanup();
}

QString FfmpegInstaller::defaultDownloadUrl()
{
    // 允许用环境变量覆盖：内网镜像 / 自动化测试都会用到
    const QString override = qEnvironmentVariable("VF_FFMPEG_URL");
    return override.isEmpty() ? QString::fromLatin1(kDefaultUrl) : override;
}

QString FfmpegInstaller::targetBinDirectory()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString portableRoot = appDir + QStringLiteral("/ffmpeg");
    if (directoryIsWritable(portableRoot))
        return portableRoot + QStringLiteral("/bin");

    // 程序目录不可写（例如装在 Program Files）→ 落到用户本地数据目录
    const QString localRoot =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/ffmpeg");
    QDir().mkpath(localRoot);
    return localRoot + QStringLiteral("/bin");
}

void FfmpegInstaller::start(const QString &downloadUrl)
{
    if (m_running)
        return;

    m_url = downloadUrl.isEmpty() ? defaultDownloadUrl() : downloadUrl;
    m_cancelled = false;

    const QString tempRoot = QDir::tempPath() + QStringLiteral("/videoforge-ffmpeg");
    QDir(tempRoot).removeRecursively();
    QDir().mkpath(tempRoot);
    m_archivePath = tempRoot + QStringLiteral("/ffmpeg-download.zip");
    m_extractRoot = tempRoot + QStringLiteral("/extracted");

    m_sink = new QFile(m_archivePath, this);
    if (!m_sink->open(QIODevice::WriteOnly)) {
        fail(QStringLiteral("无法创建临时文件：%1").arg(QDir::toNativeSeparators(m_archivePath)));
        return;
    }

    QNetworkRequest request{QUrl(m_url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("VideoForge/1.0"));

    m_running = true;
    emit statusMessage(QStringLiteral("正在连接下载服务器…"));
    m_reply = m_network->get(request);

    connect(m_reply, &QNetworkReply::downloadProgress, this,
            [this](qint64 received, qint64 total) { emit downloadProgress(received, total); });
    connect(m_reply, &QNetworkReply::readyRead, this, [this]() {
        if (m_sink && m_reply)
            m_sink->write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::finished, this, &FfmpegInstaller::handleDownloadFinished);
}

void FfmpegInstaller::handleDownloadFinished()
{
    if (!m_reply)
        return;

    const QNetworkReply::NetworkError error = m_reply->error();
    const QString errorText = m_reply->errorString();
    if (m_sink) {
        m_sink->write(m_reply->readAll());
        m_sink->close();
    }
    m_reply->deleteLater();
    m_reply = nullptr;

    if (m_cancelled) {
        cleanup();
        return;
    }

    if (error != QNetworkReply::NoError) {
        fail(QStringLiteral("下载失败：%1").arg(errorText));
        return;
    }

    const QFileInfo archiveInfo(m_archivePath);
    if (!archiveInfo.exists() || archiveInfo.size() < 1024) {
        fail(QStringLiteral("下载的文件不完整（只有 %1），请检查网络后重试。")
                 .arg(fmt::size(archiveInfo.size())));
        return;
    }

    emit statusMessage(QStringLiteral("下载完成（%1），正在解压…").arg(fmt::size(archiveInfo.size())));

    QString extractError;
    if (!extractArchive(m_archivePath, m_extractRoot, &extractError)) {
        fail(extractError);
        return;
    }

    emit statusMessage(QStringLiteral("正在安装…"));
    QString ffmpegPath;
    QString installError;
    if (!installFromExtracted(m_extractRoot, &ffmpegPath, &installError)) {
        fail(installError);
        return;
    }

    m_appliedFfmpegPath = ffmpegPath;
    m_running = false;
    emit statusMessage(QStringLiteral("安装完成"));
    emit finished(true, ffmpegPath, QString());
    // 保留下载缓存由调用方决定；这里清掉临时目录
    QDir(m_extractRoot).removeRecursively();
}

bool FfmpegInstaller::extractArchive(const QString &archivePath, const QString &destination, QString *error)
{
    QDir().mkpath(destination);

    // 首选 Windows 自带的 bsdtar（Win10 1803+），比 PowerShell 快且没有执行策略问题
    const QString tar = QStandardPaths::findExecutable(QStringLiteral("tar"));
    if (!tar.isEmpty()) {
        QProcess process;
        suppressConsoleWindow(process);
        process.start(tar, {QStringLiteral("-xf"), archivePath,
                            QStringLiteral("-C"), destination});
        if (process.waitForStarted(5000) && process.waitForFinished(180000)
            && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
            return true;
        }
    }

    // 回退：用 PowerShell 的 Expand-Archive（同样系统自带）
    const QString powershell =
        QStandardPaths::findExecutable(QStringLiteral("powershell"));
    if (powershell.isEmpty()) {
        if (error)
            *error = QStringLiteral("系统里找不到 tar 或 powershell，无法解压。请手动下载 ffmpeg 后选择路径。");
        return false;
    }

    // 用 -LiteralPath 避免路径里的通配符被解释；单引号包裹以兼容空格
    if (archivePath.contains(QLatin1Char('\'')) || destination.contains(QLatin1Char('\''))) {
        if (error)
            *error = QStringLiteral("临时目录路径包含单引号，无法解压。请把程序移到更简单的路径下。");
        return false;
    }
    const QString script = QStringLiteral(
        "Expand-Archive -LiteralPath '%1' -DestinationPath '%2' -Force")
        .arg(archivePath, destination);

    QProcess process;
    suppressConsoleWindow(process);
    process.start(powershell, {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                               QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                               QStringLiteral("-Command"), script});
    if (!process.waitForStarted(5000)) {
        if (error)
            *error = QStringLiteral("无法启动 powershell 进行解压。");
        return false;
    }
    if (!process.waitForFinished(300000)) {
        process.kill();
        process.waitForFinished(2000);
        if (error)
            *error = QStringLiteral("解压超时。");
        return false;
    }
    if (process.exitCode() != 0) {
        const QString stderrText = QString::fromLocal8Bit(process.readAllStandardError()).trimmed();
        if (error)
            *error = QStringLiteral("解压失败：%1")
                         .arg(stderrText.isEmpty() ? QStringLiteral("未知错误") : stderrText);
        return false;
    }
    return true;
}

bool FfmpegInstaller::installFromExtracted(const QString &extractedRoot, QString *ffmpegOut,
                                          QString *error)
{
    // 压缩包里通常是 ffmpeg-x.y-essentials_build/bin/ffmpeg.exe，所以递归找
    QString foundFfmpeg;
    QDirIterator iterator(extractedRoot, {QStringLiteral("ffmpeg.exe")}, QDir::Files,
                          QDirIterator::Subdirectories);
    if (iterator.hasNext())
        foundFfmpeg = iterator.next();

    if (foundFfmpeg.isEmpty()) {
        if (error)
            *error = QStringLiteral("解压后的文件里没有找到 ffmpeg.exe，压缩包格式可能变了。");
        return false;
    }

    const QString sourceBin = QFileInfo(foundFfmpeg).absolutePath();
    const QString sourceProbe = sourceBin + QStringLiteral("/ffprobe.exe");
    if (!QFileInfo::exists(sourceProbe)) {
        if (error)
            *error = QStringLiteral("压缩包里缺少 ffprobe.exe（转换与信息读取都需要它）。");
        return false;
    }

    const QString targetBin = targetBinDirectory();
    QDir().mkpath(targetBin);

    // 只搬需要的文件：整个包里有几十个 dll、文档、示例，没必要都塞进程序目录
    const QStringList wanted{QStringLiteral("ffmpeg.exe"), QStringLiteral("ffprobe.exe"),
                             QStringLiteral("LICENSE"), QStringLiteral("README.txt")};
    for (const QString &name : wanted) {
        QString source = sourceBin + QLatin1Char('/') + name;
        if (!QFileInfo::exists(source)) {
            // LICENSE 之类通常放在压缩包根目录（bin 的上一级）
            const QString parentCandidate =
                QFileInfo(sourceBin).absolutePath() + QLatin1Char('/') + name;
            if (QFileInfo::exists(parentCandidate))
                source = parentCandidate;
            else
                continue;
        }
        const QString target = targetBin + QLatin1Char('/') + name;
        if (QFileInfo::exists(target))
            QFile::remove(target);
        if (!QFile::copy(source, target)) {
            if (error)
                *error = QStringLiteral("无法写入 %1（可能没有权限）。")
                             .arg(QDir::toNativeSeparators(target));
            return false;
        }
    }

    const QString installed = targetBin + QStringLiteral("/ffmpeg.exe");
    if (!QFileInfo::exists(installed)) {
        if (error)
            *error = QStringLiteral("安装后没有找到 ffmpeg.exe。");
        return false;
    }

    if (ffmpegOut)
        *ffmpegOut = installed;
    return true;
}

void FfmpegInstaller::cancel()
{
    if (!m_running)
        return;
    m_cancelled = true;
    if (m_reply) {
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    m_running = false;
    cleanup();
    emit finished(false, QString(), QStringLiteral("已取消下载"));
}

void FfmpegInstaller::fail(const QString &message)
{
    m_running = false;
    cleanup();
    emit finished(false, QString(), message);
}

void FfmpegInstaller::cleanup()
{
    if (m_sink) {
        if (m_sink->isOpen())
            m_sink->close();
        m_sink->deleteLater();
        m_sink = nullptr;
    }
    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}

} // namespace vf
