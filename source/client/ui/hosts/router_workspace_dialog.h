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

#include "base/crypto/secure_string.h"
#include "proto/router_admin.h"

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

    struct SelfCredentials
    {
        qint64 user_id = 0;
        QByteArray wrap_private_key;
        QByteArray wrap_salt;
        SecureString password;
    };

    // current.entry_id() == 0 means create mode; > 0 means modify mode. For modify, the
    // initial name and access list are taken from current.
    RouterWorkspaceDialog(
        const proto::router::Workspace& current, const QStringList& existing_names, QWidget* parent);
    ~RouterWorkspaceDialog() final;

    void setUsers(const QVector<UserEntry>& users);
    void setSelfCredentials(const SelfCredentials& self);

    // Valid after a successful exec() (returns QDialog::Accepted). Contains the new workspace
    // state ready to send to the router: entry_id (>0 for modify), name, full desired access
    // list with wrapped_gk filled in for newly granted users.
    const proto::router::Workspace& workspace() const { return workspace_; }

private:
    void onButtonBoxClicked(QAbstractButton* button);
    void onAddClicked();
    void onRemoveClicked();
    void rebuildLists();
    void updateButtonsState();
    bool buildWorkspaceProto();

    std::unique_ptr<Ui::RouterWorkspaceDialog> ui;
    qint64 entry_id_ = -1;
    QString name_;
    QStringList existing_names_;
    QHash<qint64, UserEntry> users_;
    QHash<qint64, QByteArray> initial_access_;
    QSet<qint64> access_user_ids_;
    SelfCredentials self_;
    proto::router::Workspace workspace_;

    Q_DISABLE_COPY_MOVE(RouterWorkspaceDialog)
};

#endif // CLIENT_UI_HOSTS_ROUTER_WORKSPACE_DIALOG_H
