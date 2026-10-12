#include "ui/MainWindow.h"

#include "core/CommandBuilder.h"
#include "core/ConvertPreset.h"
#include "core/Format.h"
#include "core/MediaProbe.h"
#include "core/ProcessUtil.h"
#include "core/SizeEstimator.h"
#include "core/ThumbnailService.h"
#include "job/ConvertJob.h"
#include "job/JobQueue.h"
#include "ui/AppIcon.h"
#include "ui/ConvertSettingsPanel.h"
#include "ui/FfmpegDownloadDialog.h"
#include "ui/Icons.h"
#include "ui/JobQueuePanel.h"
#include "ui/LogPanel.h"
#include "ui/MediaLibraryPanel.h"
#include "ui/PreviewPanel.h"
#include "ui/Theme.h"
#include "ui/UiWidgets.h"

#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QStorageInfo>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace vf {

namespace {

const char *kAppName = "VideoForge";
const int kDefaultMaxConcurrent = 2;

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("VideoForge · 视频格式转换器"));
    setWindowIcon(appicon::applicationIcon());
    setAcceptDrops(true);
    // 1366x768 的笔记本很常见，最小尺寸必须能在这种屏上完整显示
    setMinimumSize(1000, 680);

    m_locator = new FfmpegLocator(this);
    m_probe = new MediaProbe(m_locator, this);
    m_thumbnails = new ThumbnailService(m_locator, this);
    m_queue = new JobQueue(m_locator, this);
    m_hwDetector = new HwAccelDetector(this);

    buildActions();
    buildMenus();
    buildCentralWidget();
    buildStatusBar();
    buildTrayIcon();
    connectSignals();
    restoreSettings();
    refreshIcons();

    // ffmpeg 定位：启动时立刻做，并且把结果反映到状态栏
    m_locator->locate();
    updateFfmpegStatus();

    // 硬件编码能力探测放到后台，别拖慢启动
    refreshHardwareCapabilities();

    m_log->appendSystemMessage(
        m_locator->tools().valid()
            ? QStringLiteral("已就绪。使用 ffmpeg %1（%2）")
                  .arg(m_locator->tools().version,
                       QDir::toNativeSeparators(m_locator->tools().ffmpegPath))
            : QStringLiteral("⚠ %1").arg(m_locator->failureReason()));
    m_log->appendSystemMessage(QStringLiteral("提示：把视频文件直接拖进窗口即可开始。"));
}

MainWindow::~MainWindow() = default;

// ---------------------------------------------------------------------------
//  界面搭建
// ---------------------------------------------------------------------------
void MainWindow::buildCentralWidget()
{
    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_header = buildHeader(central);
    layout->addWidget(m_header);

    m_horizontalSplitter = new QSplitter(Qt::Horizontal, central);
    m_library = new MediaLibraryPanel(m_horizontalSplitter);
    m_library->setMinimumWidth(210);
    m_preview = new PreviewPanel(m_horizontalSplitter);
    m_preview->setMinimumWidth(340);
    m_settings = new ConvertSettingsPanel(m_horizontalSplitter);
    m_settings->setMinimumWidth(300);
    m_settings->setMaximumWidth(460);

    m_horizontalSplitter->addWidget(m_library);
    m_horizontalSplitter->addWidget(m_preview);
    m_horizontalSplitter->addWidget(m_settings);
    m_horizontalSplitter->setStretchFactor(0, 0);
    m_horizontalSplitter->setStretchFactor(1, 1);
    m_horizontalSplitter->setStretchFactor(2, 0);
    m_horizontalSplitter->setSizes({280, 760, 420});

    m_verticalSplitter = new QSplitter(Qt::Vertical, central);
    m_verticalSplitter->addWidget(m_horizontalSplitter);

    m_jobs = new JobQueuePanel(m_verticalSplitter);
    m_jobs->setMinimumHeight(150);
    m_verticalSplitter->addWidget(m_jobs);

    m_log = new LogPanel(m_verticalSplitter);
    m_verticalSplitter->addWidget(m_log);
    m_verticalSplitter->setStretchFactor(0, 6);
    m_verticalSplitter->setStretchFactor(1, 2);
    m_verticalSplitter->setStretchFactor(2, 2);
    m_verticalSplitter->setSizes({520, 180, 180});
    layout->addWidget(m_verticalSplitter);
    setCentralWidget(central);
}

void MainWindow::buildActions()
{
    m_addFilesAction = new QAction(QStringLiteral("添加文件…"), this);
    m_addFilesAction->setShortcut(QKeySequence::Open);
    m_addFilesAction->setToolTip(QStringLiteral("添加视频/音频文件（Ctrl+O）"));

    m_addFolderAction = new QAction(QStringLiteral("添加文件夹…"), this);
    m_addFolderAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+O")));

    m_removeAction = new QAction(QStringLiteral("移除选中"), this);
    m_removeAction->setShortcut(QKeySequence::Delete);

    m_clearAction = new QAction(QStringLiteral("清空媒体库"), this);

    m_startAction = new QAction(QStringLiteral("▶  开始转换"), this);
    m_startAction->setShortcut(QKeySequence(QStringLiteral("F5")));
    m_startAction->setToolTip(QStringLiteral("把媒体库中的所有文件加入转换队列（F5）"));

    m_pauseAction = new QAction(QStringLiteral("暂停全部"), this);
    m_resumeAction = new QAction(QStringLiteral("继续全部"), this);
    m_cancelAction = new QAction(QStringLiteral("取消全部"), this);
    m_clearFinishedAction = new QAction(QStringLiteral("清除已完成任务"), this);

    m_darkThemeAction = new QAction(QStringLiteral("深色主题"), this);
    m_darkThemeAction->setCheckable(true);
    m_lightThemeAction = new QAction(QStringLiteral("浅色主题"), this);
    m_lightThemeAction->setCheckable(true);

    m_toggleLogAction = new QAction(QStringLiteral("显示日志面板"), this);
    m_toggleLogAction->setCheckable(true);
    m_toggleLogAction->setChecked(true);

    m_setFfmpegAction = new QAction(QStringLiteral("设置 ffmpeg 路径…"), this);
    m_openOutputAction = new QAction(QStringLiteral("打开输出文件所在目录"), this);
    m_clearCacheAction = new QAction(QStringLiteral("清空缩略图缓存"), this);
    m_aboutAction = new QAction(QStringLiteral("关于 VideoForge"), this);
    m_aboutQtAction = new QAction(QStringLiteral("关于 Qt"), this);
    m_quitAction = new QAction(QStringLiteral("退出"), this);
    m_quitAction->setShortcut(QKeySequence::Quit);

    // 主题互斥
    auto *themeGroup = new QActionGroup(this);
    themeGroup->addAction(m_darkThemeAction);
    themeGroup->addAction(m_lightThemeAction);
    themeGroup->setExclusive(true);
    (theme::currentMode() == theme::Mode::Dark ? m_darkThemeAction : m_lightThemeAction)->setChecked(true);
}

void MainWindow::buildMenus()
{
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    fileMenu->addAction(m_addFilesAction);
    fileMenu->addAction(m_addFolderAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_removeAction);
    fileMenu->addAction(m_clearAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_openOutputAction);
    fileMenu->addSeparator();
    fileMenu->addAction(m_quitAction);

    QMenu *convertMenu = menuBar()->addMenu(QStringLiteral("转换(&C)"));
    convertMenu->addAction(m_startAction);
    convertMenu->addSeparator();
    convertMenu->addAction(m_pauseAction);
    convertMenu->addAction(m_resumeAction);
    convertMenu->addAction(m_cancelAction);
    convertMenu->addSeparator();
    convertMenu->addAction(m_clearFinishedAction);

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
    viewMenu->addAction(m_darkThemeAction);
    viewMenu->addAction(m_lightThemeAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_toggleLogAction);
    viewMenu->addSeparator();
    viewMenu->addAction(m_clearCacheAction);

    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));
    helpMenu->addAction(m_setFfmpegAction);
    helpMenu->addSeparator();
    helpMenu->addAction(m_aboutAction);
    helpMenu->addAction(m_aboutQtAction);

    // 右键菜单也顺手挂上（媒体库自带的在面板内部）
    addActions({m_addFilesAction, m_startAction});
}

QWidget *MainWindow::buildHeader(QWidget *parent)
{
    auto *header = new QWidget(parent);
    auto *headerLayout = new QVBoxLayout(header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    headerLayout->setSpacing(0);

    // ------------------------------------------------------------ 第一行：品牌 + 状态
    auto *heroRow = new QFrame(header);
    heroRow->setObjectName(QStringLiteral("heroRow"));
    auto *heroLayout = new QHBoxLayout(heroRow);
    heroLayout->setContentsMargins(14, 11, 14, 11);
    heroLayout->setSpacing(10);

    auto *logoLabel = new QLabel(heroRow);
    logoLabel->setPixmap(appicon::logo(36));
    logoLabel->setFixedSize(36, 36);
    heroLayout->addWidget(logoLabel);

    auto *textColumn = new QVBoxLayout;
    textColumn->setContentsMargins(0, 0, 0, 0);
    textColumn->setSpacing(1);
    auto *title = new QLabel(QStringLiteral("VideoForge"), heroRow);
    title->setProperty("role", "heroTitle");
    auto *subtitle = new QLabel(QStringLiteral("视频格式转换器 · ffmpeg 内核 · 全程可视化"), heroRow);
    subtitle->setProperty("role", "heroSub");
    textColumn->addWidget(title);
    textColumn->addWidget(subtitle);
    heroLayout->addLayout(textColumn);

    heroLayout->addStretch(1);

    m_accelChip = new Chip(QStringLiteral("正在检测硬件…"), theme::mutedText(),
                           icons::Glyph::Gauge, heroRow);
    heroLayout->addWidget(m_accelChip);

    m_queueChip = new Chip(QStringLiteral("队列为空"), theme::mutedText(),
                           icons::Glyph::List, heroRow);
    heroLayout->addWidget(m_queueChip);

    headerLayout->addWidget(heroRow);

    // ------------------------------------------------------------ 第二行：操作按钮
    auto *actionRow = new QFrame(header);
    actionRow->setObjectName(QStringLiteral("actionRow"));
    auto *actionLayout = new QHBoxLayout(actionRow);
    actionLayout->setContentsMargins(10, 7, 12, 8);
    actionLayout->setSpacing(4);

    auto makeToolButton = [actionRow](QAction *action, bool primary = false) {
        auto *button = new QToolButton(actionRow);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setIconSize(QSize(16, 16));
        button->setCursor(Qt::PointingHandCursor);
        if (primary)
            button->setProperty("primary", true);
        return button;
    };

    actionLayout->addWidget(makeToolButton(m_addFilesAction));
    actionLayout->addWidget(makeToolButton(m_addFolderAction));
    actionLayout->addSpacing(6);
    actionLayout->addWidget(makeToolButton(m_removeAction));
    actionLayout->addWidget(makeToolButton(m_clearAction));
    actionLayout->addSpacing(6);
    actionLayout->addWidget(makeToolButton(m_startAction, true));
    actionLayout->addWidget(makeToolButton(m_pauseAction));
    actionLayout->addWidget(makeToolButton(m_resumeAction));
    actionLayout->addWidget(makeToolButton(m_cancelAction));
    actionLayout->addSpacing(6);
    actionLayout->addWidget(makeToolButton(m_openOutputAction));
    actionLayout->addWidget(makeToolButton(m_setFfmpegAction));

    actionLayout->addStretch(1);

    // 主题切换做成图标按钮，靠右
    for (QAction *themeAction : {m_darkThemeAction, m_lightThemeAction}) {
        auto *button = new QToolButton(actionRow);
        button->setDefaultAction(themeAction);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setIconSize(QSize(16, 16));
        button->setCursor(Qt::PointingHandCursor);
        actionLayout->addWidget(button);
    }

    headerLayout->addWidget(actionRow);
    return header;
}

void MainWindow::buildStatusBar()
{
    // ffmpeg 状态做成"可点击的胶囊按钮"：用户看到红点就知道该点它
    m_ffmpegStatusButton = new QPushButton(this);
    m_ffmpegStatusButton->setProperty("chip", true);
    m_ffmpegStatusButton->setCursor(Qt::PointingHandCursor);
    m_ffmpegStatusButton->setFlat(true);
    connect(m_ffmpegStatusButton, &QPushButton::clicked, this, &MainWindow::onChooseFfmpeg);
    statusBar()->addWidget(m_ffmpegStatusButton);

    m_currentFileLabel = new QLabel(this);
    m_currentFileLabel->setContentsMargins(12, 0, 8, 0);
    m_currentFileLabel->setProperty("role", "hint");
    statusBar()->addWidget(m_currentFileLabel, 1);

    m_queueStatusLabel = new QLabel(this);
    m_queueStatusLabel->setContentsMargins(8, 0, 12, 0);
    m_queueStatusLabel->setProperty("role", "hint");
    statusBar()->addPermanentWidget(m_queueStatusLabel);

    m_overallProgress = new QProgressBar(this);
    m_overallProgress->setRange(0, 1000);
    m_overallProgress->setValue(0);
    m_overallProgress->setFixedWidth(210);
    m_overallProgress->setTextVisible(true);
    m_overallProgress->setFormat(QStringLiteral("总进度 %p%"));
    m_overallProgress->setVisible(false);
    statusBar()->addPermanentWidget(m_overallProgress);
}

void MainWindow::refreshIcons()
{
    const QColor normal = theme::textColor();
    const QColor muted = theme::mutedText();
    const QColor accent = theme::accent();
    const QColor danger = theme::dangerColor();
    const QColor success = theme::successColor();

    m_addFilesAction->setIcon(icons::icon(icons::Glyph::Add, accent, 16));
    m_addFolderAction->setIcon(icons::icon(icons::Glyph::Folder, normal, 16));
    m_removeAction->setIcon(icons::icon(icons::Glyph::Trash, normal, 16));
    m_clearAction->setIcon(icons::icon(icons::Glyph::Stack, normal, 16));
    m_startAction->setIcon(icons::icon(icons::Glyph::PlayCircle, QColor(QStringLiteral("#FFFFFF")), 17));
    m_pauseAction->setIcon(icons::icon(icons::Glyph::Pause, normal, 16));
    m_resumeAction->setIcon(icons::icon(icons::Glyph::Play, normal, 16));
    m_cancelAction->setIcon(icons::icon(icons::Glyph::Stop, danger, 16));
    m_clearFinishedAction->setIcon(icons::icon(icons::Glyph::Check, success, 16));
    m_darkThemeAction->setIcon(icons::icon(icons::Glyph::Moon, normal, 16));
    m_lightThemeAction->setIcon(icons::icon(icons::Glyph::Sun, normal, 16));
    m_toggleLogAction->setIcon(icons::icon(icons::Glyph::Terminal, normal, 16));
    m_setFfmpegAction->setIcon(icons::icon(icons::Glyph::Gear, normal, 16));
    m_openOutputAction->setIcon(icons::icon(icons::Glyph::FolderOpen, normal, 16));
    m_clearCacheAction->setIcon(icons::icon(icons::Glyph::Refresh, normal, 16));
    m_aboutAction->setIcon(icons::icon(icons::Glyph::Info, accent, 16));
    m_aboutQtAction->setIcon(icons::icon(icons::Glyph::Info, muted, 16));
    m_quitAction->setIcon(icons::icon(icons::Glyph::Cross, muted, 16));

    updateFfmpegStatus();
    updateStatusBar();
}

void MainWindow::updateStatusBar()
{
    const int total = m_queue->count();
    const int running = m_queue->runningCount();
    const int completed = m_queue->finishedCount();
    const int failed = m_queue->failedCount();

    if (m_queueChip) {
        if (total == 0) {
            m_queueChip->setText(QStringLiteral("队列为空"));
            m_queueChip->setColor(theme::mutedText());
            m_queueChip->setGlyph(icons::Glyph::List);
        } else {
            m_queueChip->setText(QStringLiteral("队列 %1 · 进行 %2 · 完成 %3 · 失败 %4")
                                     .arg(total).arg(running).arg(completed).arg(failed));
            m_queueChip->setColor(failed > 0 ? theme::dangerColor()
                                             : (running > 0 ? theme::accent()
                                                            : theme::successColor()));
            m_queueChip->setGlyph(running > 0 ? icons::Glyph::Refresh : icons::Glyph::List);
        }
    }

    if (m_queueStatusLabel) {
        m_queueStatusLabel->setText(total == 0
            ? QStringLiteral("就绪")
            : QStringLiteral("进行 %1 / 共 %2").arg(running).arg(total));
    }

    if (m_overallProgress->isVisible())
        m_overallProgress->setValue(static_cast<int>(m_queue->overallProgress() * 1000));
    recomputeBusyState();
}

void MainWindow::updateFfmpegStatus()
{
    const FfmpegTools tools = m_locator->tools();
    if (!m_ffmpegStatusButton)
        return;

    if (tools.valid()) {
        m_ffmpegStatusButton->setText(QStringLiteral("ffmpeg %1").arg(tools.version));
        m_ffmpegStatusButton->setIcon(icons::icon(icons::Glyph::Check, theme::successColor(), 13));
        m_ffmpegStatusButton->setStyleSheet(
            QStringLiteral("QPushButton { color: %1; border-color: %2; }")
                .arg(theme::successColor().name(),
                     QColor(theme::successColor()).darker(180).name()));
        m_ffmpegStatusButton->setToolTip(QStringLiteral("ffmpeg：%1\nffprobe：%2\n点击可更换")
                                            .arg(QDir::toNativeSeparators(tools.ffmpegPath),
                                                 QDir::toNativeSeparators(tools.ffprobePath)));
    } else {
        m_ffmpegStatusButton->setText(QStringLiteral("未找到 ffmpeg · 点击设置"));
        m_ffmpegStatusButton->setIcon(icons::icon(icons::Glyph::Warning, theme::dangerColor(), 13));
        m_ffmpegStatusButton->setStyleSheet(
            QStringLiteral("QPushButton { color: %1; border-color: %2; }")
                .arg(theme::dangerColor().name(),
                     QColor(theme::dangerColor()).darker(180).name()));
        m_ffmpegStatusButton->setToolTip(m_locator->failureReason());
    }
}

void MainWindow::buildTrayIcon()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    m_tray = new QSystemTrayIcon(appicon::applicationIcon(), this);
    m_tray->setToolTip(QStringLiteral("VideoForge · 视频格式转换器"));

    auto *menu = new QMenu(this);
    menu->addAction(QStringLiteral("显示主窗口"), this, [this]() {
        show();
        setWindowState(windowState() & ~Qt::WindowMinimized);
        raise();
        activateWindow();
    });
    menu->addAction(m_addFilesAction);
    menu->addSeparator();
    menu->addAction(m_quitAction);
    m_tray->setContextMenu(menu);

    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
            show();
            setWindowState(windowState() & ~Qt::WindowMinimized);
            raise();
            activateWindow();
        }
    });
    m_tray->show();
}

void MainWindow::connectSignals()
{
    // ---- 媒体库 ----
    connect(m_library, &MediaLibraryPanel::addRequested, this, &MainWindow::onAddFiles);
    connect(m_library, &MediaLibraryPanel::addFolderRequested, this, &MainWindow::onAddFolder);
    connect(m_library, &MediaLibraryPanel::removeRequested, this, [this]() {
        updateConversionScope();
        if (m_library->count() == 0) {
            m_preview->clearMedia();
            m_settings->clearMedia();
            m_currentFile.clear();
        }
    });
    connect(m_library, &MediaLibraryPanel::pathsAdded, this, [this](int count) {
        m_log->appendSystemMessage(QStringLiteral("已添加 %1 个文件到媒体库").arg(count));
        // 新加入的文件立刻后台探测，这样用户点开就有信息，不用等
        for (const QString &path : m_library->mediaPaths()) {
            if (!m_infoCache.contains(path))
                probeFile(path);
        }
        updateConversionScope();
    });
    connect(m_library, &MediaLibraryPanel::currentChanged, this, &MainWindow::onCurrentMediaChanged);
    connect(m_library, &MediaLibraryPanel::selectionChanged, this, &MainWindow::updateConversionScope);
    connect(m_library, &MediaLibraryPanel::convertSelectionRequested, this, [this]() {
        // 右键"仅转换选中的文件"：确保确实有选中项再开始
        if (m_library->selectionCount() > 0)
            onStartConversion();
    });

    // ---- 探测 ----
    connect(m_probe, &MediaProbe::probed, this, &MainWindow::onMediaProbed);
    connect(m_probe, &MediaProbe::failed, this, &MainWindow::onMediaProbeFailed);

    // ---- 缩略图 ----
    connect(m_thumbnails, &ThumbnailService::coverReady, this, &MainWindow::onCoverReady);
    connect(m_thumbnails, &ThumbnailService::filmstripReady, this, &MainWindow::onFilmstripReady);
    connect(m_thumbnails, &ThumbnailService::thumbnailFailed, this,
            [this](const QString &filePath, const QString &error) {
                if (filePath == m_currentFile)
                    m_preview->setHint(QStringLiteral("缩略图生成失败：%1").arg(error));
            });

    // ---- 预览 -> 裁剪 ----
    connect(m_preview, &PreviewPanel::trimStartRequested, this, [this](double seconds) {
        m_settings->setTrimStart(seconds);
        statusBar()->showMessage(QStringLiteral("已把 %1 设为裁剪起点").arg(fmt::duration(seconds)), 4000);
    });
    connect(m_preview, &PreviewPanel::trimEndRequested, this, [this](double seconds) {
        m_settings->setTrimEnd(seconds);
        statusBar()->showMessage(QStringLiteral("已把 %1 设为裁剪终点").arg(fmt::duration(seconds)), 4000);
    });

    // ---- 设置面板 ----
    connect(m_settings, &ConvertSettingsPanel::startRequested, this, &MainWindow::onStartConversion);
    connect(m_settings, &ConvertSettingsPanel::stopRequested, this, &MainWindow::onCancelAll);

    // ---- 队列 ----
    connect(m_queue, &JobQueue::jobAdded, this, &MainWindow::onJobAdded);
    connect(m_queue, &JobQueue::jobProgressChanged, this, [this](ConvertJob *job) {
        m_jobs->updateJob(job);
        if (m_overallProgress->isVisible()) {
            const double overall = m_queue->overallProgress();
            m_overallProgress->setValue(static_cast<int>(overall * 1000));
            setWindowTitle(QStringLiteral("VideoForge · 转换中 %1%")
                               .arg(static_cast<int>(overall * 100 + 0.5)));
        }
    });
    connect(m_queue, &JobQueue::jobStateChanged, this, [this](ConvertJob *job) {
        m_jobs->updateJob(job);
        updateStatusBar();
    });
    connect(m_queue, &JobQueue::jobFinished, this, &MainWindow::onJobFinished);
    connect(m_queue, &JobQueue::jobRemoved, this, [this](const QString &jobId) {
        m_jobs->removeJob(jobId);
        updateStatusBar();
    });
    connect(m_queue, &JobQueue::allFinished, this, &MainWindow::onAllJobsFinished);

    connect(m_jobs, &JobQueuePanel::cancelRequested, m_queue, &JobQueue::cancelJob);
    connect(m_jobs, &JobQueuePanel::pauseRequested, m_queue, &JobQueue::pauseJob);
    connect(m_jobs, &JobQueuePanel::resumeRequested, m_queue, &JobQueue::resumeJob);
    connect(m_jobs, &JobQueuePanel::retryRequested, m_queue, &JobQueue::retryJob);
    connect(m_jobs, &JobQueuePanel::removeRequested, m_queue, &JobQueue::removeJob);
    connect(m_jobs, &JobQueuePanel::selectionChanged, this, &MainWindow::onJobSelectionChanged);
    connect(m_jobs, &JobQueuePanel::openOutputRequested, this, [this](const QString &jobId) {
        if (ConvertJob *job = m_queue->job(jobId))
            openContainingFolder(job->outputPath());
    });
    connect(m_jobs, &JobQueuePanel::playOutputRequested, this, [this](const QString &jobId) {
        if (ConvertJob *job = m_queue->job(jobId))
            QDesktopServices::openUrl(QUrl::fromLocalFile(job->outputPath()));
    });

    // ---- 动作 ----
    connect(m_addFilesAction, &QAction::triggered, this, &MainWindow::onAddFiles);
    connect(m_addFolderAction, &QAction::triggered, this, &MainWindow::onAddFolder);
    connect(m_removeAction, &QAction::triggered, this, [this]() { m_library->removeSelected(); });
    connect(m_clearAction, &QAction::triggered, this, [this]() {
        m_library->clearAll();
        m_preview->clearMedia();
        m_settings->clearMedia();
        m_currentFile.clear();
        updateConversionScope();
    });
    connect(m_startAction, &QAction::triggered, this, &MainWindow::onStartConversion);
    connect(m_pauseAction, &QAction::triggered, this, &MainWindow::onPauseAll);
    connect(m_resumeAction, &QAction::triggered, this, &MainWindow::onResumeAll);
    connect(m_cancelAction, &QAction::triggered, this, &MainWindow::onCancelAll);
    connect(m_clearFinishedAction, &QAction::triggered, this, [this]() {
        m_queue->clearFinished();
        m_jobs->clearFinished();
        updateStatusBar();
    });
    connect(m_darkThemeAction, &QAction::triggered, this, [this]() {
        theme::apply(*qApp, theme::Mode::Dark);
        refreshIcons();
        refreshThumbnails(m_infoCache.value(m_currentFile));
    });
    connect(m_lightThemeAction, &QAction::triggered, this, [this]() {
        theme::apply(*qApp, theme::Mode::Light);
        refreshIcons();
        refreshThumbnails(m_infoCache.value(m_currentFile));
    });
    connect(m_toggleLogAction, &QAction::toggled, this, [this](bool visible) {
        m_log->setVisible(visible);
    });
    // 只有"用户主动触发"才标记为用户已选择；setChecked() 不会发 triggered
    connect(m_toggleLogAction, &QAction::triggered, this, [this](bool) {
        m_logVisibilityUserSet = true;
    });
    connect(m_setFfmpegAction, &QAction::triggered, this, &MainWindow::onChooseFfmpeg);
    connect(m_openOutputAction, &QAction::triggered, this, [this]() {
        const QString directory = m_settings->outputDirectory();
        if (!directory.isEmpty() && QFileInfo::exists(directory)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
            return;
        }
        const QString current = m_library->currentPath();
        if (!current.isEmpty()) {
            openContainingFolder(current);
            return;
        }
        QMessageBox::information(this, QStringLiteral("打开输出目录"),
                                 QStringLiteral("还没有可打开的输出目录。先转换一个文件试试。"));
    });
    connect(m_clearCacheAction, &QAction::triggered, this, [this]() {
        ThumbnailService::clearCache();
        m_log->appendSystemMessage(QStringLiteral("缩略图缓存已清空，下次打开会重新生成。"));
        QMessageBox::information(this, QStringLiteral("清空缓存"),
                                 QStringLiteral("缩略图缓存已清空。"));
    });
    connect(m_aboutAction, &QAction::triggered, this, &MainWindow::onAbout);
    connect(m_aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
    connect(m_quitAction, &QAction::triggered, this, [this]() {
        m_forceQuit = true;
        close();
    });

    // ---- 状态栏刷新 ----
    connect(m_queue, &JobQueue::queueChanged, this, &MainWindow::updateStatusBar);
}

void MainWindow::restoreSettings()
{
    QSettings settings;
    // 自动化截图/测试时用 VF_RESET_LAYOUT=1 忽略上次保存的窗口布局，保证截图可复现
    const bool ignoreLayout = qEnvironmentVariableIsSet("VF_RESET_LAYOUT");

    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (!ignoreLayout && !geometry.isEmpty())
        restoreGeometry(geometry);
    else
        resize(1360, 880);

    const QByteArray horizontal = settings.value(QStringLiteral("window/splitterH")).toByteArray();
    if (!ignoreLayout && !horizontal.isEmpty())
        m_horizontalSplitter->restoreState(horizontal);
    const QByteArray vertical = settings.value(QStringLiteral("window/splitterV")).toByteArray();
    if (!ignoreLayout && !vertical.isEmpty())
        m_verticalSplitter->restoreState(vertical);

    m_queue->setMaxConcurrent(
        settings.value(QStringLiteral("convert/maxConcurrent"), kDefaultMaxConcurrent).toInt());

    const QString outputDir = settings.value(QStringLiteral("convert/outputDir")).toString();
    m_settings->setOutputDirectory(outputDir);

    // 日志面板默认可见性取决于**屏幕**高度，而不是窗口高度：
    // restoreSettings 执行时窗口还没被 resize/show，拿 height() 判断会拿到默认值。
    // 矮屏（笔记本 768p）上"三栏 + 队列 + 日志"全展开会挤到互相重叠，
    // 所以矮屏默认收起日志（视图菜单里随时可以打开）。
    const QScreen *screen = QGuiApplication::primaryScreen();
    const int screenHeight = screen ? screen->availableGeometry().height() : 1080;
    const bool logVisibleDefault = screenHeight >= 820;
    const bool logVisible = settings.value(QStringLiteral("view/logVisible"),
                                           logVisibleDefault).toBool();
    m_toggleLogAction->setChecked(logVisible);
    m_log->setVisible(logVisible);

    const bool lightTheme = settings.value(QStringLiteral("view/lightTheme"), false).toBool();
    (lightTheme ? m_lightThemeAction : m_darkThemeAction)->setChecked(true);

    // 记住上次用的预设与字幕策略：常用 HEVC 的人不该每次启动都重选一遍
    const QString presetId = settings.value(QStringLiteral("convert/presetId")).toString();
    if (!presetId.isEmpty() && !m_settings->setPresetById(presetId))
        m_settings->setPresetById(defaultPresetId());
    const QString subtitleMode = settings.value(QStringLiteral("convert/subtitleMode")).toString();
    if (!subtitleMode.isEmpty())
        m_settings->setSubtitleMode(subtitleMode);
}

void MainWindow::saveSettings()
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/splitterH"), m_horizontalSplitter->saveState());
    settings.setValue(QStringLiteral("window/splitterV"), m_verticalSplitter->saveState());
    settings.setValue(QStringLiteral("convert/maxConcurrent"), m_queue->maxConcurrent());
    settings.setValue(QStringLiteral("convert/outputDir"), m_settings->outputDirectory());
    settings.setValue(QStringLiteral("view/logVisible"), m_toggleLogAction->isChecked());
    settings.setValue(QStringLiteral("view/lightTheme"),
                      theme::currentMode() == theme::Mode::Light);
    settings.setValue(QStringLiteral("convert/presetId"), m_settings->currentPresetId());
    settings.setValue(QStringLiteral("convert/subtitleMode"), m_settings->subtitleMode());
}

// ---------------------------------------------------------------------------
//  文件导入与探测
// ---------------------------------------------------------------------------
void MainWindow::onAddFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("选择要转换的视频/音频文件"), QString(), fmt::mediaFileDialogFilter());
    if (!files.isEmpty())
        importPaths(files);
}

void MainWindow::onAddFolder()
{
    const QString directory = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择包含媒体文件的文件夹"));
    if (!directory.isEmpty())
        importPaths({directory});
}

void MainWindow::importPaths(const QStringList &paths)
{
    const int added = m_library->addPaths(paths);
    if (added == 0) {
        if (!paths.isEmpty()) {
            statusBar()->showMessage(
                QStringLiteral("没有新增文件（可能已存在，或格式不被支持）"), 6000);
        }
    }
}

void MainWindow::loadFile(const QString &filePath)
{
    importPaths({filePath});
    m_library->selectPath(filePath);
}

void MainWindow::probeFile(const QString &filePath)
{
    m_probe->probeAsync(filePath);
}

void MainWindow::onCurrentMediaChanged(const QString &filePath)
{
    if (filePath.isEmpty() || filePath == m_currentFile)
        return;
    m_currentFile = filePath;
    m_thumbnails->invalidate();

    m_currentFileLabel->setText(QStringLiteral("当前：%1").arg(QFileInfo(filePath).fileName()));

    if (m_infoCache.contains(filePath)) {
        const MediaInfo info = m_infoCache.value(filePath);
        m_preview->showMedia(info);
        m_settings->setMediaInfo(info);
        refreshThumbnails(info);
        return;
    }

    // 还没有探测结果：先给个占位，再触发探测
    MediaInfo placeholder;
    placeholder.filePath = filePath;
    placeholder.errorMessage = QStringLiteral("正在读取媒体信息…");
    m_preview->showMedia(placeholder);
    m_preview->setHint(QStringLiteral("正在读取媒体信息…"));
    probeFile(filePath);
}

void MainWindow::onMediaProbed(const MediaInfo &info)
{
    m_infoCache.insert(info.filePath, info);
    m_library->updateItemDetails(info.filePath, info.summaryLine());

    if (info.filePath != m_currentFile)
        return;

    m_preview->showMedia(info);
    m_settings->setMediaInfo(info);
    refreshThumbnails(info);
    emit mediaReady(info.filePath);
}

void MainWindow::onMediaProbeFailed(const QString &filePath, const QString &error)
{
    m_library->markFailed(filePath, error);
    m_log->appendSystemMessage(QStringLiteral("读取失败：%1 — %2")
                                   .arg(QFileInfo(filePath).fileName(), error));
    if (filePath == m_currentFile)
        m_preview->setHint(error);
}

void MainWindow::refreshThumbnails(const MediaInfo &info)
{
    if (!info.ok || info.filePath.isEmpty())
        return;
    m_preview->setFilmstrip({}, info.durationSeconds);
    m_thumbnails->requestCover(info.filePath, info.durationSeconds);
    m_thumbnails->requestFilmstrip(info.filePath, info.durationSeconds, 12);
}

void MainWindow::onCoverReady(const QString &filePath, const QImage &image, bool fromCache)
{
    Q_UNUSED(fromCache);
    m_library->updateItemThumbnail(filePath, QPixmap::fromImage(image));
    if (filePath == m_currentFile) {
        m_preview->setCoverImage(image);
        m_preview->setHint(QString());
    }
}

void MainWindow::onFilmstripReady(const QString &filePath, const QList<QImage> &frames, double duration)
{
    if (filePath != m_currentFile)
        return;
    m_preview->setFilmstrip(frames, duration);
}

// ---------------------------------------------------------------------------
//  转换流程
// ---------------------------------------------------------------------------
void MainWindow::updateConversionScope()
{
    const int selected = m_library->selectionCount();
    const int total = m_library->count();
    if (selected > 0)
        m_settings->setConversionScope(selected, true);
    else
        m_settings->setConversionScope(total, false);
}

bool MainWindow::confirmDiskSpace(const QHash<QString, qint64> &volumeEstimates, int fileCount)
{
    QStringList shortages;
    for (auto it = volumeEstimates.constBegin(); it != volumeEstimates.constEnd(); ++it) {
        const QString volume = it.key();
        if (volume == QLatin1String("?"))
            continue;

        QStorageInfo storage(volume);
        if (!storage.isValid() || !storage.isReady())
            continue;   // 读不出剩余空间就不拦，交给 ffmpeg 自己报错

        const FreeSpaceVerdict verdict =
            SizeEstimator::evaluateFreeSpace(it.value(), storage.bytesAvailable());
        if (verdict.enough)
            continue;

        shortages << QStringLiteral("· %1\n    预计需要 %2，当前可用 %3，还差 %4")
                         .arg(QDir::toNativeSeparators(volume),
                              fmt::size(verdict.requiredBytes),
                              fmt::size(verdict.freeBytes),
                              fmt::size(verdict.missingBytes));
    }

    if (shortages.isEmpty())
        return true;

    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("磁盘空间不足"));
    box.setIcon(QMessageBox::Critical);
    box.setText(QStringLiteral("目标磁盘空间不足，已阻止本次转换。"));
    box.setInformativeText(
        QStringLiteral("共 %1 个文件待转换，但以下磁盘放不下：\n\n%2\n\n"
                       "建议：清理磁盘，或在右侧把输出目录改到其它盘，或减少一次转换的文件数量。")
            .arg(fileCount)
            .arg(shortages.join(QStringLiteral("\n\n"))));
    box.addButton(QStringLiteral("知道了"), QMessageBox::AcceptRole);
    box.exec();

    m_log->appendSystemMessage(QStringLiteral("⚠ 磁盘空间不足，已阻止转换：\n%1")
                                   .arg(shortages.join(QLatin1Char('\n'))));
    return false;
}

void MainWindow::onStartConversion()
{
    if (!m_locator->tools().valid()) {
        QMessageBox::warning(this, QStringLiteral("缺少 ffmpeg"),
                             m_locator->failureReason()
                                 + QStringLiteral("\n\n点击“确定”后可手动选择或自动下载 ffmpeg。"));
        onChooseFfmpeg();
        return;
    }

    // 有选中项就只转选中项，否则转整个媒体库
    QStringList files = m_library->selectedPaths();
    const bool fromSelection = !files.isEmpty();
    if (!fromSelection)
        files = m_library->mediaPaths();

    if (files.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("还没有文件"),
                                 QStringLiteral("请先把视频文件拖进窗口，或点击“添加文件”。"));
        return;
    }

    // 把当前预览的文件排在第一个，符合用户预期
    if (!m_library->currentPath().isEmpty()) {
        const QString current = m_library->currentPath();
        if (files.removeAll(current) > 0)
            files.prepend(current);
    }

    const ConvertPreset preset = m_settings->currentPreset();
    const ConvertOptions baseOptions = m_settings->currentOptions();
    const bool sameAsSource = m_settings->outputDirectory().isEmpty();
    const QString outputDirectory = sameAsSource ? QString() : m_settings->outputDirectory();

    // ---------------------------------------------------------------- 第一遍：预检
    // 在真正排队之前，把"不可能完成的任务"和"磁盘不够"都拦下来。
    struct Prepared {
        MediaInfo info;
        ConvertOptions options;
    };
    QList<Prepared> prepared;
    int skipped = 0;
    QStringList problems;
    qint64 estimatedBytes = 0;
    QHash<QString, qint64> volumeEstimates;   ///< 每个磁盘卷预计要写入多少字节

    QApplication::setOverrideCursor(Qt::WaitCursor);
    for (const QString &filePath : files) {
        MediaInfo info = m_infoCache.value(filePath);
        if (!info.ok) {
            // 还没探测过（或探测失败）：同步补一次，保证任务参数正确
            info = MediaProbe::probeSync(m_locator->tools().ffprobePath, filePath);
            if (info.ok) {
                m_infoCache.insert(info.filePath, info);
                m_library->updateItemDetails(filePath, info.summaryLine());
            }
        }
        if (!info.ok) {
            ++skipped;
            problems << QStringLiteral("%1：%2")
                            .arg(QFileInfo(filePath).fileName(),
                                 info.errorMessage.isEmpty() ? QStringLiteral("无法读取")
                                                             : info.errorMessage);
            m_library->markFailed(filePath, info.errorMessage);
            continue;
        }

        ConvertOptions options = baseOptions;
        options.outputPath = CommandBuilder::defaultOutputPath(filePath, preset.extension,
                                                              outputDirectory);

        // 预检命令行：像"用视频预设转换纯音频文件"这种根本不可能完成的任务，
        // 在这里就拦下来并给出人话解释，而不是塞进队列再失败。
        const CommandPlan plan = CommandBuilder::build(m_locator->tools().ffmpegPath,
                                                       info, preset, options);
        if (!plan.valid) {
            ++skipped;
            problems << QStringLiteral("%1：%2")
                            .arg(QFileInfo(filePath).fileName(), plan.errorMessage);
            continue;
        }

        const SizeEstimate estimate = SizeEstimator::estimate(info, preset, options);
        estimatedBytes += estimate.bytes;
        if (estimate.bytes > 0) {
            const QString volume = QStorageInfo(QFileInfo(options.outputPath).absolutePath())
                                       .rootPath();
            volumeEstimates[volume.isEmpty() ? QStringLiteral("?") : volume] += estimate.bytes;
        }
        prepared.append({info, options});
    }
    QApplication::restoreOverrideCursor();

    if (prepared.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("没有可转换的任务"),
                             problems.isEmpty()
                                 ? QStringLiteral("没有可转换的文件。")
                                 : problems.join(QLatin1Char('\n')));
        return;
    }

    // ---------------------------------------------------------------- 磁盘空间预检
    // 预估体积已经算出来了，就别等转一半才发现磁盘满了。
    if (!confirmDiskSpace(volumeEstimates, prepared.size()))
        return;

    // ---------------------------------------------------------------- 第二遍：入队
    int queued = 0;
    for (const Prepared &item : prepared) {
        const QString jobId = m_queue->enqueue(item.info, preset, item.options);
        if (ConvertJob *job = m_queue->job(jobId)) {
            for (const QString &warning : job->warnings())
                m_log->appendSystemMessage(QStringLiteral("%1：%2")
                                               .arg(QFileInfo(item.info.filePath).fileName(), warning));
            ++queued;
        }
    }

    QApplication::restoreOverrideCursor();

    if (queued == 0) {
        QMessageBox::warning(this, QStringLiteral("没有可转换的任务"),
                             problems.isEmpty()
                                 ? QStringLiteral("没有可转换的文件。")
                                 : problems.join(QLatin1Char('\n')));
        return;
    }

    m_conversionRunning = true;
    m_settings->setBusy(true);
    m_overallProgress->setVisible(true);
    m_overallProgress->setValue(0);
    m_queue->startScheduling();

    m_log->appendSystemMessage(QStringLiteral("已加入 %1 个转换任务（输出格式：%2）")
                                   .arg(queued).arg(preset.displayName));
    if (skipped > 0) {
        m_log->appendSystemMessage(QStringLiteral("%1 个文件被跳过：\n%2")
                                       .arg(skipped).arg(problems.join(QLatin1Char('\n'))));
    }
    updateStatusBar();
}

void MainWindow::triggerConversionForTest()
{
    onStartConversion();
}

bool MainWindow::setPresetForTest(const QString &presetId)
{
    return m_settings->setPresetById(presetId);
}

bool MainWindow::setSubtitleModeForTest(const QString &mode)
{
    return m_settings->setSubtitleMode(mode);
}

int MainWindow::selectForTest(const QString &substring)
{
    const int matched = m_library->selectMatching(substring);
    updateConversionScope();
    return matched;
}

QString MainWindow::layoutDiagnostics() const
{
    struct Entry { QString name; int minimumWidth; int width; };
    QList<Entry> entries;

    const QList<QWidget *> widgets = m_settings->findChildren<QWidget *>();
    for (QWidget *widget : widgets) {
        if (widget->isHidden())
            continue;
        QString name;
        if (auto *group = qobject_cast<QGroupBox *>(widget))
            name = QStringLiteral("GroupBox: ") + group->title();
        else if (auto *combo = qobject_cast<QComboBox *>(widget))
            name = QStringLiteral("ComboBox: ") + combo->currentText().left(22);
        else if (auto *check = qobject_cast<QCheckBox *>(widget))
            name = QStringLiteral("CheckBox: ") + check->text().left(22);
        else if (auto *button = qobject_cast<QPushButton *>(widget))
            name = QStringLiteral("Button: ") + button->text();
        else if (auto *label = qobject_cast<QLabel *>(widget))
            name = QStringLiteral("Label: ") + label->text().left(26);
        else if (qobject_cast<QDoubleSpinBox *>(widget))
            name = QStringLiteral("DoubleSpinBox");
        else if (qobject_cast<QSpinBox *>(widget))
            name = QStringLiteral("SpinBox");
        else if (qobject_cast<QLineEdit *>(widget))
            name = QStringLiteral("LineEdit");
        else if (qobject_cast<QSlider *>(widget))
            name = QStringLiteral("Slider");
        else
            name = QString::fromLatin1(widget->metaObject()->className());
        entries.append({name, widget->minimumSizeHint().width(), widget->width()});
    }
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return a.minimumWidth > b.minimumWidth;
    });

    QString text = QStringLiteral("窗口 %1x%2，日志面板可见=%3，用户手动切换过=%4\n"
                                  "设置面板宽度 = %5，最小宽度需求 = %6\n\n按最小宽度需求排序（前 15 项）：\n")
                       .arg(width()).arg(height())
                       .arg(m_log && m_log->isVisible() ? QStringLiteral("是") : QStringLiteral("否"))
                       .arg(m_logVisibilityUserSet ? QStringLiteral("是") : QStringLiteral("否"))
                       .arg(m_settings->width())
                       .arg(m_settings->minimumSizeHint().width());
    const int count = qMin(15, entries.size());
    for (int i = 0; i < count; ++i) {
        const Entry &entry = entries.at(i);
        text += QStringLiteral("  %1  min=%2  now=%3\n")
                    .arg(entry.name, -46)
                    .arg(entry.minimumWidth, 5)
                    .arg(entry.width, 5);
    }
    return text;
}

void MainWindow::onCancelAll()
{
    m_queue->cancelAll();
    m_log->appendSystemMessage(QStringLiteral("已请求取消所有正在进行的任务。"));
}

void MainWindow::onPauseAll()
{
    m_queue->pauseAll();
    m_log->appendSystemMessage(QStringLiteral("已暂停所有正在进行的任务。"));
}

void MainWindow::onResumeAll()
{
    m_queue->resumeAll();
    m_log->appendSystemMessage(QStringLiteral("已继续所有暂停的任务。"));
}

void MainWindow::onJobAdded(vf::ConvertJob *job)
{
    m_jobs->addJob(job);
    connect(job, &ConvertJob::logAppended, this, [this](const QString &jobId, const QString &text) {
        m_log->appendLog(jobId, text);
    });
    updateStatusBar();
}

void MainWindow::onJobFinished(vf::ConvertJob *job, vf::JobState state)
{
    m_jobs->updateJob(job);
    if (state == JobState::Completed) {
        const QString verified = job->verification().summary;
        m_log->appendSystemMessage(QStringLiteral("✅ 完成：%1 → %2%3")
                                       .arg(QFileInfo(job->mediaInfo().filePath).fileName(),
                                            QDir::toNativeSeparators(job->outputPath()),
                                            verified.isEmpty()
                                                ? QString()
                                                : QStringLiteral("\n    已校验产物：%1").arg(verified)));
    } else if (state == JobState::Failed) {
        m_log->appendSystemMessage(QStringLiteral("❌ 失败：%1\n%2")
                                       .arg(QFileInfo(job->mediaInfo().filePath).fileName(),
                                            job->errorMessage()));
    }
    updateStatusBar();
    recomputeBusyState();
}

void MainWindow::onAllJobsFinished()
{
    m_conversionRunning = false;
    m_settings->setBusy(false);
    m_overallProgress->setVisible(false);
    setWindowTitle(QStringLiteral("VideoForge · 视频格式转换器"));

    const int completed = m_queue->finishedCount();
    const int failed = m_queue->failedCount();

    QString message = QStringLiteral("全部任务已结束：成功 %1 个").arg(completed);
    if (failed > 0)
        message += QStringLiteral("，失败 %1 个（详情见运行日志）").arg(failed);

    m_log->appendSystemMessage(message);
    statusBar()->showMessage(message, 12000);

    notify(failed > 0 ? QStringLiteral("转换结束（有失败）") : QStringLiteral("转换完成"),
           message, failed > 0);
    updateStatusBar();
}

void MainWindow::recomputeBusyState()
{
    const bool busy = m_queue->isBusy();
    m_settings->setBusy(busy);
    m_startAction->setEnabled(!busy);
    if (!busy) {
        m_conversionRunning = false;
        m_overallProgress->setVisible(false);
        setWindowTitle(QStringLiteral("VideoForge · 视频格式转换器"));
    }
}

void MainWindow::onJobSelectionChanged(const QString &jobId)
{
    if (ConvertJob *job = m_queue->job(jobId)) {
        m_log->setCommand(jobId, job->displayCommand());
        if (!job->errorMessage().isEmpty())
            m_log->appendSystemMessage(QStringLiteral("%1：%2")
                                           .arg(QFileInfo(job->mediaInfo().filePath).fileName(),
                                                job->errorMessage()));
    } else {
        m_log->setCommand(QString(), QString());
    }
}

bool MainWindow::downloadFfmpeg()
{
    FfmpegDownloadDialog dialog(this);
    m_log->appendSystemMessage(QStringLiteral("开始自动下载 ffmpeg…"));
    const int result = dialog.runAndDownload();
    if (result != QDialog::Accepted || dialog.installedFfmpegPath().isEmpty()) {
        m_log->appendSystemMessage(QStringLiteral("自动下载未完成。"));
        return false;
    }

    if (!m_locator->useExecutable(dialog.installedFfmpegPath())) {
        QMessageBox::warning(this, QStringLiteral("安装后校验失败"),
                             QStringLiteral("下载完成的 ffmpeg 无法正常执行：\n%1")
                                 .arg(QDir::toNativeSeparators(dialog.installedFfmpegPath())));
        return false;
    }

    updateFfmpegStatus();
    m_log->appendSystemMessage(QStringLiteral("✅ ffmpeg %1 已就绪：%2")
                                   .arg(m_locator->tools().version,
                                        QDir::toNativeSeparators(m_locator->tools().ffmpegPath)));
    refreshHardwareCapabilities();
    QMessageBox::information(this, QStringLiteral("安装成功"),
                             QStringLiteral("ffmpeg %1 已安装并启用。\n%2")
                                 .arg(m_locator->tools().version,
                                      QDir::toNativeSeparators(m_locator->tools().ffmpegPath)));
    return true;
}

void MainWindow::refreshHardwareCapabilities()
{
    if (!m_locator->tools().valid())
        return;
    const QString ffmpegPath = m_locator->tools().ffmpegPath;
    auto *watcher = new QFutureWatcher<QList<HardwareCapability>>(this);
    connect(watcher, &QFutureWatcherBase::finished, this, [this, watcher]() {
        const QList<HardwareCapability> capabilities = watcher->result();
        watcher->deleteLater();
        m_hwDetectionDone = true;
        m_settings->setHardwareCapabilities(capabilities);

        // 头部胶囊显示硬件加速能力，让"能不能加速"一眼可见
        if (!m_accelChip)
            return;
        QStringList available;
        for (const HardwareCapability &capability : capabilities) {
            if (capability.available)
                available << capability.name;
        }
        if (available.isEmpty()) {
            m_accelChip->setText(QStringLiteral("软件编码 · CPU"));
            m_accelChip->setColor(theme::mutedText());
            m_accelChip->setGlyph(icons::Glyph::Gauge);
        } else {
            m_accelChip->setText(QStringLiteral("硬件加速 · %1").arg(available.first()));
            m_accelChip->setColor(theme::successColor());
            m_accelChip->setGlyph(icons::Glyph::Bolt);
        }
    });
    watcher->setFuture(QtConcurrent::run([ffmpegPath]() {
        HwAccelDetector detector;
        return detector.detect(ffmpegPath);
    }));
}

void MainWindow::onChooseFfmpeg()
{
    // 没找到 ffmpeg 时先问一句：要不要一键下载？手动选路径是备选。
    if (!m_locator->tools().valid()) {
        QMessageBox ask(this);
        ask.setWindowTitle(QStringLiteral("需要 ffmpeg"));
        ask.setIcon(QMessageBox::Question);
        ask.setText(QStringLiteral("VideoForge 需要 ffmpeg 才能转换视频，当前没有找到。"));
        ask.setInformativeText(m_locator->failureReason()
                               + QStringLiteral("\n\n要现在自动下载安装吗？（约 115 MB）"));
        QPushButton *downloadButton = ask.addButton(QStringLiteral("自动下载并安装"),
                                                    QMessageBox::AcceptRole);
        QPushButton *manualButton = ask.addButton(QStringLiteral("我自己指定路径"),
                                                  QMessageBox::ActionRole);
        ask.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
        ask.exec();
        if (ask.clickedButton() == downloadButton) {
            downloadFfmpeg();
            return;
        }
        if (ask.clickedButton() != manualButton)
            return;
    }

    const QString filter = QStringLiteral("ffmpeg 可执行文件 (ffmpeg.exe);;所有文件 (*.*)");
    QString startDirectory;
    if (!m_locator->tools().ffmpegPath.isEmpty())
        startDirectory = QFileInfo(m_locator->tools().ffmpegPath).absolutePath();

    const QString filePath = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择 ffmpeg.exe（同目录下需要有 ffprobe.exe）"),
        startDirectory, filter);
    if (filePath.isEmpty())
        return;

    if (m_locator->useExecutable(filePath)) {
        updateFfmpegStatus();
        m_log->appendSystemMessage(QStringLiteral("已设置 ffmpeg %1：%2")
                                       .arg(m_locator->tools().version,
                                            QDir::toNativeSeparators(filePath)));
        QMessageBox::information(this, QStringLiteral("设置成功"),
                                 QStringLiteral("已启用 ffmpeg %1\n%2")
                                     .arg(m_locator->tools().version,
                                          QDir::toNativeSeparators(m_locator->tools().ffmpegPath)));

        // 换 ffmpeg 后重新探测硬件能力
        refreshHardwareCapabilities();
    } else {
        QMessageBox::warning(
            this, QStringLiteral("设置失败"),
            QStringLiteral("这个文件不能正常执行，或者同目录下找不到 ffprobe.exe。\n\n"
                           "请选择 ffmpeg 解压包 bin 目录里的 ffmpeg.exe。"));
    }
}

void MainWindow::onToggleTheme()
{
    const theme::Mode mode = theme::toggle();
    theme::apply(*qApp, mode);
}

void MainWindow::onAbout()
{
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("关于 VideoForge"));
    box.setIconPixmap(appicon::logo(64));
    box.setTextFormat(Qt::RichText);
    box.setText(QStringLiteral(
        "<h3>VideoForge 视频格式转换器 <span style='color:#4C8DFF'>v1.0.0</span></h3>"
        "<p>一个用 Qt 6 + C++ 写的 Windows 桌面客户端，通过 ffmpeg 命令行完成音视频格式转换。</p>"
        "<p><b>核心能力</b><br>"
        "· 读取并展示完整的媒体信息与缩略图 / 胶片条<br>"
        "· 内置常用格式预设，一键批量转换<br>"
        "· 实时进度、速度、剩余时间与输出体积预估<br>"
        "· 可查看并复制真正执行的 ffmpeg 命令行<br>"
        "· 硬件加速（NVENC / QSV / AMF）自动探测</p>"
        "<p><b>环境</b><br>Qt %1 · ffmpeg %2</p>"
        "<p style='color:#8B939C'>开源许可：MIT</p>")
        .arg(QString::fromLatin1(qVersion()),
             m_locator->tools().version.isEmpty() ? QStringLiteral("未检测到")
                                                  : m_locator->tools().version));
    box.exec();
}

void MainWindow::notify(const QString &title, const QString &message, bool warning)
{
    if (m_tray && m_tray->isVisible())
        m_tray->showMessage(title, message, warning ? QSystemTrayIcon::Warning
                                                    : QSystemTrayIcon::Information, 6000);
}

void MainWindow::openContainingFolder(const QString &filePath)
{
    const QFileInfo info(filePath);
    if (!info.exists()) {
        // 文件可能已被移动：退而打开其所在目录
        QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
}

bool MainWindow::isMediaReady(const QString &filePath) const
{
    return m_infoCache.contains(filePath) && !m_currentFile.isEmpty();
}

// ---------------------------------------------------------------------------
//  事件
// ---------------------------------------------------------------------------
void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QStringList paths;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile())
            paths << url.toLocalFile();
    }
    if (!paths.isEmpty()) {
        importPaths(paths);
        event->acceptProposedAction();
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Ctrl+V 粘贴文件路径：从资源管理器复制文件后直接粘贴，是客户端该有的顺手功能
    if (event->matches(QKeySequence::Paste)) {
        const QString text = QApplication::clipboard()->text();
        if (!text.isEmpty()) {
            QStringList paths;
            const QStringList lines = text.split(QRegularExpression(QStringLiteral("[\\r\\n]+")),
                                                Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                const QString candidate = line.trimmed().remove(QLatin1Char('"'));
                if (QFileInfo::exists(candidate))
                    paths << candidate;
            }
            if (!paths.isEmpty()) {
                importPaths(paths);
                event->accept();
                return;
            }
        }
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::updateLogVisibilityForHeight()
{
    // 窗口太矮时自动收起日志面板，避免"三栏 + 队列 + 日志"被挤到互相重叠。
    // 用两个不同阈值（收起 780 / 展开 840）形成迟滞，防止在边界上反复抖动。
    // 用户一旦手动切换过，就不再自动干预。
    if (m_logVisibilityUserSet || !m_log)
        return;

    const int h = height();
    bool shouldShow = m_log->isVisible();
    if (h < 780)
        shouldShow = false;
    else if (h >= 840)
        shouldShow = true;

    if (shouldShow != m_log->isVisible())
        m_toggleLogAction->setChecked(shouldShow);
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    // 首次显示时窗口尺寸才算最终确定（resize() 在 show() 之前不一定派发 resizeEvent），
    // 所以这里也要判一次。
    updateLogVisibilityForHeight();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateLogVisibilityForHeight();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_forceQuit && m_queue->isBusy()) {
        QMessageBox box(this);
        box.setWindowTitle(QStringLiteral("还有任务在进行"));
        box.setIcon(QMessageBox::Question);
        box.setText(QStringLiteral("当前还有 %1 个任务没有完成，怎么处理？")
                        .arg(m_queue->runningCount() + m_queue->queuedCount()));
        box.setInformativeText(QStringLiteral("继续转换：窗口隐藏到系统托盘，任务在后台跑完。\n"
                                              "取消任务：立刻停止所有转换并退出。"));
        QPushButton *keepRunning = box.addButton(QStringLiteral("继续转换（最小化到托盘）"),
                                                 QMessageBox::AcceptRole);
        QPushButton *cancelTasks = box.addButton(QStringLiteral("取消任务并退出"),
                                                 QMessageBox::DestructiveRole);
        box.addButton(QStringLiteral("返回"), QMessageBox::RejectRole);
        box.exec();

        if (box.clickedButton() != keepRunning && box.clickedButton() != cancelTasks) {
            event->ignore();
            return;
        }
        if (box.clickedButton() == cancelTasks) {
            m_queue->cancelAll();
        } else if (m_tray) {
            hide();
            notify(QStringLiteral("VideoForge 仍在后台转换"),
                   QStringLiteral("双击托盘图标可重新打开窗口。"));
            event->ignore();
            return;
        }
    }

    saveSettings();
    event->accept();
}

} // namespace vf
