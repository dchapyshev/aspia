//
// PROJECT:         Aspia
// FILE:            client/ui/file_panel.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/file_panel.h"

#include <QAction>
#include <QDebug>
#include <QKeyEvent>
#include <QMenu>
#include <QMessageBox>

#include "client/ui/file_item.h"
#include "client/file_remover.h"
#include "client/file_status.h"
#include "host/file_platform_util.h"
#include "host/file_request.h"

namespace aspia {

namespace {

const char* kReplySlot = "reply";

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

    connect(ui.address_bar, QOverload<int>::of(&QComboBox::activated),
            this, &FilePanel::onAddressItemChanged);

    connect(ui.tree, &FileTreeWidget::itemDoubleClicked,
            this, &FilePanel::onFileDoubleClicked);
    connect(ui.tree, &FileTreeWidget::itemSelectionChanged,
            this, &FilePanel::onFileSelectionChanged);
    connect(ui.tree, &FileTreeWidget::fileNameChanged,
            this, &FilePanel::onFileNameChanged);
    connect(ui.tree, &FileTreeWidget::customContextMenuRequested,
            this, &FilePanel::onFileContextMenu);

    connect(ui.button_up, &QPushButton::pressed, this, &FilePanel::toParentFolder);
    connect(ui.button_refresh, &QPushButton::pressed, this, &FilePanel::refresh);
    connect(ui.button_add, &QPushButton::pressed, this, &FilePanel::addFolder);
    connect(ui.button_delete, &QPushButton::pressed, this, &FilePanel::removeSelected);
    connect(ui.button_send, &QPushButton::pressed, this, &FilePanel::sendSelected);

    connect(ui.tree, &FileTreeWidget::fileListDroped, [this](const QList<FileTransfer::Item>& items)
    {
        emit receiveItems(this, items);
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

    for (int i = ui.tree->topLevelItemCount() - 1; i >= 0; --i)
    {
        QTreeWidgetItem* item = ui.tree->takeTopLevelItem(i);
        delete item;
    }

    int current_item = ui.address_bar->count();

    ui.address_bar->addItem(FilePlatformUtil::directoryIcon(), current_path_);
    ui.address_bar->setCurrentIndex(current_item);

    onAddressItemChanged(current_item);
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
            return;
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
            return;
        }

        updateFiles(reply.file_list());
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
    emit request(FileRequest::driveListRequest(this, kReplySlot));
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

    emit request(FileRequest::fileListRequest(this, current_path_, kReplySlot));
}

void FilePanel::onFileDoubleClicked(QTreeWidgetItem* item, int column)
{
    FileItem* file_item = dynamic_cast<FileItem*>(item);
    if (!file_item || !file_item->isDirectory())
        return;

    toChildFolder(file_item->text(0));
}

void FilePanel::onFileSelectionChanged()
{
    int selected_count = selectedFilesCount();

    ui.label_status->setText(tr("%1 object(s) selected").arg(selected_count));

    if (selected_count > 0)
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

void FilePanel::onFileNameChanged(FileItem* file_item)
{
    QString initial_name = file_item->initialName();
    QString current_name = file_item->currentName();

    if (initial_name.isEmpty()) // New item.
    {
        if (current_name.isEmpty())
        {
            QMessageBox::warning(this,
                                 tr("Warning"),
                                 tr("Folder name can not be empty."),
                                 QMessageBox::Ok);
            delete file_item;
            return;
        }

        emit request(FileRequest::createDirectoryRequest(
            this, currentPath() + current_name, kReplySlot));
    }
    else // Rename item.
    {
        if (current_name == initial_name)
            return;

        emit request(FileRequest::renameRequest(
            this,
            currentPath() + initial_name,
            currentPath() + current_name,
            kReplySlot));
    }
}

void FilePanel::onFileContextMenu(const QPoint& point)
{
    QMenu menu;

    QScopedPointer<QAction> copy_action;
    QScopedPointer<QAction> delete_action;

    if (selectedFilesCount() > 0)
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

    QAction* selected_action = menu.exec(ui.tree->mapToGlobal(point));
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
    FileItem* item = new FileItem(QString());

    ui.tree->addTopLevelItem(item);
    ui.tree->editItem(item);
}

void FilePanel::removeSelected()
{
    QList<FileRemover::Item> items;

    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
    {
        FileItem* file_item = dynamic_cast<FileItem*>(ui.tree->topLevelItem(i));

        if (file_item && ui.tree->isItemSelected(file_item))
        {
            items.push_back(FileRemover::Item(file_item->currentName(),
                                              file_item->isDirectory()));
        }
    }

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

    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
    {
        FileItem* file_item = dynamic_cast<FileItem*>(ui.tree->topLevelItem(i));

        if (file_item && ui.tree->isItemSelected(file_item))
        {
            items.push_back(FileTransfer::Item(file_item->currentName(),
                                               file_item->fileSize(),
                                               file_item->isDirectory()));
        }
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

void FilePanel::updateFiles(const proto::file_transfer::FileList& list)
{
    for (int i = ui.tree->topLevelItemCount() - 1; i >= 0; --i)
    {
        QTreeWidgetItem* item = ui.tree->takeTopLevelItem(i);
        delete item;
    }

    for (int i = 0; i < list.item_size(); ++i)
        ui.tree->addTopLevelItem(new FileItem(list.item(i)));
}

int FilePanel::selectedFilesCount()
{
    int selected_count = 0;

    int total_count = ui.tree->topLevelItemCount();
    for (int i = 0; i < total_count; ++i)
    {
        if (ui.tree->isItemSelected(ui.tree->topLevelItem(i)))
            ++selected_count;
    }

    return selected_count;
}

} // namespace aspia
