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

#include "client/ui/hosts/router_workspace_dialog.h"

#include <optional>

#include <QAbstractButton>
#include <QIcon>
#include <QListWidgetItem>
#include <QPushButton>

#include "base/logging.h"
#include "base/crypto/key_pair.h"
#include "base/crypto/private_key_cryptor.h"
#include "base/crypto/random.h"
#include "base/crypto/sealed_box.h"
#include "base/crypto/secure_byte_array.h"
#include "common/ui/msg_box.h"
#include "ui_router_workspace_dialog.h"

namespace {

constexpr int kMaxNameLength = 64;
constexpr int kGroupKeySize  = 32;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterWorkspaceDialog::RouterWorkspaceDialog(
    const proto::router::Workspace& current, const QStringList& existing_names, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::RouterWorkspaceDialog>()),
      entry_id_(current.entry_id()),
      name_(QString::fromStdString(current.name())),
      existing_names_(existing_names)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    for (int i = 0; i < current.access_size(); ++i)
    {
        const qint64 user_id = current.access(i).user_id();
        initial_access_.insert(user_id, QByteArray::fromStdString(current.access(i).wrapped_gk()));
        access_user_ids_.insert(user_id);
    }

    ui->edit_name->setMaxLength(kMaxNameLength);
    ui->edit_name->setText(name_);

    connect(ui->buttonbox, &QDialogButtonBox::clicked, this, &RouterWorkspaceDialog::onButtonBoxClicked);
    connect(ui->button_add, &QPushButton::clicked, this, &RouterWorkspaceDialog::onAddClicked);
    connect(ui->button_remove, &QPushButton::clicked, this, &RouterWorkspaceDialog::onRemoveClicked);
    connect(ui->list_available, &QListWidget::itemSelectionChanged,
            this, &RouterWorkspaceDialog::updateButtonsState);
    connect(ui->list_with_access, &QListWidget::itemSelectionChanged,
            this, &RouterWorkspaceDialog::updateButtonsState);

    updateButtonsState();
}

//--------------------------------------------------------------------------------------------------
RouterWorkspaceDialog::~RouterWorkspaceDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::setUsers(const QVector<UserEntry>& users)
{
    users_.clear();
    for (const UserEntry& user : users)
        users_.insert(user.entry_id, user);

    rebuildLists();
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::setSelfCredentials(const SelfCredentials& self)
{
    self_ = self;
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onButtonBoxClicked(QAbstractButton* button)
{
    QDialogButtonBox::StandardButton standard_button = ui->buttonbox->standardButton(button);
    if (standard_button != QDialogButtonBox::Ok)
    {
        LOG(INFO) << "[ACTION] Action rejected";
        reject();
        close();
        return;
    }

    const QString new_name = ui->edit_name->text().trimmed();

    if (new_name.isEmpty())
    {
        LOG(ERROR) << "Empty workspace name";
        MsgBox::warning(this, tr("Workspace name cannot be empty."));
        ui->edit_name->setFocus();
        return;
    }

    for (const QString& existing : std::as_const(existing_names_))
    {
        if (new_name.compare(existing, Qt::CaseInsensitive) == 0)
        {
            LOG(ERROR) << "Workspace name already exists:" << new_name;
            MsgBox::warning(this, tr("A workspace with the specified name already exists."));
            ui->edit_name->selectAll();
            ui->edit_name->setFocus();
            return;
        }
    }

    name_ = new_name;

    if (!buildWorkspaceProto())
        return;

    LOG(INFO) << "[ACTION] Action accepted";
    accept();
    close();
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onAddClicked()
{
    QListWidgetItem* item = ui->list_available->currentItem();
    if (!item)
        return;

    const qint64 user_id = item->data(Qt::UserRole).toLongLong();
    auto it = users_.constFind(user_id);
    if (it == users_.constEnd())
        return;

    if (it->public_key.isEmpty())
    {
        MsgBox::warning(this, tr("This user does not have encryption keys configured. "
                                 "Recreate the user or change the password to generate them."));
        return;
    }

    access_user_ids_.insert(user_id);
    rebuildLists();
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onRemoveClicked()
{
    QListWidgetItem* item = ui->list_with_access->currentItem();
    if (!item)
        return;

    const qint64 user_id = item->data(Qt::UserRole).toLongLong();
    if (user_id == self_.user_id)
    {
        MsgBox::warning(this, tr("You cannot revoke your own access to the workspace."));
        return;
    }

    access_user_ids_.remove(user_id);
    rebuildLists();
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::rebuildLists()
{
    ui->list_with_access->clear();
    ui->list_available->clear();

    for (const UserEntry& user : std::as_const(users_))
    {
        const QIcon icon(user.public_key.isEmpty() ? ":/img/user-disable.svg" : ":/img/user.svg");
        QListWidgetItem* item = new QListWidgetItem(icon, user.name);
        item->setData(Qt::UserRole, user.entry_id);

        if (access_user_ids_.contains(user.entry_id))
            ui->list_with_access->addItem(item);
        else
            ui->list_available->addItem(item);
    }

    ui->list_with_access->sortItems();
    ui->list_available->sortItems();

    updateButtonsState();
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::updateButtonsState()
{
    ui->button_add->setEnabled(ui->list_available->currentItem() != nullptr);

    QListWidgetItem* selected_with_access = ui->list_with_access->currentItem();
    const bool can_remove = selected_with_access &&
        selected_with_access->data(Qt::UserRole).toLongLong() != self_.user_id;
    ui->button_remove->setEnabled(can_remove);
}

//--------------------------------------------------------------------------------------------------
bool RouterWorkspaceDialog::buildWorkspaceProto()
{
    workspace_.Clear();
    if (entry_id_ > 0)
        workspace_.set_entry_id(entry_id_);
    workspace_.set_name(name_.toStdString());

    QSet<qint64> added;
    for (qint64 user_id : std::as_const(access_user_ids_))
    {
        if (!initial_access_.contains(user_id))
            added.insert(user_id);
    }

    QByteArray group_key;

    if (!added.isEmpty())
    {
        if (self_.wrap_private_key.isEmpty() || self_.wrap_salt.isEmpty())
        {
            LOG(ERROR) << "Self credentials missing wrap key/salt";
            MsgBox::warning(this, tr("Your account does not have encryption keys configured. "
                                     "Recreate your user or change your password to generate them."));
            return false;
        }

        const QByteArray self_wrapped_gk = initial_access_.value(self_.user_id);

        if (self_wrapped_gk.isEmpty())
        {
            // Bootstrap: workspace has no GK yet (no access records, or self is not among them).
            // Generate a fresh GK for the new set of grantees.
            group_key = Random::byteArray(kGroupKeySize);
        }
        else
        {
            SecureByteArray self_private_key = PrivateKeyCryptor::decrypt(
                self_.wrap_private_key, self_.password, self_.wrap_salt);
            if (self_private_key.isEmpty())
            {
                LOG(ERROR) << "Failed to decrypt private key";
                MsgBox::warning(this, tr("Failed to decrypt your private key."));
                return false;
            }

            KeyPair self_key_pair = KeyPair::fromPrivateKey(self_private_key);
            if (!self_key_pair.isValid())
            {
                LOG(ERROR) << "Failed to load key pair";
                MsgBox::warning(this, tr("Failed to load your key pair."));
                return false;
            }

            std::optional<QByteArray> opened = SealedBox::open(self_wrapped_gk, self_key_pair);
            if (!opened.has_value() || opened->isEmpty())
            {
                LOG(ERROR) << "Failed to unwrap workspace key";
                MsgBox::warning(this, tr("Failed to unwrap workspace key."));
                return false;
            }

            group_key = *opened;
        }
    }

    for (qint64 user_id : std::as_const(access_user_ids_))
    {
        proto::router::WorkspaceAccess* access = workspace_.add_access();
        access->set_user_id(user_id);

        if (!added.contains(user_id))
            continue;

        auto it = users_.constFind(user_id);
        if (it == users_.constEnd() || it->public_key.isEmpty())
        {
            LOG(ERROR) << "Missing public key for user_id:" << user_id;
            MsgBox::warning(this, tr("Failed to encrypt workspace key for one of the users."));
            return false;
        }

        QByteArray wrapped_gk = SealedBox::seal(group_key, it->public_key);
        if (wrapped_gk.isEmpty())
        {
            LOG(ERROR) << "Failed to seal group key for user_id:" << user_id;
            MsgBox::warning(this, tr("Failed to encrypt workspace key for one of the users."));
            return false;
        }

        access->set_wrapped_gk(wrapped_gk.toStdString());
    }

    return true;
}
