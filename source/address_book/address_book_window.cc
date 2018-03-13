//
// PROJECT:         Aspia
// FILE:            address_book/address_book_main.h
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "address_book/address_book_window.h"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

#include "address_book/address_book_dialog.h"
#include "address_book/computer_group_dialog.h"
#include "address_book/computer_dialog.h"
#include "address_book/computer.h"
#include "address_book/open_address_book_dialog.h"
#include "base/logging.h"
#include "crypto/string_encryptor.h"

namespace aspia {

AddressBookWindow::AddressBookWindow(const QString& file_path, QWidget* parent)
    : QMainWindow(parent),
      file_path_(file_path)
{
    ui.setupUi(this);

    QSettings settings;
    restoreGeometry(settings.value("WindowGeometry").toByteArray());
    restoreState(settings.value("WindowState").toByteArray());

    connect(ui.action_new, SIGNAL(triggered()),
            this, SLOT(OnNewAction()));
    connect(ui.action_open, SIGNAL(triggered()),
            this, SLOT(OnOpenAction()));
    connect(ui.action_save, SIGNAL(triggered()),
            this, SLOT(OnSaveAction()));
    connect(ui.action_save_as, SIGNAL(triggered()),
            this, SLOT(OnSaveAsAction()));
    connect(ui.action_close, SIGNAL(triggered()),
            this, SLOT(OnCloseAction()));
    connect(ui.action_address_book_properties, SIGNAL(triggered()),
            this, SLOT(OnAddressBookPropertiesAction()));
    connect(ui.action_add_computer, SIGNAL(triggered()),
            this, SLOT(OnAddComputerAction()));
    connect(ui.action_modify_computer, SIGNAL(triggered()),
            this, SLOT(OnModifyComputerAction()));
    connect(ui.action_delete_computer, SIGNAL(triggered()),
            this, SLOT(OnDeleteComputerAction()));
    connect(ui.action_add_computer_group, SIGNAL(triggered()),
            this, SLOT(OnAddComputerGroupAction()));
    connect(ui.action_modify_computer_group, SIGNAL(triggered()),
            this, SLOT(OnModifyComputerGroupAction()));
    connect(ui.action_delete_computer_group, SIGNAL(triggered()),
            this, SLOT(OnDeleteComputerGroupAction()));
    connect(ui.action_online_help, SIGNAL(triggered()), this, SLOT(OnOnlineHelpAction()));
    connect(ui.action_exit, SIGNAL(triggered()), this, SLOT(OnExitAction()));
    connect(ui.tree_group, SIGNAL(itemClicked(QTreeWidgetItem*, int)),
            this, SLOT(OnGroupItemClicked(QTreeWidgetItem*, int)));
    connect(ui.tree_group, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(OnGroupContextMenu(const QPoint&)));
    connect(ui.tree_group, SIGNAL(itemCollapsed(QTreeWidgetItem*)),
            this, SLOT(OnGroupItemCollapsed(QTreeWidgetItem*)));
    connect(ui.tree_group, SIGNAL(itemExpanded(QTreeWidgetItem*)),
            this, SLOT(OnGroupItemExpanded(QTreeWidgetItem*)));
    connect(ui.tree_computer, SIGNAL(itemClicked(QTreeWidgetItem*, int)),
            this, SLOT(OnComputerItemClicked(QTreeWidgetItem*, int)));
    connect(ui.tree_computer, SIGNAL(customContextMenuRequested(const QPoint&)),
            this, SLOT(OnComputerContextMenu(const QPoint&)));
    connect(ui.action_desktop_manage, SIGNAL(toggled(bool)),
            this, SLOT(OnDesktopManageSessionToggled(bool)));
    connect(ui.action_desktop_view, SIGNAL(toggled(bool)),
            this, SLOT(OnDesktopViewSessionToggled(bool)));
    connect(ui.action_file_transfer, SIGNAL(toggled(bool)),
            this, SLOT(OnFileTransferSessionToggled(bool)));
    connect(ui.action_system_info, SIGNAL(toggled(bool)),
            this, SLOT(OnSystemInfoSessionToggled(bool)));

    if (!file_path_.isEmpty())
        OpenAddressBook(file_path_);
}

void AddressBookWindow::OnNewAction()
{
    if (!CloseAddressBook())
        return;

    std::unique_ptr<AddressBook> address_book = AddressBook::Create();

    encryption_type_ = proto::AddressBook::ENCRYPTION_TYPE_NONE;
    password_.clear();

    AddressBookDialog dialog(this, address_book.get(), &encryption_type_, &password_);
    if (dialog.exec() != QDialog::Accepted)
        return;

    address_book_ = std::move(address_book);

    ui.tree_computer->setEnabled(true);
    ui.tree_group->setEnabled(true);
    ui.tree_group->addTopLevelItem(address_book_.get());

    SetChanged(true);
}

void AddressBookWindow::OnOpenAction()
{
    OpenAddressBook(QString());
}

void AddressBookWindow::OnSaveAction()
{
    if (!address_book_)
        return;

    SaveAddressBook(file_path_);
}

void AddressBookWindow::OnSaveAsAction()
{
    SaveAddressBook(QString());
}

void AddressBookWindow::OnCloseAction()
{
    CloseAddressBook();
}

void AddressBookWindow::OnAddressBookPropertiesAction()
{
    if (!address_book_)
        return;

    AddressBookDialog dialog(this, address_book_.get(), &encryption_type_, &password_);
    if (dialog.exec() != QDialog::Accepted)
        return;

    SetChanged(true);
}

void AddressBookWindow::OnAddComputerAction()
{
    if (!address_book_)
        return;

    ComputerGroup* parent_item = reinterpret_cast<ComputerGroup*>(ui.tree_group->currentItem());
    if (!parent_item)
        return;

    std::unique_ptr<Computer> computer = Computer::Create();

    ComputerDialog dialog(this, computer.get(), parent_item);
    if (dialog.exec() != QDialog::Accepted)
        return;

    Computer* computer_released = computer.release();

    parent_item->AddChildComputer(computer_released);
    if (ui.tree_group->currentItem() == parent_item)
        ui.tree_computer->addTopLevelItem(computer_released);

    SetChanged(true);
}

void AddressBookWindow::OnModifyComputerAction()
{
    if (!address_book_)
        return;

    Computer* current_item = reinterpret_cast<Computer*>(ui.tree_computer->currentItem());
    if (!current_item)
        return;

    ComputerDialog dialog(this, current_item, current_item->ParentComputerGroup());
    if (dialog.exec() != QDialog::Accepted)
        return;

    SetChanged(true);
}

void AddressBookWindow::OnDeleteComputerAction()
{
    if (!address_book_)
        return;

    Computer* current_item = reinterpret_cast<Computer*>(ui.tree_computer->currentItem());
    if (!current_item)
        return;

    QString message =
        tr("Are you sure you want to delete computer \"%1\"?").arg(current_item->Name());

    if (QMessageBox(QMessageBox::Question,
                    tr("Confirmation"),
                    message,
                    QMessageBox::Ok | QMessageBox::Cancel,
                    this).exec() == QMessageBox::Ok)
    {
        ComputerGroup* parent_group = current_item->ParentComputerGroup();
        if (parent_group->DeleteChildComputer(current_item))
            SetChanged(true);
    }
}

void AddressBookWindow::OnAddComputerGroupAction()
{
    if (!address_book_)
        return;

    ComputerGroup* parent_item = reinterpret_cast<ComputerGroup*>(ui.tree_group->currentItem());
    if (!parent_item)
        return;

    std::unique_ptr<ComputerGroup> computer_group = ComputerGroup::Create();

    ComputerGroupDialog dialog(this, computer_group.get(), parent_item);
    if (dialog.exec() != QDialog::Accepted)
        return;

    ComputerGroup* computer_group_released = computer_group.release();
    parent_item->AddChildComputerGroup(computer_group_released);
    ui.tree_group->setCurrentItem(computer_group_released);
    SetChanged(true);
}

void AddressBookWindow::OnModifyComputerGroupAction()
{
    if (!address_book_)
        return;

    ComputerGroup* current_item = reinterpret_cast<ComputerGroup*>(ui.tree_group->currentItem());
    if (!current_item)
        return;

    ComputerGroup* parent_item = current_item->ParentComputerGroup();
    if (!parent_item)
        return;

    ComputerGroupDialog dialog(this, current_item, parent_item);
    if (dialog.exec() != QDialog::Accepted)
        return;
}

void AddressBookWindow::OnDeleteComputerGroupAction()
{
    if (!address_book_)
        return;

    ComputerGroup* current_item = reinterpret_cast<ComputerGroup*>(ui.tree_group->currentItem());
    if (!current_item)
        return;

    ComputerGroup* parent_item = current_item->ParentComputerGroup();
    if (!parent_item)
        return;

    QString message =
        tr("Are you sure you want to delete computer group \"%1\" and all child items?")
        .arg(current_item->Name());

    if (QMessageBox(QMessageBox::Question,
                    tr("Confirmation"),
                    message,
                    QMessageBox::Ok | QMessageBox::Cancel,
                    this).exec() == QMessageBox::Ok)
    {
        if (parent_item->DeleteChildComputerGroup(current_item))
            SetChanged(true);
    }
}

void AddressBookWindow::OnOnlineHelpAction()
{
    QDesktopServices::openUrl(QUrl(tr("https://aspia.org/help")));
}

void AddressBookWindow::OnExitAction()
{
    close();
}

void AddressBookWindow::OnGroupItemClicked(QTreeWidgetItem* item, int /* column */)
{
    ComputerGroup* current_item = reinterpret_cast<ComputerGroup*>(item);
    if (!current_item)
        return;

    ui.action_add_computer_group->setEnabled(true);

    ui.action_add_computer->setEnabled(true);
    ui.action_modify_computer->setEnabled(false);
    ui.action_delete_computer->setEnabled(false);

    if (!current_item->ParentComputerGroup())
    {
        ui.action_modify_computer_group->setEnabled(false);
        ui.action_delete_computer_group->setEnabled(false);
    }
    else
    {
        ui.action_modify_computer_group->setEnabled(true);
        ui.action_delete_computer_group->setEnabled(true);
    }

    UpdateComputerList(current_item);
}

void AddressBookWindow::OnGroupContextMenu(const QPoint& point)
{
    ComputerGroup* current_item = reinterpret_cast<ComputerGroup*>(ui.tree_group->itemAt(point));
    if (!current_item)
        return;

    ui.tree_group->setCurrentItem(current_item);
    OnGroupItemClicked(current_item, 0);

    QMenu menu;

    if (!current_item->ParentComputerGroup())
    {
        menu.addAction(ui.action_address_book_properties);
    }
    else
    {
        menu.addAction(ui.action_modify_computer_group);
        menu.addAction(ui.action_delete_computer_group);
    }

    menu.addSeparator();
    menu.addAction(ui.action_add_computer_group);
    menu.addAction(ui.action_add_computer);

    menu.exec(ui.tree_group->mapToGlobal(point));
}

void AddressBookWindow::OnGroupItemCollapsed(QTreeWidgetItem* item)
{
    ComputerGroup* current_item = reinterpret_cast<ComputerGroup*>(item);
    if (!current_item)
        return;

    current_item->SetExpanded(false);
    SetChanged(true);
}

void AddressBookWindow::OnGroupItemExpanded(QTreeWidgetItem* item)
{
    ComputerGroup* current_item = reinterpret_cast<ComputerGroup*>(item);
    if (!current_item)
        return;

    current_item->SetExpanded(true);
    SetChanged(true);
}

void AddressBookWindow::OnComputerItemClicked(QTreeWidgetItem* item, int /* column */)
{
    Computer* current_item = reinterpret_cast<Computer*>(item);
    if (!current_item)
        return;

    ui.action_modify_computer->setEnabled(true);
    ui.action_delete_computer->setEnabled(true);
}

void AddressBookWindow::OnComputerContextMenu(const QPoint& point)
{
    QMenu menu;

    Computer* current_item = reinterpret_cast<Computer*>(ui.tree_computer->itemAt(point));
    if (current_item)
    {
        ui.tree_computer->setCurrentItem(current_item);
        OnComputerItemClicked(current_item, 0);

        menu.addAction(ui.action_desktop_manage_connect);
        menu.addAction(ui.action_desktop_view_connect);
        menu.addAction(ui.action_file_transfer_connect);
        menu.addAction(ui.action_system_info_connect);
        menu.addSeparator();
        menu.addAction(ui.action_modify_computer);
        menu.addAction(ui.action_delete_computer);
        menu.addSeparator();
    }

    menu.addAction(ui.action_add_computer);
    menu.exec(ui.tree_computer->mapToGlobal(point));
}

void AddressBookWindow::OnDesktopManageSessionToggled(bool checked)
{
    if (checked)
    {
        ui.action_desktop_view->setChecked(false);
        ui.action_file_transfer->setChecked(false);
        ui.action_system_info->setChecked(false);
    }
}

void AddressBookWindow::OnDesktopViewSessionToggled(bool checked)
{
    if (checked)
    {
        ui.action_desktop_manage->setChecked(false);
        ui.action_file_transfer->setChecked(false);
        ui.action_system_info->setChecked(false);
    }
}

void AddressBookWindow::OnFileTransferSessionToggled(bool checked)
{
    if (checked)
    {
        ui.action_desktop_manage->setChecked(false);
        ui.action_desktop_view->setChecked(false);
        ui.action_system_info->setChecked(false);
    }
}

void AddressBookWindow::OnSystemInfoSessionToggled(bool checked)
{
    if (checked)
    {
        ui.action_desktop_manage->setChecked(false);
        ui.action_desktop_view->setChecked(false);
        ui.action_file_transfer->setChecked(false);
    }
}

void AddressBookWindow::closeEvent(QCloseEvent* event)
{
    if (address_book_ && is_changed_)
    {
        if (!CloseAddressBook())
            return;
    }

    QSettings settings;
    settings.setValue("WindowGeometry", saveGeometry());
    settings.setValue("WindowState", saveState());

    QMainWindow::closeEvent(event);
}

void AddressBookWindow::ShowOpenError(const QString& message)
{
    QMessageBox dialog(this);

    dialog.setIcon(QMessageBox::Warning);
    dialog.setWindowTitle(tr("Warning"));
    dialog.setInformativeText(message);
    dialog.setText(tr("Could not open address book"));
    dialog.setStandardButtons(QMessageBox::Ok);

    dialog.exec();
}

void AddressBookWindow::ShowSaveError(const QString& message)
{
    QMessageBox dialog(this);

    dialog.setIcon(QMessageBox::Warning);
    dialog.setWindowTitle(tr("Warning"));
    dialog.setInformativeText(message);
    dialog.setText(tr("Failed to save address book"));
    dialog.setStandardButtons(QMessageBox::Ok);

    dialog.exec();
}

void AddressBookWindow::UpdateComputerList(ComputerGroup* computer_group)
{
    for (int i = ui.tree_computer->topLevelItemCount() - 1; i >= 0; --i)
    {
        QTreeWidgetItem* item = ui.tree_computer->takeTopLevelItem(i);
        delete item;
    }

    ui.tree_computer->addTopLevelItems(computer_group->ComputerList());
}

void AddressBookWindow::SetChanged(bool changed)
{
    ui.action_save->setEnabled(changed);
    is_changed_ = changed;
}

bool AddressBookWindow::OpenAddressBook(const QString& file_path)
{
    if (!CloseAddressBook())
        return false;

    file_path_ = file_path;
    if (file_path_.isEmpty())
    {
        file_path_ = QFileDialog::getOpenFileName(this,
                                                  tr("Open Address Book"),
                                                  QDir::homePath(),
                                                  tr("Aspia Address Book (*.aab)"));
        if (file_path_.isEmpty())
            return false;
    }

    QFile file(file_path_);
    if (!file.open(QIODevice::ReadOnly))
    {
        ShowOpenError(tr("Unable to open address book file."));
        return false;
    }

    QByteArray buffer = file.readAll();
    if (!buffer.size())
    {
        ShowOpenError(tr("Unable to read address book file."));
        return false;
    }

    proto::AddressBook address_book;

    if (!address_book.ParseFromArray(buffer.data(), buffer.size()))
    {
        ShowOpenError(tr("The address book file is corrupted or has an unknown format."));
        return false;
    }

    encryption_type_ = address_book.encryption_type();

    switch (address_book.encryption_type())
    {
        case proto::AddressBook::ENCRYPTION_TYPE_NONE:
            address_book_ = AddressBook::Open(address_book.data());
            break;

        case proto::AddressBook::ENCRYPTION_TYPE_XCHACHA20_POLY1305:
        {
            OpenAddressBookDialog dialog(this, address_book.encryption_type());
            if (dialog.exec() != QDialog::Accepted)
                return false;

            password_ = dialog.Password();

            QCryptographicHash key_hash(QCryptographicHash::Sha256);
            key_hash.addData(password_.toUtf8());

            std::string decrypted;
            if (!DecryptString(address_book.data(), key_hash.result(), decrypted))
            {
                ShowOpenError(tr("Unable to decrypt the address book with the specified password."));
                return false;
            }

            address_book_ = AddressBook::Open(decrypted);
        }
        break;

        default:
        {
            ShowOpenError(tr("The address book file is encrypted with an unsupported encryption type."));
            return false;
        }
        break;
    }

    if (!address_book_)
    {
        ShowOpenError(tr("The address book file is corrupted or has an unknown format."));
        return false;
    }

    ui.tree_computer->setEnabled(true);
    ui.tree_group->setEnabled(true);

    ui.tree_group->addTopLevelItem(address_book_.get());
    ui.tree_group->setCurrentItem(address_book_.get());
    ui.tree_group->setFocus();

    OnGroupItemClicked(address_book_.get(), 0);

    // Restore the state of the address book after adding it to the control.
    address_book_->RestoreAppearance();

    ui.action_address_book_properties->setEnabled(true);

    ui.action_save->setEnabled(true);
    ui.action_save_as->setEnabled(true);
    ui.action_close->setEnabled(true);

    UpdateComputerList(address_book_.get());
    SetChanged(false);
    return true;
}

bool AddressBookWindow::SaveAddressBook(const QString& file_path)
{
    if (!address_book_)
        return false;

    proto::AddressBook address_book;
    address_book.set_encryption_type(encryption_type_);

    switch (address_book.encryption_type())
    {
        case proto::AddressBook::ENCRYPTION_TYPE_NONE:
            address_book.set_data(address_book_->Serialize());
            break;

        case proto::AddressBook::ENCRYPTION_TYPE_XCHACHA20_POLY1305:
        {
            QCryptographicHash key_hash(QCryptographicHash::Sha256);
            key_hash.addData(password_.toUtf8());

            address_book.set_data(EncryptString(address_book_->Serialize(), key_hash.result()));
        }
        break;

        default:
            DLOG(LS_FATAL) << "Unknown encryption type: " << address_book.encryption_type();
            break;
    }

    file_path_ = file_path;
    if (file_path_.isEmpty())
    {
        file_path_ = QFileDialog::getSaveFileName(this,
                                                  tr("Save Address Book"),
                                                  QDir::homePath(),
                                                  tr("Aspia Address Book (*.aab)"));
        if (file_path_.isEmpty())
            return false;
    }

    QFile file(file_path_);
    if (!file.open(QIODevice::WriteOnly))
    {
        ShowSaveError(tr("Unable to create or open address book file."));
        return false;
    }

    std::string buffer = address_book.SerializeAsString();
    if (file.write(buffer.c_str(), buffer.size()) != buffer.size())
    {
        ShowSaveError(tr("Unable to write address book file."));
        return false;
    }

    SetChanged(false);
    return true;
}

bool AddressBookWindow::CloseAddressBook()
{
    if (address_book_)
    {
        if (is_changed_)
        {
            int ret = QMessageBox(QMessageBox::Question,
                                  tr("Confirmation"),
                                  tr("Address book changed. Save changes?"),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                                  this).exec();
            switch (ret)
            {
                case QMessageBox::Yes:
                {
                    if (!SaveAddressBook(file_path_))
                        return false;
                }
                break;

                case QMessageBox::Cancel:
                    return false;

                default:
                    break;
            }
        }

        for (int i = ui.tree_computer->topLevelItemCount() - 1; i >= 0; --i)
        {
            QTreeWidgetItem* item = ui.tree_computer->takeTopLevelItem(i);
            delete item;
        }

        address_book_.reset();
        file_path_.clear();
        password_.clear();
    }

    ui.tree_computer->setEnabled(false);
    ui.tree_group->setEnabled(false);

    ui.action_address_book_properties->setEnabled(false);

    ui.action_add_computer_group->setEnabled(false);
    ui.action_delete_computer_group->setEnabled(false);
    ui.action_modify_computer_group->setEnabled(false);

    ui.action_add_computer->setEnabled(false);
    ui.action_delete_computer->setEnabled(false);
    ui.action_modify_computer->setEnabled(false);

    ui.action_save->setEnabled(false);
    ui.action_save_as->setEnabled(false);
    ui.action_close->setEnabled(false);

    SetChanged(false);
    return true;
}

} // namespace aspia
