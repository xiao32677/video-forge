#include "ui/FfmpegDownloadDialog.h"

#include "core/FfmpegInstaller.h"
#include "core/Format.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/UiWidgets.h"

#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

namespace vf {

FfmpegDownloadDialog::FfmpegDownloadDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("自动下载 ffmpeg"));
    setModal(true);
    setMinimumWidth(520);

    m_installer = new FfmpegInstaller(this);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 16);
    layout->setSpacing(12);

    auto *titleRow = new QHBoxLayout;
    titleRow->setSpacing(9);
    titleRow->addWidget(new GlyphLabel(icons::Glyph::Download, 20, QColor(), this));
    auto *title = new QLabel(QStringLiteral("一键下载并安装 ffmpeg"), this);
    title->setProperty("role", "heroTitle");
    titleRow->addWidget(title);
    titleRow->addStretch();
    layout->addLayout(titleRow);

    m_headline = new QLabel(this);
    m_headline->setWordWrap(true);
    m_headline->setText(
        QStringLiteral("会从 gyan.dev 下载官方 essentials 构建（约 115 MB，含 ffmpeg 与 ffprobe），\n"
                       "自动解压到程序目录下的 <code>ffmpeg/bin</code> 并立即启用。\n\n"
                       "下载来源与许可见项目 README；如果你已有 ffmpeg，直接选“手动指定路径”更快。"));
    m_headline->setTextFormat(Qt::RichText);
    m_headline->setProperty("role", "hint");
    layout->addWidget(m_headline);

    m_progress = new QProgressBar(this);
    m_progress->setRange(0, 1000);
    m_progress->setValue(0);
    m_progress->setTextVisible(true);
    m_progress->setFormat(QStringLiteral("准备中…"));
    layout->addWidget(m_progress);

    m_detail = new QLabel(this);
    m_detail->setProperty("role", "hint");
    m_detail->setWordWrap(true);
    m_detail->setMinimumHeight(34);
    layout->addWidget(m_detail);

    auto *buttonRow = new QHBoxLayout;
    buttonRow->addStretch();

    auto *manualButton = new QPushButton(QStringLiteral("手动指定路径"), this);
    manualButton->setProperty("ghost", true);
    connect(manualButton, &QPushButton::clicked, this, [this]() {
        m_installer->cancel();
        reject();
    });
    buttonRow->addWidget(manualButton);

    m_startButton = new QPushButton(QStringLiteral("开始下载"), this);
    m_startButton->setProperty("accent", true);
    m_startButton->setIcon(icons::icon(icons::Glyph::Download,
                                       QColor(QStringLiteral("#FFFFFF")), 15));
    m_startButton->setDefault(true);
    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        setBusy(true);
        m_installedPath.clear();
        m_progress->setValue(0);
        m_progress->setFormat(QStringLiteral("0%"));
        m_installer->start();
    });
    buttonRow->addWidget(m_startButton);

    m_closeButton = new QPushButton(QStringLiteral("取消"), this);
    connect(m_closeButton, &QPushButton::clicked, this, [this]() {
        m_installer->cancel();
        reject();
    });
    buttonRow->addWidget(m_closeButton);

    layout->addLayout(buttonRow);

    connect(m_installer, &FfmpegInstaller::downloadProgress, this,
            [this](qint64 received, qint64 total) {
                if (total > 0) {
                    m_progress->setRange(0, 1000);
                    m_progress->setValue(static_cast<int>(1000.0 * received / total));
                    m_progress->setFormat(QStringLiteral("%1 / %2  (%p%)")
                                              .arg(fmt::size(received), fmt::size(total)));
                } else {
                    // 服务器没给长度：用忙碌态表示"正在进行"
                    m_progress->setRange(0, 0);
                    m_progress->setFormat(QStringLiteral("已下载 %1").arg(fmt::size(received)));
                }
            });
    connect(m_installer, &FfmpegInstaller::statusMessage, this,
            [this](const QString &text) { m_detail->setText(text); });
    connect(m_installer, &FfmpegInstaller::finished, this,
            [this](bool ok, const QString &ffmpegPath, const QString &errorMessage) {
                setBusy(false);
                if (ok) {
                    m_installedPath = ffmpegPath;
                    m_progress->setRange(0, 1000);
                    m_progress->setValue(1000);
                    m_progress->setFormat(QStringLiteral("完成"));
                    m_detail->setText(QStringLiteral("已安装到：%1")
                                          .arg(QDir::toNativeSeparators(ffmpegPath)));
                    accept();
                    return;
                }
                m_progress->setRange(0, 1000);
                m_progress->setValue(0);
                m_progress->setFormat(QStringLiteral("未完成"));
                m_detail->setText(QStringLiteral("⚠ %1").arg(errorMessage));
                m_startButton->setText(QStringLiteral("重试"));
            });
}

void FfmpegDownloadDialog::appendStatus(const QString &text)
{
    m_detail->setText(text);
}

void FfmpegDownloadDialog::setBusy(bool busy)
{
    m_startButton->setEnabled(!busy);
    if (busy)
        m_startButton->setText(QStringLiteral("下载中…"));
    else if (m_installedPath.isEmpty())
        m_startButton->setText(QStringLiteral("开始下载"));
}

int FfmpegDownloadDialog::runAndDownload()
{
    show();
    // 打开即开始，省掉一次点击
    QTimer::singleShot(0, m_startButton, &QPushButton::click);
    return exec();
}

} // namespace vf
