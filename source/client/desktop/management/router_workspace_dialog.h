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

#ifndef CLIENT_DESKTOP_MANAGEMENT_ROUTER_WORKSPACE_DIALOG_H
#define CLIENT_DESKTOP_MANAGEMENT_ROUTER_WORKSPACE_DIALOG_H

#include <QDialog>

#include <memory>

#include "client/page_model.h"
#include "client/router.h"
#include "client/desktop/management/workspace_edit_model.h"

class QAbstractButton;
class QComboBox;
class QListWidget;
class QToolButton;

namespace Ui {
class RouterWorkspaceDialog;
} // namespace Ui

namespace proto::router {
class HostResult;
class UserList;
class WorkspaceResult;
} // namespace proto::router

class RouterWorkspaceDialog final : public QDialog
{
    Q_OBJECT

public:
    // workspace_id == 0 means create mode; > 0 means modify mode.
    RouterWorkspaceDialog(qint64 router_id, qint64 workspace_id, QWidget* parent);
    ~RouterWorkspaceDialog() final;

private slots:
    void onWorkspaceListReceived(const Router::WorkspaceList& list);
    void onUserListReceived(const proto::router::UserList& list);
    void onHostListReceived(const Router::HostList& list);
    void onHostResultReceived(const proto::router::HostResult& result);
    void onWorkspaceResultReceived(const proto::router::WorkspaceResult& result);
    void onButtonBoxClicked(QAbstractButton* button);
    void onAddClicked();
    void onRemoveClicked();
    void onHostAddClicked();
    void onHostRemoveClicked();

private:
    // The reply of a member lookup, one user per request.
    void applyMemberLookup(const proto::router::UserList& list, qint64 user_id);

    // Membership is an edit of the workspace record and applies on OK; the hosts are moved one by
    // one and apply right away, so a host operation needs a workspace to move the host into. In
    // create mode the first one creates the workspace from the name of the form and runs after
    // the reply.
    void moveHost(quint64 host_id, qint64 workspace_id);
    void runPendingHostMove();

    void fetchUsers();
    void fetchMemberNames();
    void fetchHosts();
    void refetchLists();
    void rebuildLists();
    void rebuildHostLists();
    void updatePagination(QComboBox* combo, QToolButton* prev, QToolButton* next,
                          const PageModel& page);
    void updateButtonsState();
    void updateLoadingState();

    // The name of the form, checked the way the router checks it. Empty when it is not usable.
    QString validatedName();

    std::unique_ptr<Ui::RouterWorkspaceDialog> ui;
    qint64 router_id_ = 0;
    qint64 entry_id_ = 0;
    bool is_admin_ = false;

    // The whole edit state machine (server snapshot, operator intents, effective membership,
    // the revision the save is based on) lives in the model, where it is unit-tested; the dialog
    // only feeds replies in and mirrors the state to the widgets.
    WorkspaceEditModel model_;

    PageModel users_page_;
    PageModel hosts_in_page_;
    PageModel hosts_free_page_;

    // The pages of hosts as they arrived; the hosts are not part of the model, because a host
    // operation is applied by the router before it is shown.
    QList<Router::Host> hosts_in_;
    QList<Router::Host> hosts_free_;

    // The host operation waiting for the workspace of a create-mode dialog to be created.
    quint64 pending_host_id_ = 0;
    qint64 pending_host_workspace_id_ = 0;

    bool closing_ = false;

    Q_DISABLE_COPY_MOVE(RouterWorkspaceDialog)
};

#endif // CLIENT_DESKTOP_MANAGEMENT_ROUTER_WORKSPACE_DIALOG_H
