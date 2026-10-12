#include "ui/JobQueuePanel.h"
#include "core/Format.h"
#include "job/ConvertJob.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/UiWidgets.h"

#include <QApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

namespace vf {

namespace {

constexpr int kJobIdRole = Qt::UserRole + 1;
constexpr int kProgressRole = Qt::UserRole + 2;

enum Column {
    ColumnFile = 0,
    ColumnState,
    ColumnProgress,
    ColumnSpeed,
    ColumnEta,
    ColumnOutput,
    ColumnCount,
};

QColor stateColor(JobState state)
{
    switch (state) {
    case JobState::Completed: return theme::successColor();
    case JobState::Failed:    return theme::dangerColor();
    case JobState::Running:   return theme::accent();
    case JobState::Paused:    return theme::warningColor();
    case JobState::Canceled:  return theme::mutedText();
    case JobState::Queued:    break;
    }
    return theme::mutedText();
}

QTableWidgetItem *makeItem(const QString &text, JobState state)
{
    auto *item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    if (state == JobState::Failed)
        item->setForeground(theme::dangerColor());
    else if (state == JobState::Completed)
        item->setForeground(theme::successColor());
    else if (state == JobState::Running)
        item->setForeground(theme::accent());
    return item;
}

} // namespace

// ---------------------------------------------------------------------------
//  进度条代理
// ---------------------------------------------------------------------------
ProgressBarDelegate::ProgressBarDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

void ProgressBarDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                const QModelIndex &index) const
{
    const double progress = index.data(kProgressRole).toDouble();
    const JobState state = static_cast<JobState>(index.data(Qt::UserRole + 3).toInt());

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // 先画单元格背景（交替行色/选中色由样式负责），再叠加进度条
    QStyleOptionViewItem backgroundOption(option);
    initStyleOption(&backgroundOption, index);
    backgroundOption.text.clear();
    QStyle *style = backgroundOption.widget ? backgroundOption.widget->style()
                                            : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &backgroundOption, painter,
                         backgroundOption.widget);

    const QRectF track = QRectF(option.rect).adjusted(7, option.rect.height() * 0.26,
                                                      -7, -option.rect.height() * 0.26);
    painter->setPen(Qt::NoPen);
    QColor trackColor = theme::mutedText();
    trackColor.setAlpha(state == JobState::Queued ? 40 : 60);
    painter->setBrush(trackColor);
    painter->drawRoundedRect(track, track.height() / 2.0, track.height() / 2.0);

    if (progress > 0.001) {
        QRectF filled = track;
        filled.setWidth(qMax(track.height(), track.width() * qBound(0.0, progress, 1.0)));

        QLinearGradient gradient(track.topLeft(), track.topRight());
        if (state == JobState::Failed || state == JobState::Canceled) {
            gradient.setColorAt(0.0, theme::dangerColor());
            gradient.setColorAt(1.0, theme::dangerColor().darker(125));
        } else if (state == JobState::Paused) {
            gradient.setColorAt(0.0, theme::warningColor());
            gradient.setColorAt(1.0, theme::warningColor().darker(120));
        } else if (state == JobState::Completed) {
            gradient.setColorAt(0.0, theme::successColor());
            gradient.setColorAt(1.0, theme::successColor().darker(118));
        } else {
            gradient.setColorAt(0.0, theme::accent());
            gradient.setColorAt(1.0, theme::accentAlt());
        }
        painter->setBrush(gradient);
        painter->drawRoundedRect(filled, filled.height() / 2.0, filled.height() / 2.0);
    }

    // 百分比文字
    const QString text = QStringLiteral("%1%").arg(static_cast<int>(progress * 100.0 + 0.5));
    QFont font = option.font;
    font.setPointSizeF(qMax(7.5, font.pointSizeF() - 1.4));
    font.setWeight(QFont::DemiBold);
    painter->setFont(font);
    painter->setPen(theme::textColor());
    painter->drawText(option.rect, Qt::AlignCenter, text);

    painter->restore();
}

// ---------------------------------------------------------------------------
//  状态胶囊代理
// ---------------------------------------------------------------------------
namespace {

class StatePillDelegate : public QStyledItemDelegate
{
public:
    explicit StatePillDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const JobState state = static_cast<JobState>(index.data(Qt::UserRole + 3).toInt());
        const QString text = index.data(Qt::DisplayRole).toString();
        const QColor color = stateColor(state);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        QStyleOptionViewItem backgroundOption(option);
        initStyleOption(&backgroundOption, index);
        backgroundOption.text.clear();
        QStyle *style = backgroundOption.widget ? backgroundOption.widget->style()
                                                : QApplication::style();
        style->drawPrimitive(QStyle::PE_PanelItemViewItem, &backgroundOption, painter,
                             backgroundOption.widget);

        QFont font = option.font;
        font.setPointSizeF(qMax(7.5, font.pointSizeF() - 1.2));
        font.setWeight(QFont::DemiBold);

        const QFontMetrics metrics(font);
        const int textWidth = metrics.horizontalAdvance(text);
        const int pillWidth = qMin(option.rect.width() - 10, textWidth + 22);
        const QRectF pill(option.rect.left() + 6,
                          option.rect.center().y() - 10,
                          qMax(46, pillWidth), 20);

        QColor fill = color;
        fill.setAlpha(state == JobState::Queued ? 26 : 40);
        QColor border = color;
        border.setAlpha(state == JobState::Queued ? 60 : 120);

        painter->setPen(QPen(border, 1.0));
        painter->setBrush(fill);
        painter->drawRoundedRect(pill, pill.height() / 2.0, pill.height() / 2.0);

        // 状态圆点
        painter->setPen(Qt::NoPen);
        painter->setBrush(color);
        painter->drawEllipse(QPointF(pill.left() + 10, pill.center().y()), 3.0, 3.0);

        painter->setFont(font);
        painter->setPen(color);
        painter->drawText(QRectF(pill.left() + 18, pill.top(),
                                 pill.width() - 22, pill.height()),
                          Qt::AlignVCenter | Qt::AlignLeft, text);

        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QSize base = QStyledItemDelegate::sizeHint(option, index);
        base.setWidth(qMax(base.width() + 20, 92));
        return base;
    }
};

} // namespace

// ---------------------------------------------------------------------------
//  面板
// ---------------------------------------------------------------------------
JobQueuePanel::JobQueuePanel(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 10);
    layout->setSpacing(6);

    auto *headerRow = new QHBoxLayout;
    headerRow->setSpacing(7);
    headerRow->addWidget(new GlyphLabel(icons::Glyph::List, 16, QColor(), this));
    auto *title = new QLabel(QStringLiteral("转换队列"), this);
    title->setProperty("role", "panelTitle");
    headerRow->addWidget(title);

    m_summary = new QLabel(QStringLiteral("暂无任务"), this);
    m_summary->setProperty("role", "hint");
    headerRow->addWidget(m_summary);
    headerRow->addStretch();

    m_clearFinishedButton = new QPushButton(QStringLiteral("清除已完成"), this);
    m_clearFinishedButton->setIcon(icons::icon(icons::Glyph::Check, theme::successColor(), 14));
    m_clearFinishedButton->setCursor(Qt::PointingHandCursor);
    connect(m_clearFinishedButton, &QPushButton::clicked, this, &JobQueuePanel::clearFinished);
    headerRow->addWidget(m_clearFinishedButton);
    layout->addLayout(headerRow);

    m_table = new QTableWidget(this);
    m_table->setColumnCount(ColumnCount);
    m_table->setHorizontalHeaderLabels({QStringLiteral("文件"), QStringLiteral("状态"),
                                        QStringLiteral("进度"), QStringLiteral("速度"),
                                        QStringLiteral("剩余"), QStringLiteral("输出文件")});
    m_table->verticalHeader()->setVisible(false);
    m_table->verticalHeader()->setDefaultSectionSize(34);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setShowGrid(false);
    m_table->setAlternatingRowColors(true);
    m_table->setFrameShape(QFrame::NoFrame);
    m_table->setContextMenuPolicy(Qt::CustomContextMenu);
    m_table->setItemDelegateForColumn(ColumnProgress, new ProgressBarDelegate(this));
    m_table->setItemDelegateForColumn(ColumnState, new StatePillDelegate(this));
    m_table->horizontalHeader()->setHighlightSections(false);
    m_table->horizontalHeader()->setSectionResizeMode(ColumnFile, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(ColumnOutput, QHeaderView::Stretch);
    for (int column : {ColumnState, ColumnProgress, ColumnSpeed, ColumnEta})
        m_table->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    m_table->setMinimumHeight(140);

    layout->addWidget(m_table, 1);

    connect(m_table, &QTableWidget::itemSelectionChanged, this, [this]() {
        emit selectionChanged(selectedJobId());
    });
    connect(m_table, &QTableWidget::customContextMenuRequested, this, [this](const QPoint &position) {
        const int row = m_table->rowAt(position.y());
        if (row < 0)
            return;
        QTableWidgetItem *first = m_table->item(row, ColumnFile);
        if (!first)
            return;
        const QString jobId = first->data(kJobIdRole).toString();
        const JobState state = static_cast<JobState>(m_table->item(row, ColumnFile)
                                                         ->data(Qt::UserRole + 3).toInt());

        QMenu menu(this);
        QAction *pauseAction = menu.addAction(QStringLiteral("暂停"));
        QAction *resumeAction = menu.addAction(QStringLiteral("继续"));
        QAction *cancelAction = menu.addAction(QStringLiteral("取消"));
        menu.addSeparator();
        QAction *retryAction = menu.addAction(QStringLiteral("重新转换"));
        QAction *playAction = menu.addAction(QStringLiteral("播放输出文件"));
        QAction *openAction = menu.addAction(QStringLiteral("打开所在文件夹"));
        menu.addSeparator();
        QAction *removeAction = menu.addAction(QStringLiteral("从列表移除"));

        pauseAction->setEnabled(state == JobState::Running);
        resumeAction->setEnabled(state == JobState::Paused);
        cancelAction->setEnabled(state == JobState::Running || state == JobState::Queued
                                 || state == JobState::Paused);
        retryAction->setEnabled(state == JobState::Failed || state == JobState::Canceled);
        playAction->setEnabled(state == JobState::Completed);
        openAction->setEnabled(state == JobState::Completed);

        QAction *chosen = menu.exec(m_table->viewport()->mapToGlobal(position));
        if (!chosen)
            return;
        if (chosen == pauseAction)         emit pauseRequested(jobId);
        else if (chosen == resumeAction)   emit resumeRequested(jobId);
        else if (chosen == cancelAction)   emit cancelRequested(jobId);
        else if (chosen == retryAction)    emit retryRequested(jobId);
        else if (chosen == playAction)     emit playOutputRequested(jobId);
        else if (chosen == openAction)     emit openOutputRequested(jobId);
        else if (chosen == removeAction)   emit removeRequested(jobId);
    });

    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        QTableWidgetItem *first = m_table->item(row, ColumnFile);
        if (!first)
            return;
        const JobState state = static_cast<JobState>(first->data(Qt::UserRole + 3).toInt());
        const QString jobId = first->data(kJobIdRole).toString();
        if (state == JobState::Completed)
            emit playOutputRequested(jobId);
        else if (state == JobState::Failed || state == JobState::Canceled)
            emit retryRequested(jobId);
    });
}

int JobQueuePanel::rowForJob(const QString &jobId) const
{
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem *item = m_table->item(row, ColumnFile);
        if (item && item->data(kJobIdRole).toString() == jobId)
            return row;
    }
    return -1;
}

void JobQueuePanel::addJob(ConvertJob *job)
{
    if (!job || rowForJob(job->jobId()) >= 0)
        return;

    const int row = m_table->rowCount();
    m_table->insertRow(row);

    auto *fileItem = makeItem(QFileInfo(job->mediaInfo().filePath).fileName(), job->state());
    fileItem->setData(kJobIdRole, job->jobId());
    fileItem->setData(Qt::UserRole + 3, static_cast<int>(job->state()));
    fileItem->setToolTip(job->mediaInfo().filePath);
    fileItem->setIcon(icons::icon(job->mediaInfo().hasVideo() ? icons::Glyph::Film
                                                              : icons::Glyph::Music,
                                  theme::mutedText(), 14));
    m_table->setItem(row, ColumnFile, fileItem);

    m_table->setItem(row, ColumnState, makeItem(jobStateText(job->state()), job->state()));

    auto *progressItem = new QTableWidgetItem;
    progressItem->setData(Qt::UserRole + 3, static_cast<int>(job->state()));
    progressItem->setData(kProgressRole, 0.0);
    m_table->setItem(row, ColumnProgress, progressItem);

    m_table->setItem(row, ColumnSpeed, makeItem(QStringLiteral("--"), job->state()));
    m_table->setItem(row, ColumnEta, makeItem(QStringLiteral("--"), job->state()));

    auto *outputItem = makeItem(QFileInfo(job->outputPath()).fileName(), job->state());
    outputItem->setToolTip(job->outputPath());
    m_table->setItem(row, ColumnOutput, outputItem);

    updateJob(job);
    refreshSummary();
}

void JobQueuePanel::updateJob(ConvertJob *job)
{
    if (!job)
        return;
    const int row = rowForJob(job->jobId());
    if (row < 0)
        return;

    const JobState state = job->state();

    if (QTableWidgetItem *item = m_table->item(row, ColumnFile)) {
        item->setData(Qt::UserRole + 3, static_cast<int>(state));
        item->setData(Qt::ForegroundRole, QVariant());
    }

    if (QTableWidgetItem *item = m_table->item(row, ColumnState)) {
        item->setText(jobStateText(state));
        QColor color = stateColor(state);
        item->setForeground(color);
        if (state == JobState::Failed && !job->errorMessage().isEmpty())
            item->setToolTip(job->errorMessage());
        else if (!job->warnings().isEmpty())
            item->setToolTip(job->warnings().join(QLatin1Char('\n')));
    }

    if (QTableWidgetItem *item = m_table->item(row, ColumnProgress)) {
        item->setData(Qt::UserRole + 3, static_cast<int>(state));
        item->setData(kProgressRole, job->progress());
    }

    if (QTableWidgetItem *item = m_table->item(row, ColumnSpeed)) {
        if (state == JobState::Running)
            item->setText(fmt::speed(job->speed()));
        else if (state == JobState::Completed)
            item->setText(QStringLiteral("完成"));
        else
            item->setText(QStringLiteral("--"));
    }

    if (QTableWidgetItem *item = m_table->item(row, ColumnEta)) {
        if (state == JobState::Completed)
            item->setText(fmt::duration(job->elapsedSeconds()));
        else if (state == JobState::Running) {
            const double eta = job->etaSeconds();
            item->setText(eta >= 0 ? QStringLiteral("约 %1").arg(fmt::duration(eta))
                                   : QStringLiteral("计算中…"));
        } else if (state == JobState::Paused) {
            item->setText(QStringLiteral("已暂停"));
        } else {
            item->setText(QStringLiteral("--"));
        }
    }

    if (QTableWidgetItem *item = m_table->item(row, ColumnOutput)) {
        if (state == JobState::Completed) {
            item->setForeground(theme::successColor());
            item->setIcon(icons::icon(icons::Glyph::Check, theme::successColor(), 13));
        } else {
            item->setData(Qt::ForegroundRole, QVariant());
            item->setIcon(QIcon());
        }
    }

    m_table->viewport()->update();
    refreshSummary();
}

void JobQueuePanel::removeJob(const QString &jobId)
{
    const int row = rowForJob(jobId);
    if (row >= 0)
        m_table->removeRow(row);
    refreshSummary();
}

void JobQueuePanel::clearAll()
{
    m_table->setRowCount(0);
    refreshSummary();
}

void JobQueuePanel::clearFinished()
{
    for (int row = m_table->rowCount() - 1; row >= 0; --row) {
        QTableWidgetItem *item = m_table->item(row, ColumnFile);
        if (!item)
            continue;
        const JobState state = static_cast<JobState>(item->data(Qt::UserRole + 3).toInt());
        if (state == JobState::Completed || state == JobState::Failed || state == JobState::Canceled)
            m_table->removeRow(row);
    }
    refreshSummary();
}

QString JobQueuePanel::selectedJobId() const
{
    const int row = m_table->currentRow();
    if (row < 0)
        return QString();
    QTableWidgetItem *item = m_table->item(row, ColumnFile);
    return item ? item->data(kJobIdRole).toString() : QString();
}

int JobQueuePanel::rowCount() const
{
    return m_table->rowCount();
}

void JobQueuePanel::refreshSummary()
{
    int completed = 0;
    int failed = 0;
    int running = 0;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        QTableWidgetItem *item = m_table->item(row, ColumnFile);
        if (!item)
            continue;
        const JobState state = static_cast<JobState>(item->data(Qt::UserRole + 3).toInt());
        if (state == JobState::Completed)
            ++completed;
        else if (state == JobState::Failed)
            ++failed;
        else if (state == JobState::Running)
            ++running;
    }

    if (m_table->rowCount() == 0) {
        m_summary->setText(QStringLiteral("暂无任务"));
        m_clearFinishedButton->setEnabled(false);
        return;
    }
    m_summary->setText(QStringLiteral("共 %1 个任务 · 进行中 %2 · 已完成 %3 · 失败 %4")
                           .arg(m_table->rowCount()).arg(running).arg(completed).arg(failed));
    m_clearFinishedButton->setEnabled(completed + failed > 0);
}

} // namespace vf
