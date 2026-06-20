//
// Aspia Project
// Copyright (C) 2016-2026 Dmitry Chapyshev <dmitry@aspia.ru>
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

#include "client/android/file_panel_widget.h"

#include <QHBoxLayout>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <utility>

#include "base/gui_application.h"
#include "client/file_error_code.h"
#include "common/file_platform_util.h"
#include "common/android/bottom_sheet.h"
#include "common/android/button.h"
#include "common/android/dialog.h"
#include "common/android/icon_button.h"
#include "common/android/label.h"
#include "common/android/line_edit.h"
#include "common/android/menu.h"
#include "common/android/message_dialog.h"
#include "common/android/tree_widget.h"
#include "proto/file_transfer.h"

namespace {

// Item data roles. A drive row carries kPathRole (its absolute path); a file/folder row carries the
// name and, for files, the size; kIsDirRole tells folders from files.
constexpr int kNameRole = Qt::UserRole;
constexpr int kIsDirRole = Qt::UserRole + 1;
constexpr int kSizeRole = Qt::UserRole + 2;
constexpr int kPathRole = Qt::UserRole + 3;

//--------------------------------------------------------------------------------------------------
QString parentPath(const QString& path)
{
    int from = path.endsWith('/') ? -2 : -1;
    int last_slash = path.lastIndexOf('/', from);
    if (last_slash == -1)
        return QString();

    return path.left(last_slash + 1);
}

//--------------------------------------------------------------------------------------------------
// The last component of a path, used as the breadcrumb caption (e.g. "C:/Users/" -> "Users").
QString segmentName(const QString& path)
{
    QString trimmed = path;
    if (trimmed.endsWith('/'))
        trimmed.chop(1);

    const int last_slash = trimmed.lastIndexOf('/');
    const QString name = (last_slash >= 0) ? trimmed.mid(last_slash + 1) : trimmed;
    return name.isEmpty() ? path : name;
}

} // namespace

//--------------------------------------------------------------------------------------------------
FilePanelWidget::FilePanelWidget(FileTask::Target target, QWidget* parent)
    : QWidget(parent),
      target_(target),
      title_(new Label(QString(), Label::Role::TITLE, this)),
      path_button_(new Button(QString(), Button::Role::TEXT, this)),
      up_button_(new IconButton(":/img/arrow-up.svg", this)),
      list_(new TreeWidget(this))
{
    IconButton* new_folder_button = new IconButton(":/img/material/create_new_folder.svg", this);
    IconButton* refresh_button = new IconButton(":/img/material/refresh.svg", this);

    QHBoxLayout* path_row = new QHBoxLayout();
    path_row->setContentsMargins(0, 0, 0, 0);
    path_row->addWidget(up_button_);
    path_row->addWidget(path_button_, 1);
    path_row->addWidget(new_folder_button);
    path_row->addWidget(refresh_button);

    list_->setColumnCount(1);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(title_);
    layout->addLayout(path_row);
    layout->addWidget(list_, 1);

    connect(list_, &QTreeWidget::itemClicked, this, &FilePanelWidget::onItemClicked);
    connect(list_, &TreeWidget::sig_itemLongPressed, this, &FilePanelWidget::showItemActions);
    connect(up_button_, &IconButton::clicked, this, &FilePanelWidget::onUpClicked);
    connect(new_folder_button, &IconButton::clicked, this, &FilePanelWidget::onNewFolderClicked);
    connect(refresh_button, &IconButton::clicked, this, &FilePanelWidget::refresh);
    connect(path_button_, &Button::clicked, this, &FilePanelWidget::showPathMenu);

    path_button_->setText(tr("This computer"));
    up_button_->setEnabled(false);
}

//--------------------------------------------------------------------------------------------------
FilePanelWidget::~FilePanelWidget() = default;

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::setTitle(const QString& title)
{
    title_->setText(title);
}

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::setHeaderVisible(bool visible)
{
    title_->setVisible(visible);
}

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::onDriveList(
    proto::file_transfer::ErrorCode error_code, const proto::file_transfer::DriveList& drive_list)
{
    if (error_code != proto::file_transfer::ERROR_CODE_SUCCESS)
    {
        showError(tr("Failed to get list of drives: %1").arg(fileErrorToString(error_code)));
        return;
    }

    list_->clear();

    for (int i = 0; i < drive_list.item_size(); ++i)
    {
        const proto::file_transfer::DriveList::Item& drive = drive_list.item(i);
        const QString path = QString::fromStdString(drive.path());

        // Named storages and special folders are labelled by their type; plain drives show the path.
        QString name;
        switch (drive.type())
        {
            case proto::file_transfer::DriveList::Item::TYPE_HOME_FOLDER:
                name = tr("Home Folder");
                break;

            case proto::file_transfer::DriveList::Item::TYPE_DESKTOP_FOLDER:
                name = tr("Desktop");
                break;

            case proto::file_transfer::DriveList::Item::TYPE_INTERNAL_STORAGE:
                name = tr("Internal storage");
                break;

            case proto::file_transfer::DriveList::Item::TYPE_SD_CARD:
                name = tr("SD card");
                break;

            case proto::file_transfer::DriveList::Item::TYPE_DOWNLOAD_FOLDER:
                name = tr("Downloads");
                break;

            case proto::file_transfer::DriveList::Item::TYPE_CAMERA_FOLDER:
                name = tr("Camera");
                break;

            case proto::file_transfer::DriveList::Item::TYPE_PICTURES_FOLDER:
                name = tr("Pictures");
                break;

            case proto::file_transfer::DriveList::Item::TYPE_DOCUMENTS_FOLDER:
                name = tr("Documents");
                break;

            default:
                name = path;
                break;
        }

        QTreeWidgetItem* item = new QTreeWidgetItem(list_, { name });
        item->setIcon(0, FilePlatformUtil::driveIcon(drive.type()));
        item->setData(0, kPathRole, path);
    }
}

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::onFileList(
    proto::file_transfer::ErrorCode error_code, const proto::file_transfer::List& file_list)
{
    if (error_code != proto::file_transfer::ERROR_CODE_SUCCESS)
    {
        showError(tr("Failed to get list of files: %1").arg(fileErrorToString(error_code)));
        return;
    }

    list_->clear();

    const QIcon folder_icon = GuiApplication::svgIcon(":/img/folder.svg");
    const QIcon file_icon = GuiApplication::svgIcon(":/img/file.svg");

    // Folders first, then files, both kept in the order the host reported.
    for (int i = 0; i < file_list.item_size(); ++i)
    {
        const proto::file_transfer::List::Item& entry = file_list.item(i);
        if (!entry.is_directory())
            continue;

        QTreeWidgetItem* item = new QTreeWidgetItem(list_, { QString::fromStdString(entry.name()) });
        item->setIcon(0, folder_icon);
        item->setData(0, kNameRole, QString::fromStdString(entry.name()));
        item->setData(0, kIsDirRole, true);
    }

    for (int i = 0; i < file_list.item_size(); ++i)
    {
        const proto::file_transfer::List::Item& entry = file_list.item(i);
        if (entry.is_directory())
            continue;

        QTreeWidgetItem* item = new QTreeWidgetItem(list_, { QString::fromStdString(entry.name()) });
        item->setIcon(0, file_icon);
        item->setData(0, kNameRole, QString::fromStdString(entry.name()));
        item->setData(0, kIsDirRole, false);
        item->setData(0, kSizeRole, static_cast<qint64>(entry.size()));
    }
}

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::onCreateDirectory(proto::file_transfer::ErrorCode error_code)
{
    if (error_code != proto::file_transfer::ERROR_CODE_SUCCESS)
    {
        showError(tr("Failed to create directory: %1").arg(fileErrorToString(error_code)));
        return;
    }

    refresh();
}

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::refresh()
{
    if (current_path_.isEmpty())
        emit sig_driveListRequested();
    else
        emit sig_fileListRequested(current_path_);
}

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::onItemClicked(QTreeWidgetItem* item, int /* column */)
{
    if (!item)
        return;

    const QVariant drive_path = item->data(0, kPathRole);
    if (drive_path.isValid())
    {
        setPath(drive_path.toString());
        return;
    }

    if (item->data(0, kIsDirRole).toBool())
    {
        setPath(current_path_ + item->data(0, kNameRole).toString() + "/");
        return;
    }

    showItemActions(item);
}

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::onUpClicked()
{
    if (!current_path_.isEmpty())
        setPath(parentPath(current_path_));
}

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::onNewFolderClicked()
{
    if (current_path_.isEmpty())
        return;

    Dialog dialog(this);
    dialog.setTitle(tr("Create Folder"));

    LineEdit* name = new LineEdit(&dialog);
    name->setLabel(tr("Folder name"));
    dialog.contentLayout()->addWidget(name);

    Button* cancel = dialog.addButton(tr("Cancel"), Button::Role::TEXT);
    Button* accept = dialog.addButton(tr("Create"), Button::Role::FILLED);
    connect(cancel, &Button::clicked, &dialog, &QDialog::reject);
    connect(accept, &Button::clicked, &dialog, &QDialog::accept);

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString folder_name = name->text();
    if (folder_name.isEmpty())
        return;

    if (!FilePlatformUtil::isValidFileName(folder_name))
    {
        showError(tr("Name contains invalid characters."));
        return;
    }

    emit sig_createDirectoryRequested(current_path_ + folder_name);
}

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::setPath(const QString& path)
{
    current_path_ = path;
    path_button_->setText(path.isEmpty() ? tr("This computer") : path);
    up_button_->setEnabled(!path.isEmpty());

    refresh();
}

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::showPathMenu()
{
    // The breadcrumb: the root (drive list) followed by every parent down to the current directory.
    QList<QString> paths;
    for (QString path = current_path_; !path.isEmpty(); path = parentPath(path))
        paths.prepend(path);

    Menu* menu = new Menu(this);
    menu->addItem(tr("This computer"));
    for (const QString& path : std::as_const(paths))
        menu->addItem(segmentName(path));

    connect(menu, &Menu::sig_triggered, this, [this, paths](int index)
    {
        setPath(index == 0 ? QString() : paths.at(index - 1));
    });

    const QRect anchor(path_button_->mapToGlobal(QPoint(0, 0)), path_button_->size());
    menu->popup(anchor);
}

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::showItemActions(QTreeWidgetItem* item)
{
    if (!item || item->data(0, kPathRole).isValid() || current_path_.isEmpty())
        return;

    const QString name = item->data(0, kNameRole).toString();
    const bool is_directory = item->data(0, kIsDirRole).toBool();
    const qint64 size = item->data(0, kSizeRole).toLongLong();

    BottomSheet* sheet = new BottomSheet(this);
    sheet->setTitle(name);
    sheet->addItem(target_ == FileTask::Target::LOCAL ? tr("Upload") : tr("Download"),
                   ":/img/file-explorer.svg", false, false);
    sheet->addItem(tr("Delete"), ":/img/material/delete.svg");

    connect(sheet, &BottomSheet::sig_triggered, this, [this, name, is_directory, size](int index)
    {
        if (index == 0)
        {
            emit sig_sendRequested({ FileTransfer::Item(name, size, is_directory) });
        }
        else
        {
            if (!MessageDialog::confirm(this, tr("Delete"),
                    tr("Delete \"%1\"?").arg(name), tr("Delete")))
                return;

            FileRemover::TaskList items;
            items.emplace_back(current_path_ + name, is_directory);
            emit sig_removeRequested(items);
        }
    });

    sheet->showSheet();
}

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::showError(const QString& message)
{
    MessageDialog::info(this, tr("Error"), message);
}
