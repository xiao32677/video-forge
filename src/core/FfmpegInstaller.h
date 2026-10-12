#pragma once

// =============================================================================
//  VideoForge - ffmpeg 一键引导下载
//
//  背景：便携包是自带 ffmpeg 的，但"用户自己编译的版本"或"精简包"都可能没有。
//  以前只能让用户手动去官网下载、解压、再选路径——对普通用户是三道坎。
//  这里做成"一键完成"：下载 → 解压 → 找到 ffmpeg/ffprobe → 自动接管。
//
//  设计取舍：
//    * 不引入第三方压缩库：Windows 10 1803+ 自带 bsdtar 能解 zip，
//      失败再回退到 PowerShell 的 Expand-Archive。两者都是系统自带。
//    * 全程异步（QNetworkAccessManager + QProcess），界面不会卡住。
//    * 下载地址可用环境变量 VF_FFMPEG_URL 覆盖，方便内网镜像与自动化测试。
// =============================================================================

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QProcess;
class QFile;

namespace vf {

class FfmpegInstaller : public QObject
{
    Q_OBJECT

public:
    explicit FfmpegInstaller(QObject *parent = nullptr);
    ~FfmpegInstaller() override;

    /// 默认下载地址（gyan.dev 的 essentials 构建，含 ffmpeg + ffprobe，约 115 MB）
    static QString defaultDownloadUrl();

    /// 目标安装目录：优先放在程序目录下的 ffmpeg/bin（便携包场景），
    /// 程序目录不可写时退到用户本地数据目录。
    static QString targetBinDirectory();

    /// 开始下载并安装
    void start(const QString &downloadUrl = QString());
    void cancel();

    bool isRunning() const { return m_running; }

signals:
    /// 下载进度；total 为 -1 表示服务器没给长度
    void downloadProgress(qint64 received, qint64 total);
    /// 阶段说明（下载中 / 解压中 / 安装中）
    void statusMessage(const QString &text);
    /// 结束：ok=true 时 ffmpegPath 是可直接使用的可执行文件路径
    void finished(bool ok, const QString &ffmpegPath, const QString &errorMessage);

private:
    void handleDownloadFinished();
    bool extractArchive(const QString &archivePath, const QString &destination, QString *error);
    /// 在解压目录里递归找到 ffmpeg.exe，把同目录内容搬到 targetBinDirectory()
    bool installFromExtracted(const QString &extractedRoot, QString *ffmpegOut, QString *error);
    void fail(const QString &message);
    void cleanup();

    QNetworkAccessManager *m_network = nullptr;
    QNetworkReply *m_reply = nullptr;
    QFile *m_sink = nullptr;
    QString m_archivePath;
    QString m_extractRoot;
    QString m_url;
    QString m_appliedFfmpegPath;
    bool m_running = false;
    bool m_cancelled = false;
};

} // namespace vf
