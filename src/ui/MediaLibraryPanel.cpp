#include "ui/MediaLibraryPanel.h"
#include "core/Format.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/UiWidgets.h"

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
#include <QPainterPath>
#include <QPolygonF>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QStyle>
#include <QVBoxLayout>

namespace vf {

namespace {

constexpr int kThumbWidth = 78;
constexpr int kThumbHeight = 46;
constexpr int kRowHeight = 66;

constexpr int kPathRole = Qt::UserRole + 1;
constexpr int kDetailsRole = Qt::UserRole + 2;
constexpr int kNameRole = Qt::UserRole + 3;
constexpr int kFailedRole = Qt::UserRole + 4;
constexpr int kThumbRole = Qt::UserRole + 5;

/// 无缩略图时的占位图
QPixmap placeholderThumbnail()
{
    QPixmap pixmap(kThumbWidth, kThumbHeight);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme::elevatedBackground());
    painter.drawRoundedRect(QRectF(0, 0, kThumbWidth, kThumbHeight), 6, 6);

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
    return QPixmap::fromImage(image.scaled(kThumbWidth * 2, kThumbHeight * 2,
                                           Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

/// 根据扩展名给出一个"是视频还是音频"的小图标
icons::Glyph glyphForFile(const QString &filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    static const QStringList audioSuffixes{
        QStringLiteral("mp3"), QStringLiteral("wav"), QStringLiteral("flac"), QStringLiteral("aac"),
        QStringLiteral("m4a"), QStringLiteral("ogg"), QStringLiteral("wma"), QStringLiteral("opus"),
        QStringLiteral("aiff"), QStringLiteral("ape"), QStringLiteral("mka"), QStringLiteral("ac3"),
    };
    return audioSuffixes.contains(suffix) ? icons::Glyph::Music : icons::Glyph::Film;
}

/// 媒体库列表项的自绘代理：圆角卡片 + 缩略图 + 两行文字
class MediaItemDelegate : public QStyledItemDelegate
{
public:
    explicit MediaItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        return QSize(200, kRowHeight);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const bool failed = index.data(kFailedRole).toBool();
        const QString name = index.data(kNameRole).toString();
        const QString details = index.data(kDetailsRole).toString();
        const QPixmap thumb = index.data(kThumbRole).value<QPixmap>();

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QRectF card = QRectF(option.rect).adjusted(4, 3, -4, -3);

        // ---- 背景 ----
        if (selected) {
            QColor fill = theme::accent();
            fill.setAlpha(46);
            painter->setBrush(fill);
            painter->setPen(QPen(QColor(theme::accent().red(), theme::accent().green(),
                                        theme::accent().blue(), 150), 1.0));
            painter->drawRoundedRect(card, 9, 9);
        } else if (hovered) {
            painter->setBrush(theme::elevatedBackground());
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(card, 9, 9);
        }

        // ---- 缩略图 ----
        const QRectF thumbRect(card.left() + 8,
                               card.center().y() - kThumbHeight / 2.0,
                               kThumbWidth, kThumbHeight);
        QPainterPath clip;
        clip.addRoundedRect(thumbRect, 7, 7);
        painter->save();
        painter->setClipPath(clip);
        if (thumb.isNull()) {
            painter->drawPixmap(thumbRect.toRect(), placeholderThumbnail());
        } else {
            const QPixmap scaled = thumb.scaled(thumbRect.size().toSize(),
                                                Qt::KeepAspectRatioByExpanding,
                                                Qt::SmoothTransformation);
            const QRect source((scaled.width() - static_cast<int>(thumbRect.width())) / 2,
                               (scaled.height() - static_cast<int>(thumbRect.height())) / 2,
                               static_cast<int>(thumbRect.width()),
                               static_cast<int>(thumbRect.height()));
            painter->drawPixmap(thumbRect.toRect(), scaled, source);
        }
        painter->restore();
        painter->setBrush(Qt::NoBrush);
        painter->setPen(QPen(theme::borderColor(), 1.0));
        painter->drawRoundedRect(thumbRect, 7, 7);

        // 缩略图右下角的小徽标：视频 / 音频
        const int badgeSize = 16;
        const QRectF badgeRect(thumbRect.right() - badgeSize - 3,
                               thumbRect.bottom() - badgeSize - 3,
                               badgeSize, badgeSize);
        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(0, 0, 0, 150));
        painter->drawEllipse(badgeRect);
        const QPixmap badgePixmap = icons::pixmap(
            failed ? icons::Glyph::Warning : glyphForFile(index.data(kPathRole).toString()),
            failed ? theme::dangerColor() : QColor(255, 255, 255), 11);
        painter->drawPixmap(badgeRect.toRect(), badgePixmap);

        // ---- 文字 ----
        const qreal textLeft = thumbRect.right() + 11;
        const qreal textRight = card.right() - 8;
        const qreal textWidth = qMax(20.0, textRight - textLeft);

        QFont nameFont = option.font;
        nameFont.setPointSizeF(option.font.pointSizeF() + 0.2);
        nameFont.setWeight(QFont::DemiBold);
        painter->setFont(nameFont);
        painter->setPen(failed ? theme::dangerColor() : theme::textColor());
        const QString elidedName = QFontMetrics(nameFont).elidedText(name, Qt::ElideMiddle,
                                                                    static_cast<int>(textWidth));
        const QRectF nameRect(textLeft, card.center().y() - 21, textWidth, 20);
        painter->drawText(nameRect, Qt::AlignVCenter | Qt::AlignLeft, elidedName);

        QFont detailFont = option.font;
        detailFont.setPointSizeF(qMax(7.0, option.font.pointSizeF() - 1.4));
        painter->setFont(detailFont);
        painter->setPen(failed ? theme::dangerColor() : theme::mutedText());
        const QString elidedDetails = QFontMetrics(detailFont).elidedText(
            failed && !details.isEmpty() ? details : details, Qt::ElideRight,
            static_cast<int>(textWidth));
        const QRectF detailRect(textLeft, card.center().y() + 1, textWidth, 18);
        painter->drawText(detailRect, Qt::AlignVCenter | Qt::AlignLeft, elidedDetails);

        painter->restore();
    }
};

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
    headerRow->setSpacing(7);
    headerRow->addWidget(new GlyphLabel(icons::Glyph::Film, 16, QColor(), this));
    m_header = new QLabel(QStringLiteral("媒体库"), this);
    m_header->setProperty("role", "panelTitle");
    headerRow->addWidget(m_header);
    headerRow->addStretch();

    auto *addButton = new QPushButton(QStringLiteral("添加"), this);
    addButton->setIcon(icons::icon(icons::Glyph::Add, theme::accent(), 14));
    addButton->setToolTip(QStringLiteral("添加视频/音频文件，也可以直接把文件拖进窗口（Ctrl+O）"));
    addButton->setCursor(Qt::PointingHandCursor);
    connect(addButton, &QPushButton::clicked, this, &MediaLibraryPanel::addRequested);
    headerRow->addWidget(addButton);

    auto *folderButton = new QPushButton(this);
    folderButton->setIcon(icons::icon(icons::Glyph::Folder, theme::mutedText(), 14));
    folderButton->setToolTip(QStringLiteral("导入整个文件夹里的媒体文件（Ctrl+Shift+O）"));
    folderButton->setCursor(Qt::PointingHandCursor);
    folderButton->setFixedWidth(36);
    connect(folderButton, &QPushButton::clicked, this, &MediaLibraryPanel::addFolderRequested);
    headerRow->addWidget(folderButton);

    layout->addLayout(headerRow);

    m_list = new QListWidget(this);
    m_list->setItemDelegate(new MediaItemDelegate(m_list));
    m_list->setUniformItemSizes(true);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setContextMenuPolicy(Qt::CustomContextMenu);
    m_list->setWordWrap(false);
    m_list->setMouseTracking(true);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setFrameShape(QFrame::NoFrame);
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
        item->setData(kPathRole, file);
        item->setData(kNameRole, info.fileName());
        item->setData(kDetailsRole, QStringLiteral("正在读取信息…"));
        item->setData(kFailedRole, false);
        item->setData(kThumbRole, placeholderThumbnail());
        item->setToolTip(file);
        item->setSizeHint(QSize(0, kRowHeight));
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
    item->setData(kDetailsRole, details);
    item->setData(kFailedRole, false);
    item->setToolTip(QStringLiteral("%1\n%2").arg(path, details));
}

void MediaLibraryPanel::updateItemThumbnail(const QString &filePath, const QPixmap &pixmap)
{
    QListWidgetItem *item = findItem(filePath);
    if (!item || pixmap.isNull())
        return;
    item->setData(kThumbRole, pixmap);
}

void MediaLibraryPanel::markFailed(const QString &filePath, const QString &reason)
{
    QListWidgetItem *item = findItem(filePath);
    if (!item)
        return;
    item->setData(kFailedRole, true);
    item->setData(kDetailsRole, reason);
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
