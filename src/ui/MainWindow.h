#pragma once

// =============================================================================
//  VideoForge - 主窗口
//
//  客户端形态的集中体现：
//    * 原生菜单栏 / 工具栏 / 状态栏
//    * 三栏工作区（媒体库 · 预览 · 设置）+ 底部任务队列 + 日志
//    * 全窗口拖放、剪贴板粘贴、系统托盘、任务完成通知
//    * 配置持久化（窗口布局、主题、输出目录、并发数）
// =============================================================================

#include "core/FfmpegLocator.h"
#include "core/HwAccelDetector.h"
#include "core/MediaInfo.h"
#include "job/ConvertJob.h"

#include <QHash>
#include <QMainWindow>
#include <QStringList>

class QAction;
class QLabel;
class QProgressBar;
class QPushButton;
class QSplitter;
class QSystemTrayIcon;
class QTimer;
class QToolBar;

namespace vf {

class Chip;
class ConvertJob;
class ConvertSettingsPanel;
class JobQueuePanel;
class LogPanel;
class MediaLibraryPanel;
class MediaProbe;
class PreviewPanel;
class ThumbnailService;
class JobQueue;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /// 载入一个文件（命令行启动参数或拖放时使用）
    void loadFile(const QString &filePath);

    /// 导入一组文件
    void importPaths(const QStringList &paths);

    // ---- 自动化测试钩子（--screenshot 模式使用，不影响正常使用）----
    bool isMediaReady(const QString &filePath) const;
    void triggerConversionForTest();
    bool setPresetForTest(const QString &presetId);
    bool setSubtitleModeForTest(const QString &mode);
    /// 按文件名子串多选媒体库内容（验证"只转选中项"）
    int selectForTest(const QString &substring);

    /// 布局诊断：列出界面里"最小宽度需求"最大的若干控件。
    /// 用于排查"某个控件把侧栏撑宽导致右侧被裁掉"这类问题。
    QString layoutDiagnostics() const;

signals:
    /// 某个文件的信息 + 缩略图都已就绪（自动化测试用）
    void mediaReady(const QString &filePath);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onAddFiles();
    void onAddFolder();
    void onCurrentMediaChanged(const QString &filePath);
    void onMediaProbed(const vf::MediaInfo &info);
    void onMediaProbeFailed(const QString &filePath, const QString &error);
    void onCoverReady(const QString &filePath, const QImage &image, bool fromCache);
    void onFilmstripReady(const QString &filePath, const QList<QImage> &frames, double duration);
    void onStartConversion();
    void onCancelAll();
    void onPauseAll();
    void onResumeAll();
    void onChooseFfmpeg();
    /// 一键下载安装 ffmpeg；返回是否成功
    bool downloadFfmpeg();
    /// 重新探测硬件编码能力（换过 ffmpeg 之后要重跑）
    void refreshHardwareCapabilities();
    void onToggleTheme();
    void onAbout();
    /// 第三方许可说明（分发了 Qt DLL 与 ffmpeg 二进制，必须能查到许可与来源）
    void onShowLicenses();
    void onJobAdded(vf::ConvertJob *job);
    void onJobFinished(vf::ConvertJob *job, vf::JobState state);
    void onAllJobsFinished();
    void onJobSelectionChanged(const QString &jobId);
    void updateStatusBar();

private:
    void buildActions();
    void buildMenus();
    /// 顶部区域：第一行品牌 + 状态胶囊，第二行操作按钮
    QWidget *buildHeader(QWidget *parent);
    void buildCentralWidget();
    void buildStatusBar();
    void buildTrayIcon();
    void connectSignals();
    void restoreSettings();
    void saveSettings();

    /// 主题切换后重建图标（图标颜色跟随主题）
    void refreshIcons();
    void updateFfmpegStatus();
    void refreshThumbnails(const MediaInfo &info);
    void probeFile(const QString &filePath);
    /// 刷新"这次会转换哪几个文件"的提示
    void updateConversionScope();
    /// 磁盘空间预检：不够就直接拦住，并告诉用户还差多少
    bool confirmDiskSpace(const QHash<QString, qint64> &volumeEstimates, int fileCount);
    void recomputeBusyState();
    /// 按窗口高度自动收起/展开日志面板（用户手动切换过就不再干预）
    void updateLogVisibilityForHeight();
    void notify(const QString &title, const QString &message, bool warning = false);
    void openContainingFolder(const QString &filePath);

    // ---- 核心服务 ----
    FfmpegLocator *m_locator = nullptr;
    MediaProbe *m_probe = nullptr;
    ThumbnailService *m_thumbnails = nullptr;
    JobQueue *m_queue = nullptr;
    HwAccelDetector *m_hwDetector = nullptr;

    // ---- 界面 ----
    MediaLibraryPanel *m_library = nullptr;
    PreviewPanel *m_preview = nullptr;
    ConvertSettingsPanel *m_settings = nullptr;
    JobQueuePanel *m_jobs = nullptr;
    LogPanel *m_log = nullptr;
    QSplitter *m_verticalSplitter = nullptr;
    QSplitter *m_horizontalSplitter = nullptr;

    QWidget *m_header = nullptr;
    QPushButton *m_ffmpegStatusButton = nullptr;
    Chip *m_accelChip = nullptr;
    Chip *m_queueChip = nullptr;
    QLabel *m_ffmpegStatusLabel = nullptr;
    QLabel *m_queueStatusLabel = nullptr;
    QLabel *m_currentFileLabel = nullptr;
    QProgressBar *m_overallProgress = nullptr;
    QSystemTrayIcon *m_tray = nullptr;

    QAction *m_addFilesAction = nullptr;
    QAction *m_addFolderAction = nullptr;
    QAction *m_removeAction = nullptr;
    QAction *m_clearAction = nullptr;
    QAction *m_startAction = nullptr;
    QAction *m_pauseAction = nullptr;
    QAction *m_resumeAction = nullptr;
    QAction *m_cancelAction = nullptr;
    QAction *m_clearFinishedAction = nullptr;
    QAction *m_darkThemeAction = nullptr;
    QAction *m_lightThemeAction = nullptr;
    QAction *m_toggleLogAction = nullptr;
    QAction *m_setFfmpegAction = nullptr;
    QAction *m_openOutputAction = nullptr;
    QAction *m_clearCacheAction = nullptr;
    QAction *m_aboutAction = nullptr;
    QAction *m_aboutQtAction = nullptr;
    QAction *m_licensesAction = nullptr;
    QAction *m_quitAction = nullptr;

    // ---- 状态 ----
    QHash<QString, MediaInfo> m_infoCache;
    QHash<QString, QStringList> m_filmstripCache;
    QString m_currentFile;
    bool m_hwDetectionDone = false;
    bool m_conversionRunning = false;
    bool m_forceQuit = false;
    /// 用户是否手动切换过日志面板（切换过就不再自动干预）
    bool m_logVisibilityUserSet = false;
};

} // namespace vf
