//
// PROJECT:         Aspia
// FILE:            console/address_book_tab.cc
// LICENSE:         GNU General Public License 3
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "console/address_book_tab.h"

#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>

#include "base/message_serialization.h"
#include "codec/video_util.h"
#include "console/address_book_dialog.h"
#include "console/computer_dialog.h"
#include "console/computer_group_dialog.h"
#include "console/computer_item.h"
#include "console/console_settings.h"
#include "console/open_address_book_dialog.h"
#include "crypto/data_encryptor.h"

namespace aspia {

namespace {

void showOpenError(QWidget* parent, const QString& message)
{
    QMessageBox dialog(parent);

    dialog.setIcon(QMessageBox::Warning);
    dialog.setWindowTitle(QApplication::tr("Warning"));
    dialog.setInformativeText(message);
    dialog.setText(QApplication::tr("Could not open address book"));
    dialog.setStandardButtons(QMessageBox::Ok);

    dialog.exec();
}

void showSaveError(QWidget* parent, const QString& message)
{
    QMessageBox dialog(parent);

    dialog.setIcon(QMessageBox::Warning);
    dialog.setWindowTitle(QApplication::tr("Warning"));
    dialog.setInformativeText(message);
    dialog.setText(QApplication::tr("Failed to save address book"));
    dialog.setStandardButtons(QMessageBox::Ok);

    dialog.exec();
}

std::unique_ptr<proto::address_book::Computer> createDefaultComputer()
{
    std::unique_ptr<proto::address_book::Computer> computer =
        std::make_unique<proto::address_book::Computer>();

    computer->set_port(kDefaultHostTcpPort);

    proto::desktop::Config* desktop_manage =
        computer->mutable_session_config()->mutable_desktop_manage();

    desktop_manage->set_flags(proto::desktop::Config::ENABLE_CLIPBOARD |
                              proto::desktop::Config::ENABLE_CURSOR_SHAPE);
    desktop_manage->set_video_encoding(proto::desktop::VideoEncoding::VIDEO_ENCODING_ZLIB);
    desktop_manage->set_update_interval(30);
    desktop_manage->set_compress_ratio(6);
    VideoUtil::toVideoPixelFormat(PixelFormat::RGB565(), desktop_manage->mutable_pixel_format());

    proto::desktop::Config* desktop_view =
        computer->mutable_session_config()->mutable_desktop_view();

    desktop_view->set_flags(0);
    desktop_view->set_video_encoding(proto::desktop::VideoEncoding::VIDEO_ENCODING_ZLIB);
    desktop_view->set_update_interval(30);
    desktop_view->set_compress_ratio(6);
    VideoUtil::toVideoPixelFormat(PixelFormat::RGB565(), desktop_view->mutable_pixel_format());

    return computer;
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

    QList<int> sizes;
    sizes.push_back(200);
    sizes.push_back(width() - 200);

    ui.splitter->setSizes(sizes);

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

            if (child_item->IsExpanded())
                child_item->setExpanded(true);

            restore_child(child_item);
        }
    };

    restore_child(group_item);

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

AddressBookTab::~AddressBookTab() = default;

// static
AddressBookTab* AddressBookTab::createNewAddressBook(QWidget* parent)
{
    proto::address_book::File file;
    proto::address_book::Data data;
    QByteArray key;

    AddressBookDialog dialog(parent, &file, &data, &key);
    if (dialog.exec() != QDialog::Accepted)
        return nullptr;

    AddressBookTab* tab = new AddressBookTab(
        QString(), std::move(file), std::move(data), std::move(key), parent);

    tab->setChanged(true);

    return tab;
}

// static
AddressBookTab* AddressBookTab::openAddressBook(const QString& file_path, QWidget* parent)
{
    if (file_path.isEmpty())
        return nullptr;

    QFile file(file_path);
    if (!file.open(QIODevice::ReadOnly))
    {
        showOpenError(parent, tr("Unable to open address book file."));
        return nullptr;
    }

    QByteArray buffer = file.readAll();
    if (buffer.isEmpty())
    {
        showOpenError(parent, tr("Unable to read address book file."));
        return nullptr;
    }

    proto::address_book::File address_book_file;

    if (!address_book_file.ParseFromArray(buffer.constData(), buffer.size()))
    {
        showOpenError(parent, tr("The address book file is corrupted or has an unknown format."));
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
                showOpenError(parent, tr("The address book file is corrupted or has an unknown format."));
                return nullptr;
            }
        }
        break;

        case proto::address_book::ENCRYPTION_TYPE_XCHACHA20_POLY1305:
        {
            OpenAddressBookDialog dialog(parent, address_book_file.encryption_type());
            if (dialog.exec() != QDialog::Accepted)
                return nullptr;

            key = DataEncryptor::createKey(
                dialog.password().toUtf8(),
                QByteArray::fromStdString(address_book_file.hashing_salt()),
                address_book_file.hashing_rounds());

            QByteArray decrypted_data;

            if (!DataEncryptor::decrypt(address_book_file.data().c_str(),
                                        address_book_file.data().size(),
                                        key,
                                        &decrypted_data))
            {
                showOpenError(parent, tr("Unable to decrypt the address book with the specified password."));
                return nullptr;
            }

            if (!parseMessage(decrypted_data, address_book_data))
            {
                showOpenError(parent, tr("The address book file is corrupted or has an unknown format."));
                return nullptr;
            }
        }
        break;

        default:
        {
            showOpenError(parent, tr("The address book file is encrypted with an unsupported encryption type."));
            return nullptr;
        }
        break;
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

QString AddressBookTab::addressBookPath() const
{
    return file_path_;
}

proto::address_book::Computer* AddressBookTab::currentComputer() const
{
    ComputerItem* current_item = dynamic_cast<ComputerItem*>(ui.tree_computer->currentItem());
    if (!current_item)
        return nullptr;

    return current_item->computer();
}

void AddressBookTab::save()
{
    saveToFile(file_path_);
}

void AddressBookTab::saveAs()
{
    saveToFile(QString());
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

    std::unique_ptr<proto::address_book::Computer> computer = createDefaultComputer();

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
        qDebug("Invalid root item");
        return;
    }

    AddressBookDialog dialog(this, &file_, &data_, &key_);
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
    emit computerGroupContextMenu(ui.tree_group->mapToGlobal(point), is_root);
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
    if (!current_item)
        return;

    ui.tree_computer->setCurrentItem(current_item);
    onComputerItemClicked(current_item, 0);

    emit computerContextMenu(ui.tree_computer->mapToGlobal(point));
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

void AddressBookTab::setChanged(bool value)
{
    is_changed_ = value;
    emit addressBookChanged(value);
}

void AddressBookTab::updateComputerList(ComputerGroupItem* computer_group)
{
    for (int i = ui.tree_computer->topLevelItemCount() - 1; i >= 0; --i)
    {
        QTreeWidgetItem* item = ui.tree_computer->takeTopLevelItem(i);
        delete item;
    }

    ui.tree_computer->addTopLevelItems(computer_group->ComputerList());
}

bool AddressBookTab::saveToFile(const QString& file_path)
{
    QByteArray serialized_data = serializeMessage(data_);

    switch (file_.encryption_type())
    {
        case proto::address_book::ENCRYPTION_TYPE_NONE:
            file_.set_data(serialized_data.constData(), serialized_data.size());
            break;

        case proto::address_book::ENCRYPTION_TYPE_XCHACHA20_POLY1305:
        {
            QByteArray encrypted_data = DataEncryptor::encrypt(serialized_data, key_);
            file_.set_data(encrypted_data.constData(), encrypted_data.size());
        }
        break;

        default:
            qFatal("Unknown encryption type: %d", file_.encryption_type());
            return false;
    }

    QString path = file_path;

    if (path.isEmpty())
    {
        ConsoleSettings settings;

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

    QByteArray buffer = serializeMessage(file_);

    if (file.write(buffer) != buffer.size())
    {
        showSaveError(this, tr("Unable to write address book file."));
        return false;
    }

    file_path_ = path;

    setChanged(false);
    return true;
}

} // namespace aspia
