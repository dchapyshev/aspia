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

class RouterWorkspaceDialog final : public QDialog
{
    Q_OBJECT

public:
    struct UserEntry
    {
        qint64 entry_id = 0;
        QString name;
        QByteArray public_key;
    };

    // current.entry_id == 0 means create mode; > 0 means modify mode. For modify, the initial
    // name, plaintext comment and access user list are taken from current. self_user_id is the
    // current admin's id, used to block revoking own access.
    RouterWorkspaceDialog(
        const Router::Workspace& current, qint64 self_user_id,
        const QStringList& existing_names, QWidget* parent);
    ~RouterWorkspaceDialog() final;

    void setUsers(const QList<UserEntry>& users);

    // Valid after a successful exec() (returns QDialog::Accepted). The router will encrypt
    // comment + seal GK for newly granted users before sending to the server.
    const Router::Workspace& workspace() const { return workspace_; }

private:
    void onButtonBoxClicked(QAbstractButton* button);
    void onAddClicked();
    void onRemoveClicked();
    void rebuildLists();
    void updateButtonsState();

    std::unique_ptr<Ui::RouterWorkspaceDialog> ui;
    qint64 entry_id_ = 0;
    qint64 self_user_id_ = 0;
    QStringList existing_names_;
    QHash<qint64, UserEntry> users_;
    QSet<qint64> initial_access_user_ids_;
    QSet<qint64> access_user_ids_;
    Router::Workspace workspace_;

    Q_DISABLE_COPY_MOVE(RouterWorkspaceDialog)
};

#endif // CLIENT_UI_HOSTS_ROUTER_WORKSPACE_DIALOG_H
