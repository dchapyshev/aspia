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

#include <QAbstractButton>
#include <QListWidgetItem>
#include <QPushButton>

#include "base/logging.h"
#include "common/ui/msg_box.h"
#include "ui_router_workspace_dialog.h"

namespace {

constexpr int kMaxNameLength = 64;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterWorkspaceDialog::RouterWorkspaceDialog(
    qint64 entry_id, const QString& name, const QStringList& existing_names, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::RouterWorkspaceDialog>()),
      entry_id_(entry_id),
      name_(name),
      existing_names_(existing_names)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->edit_name->setMaxLength(kMaxNameLength);
    ui->edit_name->setText(name_);

    connect(ui->buttonbox, &QDialogButtonBox::clicked, this, &RouterWorkspaceDialog::onButtonBoxClicked);
    connect(ui->button_add, &QPushButton::clicked, this, &RouterWorkspaceDialog::onAddClicked);
    connect(ui->button_remove, &QPushButton::clicked, this, &RouterWorkspaceDialog::onRemoveClicked);
}

//--------------------------------------------------------------------------------------------------
RouterWorkspaceDialog::~RouterWorkspaceDialog()
{
    LOG(INFO) << "Dtor";
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

    LOG(INFO) << "[ACTION] Action accepted";
    accept();
    close();
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
void RouterWorkspaceDialog::setAccessUserIds(const QSet<qint64>& user_ids_with_access)
{
    access_user_ids_ = user_ids_with_access;
    rebuildLists();
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::setSelfUserId(qint64 user_id)
{
    self_user_id_ = user_id;
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

    if (entry_id_ > 0)
    {
        emit sig_grantClicked(entry_id_, user_id, it->public_key);
    }
    else
    {
        // Create mode - mutate locally; final access list is committed on accept().
        access_user_ids_.insert(user_id);
        rebuildLists();
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onRemoveClicked()
{
    QListWidgetItem* item = ui->list_with_access->currentItem();
    if (!item)
        return;

    const qint64 user_id = item->data(Qt::UserRole).toLongLong();
    if (user_id == self_user_id_)
    {
        MsgBox::warning(this, tr("You cannot revoke your own access to the workspace."));
        return;
    }

    if (entry_id_ > 0)
    {
        emit sig_revokeClicked(entry_id_, user_id);
    }
    else
    {
        access_user_ids_.remove(user_id);
        rebuildLists();
    }
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::rebuildLists()
{
    ui->list_with_access->clear();
    ui->list_available->clear();

    for (const UserEntry& user : std::as_const(users_))
    {
        QListWidgetItem* item = new QListWidgetItem(user.name);
        item->setData(Qt::UserRole, user.entry_id);

        if (access_user_ids_.contains(user.entry_id))
        {
            ui->list_with_access->addItem(item);
        }
        else if (user.public_key.isEmpty())
        {
            // Legacy users without keys cannot receive workspace access.
            delete item;
        }
        else
        {
            ui->list_available->addItem(item);
        }
    }

    ui->list_with_access->sortItems();
    ui->list_available->sortItems();
}
