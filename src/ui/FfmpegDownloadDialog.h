#pragma once

// =============================================================================
//  VideoForge - ffmpeg 自动下载对话框
//
//  目标：把"去官网找 ffmpeg → 下载 → 解压 → 回来选路径"这四步变成一次点击。
//  对话框本身只负责展示进度与结果，真正的下载/解压逻辑在 core/FfmpegInstaller。
// =============================================================================

#include <QDialog>

class QLabel;
class QProgressBar;
class QPushButton;

namespace vf {

class FfmpegInstaller;

class FfmpegDownloadDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FfmpegDownloadDialog(QWidget *parent = nullptr);

    /// 下载成功后返回可用的 ffmpeg.exe 路径（供调用方交给 FfmpegLocator）
    QString installedFfmpegPath() const { return m_installedPath; }

    /// 打开对话框并立即开始下载；返回 QDialog::Accepted 表示安装成功
    int runAndDownload();

private:
    void appendStatus(const QString &text);
    void setBusy(bool busy);

    FfmpegInstaller *m_installer = nullptr;
    QLabel *m_headline = nullptr;
    QLabel *m_detail = nullptr;
    QProgressBar *m_progress = nullptr;
    QPushButton *m_startButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    QString m_installedPath;
};

} // namespace vf
