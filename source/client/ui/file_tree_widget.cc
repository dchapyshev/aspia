//
// PROJECT:         Aspia
// FILE:            client/ui/file_tree_widget.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/file_tree_widget.h"

#include <QApplication>
#include <QMouseEvent>

#include "client/ui/file_item.h"
#include "client/ui/file_item_delegate.h"
#include "client/ui/file_item_drag.h"
#include "client/ui/file_item_mime_data.h"

namespace aspia {

FileTreeWidget::FileTreeWidget(QWidget* parent)
    : QTreeWidget(parent)
{
    setAcceptDrops(true);

    FileColumnDelegate* column_delegate = new FileColumnDelegate(this);

    connect(column_delegate, &FileColumnDelegate::editingFinished,
            this, &FileTreeWidget::onEditingFinished);

    setItemDelegateForColumn(0, column_delegate);
    setItemDelegateForColumn(1, new FileReadOnlyColumnDelegate(this));
    setItemDelegateForColumn(2, new FileReadOnlyColumnDelegate(this));
    setItemDelegateForColumn(3, new FileReadOnlyColumnDelegate(this));
}

void FileTreeWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        start_pos_ = event->pos();

    QTreeWidget::mousePressEvent(event);
}

void FileTreeWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        int distance = (event->pos() - start_pos_).manhattanLength();

        if (distance > QApplication::startDragDistance())
        {
            startDrag(Qt::CopyAction);
            return;
        }
    }

    QTreeWidget::mouseMoveEvent(event);
}

void FileTreeWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasFormat(FileItemMimeData::mimeType()))
        event->acceptProposedAction();
}

void FileTreeWidget::dragMoveEvent(QDragMoveEvent* event)
{
    event->ignore();

    if (event->source() != this)
        event->acceptProposedAction();

    QWidget::dragMoveEvent(event);
}

void FileTreeWidget::dropEvent(QDropEvent* event)
{
    const FileItemMimeData* mime_data =
        dynamic_cast<const FileItemMimeData*>(event->mimeData());

    if (!mime_data)
        return;

    emit fileListDroped(mime_data->fileList());
}

void FileTreeWidget::startDrag(Qt::DropActions supported_actions)
{
    QList<FileTransfer::Item> file_list;

    for (int i = 0; i < topLevelItemCount(); ++i)
    {
        FileItem* file_item = dynamic_cast<FileItem*>(topLevelItem(i));

        if (file_item && isItemSelected(file_item))
        {
            file_list.push_back(FileTransfer::Item(file_item->currentName(),
                                                   file_item->fileSize(),
                                                   file_item->isDirectory()));
        }
    }

    if (file_list.isEmpty())
        return;

    FileItemDrag* drag = new FileItemDrag(this);
    drag->setFileList(file_list);
    drag->exec(supported_actions);
}

void FileTreeWidget::onEditingFinished(const QModelIndex& index) const
{
    FileItem* file_item = dynamic_cast<FileItem*>(itemFromIndex(index));
    if (!file_item)
        return;

    emit fileNameChanged(file_item);
}

} // namespace aspia
