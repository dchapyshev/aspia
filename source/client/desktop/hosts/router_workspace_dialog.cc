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

#include "client/desktop/hosts/router_workspace_dialog.h"

#include <QAbstractButton>
#include <QIcon>
#include <QListWidgetItem>
#include <QPushButton>

#include "base/logging.h"
#include "common/desktop/msg_box.h"
#include "proto/router_admin.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "ui_router_workspace_dialog.h"

namespace {

constexpr int kMaxNameLength = 64;

} // namespace

//--------------------------------------------------------------------------------------------------
RouterWorkspaceDialog::RouterWorkspaceDialog(
    qint64 router_id, qint64 workspace_id, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::RouterWorkspaceDialog>()),
      router_id_(router_id),
      entry_id_(workspace_id)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    ui->edit_name->setMaxLength(kMaxNameLength);

    connect(ui->buttonbox, &QDialogButtonBox::clicked, this, &RouterWorkspaceDialog::onButtonBoxClicked);
    connect(ui->button_add, &QPushButton::clicked, this, &RouterWorkspaceDialog::onAddClicked);
    connect(ui->button_remove, &QPushButton::clicked, this, &RouterWorkspaceDialog::onRemoveClicked);
    connect(ui->button_host_add, &QPushButton::clicked, this, &RouterWorkspaceDialog::onHostAddClicked);
    connect(ui->button_host_remove, &QPushButton::clicked, this, &RouterWorkspaceDialog::onHostRemoveClicked);
    connect(ui->list_available, &QListWidget::itemSelectionChanged,
            this, &RouterWorkspaceDialog::updateButtonsState);
    connect(ui->list_with_access, &QListWidget::itemSelectionChanged,
            this, &RouterWorkspaceDialog::updateButtonsState);
    connect(ui->list_hosts_available, &QListWidget::itemSelectionChanged,
            this, &RouterWorkspaceDialog::updateButtonsState);
    connect(ui->list_hosts_in_workspace, &QListWidget::itemSelectionChanged,
            this, &RouterWorkspaceDialog::updateButtonsState);

    updateLoadingState();

    Router* router = Router::instance(router_id_);
    CHECK(router);

    connect(router, &Router::sig_statusChanged, this, [this](qint64 /* router_id */, Router::Status status)
    {
        if (status != Router::Status::ONLINE)
            reject();
    });

    // Always fetch the full list so we have all the other names available for uniqueness
    // validation; in modify mode the entry matching entry_id_ also populates the form.
    router->listWorkspaces(Router::CachePolicy::USE_CACHE, 0, this,
                           &RouterWorkspaceDialog::onWorkspaceListReceived);
    router->listUsers(this, &RouterWorkspaceDialog::onUserListReceived);

    proto::router::HostListRequest host_request;
    host_request.set_mode(proto::router::HostListRequest::MODE_ALL);
    router->listHosts(Router::CachePolicy::RELOAD, std::move(host_request), this,
                      &RouterWorkspaceDialog::onHostListReceived);
}

//--------------------------------------------------------------------------------------------------
RouterWorkspaceDialog::~RouterWorkspaceDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onWorkspaceListReceived(const Router::WorkspaceList& list)
{
    existing_names_.clear();

    for (const Router::Workspace& workspace : std::as_const(list.workspaces))
    {
        if (entry_id_ > 0 && workspace.entry_id == entry_id_)
        {
            // The workspace we are editing - populate the form. Its own name is excluded from
            // existing_names_ so the uniqueness check does not flag the unchanged name.
            ui->edit_name->setText(workspace.name);
            ui->edit_comment->setPlainText(workspace.comment);

            initial_access_user_ids_.clear();
            access_user_ids_.clear();
            for (const Router::Workspace::Access& access : std::as_const(workspace.access))
            {
                initial_access_user_ids_.insert(access.user_id);
                access_user_ids_.insert(access.user_id);
            }
        }
        else
        {
            existing_names_.append(workspace.name);
        }
    }

    workspaces_loaded_ = true;
    updateLoadingState();
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onUserListReceived(const proto::router::UserList& list)
{
    users_.clear();
    for (int i = 0; i < list.user_size(); ++i)
    {
        const proto::router::User& user = list.user(i);

        UserEntry& entry = users_[user.entry_id()];
        entry.entry_id   = user.entry_id();
        entry.is_admin   = (user.sessions() & proto::router::SESSION_TYPE_ADMIN) != 0;
        entry.name       = QString::fromStdString(user.name());
        entry.public_key = QByteArray::fromStdString(user.public_key());

        // On add (entry_id_ == 0) every admin is auto-included in the new workspace's access
        // so the workspace is reachable by all admins from creation. On modify the access list
        // comes from the workspace itself; admins not already in it stay out (the constraint
        // is enforced only at creation).
        if (entry_id_ == 0 && entry.is_admin)
            access_user_ids_.insert(entry.entry_id);
    }

    users_loaded_ = true;
    updateLoadingState();
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onHostListReceived(const Router::HostList& list)
{
    ui->list_hosts_available->clear();
    ui->list_hosts_in_workspace->clear();
    initial_host_ids_.clear();

    for (const Router::Host& host : std::as_const(list.hosts))
    {
        // Hosts of other workspaces are not visible in this dialog. Only the unassigned ones
        // (available to add) and the ones already in the workspace we are editing.
        QListWidget* target = nullptr;
        if (host.workspace_id == 0)
        {
            target = ui->list_hosts_available;
        }
        else if (entry_id_ > 0 && host.workspace_id == entry_id_)
        {
            // Remember hosts that are already in the workspace: only these carry encrypted
            // fields that get wiped when removed, so only these need the warning.
            target = ui->list_hosts_in_workspace;
            initial_host_ids_.insert(host.host_id);
        }
        else
        {
            continue;
        }

        const QString name = host.computer_name.isEmpty()
            ? QString::number(host.host_id)
            : QString("%1 (%2)").arg(host.host_id).arg(host.computer_name);

        QListWidgetItem* item = new QListWidgetItem(QIcon(":/img/computer.svg"), name);
        item->setData(Qt::UserRole, host.host_id);
        target->addItem(item);
    }

    ui->list_hosts_available->sortItems();
    ui->list_hosts_in_workspace->sortItems();

    hosts_loaded_ = true;
    updateLoadingState();
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

    workspace_.host_ids.reserve(ui->list_hosts_in_workspace->count());
    for (int i = 0; i < ui->list_hosts_in_workspace->count(); ++i)
    {
        QListWidgetItem* item = ui->list_hosts_in_workspace->item(i);
        workspace_.host_ids.append(item->data(Qt::UserRole).toULongLong());
    }

    Router* router = Router::instance(router_id_);
    if (!router)
    {
        LOG(ERROR) << "Router instance is gone";
        return;
    }

    // Disable the dialog while waiting for the server response so the operator cannot submit
    // again or close-and-resubmit; re-enabled by onWorkspaceResultReceived on error.
    setEnabled(false);

    LOG(INFO) << "[ACTION] Submitting workspace (entry_id:" << entry_id_
              << ", access entries:" << workspace_.access.size() << ")";
    if (entry_id_ > 0)
        router->modifyWorkspace(workspace_, this, &RouterWorkspaceDialog::onWorkspaceResultReceived);
    else
        router->addWorkspace(workspace_, this, &RouterWorkspaceDialog::onWorkspaceResultReceived);
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onWorkspaceResultReceived(const proto::router::WorkspaceResult& result)
{
    const std::string& error_code = result.error_code();
    if (error_code == proto::router::kErrorOk)
    {
        LOG(INFO) << "[ACTION] Workspace saved";
        accept();
        close();
        return;
    }

    const char* message;
    if (error_code == proto::router::kErrorInvalidRequest)
        message = QT_TR_NOOP("Invalid workspace request.");
    else if (error_code == proto::router::kErrorInternalError)
        message = QT_TR_NOOP("Unknown internal error.");
    else if (error_code == proto::router::kErrorInvalidData)
        message = QT_TR_NOOP("Invalid data was passed.");
    else if (error_code == proto::router::kErrorAlreadyExists)
        message = QT_TR_NOOP("A workspace with the specified name already exists.");
    else if (error_code == proto::router::kErrorNotFound)
        message = QT_TR_NOOP("Workspace not found.");
    else
        message = QT_TR_NOOP("Unknown error type.");

    LOG(ERROR) << "Workspace save failed:" << error_code;
    setEnabled(true);
    MsgBox::warning(this, tr(message));
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onAddClicked()
{
    QListWidgetItem* item = ui->list_available->currentItem();
    if (!item)
        return;

    const qint64 user_id = item->data(Qt::UserRole).toLongLong();
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
void RouterWorkspaceDialog::onHostAddClicked()
{
    QListWidgetItem* item = ui->list_hosts_available->currentItem();
    if (!item)
        return;

    ui->list_hosts_available->takeItem(ui->list_hosts_available->row(item));
    ui->list_hosts_in_workspace->addItem(item);
    ui->list_hosts_in_workspace->sortItems();

    updateButtonsState();
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onHostRemoveClicked()
{
    QListWidgetItem* item = ui->list_hosts_in_workspace->currentItem();
    if (!item)
        return;

    // Only hosts that were already in the workspace when the dialog opened carry encrypted
    // fields. A host outside any workspace cannot keep them: they are sealed with the workspace
    // group key and are wiped on the router when the host leaves. Warn that this is irreversible;
    // hosts added during this session have nothing to clear, so they are removed silently.
    if (initial_host_ids_.contains(item->data(Qt::UserRole).toULongLong()))
    {
        const QString message = tr("Removing the host from the workspace will permanently clear "
                                   "its encrypted fields (comment, user name and password). This "
                                   "action cannot be undone.\n\nAre you sure you want to continue?");
        if (MsgBox::question(this, message) == MsgBox::No)
        {
            LOG(INFO) << "Action is rejected by user";
            return;
        }
    }

    ui->list_hosts_in_workspace->takeItem(ui->list_hosts_in_workspace->row(item));
    ui->list_hosts_available->addItem(item);
    ui->list_hosts_available->sortItems();

    updateButtonsState();
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::rebuildLists()
{
    ui->list_with_access->clear();
    ui->list_available->clear();

    for (const UserEntry& user : std::as_const(users_))
    {
        QListWidgetItem* item = new QListWidgetItem(QIcon(":/img/user.svg"), user.name);
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

    ui->button_host_add->setEnabled(ui->list_hosts_available->currentItem() != nullptr);
    ui->button_host_remove->setEnabled(ui->list_hosts_in_workspace->currentItem() != nullptr);
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::updateLoadingState()
{
    const bool ready = workspaces_loaded_ && users_loaded_ && hosts_loaded_;

    ui->edit_name->setEnabled(ready);
    ui->edit_comment->setEnabled(ready);
    ui->list_available->setEnabled(ready);
    ui->list_with_access->setEnabled(ready);
    ui->list_hosts_available->setEnabled(ready);
    ui->list_hosts_in_workspace->setEnabled(ready);

    if (QPushButton* ok_button = ui->buttonbox->button(QDialogButtonBox::Ok))
        ok_button->setEnabled(ready);

    if (ready)
    {
        rebuildLists();
    }
    else
    {
        ui->button_add->setEnabled(false);
        ui->button_remove->setEnabled(false);
        ui->button_host_add->setEnabled(false);
        ui->button_host_remove->setEnabled(false);
    }
}
