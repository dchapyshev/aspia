//
// SmartCafe Project
// Copyright (C) 2016-2025 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/ui/file_transfer/file_list.h"

#include "client/ui/file_transfer/address_bar_model.h"
#include "client/ui/file_transfer/file_item_delegate.h"
#include "client/ui/file_transfer/file_list_model.h"

#include <QHeaderView>
#include <QKeyEvent>
#include <QProxyStyle>

namespace client {

namespace {

class TreeViewProxyStyle final : public QProxyStyle
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
                       const QWidget* widget) const final
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

//--------------------------------------------------------------------------------------------------
FileList::FileList(QWidget* parent)
    : QTreeView(parent)
{
    model_ = new FileListModel(this);

    setModel(model_);
    setStyle(new TreeViewProxyStyle(style()));
    setItemDelegate(new FileItemDelegate(this));

    connect(model_, &FileListModel::sig_nameChangeRequest, this, &FileList::sig_nameChangeRequest);
    connect(model_, &FileListModel::sig_createFolderRequest, this, &FileList::sig_createFolderRequest);
    connect(model_, &FileListModel::sig_fileListDropped, this, &FileList::sig_fileListDropped);
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
void FileList::showFileList(const proto::file_transfer::List& file_list)
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

//--------------------------------------------------------------------------------------------------
void FileList::setMimeType(const QString& mime_type)
{
    model_->setMimeType(mime_type);
}

//--------------------------------------------------------------------------------------------------
bool FileList::isDriveListShown() const
{
    return model() != model_;
}

//--------------------------------------------------------------------------------------------------
bool FileList::isFileListShown() const
{
    return model() == model_;
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
void FileList::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_5_15);

    stream >> drive_list_state_;
    stream >> file_list_state_;

    if (isDriveListShown())
        header()->restoreState(drive_list_state_);
    else
        header()->restoreState(file_list_state_);
}

//--------------------------------------------------------------------------------------------------
QByteArray FileList::saveState() const
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_5_15);

        if (isDriveListShown())
        {
            stream << header()->saveState();
            stream << file_list_state_;
        }
        else
        {
            stream << drive_list_state_;
            stream << header()->saveState();
        }
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
void FileList::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MouseButton::RightButton)
    {
        event->ignore();
        return;
    }

    QTreeView::mouseDoubleClickEvent(event);
}

//--------------------------------------------------------------------------------------------------
void FileList::saveColumnsState()
{
    QByteArray state = header()->saveState();

    if (isDriveListShown())
        drive_list_state_ = std::move(state);
    else
        file_list_state_ = std::move(state);
}

} // namespace client
