//
// Aspia Project
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

#include "console/address_book_tab.h"

#include "base/logging.h"
#include "base/serialization.h"
#include "base/crypto/data_cryptor_chacha20_poly1305.h"
#include "base/crypto/data_cryptor_fake.h"
#include "base/crypto/password_hash.h"
#include "base/crypto/secure_memory.h"
#include "client/online_checker/online_checker.h"
#include "console/address_book_dialog.h"
#include "console/computer_dialog.h"
#include "console/computer_factory.h"
#include "console/computer_group_dialog.h"
#include "console/computer_item.h"
#include "console/open_address_book_dialog.h"
#include "console/settings.h"

#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>

namespace console {

namespace {

//--------------------------------------------------------------------------------------------------
void cleanupComputer(proto::address_book::Computer* computer)
{
    if (!computer)
        return;

    base::memZero(computer->mutable_name());
    base::memZero(computer->mutable_address());
    base::memZero(computer->mutable_username());
    base::memZero(computer->mutable_password());
    base::memZero(computer->mutable_comment());
}

//--------------------------------------------------------------------------------------------------
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

        base::memZero(child_group->mutable_name());
        base::memZero(child_group->mutable_comment());

        cleanupComputerGroup(child_group);
    }
}

//--------------------------------------------------------------------------------------------------
void cleanupData(proto::address_book::Data* data)
{
    if (!data)
        return;

    cleanupComputerGroup(data->mutable_root_group());
}

//--------------------------------------------------------------------------------------------------
void cleanupFile(proto::address_book::File* file)
{
    if (!file)
        return;

    base::memZero(file->mutable_hashing_salt());
    base::memZero(file->mutable_data());
}

} // namespace

//--------------------------------------------------------------------------------------------------
AddressBookTab::AddressBookTab(const QString& file_path,
                               proto::address_book::File&& file,
                               proto::address_book::Data&& data,
                               std::string&& key,
                               QWidget* parent)
    : QWidget(parent),
      file_path_(file_path),
      key_(std::move(key)),
      file_(std::move(file)),
      data_(std::move(data))
{
    LOG(LS_INFO) << "Ctor";
    ui.setupUi(this);

    ui.tree_group->setComputerMimeType(ui.tree_computer->mimeType());

    Settings settings;
    restoreState(settings.addressBookState());

    reloadAll();

    connect(ui.tree_group, &ComputerGroupTree::itemSelectionChanged, this, [this]()
    {
        if (ui.tree_group->dragging())
            return;

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

    connect(ui.tree_group, &ComputerGroupTree::sig_itemDropped,
            this, &AddressBookTab::onGroupItemDropped);

    connect(ui.tree_computer, &ComputerTree::itemClicked,
            this, &AddressBookTab::onComputerItemClicked);

    connect(ui.tree_computer, &ComputerTree::customContextMenuRequested,
            this, &AddressBookTab::onComputerContextMenu);

    connect(ui.tree_computer, &ComputerTree::itemDoubleClicked,
            this, &AddressBookTab::onComputerItemDoubleClicked);
}

//--------------------------------------------------------------------------------------------------
AddressBookTab::~AddressBookTab()
{
    LOG(LS_INFO) << "Dtor";

    cleanupData(&data_);
    cleanupFile(&file_);

    base::memZero(&key_);

    Settings settings;
    settings.setAddressBookState(saveState());
}

//--------------------------------------------------------------------------------------------------
// static
AddressBookTab* AddressBookTab::createNew(QWidget* parent)
{
    LOG(LS_INFO) << "[ACTION] Create new";

    proto::address_book::File file;
    proto::address_book::Data data;
    std::string key;

    file.set_encryption_type(proto::address_book::ENCRYPTION_TYPE_NONE);

    AddressBookDialog dialog(parent, QString(), &file, &data, &key);
    if (dialog.exec() != QDialog::Accepted)
    {
        LOG(LS_INFO) << "[ACTION] Create new rejected by user";
        return nullptr;
    }

    LOG(LS_INFO) << "[ACTION] Create new accepted by user";

    AddressBookTab* tab = new AddressBookTab(
        QString(), std::move(file), std::move(data), std::move(key), parent);

    tab->setChanged(true);
    return tab;
}

//--------------------------------------------------------------------------------------------------
// static
AddressBookTab* AddressBookTab::openFromFile(const QString& file_path, QWidget* parent)
{
    LOG(LS_INFO) << "Open address book from file:" << file_path;

    if (file_path.isEmpty())
    {
        LOG(LS_ERROR) << "Empty file path";
        return nullptr;
    }

    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        LOG(LS_ERROR) << "Unable to open file:" << file.errorString();
        showOpenError(parent, tr("Unable to open address book file \"%1\".").arg(file_path));
        return nullptr;
    }

    QByteArray buffer = file.readAll();
    if (buffer.isEmpty())
    {
        LOG(LS_ERROR) << "Unable to read address book file";
        showOpenError(parent, tr("Unable to read address book file \"%1\".").arg(file_path));
        return nullptr;
    }

    proto::address_book::File address_book_file;

    if (!address_book_file.ParseFromArray(buffer.constData(), buffer.size()))
    {
        LOG(LS_ERROR) << "Unable to parse address book file";
        showOpenError(parent,
                      tr("The address book file \"%1\" is corrupted or has an unknown format.")
                      .arg(file_path));
        return nullptr;
    }

    proto::address_book::Data address_book_data;

    std::unique_ptr<base::DataCryptor> cryptor;
    std::string key;

    switch (address_book_file.encryption_type())
    {
        case proto::address_book::ENCRYPTION_TYPE_NONE:
            cryptor = std::make_unique<base::DataCryptorFake>();
            break;

        case proto::address_book::ENCRYPTION_TYPE_CHACHA20_POLY1305:
        {
            OpenAddressBookDialog dialog(parent, file_path, address_book_file.encryption_type());
            if (dialog.exec() != QDialog::Accepted)
                return nullptr;

            key = base::PasswordHash::hash(
                base::PasswordHash::SCRYPT,
                dialog.password(),
                address_book_file.hashing_salt());

            cryptor = std::make_unique<base::DataCryptorChaCha20Poly1305>(key);
        }
        break;

        default:
            LOG(LS_ERROR) << "Unexpected encryption type:" << address_book_file.encryption_type();
            break;
    }

    if (!cryptor)
    {
        LOG(LS_ERROR) << "Unsupported encryption type";
        showOpenError(parent, tr("The address book file is encrypted with an unsupported encryption type."));
        return nullptr;
    }

    std::string decrypted_data;
    if (!cryptor->decrypt(address_book_file.data(), &decrypted_data))
    {
        LOG(LS_ERROR) << "Unable to decrypt address book";
        showOpenError(parent, tr("Unable to decrypt the address book with the specified password."));
        return nullptr;
    }

    if (!address_book_data.ParseFromString(decrypted_data))
    {
        LOG(LS_ERROR) << "Unable to parse address book";
        showOpenError(parent, tr("The address book file is corrupted or has an unknown format."));
        return nullptr;
    }

    base::memZero(&decrypted_data);

    return new AddressBookTab(file_path,
                              std::move(address_book_file),
                              std::move(address_book_data),
                              std::move(key),
                              parent);
}

//--------------------------------------------------------------------------------------------------
QString AddressBookTab::addressBookName() const
{
    return QString::fromStdString(data_.root_group().name());
}

//--------------------------------------------------------------------------------------------------
QString AddressBookTab::addressBookGuid() const
{
    return QString::fromStdString(data_.guid());
}

//--------------------------------------------------------------------------------------------------
ComputerItem* AddressBookTab::currentComputer() const
{
    return dynamic_cast<ComputerItem*>(ui.tree_computer->currentItem());
}

//--------------------------------------------------------------------------------------------------
QString AddressBookTab::displayName() const
{
    return QString::fromStdString(data_.display_name());
}

//--------------------------------------------------------------------------------------------------
proto::address_book::ComputerGroup* AddressBookTab::currentComputerGroup() const
{
    ComputerGroupItem* current_item =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->currentItem());
    if (!current_item)
        return nullptr;

    return current_item->computerGroup();
}

//--------------------------------------------------------------------------------------------------
proto::address_book::ComputerGroup* AddressBookTab::rootComputerGroup()
{
    return data_.mutable_root_group();
}

//--------------------------------------------------------------------------------------------------
AddressBookTab* AddressBookTab::duplicateTab() const
{
    LOG(LS_INFO) << "[ACTION] Duplicate tab";
    return new AddressBookTab(filePath(),
                              proto::address_book::File(file_),
                              proto::address_book::Data(data_),
                              std::string(key_),
                              static_cast<QWidget*>(parent()));
}

//--------------------------------------------------------------------------------------------------
bool AddressBookTab::save()
{
    LOG(LS_INFO) << "[ACTION] Save";
    return saveToFile(file_path_);
}

//--------------------------------------------------------------------------------------------------
bool AddressBookTab::saveAs()
{
    LOG(LS_INFO) << "[ACTION] Save as";
    return saveToFile(QString());
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::reloadAll()
{
    LOG(LS_INFO) << "Reload address book";

    ui.tree_group->clear();
    ui.tree_computer->clear();

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

    ui.tree_group->sortItems(0, Qt::AscendingOrder);
}

//--------------------------------------------------------------------------------------------------
bool AddressBookTab::isRouterEnabled() const
{
    return data_.enable_router();
}

//--------------------------------------------------------------------------------------------------
std::optional<client::RouterConfig> AddressBookTab::routerConfig() const
{
    if (!data_.enable_router())
        return std::nullopt;

    const proto::address_book::Router& router = data_.router();
    client::RouterConfig router_config;

    router_config.address  = QString::fromStdString(router.address());
    router_config.port     = static_cast<quint16>(router.port());
    router_config.username = QString::fromStdString(router.username());
    router_config.password = QString::fromStdString(router.password());

    return std::move(router_config);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::addComputerGroup()
{
    LOG(LS_INFO) << "[ACTION] Add computer group";
    stopOnlineChecker();

    ComputerGroupItem* parent_item =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->currentItem());
    if (!parent_item)
    {
        LOG(LS_ERROR) << "Unable to get parent item";
        return;
    }

    std::unique_ptr<proto::address_book::ComputerGroup> computer_group =
        std::make_unique<proto::address_book::ComputerGroup>();
    proto::address_book::ComputerGroupConfig* group_config = computer_group->mutable_config();

    ComputerFactory::setDefaultDesktopManageConfig(
        group_config->mutable_session_config()->mutable_desktop_manage());
    ComputerFactory::setDefaultDesktopViewConfig(
        group_config->mutable_session_config()->mutable_desktop_view());

    proto::address_book::InheritConfig* inherit = group_config->mutable_inherit();
    inherit->set_credentials(true);
    inherit->set_desktop_manage(true);
    inherit->set_desktop_view(true);

    ComputerGroupDialog dialog(this,
                               ComputerGroupDialog::CreateComputerGroup,
                               parentName(parent_item),
                               computer_group.get());
    if (dialog.exec() != QDialog::Accepted)
    {
        LOG(LS_INFO) << "[ACTION] Add computer group rejected by user";
        return;
    }

    LOG(LS_INFO) << "[ACTION] Add computer group accepted by user";

    proto::address_book::ComputerGroup* computer_group_released = computer_group.release();

    ComputerGroupItem* item = parent_item->addChildComputerGroup(computer_group_released);
    ui.tree_group->setCurrentItem(item);
    ui.tree_group->sortItems(0, Qt::AscendingOrder);
    setChanged(true);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::addComputer()
{
    LOG(LS_INFO) << "[ACTION] Add computer";
    stopOnlineChecker();

    ComputerGroupItem* parent_item =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->currentItem());
    if (!parent_item)
        return;

    ComputerDialog dialog(this,
                          ComputerDialog::Mode::CREATE,
                          parentName(parent_item));
    if (dialog.exec() != QDialog::Accepted)
    {
        LOG(LS_INFO) << "[ACTION] Add computer rejected by user";
        return;
    }

    LOG(LS_INFO) << "[ACTION] Add computer accepted by user";

    proto::address_book::Computer* computer =
        new proto::address_book::Computer(dialog.computer());

    parent_item->addChildComputer(computer);
    if (ui.tree_group->currentItem() == parent_item)
    {
        ui.tree_computer->addTopLevelItem(new ComputerItem(computer, parent_item));
    }

    setChanged(true);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::copyComputer()
{
    LOG(LS_INFO) << "[ACTION] Copy computer";
    stopOnlineChecker();

    ComputerItem* current_item = dynamic_cast<ComputerItem*>(ui.tree_computer->currentItem());
    if (!current_item)
    {
        LOG(LS_ERROR) << "Unable to get current item";
        return;
    }

    ComputerGroupItem* parent_group_item = current_item->parentComputerGroupItem();
    if (!parent_group_item)
    {
        LOG(LS_ERROR) << "Unable to get parent group item";
        return;
    }

    ComputerDialog dialog(this,
                          ComputerDialog::Mode::COPY,
                          parentName(parent_group_item),
                          *current_item->computer());
    if (dialog.exec() != QDialog::Accepted)
    {
        LOG(LS_INFO) << "[ACTION] Copy computer rejected by user";
        return;
    }

    LOG(LS_INFO) << "[ACTION] Copy computer accepted by user";

    proto::address_book::Computer* computer =
        new proto::address_book::Computer(dialog.computer());

    parent_group_item->addChildComputer(computer);
    if (ui.tree_group->currentItem() == parent_group_item)
    {
        ui.tree_computer->addTopLevelItem(new ComputerItem(computer, parent_group_item));
    }

    setChanged(true);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::modifyAddressBook()
{
    LOG(LS_INFO) << "[ACTION] Modify address book";
    stopOnlineChecker();

    ComputerGroupItem* root_item = rootComputerGroupItem();
    if (!root_item)
    {
        LOG(LS_ERROR) << "Invalid root item";
        return;
    }

    AddressBookDialog dialog(this, file_path_, &file_, &data_, &key_);
    if (dialog.exec() != QDialog::Accepted)
    {
        LOG(LS_INFO) << "[ACTION] Address book not modified";
        return;
    }

    LOG(LS_INFO) << "[ACTION] Address book modified";

    root_item->updateItem();
    setChanged(true);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::modifyComputerGroup()
{
    LOG(LS_INFO) << "[ACTION] Modify computer group";
    stopOnlineChecker();

    ComputerGroupItem* current_item =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->currentItem());
    if (!current_item)
    {
        LOG(LS_ERROR) << "Unable to get current item for group";
        return;
    }

    ComputerGroupItem* parent_item = dynamic_cast<ComputerGroupItem*>(current_item->parent());
    if (!parent_item)
    {
        LOG(LS_ERROR) << "Unable to get parent item for group";
        return;
    }

    proto::address_book::ComputerGroup* computer_group = current_item->computerGroup();
    if (!computer_group->has_config())
    {
        proto::address_book::ComputerGroupConfig* group_config = computer_group->mutable_config();

        ComputerFactory::setDefaultDesktopManageConfig(
            group_config->mutable_session_config()->mutable_desktop_manage());
        ComputerFactory::setDefaultDesktopViewConfig(
            group_config->mutable_session_config()->mutable_desktop_view());

        proto::address_book::InheritConfig* inherit = group_config->mutable_inherit();
        inherit->set_credentials(true);
        inherit->set_desktop_manage(true);
        inherit->set_desktop_view(true);
    }

    ComputerGroupDialog dialog(this,
                               ComputerGroupDialog::ModifyComputerGroup,
                               parentName(parent_item),
                               computer_group);
    if (dialog.exec() != QDialog::Accepted)
    {
        LOG(LS_INFO) << "[ACTION] Computer group not modified";
        return;
    }

    LOG(LS_INFO) << "[ACTION] Computer group modified";

    current_item->updateItem();
    setChanged(true);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::modifyComputer()
{
    LOG(LS_INFO) << "[ACTION] Modify computer";
    stopOnlineChecker();

    ComputerItem* current_item = dynamic_cast<ComputerItem*>(ui.tree_computer->currentItem());
    if (!current_item)
    {
        LOG(LS_ERROR) << "Unable to get current item";
        return;
    }

    ComputerDialog dialog(this,
                          ComputerDialog::Mode::MODIFY,
                          parentName(current_item->parentComputerGroupItem()),
                          *current_item->computer());
    if (dialog.exec() != QDialog::Accepted)
    {
        LOG(LS_INFO) << "[ACTION] Computer not modified";
        return;
    }

    LOG(LS_INFO) << "[ACTION] Computer modified";

    current_item->computer()->CopyFrom(dialog.computer());
    current_item->updateItem();
    setChanged(true);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::removeComputerGroup()
{
    LOG(LS_INFO) << "[ACTION] Remove computer group";
    stopOnlineChecker();

    ComputerGroupItem* current_item =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->currentItem());
    if (!current_item)
    {
        LOG(LS_ERROR) << "Unable to get current item for group";
        return;
    }

    ComputerGroupItem* parent_item = dynamic_cast<ComputerGroupItem*>(current_item->parent());
    if (!parent_item)
    {
        LOG(LS_ERROR) << "Unable to get parent item for group";
        return;
    }

    QString message =
        tr("Are you sure you want to delete computer group \"%1\" and all child items?")
        .arg(QString::fromStdString(current_item->computerGroup()->name()));

    QMessageBox message_box(QMessageBox::Question,
                            tr("Confirmation"),
                            message,
                            QMessageBox::Yes | QMessageBox::No,
                            this);
    message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
    message_box.button(QMessageBox::No)->setText(tr("No"));

    if (message_box.exec() == QMessageBox::Yes)
    {
        LOG(LS_INFO) << "[ACTION] Computer group removing confirmed by user";
        cleanupComputerGroup(current_item->computerGroup());

        if (parent_item->deleteChildComputerGroup(current_item))
            setChanged(true);
    }
    else
    {
        LOG(LS_INFO) << "[ACTION] Computer group removing rejected by user";
    }
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::removeComputer()
{
    LOG(LS_INFO) << "[ACTION] Remove computer";
    stopOnlineChecker();

    ComputerItem* current_item = dynamic_cast<ComputerItem*>(ui.tree_computer->currentItem());
    if (!current_item)
    {
        LOG(LS_ERROR) << "Unable to get current item";
        return;
    }

    QString message = tr("Are you sure you want to delete computer \"%1\"?")
        .arg(QString::fromStdString(current_item->computer()->name()));

    QMessageBox message_box(QMessageBox::Question,
                            tr("Confirmation"),
                            message,
                            QMessageBox::Yes | QMessageBox::No,
                            this);
    message_box.button(QMessageBox::Yes)->setText(tr("Yes"));
    message_box.button(QMessageBox::No)->setText(tr("No"));

    if (message_box.exec() == QMessageBox::Yes)
    {
        LOG(LS_INFO) << "[ACTION] Computer removing confirmed by user";
        ComputerGroupItem* parent_group = current_item->parentComputerGroupItem();

        cleanupComputer(current_item->computer());

        if (parent_group->deleteChildComputer(current_item->computer()))
        {
            delete current_item;
            setChanged(true);
        }
    }
    else
    {
        LOG(LS_INFO) << "[ACTION] Computer removing rejected by user";
    }
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::startOnlineChecker()
{
    stopOnlineChecker();

    client::OnlineChecker::ComputerList computers;

    for (int i = 0; i < ui.tree_computer->topLevelItemCount(); ++i)
    {
        ComputerItem* computer_item = static_cast<ComputerItem*>(ui.tree_computer->topLevelItem(i));
        if (!computer_item)
        {
            LOG(LS_ERROR) << "Unable to get computer item:" << i;
            continue;
        }

        proto::address_book::Computer* computer = computer_item->computer();
        if (!computer)
        {
            LOG(LS_ERROR) << "Unable to get computer:" << i;
            continue;
        }

        client::OnlineChecker::Computer computer_to_check;
        computer_to_check.computer_id = computer_item->computerId();
        computer_to_check.address_or_id = QString::fromStdString(computer->address());
        computer_to_check.port = computer->port();

        computers.push_back(std::move(computer_to_check));
    }

    if (computers.empty())
    {
        LOG(LS_INFO) << "No computers to check";
        return;
    }

    emit sig_updateStateForComputers(true);

    online_checker_ = std::make_unique<client::OnlineChecker>();

    connect(online_checker_.get(), &client::OnlineChecker::sig_checkerResult,
            this, &AddressBookTab::onOnlineCheckerResult);
    connect(online_checker_.get(), &client::OnlineChecker::sig_checkerFinished,
            this, &AddressBookTab::onOnlineCheckerFinished);

    online_checker_->checkComputers(routerConfig(), computers);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::stopOnlineChecker()
{
    if (online_checker_)
    {
        LOG(LS_INFO) << "Destory online checker";
        online_checker_.reset();
    }

    for (int i = 0; i < ui.tree_computer->topLevelItemCount(); ++i)
    {
        ComputerItem* computer_item = static_cast<ComputerItem*>(ui.tree_computer->topLevelItem(i));

        computer_item->setIcon(ComputerItem::COLUMN_INDEX_NAME, QIcon(":/img/computer.png"));
        computer_item->setText(ComputerItem::COLUMN_INDEX_STATUS, QString());
    }

    emit sig_updateStateForComputers(false);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::onGroupItemClicked(QTreeWidgetItem* item, int /* column */)
{
    LOG(LS_INFO) << "[ACTION] Group item clicked";

    ComputerGroupItem* current_item = dynamic_cast<ComputerGroupItem*>(item);
    if (!current_item)
    {
        LOG(LS_ERROR) << "Unable to get current item";
        return;
    }

    bool is_root = !current_item->parent();
    emit sig_computerGroupActivated(true, is_root);
    updateComputerList(current_item);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::onGroupContextMenu(const QPoint& point)
{
    LOG(LS_INFO) << "[ACTION] Group context menu";

    ComputerGroupItem* current_item =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->itemAt(point));
    if (!current_item)
    {
        LOG(LS_ERROR) << "Unable to get current item";
        return;
    }

    ui.tree_group->setCurrentItem(current_item);
    onGroupItemClicked(current_item, 0);

    bool is_root = !current_item->parent();
    emit sig_computerGroupContextMenu(ui.tree_group->viewport()->mapToGlobal(point), is_root);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::onGroupItemCollapsed(QTreeWidgetItem* item)
{
    LOG(LS_INFO) << "[ACTION] Group item collapsed";

    ComputerGroupItem* current_item = dynamic_cast<ComputerGroupItem*>(item);
    if (!current_item)
    {
        LOG(LS_INFO) << "Unable to get current item";
        return;
    }

    current_item->SetExpanded(false);
    setChanged(true);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::onGroupItemExpanded(QTreeWidgetItem* item)
{
    LOG(LS_INFO) << "[ACTION] Group item expanded";

    ComputerGroupItem* current_item = dynamic_cast<ComputerGroupItem*>(item);
    if (!current_item)
    {
        LOG(LS_ERROR) << "Unable to get current item";
        return;
    }

    current_item->SetExpanded(true);
    setChanged(true);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::onGroupItemDropped()
{
    LOG(LS_INFO) << "[ACTION] Group item dropped";

    ComputerGroupItem* current_item =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->currentItem());
    if (!current_item)
    {
        LOG(LS_ERROR) << "Unable to get current item";
        return;
    }

    ui.tree_group->sortItems(0, Qt::AscendingOrder);
    updateComputerList(current_item);
    setChanged(true);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::onComputerItemClicked(QTreeWidgetItem* item, int /* column */)
{
    LOG(LS_INFO) << "[ACTION] Computer item clicked";

    ComputerItem* current_item = dynamic_cast<ComputerItem*>(item);
    if (!current_item)
    {
        LOG(LS_ERROR) << "Unable to get current item";
        return;
    }

    emit sig_computerActivated(true);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::onComputerContextMenu(const QPoint& point)
{
    LOG(LS_INFO) << "[ACTION] Computer context menu";

    ComputerItem* current_item = dynamic_cast<ComputerItem*>(ui.tree_computer->itemAt(point));
    if (current_item)
    {
        ui.tree_computer->setCurrentItem(current_item);
        onComputerItemClicked(current_item, 0);
    }

    emit sig_computerContextMenu(current_item, ui.tree_computer->viewport()->mapToGlobal(point));
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::onComputerItemDoubleClicked(QTreeWidgetItem* item, int /* column */)
{
    LOG(LS_INFO) << "[ACTION] Computer item double clicked";

    ComputerItem* current_item = dynamic_cast<ComputerItem*>(item);
    if (!current_item)
    {
        LOG(LS_ERROR) << "Unable to get current item";
        return;
    }

    emit sig_computerDoubleClicked(current_item->computerToConnect());
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::showEvent(QShowEvent* event)
{
    ComputerGroupItem* current_group =
        dynamic_cast<ComputerGroupItem*>(ui.tree_group->currentItem());
    if (!current_group)
    {
        emit sig_computerGroupActivated(false, false);
    }
    else
    {
        bool is_root = !current_group->parent();
        emit sig_computerGroupActivated(true, is_root);
    }

    ComputerItem* current_computer = dynamic_cast<ComputerItem*>(ui.tree_computer->currentItem());

    if (!current_computer)
        emit sig_computerActivated(false);
    else
        emit sig_computerActivated(true);

    QWidget::showEvent(event);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::keyPressEvent(QKeyEvent* event)
{
    QWidget* focus_widget = QApplication::focusWidget();

    switch (event->key())
    {
        case Qt::Key_Insert:
        {
            LOG(LS_INFO) << "[ACTION] Insert key pressed";
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
                {
                    LOG(LS_ERROR) << "Unable to get current item";
                    break;
                }

                LOG(LS_INFO) << "[ACTION] F2 key pressed";

                if (current_item->parent())
                    modifyComputerGroup();
                else
                    modifyAddressBook();
            }
            else if (focus_widget == ui.tree_computer)
            {
                LOG(LS_INFO) << "[ACTION] F2 key pressed";
                modifyComputer();
            }
        }
        break;

        case Qt::Key_Delete:
        {
            LOG(LS_INFO) << "[ACTION] Delete key pressed";
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
                LOG(LS_INFO) << "[ACTION] Enter key pressed";
                ComputerItem* current_item =
                    dynamic_cast<ComputerItem*>(ui.tree_computer->currentItem());
                if (!current_item)
                    break;

                emit sig_computerDoubleClicked(current_item->computerToConnect());
            }
        }
        break;

        default:
            break;
    }

    QWidget::keyPressEvent(event);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::onOnlineCheckerResult(int computer_id, bool online)
{
    ComputerItem* item = nullptr;

    for (int i = 0; i < ui.tree_computer->topLevelItemCount(); ++i)
    {
        item = static_cast<ComputerItem*>(ui.tree_computer->topLevelItem(i));
        if (!item)
            return;

        if (item->computerId() == computer_id)
            break;
    }

    if (!item)
    {
        LOG(LS_ERROR) << "Computer with id" << computer_id << "not found in list";
        return;
    }

    QIcon icon;
    QString status;

    if (online)
    {
        icon = QIcon(":/img/computer-online.png");
        status = tr("Online");
    }
    else
    {
        icon = QIcon(":/img/computer-offline.png");
        status = tr("Offline");
    }

    item->setIcon(ComputerItem::COLUMN_INDEX_NAME, icon);
    item->setText(ComputerItem::COLUMN_INDEX_STATUS, status);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::onOnlineCheckerFinished()
{
    LOG(LS_INFO) << "Online checked finished";

    QTimer::singleShot(0, this, [this]()
    {
        if (online_checker_)
        {
            LOG(LS_INFO) << "Destory online checked";
            online_checker_.reset();
        }

        emit sig_updateStateForComputers(false);
    });
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::setChanged(bool value)
{
    is_changed_ = value;
    emit sig_addressBookChanged(value);
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::retranslateUi()
{
    ui.retranslateUi(this);

    ComputerGroupItem* root_item = rootComputerGroupItem();
    if (root_item)
        root_item->updateItem();

    QTreeWidgetItem* current = ui.tree_group->currentItem();
    if (current)
        onGroupItemClicked(current, 0);
}

//--------------------------------------------------------------------------------------------------
QByteArray AddressBookTab::saveState()
{
    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_5_12);

        stream << ui.tree_computer->header()->saveState();
        stream << ui.splitter->saveState();
    }

    return buffer;
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::restoreState(const QByteArray& state)
{
    QDataStream stream(state);
    stream.setVersion(QDataStream::Qt_5_12);

    QByteArray columns_state;
    QByteArray splitter_state;

    stream >> columns_state;
    stream >> splitter_state;

    if (!columns_state.isEmpty())
    {
        ui.tree_computer->header()->restoreState(columns_state);
    }

    if (!splitter_state.isEmpty())
    {
        ui.splitter->restoreState(splitter_state);
    }
    else
    {
        QList<int> sizes;
        sizes.push_back(200);
        sizes.push_back(width() - 200);
        ui.splitter->setSizes(sizes);
    }
}

//--------------------------------------------------------------------------------------------------
void AddressBookTab::updateComputerList(ComputerGroupItem* computer_group)
{
    if (online_checker_)
    {
        LOG(LS_INFO) << "Destroy online checker";
        online_checker_.reset();
    }

    for (int i = ui.tree_computer->topLevelItemCount() - 1; i >= 0; --i)
        std::unique_ptr<QTreeWidgetItem> item_deleter(ui.tree_computer->takeTopLevelItem(i));

    ui.tree_computer->addTopLevelItems(computer_group->ComputerList());
}

//--------------------------------------------------------------------------------------------------
bool AddressBookTab::saveToFile(const QString& file_path)
{
    LOG(LS_INFO) << "Save address book to file:" << file_path;

    std::string serialized_data = data_.SerializeAsString();
    std::unique_ptr<base::DataCryptor> cryptor;

    switch (file_.encryption_type())
    {
        case proto::address_book::ENCRYPTION_TYPE_NONE:
            cryptor = std::make_unique<base::DataCryptorFake>();
            break;

        case proto::address_book::ENCRYPTION_TYPE_CHACHA20_POLY1305:
            cryptor = std::make_unique<base::DataCryptorChaCha20Poly1305>(key_);
            break;

        default:
            LOG(LS_FATAL) << "Unknown encryption type:" << file_.encryption_type();
            return false;
    }

    std::string encrypted_data;
    CHECK(cryptor->encrypt(serialized_data, &encrypted_data));
    base::memZero(&serialized_data);

    file_.set_data(std::move(encrypted_data));

    QString path = file_path;
    if (path.isEmpty())
    {
        LOG(LS_INFO) << "File path is empty. Show dialog to save file";
        Settings settings;

        path = QFileDialog::getSaveFileName(this,
                                            tr("Save Address Book"),
                                            settings.lastDirectory(),
                                            tr("Aspia Address Book (*.aab)"));
        if (path.isEmpty())
        {
            LOG(LS_INFO) << "File path not selected";
            return false;
        }

        LOG(LS_INFO) << "Selected file path:" << path;
        settings.setLastDirectory(QFileInfo(path).absolutePath());
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
    {
        LOG(LS_ERROR) << "Unable to open file for write";
        showSaveError(this, tr("Unable to create or open address book file."));
        return false;
    }

    QByteArray buffer = base::serialize(file_);

    qint64 bytes_written = file.write(
        reinterpret_cast<const char*>(buffer.data()), static_cast<qint64>(buffer.size()));

    base::memZero(buffer.data(), buffer.size());

    if (bytes_written != static_cast<qint64>(buffer.size()))
    {
        LOG(LS_ERROR) << "Unable to write file:" << file.errorString();
        showSaveError(this, tr("Unable to write address book file."));
        return false;
    }

    file_path_ = path;

    LOG(LS_INFO) << "Address book saved";
    setChanged(false);
    return true;
}

//--------------------------------------------------------------------------------------------------
ComputerGroupItem* AddressBookTab::rootComputerGroupItem()
{
    ComputerGroupItem* root_item = nullptr;

    for (int i = 0; i < ui.tree_group->topLevelItemCount(); ++i)
    {
        root_item = dynamic_cast<ComputerGroupItem*>(ui.tree_group->topLevelItem(i));
        if (root_item)
            break;
    }

    return root_item;
}

//--------------------------------------------------------------------------------------------------
// static
QString AddressBookTab::parentName(ComputerGroupItem* item)
{
    if (!item->parent())
        return tr("Root Group");

    return QString::fromStdString(item->computerGroup()->name());
}

//--------------------------------------------------------------------------------------------------
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

//--------------------------------------------------------------------------------------------------
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
