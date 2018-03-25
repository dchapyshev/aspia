//
// PROJECT:         Aspia
// FILE:            client/ui/file_list_widget.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/file_list_widget.h"

#include "client/ui/file_item.h"

namespace aspia {

FileListWidget::FileListWidget(QWidget* parent)
    : QTreeWidget(parent)
{
    connect(this, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
            this, SLOT(fileDoubleClicked(QTreeWidgetItem*, int)));
}

FileListWidget::~FileListWidget() = default;

void FileListWidget::updateFiles(const proto::file_transfer::FileList& file_list)
{
    for (int i = topLevelItemCount() - 1; i >= 0; --i)
    {
        QTreeWidgetItem* item = takeTopLevelItem(i);
        delete item;
    }

    for (int i = 0; i < file_list.item_size(); ++i)
    {
        addTopLevelItem(new FileItem(file_list.item(i)));
    }
}

void FileListWidget::fileDoubleClicked(QTreeWidgetItem* item, int column)
{
    FileItem* file_item = reinterpret_cast<FileItem*>(item);
    if (!file_item->isDirectory())
        return;

    emit addressChanged(file_item->text(0));
}

} // namespace aspia
