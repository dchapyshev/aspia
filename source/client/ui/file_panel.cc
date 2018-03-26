//
// PROJECT:         Aspia
// FILE:            client/ui/file_panel.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "client/ui/file_panel.h"

#include <QDebug>
#include <QMessageBox>
#include <QStyledItemDelegate>

#include "client/ui/file_item.h"
#include "client/ui/file_remove_dialog.h"
#include "client/file_platform_util.h"
#include "client/file_request.h"
#include "client/file_status.h"

namespace aspia {

namespace {

class NoEditDelegate : public QStyledItemDelegate
{
public:
    NoEditDelegate(QWidget* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
        // Nothing
    }

    QWidget* createEditor(QWidget* parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        return nullptr;
    }

private:
    DISALLOW_COPY_AND_ASSIGN(NoEditDelegate);
};

QString normalizePath(const QString& path)
{
    QString normalized_path = path;

    normalized_path.replace('\\', '/');

    if (!normalized_path.endsWith('/'))
        normalized_path += '/';

    return normalized_path;
}

} // namespace

FilePanel::FilePanel(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);

    ui.tree->setItemDelegateForColumn(1, new NoEditDelegate(this));
    ui.tree->setItemDelegateForColumn(2, new NoEditDelegate(this));
    ui.tree->setItemDelegateForColumn(3, new NoEditDelegate(this));

    connect(ui.address_bar, SIGNAL(activated(int)), SLOT(addressItemChanged(int)));

    connect(ui.tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
            SLOT(fileDoubleClicked(QTreeWidgetItem*, int)));

    connect(ui.tree, SIGNAL(itemSelectionChanged()), SLOT(fileSelectionChanged()));

    connect(ui.tree, SIGNAL(itemChanged(QTreeWidgetItem*, int)),
            SLOT(fileItemChanged(QTreeWidgetItem*, int)));

    connect(ui.button_up, SIGNAL(pressed()), SLOT(toParentFolder()));
    connect(ui.button_refresh, SIGNAL(pressed()), SLOT(refresh()));
    connect(ui.button_add, SIGNAL(pressed()), SLOT(addFolder()));
    connect(ui.button_delete, SIGNAL(pressed()), SLOT(removeSelected()));
    connect(ui.button_send, SIGNAL(pressed()), SLOT(sendSelected()));
}

FilePanel::~FilePanel() = default;

void FilePanel::setPanelName(const QString& name)
{
    ui.label_name->setText(name);
}

QString FilePanel::currentPath() const
{
    return current_path_;
}

void FilePanel::setCurrentPath(const QString& path)
{
    current_path_ = normalizePath(path);

    for (int i = 0; i < ui.address_bar->count(); ++i)
    {
        if (addressItemPath(i) == current_path_)
        {
            ui.address_bar->setCurrentIndex(i);
            addressItemChanged(i);
            return;
        }
    }

    int current_item = ui.address_bar->count();

    ui.address_bar->addItem(FilePlatformUtil::directoryIcon(), current_path_);
    ui.address_bar->setCurrentIndex(current_item);

    addressItemChanged(current_item);
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
    emit request(FileRequest::driveListRequest(), FileReplyReceiver(this, "reply"));
}

void FilePanel::addressItemChanged(int index)
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

    for (int i = 0; i < ui.address_bar->count(); ++i)
    {
        if (ui.address_bar->itemData(i).toString().isEmpty() &&
            ui.address_bar->itemText(i) != ui.address_bar->itemText(index))
        {
            ui.address_bar->removeItem(i);
            break;
        }
    }

    emit request(FileRequest::fileListRequest(current_path_), FileReplyReceiver(this, "reply"));
}

void FilePanel::fileDoubleClicked(QTreeWidgetItem* item, int column)
{
    FileItem* file_item = reinterpret_cast<FileItem*>(item);
    if (!file_item->isDirectory())
        return;

    toChildFolder(file_item->text(0));
}

void FilePanel::fileSelectionChanged()
{
    int selected_count = 0;

    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
    {
        if (ui.tree->isItemSelected(ui.tree->topLevelItem(i)))
            ++selected_count;
    }

    ui.label_status->setText(tr("%1 object(s) selected").arg(selected_count));
}

void FilePanel::fileItemChanged(QTreeWidgetItem* item, int column)
{
    if (column != 0)
        return;

    FileItem* file_item = reinterpret_cast<FileItem*>(item);

    QString initial_name = file_item->initialName();
    QString current_name = file_item->currentName();

    if (initial_name.isEmpty())
    {
        // New item.
        emit request(FileRequest::createDirectoryRequest(currentPath() + current_name),
                     FileReplyReceiver(this, "reply"));
    }
    else
    {
        // Rename item.
        if (current_name == initial_name)
            return;

        emit request(FileRequest::renameRequest(currentPath() + initial_name,
                                                currentPath() + current_name),
                     FileReplyReceiver(this, "reply"));
    }
}

void FilePanel::toChildFolder(const QString& child_name)
{
    setCurrentPath(current_path_ + child_name);
}

void FilePanel::toParentFolder()
{
    int from = -1;
    if (current_path_.endsWith('/'))
        from = -2;

    int last_slash = current_path_.lastIndexOf('/', from);
    if (last_slash == -1)
        return;

    setCurrentPath(current_path_.left(last_slash));
}

void FilePanel::addFolder()
{
    FileItem* item = new FileItem("");

    ui.tree->addTopLevelItem(item);
    ui.tree->editItem(item);
}

void FilePanel::removeSelected()
{
    QList<FileRemoveTask> tasks;

    for (int i = 0; i < ui.tree->topLevelItemCount(); ++i)
    {
        FileItem* file_item = reinterpret_cast<FileItem*>(ui.tree->topLevelItem(i));

        if (ui.tree->isItemSelected(file_item))
        {
            tasks.push_back(FileRemoveTask(currentPath() + file_item->currentName(),
                                           file_item->isDirectory()));
        }
    }

    if (tasks.isEmpty())
        return;

    if (QMessageBox::question(this,
                              tr("Confirmation"),
                              tr("Are you sure you want to delete the selected items?"),
                              QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
    {
        return;
    }

    FileRemoveDialog* progress_dialog = new FileRemoveDialog(this);
    remover_ = new FileRemover(this);

    connect(remover_, SIGNAL(started()), progress_dialog, SLOT(open()));
    connect(remover_, SIGNAL(finished()), progress_dialog, SLOT(close()));
    connect(remover_, SIGNAL(finished()), SLOT(refresh()));
    connect(remover_, SIGNAL(finished()), remover_, SLOT(deleteLater()));

    connect(remover_, SIGNAL(progressChanged(const QString&, int)),
            progress_dialog, SLOT(setProgress(const QString&, int)));
    connect(remover_, SIGNAL(error(FileRemover::Actions, const QString&)),
            SLOT(removeError(FileRemover::Actions, const QString&)));

    connect(remover_,
            SIGNAL(request(const proto::file_transfer::Request&, const FileReplyReceiver&)),
            SIGNAL(request(const proto::file_transfer::Request&, const FileReplyReceiver&)));

    connect(progress_dialog, SIGNAL(cancel()), SLOT(removeCancel()));

    remover_->start(tasks);
}

void FilePanel::removeError(FileRemover::Actions actions, const QString& message)
{
    QMessageBox dialog(this);

    dialog.setWindowTitle(tr("Warning"));
    dialog.setIcon(QMessageBox::Warning);
    dialog.setText(message);

    QAbstractButton* skip_button = nullptr;
    QAbstractButton* skip_all_button = nullptr;
    QAbstractButton* abort_button = nullptr;

    if (actions & FileRemover::Skip)
        skip_button = dialog.addButton(tr("Skip"), QMessageBox::ButtonRole::ActionRole);

    if (actions & FileRemover::SkipAll)
        skip_all_button = dialog.addButton(tr("Skip All"), QMessageBox::ButtonRole::ActionRole);

    if (actions & FileRemover::Abort)
        abort_button = dialog.addButton(tr("Abort"), QMessageBox::ButtonRole::ActionRole);

    dialog.exec();

    QAbstractButton* button = dialog.clickedButton();
    if (button != nullptr)
    {
        if (button == skip_button)
        {
            remover_->applyAction(FileRemover::Skip);
            return;
        }
        else if (button == skip_all_button)
        {
            remover_->applyAction(FileRemover::SkipAll);
            return;
        }
    }

    remover_->applyAction(FileRemover::Abort);
}

void FilePanel::removeCancel()
{
    remover_.clear();
}

void FilePanel::sendSelected()
{
    // TODO
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

        QString path = normalizePath(QString::fromUtf8(item.path().c_str(), item.path().size()));
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
    {
        ui.tree->addTopLevelItem(new FileItem(list.item(i)));
    }
}

} // namespace aspia
