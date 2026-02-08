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

#include "client/ui/file_transfer/file_panel.h"

#include <QAbstractButton>
#include <QAction>
#include <QLineEdit>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>

#include "base/logging.h"
#include "client/file_remover.h"
#include "client/ui/file_transfer/address_bar_model.h"
#include "client/ui/file_transfer/file_error_code.h"
#include "client/ui/file_transfer/file_item_delegate.h"
#include "client/ui/file_transfer/file_list_model.h"
#include "common/file_platform_util.h"

namespace client {

namespace {

//--------------------------------------------------------------------------------------------------
QString parentPath(const QString& path)
{
    int from = -1;
    if (path.endsWith(QLatin1Char('/')))
        from = -2;

    int last_slash = path.lastIndexOf(QLatin1Char('/'), from);
    if (last_slash == -1)
        return AddressBarModel::computerPath();

    return path.left(last_slash + 1);
}

} // namespace

//--------------------------------------------------------------------------------------------------
FilePanel::FilePanel(QWidget* parent)
    : QWidget(parent)
{
    LOG(INFO) << "Ctor";
    ui.setupUi(this);

    FileItemDelegate* delegate = static_cast<FileItemDelegate*>(ui.list->itemDelegate());
    connect(delegate, &FileItemDelegate::sig_editFinished, this, &FilePanel::refresh);

    connect(ui.address_bar, &AddressBar::sig_pathChanged, this, &FilePanel::onPathChanged);

    connect(ui.list, &FileList::activated, this, &FilePanel::onListItemActivated);
    connect(ui.list, &FileList::customContextMenuRequested, this, &FilePanel::onListContextMenu);
    connect(ui.list, &FileList::sig_nameChangeRequest, this, &FilePanel::onNameChangeRequest);
    connect(ui.list, &FileList::sig_createFolderRequest, this, &FilePanel::onCreateFolderRequest);

    connect(ui.action_up, &QAction::triggered, this, &FilePanel::toParentFolder);
    connect(ui.action_refresh, &QAction::triggered, this, &FilePanel::refresh);
    connect(ui.action_add_folder, &QAction::triggered, this, &FilePanel::addFolder);
    connect(ui.action_delete, &QAction::triggered, this, &FilePanel::removeSelected);
    connect(ui.action_send, &QAction::triggered, this, &FilePanel::sendSelected);

    connect(ui.list, &FileList::sig_fileListDropped,
            this, [this](const QString& folder_name, const QList<FileTransfer::Item>& items)
    {
        QString target_folder = currentPath();
        if (!folder_name.isEmpty())
            target_folder += folder_name;

        emit sig_receiveItems(this, target_folder, items);
    });

    ui.list->setFocus();
}

//--------------------------------------------------------------------------------------------------
FilePanel::~FilePanel()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void FilePanel::onDriveList(
    proto::file_transfer::ErrorCode error_code, const proto::file_transfer::DriveList& drive_list)
{
    if (error_code != proto::file_transfer::ERROR_CODE_SUCCESS)
    {
        showError(tr("Failed to get list of drives: %1").arg(fileErrorToString(error_code)));
    }
    else
    {
        ui.address_bar->setDriveList(drive_list);
    }

    // Request completed. Turn on the panel.
    setEnabled(true);
}

//--------------------------------------------------------------------------------------------------
void FilePanel::onFileList(
    proto::file_transfer::ErrorCode error_code, const proto::file_transfer::List& file_list)
{
    if (error_code != proto::file_transfer::ERROR_CODE_SUCCESS)
    {
        showError(tr("Failed to get list of files: %1").arg(fileErrorToString(error_code)));
        ui.address_bar->setCurrentPath(ui.address_bar->previousPath());
    }
    else
    {
        ui.action_up->setEnabled(true);
        ui.action_add_folder->setEnabled(true);

        ui.list->showFileList(file_list);

        QItemSelectionModel* selection_model = ui.list->selectionModel();

        connect(selection_model, &QItemSelectionModel::selectionChanged,
                this, &FilePanel::onListSelectionChanged);
    }

    // Request completed. Turn on the panel.
    setEnabled(true);
}

//--------------------------------------------------------------------------------------------------
void FilePanel::onCreateDirectory(proto::file_transfer::ErrorCode error_code)
{
    if (error_code != proto::file_transfer::ERROR_CODE_SUCCESS)
    {
        showError(tr("Failed to create directory: %1").arg(fileErrorToString(error_code)));
    }

    // Request completed. Turn on the panel.
    setEnabled(true);
}

//--------------------------------------------------------------------------------------------------
void FilePanel::onRename(proto::file_transfer::ErrorCode error_code)
{
    if (error_code != proto::file_transfer::ERROR_CODE_SUCCESS)
    {
        showError(tr("Failed to rename item: %1").arg(fileErrorToString(error_code)));
    }

    // Request completed. Turn on the panel.
    setEnabled(true);
}

//--------------------------------------------------------------------------------------------------
void FilePanel::onPathChanged(const QString& path)
{
    emit sig_pathChanged(this, ui.address_bar->currentPath());

    ui.action_up->setEnabled(false);
    ui.action_add_folder->setEnabled(false);
    ui.action_delete->setEnabled(false);

    setTransferEnabled(false);

    AddressBarModel* model = static_cast<AddressBarModel*>(ui.address_bar->model());

    if (path == model->computerPath())
    {
        ui.list->showDriveList(model);
        ui.label_status->clear();
    }
    else
    {
        emit sig_fileList(path);
    }
}

//--------------------------------------------------------------------------------------------------
void FilePanel::setPanelName(const QString& name)
{
    ui.label_name->setText(name);
}

//--------------------------------------------------------------------------------------------------
void FilePanel::setMimeType(const QString& mime_type)
{
    ui.list->setMimeType(mime_type);
}

//--------------------------------------------------------------------------------------------------
void FilePanel::setTransferAllowed(bool allowed)
{
    transfer_allowed_ = allowed;
    ui.action_send->setEnabled(transfer_allowed_ && transfer_enabled_);
}

//--------------------------------------------------------------------------------------------------
void FilePanel::setTransferEnabled(bool enabled)
{
    transfer_enabled_ = enabled;
    ui.action_send->setEnabled(transfer_allowed_ && transfer_enabled_);
}

//--------------------------------------------------------------------------------------------------
QByteArray FilePanel::saveState() const
{
    return ui.list->saveState();
}

//--------------------------------------------------------------------------------------------------
void FilePanel::restoreState(const QByteArray& state)
{
    ui.list->restoreState(state);
}

//--------------------------------------------------------------------------------------------------
void FilePanel::refresh()
{
    emit sig_driveList();
}

//--------------------------------------------------------------------------------------------------
void FilePanel::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
        case Qt::Key_Backspace:
            toParentFolder();
            break;

        case Qt::Key_F4:
            ui.address_bar->showPopup();
            break;

        case Qt::Key_F5:
            refresh();
            break;

        case Qt::Key_Delete:
            removeSelected();
            break;

        case Qt::Key_F11:
            sendSelected();
            break;

        default:
            break;
    }

    QWidget::keyPressEvent(event);
}

//--------------------------------------------------------------------------------------------------
void FilePanel::onListItemActivated(const QModelIndex& index)
{
    if (ui.list->isFileListShown())
    {
        FileListModel* model = static_cast<FileListModel*>(ui.list->model());
        if (model->isFolder(index))
            toChildFolder(model->nameAt(index));
    }
    else
    {
        ui.address_bar->setCurrentPath(ui.address_bar->pathAt(index));
    }
}

//--------------------------------------------------------------------------------------------------
void FilePanel::onListSelectionChanged()
{
    QItemSelectionModel* selection_model = ui.list->selectionModel();

    ui.label_status->setText(tr("%1 object(s) selected")
                             .arg(selection_model->selectedRows().count()));

    if (selection_model->hasSelection())
    {
        ui.action_delete->setEnabled(true);
        setTransferEnabled(true);
    }
    else
    {
        ui.action_delete->setEnabled(false);
        setTransferEnabled(false);
    }
}

//--------------------------------------------------------------------------------------------------
void FilePanel::onNameChangeRequest(const QString& old_name, const QString& new_name)
{
    if (new_name.isEmpty())
    {
        showError(tr("Folder name can not be empty."));
    }
    else if (old_name.compare(new_name, Qt::CaseInsensitive) != 0)
    {
        if (!common::FilePlatformUtil::isValidFileName(new_name))
        {
            showError(tr("Name contains invalid characters."));
            return;
        }

        emit sig_rename(currentPath() + old_name, currentPath() + new_name);
    }
}

//--------------------------------------------------------------------------------------------------
void FilePanel::onCreateFolderRequest(const QString& name)
{
    if (name.isEmpty())
    {
        showError(tr("Folder name can not be empty."));
    }
    else
    {
        if (!common::FilePlatformUtil::isValidFileName(name))
        {
            showError(tr("Name contains invalid characters."));
            return;
        }

        emit sig_createDirectory(currentPath() + name);
    }
}

//--------------------------------------------------------------------------------------------------
void FilePanel::onListContextMenu(const QPoint& point)
{
    if (!ui.address_bar->hasCurrentPath())
        return;

    QMenu menu;

    std::unique_ptr<QAction> copy_action;
    std::unique_ptr<QAction> delete_action;

    if (ui.list->selectionModel()->hasSelection())
    {
        copy_action.reset(new QAction(QIcon(":/img/send.svg"), tr("&Send\tF11")));
        delete_action.reset(new QAction(QIcon(":/img/close.svg"), tr("&Delete\tDelete")));

        copy_action->setEnabled(transfer_allowed_ && transfer_enabled_);

        menu.addAction(copy_action.get());
        menu.addAction(delete_action.get());
        menu.addSeparator();
    }

    std::unique_ptr<QAction> add_folder_action(new QAction(
        QIcon(":/img/add-folder.svg"), tr("&Create Folder")));

    menu.addAction(add_folder_action.get());

    QAction* selected_action = menu.exec(ui.list->viewport()->mapToGlobal(point));
    if (!selected_action)
        return;

    if (selected_action == delete_action.get())
        removeSelected();
    else if (selected_action == copy_action.get())
        sendSelected();
    else if (selected_action == add_folder_action.get())
        addFolder();
}

//--------------------------------------------------------------------------------------------------
void FilePanel::toChildFolder(const QString& child_name)
{
    LOG(INFO) << "toChildFolder called:" << child_name;

    ui.address_bar->setCurrentPath(ui.address_bar->currentPath() + child_name);
    ui.action_up->setEnabled(true);
}

//--------------------------------------------------------------------------------------------------
void FilePanel::toParentFolder()
{
    LOG(INFO) << "toParentFolder called";

    if (ui.action_up->isEnabled())
    {
        QString parent_path = parentPath(ui.address_bar->currentPath());
        ui.address_bar->setCurrentPath(parent_path);
    }
}

//--------------------------------------------------------------------------------------------------
void FilePanel::addFolder()
{
    if (ui.action_add_folder->isEnabled())
        ui.list->createFolder();
}

//--------------------------------------------------------------------------------------------------
void FilePanel::removeSelected()
{
    if (!ui.action_delete->isEnabled())
        return;

    FileListModel* model = static_cast<FileListModel*>(ui.list->model());
    QString current_path = currentPath();

    QModelIndexList selected_rows = ui.list->selectionModel()->selectedRows();
    FileRemover::TaskList items;

    for (const auto& index : std::as_const(selected_rows))
        items.emplace_back(current_path + model->nameAt(index), model->isFolder(index));

    if (items.isEmpty())
        return;

    QMessageBox message_box(QMessageBox::Question,
                            tr("Confirmation"),
                            tr("Are you sure you want to delete the selected items?"),
                            QMessageBox::Yes | QMessageBox::No,
                            this);
    message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
    message_box.button(QMessageBox::No)->setText(tr("No"));

    if (message_box.exec() != QMessageBox::Yes)
        return;

    emit sig_removeItems(this, items);
}

//--------------------------------------------------------------------------------------------------
void FilePanel::sendSelected()
{
    if (!ui.action_send->isEnabled())
        return;

    FileListModel* model = static_cast<FileListModel*>(ui.list->model());

    QModelIndexList selected_rows = ui.list->selectionModel()->selectedRows();
    QList<FileTransfer::Item> items;

    for (const auto& index : std::as_const(selected_rows))
        items.emplace_back(model->nameAt(index), model->sizeAt(index), model->isFolder(index));

    if (items.isEmpty())
        return;

    emit sig_sendItems(this, items);
}

//--------------------------------------------------------------------------------------------------
void FilePanel::showError(const QString& message)
{
    QMessageBox::warning(this, tr("Warning"), message, QMessageBox::Ok);
}

} // namespace client
