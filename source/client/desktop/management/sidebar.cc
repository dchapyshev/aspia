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

#include "client/desktop/management/sidebar.h"

#include <QApplication>
#include <QDataStream>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QSet>
#include <QTimer>
#include <QTreeWidgetItemIterator>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <unordered_map>

#include "base/auto_qpointer.h"
#include "base/logging.h"
#include "base/shared_pointer.h"
#include "base/time_types.h"
#include "base/version_constants.h"
#include "base/crypto/secure_string.h"
#include "base/net/tcp_channel.h"
#include "base/peer/user.h"
#include "client/config.h"
#include "client/desktop/management/local_group_dialog.h"
#include "client/desktop/management/local_group_widget.h"
#include "client/router/router_controller.h"
#include "client/storage/instance_settings.h"
#include "client/storage/local_group_store.h"
#include "client/storage/local_host_store.h"
#include "client/storage/router_store.h"
#include "common/desktop/credentials_dialog.h"
#include "common/desktop/msg_box.h"
#include "common/desktop/router_error.h"
#include "proto/router_client.h"
#include "proto/router_constants.h"
#include "proto/router_manager.h"

namespace {

constexpr Seconds kPendingItemTimeout{ 30 };

const char kCurrentItemKey[] = "current_item";

//--------------------------------------------------------------------------------------------------
bool isRouterAdmin()
{
    RouterSession* session = RouterController::session();
    return session && session->config().sessionType() == proto::router::SESSION_TYPE_ADMIN;
}

//--------------------------------------------------------------------------------------------------
// The hosts that a drop onto the group |group_id| of the workspace |workspace_id| moves. A host
// that is already there is left out. Only an administrator moves a host to another workspace, as
// the server refuses it for anybody else.
QList<RouterHost> hostsToMove(const QList<RouterHost>& hosts, qint64 workspace_id, qint64 group_id)
{
    const bool is_admin = isRouterAdmin();

    QList<RouterHost> result;
    for (const RouterHost& host : hosts)
    {
        if (host.workspace_id == workspace_id && host.group_id == group_id)
            continue;

        if (host.workspace_id != workspace_id && !is_admin)
            continue;

        result.append(host);
    }

    return result;
}

//--------------------------------------------------------------------------------------------------
// Names an item across restarts.
QByteArray itemKey(const SidebarItem* item)
{
    qint64 entry_id = item->groupId();
    if (item->itemType() == SidebarItem::ROUTER_WORKSPACE)
        entry_id = static_cast<const SidebarRouterWorkspace*>(item)->workspaceId();

    QByteArray buffer;

    {
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream.setVersion(QDataStream::Qt_6_10);
        stream << static_cast<qint32>(item->itemType()) << entry_id;
    }

    return buffer;
}

} // namespace

//--------------------------------------------------------------------------------------------------
Sidebar::Sidebar(QWidget* parent)
    : QWidget(parent),
      pending_timer_(new QTimer(this)),
      local_group_mime_type_(QString("application/%1").arg(QUuid::createUuid().toString())),
      router_group_mime_type_(QString("application/%1").arg(QUuid::createUuid().toString()))
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    tree_widget_ = new QTreeWidget(this);
    tree_widget_->setContextMenuPolicy(Qt::CustomContextMenu);
    tree_widget_->setSelectionBehavior(QAbstractItemView::SelectItems);
    tree_widget_->header()->setVisible(false);
    tree_widget_->setColumnCount(1);

    layout->addWidget(tree_widget_);

    pending_timer_->setSingleShot(true);
    pending_timer_->setInterval(kPendingItemTimeout);

    RouterController& controller = RouterController::instance();
    connect(&controller, &RouterController::sig_statusChanged, this, &Sidebar::onRouterStatusChanged);
    connect(&controller, &RouterController::sig_workspacesChanged, this, &Sidebar::onRefreshWorkspaces);
    connect(&controller, &RouterController::sig_groupsChanged, this, &Sidebar::onRefreshHostGroups);
    connect(&controller, &RouterController::sig_localGroupsChanged, this, &Sidebar::onLocalGroupsChanged);

    const Database::ReadResult router_result = loadRouter();

    LocalGroupConfig local_root_data;
    local_root_data.setId(0);
    local_root_data.setParentId(0);
    local_root_data.setName(tr("Local"));

    // Create local root after the router so it appears at the bottom.
    local_root_ = new SidebarLocalGroup(local_root_data, tree_widget_);
    local_root_->setExpanded(InstanceSettings().isLocalGroupExpanded(local_root_data.id()));

    // Setup drag-and-drop.
    tree_widget_->setAcceptDrops(true);
    tree_widget_->viewport()->installEventFilter(this);
    tree_widget_->installEventFilter(this);

    QTreeWidgetItem* invisible_root = tree_widget_->invisibleRootItem();
    invisible_root->setFlags(invisible_root->flags() ^ Qt::ItemIsDropEnabled);

    connect(tree_widget_, &QTreeWidget::currentItemChanged, this, &Sidebar::onCurrentItemChanged);
    connect(tree_widget_, &QTreeWidget::customContextMenuRequested, this, &Sidebar::onContextMenu);
    connect(tree_widget_, &QTreeWidget::itemExpanded, this, &Sidebar::onItemExpanded);
    connect(tree_widget_, &QTreeWidget::itemCollapsed, this, &Sidebar::onItemCollapsed);

    // Load groups from database under Local root.
    const bool groups_read = loadGroups(0, local_root_);

    tree_widget_->setCurrentItem(local_root_);

    if (!groups_read || router_result == Database::ReadResult::FAILED)
        MsgBox::warning(this, tr("Failed to read data. The list may be out of date."));
}

//--------------------------------------------------------------------------------------------------
Sidebar::~Sidebar() = default;

//--------------------------------------------------------------------------------------------------
void Sidebar::setLocalHostMimeType(const QString& mime_type)
{
    local_host_mime_type_ = mime_type;
}

//--------------------------------------------------------------------------------------------------
void Sidebar::setRouterHostMimeType(const QString& mime_type)
{
    router_host_mime_type_ = mime_type;
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::loadGroups(qint64 parent_id, QTreeWidgetItem* parent_item)
{
    QList<LocalGroupConfig> groups;
    if (!LocalGroupStore().list(parent_id, &groups))
    {
        LOG(ERROR) << "Unable to read the list of groups";
        return false;
    }

    InstanceSettings settings;
    bool result = true;

    for (const LocalGroupConfig& group : std::as_const(groups))
    {
        SidebarLocalGroup* item = new SidebarLocalGroup(group, parent_item);
        item->setExpanded(settings.isLocalGroupExpanded(group.id()));

        // Load child groups recursively.
        if (!loadGroups(group.id(), item))
            result = false;
    }

    parent_item->sortChildren(0, Qt::AscendingOrder);

    return result;
}

//--------------------------------------------------------------------------------------------------
void Sidebar::reloadGroups(qint64 selected_group_id)
{
    // Remove all child items under local root.
    while (local_root_->childCount() > 0)
        delete local_root_->child(0);

    // Reload from database.
    if (!loadGroups(0, local_root_))
        MsgBox::warning(this, tr("Failed to read data. The list may be out of date."));

    local_root_->setExpanded(InstanceSettings().isLocalGroupExpanded(0));

    // Find and select the requested group.
    QTreeWidgetItem* selected = nullptr;
    if (selected_group_id != 0)
        selected = findGroupItem(selected_group_id, local_root_);

    if (!selected)
        selected = local_root_;

    // Expand parents up to the selected item.
    for (QTreeWidgetItem* p = selected->parent(); p; p = p->parent())
        p->setExpanded(true);

    selected->setExpanded(true);
    tree_widget_->setCurrentItem(selected);

    current_group_id_ = static_cast<SidebarItem*>(selected)->groupId();
    emit sig_switchContent(SidebarItem::LOCAL_GROUP);
}

//--------------------------------------------------------------------------------------------------
Database::ReadResult Sidebar::loadRouter()
{
    RouterConfig router_config;
    const Database::FindResult found = RouterStore().find(&router_config);

    Database::ReadResult result = Database::ReadResult::OK;
    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the router";
        result = Database::ReadResult::FAILED;
    }
    else if (found == Database::FindResult::UNREADABLE)
    {
        LOG(ERROR) << "Unable to read the data of the router";
        result = Database::ReadResult::INCOMPLETE;
    }

    // A router turned off is not shown, as if there were none.
    if ((found == Database::FindResult::FOUND && router_config.isEnabled()) ||
        found == Database::FindResult::UNREADABLE)
    {
        SidebarRouter* router = new SidebarRouter(tree_widget_);
        router->setExpanded(true);
    }

    RouterController::instance().reload();
    return result;
}

//--------------------------------------------------------------------------------------------------
void Sidebar::reloadRouter()
{
    RouterConfig router_config;
    const Database::FindResult found = RouterStore().find(&router_config);
    if (found == Database::FindResult::FAILED)
    {
        LOG(ERROR) << "Unable to read the router";
        MsgBox::warning(this, tr("Failed to read data. The list may be out of date."));
        return;
    }

    if (found == Database::FindResult::UNREADABLE)
        LOG(ERROR) << "Unable to read the data of the router";

    SidebarRouter* router = routerItem();

    // A router turned off is not shown, as if there were none.
    if (found == Database::FindResult::NOT_FOUND ||
        (found == Database::FindResult::FOUND && !router_config.isEnabled()))
    {
        delete router;
    }
    else if (!router)
    {
        router = new SidebarRouter(tree_widget_);
        router->setExpanded(true);

        // The router goes above the local root.
        tree_widget_->takeTopLevelItem(tree_widget_->indexOfTopLevelItem(router));
        tree_widget_->insertTopLevelItem(tree_widget_->indexOfTopLevelItem(local_root_), router);
    }

    RouterController::instance().reload();
}

//--------------------------------------------------------------------------------------------------
void Sidebar::setRouterStatus(SidebarRouter::Status status)
{
    SidebarRouter* router = routerItem();
    if (!router)
        return;

    router->setStatus(status);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::setRouterWorkspaces(const QList<RouterWorkspace>& workspaces)
{
    if (isDragActive())
    {
        router_groups_outdated_ = true;
        return;
    }

    SidebarRouter* router = routerItem();
    if (!router)
        return;

    // Incremental diff with the existing workspace items. Keeping items in place preserves
    // selection, expansion state and the host-group subtrees across refreshes.
    QSet<qint64> incoming_ids;
    incoming_ids.reserve(workspaces.size());
    for (const RouterWorkspace& workspace : workspaces)
        incoming_ids.insert(workspace.entry_id);

    // Iterate in reverse so takeChild() index shifts do not skip siblings.
    QHash<qint64, SidebarRouterWorkspace*> existing;
    for (int i = router->childCount() - 1; i >= 0; --i)
    {
        SidebarItem* child = static_cast<SidebarItem*>(router->child(i));
        if (child->itemType() != SidebarItem::ROUTER_WORKSPACE)
            continue;

        auto* workspace_item = static_cast<SidebarRouterWorkspace*>(child);
        if (!incoming_ids.contains(workspace_item->workspaceId()))
        {
            delete router->takeChild(i);
            continue;
        }
        existing.insert(workspace_item->workspaceId(), workspace_item);
    }

    InstanceSettings settings;
    for (const RouterWorkspace& workspace : workspaces)
    {
        SidebarRouterWorkspace* item = existing.value(workspace.entry_id);
        if (!item)
        {
            item = new SidebarRouterWorkspace(workspace, router);
            item->setExpanded(settings.isWorkspaceExpanded(workspace.entry_id));
        }
        else
        {
            item->update(workspace);
        }
    }

    router->sortChildren(0, Qt::AscendingOrder);
    router->setExpanded(true);
    selectPendingItem();
}

//--------------------------------------------------------------------------------------------------
void Sidebar::setRouterHostGroups(qint64 workspace_id, const QList<RouterGroup>& groups)
{
    if (isDragActive())
    {
        router_groups_outdated_ = true;
        return;
    }

    SidebarRouter* router = routerItem();
    if (!router)
        return;

    // Find the workspace item under the router by its id.
    SidebarRouterWorkspace* workspace_item = nullptr;
    for (int i = 0; i < router->childCount(); ++i)
    {
        SidebarItem* child = static_cast<SidebarItem*>(router->child(i));
        if (child->itemType() != SidebarItem::ROUTER_WORKSPACE)
            continue;
        auto* candidate = static_cast<SidebarRouterWorkspace*>(child);
        if (candidate->workspaceId() == workspace_id)
        {
            workspace_item = candidate;
            break;
        }
    }

    if (!workspace_item)
        return;

    // Incremental diff with the existing subtree under the workspace. Keeping items in place
    // preserves selection and expansion state across refreshes, and avoids visible flicker.
    //
    // Three passes:
    //   1. Index the incoming list: a flat set of entry_ids for the deletion check, and a
    //      parent_id -> children index for the DFS in pass 3.
    //   2. Walk the existing subtree once. Items whose id is missing from the incoming set are
    //      dropped (their descendants go with them via Qt's child ownership); surviving items
    //      land in `existing` and stay where they are.
    //   3. DFS the incoming tree in parent-first order. For each node create it under its
    //      target parent if missing; otherwise update name and re-parent if it moved.

    QHash<qint64, QList<const RouterGroup*>> children_of;
    QSet<qint64> incoming_ids;
    incoming_ids.reserve(groups.size());
    for (const RouterGroup& group : groups)
    {
        children_of[group.parent_id].append(&group);
        incoming_ids.insert(group.entry_id);
    }

    QHash<qint64, SidebarRouterGroup*> existing;
    std::function<void(QTreeWidgetItem*)> sweep = [&](QTreeWidgetItem* parent)
    {
        // Iterate in reverse so takeChild() index shifts do not skip siblings.
        for (int i = parent->childCount() - 1; i >= 0; --i)
        {
            SidebarItem* child = static_cast<SidebarItem*>(parent->child(i));
            if (child->itemType() != SidebarItem::ROUTER_GROUP)
                continue;
            auto* group_item = static_cast<SidebarRouterGroup*>(child);
            if (!incoming_ids.contains(group_item->groupId()))
            {
                delete parent->takeChild(i);
                continue;
            }
            existing.insert(group_item->groupId(), group_item);
            sweep(group_item);
        }
    };
    sweep(workspace_item);

    InstanceSettings settings;
    QSet<qint64> visited;
    std::function<void(qint64, QTreeWidgetItem*)> apply = [&](qint64 parent_id,
                                                              QTreeWidgetItem* tree_parent)
    {
        for (const RouterGroup* group : std::as_const(children_of[parent_id]))
        {
            // Guard against cycles and duplicate entry_ids in the untrusted group list.
            if (visited.contains(group->entry_id))
                continue;
            visited.insert(group->entry_id);

            SidebarRouterGroup* item = existing.value(group->entry_id);
            if (!item)
            {
                item = new SidebarRouterGroup(*group, tree_parent);
                item->setExpanded(settings.isRouterGroupExpanded(group->entry_id));
                existing.insert(group->entry_id, item);
            }
            else
            {
                item->update(*group);
                if (item->parent() != tree_parent)
                {
                    QTreeWidgetItem* current_parent = item->parent();
                    current_parent->takeChild(current_parent->indexOfChild(item));
                    tree_parent->addChild(item);
                }
            }
            apply(group->entry_id, item);
        }

        tree_parent->sortChildren(0, Qt::AscendingOrder);
    };
    apply(0, workspace_item);

    selectPendingItem();
}

//--------------------------------------------------------------------------------------------------
qint64 Sidebar::currentGroupId() const
{
    return current_group_id_;
}

//--------------------------------------------------------------------------------------------------
SidebarItem* Sidebar::currentItem() const
{
    return static_cast<SidebarItem*>(tree_widget_->currentItem());
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::isRouterItemCurrent() const
{
    QTreeWidgetItem* item = tree_widget_->currentItem();
    if (!item)
        return false;

    while (item->parent())
        item = item->parent();

    return static_cast<SidebarItem*>(item)->itemType() == SidebarItem::ROUTER;
}

//--------------------------------------------------------------------------------------------------
SidebarRouter* Sidebar::routerItem() const
{
    for (int i = 0; i < tree_widget_->topLevelItemCount(); ++i)
    {
        SidebarItem* item = static_cast<SidebarItem*>(tree_widget_->topLevelItem(i));
        if (item->itemType() == SidebarItem::ROUTER)
            return static_cast<SidebarRouter*>(item);
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
QList<qint64> Sidebar::routerWorkspaceIds() const
{
    QList<qint64> ids;

    SidebarRouter* router = routerItem();
    if (!router)
        return ids;

    for (int i = 0; i < router->childCount(); ++i)
    {
        SidebarItem* child = static_cast<SidebarItem*>(router->child(i));
        if (child->itemType() == SidebarItem::ROUTER_WORKSPACE)
            ids.append(static_cast<SidebarRouterWorkspace*>(child)->workspaceId());
    }

    return ids;
}

//--------------------------------------------------------------------------------------------------
void Sidebar::changeRouterPassword()
{
    if (!RouterController::session())
        return;

    AutoQPointer<CredentialsDialog> dialog(new CredentialsDialog(CredentialsDialog::Type::SET_PASSWORD, this));
    dialog->setWindowTitle(tr("Change Password"));
    dialog->setValidator([](CredentialsDialog* dialog) -> bool
    {
        SecureString password = dialog->password();

        if (!User::isValidPassword(password))
        {
            QString message = password.size() < User::kMinPasswordLength
                ? tr("The password can not be shorter than %n characters.", "", User::kMinPasswordLength)
                : tr("The password can not be longer than %n characters.", "", User::kMaxPasswordLength);

            MsgBox::warning(dialog, message);
            return false;
        }

        if (!User::isSafePassword(password))
        {
            QString unsafe = tr("Password you entered does not meet the security requirements!");
            QString safe = tr("The password must contain lowercase and uppercase characters, "
                "numbers and should not be shorter than %n characters.",
                "", User::kSafePasswordLength);
            QString question = tr("Do you want to enter a different password?");

            if (MsgBox::warning(dialog, QString("<b>%1</b><br/>%2<br/>%3").arg(unsafe, safe, question),
                MsgBox::Yes | MsgBox::No) == MsgBox::Yes)
                return false;
        }

        return true;
    });

    if (dialog->exec() != QDialog::Accepted)
        return;

    // A disconnect while the dialog is shown deletes the session, so it is looked up again.
    RouterSession* session = RouterController::session();
    if (!session)
        return;

    // On success the router revokes every device token of the account and drops all of its
    // sessions, this one included, so a reply arrives only when it refuses. The client reconnects
    // with the new password, and the user is asked for a code again on the fresh session (handled
    // by the existing two-factor plumbing).
    RouterController::instance().addEvent(RouterEvent::Severity::INFO,
        tr("Changing the password. Waiting for the session to sign in again..."));

    session->changePassword(dialog->password(), { this,
        [this](const proto::router::ChangePasswordResult& result)
    {
        // The death of the session answers this request as a lost connection, which is what a
        // password the router accepted looks like from here.
        const std::string& error_code = result.error_code();
        if (error_code != proto::router::kErrorOk && error_code != proto::router::kErrorLostConnection)
            MsgBox::warning(this, routerErrorText(error_code));
    } });
}

//--------------------------------------------------------------------------------------------------
QVariantMap Sidebar::saveState() const
{
    QVariantMap state;

    if (!pending_item_.isEmpty())
        state.insert(kCurrentItemKey, pending_item_);
    else if (SidebarItem* item = currentItem())
        state.insert(kCurrentItemKey, itemKey(item));

    return state;
}

//--------------------------------------------------------------------------------------------------
void Sidebar::restoreState(const QVariantMap& state)
{
    const QByteArray key = state.value(kCurrentItemKey).toByteArray();

    QTreeWidgetItem* item = findItem(key);
    if (item)
    {
        tree_widget_->setCurrentItem(item);
        return;
    }

    QDataStream stream(key);
    stream.setVersion(QDataStream::Qt_6_10);

    qint32 type = 0;
    stream >> type;

    SidebarRouter* router = routerItem();
    if (!router || stream.status() != QDataStream::Ok || type == SidebarItem::LOCAL_GROUP)
        return;

    // The items of a router appear once it is connected, so until then the router stands in for the
    // item. The item is selected if it appears in time and the user has not selected another one.
    // Until the user does, it is also what gets saved.
    tree_widget_->setCurrentItem(router);
    pending_item_ = key;
    pending_timer_->start();
}

//--------------------------------------------------------------------------------------------------
void Sidebar::cancelPendingItem()
{
    pending_item_.clear();
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onRefreshWorkspaces()
{
    RouterSession* session = RouterController::session();
    if (!session)
        return;

    session->listWorkspaces(RouterSession::CachePolicy::RELOAD, 0, { this,
        [this](const RouterWorkspaceList& list)
    {
        if (list.error_code != proto::router::kErrorOk)
        {
            // An error reply carries no list; applying it would wipe every workspace (and the
            // group subtrees) from the sidebar. Keep what is shown.
            LOG(ERROR) << "Unable to get the list of the workspaces:" << list.error_code;
            return;
        }

        setRouterWorkspaces(list.workspaces);

        // Once the workspace items are in place, fan out a host-group fetch per workspace.
        // Each response builds the subtree under its workspace via setRouterHostGroups().
        RouterSession* session = RouterController::session();
        if (!session)
            return;

        for (const RouterWorkspace& workspace : list.workspaces)
        {
            const qint64 workspace_id = workspace.entry_id;
            session->listGroups(RouterSession::CachePolicy::RELOAD, workspace_id, { this,
                [this, workspace_id](const RouterGroupList& result)
            {
                if (result.error_code != proto::router::kErrorOk)
                {
                    LOG(ERROR) << "Unable to get the list of the groups:" << result.error_code;
                    return;
                }
                setRouterHostGroups(workspace_id, result.groups);
            } });
        }
    } });
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onRefreshHostGroups()
{
    // The workspace items already exist in the sidebar; refetch the group tree for each one
    // without disturbing the workspaces themselves.
    RouterSession* session = RouterController::session();
    if (!session)
        return;

    const QList<qint64> workspace_ids = routerWorkspaceIds();
    for (qint64 workspace_id : std::as_const(workspace_ids))
    {
        session->listGroups(RouterSession::CachePolicy::RELOAD, workspace_id, { this,
            [this, workspace_id](const RouterGroupList& result)
        {
            if (result.error_code != proto::router::kErrorOk)
            {
                // Same as onRefreshWorkspaces: an error reply must not empty the subtree.
                LOG(ERROR) << "Unable to get the list of the groups:" << result.error_code;
                return;
            }
            setRouterHostGroups(workspace_id, result.groups);
        } });
    }
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onAddGroup()
{
    LOG(INFO) << "[ACTION] Add group";

    SidebarItem* item = currentItem();
    if (!item)
    {
        LOG(INFO) << "No current local group item";
        return;
    }

    const qint64 parent_id = item->groupId();

    AutoQPointer<LocalGroupDialog> dialog(new LocalGroupDialog(-1, parent_id, this));
    if (dialog->exec() == LocalGroupDialog::Rejected)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    reloadGroups(parent_id);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onEditGroup()
{
    LOG(INFO) << "[ACTION] Edit group";

    SidebarItem* item = currentItem();
    if (!item)
    {
        LOG(INFO) << "No current local group item";
        return;
    }

    if (item->itemType() != SidebarItem::LOCAL_GROUP)
        return;

    SidebarLocalGroup* local_group = static_cast<SidebarLocalGroup*>(item);

    const qint64 group_id = local_group->groupId();
    if (group_id == 0)
        return;

    AutoQPointer<LocalGroupDialog> dialog(new LocalGroupDialog(group_id, local_group->parentId(), this));
    if (dialog->exec() == LocalGroupDialog::Rejected)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    reloadGroups(group_id);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onRemoveGroup()
{
    LOG(INFO) << "[ACTION] Delete group";

    SidebarItem* item = currentItem();
    if (!item)
    {
        LOG(INFO) << "No current local group item";
        return;
    }

    if (item->itemType() != SidebarItem::LOCAL_GROUP || item->groupId() == 0) // Root group.
        return;

    SidebarLocalGroup* local_group = static_cast<SidebarLocalGroup*>(item);

    qint64 parent_id = local_group->parentId();
    qint64 group_id = local_group->groupId();

    QString message = tr("Are you sure you want to delete group \"%1\"?").arg(local_group->groupName());

    if (MsgBox::question(this, message) == MsgBox::No)
    {
        LOG(INFO) << "[ACTION] Rejected by user";
        return;
    }

    if (!LocalGroupStore().remove(group_id))
    {
        MsgBox::warning(this, tr("Unable to remove group"));
        LOG(INFO) << "Unable to remove group with id" << group_id;
        return;
    }

    RouterController::startSyncRecordExchange();
    InstanceSettings().removeLocalGroupExpanded(group_id);

    reloadGroups(parent_id);
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == tree_widget_)
    {
        if (event->type() == QEvent::KeyPress)
        {
            switch (static_cast<QKeyEvent*>(event)->key())
            {
                case Qt::Key_Insert:
                    emit sig_addGroup();
                    return true;

                case Qt::Key_Delete:
                {
                    SidebarItem* item = currentItem();
                    if (!item || item->itemType() != SidebarItem::ROUTER)
                        emit sig_removeGroup();
                    return true;
                }

                case Qt::Key_F2:
                {
                    SidebarItem* item = currentItem();
                    if (!item || item->itemType() != SidebarItem::ROUTER)
                        emit sig_editGroup();
                    return true;
                }

                default:
                    break;
            }
        }
    }
    else if (watched == tree_widget_->viewport())
    {
        switch (event->type())
        {
            case QEvent::MouseButtonPress:
                return onMousePress(static_cast<QMouseEvent*>(event));

            case QEvent::MouseMove:
                return onMouseMove(static_cast<QMouseEvent*>(event));

            case QEvent::DragEnter:
                return onDragEnter(static_cast<QDragEnterEvent*>(event));

            case QEvent::DragMove:
                return onDragMove(static_cast<QDragMoveEvent*>(event));

            case QEvent::DragLeave:
                return onDragLeave(static_cast<QDragLeaveEvent*>(event));

            case QEvent::Drop:
            {
                const bool handled = onDrop(static_cast<QDropEvent*>(event));
                updateOutdatedItems();
                return handled;
            }

            default:
                break;
        }
    }

    return QWidget::eventFilter(watched, event);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onCurrentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous)
{
    if (!current)
        return;

    pending_item_.clear();

    if (dragging_)
        return;

    SidebarItem* item = static_cast<SidebarItem*>(current);

    switch (item->itemType())
    {
        case SidebarItem::LOCAL_GROUP:
            current_group_id_ = item->groupId();
            break;

        case SidebarItem::ROUTER:
        case SidebarItem::ROUTER_HOSTS:
        case SidebarItem::ROUTER_USERS:
        case SidebarItem::ROUTER_CLIENTS:
        case SidebarItem::ROUTER_RELAYS:
        case SidebarItem::ROUTER_QS_HOSTS:
        case SidebarItem::ROUTER_ACCESS_LOG:
            // Nothing
            break;

        case SidebarItem::ROUTER_WORKSPACE:
        case SidebarItem::ROUTER_GROUP:
            current_group_id_ = item->groupId();
            break;
    }

    emit sig_switchContent(item->itemType());
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onContextMenu(const QPoint& pos)
{
    SidebarItem* item = static_cast<SidebarItem*>(tree_widget_->itemAt(pos));
    if (!item)
        return;

    tree_widget_->setCurrentItem(item);

    emit sig_contextMenu(item->itemType(), tree_widget_->viewport()->mapToGlobal(pos));
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onItemExpanded(QTreeWidgetItem* item)
{
    CHECK(item);

    SidebarItem* sidebar_item = static_cast<SidebarItem*>(item);
    if (sidebar_item->itemType() == SidebarItem::LOCAL_GROUP)
    {
        InstanceSettings().setLocalGroupExpanded(sidebar_item->groupId(), true);
    }
    else if (sidebar_item->itemType() == SidebarItem::ROUTER_WORKSPACE)
    {
        auto* workspace_item = static_cast<SidebarRouterWorkspace*>(sidebar_item);
        InstanceSettings().setWorkspaceExpanded(workspace_item->workspaceId(), true);
    }
    else if (sidebar_item->itemType() == SidebarItem::ROUTER_GROUP)
    {
        auto* group_item = static_cast<SidebarRouterGroup*>(sidebar_item);
        InstanceSettings().setRouterGroupExpanded(group_item->groupId(), true);
    }
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onItemCollapsed(QTreeWidgetItem* item)
{
    CHECK(item);

    SidebarItem* sidebar_item = static_cast<SidebarItem*>(item);
    if (sidebar_item->itemType() == SidebarItem::LOCAL_GROUP)
    {
        InstanceSettings().setLocalGroupExpanded(sidebar_item->groupId(), false);
    }
    else if (sidebar_item->itemType() == SidebarItem::ROUTER_WORKSPACE)
    {
        auto* workspace_item = static_cast<SidebarRouterWorkspace*>(sidebar_item);
        InstanceSettings().setWorkspaceExpanded(workspace_item->workspaceId(), false);
    }
    else if (sidebar_item->itemType() == SidebarItem::ROUTER_GROUP)
    {
        auto* group_item = static_cast<SidebarRouterGroup*>(sidebar_item);
        InstanceSettings().setRouterGroupExpanded(group_item->groupId(), false);
    }
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onRouterStatusChanged(RouterStatus status)
{
    SidebarRouter::Status sidebar_status = SidebarRouter::Status::OFFLINE;
    switch (status)
    {
        case RouterStatus::OFFLINE:    sidebar_status = SidebarRouter::Status::OFFLINE;    break;
        case RouterStatus::CONNECTING: sidebar_status = SidebarRouter::Status::CONNECTING; break;
        case RouterStatus::TWO_FACTOR: sidebar_status = SidebarRouter::Status::TWO_FACTOR; break;
        case RouterStatus::ONLINE:     sidebar_status = SidebarRouter::Status::ONLINE;     break;
        case RouterStatus::UNREADABLE: sidebar_status = SidebarRouter::Status::UNREADABLE; break;
    }
    setRouterStatus(sidebar_status);

    if (isDragActive())
    {
        router_status_outdated_ = true;
        return;
    }

    if (status == RouterStatus::ONLINE)
    {
        if (!pending_item_.isEmpty())
            pending_timer_->start();

        buildRouterSections();
        onRefreshWorkspaces();
    }
    else
    {
        SidebarRouter* router = routerItem();
        SidebarItem* item = currentItem();

        if (router && item && item != router && isRouterItemCurrent())
        {
            const QByteArray key = itemKey(item);
            tree_widget_->setCurrentItem(router);
            pending_item_ = key;
        }

        removeRouterSections();
        setRouterWorkspaces({});
    }
}

//--------------------------------------------------------------------------------------------------
void Sidebar::onLocalGroupsChanged()
{
    if (isDragActive())
    {
        local_groups_outdated_ = true;
        return;
    }

    QList<LocalGroupConfig> groups;
    if (!LocalGroupStore().all(&groups))
    {
        LOG(ERROR) << "Unable to read the list of groups";
        return;
    }

    std::unordered_map<qint64, std::vector<const LocalGroupConfig*>> groups_by_parent;
    for (const LocalGroupConfig& group : std::as_const(groups))
        groups_by_parent[group.parentId()].emplace_back(&group);

    std::unordered_map<qint64, SidebarLocalGroup*> unclaimed_items;
    for (QTreeWidgetItemIterator it(tree_widget_); *it; ++it)
    {
        SidebarItem* item = static_cast<SidebarItem*>(*it);
        if (item->itemType() == SidebarItem::LOCAL_GROUP && item != local_root_)
            unclaimed_items.emplace(item->groupId(), static_cast<SidebarLocalGroup*>(item));
    }

    SidebarItem* selected_item = currentItem();
    if (selected_item && selected_item->itemType() != SidebarItem::LOCAL_GROUP)
        selected_item = nullptr;

    {
        QSignalBlocker blocker(tree_widget_);
        InstanceSettings settings;

        std::function<void(qint64, QTreeWidgetItem*)> place_groups =
            [&](qint64 parent_id, QTreeWidgetItem* parent_item)
        {
            const auto child_groups = groups_by_parent.find(parent_id);
            if (child_groups == groups_by_parent.end())
                return;

            for (const LocalGroupConfig* group : child_groups->second)
            {
                SidebarLocalGroup* group_item = nullptr;

                const auto unclaimed_item = unclaimed_items.find(group->id());
                if (unclaimed_item == unclaimed_items.end())
                {
                    group_item = new SidebarLocalGroup(*group, parent_item);
                }
                else
                {
                    group_item = unclaimed_item->second;
                    unclaimed_items.erase(unclaimed_item);

                    group_item->update(*group);
                    if (group_item->parent() != parent_item)
                    {
                        group_item->parent()->removeChild(group_item);
                        parent_item->addChild(group_item);
                    }
                }

                group_item->setExpanded(settings.isLocalGroupExpanded(group->id()));
                place_groups(group->id(), group_item);
            }

            parent_item->sortChildren(0, Qt::AscendingOrder);
        };
        place_groups(0, local_root_);

        while (selected_item && unclaimed_items.contains(selected_item->groupId()))
            selected_item = static_cast<SidebarItem*>(selected_item->parent());

        std::vector<SidebarLocalGroup*> removed_items;
        for (const auto& [group_id, group_item] : unclaimed_items)
        {
            settings.removeLocalGroupExpanded(group_id);

            // An item is deleted together with the items below it.
            if (!unclaimed_items.contains(static_cast<SidebarItem*>(group_item->parent())->groupId()))
                removed_items.emplace_back(group_item);
        }

        for (SidebarLocalGroup* removed_item : removed_items)
            delete removed_item;

        if (selected_item)
            tree_widget_->setCurrentItem(selected_item);
    }

    if (selected_item && selected_item->groupId() != current_group_id_)
        onCurrentItemChanged(selected_item, nullptr);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::buildRouterSections()
{
    SidebarRouter* router = routerItem();
    if (!router)
        return;

    removeRouterSections();

    // Sections live above the workspace subtree. Insert each at the top in reverse display order.
    auto move_to_top = [router](QTreeWidgetItem* item)
    {
        router->insertChild(0, router->takeChild(router->indexOfChild(item)));
    };

    move_to_top(new SidebarRouterQsHosts(router));

    // Administrative sections are only meaningful for an administrator session.
    RouterSession* session = RouterController::session();
    if (session && session->config().sessionType() == proto::router::SESSION_TYPE_ADMIN)
    {
        if (session->version() >= kVersion_3_2_0)
            move_to_top(new SidebarRouterAccessLog(router));

        move_to_top(new SidebarRouterUsers(router));
        move_to_top(new SidebarRouterRelays(router));
        move_to_top(new SidebarRouterClients(router));
        move_to_top(new SidebarRouterHosts(router));
        move_to_top(new SidebarRouterTempHosts(router));
    }

    router->setExpanded(true);
    selectPendingItem();
}

//--------------------------------------------------------------------------------------------------
void Sidebar::removeRouterSections()
{
    SidebarRouter* router = routerItem();
    if (!router)
        return;

    for (int i = router->childCount() - 1; i >= 0; --i)
    {
        SidebarItem* child = static_cast<SidebarItem*>(router->child(i));
        const SidebarItem::Type type = child->itemType();
        if (type == SidebarItem::ROUTER_HOSTS || type == SidebarItem::ROUTER_USERS ||
            type == SidebarItem::ROUTER_CLIENTS || type == SidebarItem::ROUTER_RELAYS ||
            type == SidebarItem::ROUTER_TEMP_HOSTS || type == SidebarItem::ROUTER_QS_HOSTS ||
            type == SidebarItem::ROUTER_ACCESS_LOG)
        {
            delete router->takeChild(i);
        }
    }
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::onMousePress(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        start_pos_ = event->pos();

    return false;
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::onMouseMove(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton)
    {
        int distance = (event->pos() - start_pos_).manhattanLength();
        if (distance > QApplication::startDragDistance())
        {
            startDrag();
            return true;
        }
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
void Sidebar::startDrag()
{
    QTreeWidgetItem* tree_item = tree_widget_->itemAt(start_pos_);
    if (!tree_item)
        return;

    SidebarItem* item = static_cast<SidebarItem*>(tree_item);
    if (item->itemType() == SidebarItem::LOCAL_GROUP)
    {
        SidebarLocalGroup* group_item = static_cast<SidebarLocalGroup*>(item);

        // Don't allow dragging the root "Local" group.
        if (group_item->groupId() == 0)
            return;

        LocalGroupDrag drag(this);
        drag.setGroupItem(group_item, local_group_mime_type_);

        const QIcon icon = group_item->icon(0);
        drag.setPixmap(icon.pixmap(icon.actualSize(QSize(16, 16))));

        drag.exec(Qt::MoveAction);
    }
    else if (item->itemType() == SidebarItem::ROUTER_GROUP)
    {
        auto* group_item = static_cast<SidebarRouterGroup*>(item);

        // Clients are read-only and cannot move host groups.
        RouterSession* session = RouterController::session();
        if (!session || session->config().sessionType() == proto::router::SESSION_TYPE_OPERATOR)
            return;

        RouterGroupDrag drag(this);
        drag.setGroupItem(group_item, router_group_mime_type_);

        const QIcon icon = group_item->icon(0);
        drag.setPixmap(icon.pixmap(icon.actualSize(QSize(16, 16))));

        drag.exec(Qt::MoveAction);
    }

    updateOutdatedItems();
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::onDragEnter(QDragEnterEvent* event)
{
    const QMimeData* mime_data = event->mimeData();

    if (mime_data->hasFormat(local_host_mime_type_) || mime_data->hasFormat(router_host_mime_type_) ||
        mime_data->hasFormat(local_group_mime_type_) || mime_data->hasFormat(router_group_mime_type_))
    {
        event->acceptProposedAction();
        dragging_ = true;
        drag_source_item_ = tree_widget_->currentItem();
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::onDragMove(QDragMoveEvent* event)
{
    event->ignore();

    const QMimeData* mime_data = event->mimeData();

    if (mime_data->hasFormat(local_group_mime_type_))
    {
        QTreeWidgetItem* source_item = tree_widget_->itemAt(start_pos_);
        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());

        if (isAllowedDropTarget(target_tree_item, source_item))
        {
            tree_widget_->clearSelection();
            target_tree_item->setSelected(true);
            event->acceptProposedAction();
        }

        return true;
    }
    else if (mime_data->hasFormat(router_group_mime_type_))
    {
        const RouterGroupMimeData* group_mime_data = dynamic_cast<const RouterGroupMimeData*>(mime_data);
        if (!group_mime_data)
            return true;

        SidebarRouterGroup* source_group = group_mime_data->groupItem();
        if (!source_group)
            return true;

        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());
        if (!target_tree_item || target_tree_item == tree_widget_->invisibleRootItem() ||
            target_tree_item == source_group)
        {
            return true;
        }

        // The target is either a host group (nest under it) or the workspace item (move to
        // the workspace root). Cross-router/workspace moves are not supported by modifyGroup.
        SidebarItem* target_item = static_cast<SidebarItem*>(target_tree_item);
        if (target_item->itemType() == SidebarItem::ROUTER_GROUP)
        {
            auto* target_group = static_cast<SidebarRouterGroup*>(target_item);
            if (target_group->workspaceId() != source_group->workspaceId())
                return true;
        }
        else if (target_item->itemType() == SidebarItem::ROUTER_WORKSPACE)
        {
            auto* target_workspace = static_cast<SidebarRouterWorkspace*>(target_item);
            if (target_workspace->workspaceId() != source_group->workspaceId())
                return true;
        }
        else
        {
            return true;
        }

        // No-op move: target is already the source's parent.
        QTreeWidgetItem* current_parent = source_group->parent();
        if (current_parent == target_tree_item)
            return true;

        // Prevent a cycle: target must not be source itself or any of its descendants.
        std::function<bool(QTreeWidgetItem*, QTreeWidgetItem*)> isChild =
            [&](QTreeWidgetItem* parent, QTreeWidgetItem* child) -> bool
        {
            for (int i = 0; i < parent->childCount(); ++i)
            {
                QTreeWidgetItem* current = parent->child(i);
                if (current == child)
                    return true;
                if (isChild(current, child))
                    return true;
            }
            return false;
        };
        if (isChild(source_group, target_tree_item))
            return true;

        tree_widget_->clearSelection();
        target_tree_item->setSelected(true);
        event->acceptProposedAction();
        return true;
    }
    else if (mime_data->hasFormat(local_host_mime_type_))
    {
        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());
        if (!target_tree_item || target_tree_item == tree_widget_->invisibleRootItem())
            return true;

        SidebarItem* target_item = static_cast<SidebarItem*>(target_tree_item);
        if (target_item->itemType() != SidebarItem::LOCAL_GROUP)
            return true;

        const LocalHostMimeData* host_mime_data = dynamic_cast<const LocalHostMimeData*>(mime_data);
        if (!host_mime_data)
            return true;

        // Don't allow drop to the group all the hosts are already in.
        const QList<LocalHostConfig>& hosts = host_mime_data->hosts();
        const qint64 target_group_id = target_item->groupId();
        if (std::none_of(hosts.cbegin(), hosts.cend(),
                         [target_group_id](const LocalHostConfig& host)
            {
                return host.groupId() != target_group_id;
            }))
        {
            return true;
        }

        tree_widget_->clearSelection();
        target_tree_item->setSelected(true);
        event->acceptProposedAction();
        return true;
    }
    else if (mime_data->hasFormat(router_host_mime_type_))
    {
        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());
        if (!target_tree_item || target_tree_item == tree_widget_->invisibleRootItem())
            return true;

        const RouterHostMimeData* host_mime_data = dynamic_cast<const RouterHostMimeData*>(mime_data);
        if (!host_mime_data)
            return true;

        // The target is either a host group or the workspace item (move to the workspace root,
        // group id 0).
        SidebarItem* target_item = static_cast<SidebarItem*>(target_tree_item);
        qint64 target_workspace_id = 0;
        qint64 target_group_id = 0;

        if (target_item->itemType() == SidebarItem::ROUTER_GROUP)
        {
            auto* group_item = static_cast<SidebarRouterGroup*>(target_item);
            target_workspace_id = group_item->workspaceId();
            target_group_id = group_item->groupId();
        }
        else if (target_item->itemType() == SidebarItem::ROUTER_WORKSPACE)
        {
            auto* workspace_item = static_cast<SidebarRouterWorkspace*>(target_item);
            target_workspace_id = workspace_item->workspaceId();
        }
        else
        {
            return true;
        }

        if (hostsToMove(host_mime_data->hosts(), target_workspace_id, target_group_id).isEmpty())
            return true;

        tree_widget_->clearSelection();
        target_tree_item->setSelected(true);
        event->acceptProposedAction();
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::onDragLeave(QDragLeaveEvent* /* event */)
{
    dragging_ = false;
    if (drag_source_item_)
    {
        tree_widget_->setCurrentItem(drag_source_item_);
        drag_source_item_ = nullptr;
    }

    updateOutdatedItems();
    return false;
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::onDrop(QDropEvent* event)
{
    dragging_ = false;

    QTreeWidgetItem* saved_item = drag_source_item_;
    drag_source_item_ = nullptr;

    auto restoreSelection = [&]()
    {
        if (saved_item)
            tree_widget_->setCurrentItem(saved_item);
    };

    const QMimeData* mime_data = event->mimeData();
    if (!mime_data)
        return false;

    if (mime_data->hasFormat(local_group_mime_type_))
    {
        const LocalGroupMimeData* group_mime_data = dynamic_cast<const LocalGroupMimeData*>(mime_data);
        if (!group_mime_data)
        {
            restoreSelection();
            return true;
        }

        SidebarLocalGroup* source_group = group_mime_data->groupItem();
        if (!source_group)
        {
            restoreSelection();
            return true;
        }

        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());
        if (!isAllowedDropTarget(target_tree_item, source_group))
        {
            restoreSelection();
            return true;
        }

        SidebarItem* target_item = static_cast<SidebarItem*>(target_tree_item);

        LocalGroupStore store;

        // Check if a group with the same name already exists in the target group.
        QList<LocalGroupConfig> target_groups;
        if (!store.list(target_item->groupId(), &target_groups))
        {
            LOG(ERROR) << "Unable to read the groups of the target group";
            MsgBox::warning(tree_widget_, tr("Failed to move the group."));
            restoreSelection();
            return true;
        }

        for (const LocalGroupConfig& existing : std::as_const(target_groups))
        {
            if (existing.id() != source_group->groupId() && existing.name() == source_group->groupName())
            {
                MsgBox::warning(tree_widget_,
                    tr("A group with this name already exists in the selected parent group."));
                restoreSelection();
                return true;
            }
        }

        // Update the group's parent in the database.
        if (!store.move(source_group->groupId(), target_item->groupId()))
        {
            MsgBox::warning(tree_widget_, tr("Failed to move the group."));
            restoreSelection();
            return true;
        }

        RouterController::startSyncRecordExchange();
        event->acceptProposedAction();

        // Reload groups and select the moved group.
        reloadGroups(source_group->groupId());
        return true;
    }
    else if (mime_data->hasFormat(local_host_mime_type_))
    {
        const LocalHostMimeData* host_mime_data = dynamic_cast<const LocalHostMimeData*>(mime_data);
        if (!host_mime_data)
        {
            restoreSelection();
            return true;
        }

        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());
        if (!target_tree_item || target_tree_item == tree_widget_->invisibleRootItem())
        {
            restoreSelection();
            return true;
        }

        SidebarItem* target_item = static_cast<SidebarItem*>(target_tree_item);
        if (target_item->itemType() != SidebarItem::LOCAL_GROUP)
        {
            restoreSelection();
            return true;
        }

        // Repeat the check from onDragMove in case the user releases over a target that wasn't
        // validated. The hosts already in the target group stay where they are.
        const qint64 target_group_id = target_item->groupId();

        QList<LocalHostConfig> hosts;
        for (const LocalHostConfig& host : host_mime_data->hosts())
        {
            if (host.groupId() != target_group_id)
                hosts.append(host);
        }

        if (hosts.isEmpty())
        {
            restoreSelection();
            return true;
        }

        const bool single = hosts.size() == 1;

        LocalHostStore store;

        // Check if hosts with the same names already exist in the target group.
        QList<LocalHostConfig> target_hosts;
        if (store.list(target_group_id, &target_hosts) == Database::ReadResult::FAILED)
        {
            LOG(ERROR) << "Unable to read the hosts of the target group";
            restoreSelection();
            MsgBox::warning(tree_widget_, single ?
                tr("Failed to move the host to the selected group.") :
                tr("Failed to move the hosts to the selected group."));
            return true;
        }

        QSet<QString> target_names;
        for (const LocalHostConfig& existing : std::as_const(target_hosts))
            target_names.insert(existing.name());

        int name_conflicts = 0;
        int failed = 0;
        int moved = 0;

        for (const LocalHostConfig& host : std::as_const(hosts))
        {
            if (target_names.contains(host.name()))
            {
                LOG(INFO) << "Host" << host.id() << "has a namesake in the target group";
                ++name_conflicts;
                continue;
            }

            if (!store.move(host.id(), target_group_id))
            {
                ++failed;
                continue;
            }

            target_names.insert(host.name());
            ++moved;
        }

        QStringList errors;

        if (name_conflicts > 0)
        {
            errors.append(single ?
                tr("A host with this name already exists in the selected group.") :
                tr("%n of the selected hosts were not moved because hosts with the same names already "
                   "exist in the selected group.", "", name_conflicts));
        }

        if (failed > 0)
        {
            errors.append(single ?
                tr("Failed to move the host to the selected group.") :
                tr("Failed to move %n of the selected hosts.", "", failed));
        }

        restoreSelection();

        if (!errors.isEmpty())
            MsgBox::warning(tree_widget_, errors.join('\n'));

        if (moved > 0)
            event->acceptProposedAction();

        if (moved > 0)
        {
            RouterController::startSyncRecordExchange();
            emit sig_itemDropped();
        }

        return true;
    }
    else if (mime_data->hasFormat(router_group_mime_type_))
    {
        const RouterGroupMimeData* group_mime_data = dynamic_cast<const RouterGroupMimeData*>(mime_data);
        if (!group_mime_data)
        {
            restoreSelection();
            return true;
        }

        SidebarRouterGroup* source_group = group_mime_data->groupItem();
        if (!source_group)
        {
            restoreSelection();
            return true;
        }

        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());
        if (!target_tree_item || target_tree_item == tree_widget_->invisibleRootItem() ||
            target_tree_item == source_group)
        {
            restoreSelection();
            return true;
        }

        // The target is either a host group (drop = nest under it, parent_id is its entry_id)
        // or the workspace item (drop = move to the workspace root, parent_id 0).
        SidebarItem* target_item = static_cast<SidebarItem*>(target_tree_item);
        qint64 target_workspace_id = 0;
        qint64 target_group_id = 0;

        if (target_item->itemType() == SidebarItem::ROUTER_GROUP)
        {
            auto* target_group = static_cast<SidebarRouterGroup*>(target_item);
            target_workspace_id = target_group->workspaceId();
            target_group_id = target_group->groupId();
        }
        else if (target_item->itemType() == SidebarItem::ROUTER_WORKSPACE)
        {
            auto* target_workspace = static_cast<SidebarRouterWorkspace*>(target_item);
            target_workspace_id = target_workspace->workspaceId();
        }
        else
        {
            restoreSelection();
            return true;
        }

        if (target_workspace_id != source_group->workspaceId())
        {
            restoreSelection();
            return true;
        }

        if (source_group->parent() == target_tree_item)
        {
            restoreSelection();
            return true;
        }

        RouterSession* session = RouterController::session();
        if (!session)
        {
            restoreSelection();
            return true;
        }

        // Copy the existing record and just rewrite parent_id.
        RouterGroup group = source_group->group();
        group.parent_id = target_group_id;

        session->modifyGroup(source_group->workspaceId(), group, { this,
            [this](const proto::router::GroupResult& result)
        {
            if (result.error_code() != proto::router::kErrorOk)
            {
                LOG(ERROR) << "Move group failed:" << result.error_code();
                MsgBox::warning(tree_widget_, tr("Failed to move the group."));
                return;
            }
            emit sig_routerGroupMoved();
        } });

        event->acceptProposedAction();
        restoreSelection();
        return true;
    }
    else if (mime_data->hasFormat(router_host_mime_type_))
    {
        const RouterHostMimeData* host_mime_data = dynamic_cast<const RouterHostMimeData*>(mime_data);
        if (!host_mime_data)
        {
            restoreSelection();
            return true;
        }

        QTreeWidgetItem* target_tree_item = tree_widget_->itemAt(event->position().toPoint());
        if (!target_tree_item || target_tree_item == tree_widget_->invisibleRootItem())
        {
            restoreSelection();
            return true;
        }

        // The target is either a host group or the workspace item (move to the workspace root).
        SidebarItem* target_item = static_cast<SidebarItem*>(target_tree_item);
        qint64 target_workspace_id = 0;
        qint64 target_group_id = 0;

        if (target_item->itemType() == SidebarItem::ROUTER_GROUP)
        {
            auto* group_item = static_cast<SidebarRouterGroup*>(target_item);
            target_workspace_id = group_item->workspaceId();
            target_group_id = group_item->groupId();
        }
        else if (target_item->itemType() == SidebarItem::ROUTER_WORKSPACE)
        {
            auto* workspace_item = static_cast<SidebarRouterWorkspace*>(target_item);
            target_workspace_id = workspace_item->workspaceId();
        }
        else
        {
            restoreSelection();
            return true;
        }

        // Repeat the eligibility checks from onDragMove in case the user releases over a
        // target that wasn't validated (DragLeave without DragMove can happen).
        const QList<RouterHost> hosts =
            hostsToMove(host_mime_data->hosts(), target_workspace_id, target_group_id);
        if (hosts.isEmpty())
        {
            restoreSelection();
            return true;
        }

        RouterSession* session = RouterController::session();
        if (!session)
        {
            restoreSelection();
            return true;
        }

        struct Batch
        {
            int pending = 0;
            int failed = 0;
        };

        const int total = static_cast<int>(hosts.size());
        SharedPointer<Batch> batch(new Batch());
        batch->pending = total;

        for (RouterHost host : hosts)
        {
            const HostId host_id = host.host_id;

            host.workspace_id = target_workspace_id;
            host.group_id = target_group_id;
            session->editHost(host, { this,
                [this, batch, total, host_id](const proto::router::HostResult& result)
            {
                if (result.error_code() != proto::router::kErrorOk)
                {
                    LOG(ERROR) << "Move of host" << host_id << "failed:" << result.error_code();
                    ++batch->failed;
                }

                if (--batch->pending > 0)
                    return;

                if (batch->failed > 0)
                {
                    MsgBox::warning(tree_widget_, total == 1 ?
                        tr("Failed to move the host to the selected group.") :
                        tr("Failed to move %n of the selected hosts.", "", batch->failed));
                }

                if (batch->failed < total)
                    emit sig_routerHostMoved();
            } });
        }

        event->acceptProposedAction();
        restoreSelection();
        return true;
    }

    return false;
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::isAllowedDropTarget(QTreeWidgetItem* target, QTreeWidgetItem* source) const
{
    if (!target || !source)
        return false;

    if (target == tree_widget_->invisibleRootItem() || target == source)
        return false;

    SidebarItem* target_item = static_cast<SidebarItem*>(target);
    if (target_item->itemType() != SidebarItem::LOCAL_GROUP)
        return false;

    // Prevent dropping a group onto its own descendant.
    std::function<bool(QTreeWidgetItem*, QTreeWidgetItem*)> isChild =
        [&](QTreeWidgetItem* parent, QTreeWidgetItem* child) -> bool
    {
        for (int i = 0; i < parent->childCount(); ++i)
        {
            QTreeWidgetItem* current = parent->child(i);
            if (current == child)
                return true;
            if (isChild(current, child))
                return true;
        }
        return false;
    };

    return !isChild(source, target);
}

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* Sidebar::findGroupItem(qint64 group_id, QTreeWidgetItem* parent) const
{
    for (int i = 0; i < parent->childCount(); ++i)
    {
        QTreeWidgetItem* child = parent->child(i);
        if (static_cast<SidebarItem*>(child)->groupId() == group_id)
            return child;

        QTreeWidgetItem* found = findGroupItem(group_id, child);
        if (found)
            return found;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
QTreeWidgetItem* Sidebar::findItem(const QByteArray& key) const
{
    if (key.isEmpty())
        return nullptr;

    for (QTreeWidgetItemIterator it(tree_widget_); *it; ++it)
    {
        if (itemKey(static_cast<SidebarItem*>(*it)) == key)
            return *it;
    }

    return nullptr;
}

//--------------------------------------------------------------------------------------------------
void Sidebar::selectPendingItem()
{
    if (!pending_timer_->isActive())
        return;

    QTreeWidgetItem* item = findItem(pending_item_);
    if (!item)
        return;

    pending_item_.clear();
    tree_widget_->setCurrentItem(item);
}

//--------------------------------------------------------------------------------------------------
bool Sidebar::isDragActive() const
{
    return dragging_ || findChild<QDrag*>(Qt::FindDirectChildrenOnly);
}

//--------------------------------------------------------------------------------------------------
void Sidebar::updateOutdatedItems()
{
    if (isDragActive())
        return;

    if (local_groups_outdated_)
    {
        local_groups_outdated_ = false;
        onLocalGroupsChanged();
    }

    if (router_status_outdated_)
    {
        router_status_outdated_ = false;
        router_groups_outdated_ = false;

        // More than one change may have passed, so the items of the former session are dropped first.
        onRouterStatusChanged(RouterStatus::OFFLINE);
        onRouterStatusChanged(RouterController::status());
    }
    else if (router_groups_outdated_)
    {
        router_groups_outdated_ = false;
        onRefreshWorkspaces();
    }
}
