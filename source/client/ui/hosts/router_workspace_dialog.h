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

#ifndef CLIENT_UI_HOSTS_ROUTER_WORKSPACE_DIALOG_H
#define CLIENT_UI_HOSTS_ROUTER_WORKSPACE_DIALOG_H

#include <QDialog>
#include <QHash>
#include <QSet>

#include <memory>

#include "client/router.h"

class QAbstractButton;

namespace Ui {
class RouterWorkspaceDialog;
} // namespace Ui

namespace proto::router {
class HostList;
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
    void onHostListReceived(const proto::router::HostList& list);
    void onWorkspaceResultReceived(const proto::router::WorkspaceResult& result);

private:
    struct UserEntry
    {
        qint64 entry_id = 0;
        bool is_admin   = false;
        QString name;
        QByteArray public_key;
    };

    void onButtonBoxClicked(QAbstractButton* button);
    void onAddClicked();
    void onRemoveClicked();
    void onHostAddClicked();
    void onHostRemoveClicked();
    void rebuildLists();
    void updateButtonsState();
    void updateLoadingState();

    std::unique_ptr<Ui::RouterWorkspaceDialog> ui;
    qint64 router_id_ = 0;
    qint64 entry_id_ = 0;
    QStringList existing_names_;
    QHash<qint64, UserEntry> users_;
    QSet<qint64> initial_access_user_ids_;
    QSet<qint64> access_user_ids_;
    Router::Workspace workspace_;
    bool workspaces_loaded_ = false;
    bool users_loaded_ = false;
    bool hosts_loaded_ = false;

    Q_DISABLE_COPY_MOVE(RouterWorkspaceDialog)
};

#endif // CLIENT_UI_HOSTS_ROUTER_WORKSPACE_DIALOG_H
