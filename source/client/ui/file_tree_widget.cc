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
#include "client/ui/file_item_drag.h"
#include "client/ui/file_item_mime_data.h"

namespace aspia {

FileTreeWidget::FileTreeWidget(QWidget* parent)
    : QTreeWidget(parent)
{
    setAcceptDrops(true);
}

FileTreeWidget::~FileTreeWidget() = default;

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
        reinterpret_cast<const FileItemMimeData*>(event->mimeData());

    if (!mime_data)
        return;

    emit fileListDroped(mime_data->fileList());
}

void FileTreeWidget::startDrag(Qt::DropActions supported_actions)
{
    QList<FileTransfer::Item> file_list;

    for (int i = 0; i < topLevelItemCount(); ++i)
    {
        FileItem* file_item = reinterpret_cast<FileItem*>(topLevelItem(i));

        if (isItemSelected(file_item))
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
    drag->exec(Qt::CopyAction);
}

} // namespace aspia
