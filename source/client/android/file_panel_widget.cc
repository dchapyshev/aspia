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

#include "base/gui_application.h"
#include "client/file_error_code.h"
#include "common/file_platform_util.h"
#include "common/android/bottom_sheet.h"
#include "common/android/button.h"
#include "common/android/combo_box.h"
#include "common/android/dialog.h"
#include "common/android/icon_button.h"
#include "common/android/line_edit.h"
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
// The SVG resource for a storage location icon, by its type.
const char* driveIconPath(proto::file_transfer::DriveList::Item::Type type)
{
    switch (type)
    {
        case proto::file_transfer::DriveList::Item::TYPE_HOME_FOLDER:
            return ":/img/home.svg";

        case proto::file_transfer::DriveList::Item::TYPE_DESKTOP_FOLDER:
            return ":/img/desktop.svg";

        case proto::file_transfer::DriveList::Item::TYPE_DOWNLOAD_FOLDER:
            return ":/img/folder-downloads.svg";

        case proto::file_transfer::DriveList::Item::TYPE_PICTURES_FOLDER:
            return ":/img/folder-pictures.svg";

        case proto::file_transfer::DriveList::Item::TYPE_DOCUMENTS_FOLDER:
            return ":/img/folder-documents.svg";

        case proto::file_transfer::DriveList::Item::TYPE_CAMERA_FOLDER:
            return ":/img/folder-camera.svg";

        case proto::file_transfer::DriveList::Item::TYPE_SD_CARD:
            return ":/img/sd.svg";

        default:
            return ":/img/hdd.svg";
    }
}

} // namespace

//--------------------------------------------------------------------------------------------------
FilePanelWidget::FilePanelWidget(FileTask::Target target, QWidget* parent)
    : QWidget(parent),
      target_(target),
      path_combo_(new ComboBox(this)),
      up_button_(new IconButton(":/img/material/arrow_upward.svg", this)),
      list_(new TreeWidget(this))
{
    IconButton* new_folder_button = new IconButton(":/img/material/create_new_folder.svg", this);
    IconButton* refresh_button = new IconButton(":/img/material/refresh.svg", this);

    const bool is_local = (target_ == FileTask::Target::LOCAL);

    // The send arrow points towards the other panel: outward to the right for the local side and to
    // the left for the remote one.
    send_button_ = new IconButton(
        is_local ? ":/img/material/send.svg" : ":/img/material/send_left.svg", this);
    delete_button_ = new IconButton(":/img/material/delete.svg", this);

    // The send button takes the edge facing the other panel (trailing for the local side, leading
    // for the remote one); the other buttons are grouped at the opposite edge, in the same order as
    // the desktop toolbar: up, refresh, new folder, delete.
    QHBoxLayout* toolbar = new QHBoxLayout();
    toolbar->setContentsMargins(0, 0, 0, 0);
    if (!is_local)
    {
        toolbar->addWidget(send_button_);
        toolbar->addStretch();
    }
    toolbar->addWidget(up_button_);
    toolbar->addWidget(refresh_button);
    toolbar->addWidget(new_folder_button);
    toolbar->addWidget(delete_button_);
    if (is_local)
    {
        toolbar->addStretch();
        toolbar->addWidget(send_button_);
    }

    list_->setColumnCount(1);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(path_combo_);
    layout->addLayout(toolbar);
    layout->addWidget(list_, 1);

    connect(list_, &QTreeWidget::itemClicked, this, &FilePanelWidget::onItemClicked);
    connect(list_, &TreeWidget::sig_itemLongPressed, this, &FilePanelWidget::showItemActions);
    connect(up_button_, &IconButton::clicked, this, &FilePanelWidget::onUpClicked);
    connect(new_folder_button, &IconButton::clicked, this, &FilePanelWidget::onNewFolderClicked);
    connect(refresh_button, &IconButton::clicked, this, &FilePanelWidget::refresh);
    connect(send_button_, &IconButton::clicked, this, &FilePanelWidget::onSendClicked);
    connect(delete_button_, &IconButton::clicked, this, &FilePanelWidget::onDeleteClicked);
    connect(path_combo_, &QComboBox::activated, this, &FilePanelWidget::onLocationActivated);

    up_button_->setEnabled(false);
    send_button_->setEnabled(false);
    delete_button_->setEnabled(false);
}

//--------------------------------------------------------------------------------------------------
FilePanelWidget::~FilePanelWidget() = default;

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::setTitle(const QString& title)
{
    root_name_ = title;
    if (path_combo_->count() > 0)
        path_combo_->setItemText(0, root_name_);
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

    path_combo_->clear();
    path_combo_->addItem(GuiApplication::svgIcon(":/img/computer.svg"), root_name_, QString());

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
                name = tr("Internal Storage");
                break;

            case proto::file_transfer::DriveList::Item::TYPE_SD_CARD:
                name = tr("SD Card");
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

        const QIcon icon = GuiApplication::svgIcon(driveIconPath(drive.type()));

        QTreeWidgetItem* item = new QTreeWidgetItem(list_, { name });
        item->setIcon(0, icon);
        item->setData(0, kPathRole, path);

        path_combo_->addItem(icon, name, path);
        path_combo_->setItemIndented(path_combo_->count() - 1);
    }

    // clear()/addItem reset the selection, so reselect the current location.
    selectCurrentLocation();
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

    // A folder opens on tap; a file is just selected (by the view) and sent with the send button.
    // Per-item actions (upload, delete) stay on long-press.
    if (item->data(0, kIsDirRole).toBool())
    {
        // Location paths do not always end with a separator (e.g. "/storage/emulated/0").
        QString base = current_path_;
        if (!base.endsWith('/'))
            base += '/';

        setPath(base + item->data(0, kNameRole).toString() + "/");
    }
}

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::onLocationActivated(int index)
{
    setPath(path_combo_->itemData(index).toString());
}

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::onUpClicked()
{
    if (current_path_.isEmpty())
        return;

    // Going up from a location root returns to the location list rather than the file-system parent,
    // which may be outside accessible storage.
    if (isLocationRoot(current_path_))
        setPath(QString());
    else
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
void FilePanelWidget::onSendClicked()
{
    const QList<QTreeWidgetItem*> selected = list_->selectedItems();

    QList<FileTransfer::Item> items;
    for (QTreeWidgetItem* item : selected)
    {
        // Drive rows are not transferable; only files and folders are.
        if (item->data(0, kPathRole).isValid())
            continue;

        items.emplace_back(item->data(0, kNameRole).toString(),
                           item->data(0, kSizeRole).toLongLong(),
                           item->data(0, kIsDirRole).toBool());
    }

    if (!items.isEmpty())
        emit sig_sendRequested(items);
}

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::onDeleteClicked()
{
    const QList<QTreeWidgetItem*> selected = list_->selectedItems();

    FileRemover::TaskList items;
    for (QTreeWidgetItem* item : selected)
    {
        // Drive rows cannot be removed; only files and folders are.
        if (item->data(0, kPathRole).isValid())
            continue;

        items.emplace_back(current_path_ + item->data(0, kNameRole).toString(),
                           item->data(0, kIsDirRole).toBool());
    }

    if (items.empty())
        return;

    if (!MessageDialog::confirm(this, tr("Delete"), tr("Delete the selected items?"), tr("Delete")))
        return;

    emit sig_removeRequested(items);
}

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::setPath(const QString& path)
{
    current_path_ = path;
    selectCurrentLocation();
    up_button_->setEnabled(!path.isEmpty());
    send_button_->setEnabled(!path.isEmpty());
    delete_button_->setEnabled(!path.isEmpty());

    refresh();
}

//--------------------------------------------------------------------------------------------------
void FilePanelWidget::selectCurrentLocation()
{
    // Item 0 is "This computer"; pick the location whose path is the longest prefix of the current
    // one, so a folder deep inside a drive still shows that drive as the selected location.
    int best_index = 0;
    int best_length = -1;

    for (int i = 1; i < path_combo_->count(); ++i)
    {
        const QString location = path_combo_->itemData(i).toString();
        if (current_path_.startsWith(location) && location.length() > best_length)
        {
            best_index = i;
            best_length = location.length();
        }
    }

    path_combo_->setCurrentIndex(best_index);
}

//--------------------------------------------------------------------------------------------------
bool FilePanelWidget::isLocationRoot(const QString& path) const
{
    QString trimmed = path;
    if (trimmed.endsWith('/'))
        trimmed.chop(1);

    for (int i = 1; i < path_combo_->count(); ++i)
    {
        QString location = path_combo_->itemData(i).toString();
        if (location.endsWith('/'))
            location.chop(1);

        if (location == trimmed)
            return true;
    }

    return false;
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
