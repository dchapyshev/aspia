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

#include "client/router.h"
#include "client/desktop/management/workspace_edit_model.h"

class QAbstractButton;

namespace Ui {
class RouterWorkspaceDialog;
} // namespace Ui

namespace proto::router {
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
    void onWorkspaceResultReceived(const proto::router::WorkspaceResult& result);

private:
    void onButtonBoxClicked(QAbstractButton* button);
    void onAddClicked();
    void onRemoveClicked();
    void onHostAddClicked();
    void onHostRemoveClicked();
    void refetchLists();
    void rebuildLists();
    void updateButtonsState();
    void updateLoadingState();

    std::unique_ptr<Ui::RouterWorkspaceDialog> ui;
    qint64 router_id_ = 0;
    qint64 entry_id_ = 0;

    // The whole edit state machine (server snapshots, operator intents, effective sets, the
    // revision paired with the host snapshot) lives in the model, where it is unit-tested; the
    // dialog only feeds replies in and mirrors the state to the widgets.
    WorkspaceEditModel model_;
    Router::Workspace workspace_;
    bool closing_ = false;

    Q_DISABLE_COPY_MOVE(RouterWorkspaceDialog)
};

#endif // CLIENT_DESKTOP_MANAGEMENT_ROUTER_WORKSPACE_DIALOG_H
