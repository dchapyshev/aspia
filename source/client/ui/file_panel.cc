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
#include <QDebug>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>

#include "client/ui/file_list_model.h"
#include "client/file_remover.h"
#include "client/file_status.h"
#include "host/file_platform_util.h"
#include "host/file_request.h"

namespace aspia {

namespace {

QString normalizePath(const QString& path)
{
    QString normalized_path = path;

    normalized_path.replace(QLatin1Char('\\'), QLatin1Char('/'));

    if (!normalized_path.endsWith(QLatin1Char('/')))
        normalized_path += QLatin1Char('/');

    return normalized_path;
}

QString parentPath(const QString& path)
{
    int from = -1;
    if (path.endsWith(QLatin1Char('/')))
        from = -2;

    int last_slash = path.lastIndexOf(QLatin1Char('/'), from);
    if (last_slash == -1)
        return QString();

    return path.left(last_slash);
}

} // namespace

FilePanel::FilePanel(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);

    file_list_ = new FileListModel(this);
    ui.tree_files->setModel(file_list_);
    ui.tree_files->setAcceptDrops(true);

    connect(ui.address_bar, QOverload<int>::of(&QComboBox::activated),
            this, &FilePanel::onAddressItemChanged);

    connect(ui.tree_files, &QTreeView::doubleClicked, this, &FilePanel::onFileDoubleClicked);
    connect(ui.tree_files, &QTreeView::customContextMenuRequested, this, &FilePanel::onFileContextMenu);

    connect(file_list_, &FileListModel::nameChangeRequest, this, &FilePanel::onNameChangeRequest);
    connect(file_list_, &FileListModel::createFolderRequest, this, &FilePanel::onCreateFolderRequest);

    QItemSelectionModel* selection_model = ui.tree_files->selectionModel();
    connect(selection_model, &QItemSelectionModel::selectionChanged,
            this, &FilePanel::onFileSelectionChanged);

    connect(ui.button_up, &QPushButton::pressed, this, &FilePanel::toParentFolder);
    connect(ui.button_refresh, &QPushButton::pressed, this, &FilePanel::refresh);
    connect(ui.button_add, &QPushButton::pressed, this, &FilePanel::addFolder);
    connect(ui.button_delete, &QPushButton::pressed, this, &FilePanel::removeSelected);
    connect(ui.button_send, &QPushButton::pressed, this, &FilePanel::sendSelected);

    connect(file_list_, &FileListModel::fileListDropped,
            [this](const QString& folder_name, const QList<FileTransfer::Item>& items)
    {
        QString target_folder = currentPath();
        if (!folder_name.isEmpty())
            target_folder += folder_name;

        emit receiveItems(this, normalizePath(target_folder), items);
    });
}

void FilePanel::setPanelName(const QString& name)
{
    ui.label_name->setText(name);
}

void FilePanel::setCurrentPath(const QString& path)
{
    current_path_ = normalizePath(path);

    for (int i = 0; i < ui.address_bar->count(); ++i)
    {
        if (addressItemPath(i) == current_path_)
        {
            ui.address_bar->setCurrentIndex(i);
            onAddressItemChanged(i);
            return;
        }
    }

    file_list_->clear();

    int current_item = ui.address_bar->count();

    ui.address_bar->addItem(FilePlatformUtil::directoryIcon(), current_path_);
    ui.address_bar->setCurrentIndex(current_item);

    onAddressItemChanged(current_item);
}

QByteArray FilePanel::saveState() const
{
    return ui.tree_files->header()->saveState();
}

void FilePanel::restoreState(const QByteArray& state)
{
    QHeaderView* header = ui.tree_files->header();
    header->restoreState(state);

    file_list_->setSortOrder(header->sortIndicatorSection(), header->sortIndicatorOrder());
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

        updateDrives(reply.drive_list());
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
        }

        file_list_->setFileList(reply.file_list());
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

        refresh();
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

        refresh();
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

void FilePanel::onAddressItemChanged(int index)
{
    current_path_ = normalizePath(addressItemPath(index));

    // If the address is entered by the user, then the icon is missing.
    if (ui.address_bar->itemIcon(index).isNull())
    {
        int equal_item = ui.address_bar->findData(current_path_);
        if (equal_item == -1)
        {
            // We set the directory icon and update the path after normalization.
            ui.address_bar->setItemIcon(index, FilePlatformUtil::directoryIcon());
            ui.address_bar->setItemText(index, current_path_);
        }
        else
        {
            ui.address_bar->setCurrentIndex(equal_item);
            ui.address_bar->removeItem(index);
        }
    }

    int count = ui.address_bar->count();
    for (int i = 0; i < count; ++i)
    {
        if (ui.address_bar->itemData(i).toString().isEmpty() &&
            ui.address_bar->itemText(i) != ui.address_bar->itemText(index))
        {
            ui.address_bar->removeItem(i);
            break;
        }
    }

    file_list_->clear();

    FileRequest* request = FileRequest::fileListRequest(current_path_);
    connect(request, &FileRequest::replyReady, this, &FilePanel::reply);
    emit newRequest(request);
}

void FilePanel::onFileDoubleClicked(const QModelIndex& index)
{
    if (file_list_->isFolder(index))
        toChildFolder(file_list_->nameAt(index));
}

void FilePanel::onFileSelectionChanged()
{
    QItemSelectionModel* selection_model = ui.tree_files->selectionModel();

    ui.label_status->setText(tr("%1 object(s) selected")
                             .arg(selection_model->selectedRows().count()));

    if (selection_model->hasSelection())
    {
        ui.button_delete->setEnabled(true);
        ui.button_send->setEnabled(true);
    }
    else
    {
        ui.button_delete->setEnabled(false);
        ui.button_send->setEnabled(false);
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
        FileRequest* request = FileRequest::createDirectoryRequest(currentPath() + name);
        connect(request, &FileRequest::replyReady, this, &FilePanel::reply);
        emit newRequest(request);
    }
}

void FilePanel::onFileContextMenu(const QPoint& point)
{
    QMenu menu;

    QScopedPointer<QAction> copy_action;
    QScopedPointer<QAction> delete_action;

    if (ui.tree_files->selectionModel()->hasSelection())
    {
        copy_action.reset(new QAction(
            QIcon(QStringLiteral(":/icon/arrow-045.png")), tr("&Send\tF11")));
        delete_action.reset(new QAction(
            QIcon(QStringLiteral(":/icon/cross-script.png")), tr("&Delete\tDelete")));

        menu.addAction(copy_action.data());
        menu.addAction(delete_action.data());
        menu.addSeparator();
    }

    QScopedPointer<QAction> add_folder_action(new QAction(
        QIcon(QStringLiteral(":/icon/folder-plus.png")), tr("&Create Folder")));

    menu.addAction(add_folder_action.data());

    QAction* selected_action = menu.exec(ui.tree_files->mapToGlobal(point));
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
    setCurrentPath(current_path_ + child_name);
    ui.button_up->setEnabled(true);
}

void FilePanel::toParentFolder()
{
    QString parent_path = parentPath(current_path_);
    if (!parent_path.isEmpty())
        setCurrentPath(parent_path);

    ui.button_up->setEnabled(!parentPath(parent_path).isEmpty());
}

void FilePanel::addFolder()
{
    ui.tree_files->selectionModel()->select(QModelIndex(), QItemSelectionModel::Clear);

    QModelIndex index = file_list_->createFolder();
    if (index.isValid())
    {
        ui.tree_files->scrollTo(index);
        ui.tree_files->edit(index);
    }
}

void FilePanel::removeSelected()
{
    QList<FileRemover::Item> items;

    for (const auto& index : ui.tree_files->selectionModel()->selectedRows())
        items.append(FileRemover::Item(file_list_->nameAt(index), file_list_->isFolder(index)));

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
    QList<FileTransfer::Item> items;

    for (const auto& index : ui.tree_files->selectionModel()->selectedRows())
    {
        items.append(FileTransfer::Item(file_list_->nameAt(index),
                                        file_list_->sizeAt(index),
                                        file_list_->isFolder(index)));
    }

    if (items.isEmpty())
        return;

    emit sendItems(this, items);
}

QString FilePanel::addressItemPath(int index) const
{
    QString path = ui.address_bar->itemData(index).toString();
    if (path.isEmpty())
        path = ui.address_bar->itemText(index);

    return path;
}

void FilePanel::updateDrives(const proto::file_transfer::DriveList& list)
{
    ui.address_bar->clear();

    for (int i = 0; i < list.item_size(); ++i)
    {
        const proto::file_transfer::DriveList::Item& item = list.item(i);

        QString path = normalizePath(QString::fromStdString(item.path()));
        QIcon icon = FilePlatformUtil::driveIcon(item.type());

        switch (item.type())
        {
            case proto::file_transfer::DriveList::Item::TYPE_HOME_FOLDER:
                ui.address_bar->insertItem(i, icon, tr("Home Folder"), path);
                break;

            case proto::file_transfer::DriveList::Item::TYPE_DESKTOP_FOLDER:
                ui.address_bar->insertItem(i, icon, tr("Desktop"), path);
                break;

            default:
                ui.address_bar->insertItem(i, icon, path, path);
                break;
        }
    }

    if (current_path_.isEmpty())
        setCurrentPath(addressItemPath(0));
    else
        setCurrentPath(current_path_);
}

} // namespace aspia
