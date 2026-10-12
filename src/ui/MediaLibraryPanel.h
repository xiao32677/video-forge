#pragma once

// =============================================================================
//  VideoForge - 左侧媒体库面板
//
//  客户端感的关键：支持把文件/文件夹直接拖进窗口，也支持多选、追加、去重。
// =============================================================================

#include <QListWidget>
#include <QStringList>
#include <QWidget>

class QLabel;
class QListWidgetItem;

namespace vf {

class MediaLibraryPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MediaLibraryPanel(QWidget *parent = nullptr);

    /// 添加文件（自动展开文件夹、过滤不支持的格式、跳过重复项）
    /// 返回真正新增的文件数量
    int addPaths(const QStringList &paths);

    QStringList mediaPaths() const;
    /// 当前选中的文件；没有选中时返回空列表
    QStringList selectedPaths() const;
    QString currentPath() const;
    int count() const;

    /// 媒体库内容或选中状态变化（用于刷新"将要转换 N 个文件"的提示）
    int selectionCount() const;

    /// 用探测结果更新列表项的副标题（例如 "1920x1080 · 00:01:23 · 12.4 MB"）
    void updateItemDetails(const QString &filePath, const QString &details);

    /// 更新列表项的缩略图（后台抽帧完成后回填）
    void updateItemThumbnail(const QString &filePath, const QPixmap &pixmap);

    void markFailed(const QString &filePath, const QString &reason);

    void selectPath(const QString &filePath);
    /// 按文件名子串多选（自动化测试用），返回命中数量
    int selectMatching(const QString &substring);
    void clearSelection();
    void removeSelected();
    void clearAll();

signals:
    void pathsAdded(int count);
    void currentChanged(const QString &filePath);
    /// 选中项发生变化（数量或内容）
    void selectionChanged();
    void removeRequested();
    void addRequested();
    void addFolderRequested();
    /// 右键菜单里点了"仅转换选中的文件"
    void convertSelectionRequested();

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QListWidgetItem *findItem(const QString &filePath) const;
    void refreshHeader();

    QListWidget *m_list = nullptr;
    QLabel *m_header = nullptr;
    QLabel *m_emptyHint = nullptr;
};

} // namespace vf
