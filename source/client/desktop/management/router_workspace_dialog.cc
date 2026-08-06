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

#include "client/desktop/management/router_workspace_dialog.h"

#include <QAbstractButton>
#include <QIcon>
#include <QListWidgetItem>
#include <QPushButton>
#include <QScrollBar>

#include "base/logging.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/router_error.h"
#include "proto/router_admin.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "ui_router_workspace_dialog.h"

//--------------------------------------------------------------------------------------------------
RouterWorkspaceDialog::RouterWorkspaceDialog(
    qint64 router_id, qint64 workspace_id, QWidget* parent)
    : QDialog(parent),
      ui(std::make_unique<Ui::RouterWorkspaceDialog>()),
      router_id_(router_id),
      entry_id_(workspace_id),
      model_(workspace_id)
{
    LOG(INFO) << "Ctor";
    ui->setupUi(this);

    // Counts characters, the bound counts UTF-8 bytes. A non-ASCII name is caught before sending.
    ui->edit_name->setMaxLength(static_cast<int>(proto::router::kMaxEntryNameLength));

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

    // The dialog can stay open for a long time, and the router accepts a workspace only with every
    // administrator in its access list. Without this an administrator created meanwhile from
    // another console would make the dialog unsaveable.
    connect(router, &Router::sig_usersChanged, this, [this](qint64 /* router_id */)
    {
        Router* router = Router::instance(router_id_);
        if (router)
            router->listUsers({ this, &RouterWorkspaceDialog::onUserListReceived });
    });

    // The same for the workspace itself: the save is built on top of the server snapshot (the
    // access list and the revision inside the model), so the snapshot must track concurrent
    // changes. RELOAD, because the signal arrives before the cache is refreshed.
    connect(router, &Router::sig_workspacesChanged, this, [this](qint64 /* router_id */)
    {
        Router* router = Router::instance(router_id_);
        if (router)
        {
            router->listWorkspaces( Router::CachePolicy::RELOAD, 0,
                                   { this, &RouterWorkspaceDialog::onWorkspaceListReceived });
        }
    });

    // And for the hosts: the saved set is final, so a host added to this workspace from another
    // console while the dialog is open would otherwise be released by our save - wiping its
    // encrypted fields irreversibly.
    connect(router, &Router::sig_hostsChanged, this, [this](qint64 /* router_id */)
    {
        Router* router = Router::instance(router_id_);
        if (router)
        {
            proto::router::HostListRequest host_request;
            host_request.set_mode(proto::router::HostListRequest::MODE_ALL);
            router->listHosts(Router::CachePolicy::RELOAD, std::move(host_request),
                              { this, &RouterWorkspaceDialog::onHostListReceived });
        }
    });

    // Always fetch the full list so we have all the other names available for uniqueness
    // validation; in modify mode the entry matching entry_id_ also populates the form.
    router->listWorkspaces(Router::CachePolicy::USE_CACHE, 0,
                           { this, &RouterWorkspaceDialog::onWorkspaceListReceived });
    router->listUsers({ this, &RouterWorkspaceDialog::onUserListReceived });

    proto::router::HostListRequest host_request;
    host_request.set_mode(proto::router::HostListRequest::MODE_ALL);
    router->listHosts(Router::CachePolicy::RELOAD, std::move(host_request),
                      { this, &RouterWorkspaceDialog::onHostListReceived });
}

//--------------------------------------------------------------------------------------------------
RouterWorkspaceDialog::~RouterWorkspaceDialog()
{
    LOG(INFO) << "Dtor";
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onWorkspaceListReceived(const Router::WorkspaceList& list)
{
    // A reply arriving while the dialog is already going away must not repopulate it (or stack
    // another message box) - same guard in all three list handlers and in the result handler.
    if (closing_)
        return;

    if (list.error_code != proto::router::kErrorOk)
    {
        LOG(ERROR) << "Unable to get the list of the workspaces:" << list.error_code;
        if (!model_.isLoaded() && !closing_)
        {
            // Without the first list the dialog never leaves the loading state and every control
            // stays disabled, so tell the operator and close instead of hanging silently. A failed
            // refetch keeps the lists fetched before. |closing_| collapses the errors of the three
            // list requests into a single message.
            closing_ = true;
            MsgBox::warning(this, tr("Failed to get list of workspaces."));
            reject();
        }
        return;
    }

    const bool initial_load = !model_.isLoaded();

    QList<WorkspaceEditModel::WorkspaceInfo> workspaces;
    workspaces.reserve(list.workspaces.size());
    for (const Router::Workspace& workspace : std::as_const(list.workspaces))
    {
        WorkspaceEditModel::WorkspaceInfo& info = workspaces.emplaceBack();
        info.entry_id = workspace.entry_id;
        info.revision = workspace.revision;
        info.name = workspace.name;
        info.comment = workspace.comment;
        for (const Router::Workspace::Access& access : std::as_const(workspace.access))
            info.access_ids.insert(access.user_id);
    }

    if (!model_.applyWorkspaceList(workspaces))
    {
        // The workspace being edited is gone (deleted from another console). Every further
        // action would fail with NotFound, so the dialog closes right away.
        LOG(ERROR) << "Edited workspace" << entry_id_ << "no longer exists";
        closing_ = true;
        MsgBox::warning(this, tr("The workspace was deleted from another console."));
        reject();
        return;
    }

    if (entry_id_ > 0)
    {
        if (initial_load)
        {
            ui->edit_name->setText(model_.serverName());
            ui->edit_comment->setPlainText(model_.serverComment());
        }
        else
        {
            // On a refetch the fields follow the server only while the operator has not
            // touched them: an edit in progress must not be thrown away, but an untouched
            // field must not silently revert a rename made from another console either.
            if (!ui->edit_name->isModified())
                ui->edit_name->setText(model_.serverName());
            if (!ui->edit_comment->document()->isModified())
                ui->edit_comment->setPlainText(model_.serverComment());
        }
    }

    updateLoadingState();
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onUserListReceived(const proto::router::UserList& list)
{
    if (closing_)
        return;

    if (list.error_code() != proto::router::kErrorOk)
    {
        LOG(ERROR) << "Unable to get the list of the users:" << list.error_code();
        if (!model_.isLoaded() && !closing_)
        {
            // Same as the workspace list: an unusable dialog with no explanation otherwise.
            closing_ = true;
            MsgBox::warning(this, tr("Failed to get list of users."));
            reject();
        }
        // On a refetch keep what we already have: an empty answer would drop the access
        // entries of the workspace, and the router would reject the save.
        return;
    }

    QList<WorkspaceEditModel::User> users;
    users.reserve(list.user_size());
    for (int i = 0; i < list.user_size(); ++i)
    {
        const proto::router::User& user = list.user(i);

        WorkspaceEditModel::User& entry = users.emplaceBack();
        entry.entry_id   = user.entry_id();
        entry.is_admin   = (user.sessions() & proto::router::SESSION_TYPE_ADMIN) != 0;
        entry.name       = QString::fromStdString(user.name());
        entry.public_key = QByteArray::fromStdString(user.public_key());
    }

    model_.applyUserList(users);
    updateLoadingState();
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onHostListReceived(const Router::HostList& list)
{
    if (closing_)
        return;

    if (list.error_code != proto::router::kErrorOk)
    {
        LOG(ERROR) << "Unable to get the list of the hosts:" << list.error_code;
        if (!model_.isLoaded() && !closing_)
        {
            // An empty reply must not pass for the final set of the hosts: the save would release
            // every host of the workspace and wipe their encrypted fields.
            closing_ = true;
            MsgBox::warning(this, tr("Failed to get list of hosts."));
            reject();
        }
        return;
    }

    QList<WorkspaceEditModel::HostInfo> hosts;
    hosts.reserve(list.hosts.size());
    for (const Router::Host& host : std::as_const(list.hosts))
    {
        WorkspaceEditModel::HostInfo& entry = hosts.emplaceBack();
        entry.host_id = host.host_id;
        entry.workspace_id = host.workspace_id;
        entry.computer_name = host.computer_name;
    }

    model_.applyHostList(hosts);
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

    for (const QString& existing : model_.otherNames())
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

    // The request is assembled from the model without mutating it: a failed save retries
    // against the same state.
    workspace_ = Router::Workspace();
    workspace_.entry_id = entry_id_;
    workspace_.name = new_name;
    workspace_.comment = ui->edit_comment->toPlainText();

    // The revision the edit was based on: the router rejects the save with "conflict" when the
    // workspace changed meanwhile, instead of silently overwriting the concurrent change. The
    // model pairs it with the host snapshot of the same refetch cycle (see baseRevision()).
    workspace_.revision = model_.baseRevision();

    const QList<WorkspaceEditModel::AccessEntry> entries = model_.accessEntriesForSave();
    workspace_.access.reserve(entries.size());
    for (const WorkspaceEditModel::AccessEntry& entry : entries)
    {
        Router::Workspace::Access& access = workspace_.access.emplaceBack();
        access.user_id = entry.user_id;
        access.public_key = entry.public_key;
    }

    workspace_.host_ids = model_.hostIdsForSave();

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
        router->modifyWorkspace( workspace_,
                                { this, &RouterWorkspaceDialog::onWorkspaceResultReceived });
    else
        router->addWorkspace( workspace_,
                             { this, &RouterWorkspaceDialog::onWorkspaceResultReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onWorkspaceResultReceived(const proto::router::WorkspaceResult& result)
{
    if (closing_)
        return;

    const std::string& error_code = result.error_code();
    if (error_code == proto::router::kErrorOk)
    {
        LOG(INFO) << "[ACTION] Workspace saved";
        accept();
        close();
        return;
    }

    if (error_code == proto::router::kErrorConflict)
    {
        // The workspace (or the key pair of a user in its access list) changed concurrently.
        // The snapshots are refetched; the edits of the operator are kept by construction (see
        // WorkspaceEditModel), so after a review the save can simply be repeated.
        LOG(ERROR) << "Workspace save rejected: concurrent change";
        setEnabled(true);
        refetchLists();
        MsgBox::warning(this, tr("The workspace was changed from another console. The lists are "
                                 "being refreshed - check the changes and save again."));
        return;
    }

    LOG(ERROR) << "Workspace save failed:" << error_code;
    setEnabled(true);
    MsgBox::warning(this, routerErrorText(error_code));
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onAddClicked()
{
    QListWidgetItem* item = ui->list_available->currentItem();
    if (!item)
        return;

    const qint64 user_id = item->data(Qt::UserRole).toLongLong();
    model_.grantUser(user_id);
    rebuildLists();
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onRemoveClicked()
{
    QListWidgetItem* item = ui->list_with_access->currentItem();
    if (!item)
        return;

    const qint64 user_id = item->data(Qt::UserRole).toLongLong();
    if (!model_.canRevokeUser(user_id))
    {
        MsgBox::warning(this, tr("Administrators cannot be removed from the workspace access list."));
        return;
    }

    model_.revokeUser(user_id);
    rebuildLists();
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onHostAddClicked()
{
    QListWidgetItem* item = ui->list_hosts_available->currentItem();
    if (!item)
        return;

    const quint64 host_id = item->data(Qt::UserRole).toULongLong();
    model_.claimHost(host_id);
    rebuildLists();
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::onHostRemoveClicked()
{
    QListWidgetItem* item = ui->list_hosts_in_workspace->currentItem();
    if (!item)
        return;

    const quint64 host_id = item->data(Qt::UserRole).toULongLong();

    // Only hosts that are already in the workspace on the router carry encrypted fields. A host
    // outside any workspace cannot keep them: they are sealed with the workspace group key and
    // are wiped on the router when the host leaves. Warn that this is irreversible; hosts added
    // during this session have nothing to clear, so they are removed silently.
    if (model_.isServerHost(host_id))
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

    model_.releaseHost(host_id);
    rebuildLists();
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::refetchLists()
{
    Router* router = Router::instance(router_id_);
    if (!router)
        return;

    router->listWorkspaces(Router::CachePolicy::RELOAD, 0,
                           { this, &RouterWorkspaceDialog::onWorkspaceListReceived });
    router->listUsers({ this, &RouterWorkspaceDialog::onUserListReceived });

    proto::router::HostListRequest host_request;
    host_request.set_mode(proto::router::HostListRequest::MODE_ALL);
    router->listHosts(Router::CachePolicy::RELOAD, std::move(host_request),
                      { this, &RouterWorkspaceDialog::onHostListReceived });
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::rebuildLists()
{
    // A rebuild also runs on every refetch, and while another console is active the batched
    // notifications trigger one every few seconds - so the selection and the scroll position of
    // each list are carried over instead of being reset under the hands of the operator.
    const auto captureState = [](QListWidget* list, QVariant* selected, int* scroll)
    {
        QListWidgetItem* item = list->currentItem();
        if (item)
            *selected = item->data(Qt::UserRole);
        *scroll = list->verticalScrollBar()->value();
    };

    const auto restoreState = [](QListWidget* list, const QVariant& selected, int scroll)
    {
        if (selected.isValid())
        {
            for (int i = 0; i < list->count(); ++i)
            {
                if (list->item(i)->data(Qt::UserRole) == selected)
                {
                    list->setCurrentItem(list->item(i));
                    break;
                }
            }
        }
        list->verticalScrollBar()->setValue(scroll);
    };

    QVariant selected_with_access, selected_available;
    int scroll_with_access = 0, scroll_available = 0;
    captureState(ui->list_with_access, &selected_with_access, &scroll_with_access);
    captureState(ui->list_available, &selected_available, &scroll_available);

    ui->list_with_access->clear();
    ui->list_available->clear();

    const auto add_user_item = [](QListWidget* list, const WorkspaceEditModel::User& user)
    {
        QListWidgetItem* item = new QListWidgetItem(QIcon(":/img/user.svg"), user.name);
        item->setData(Qt::UserRole, user.entry_id);
        list->addItem(item);
    };

    for (const WorkspaceEditModel::User& user : model_.memberUsers())
        add_user_item(ui->list_with_access, user);
    for (const WorkspaceEditModel::User& user : model_.availableUsers())
        add_user_item(ui->list_available, user);

    ui->list_with_access->sortItems();
    ui->list_available->sortItems();

    restoreState(ui->list_with_access, selected_with_access, scroll_with_access);
    restoreState(ui->list_available, selected_available, scroll_available);

    QVariant selected_hosts_in, selected_hosts_available;
    int scroll_hosts_in = 0, scroll_hosts_available = 0;
    captureState(ui->list_hosts_in_workspace, &selected_hosts_in, &scroll_hosts_in);
    captureState(ui->list_hosts_available, &selected_hosts_available, &scroll_hosts_available);

    ui->list_hosts_available->clear();
    ui->list_hosts_in_workspace->clear();

    const auto add_host_item = [](QListWidget* list, const WorkspaceEditModel::Host& host)
    {
        const QString name = host.computer_name.isEmpty()
            ? QString::number(host.host_id)
            : QString("%1 (%2)").arg(host.host_id).arg(host.computer_name);

        QListWidgetItem* item = new QListWidgetItem(QIcon(":/img/computer.svg"), name);
        item->setData(Qt::UserRole, host.host_id);
        list->addItem(item);
    };

    for (const WorkspaceEditModel::Host& host : model_.hostsInWorkspace())
        add_host_item(ui->list_hosts_in_workspace, host);
    for (const WorkspaceEditModel::Host& host : model_.availableHosts())
        add_host_item(ui->list_hosts_available, host);

    ui->list_hosts_available->sortItems();
    ui->list_hosts_in_workspace->sortItems();

    restoreState(ui->list_hosts_in_workspace, selected_hosts_in, scroll_hosts_in);
    restoreState(ui->list_hosts_available, selected_hosts_available, scroll_hosts_available);

    updateButtonsState();
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::updateButtonsState()
{
    ui->button_add->setEnabled(ui->list_available->currentItem() != nullptr);

    bool can_remove = false;
    if (QListWidgetItem* item = ui->list_with_access->currentItem())
        can_remove = model_.canRevokeUser(item->data(Qt::UserRole).toLongLong());
    ui->button_remove->setEnabled(can_remove);

    ui->button_host_add->setEnabled(ui->list_hosts_available->currentItem() != nullptr);
    ui->button_host_remove->setEnabled(ui->list_hosts_in_workspace->currentItem() != nullptr);
}

//--------------------------------------------------------------------------------------------------
void RouterWorkspaceDialog::updateLoadingState()
{
    const bool ready = model_.isLoaded();

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
