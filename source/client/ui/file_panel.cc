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

#include "client/ui/file_panel.h"

#include <QAction>
#include <QLineEdit>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>

#include "client/ui/address_bar_model.h"
#include "client/ui/file_item_delegate.h"
#include "client/ui/file_list_model.h"
#include "client/file_remover.h"
#include "client/file_status.h"
#include "common/file_platform_util.h"
#include "common/file_request.h"

namespace aspia {

namespace {

QString parentPath(const QString& path)
{
    int from = -1;
    if (path.endsWith(QLatin1Char('/')))
        from = -2;

    int last_slash = path.lastIndexOf(QLatin1Char('/'), from);
    if (last_slash == -1)
        return AddressBarModel::computerPath();

    return path.left(last_slash);
}

} // namespace

FilePanel::FilePanel(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);

    FileItemDelegate* delegate = dynamic_cast<FileItemDelegate*>(ui.list->itemDelegate());
    connect(delegate, &FileItemDelegate::editFinished, this, &FilePanel::refresh);

    connect(ui.address_bar, &AddressBar::pathChanged, this, &FilePanel::onPathChanged);

    connect(ui.list, &FileList::activated, this, &FilePanel::onListItemActivated);
    connect(ui.list, &FileList::customContextMenuRequested, this, &FilePanel::onListContextMenu);
    connect(ui.list, &FileList::nameChangeRequest, this, &FilePanel::onNameChangeRequest);
    connect(ui.list, &FileList::createFolderRequest, this, &FilePanel::onCreateFolderRequest);

    connect(ui.action_up, &QAction::triggered, this, &FilePanel::toParentFolder);
    connect(ui.action_refresh, &QAction::triggered, this, &FilePanel::refresh);
    connect(ui.action_add_folder, &QAction::triggered, this, &FilePanel::addFolder);
    connect(ui.action_delete, &QAction::triggered, this, &FilePanel::removeSelected);
    connect(ui.action_send, &QAction::triggered, this, &FilePanel::sendSelected);

    connect(ui.list, &FileList::fileListDropped,
            [this](const QString& folder_name, const QList<FileTransfer::Item>& items)
    {
        QString target_folder = currentPath();
        if (!folder_name.isEmpty())
            target_folder += folder_name;

        emit receiveItems(this, target_folder, items);
    });

    ui.list->setFocus();
}

void FilePanel::onPathChanged(const QString& path)
{
    emit pathChanged(this, ui.address_bar->currentPath());

    AddressBarModel* model = dynamic_cast<AddressBarModel*>(ui.address_bar->model());
    if (!model)
        return;

    ui.action_up->setEnabled(false);
    ui.action_add_folder->setEnabled(false);
    ui.action_delete->setEnabled(false);

    setTransferEnabled(false);

    if (path == model->computerPath())
    {
        ui.list->showDriveList(model);
        ui.label_status->clear();
    }
    else
    {
        FileRequest* request = FileRequest::fileListRequest(path);
        connect(request, &FileRequest::replyReady, this, &FilePanel::reply);
        emit newRequest(request);
    }
}

void FilePanel::setPanelName(const QString& name)
{
    ui.label_name->setText(name);
}

void FilePanel::setMimeType(const QString& mime_type)
{
    ui.list->setMimeType(mime_type);
}

void FilePanel::setTransferAllowed(bool allowed)
{
    transfer_allowed_ = allowed;
    ui.action_send->setEnabled(transfer_allowed_ && transfer_enabled_);
}

void FilePanel::setTransferEnabled(bool enabled)
{
    transfer_enabled_ = enabled;
    ui.action_send->setEnabled(transfer_allowed_ && transfer_enabled_);
}

void FilePanel::reply(const proto::file_transfer::Request& request,
                      const proto::file_transfer::Reply& reply)
{
    if (request.has_drive_list_request())
    {
        if (reply.status() != proto::file_transfer::STATUS_SUCCESS)
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("Failed to get list of drives: %1")
                                     .arg(fileStatusToString(reply.status())),
                                 QMessageBox::Ok);
        }

        ui.address_bar->setDriveList(reply.drive_list());
    }
    else if (request.has_file_list_request())
    {
        if (reply.status() != proto::file_transfer::STATUS_SUCCESS)
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("Failed to get list of files: %1")
                                     .arg(fileStatusToString(reply.status())),
                                 QMessageBox::Ok);
            ui.address_bar->setCurrentPath(ui.address_bar->previousPath());
        }
        else
        {
            ui.action_up->setEnabled(true);
            ui.action_add_folder->setEnabled(true);

            ui.list->showFileList(reply.file_list());

            QItemSelectionModel* selection_model = ui.list->selectionModel();

            connect(selection_model, &QItemSelectionModel::selectionChanged,
                    this, &FilePanel::onListSelectionChanged);
        }
    }
    else if (request.has_create_directory_request())
    {
        if (reply.status() != proto::file_transfer::STATUS_SUCCESS)
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("Failed to create directory: %1")
                                     .arg(fileStatusToString(reply.status())),
                                 QMessageBox::Ok);
        }
    }
    else if (request.has_rename_request())
    {
        if (reply.status() != proto::file_transfer::STATUS_SUCCESS)
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("Failed to rename item: %1")
                                     .arg(fileStatusToString(reply.status())),
                                 QMessageBox::Ok);
        }
    }
}

void FilePanel::refresh()
{
    FileRequest* request = FileRequest::driveListRequest();
    connect(request, &FileRequest::replyReady, this, &FilePanel::reply);
    emit newRequest(request);
}

void FilePanel::keyPressEvent(QKeyEvent* event)
{
    switch (event->key())
    {
        case Qt::Key_Backspace:
            toParentFolder();
            break;

        case Qt::Key_F4:
        {
            QLineEdit* line_edit = ui.address_bar->lineEdit();
            line_edit->selectAll();
            line_edit->setFocus();

            ui.address_bar->showPopup();
        }
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

void FilePanel::onListItemActivated(const QModelIndex& index)
{
    if (ui.list->isFileListShown())
    {
        FileListModel* model = dynamic_cast<FileListModel*>(ui.list->model());
        if (model && model->isFolder(index))
            toChildFolder(model->nameAt(index));
    }
    else
    {
        ui.address_bar->setCurrentPath(ui.address_bar->pathAt(index));
    }
}

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

void FilePanel::onNameChangeRequest(const QString& old_name, const QString& new_name)
{
    if (new_name.isEmpty())
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Folder name can not be empty."),
                             QMessageBox::Ok);
    }
    else if (old_name.compare(new_name, Qt::CaseInsensitive) != 0)
    {
        if (!FilePlatformUtil::isValidFileName(new_name))
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("Name contains invalid characters."),
                                 QMessageBox::Ok);
            return;
        }

        FileRequest* request = FileRequest::renameRequest(currentPath() + old_name,
                                                          currentPath() + new_name);
        connect(request, &FileRequest::replyReady, this, &FilePanel::reply);
        emit newRequest(request);
    }
}

void FilePanel::onCreateFolderRequest(const QString& name)
{
    if (name.isEmpty())
    {
        QMessageBox::warning(this,
                             tr("Warning"),
                             tr("Folder name can not be empty."),
                             QMessageBox::Ok);
    }
    else
    {
        if (!FilePlatformUtil::isValidFileName(name))
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("Name contains invalid characters."),
                                 QMessageBox::Ok);
            return;
        }

        FileRequest* request = FileRequest::createDirectoryRequest(currentPath() + name);
        connect(request, &FileRequest::replyReady, this, &FilePanel::reply);
        emit newRequest(request);
    }
}

void FilePanel::onListContextMenu(const QPoint& point)
{
    if (!ui.address_bar->hasCurrentPath())
        return;

    QMenu menu;

    QScopedPointer<QAction> copy_action;
    QScopedPointer<QAction> delete_action;

    if (ui.list->selectionModel()->hasSelection())
    {
        copy_action.reset(new QAction(
            QIcon(QStringLiteral(":/icon/arrow-045.png")), tr("&Send\tF11")));
        delete_action.reset(new QAction(
            QIcon(QStringLiteral(":/icon/cross-script.png")), tr("&Delete\tDelete")));

        copy_action->setEnabled(transfer_allowed_ && transfer_enabled_);

        menu.addAction(copy_action.data());
        menu.addAction(delete_action.data());
        menu.addSeparator();
    }

    QScopedPointer<QAction> add_folder_action(new QAction(
        QIcon(QStringLiteral(":/icon/folder-plus.png")), tr("&Create Folder")));

    menu.addAction(add_folder_action.data());

    QAction* selected_action = menu.exec(ui.list->viewport()->mapToGlobal(point));
    if (!selected_action)
        return;

    if (selected_action == delete_action.data())
        removeSelected();
    else if (selected_action == copy_action.data())
        sendSelected();
    else if (selected_action == add_folder_action.data())
        addFolder();
}

void FilePanel::toChildFolder(const QString& child_name)
{
    ui.address_bar->setCurrentPath(ui.address_bar->currentPath() + child_name);
    ui.action_up->setEnabled(true);
}

void FilePanel::toParentFolder()
{
    if (ui.action_up->isEnabled())
    {
        QString parent_path = parentPath(ui.address_bar->currentPath());
        ui.address_bar->setCurrentPath(parent_path);
    }
}

void FilePanel::addFolder()
{
    if (ui.action_add_folder->isEnabled())
        ui.list->createFolder();
}

void FilePanel::removeSelected()
{
    if (!ui.action_delete->isEnabled())
        return;

    FileListModel* model = dynamic_cast<FileListModel*>(ui.list->model());
    if (!model)
        return;

    QList<FileRemover::Item> items;

    for (const auto& index : ui.list->selectionModel()->selectedRows())
        items.append(FileRemover::Item(model->nameAt(index), model->isFolder(index)));

    if (items.isEmpty())
        return;

    if (QMessageBox::question(this,
                              tr("Confirmation"),
                              tr("Are you sure you want to delete the selected items?"),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    emit removeItems(this, items);
}

void FilePanel::sendSelected()
{
    if (!ui.action_send->isEnabled())
        return;

    FileListModel* model = dynamic_cast<FileListModel*>(ui.list->model());
    if (!model)
        return;

    QList<FileTransfer::Item> items;

    for (const auto& index : ui.list->selectionModel()->selectedRows())
    {
        items.append(FileTransfer::Item(model->nameAt(index),
                                        model->sizeAt(index),
                                        model->isFolder(index)));
    }

    if (items.isEmpty())
        return;

    emit sendItems(this, items);
}

} // namespace aspia
