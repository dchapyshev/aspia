//
// PROJECT:         Aspia
// FILE:            console/console_window.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/console_window.h"

#include <QCryptographicHash>
#include <QDesktopServices>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>

#include "console/about_dialog.h"
#include "console/address_book_dialog.h"
#include "console/computer_group_dialog.h"
#include "console/computer_dialog.h"
#include "console/computer.h"
#include "console/open_address_book_dialog.h"
#include "crypto/string_encryptor.h"

namespace aspia {

ConsoleWindow::ConsoleWindow(const QString& file_path, QWidget* parent)
    : QMainWindow(parent),
      file_path_(file_path)
{
    ui.setupUi(this);

    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("WindowGeometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("WindowState")).toByteArray());

    connect(ui.action_new, &QAction::triggered, this, &ConsoleWindow::newAction);
    connect(ui.action_open, &QAction::triggered, this, &ConsoleWindow::openAction);
    connect(ui.action_save, &QAction::triggered, this, &ConsoleWindow::saveAction);
    connect(ui.action_save_as, &QAction::triggered, this, &ConsoleWindow::saveAsAction);
    connect(ui.action_close, &QAction::triggered, this, &ConsoleWindow::closeAction);

    connect(ui.action_address_book_properties, &QAction::triggered,
            this, &ConsoleWindow::addressBookPropertiesAction);

    connect(ui.action_add_computer, &QAction::triggered,
            this, &ConsoleWindow::addComputerAction);

    connect(ui.action_modify_computer, &QAction::triggered,
            this, &ConsoleWindow::modifyComputerAction);

    connect(ui.action_delete_computer, &QAction::triggered,
            this, &ConsoleWindow::deleteComputerAction);

    connect(ui.action_add_computer_group, &QAction::triggered,
            this, &ConsoleWindow::addComputerGroupAction);

    connect(ui.action_modify_computer_group, &QAction::triggered,
            this, &ConsoleWindow::modifyComputerGroupAction);

    connect(ui.action_delete_computer_group, &QAction::triggered,
            this, &ConsoleWindow::deleteComputerGroupAction);

    connect(ui.action_online_help, &QAction::triggered, this, &ConsoleWindow::onlineHelpAction);
    connect(ui.action_about, &QAction::triggered, this, &ConsoleWindow::aboutAction);
    connect(ui.action_exit, &QAction::triggered, this, &ConsoleWindow::exitAction);

    connect(ui.tree_group, &ComputerGroupTree::itemClicked,
            this, &ConsoleWindow::groupItemClicked);

    connect(ui.tree_group, &ComputerGroupTree::customContextMenuRequested,
            this, &ConsoleWindow::groupContextMenu);

    connect(ui.tree_group, &ComputerGroupTree::itemCollapsed,
            this, &ConsoleWindow::groupItemCollapsed);

    connect(ui.tree_group, &ComputerGroupTree::itemExpanded,
            this, &ConsoleWindow::groupItemExpanded);

    connect(ui.tree_computer, &ComputerTree::itemClicked,
            this, &ConsoleWindow::computerItemClicked);

    connect(ui.tree_computer, &ComputerTree::customContextMenuRequested,
            this, &ConsoleWindow::computerContextMenu);

    connect(ui.action_desktop_manage, &QAction::toggled,
            this, &ConsoleWindow::desktopManageSessionToggled);

    connect(ui.action_desktop_view, &QAction::toggled,
            this, &ConsoleWindow::desktopViewSessionToggled);

    connect(ui.action_file_transfer, &QAction::toggled,
            this, &ConsoleWindow::fileTransferSessionToggled);

    if (!file_path_.isEmpty())
        openAddressBook(file_path_);
}

ConsoleWindow::~ConsoleWindow() = default;

void ConsoleWindow::newAction()
{
    if (!closeAddressBook())
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

    setChanged(true);
}

void ConsoleWindow::openAction()
{
    openAddressBook(QString());
}

void ConsoleWindow::saveAction()
{
    if (!address_book_)
        return;

    saveAddressBook(file_path_);
}

void ConsoleWindow::saveAsAction()
{
    saveAddressBook(QString());
}

void ConsoleWindow::closeAction()
{
    closeAddressBook();
}

void ConsoleWindow::addressBookPropertiesAction()
{
    if (!address_book_)
        return;

    AddressBookDialog dialog(this, address_book_.get(), &encryption_type_, &password_);
    if (dialog.exec() != QDialog::Accepted)
        return;

    setChanged(true);
}

void ConsoleWindow::addComputerAction()
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

    setChanged(true);
}

void ConsoleWindow::modifyComputerAction()
{
    if (!address_book_)
        return;

    Computer* current_item = reinterpret_cast<Computer*>(ui.tree_computer->currentItem());
    if (!current_item)
        return;

    ComputerDialog dialog(this, current_item, current_item->ParentComputerGroup());
    if (dialog.exec() != QDialog::Accepted)
        return;

    setChanged(true);
}

void ConsoleWindow::deleteComputerAction()
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
            setChanged(true);
    }
}

void ConsoleWindow::addComputerGroupAction()
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
    setChanged(true);
}

void ConsoleWindow::modifyComputerGroupAction()
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

void ConsoleWindow::deleteComputerGroupAction()
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
            setChanged(true);
    }
}

void ConsoleWindow::onlineHelpAction()
{
    QDesktopServices::openUrl(QUrl(tr("https://aspia.net/help")));
}

void ConsoleWindow::aboutAction()
{
    AboutDialog(this).exec();
}

void ConsoleWindow::exitAction()
{
    close();
}

void ConsoleWindow::groupItemClicked(QTreeWidgetItem* item, int /* column */)
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

    updateComputerList(current_item);
}

void ConsoleWindow::groupContextMenu(const QPoint& point)
{
    ComputerGroup* current_item = reinterpret_cast<ComputerGroup*>(ui.tree_group->itemAt(point));
    if (!current_item)
        return;

    ui.tree_group->setCurrentItem(current_item);
    groupItemClicked(current_item, 0);

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

void ConsoleWindow::groupItemCollapsed(QTreeWidgetItem* item)
{
    ComputerGroup* current_item = reinterpret_cast<ComputerGroup*>(item);
    if (!current_item)
        return;

    current_item->SetExpanded(false);
    setChanged(true);
}

void ConsoleWindow::groupItemExpanded(QTreeWidgetItem* item)
{
    ComputerGroup* current_item = reinterpret_cast<ComputerGroup*>(item);
    if (!current_item)
        return;

    current_item->SetExpanded(true);
    setChanged(true);
}

void ConsoleWindow::computerItemClicked(QTreeWidgetItem* item, int /* column */)
{
    Computer* current_item = reinterpret_cast<Computer*>(item);
    if (!current_item)
        return;

    ui.action_modify_computer->setEnabled(true);
    ui.action_delete_computer->setEnabled(true);
}

void ConsoleWindow::computerContextMenu(const QPoint& point)
{
    QMenu menu;

    Computer* current_item = reinterpret_cast<Computer*>(ui.tree_computer->itemAt(point));
    if (current_item)
    {
        ui.tree_computer->setCurrentItem(current_item);
        computerItemClicked(current_item, 0);

        menu.addAction(ui.action_desktop_manage_connect);
        menu.addAction(ui.action_desktop_view_connect);
        menu.addAction(ui.action_file_transfer_connect);
        menu.addSeparator();
        menu.addAction(ui.action_modify_computer);
        menu.addAction(ui.action_delete_computer);
        menu.addSeparator();
    }

    menu.addAction(ui.action_add_computer);
    menu.exec(ui.tree_computer->mapToGlobal(point));
}

void ConsoleWindow::desktopManageSessionToggled(bool checked)
{
    if (checked)
    {
        ui.action_desktop_view->setChecked(false);
        ui.action_file_transfer->setChecked(false);
    }
}

void ConsoleWindow::desktopViewSessionToggled(bool checked)
{
    if (checked)
    {
        ui.action_desktop_manage->setChecked(false);
        ui.action_file_transfer->setChecked(false);
    }
}

void ConsoleWindow::fileTransferSessionToggled(bool checked)
{
    if (checked)
    {
        ui.action_desktop_manage->setChecked(false);
        ui.action_desktop_view->setChecked(false);
    }
}

void ConsoleWindow::closeEvent(QCloseEvent* event)
{
    if (address_book_ && is_changed_)
    {
        if (!closeAddressBook())
            return;
    }

    QSettings settings;
    settings.setValue(QStringLiteral("WindowGeometry"), saveGeometry());
    settings.setValue(QStringLiteral("WindowState"), saveState());

    QMainWindow::closeEvent(event);
}

void ConsoleWindow::showOpenError(const QString& message)
{
    QMessageBox dialog(this);

    dialog.setIcon(QMessageBox::Warning);
    dialog.setWindowTitle(tr("Warning"));
    dialog.setInformativeText(message);
    dialog.setText(tr("Could not open address book"));
    dialog.setStandardButtons(QMessageBox::Ok);

    dialog.exec();
}

void ConsoleWindow::showSaveError(const QString& message)
{
    QMessageBox dialog(this);

    dialog.setIcon(QMessageBox::Warning);
    dialog.setWindowTitle(tr("Warning"));
    dialog.setInformativeText(message);
    dialog.setText(tr("Failed to save address book"));
    dialog.setStandardButtons(QMessageBox::Ok);

    dialog.exec();
}

void ConsoleWindow::updateComputerList(ComputerGroup* computer_group)
{
    for (int i = ui.tree_computer->topLevelItemCount() - 1; i >= 0; --i)
    {
        QTreeWidgetItem* item = ui.tree_computer->takeTopLevelItem(i);
        delete item;
    }

    ui.tree_computer->addTopLevelItems(computer_group->ComputerList());
}

void ConsoleWindow::setChanged(bool changed)
{
    ui.action_save->setEnabled(changed);
    is_changed_ = changed;
}

bool ConsoleWindow::openAddressBook(const QString& file_path)
{
    if (!closeAddressBook())
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
        showOpenError(tr("Unable to open address book file."));
        return false;
    }

    QByteArray buffer = file.readAll();
    if (!buffer.size())
    {
        showOpenError(tr("Unable to read address book file."));
        return false;
    }

    proto::AddressBook address_book;

    if (!address_book.ParseFromArray(buffer.data(), buffer.size()))
    {
        showOpenError(tr("The address book file is corrupted or has an unknown format."));
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

            password_ = dialog.password();

            QCryptographicHash key_hash(QCryptographicHash::Sha256);
            key_hash.addData(password_.toUtf8());

            std::string decrypted;
            if (!DecryptString(address_book.data(), key_hash.result(), decrypted))
            {
                showOpenError(tr("Unable to decrypt the address book with the specified password."));
                return false;
            }

            address_book_ = AddressBook::Open(decrypted);
        }
        break;

        default:
        {
            showOpenError(tr("The address book file is encrypted with an unsupported encryption type."));
            return false;
        }
        break;
    }

    if (!address_book_)
    {
        showOpenError(tr("The address book file is corrupted or has an unknown format."));
        return false;
    }

    ui.tree_computer->setEnabled(true);
    ui.tree_group->setEnabled(true);

    ui.tree_group->addTopLevelItem(address_book_.get());
    ui.tree_group->setCurrentItem(address_book_.get());
    ui.tree_group->setFocus();

    groupItemClicked(address_book_.get(), 0);

    // Restore the state of the address book after adding it to the control.
    address_book_->RestoreAppearance();

    ui.action_address_book_properties->setEnabled(true);

    ui.action_save->setEnabled(true);
    ui.action_save_as->setEnabled(true);
    ui.action_close->setEnabled(true);

    updateComputerList(address_book_.get());
    setChanged(false);
    return true;
}

bool ConsoleWindow::saveAddressBook(const QString& file_path)
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
            qFatal("Unknown encryption type: %d", address_book.encryption_type());
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
        showSaveError(tr("Unable to create or open address book file."));
        return false;
    }

    std::string buffer = address_book.SerializeAsString();
    if (file.write(buffer.c_str(), buffer.size()) != buffer.size())
    {
        showSaveError(tr("Unable to write address book file."));
        return false;
    }

    setChanged(false);
    return true;
}

bool ConsoleWindow::closeAddressBook()
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
                    if (!saveAddressBook(file_path_))
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

    setChanged(false);
    return true;
}

} // namespace aspia
