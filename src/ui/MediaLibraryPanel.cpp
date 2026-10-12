#include "ui/MediaLibraryPanel.h"
#include "core/Format.h"
#include "ui/Theme.h"

#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListWidgetItem>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QPolygonF>
#include <QPushButton>
#include <QVBoxLayout>

namespace vf {

namespace {

constexpr int kThumbWidth = 72;
constexpr int kThumbHeight = 40;
constexpr int kPathRole = Qt::UserRole + 1;
constexpr int kDetailsRole = Qt::UserRole + 2;

/// 无缩略图时的占位图：深色底 + 胶片孔，看起来像"待加载"
QPixmap placeholderThumbnail()
{
    QPixmap pixmap(kThumbWidth, kThumbHeight);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    const QRectF rect(0.5, 0.5, kThumbWidth - 1.0, kThumbHeight - 1.0);
    painter.setPen(QPen(theme::mutedText().darker(140), 1));
    painter.setBrush(theme::cardBackground().lighter(115));
    painter.drawRoundedRect(rect, 5, 5);

    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::mutedText());
    const double centerY = kThumbHeight / 2.0;
    QPolygonF triangle;
    triangle << QPointF(kThumbWidth * 0.42, centerY - 8)
             << QPointF(kThumbWidth * 0.62, centerY)
             << QPointF(kThumbWidth * 0.42, centerY + 8);
    painter.drawPolygon(triangle);
    return pixmap;
}

QPixmap toThumbnail(const QImage &image)
{
    if (image.isNull())
        return placeholderThumbnail();
    return QPixmap::fromImage(
        image.scaled(kThumbWidth * 2, kThumbHeight * 2, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

/// 递归收集目录里的媒体文件
void collectFromDirectory(const QString &directory, QStringList *out, int depth = 0)
{
    if (depth > 4)
        return;
    QDir dir(directory);
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                                                   QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (entry.isDir()) {
            collectFromDirectory(entry.absoluteFilePath(), out, depth + 1);
        } else if (fmt::isSupportedMediaFile(entry.absoluteFilePath())) {
            out->append(entry.absoluteFilePath());
        }
    }
}

} // namespace

MediaLibraryPanel::MediaLibraryPanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 6, 10);
    layout->setSpacing(8);

    auto *headerRow = new QHBoxLayout;
    headerRow->setSpacing(6);
    m_header = new QLabel(QStringLiteral("媒体库"), this);
    m_header->setProperty("role", "title");
    headerRow->addWidget(m_header);
    headerRow->addStretch();

    auto *addButton = new QPushButton(QStringLiteral("+ 添加"), this);
    addButton->setToolTip(QStringLiteral("添加视频/音频文件（也可以直接把文件拖进窗口）"));
    addButton->setCursor(Qt::PointingHandCursor);
    connect(addButton, &QPushButton::clicked, this, &MediaLibraryPanel::addRequested);
    headerRow->addWidget(addButton);

    auto *folderButton = new QPushButton(QStringLiteral("文件夹"), this);
    folderButton->setToolTip(QStringLiteral("导入整个文件夹里的媒体文件"));
    folderButton->setCursor(Qt::PointingHandCursor);
    connect(folderButton, &QPushButton::clicked, this, &MediaLibraryPanel::addFolderRequested);
    headerRow->addWidget(folderButton);

    layout->addLayout(headerRow);

    m_list = new QListWidget(this);
    m_list->setIconSize(QSize(kThumbWidth, kThumbHeight));
    m_list->setUniformItemSizes(false);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setWordWrap(false);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    layout->addWidget(m_list, 1);

    m_emptyHint = new QLabel(
        QStringLiteral("把视频文件拖到这里\n\n"
                       "· 支持 mp4 / mkv / mov / avi / webm / flv …\n"
                       "· 也支持 mp3 / wav / flac 等音频\n"
                       "· 可一次拖入多个文件或整个文件夹"),
        this);
    m_emptyHint->setProperty("role", "empty");
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setWordWrap(true);
    layout->addWidget(m_emptyHint);
    m_emptyHint->setVisible(true);

    connect(m_list, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem *current, QListWidgetItem *) {
                if (current)
                    emit currentChanged(current->data(kPathRole).toString());
            });
    connect(m_list, &QListWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        QListWidgetItem *item = m_list->itemAt(position);
        QMenu menu(this);
        QAction *removeAction = menu.addAction(QStringLiteral("从列表移除"));
        QAction *clearAction = menu.addAction(QStringLiteral("清空列表"));
        QAction *chosen = menu.exec(m_list->viewport()->mapToGlobal(position));
        if (chosen == removeAction) {
            Q_UNUSED(item);
            removeSelected();
        } else if (chosen == clearAction) {
            clearAll();
        }
    });

    setAcceptDrops(true);
}

int MediaLibraryPanel::addPaths(const QStringList &paths)
{
    QStringList files;
    for (const QString &path : paths) {
        const QFileInfo info(path);
        if (info.isDir())
            collectFromDirectory(info.absoluteFilePath(), &files);
        else if (fmt::isSupportedMediaFile(path))
            files << info.absoluteFilePath();
    }

    int added = 0;
    QString firstAdded;
    for (const QString &file : files) {
        if (findItem(file))
            continue;
        auto *item = new QListWidgetItem(m_list);
        const QFileInfo info(file);
        item->setText(QStringLiteral("%1\n%2").arg(info.fileName(), QStringLiteral("正在读取信息…")));
        item->setData(kPathRole, file);
        item->setData(kDetailsRole, QString());
        item->setIcon(QIcon(placeholderThumbnail()));
        item->setToolTip(file);
        item->setSizeHint(QSize(0, kThumbHeight + 18));
        if (firstAdded.isEmpty())
            firstAdded = file;
        ++added;
    }

    refreshHeader();
    if (added > 0) {
        emit pathsAdded(added);
        if (m_list->currentItem() == nullptr && !firstAdded.isEmpty())
            selectPath(firstAdded);
    } else if (!files.isEmpty()) {
        // 全部是重复文件：提示一下，避免用户以为程序没反应
        m_header->setText(QStringLiteral("媒体库（%1 个文件已存在）").arg(files.size()));
    }
    return added;
}

QListWidgetItem *MediaLibraryPanel::findItem(const QString &filePath) const
{
    const QString normalized = QDir::toNativeSeparators(QFileInfo(filePath).absoluteFilePath());
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem *item = m_list->item(i);
        if (QDir::toNativeSeparators(item->data(kPathRole).toString()) == normalized)
            return item;
    }
    return nullptr;
}

QStringList MediaLibraryPanel::mediaPaths() const
{
    QStringList paths;
    for (int i = 0; i < m_list->count(); ++i)
        paths << m_list->item(i)->data(kPathRole).toString();
    return paths;
}

QString MediaLibraryPanel::currentPath() const
{
    QListWidgetItem *item = m_list->currentItem();
    return item ? item->data(kPathRole).toString() : QString();
}

int MediaLibraryPanel::count() const
{
    return m_list->count();
}

void MediaLibraryPanel::updateItemDetails(const QString &filePath, const QString &details)
{
    QListWidgetItem *item = findItem(filePath);
    if (!item)
        return;
    const QString path = item->data(kPathRole).toString();
    const QFileInfo info(path);
    item->setData(kDetailsRole, details);
    item->setText(QStringLiteral("%1\n%2").arg(info.fileName(), details));
    item->setToolTip(QStringLiteral("%1\n%2").arg(path, details));
}

void MediaLibraryPanel::updateItemThumbnail(const QString &filePath, const QPixmap &pixmap)
{
    QListWidgetItem *item = findItem(filePath);
    if (!item || pixmap.isNull())
        return;
    item->setIcon(QIcon(pixmap));
}

void MediaLibraryPanel::markFailed(const QString &filePath, const QString &reason)
{
    QListWidgetItem *item = findItem(filePath);
    if (!item)
        return;
    const QFileInfo info(item->data(kPathRole).toString());
    item->setText(QStringLiteral("%1\n⚠ %2").arg(info.fileName(), reason));
    item->setForeground(theme::dangerColor());
    item->setToolTip(QStringLiteral("%1\n\n%2").arg(filePath, reason));
}

void MediaLibraryPanel::selectPath(const QString &filePath)
{
    QListWidgetItem *item = findItem(filePath);
    if (!item)
        return;
    m_list->setCurrentItem(item);
    m_list->scrollToItem(item);
}

void MediaLibraryPanel::removeSelected()
{
    const QList<QListWidgetItem *> selected = m_list->selectedItems();
    for (QListWidgetItem *item : selected)
        delete m_list->takeItem(m_list->row(item));
    refreshHeader();
    emit removeRequested();
}

void MediaLibraryPanel::clearAll()
{
    m_list->clear();
    refreshHeader();
}

void MediaLibraryPanel::refreshHeader()
{
    m_header->setText(QStringLiteral("媒体库 · %1").arg(m_list->count()));
    m_emptyHint->setVisible(m_list->count() == 0);
}

void MediaLibraryPanel::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MediaLibraryPanel::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MediaLibraryPanel::dropEvent(QDropEvent *event)
{
    QStringList paths;
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        if (url.isLocalFile())
            paths << url.toLocalFile();
    }
    if (!paths.isEmpty()) {
        addPaths(paths);
        event->acceptProposedAction();
    }
}

} // namespace vf
