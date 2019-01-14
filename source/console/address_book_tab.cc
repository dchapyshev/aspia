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

#include "console/address_book_tab.h"

#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>

#include "common/message_serialization.h"
#include "console/address_book_dialog.h"
#include "console/computer_dialog.h"
#include "console/computer_factory.h"
#include "console/computer_group_dialog.h"
#include "console/computer_item.h"
#include "console/console_settings.h"
#include "console/open_address_book_dialog.h"
#include "crypto/data_cryptor_chacha20_poly1305.h"
#include "crypto/password_hash.h"
#include "crypto/secure_memory.h"

namespace console {

namespace {

void cleanupComputer(proto::address_book::Computer* computer)
{
    if (!computer)
        return;

    crypto::memZero(computer->mutable_name());
    crypto::memZero(computer->mutable_address());
    crypto::memZero(computer->mutable_username());
    crypto::memZero(computer->mutable_password());
    crypto::memZero(computer->mutable_comment());
}

void cleanupComputerGroup(proto::address_book::ComputerGroup* computer_group)
{
    if (!computer_group)
        return;

    for (int i = 0; i < computer_group->computer_size(); ++i)
        cleanupComputer(computer_group->mutable_computer(i));

    for (int i = 0; i < computer_group->computer_group_size(); ++i)
    {
        proto::address_book::ComputerGroup* child_group =
            computer_group->mutable_computer_group(i);

        crypto::memZero(child_group->mutable_name());
        crypto::memZero(child_group->mutable_comment());

        cleanupComputerGroup(child_group);
    }
}

void cleanupData(proto::address_book::Data* data)
{
    if (!data)
        return;

    cleanupComputerGroup(data->mutable_root_group());

    crypto::memZero(data->mutable_salt1());
    crypto::memZero(data->mutable_salt2());
}

void cleanupFile(proto::address_book::File* file)
{
    if (!file)
        return;

    crypto::memZero(file->mutable_hashing_salt());
    crypto::memZero(file->mutable_data());
}

} // namespace

AddressBookTab::AddressBookTab(const QString& file_path,
                               proto::address_book::File&& file,
                               proto::address_book::Data&& data,
                               QByteArray&& key,
                               QWidget* parent)
    : ConsoleTab(ConsoleTab::AddressBook, parent),
      file_path_(file_path),
      file_(std::move(file)),
      data_(std::move(data)),
      key_(std::move(key))
{
    ui.setupUi(this);

    Settings settings;

    QByteArray splitter_state = settings.splitterState();
    if (splitter_state.isEmpty())
    {
        QList<int> sizes;
        sizes.push_back(200);
        sizes.push_back(width() - 200);
        ui.splitter->setSizes(sizes);
    }
    else
    {
        ui.splitter->restoreState(splitter_state);
    }

    QHeaderView* header = ui.tree_computer->header();
    QByteArray columns_state = settings.columnsState();
    if (columns_state.isEmpty())
    {
        header->hideSection(ComputerItem::COLUMN_INDEX_CREATED);
        header->hideSection(ComputerItem::COLUMN_INDEX_MODIFIED);
    }
    else
    {
        header->restoreState(columns_state);
    }

    ComputerGroupItem* group_item = new ComputerGroupItem(data_.mutable_root_group(), nullptr);

    ui.tree_group->addTopLevelItem(group_item);
    ui.tree_group->setCurrentItem(group_item);

    updateComputerList(group_item);

    group_item->setExpanded(group_item->IsExpanded());

    std::function<void(ComputerGroupItem*)> restore_child = [&](ComputerGroupItem* item)
    {
        for (int i = 0; i < item->childCount(); ++i)
        {
            ComputerGroupItem* child_item = dynamic_cast<ComputerGroupItem*>(item->child(i));
            if (child_item)
            {
                if (child_item->IsExpanded())
                    child_item->setExpanded(true);

                restore_child(child_item);
            }
        }
    };

    restore_child(group_item);

    connect(ui.tree_group, &ComputerGroupTree::itemSelectionChanged, [this]()
    {
        QList<QTreeWidgetItem*> items = ui.tree_group->selectedItems();
        if (!items.isEmpty())
            onGroupItemClicked(items.front(), 0);
    });

    connect(ui.tree_group, &ComputerGroupTree::itemClicked,
            this, &AddressBookTab::onGroupItemClicked);

    connect(ui.tree_group, &ComputerGroupTree::customContextMenuRequested,
            this, &AddressBookTab::onGroupContextMenu);

    connect(ui.tree_group, &ComputerGroupTree::itemCollapsed,
            this, &AddressBookTab::onGroupItemCollapsed);

    connect(ui.tree_group, &ComputerGroupTree::itemExpanded,
            this, &AddressBookTab::onGroupItemExpanded);

    connect(ui.tree_group, &ComputerGroupTree::itemDropped,
            this, &AddressBookTab::onGroupItemDropped);

    connect(ui.tree_computer, &ComputerTree::itemClicked,
            this, &AddressBookTab::onComputerItemClicked);

    connect(ui.tree_computer, &ComputerTree::customContextMenuRequested,
            this, &AddressBookTab::onComputerContextMenu);

    connect(ui.tree_computer, &ComputerTree::itemDoubleClicked,
            this, &AddressBookTab::onComputerItemDoubleClicked);
}

AddressBookTab::~AddressBookTab()
{
    cleanupData(&data_);
    cleanupFile(&file_);

    crypto::memZero(&key_);

    Settings settings;
    settings.setSplitterState(ui.splitter->saveState());
    settings.setColumnsState(ui.tree_computer->header()->saveState());
}

// static
AddressBookTab* AddressBookTab::createNew(QWidget* parent)
{
    proto::address_book::File file;
    proto::address_book::Data data;
    QByteArray key;

    AddressBookDialog dialog(parent, QString(), &file, &data, &key);
    if (dialog.exec() != QDialog::Accepted)
        return nullptr;

    AddressBookTab* tab = new AddressBookTab(
        QString(), std::move(file), std::move(data), std::move(key), parent);

    tab->setChanged(true);
    return tab;
}

// static
AddressBookTab* AddressBookTab::openFromFile(const QString& file_path, QWidget* parent)
{
    if (file_path.isEmpty())
        return nullptr;

    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        showOpenError(parent, tr("Unable to open address book file \"%1\".").arg(file_path));
        return nullptr;
    }

    QByteArray buffer = file.readAll();
    if (buffer.isEmpty())
    {
        showOpenError(parent, tr("Unable to read address book file \"%1\".").arg(file_path));
        return nullptr;
    }

    proto::address_book::File address_book_file;

    if (!address_book_file.ParseFromArray(buffer.constData(), buffer.size()))
    {
        showOpenError(parent,
                      tr("The address book file \"%1\" is corrupted or has an unknown format.")
                      .arg(file_path));
        return nullptr;
    }

    proto::address_book::Data address_book_data;
    QByteArray key;

    switch (address_book_file.encryption_type())
    {
        case proto::address_book::ENCRYPTION_TYPE_NONE:
        {
            if (!address_book_data.ParseFromString(address_book_file.data()))
            {
                showOpenError(parent,
                              tr("The address book file \"%1\" is corrupted or has an unknown format.")
                              .arg(file_path));
                return nullptr;
            }
        }
        break;

        case proto::address_book::ENCRYPTION_TYPE_CHACHA20_POLY1305:
        {
            OpenAddressBookDialog dialog(parent, file_path, address_book_file.encryption_type());
            if (dialog.exec() != QDialog::Accepted)
                return nullptr;

            QByteArray salt = QByteArray::fromStdString(address_book_file.hashing_salt());
            key = crypto::PasswordHash::hash(
                crypto::PasswordHash::SCRYPT, dialog.password().toUtf8(), salt);

            std::unique_ptr<crypto::DataCryptor> cryptor =
                std::make_unique<crypto::DataCryptorChaCha20Poly1305>(key);

            QByteArray decrypted_data;
            if (!cryptor->decrypt(QByteArray::fromStdString(address_book_file.data()), &decrypted_data))
            {
                showOpenError(parent, tr("Unable to decrypt the address book with the specified password."));
                return nullptr;
            }

            if (!address_book_data.ParseFromArray(decrypted_data.constData(), decrypted_data.size()))
            {
                showOpenError(parent, tr("The address book file is corrupted or has an unknown format."));
                return nullptr;
            }

            crypto::memZero(&decrypted_data);
        }
        break;

        default:
        {
            showOpenError(parent, tr("The address book file is encrypted with an unsupported encryption type."));
            return nullptr;
        }
    }

    return new AddressBookTab(file_path,
                              std::move(address_book_file),
                              std::move(address_book_data),
                              std::move(key),
                              parent);
}

QString AddressBookTab::addressBookName() const
{
    return QString::fromStdString(data_.root_group().name());
}

ComputerItem* AddressBookTab::currentComputer() const
{
    return dynamic_cast<ComputerItem*>(ui.tree_computer->currentItem());
}

proto::address_book::ComputerGroup* AddressBookTab::currentComputerGroup() const
{
    ComputerGroupItem* current_item =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->currentItem());
    if (!current_item)
        return nullptr;

    return current_item->computerGroup();
}

AddressBookTab* AddressBookTab::duplicateTab() const
{
    return new AddressBookTab(filePath(),
                              proto::address_book::File(file_),
                              proto::address_book::Data(data_),
                              QByteArray(key_),
                              static_cast<QWidget*>(parent()));
}

bool AddressBookTab::save()
{
    return saveToFile(file_path_);
}

bool AddressBookTab::saveAs()
{
    return saveToFile(QString());
}

void AddressBookTab::addComputerGroup()
{
    ComputerGroupItem* parent_item =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->currentItem());
    if (!parent_item)
        return;

    std::unique_ptr<proto::address_book::ComputerGroup> computer_group =
        std::make_unique<proto::address_book::ComputerGroup>();

    ComputerGroupDialog dialog(this,
                               ComputerGroupDialog::CreateComputerGroup,
                               computer_group.get(),
                               parent_item->computerGroup());
    if (dialog.exec() != QDialog::Accepted)
        return;

    proto::address_book::ComputerGroup* computer_group_released = computer_group.release();

    ComputerGroupItem* item = parent_item->addChildComputerGroup(computer_group_released);
    ui.tree_group->setCurrentItem(item);
    setChanged(true);
}

void AddressBookTab::addComputer()
{
    ComputerGroupItem* parent_item =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->currentItem());
    if (!parent_item)
        return;

    std::unique_ptr<proto::address_book::Computer> computer =
        std::make_unique<proto::address_book::Computer>(ComputerFactory::defaultComputer());

    ComputerDialog dialog(this,
                          ComputerDialog::CreateComputer,
                          computer.get(),
                          parent_item->computerGroup());
    if (dialog.exec() != QDialog::Accepted)
        return;

    proto::address_book::Computer* computer_released = computer.release();

    parent_item->addChildComputer(computer_released);
    if (ui.tree_group->currentItem() == parent_item)
    {
        ui.tree_computer->addTopLevelItem(new ComputerItem(computer_released, parent_item));
    }

    setChanged(true);
}

void AddressBookTab::modifyAddressBook()
{
    ComputerGroupItem* root_item =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->topLevelItem(0));
    if (!root_item)
    {
        LOG(LS_WARNING) << "Invalid root item";
        return;
    }

    AddressBookDialog dialog(this, file_path_, &file_, &data_, &key_);
    if (dialog.exec() != QDialog::Accepted)
        return;

    root_item->updateItem();
    setChanged(true);
}

void AddressBookTab::modifyComputerGroup()
{
    ComputerGroupItem* current_item =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->currentItem());
    if (!current_item)
        return;

    ComputerGroupItem* parent_item = dynamic_cast<ComputerGroupItem*>(current_item->parent());
    if (!parent_item)
        return;

    ComputerGroupDialog dialog(this,
                               ComputerGroupDialog::ModifyComputerGroup,
                               current_item->computerGroup(),
                               parent_item->computerGroup());
    if (dialog.exec() != QDialog::Accepted)
        return;

    current_item->updateItem();
    setChanged(true);
}

void AddressBookTab::modifyComputer()
{
    ComputerItem* current_item = dynamic_cast<ComputerItem*>(ui.tree_computer->currentItem());
    if (!current_item)
        return;

    ComputerDialog dialog(this,
                          ComputerDialog::ModifyComputer,
                          current_item->computer(),
                          current_item->parentComputerGroupItem()->computerGroup());
    if (dialog.exec() != QDialog::Accepted)
        return;

    current_item->updateItem();
    setChanged(true);
}

void AddressBookTab::removeComputerGroup()
{
    ComputerGroupItem* current_item =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->currentItem());
    if (!current_item)
        return;

    ComputerGroupItem* parent_item = dynamic_cast<ComputerGroupItem*>(current_item->parent());
    if (!parent_item)
        return;

    QString message =
        tr("Are you sure you want to delete computer group \"%1\" and all child items?")
        .arg(QString::fromStdString(current_item->computerGroup()->name()));

    if (QMessageBox::question(this,
                              tr("Confirmation"),
                              message,
                              QMessageBox::Yes,
                              QMessageBox::No) == QMessageBox::Yes)
    {
        cleanupComputerGroup(current_item->computerGroup());

        if (parent_item->deleteChildComputerGroup(current_item))
            setChanged(true);
    }
}

void AddressBookTab::removeComputer()
{
    ComputerItem* current_item = dynamic_cast<ComputerItem*>(ui.tree_computer->currentItem());
    if (!current_item)
        return;

    QString message = tr("Are you sure you want to delete computer \"%1\"?")
        .arg(QString::fromStdString(current_item->computer()->name()));

    if (QMessageBox::question(this,
                              tr("Confirmation"),
                              message,
                              QMessageBox::Yes,
                              QMessageBox::No) == QMessageBox::Yes)
    {
        ComputerGroupItem* parent_group = current_item->parentComputerGroupItem();

        cleanupComputer(current_item->computer());

        if (parent_group->deleteChildComputer(current_item->computer()))
        {
            delete current_item;
            setChanged(true);
        }
    }
}

void AddressBookTab::onGroupItemClicked(QTreeWidgetItem* item, int /* column */)
{
    ComputerGroupItem* current_item = dynamic_cast<ComputerGroupItem*>(item);
    if (!current_item)
        return;

    bool is_root = !current_item->parent();
    emit computerGroupActivated(true, is_root);
    updateComputerList(current_item);
}

void AddressBookTab::onGroupContextMenu(const QPoint& point)
{
    ComputerGroupItem* current_item =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->itemAt(point));
    if (!current_item)
        return;

    ui.tree_group->setCurrentItem(current_item);
    onGroupItemClicked(current_item, 0);

    bool is_root = !current_item->parent();
    emit computerGroupContextMenu(ui.tree_group->viewport()->mapToGlobal(point), is_root);
}

void AddressBookTab::onGroupItemCollapsed(QTreeWidgetItem* item)
{
    ComputerGroupItem* current_item = dynamic_cast<ComputerGroupItem*>(item);
    if (!current_item)
        return;

    current_item->SetExpanded(false);
    setChanged(true);
}

void AddressBookTab::onGroupItemExpanded(QTreeWidgetItem* item)
{
    ComputerGroupItem* current_item = dynamic_cast<ComputerGroupItem*>(item);
    if (!current_item)
        return;

    current_item->SetExpanded(true);
    setChanged(true);
}

void AddressBookTab::onGroupItemDropped()
{
    ComputerGroupItem* current_item =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->currentItem());
    if (!current_item)
        return;

    updateComputerList(current_item);
    setChanged(true);
}

void AddressBookTab::onComputerItemClicked(QTreeWidgetItem* item, int /* column */)
{
    ComputerItem* current_item = dynamic_cast<ComputerItem*>(item);
    if (!current_item)
        return;

    emit computerActivated(true);
}

void AddressBookTab::onComputerContextMenu(const QPoint& point)
{
    ComputerItem* current_item = dynamic_cast<ComputerItem*>(ui.tree_computer->itemAt(point));
    if (current_item)
    {
        ui.tree_computer->setCurrentItem(current_item);
        onComputerItemClicked(current_item, 0);
    }

    emit computerContextMenu(current_item, ui.tree_computer->viewport()->mapToGlobal(point));
}

void AddressBookTab::onComputerItemDoubleClicked(QTreeWidgetItem* item, int column)
{
    ComputerItem* current_item = dynamic_cast<ComputerItem*>(item);
    if (!current_item)
        return;

    emit computerDoubleClicked(current_item->computer());
}

void AddressBookTab::showEvent(QShowEvent* event)
{
    ComputerGroupItem* current_group =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->currentItem());
    if (!current_group)
    {
        emit computerGroupActivated(false, false);
    }
    else
    {
        bool is_root = !current_group->parent();
        emit computerGroupActivated(true, is_root);
    }

    ComputerItem* current_computer = dynamic_cast<ComputerItem*>(ui.tree_computer->currentItem());

    if (!current_computer)
        emit computerActivated(false);
    else
        emit computerActivated(true);

    QWidget::showEvent(event);
}

void AddressBookTab::keyPressEvent(QKeyEvent* event)
{
    QWidget* focus_widget = QApplication::focusWidget();

    switch (event->key())
    {
        case Qt::Key_Insert:
        {
            if (focus_widget == ui.tree_group)
                addComputerGroup();
            else if (focus_widget == ui.tree_computer)
                addComputer();
        }
        break;

        case Qt::Key_F2:
        {
            if (focus_widget == ui.tree_group)
            {
                ComputerGroupItem* current_item =
                    dynamic_cast<ComputerGroupItem*>(ui.tree_group->currentItem());
                if (!current_item)
                    break;

                if (current_item->parent())
                    modifyComputerGroup();
                else
                    modifyAddressBook();
            }
            else if (focus_widget == ui.tree_computer)
            {
                modifyComputer();
            }
        }
        break;

        case Qt::Key_Delete:
        {
            if (focus_widget == ui.tree_group)
                removeComputerGroup();
            else if (focus_widget == ui.tree_computer)
                removeComputer();
        }
        break;

        case Qt::Key_Return:
        {
            if (focus_widget == ui.tree_computer)
            {
                ComputerItem* current_item =
                    dynamic_cast<ComputerItem*>(ui.tree_computer->currentItem());
                if (!current_item)
                    break;

                emit computerDoubleClicked(current_item->computer());
            }
        }
        break;

        default:
            break;
    }

    QWidget::keyPressEvent(event);
}

void AddressBookTab::setChanged(bool value)
{
    is_changed_ = value;
    emit addressBookChanged(value);
}

void AddressBookTab::retranslateUi()
{
    ui.retranslateUi(this);

    ComputerGroupItem* root_item =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->topLevelItem(0));
    if (root_item)
        root_item->updateItem();

    QTreeWidgetItem* current = ui.tree_group->currentItem();
    if (current)
        onGroupItemClicked(current, 0);
}

void AddressBookTab::updateComputerList(ComputerGroupItem* computer_group)
{
    for (int i = ui.tree_computer->topLevelItemCount() - 1; i >= 0; --i)
        std::unique_ptr<QTreeWidgetItem> item_deleter(ui.tree_computer->takeTopLevelItem(i));

    ui.tree_computer->addTopLevelItems(computer_group->ComputerList());
}

bool AddressBookTab::saveToFile(const QString& file_path)
{
    QByteArray serialized_data = common::serializeMessage(data_);

    switch (file_.encryption_type())
    {
        case proto::address_book::ENCRYPTION_TYPE_NONE:
            file_.set_data(serialized_data.toStdString());
            break;

        case proto::address_book::ENCRYPTION_TYPE_CHACHA20_POLY1305:
        {
            std::unique_ptr<crypto::DataCryptor> cryptor =
                std::make_unique<crypto::DataCryptorChaCha20Poly1305>(key_);

            QByteArray encrypted_data;
            CHECK(cryptor->encrypt(serialized_data, &encrypted_data));

            file_.set_data(encrypted_data.toStdString());
            crypto::memZero(&serialized_data);
        }
        break;

        default:
            LOG(LS_FATAL) << "Unknown encryption type: " << file_.encryption_type();
            return false;
    }

    QString path = file_path;
    if (path.isEmpty())
    {
        Settings settings;

        path = QFileDialog::getSaveFileName(this,
                                            tr("Save Address Book"),
                                            settings.lastDirectory(),
                                            tr("Aspia Address Book (*.aab)"));
        if (path.isEmpty())
            return false;

        settings.setLastDirectory(QFileInfo(path).absolutePath());
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        showSaveError(this, tr("Unable to create or open address book file."));
        return false;
    }

    QByteArray buffer = common::serializeMessage(file_);

    int64_t bytes_written = file.write(buffer);

    crypto::memZero(buffer.data(), buffer.size());

    if (bytes_written != buffer.size())
    {
        showSaveError(this, tr("Unable to write address book file."));
        return false;
    }

    file_path_ = path;

    setChanged(false);
    return true;
}

// static
void AddressBookTab::showOpenError(QWidget* parent, const QString& message)
{
    QMessageBox dialog(parent);

    dialog.setIcon(QMessageBox::Warning);
    dialog.setWindowTitle(tr("Warning"));
    dialog.setInformativeText(message);
    dialog.setText(tr("Could not open address book"));
    dialog.setStandardButtons(QMessageBox::Ok);

    dialog.exec();
}

// static
void AddressBookTab::showSaveError(QWidget* parent, const QString& message)
{
    QMessageBox dialog(parent);

    dialog.setIcon(QMessageBox::Warning);
    dialog.setWindowTitle(tr("Warning"));
    dialog.setInformativeText(message);
    dialog.setText(tr("Failed to save address book"));
    dialog.setStandardButtons(QMessageBox::Ok);

    dialog.exec();
}

} // namespace console
