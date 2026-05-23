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
#include <QIcon>
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
    const Router::Workspace& current, const QStringList& existing_names, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::RouterWorkspaceDialog>()),
      entry_id_(current.entry_id),
      existing_names_(existing_names)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    for (const Router::Workspace::Access& access : std::as_const(current.access))
    {
        initial_access_user_ids_.insert(access.user_id);
        access_user_ids_.insert(access.user_id);
    }

    ui->edit_name->setMaxLength(kMaxNameLength);
    ui->edit_name->setText(current.name);
    ui->edit_comment->setPlainText(current.comment);

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
void RouterWorkspaceDialog::setUsers(const QList<UserEntry>& users)
{
    users_.clear();
    for (const UserEntry& user : users)
    {
        users_.insert(user.entry_id, user);

        // On add (entry_id_ == 0) every admin is auto-included in the new workspace's access
        // so the workspace is reachable by all admins from creation. On modify the access list
        // comes from the workspace itself; admins not already in it stay out (the constraint
        // is enforced only at creation).
        if (entry_id_ == 0 && user.is_admin)
            access_user_ids_.insert(user.entry_id);
    }

    rebuildLists();
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

    workspace_ = Router::Workspace();
    workspace_.entry_id = entry_id_;
    workspace_.name = new_name;
    workspace_.comment = ui->edit_comment->toPlainText();

    workspace_.access.reserve(access_user_ids_.size());
    for (qint64 user_id : std::as_const(access_user_ids_))
    {
        Router::Workspace::Access& access = workspace_.access.emplaceBack();
        access.user_id = user_id;

        // Newly granted users need a sealed GK from the router; pass the public key. For
        // already-granted users public_key stays empty, signalling "keep existing access".
        if (!initial_access_user_ids_.contains(user_id))
        {
            auto it = users_.constFind(user_id);
            if (it != users_.constEnd())
                access.public_key = it->public_key;
        }
    }

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
    auto it = users_.constFind(user_id);
    if (it != users_.constEnd() && it->is_admin)
    {
        MsgBox::warning(this, tr("Administrators cannot be removed from the workspace access list."));
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

    bool can_remove = false;
    if (QListWidgetItem* item = ui->list_with_access->currentItem())
    {
        const qint64 user_id = item->data(Qt::UserRole).toLongLong();
        auto it = users_.constFind(user_id);
        can_remove = (it == users_.constEnd() || !it->is_admin);
    }
    ui->button_remove->setEnabled(can_remove);
}
