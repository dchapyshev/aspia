//
// Aspia Project
// Copyright (C) 2018 Dmitry Chapyshev <dmitry@aspia.ru>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "client/ui/file_list.h"

#include <QHeaderView>
#include <QKeyEvent>
#include <QProxyStyle>

#include "client/ui/address_bar_model.h"
#include "client/ui/file_item_delegate.h"
#include "client/ui/file_list_model.h"

namespace aspia {

namespace {

class TreeViewProxyStyle : public QProxyStyle
{
public:
    explicit TreeViewProxyStyle(QStyle* style)
        : QProxyStyle(style)
    {
        // Nothing
    }

    void drawPrimitive(PrimitiveElement element,
                       const QStyleOption* option,
                       QPainter* painter,
                       const QWidget* widget) const override
    {
        if (element == QStyle::PE_IndicatorItemViewItemDrop && !option->rect.isNull())
        {
            QStyleOption option_dup(*option);

            if (widget)
            {
                option_dup.rect.setX(0);
                option_dup.rect.setWidth(widget->width() - 3);
            }

            QProxyStyle::drawPrimitive(element, &option_dup, painter, widget);
            return;
        }

        QProxyStyle::drawPrimitive(element, option, painter, widget);
    }
};

} // namespace

FileList::FileList(QWidget* parent)
    : QTreeView(parent)
{
    model_ = new FileListModel(this);

    setModel(model_);
    setStyle(new TreeViewProxyStyle(style()));
    setItemDelegate(new FileItemDelegate(this));

    connect(model_, &FileListModel::nameChangeRequest, this, &FileList::nameChangeRequest);
    connect(model_, &FileListModel::createFolderRequest, this, &FileList::createFolderRequest);
    connect(model_, &FileListModel::fileListDropped, this, &FileList::fileListDropped);
}

void FileList::showDriveList(AddressBarModel* model)
{
    saveColumnsState();
    model_->clear();

    setModel(model);
    setRootIndex(model->computerIndex());
    setSelectionMode(QTreeView::SingleSelection);
    setSortingEnabled(false);
    header()->restoreState(drive_list_state_);
}

void FileList::showFileList(const proto::file_transfer::FileList& file_list)
{
    saveColumnsState();
    model_->clear();

    setModel(model_);
    setSelectionMode(QTreeView::ExtendedSelection);
    setSortingEnabled(true);

    header()->restoreState(file_list_state_);

    model_->setSortOrder(header()->sortIndicatorSection(), header()->sortIndicatorOrder());
    model_->setFileList(file_list);
}

void FileList::setMimeType(const QString& mime_type)
{
    model_->setMimeType(mime_type);
}

bool FileList::isDriveListShown() const
{
    return model() != model_;
}

bool FileList::isFileListShown() const
{
    return model() == model_;
}

void FileList::createFolder()
{
    selectionModel()->select(QModelIndex(), QItemSelectionModel::Clear);

    QModelIndex index = model_->createFolder();
    if (index.isValid())
    {
        scrollTo(index);
        edit(index);
    }
}

void FileList::setDriveListState(const QByteArray& state)
{
    drive_list_state_ = state;

    if (isDriveListShown())
        header()->restoreState(drive_list_state_);
}

QByteArray FileList::driveListState() const
{
    if (isDriveListShown())
        return header()->saveState();

    return drive_list_state_;
}

void FileList::setFileListState(const QByteArray& state)
{
    file_list_state_ = state;

    if (isFileListShown())
        header()->restoreState(file_list_state_);
}

QByteArray FileList::fileListState() const
{
    if (isFileListShown())
        return header()->saveState();

    return file_list_state_;
}

void FileList::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_F2)
    {
        QModelIndexList list = selectionModel()->selectedRows();
        if (list.count() == 1)
        {
            edit(list.constFirst());
            event->ignore();
        }
    }

    QTreeView::keyPressEvent(event);
}

void FileList::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MouseButton::RightButton)
    {
        event->ignore();
        return;
    }

    QTreeView::mouseDoubleClickEvent(event);
}

void FileList::saveColumnsState()
{
    drive_list_state_ = driveListState();
    file_list_state_ = fileListState();
}

} // namespace aspia
